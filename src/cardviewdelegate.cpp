#include "cardviewdelegate.h"
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QIcon>
#include "tagmanager.h"

CardViewDelegate::CardViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void CardViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;
    bool isSelected = option.state & QStyle::State_Selected;
    bool isHovered = option.state & QStyle::State_MouseOver;

    // Background Card Color
    QColor bgCol = QColor("#181825"); // Base background
    if (isSelected) {
        bgCol = QColor("#2d3149"); // Selected tint
    } else if (isHovered) {
        bgCol = QColor("#313244"); // Hover tint
    }

    // Draw card border and background
    painter->setBrush(bgCol);
    QPen borderPen(isSelected ? QColor("#89b4fa") : QColor("#313244"), isSelected ? 2 : 1);
    painter->setPen(borderPen);
    painter->drawRoundedRect(rect.adjusted(2, 2, -2, -2), 6, 6);

    // Get metadata from filesystem model
    // Column 0 index path
    QModelIndex col0 = index.siblingAtColumn(0);
    QString path = col0.data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        path = col0.data(Qt::DisplayRole).toString(); // fallback
    }
    QFileInfo info(path);

    // Left Side Icon/Thumbnail
    QIcon icon = col0.data(Qt::DecorationRole).value<QIcon>();
    QRect iconRect(rect.x() + 8, rect.y() + 8, 48, 48);
    if (!icon.isNull()) {
        icon.paint(painter, iconRect, Qt::AlignCenter);
    }

    // Right Side Information Texts
    int textX = rect.x() + 64;
    int textY = rect.y() + 18;
    int textW = rect.width() - 72;

    // 1. Title / Name (Bold)
    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(9);
    painter->setFont(font);
    painter->setPen(QColor("#cdd6f4")); // mocha text

    QString nameStr = info.fileName();
    if (nameStr.isEmpty()) nameStr = info.absoluteFilePath();
    
    // Elide filename if too long
    QFontMetrics fm(font);
    QString elidedName = fm.elidedText(nameStr, Qt::ElideRight, textW);
    painter->drawText(textX, textY, elidedName);

    // 2. Secondary Info (Size + Extension / Tags)
    font.setBold(false);
    font.setPointSize(8);
    painter->setFont(font);
    painter->setPen(QColor("#a6adc8")); // dimmed text

    double kb = info.size() / 1024.0;
    double mb = kb / 1024.0;
    QString sizeStr = info.isDir() ? "Folder" : ((mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1));

    QStringList tags = TagManager::instance().getFileTags(info.absoluteFilePath());
    QString tagStr = tags.isEmpty() ? "" : QString(" [%1]").arg(tags.first());
    
    QString detailsStr = QString("%1 | %2%3").arg(info.suffix().toUpper()).arg(sizeStr).arg(tagStr);
    if (info.isDir()) detailsStr = QString("Folder | %1 items").arg(QDir(info.absoluteFilePath()).count() - 2);

    QString elidedDetails = fm.elidedText(detailsStr, Qt::ElideRight, textW);
    painter->drawText(textX, textY + 16, elidedDetails);

    // 3. Date Modified
    painter->setPen(QColor("#585b70")); // darker date text
    QString dateStr = info.lastModified().toString("yyyy-MM-dd hh:mm");
    painter->drawText(textX, textY + 30, dateStr);

    painter->restore();
}

QSize CardViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(190, 68);
}
