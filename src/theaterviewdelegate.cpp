#include "theaterviewdelegate.h"
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QIcon>
#include <QAbstractProxyModel>
#include "tagmanager.h"
#include "filepanel.h"
#include <QProcess>
#include <QTimer>

TheaterViewDelegate::TheaterViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void TheaterViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;
    bool isSelected = option.state & QStyle::State_Selected;
    bool isHovered = option.state & QStyle::State_MouseOver;

    QColor bgCol = QColor("#181825");
    if (isSelected) {
        bgCol = QColor("#2d3149");
    } else if (isHovered) {
        bgCol = QColor("#313244");
    }

    painter->setBrush(bgCol);
    QPen borderPen(isSelected ? QColor("#89b4fa") : QColor("#313244"), isSelected ? 2 : 1);
    painter->setPen(borderPen);
    painter->drawRoundedRect(rect.adjusted(3, 3, -3, -3), 8, 8);

    QModelIndex col0 = index.siblingAtColumn(0);
    QString path;
    const FileFilterProxyModel* filterModel = nullptr;
    const QAbstractItemModel* m = index.model();
    QModelIndex mappedIndex = col0;

    while (m) {
        if (const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>(m)) {
            if (const FileFilterProxyModel* ffm = qobject_cast<const FileFilterProxyModel*>(proxy)) {
                filterModel = ffm;
            }
            mappedIndex = proxy->mapToSource(mappedIndex);
            m = proxy->sourceModel();
        } else {
            break;
        }
    }

    if (m) {
        if (const QFileSystemModel* fsm = qobject_cast<const QFileSystemModel*>(m)) {
            path = fsm->filePath(mappedIndex);
        }
    }

    if (path.isEmpty()) {
        path = col0.data(Qt::UserRole).toString();
        if (path.isEmpty()) {
            path = col0.data(Qt::DisplayRole).toString();
        }
    }
    QFileInfo info(path);

    bool hasCover = false;
    if (filterModel) {
        hasCover = filterModel->hasCasingCover(path);
    }

    QRect imageRect(rect.x() + 8, rect.y() + 8, rect.width() - 16, 205);

    // Query DecorationRole to trigger the asynchronous casing runnable inside the model
    QIcon icon = col0.data(Qt::DecorationRole).value<QIcon>();

    // Handle hover animation state
    if (isHovered) {
        QWidget* view = option.widget ? const_cast<QWidget*>(option.widget) : nullptr;
        startHoverAnimation(path, view);
    } else if (m_activeHoverPath == path) {
        stopHoverAnimation();
    }

    bool hasHoverFrames = isHovered && (m_activeHoverPath == path) && m_hoverFramesCache.contains(path) && !m_hoverFramesCache[path].isEmpty();

    if (hasHoverFrames) {
        int idx = m_hoverFrameIndex % m_hoverFramesCache[path].size();
        QPixmap currentFrame = m_hoverFramesCache[path][idx];
        painter->save();
        painter->setClipRect(imageRect.adjusted(1, 1, -1, -1));
        painter->drawPixmap(imageRect, currentFrame.scaled(imageRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        painter->restore();
    } else if (hasCover && !icon.isNull()) {
        icon.paint(painter, imageRect, Qt::AlignCenter);
    } else {
        QLinearGradient posterGrad(imageRect.topLeft(), imageRect.bottomRight());
        posterGrad.setColorAt(0.0, QColor("#1e1e2e"));
        posterGrad.setColorAt(1.0, QColor("#11111b"));
        painter->setBrush(posterGrad);
        painter->setPen(QPen(QColor("#45475a"), 1));
        painter->drawRoundedRect(imageRect, 6, 6);

        painter->setPen(QPen(QColor("#585b70"), 2));
        painter->setBrush(QColor("#313244"));
        int cx = imageRect.center().x();
        int cy = imageRect.center().y() - 15;
        
        if (info.isDir()) {
            painter->drawRect(cx - 20, cy - 15, 40, 30);
            painter->drawEllipse(cx - 10, cy - 5, 8, 8);
            painter->drawEllipse(cx + 10, cy - 5, 8, 8);
        } else {
            painter->drawRect(cx - 15, cy - 20, 30, 40);
            painter->drawLine(cx - 8, cy - 10, cx + 8, cy - 10);
            painter->drawLine(cx - 8, cy, cx + 8, cy);
            painter->drawLine(cx - 8, cy + 10, cx + 8, cy + 10);
        }

        QFont titleFont = painter->font();
        titleFont.setPointSize(7);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(QColor("#a6adc8"));
        
        QString baseTitle = info.fileName();
        QFontMetrics fmTitle(titleFont);
        QString elTitle = fmTitle.elidedText(baseTitle, Qt::ElideRight, imageRect.width() - 12);
        painter->drawText(QRect(imageRect.x() + 6, cy + 30, imageRect.width() - 12, 50), Qt::AlignCenter | Qt::TextWordWrap, elTitle);
    }

    int textX = rect.x() + 8;
    int textY = rect.y() + 225;
    int textW = rect.width() - 16;

    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(8);
    painter->setFont(font);
    painter->setPen(QColor("#cdd6f4"));

    QString nameStr = info.fileName();
    QFontMetrics fm(font);
    QString elidedName = fm.elidedText(nameStr, Qt::ElideRight, textW);
    painter->drawText(QRect(textX, textY, textW, 16), Qt::AlignCenter, elidedName);

    font.setBold(false);
    font.setPointSize(7);
    painter->setFont(font);
    painter->setPen(QColor("#585b70"));

    double kb = info.size() / 1024.0;
    double mb = kb / 1024.0;
    QString sizeStr = info.isDir() ? "Folder" : ((mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1));
    painter->drawText(QRect(textX, textY + 16, textW, 14), Qt::AlignCenter, sizeStr);

    painter->restore();
}

QSize TheaterViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(170, 270);
}

void TheaterViewDelegate::startHoverAnimation(const QString& path, QWidget* view) const {
    if (m_activeHoverPath == path) return;
    
    stopHoverAnimation();
    m_activeHoverPath = path;
    m_hoverFrameIndex = 0;

    if (!m_hoverTimer) {
        m_hoverTimer = new QTimer(const_cast<TheaterViewDelegate*>(this));
        connect(m_hoverTimer, &QTimer::timeout, this, [this, view]() {
            if (!m_activeHoverPath.isEmpty() && m_hoverFramesCache.contains(m_activeHoverPath)) {
                int count = m_hoverFramesCache[m_activeHoverPath].size();
                if (count > 0) {
                    m_hoverFrameIndex = (m_hoverFrameIndex + 1) % count;
                    if (view) view->update();
                }
            }
        });
    }

    if (m_hoverFramesCache.contains(path)) {
        m_hoverTimer->start(350); // 350ms frame time for nice smooth slideshow
    } else {
        extractFramesInBackground(path, view);
    }
}

void TheaterViewDelegate::stopHoverAnimation() const {
    m_activeHoverPath.clear();
    m_hoverFrameIndex = 0;
    if (m_hoverTimer) {
        m_hoverTimer->stop();
    }
}

void TheaterViewDelegate::extractFramesInBackground(const QString& path, QWidget* view) const {
    if (m_hoverLoading.value(path, false)) return;
    m_hoverLoading[path] = true;

    // Run extraction in a background task or process
    QString targetFile = path;
    QFileInfo fi(path);
    if (fi.isDir()) {
        // Find first video file
        QDir dir(path);
        QStringList videoExts = { "mp4", "mkv", "avi", "mov", "webm" };
        QFileInfoList list = dir.entryInfoList(QDir::Files);
        for (const QFileInfo& sub : list) {
            if (videoExts.contains(sub.suffix().toLower())) {
                targetFile = sub.absoluteFilePath();
                break;
            }
        }
        if (targetFile == path) {
            m_hoverLoading[path] = false;
            return;
        }
    }

    QString tempPrefix = QDir::temp().filePath("amifiles_h_" + QString::number(qHash(path)) + "_");
    QStringList args;
    args << "-ss" << "10"
         << "-i" << targetFile
         << "-vf" << "fps=1,scale=240:-1"
         << "-t" << "5"
         << "-y"
         << tempPrefix + "%d.jpg";

    QProcess* proc = new QProcess(const_cast<TheaterViewDelegate*>(this));
    proc->start("ffmpeg", args);
    connect(proc, &QProcess::finished, this, [this, path, tempPrefix, view, proc]() {
        proc->deleteLater();
        
        QList<QPixmap> frames;
        for (int i = 1; i <= 5; ++i) {
            QString filePath = tempPrefix + QString::number(i) + ".jpg";
            if (QFile::exists(filePath)) {
                QPixmap p(filePath);
                if (!p.isNull()) {
                    frames.append(p);
                }
                QFile::remove(filePath);
            }
        }
        
        m_hoverLoading[path] = false;
        if (!frames.isEmpty()) {
            m_hoverFramesCache[path] = frames;
            if (m_activeHoverPath == path) {
                m_hoverTimer->start(350);
                if (view) view->update();
            }
        }
    });
}
