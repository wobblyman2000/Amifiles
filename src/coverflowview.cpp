#include "coverflowview.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QDebug>

CoverFlowView::CoverFlowView(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void CoverFlowView::setModel(QAbstractItemModel* model) {
    if (m_model) {
        disconnect(m_model, &QAbstractItemModel::modelReset, this, &CoverFlowView::onModelReset);
        disconnect(m_model, &QAbstractItemModel::dataChanged, this, &CoverFlowView::onDataChanged);
        disconnect(m_model, &QAbstractItemModel::rowsInserted, this, &CoverFlowView::onModelReset);
        disconnect(m_model, &QAbstractItemModel::rowsRemoved, this, &CoverFlowView::onModelReset);
        disconnect(m_model, &QAbstractItemModel::layoutChanged, this, &CoverFlowView::onModelReset);
    }
    m_model = model;
    m_rootIndex = QModelIndex(); // Clear root index to prevent matching stale/dangling indexes of the old model
    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, &CoverFlowView::onModelReset);
        connect(m_model, &QAbstractItemModel::dataChanged, this, &CoverFlowView::onDataChanged);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &CoverFlowView::onModelReset);
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, &CoverFlowView::onModelReset);
        connect(m_model, &QAbstractItemModel::layoutChanged, this, &CoverFlowView::onModelReset);
    }
    onModelReset();
}

void CoverFlowView::setRootIndex(const QModelIndex& index) {
    m_rootIndex = index;
    onModelReset();
}

void CoverFlowView::setSelectionModel(QItemSelectionModel* selectionModel) {
    if (m_selectionModel) {
        disconnect(m_selectionModel, &QItemSelectionModel::selectionChanged, this, &CoverFlowView::onSelectionChanged);
    }
    m_selectionModel = selectionModel;
    if (m_selectionModel) {
        connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this, &CoverFlowView::onSelectionChanged);
    }
}

void CoverFlowView::setSelectedIndex(int index) {
    int count = m_model ? m_model->rowCount(m_rootIndex) : 0;
    if (count == 0) {
        m_currentIndex = 0;
        return;
    }
    m_currentIndex = qBound(0, index, count - 1);
    updateSelection();
    update();
    emit currentIndexChanged(m_currentIndex);
}

QModelIndex CoverFlowView::indexAt(const QPoint& pos) const {
    if (!m_model) return QModelIndex();
    int count = m_model->rowCount(m_rootIndex);
    if (count == 0) return QModelIndex();

    // Check hit test for visible cards (from center out)
    QList<int> checkOrder;
    checkOrder.append(m_currentIndex);
    for (int i = 1; i <= 10; ++i) {
        if (m_currentIndex - i >= 0) checkOrder.append(m_currentIndex - i);
        if (m_currentIndex + i < count) checkOrder.append(m_currentIndex + i);
    }

    for (int idx : checkOrder) {
        QRect r = getCardRect(idx);
        if (r.contains(pos)) {
            return m_model->index(idx, 0, m_rootIndex);
        }
    }
    return QModelIndex();
}

void CoverFlowView::onModelReset() {
    m_pixmapCache.clear();
    int count = m_model ? m_model->rowCount(m_rootIndex) : 0;
    if (count > 0) {
        m_currentIndex = qBound(0, m_currentIndex, count - 1);
    } else {
        m_currentIndex = 0;
    }
    update();
}

void CoverFlowView::onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
    Q_UNUSED(roles);
    if (topLeft.parent() == m_rootIndex) {
        m_pixmapCache.clear();
        update();
    }
}

void CoverFlowView::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    if (!m_selectionModel || !m_model) return;
    QModelIndexList list = m_selectionModel->selectedIndexes();
    if (!list.isEmpty()) {
        int idx = list.first().row();
        if (idx != m_currentIndex) {
            m_currentIndex = qBound(0, idx, m_model->rowCount(m_rootIndex) - 1);
            update();
        }
    }
}

void CoverFlowView::updateSelection() {
    if (!m_selectionModel || !m_model) return;
    int count = m_model->rowCount(m_rootIndex);
    if (m_currentIndex >= 0 && m_currentIndex < count) {
        QModelIndex idx = m_model->index(m_currentIndex, 0, m_rootIndex);
        m_selectionModel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

QPixmap CoverFlowView::getItemPixmap(const QModelIndex& index) {
    QString filePath = m_model->data(index, Qt::UserRole + 1).toString(); // custom path role or filepath role
    if (filePath.isEmpty()) {
        filePath = m_model->data(index, Qt::DisplayRole).toString();
    }

    if (m_pixmapCache.contains(filePath)) {
        return m_pixmapCache[filePath];
    }

    // Try loading thumbnail or cover art
    QPixmap pix;
    QFileInfo info(filePath);
    if (info.exists() && info.isFile()) {
        QString ext = info.suffix().toLower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif" || ext == "webp") {
            pix.load(filePath);
        }
    }

    if (pix.isNull()) {
        // Fallback to system icon
        QIcon icon = m_model->data(index, Qt::DecorationRole).value<QIcon>();
        if (icon.isNull()) {
            icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
        }
        pix = icon.pixmap(128, 128);
    }

    if (!pix.isNull()) {
        // Cache scaled version to make rendering super fast
        pix = pix.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_pixmapCache[filePath] = pix;
    }
    return pix;
}

QRect CoverFlowView::getCardRect(int itemIndex) const {
    int cx = width() / 2;
    int cy = height() / 2;

    if (itemIndex == m_currentIndex) {
        return QRect(cx - 90, cy - 110, 180, 180);
    } else if (itemIndex < m_currentIndex) {
        int dist = m_currentIndex - itemIndex;
        int x = cx - 130 - (dist - 1) * 45;
        return QRect(x - 60, cy - 70, 120, 120);
    } else {
        int dist = itemIndex - m_currentIndex;
        int x = cx + 130 + (dist - 1) * 45;
        return QRect(x - 60, cy - 70, 120, 120);
    }
}

void CoverFlowView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Draw glossy dark floor background
    QLinearGradient bgGrad(0, 0, 0, height());
    bgGrad.setColorAt(0, QColor("#1e1e2e"));
    bgGrad.setColorAt(0.6, QColor("#11111b"));
    bgGrad.setColorAt(1.0, QColor("#09090f"));
    painter.fillRect(rect(), bgGrad);

    int count = m_model ? m_model->rowCount(m_rootIndex) : 0;
    if (count == 0) {
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(rect(), Qt::AlignCenter, "This folder is empty");
        return;
    }

    // Ensure index bounds
    m_currentIndex = qBound(0, m_currentIndex, count - 1);

    // Helper lambda to draw a single card and its reflection
    auto drawCard = [this, &painter](int idx, const QRect& r, double opacity) {
        QModelIndex modelIdx = m_model->index(idx, 0, m_rootIndex);
        QPixmap pix = getItemPixmap(modelIdx);
        if (pix.isNull()) return;

        painter.save();
        painter.setOpacity(opacity);

        // Center within rect
        int px = r.x() + (r.width() - pix.width()) / 2;
        int py = r.y() + (r.height() - pix.height()) / 2;

        // Draw shadow/border frame
        painter.setPen(QPen(QColor("#313244"), idx == m_currentIndex ? 2 : 1));
        painter.drawRect(px - 1, py - 1, pix.width() + 1, pix.height() + 1);

        // Draw card image
        painter.drawPixmap(px, py, pix);

        // Draw mirror reflection below card
        QPixmap reflection = pix.transformed(QTransform().scale(1, -1));
        int refY = py + pix.height() + 2;
        int refH = qMin(reflection.height() / 2, height() - refY);

        if (refH > 0) {
            QPixmap croppedRef = reflection.copy(0, 0, reflection.width(), refH);
            
            // Mask gradient for fading out
            QPixmap mask(croppedRef.size());
            mask.fill(Qt::transparent);
            QPainter maskPainter(&mask);
            QLinearGradient grad(0, 0, 0, refH);
            grad.setColorAt(0, QColor(255, 255, 255, 120)); // fading start opacity
            grad.setColorAt(1.0, Qt::transparent);
            maskPainter.fillRect(mask.rect(), grad);
            maskPainter.end();

            // Apply mask
            QPainter refPainter(&croppedRef);
            refPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            refPainter.drawPixmap(0, 0, mask);
            refPainter.end();

            painter.drawPixmap(px, refY, croppedRef);
        }

        painter.restore();
    };

    // Render left cards (from left to right up to index - 1)
    for (int i = qMax(0, m_currentIndex - 10); i < m_currentIndex; ++i) {
        int dist = m_currentIndex - i;
        double opacity = qMax(0.2, 1.0 - (dist * 0.15));
        drawCard(i, getCardRect(i), opacity);
    }

    // Render right cards (from far right down to index + 1)
    for (int i = qMin(count - 1, m_currentIndex + 10); i > m_currentIndex; --i) {
        int dist = i - m_currentIndex;
        double opacity = qMax(0.2, 1.0 - (dist * 0.15));
        drawCard(i, getCardRect(i), opacity);
    }

    // Render center card (drawn last to overlay perfectly)
    drawCard(m_currentIndex, getCardRect(m_currentIndex), 1.0);

    // Draw active item text label
    QModelIndex centerIdx = m_model->index(m_currentIndex, 0, m_rootIndex);
    QString name = m_model->data(centerIdx, Qt::DisplayRole).toString();

    painter.save();
    painter.setPen(QColor("#cdd6f4"));
    QFont f = painter.font();
    f.setBold(true);
    f.setPointSize(12);
    painter.setFont(f);

    QRect textRect(10, height() - 55, width() - 20, 25);
    painter.drawText(textRect, Qt::AlignCenter, name);

    // Subtitle (size or file info)
    f.setBold(false);
    f.setPointSize(9);
    painter.setFont(f);
    painter.setPen(QColor("#a6adc8"));

    QString sizeStr = m_model->data(m_model->index(m_currentIndex, 1, m_rootIndex), Qt::DisplayRole).toString();
    QString dateStr = m_model->data(m_model->index(m_currentIndex, 2, m_rootIndex), Qt::DisplayRole).toString();
    QString subStr = sizeStr.isEmpty() ? dateStr : QString("%1  |  %2").arg(sizeStr).arg(dateStr);

    QRect subRect(10, height() - 30, width() - 20, 20);
    painter.drawText(subRect, Qt::AlignCenter, subStr);

    painter.restore();
}

void CoverFlowView::keyPressEvent(QKeyEvent* event) {
    int count = m_model ? m_model->rowCount(m_rootIndex) : 0;
    if (count == 0) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up) {
        setSelectedIndex(m_currentIndex - 1);
        updateSelection();
        event->accept();
    } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down) {
        setSelectedIndex(m_currentIndex + 1);
        updateSelection();
        event->accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_currentIndex >= 0 && m_currentIndex < count) {
            emit itemDoubleClicked(m_model->index(m_currentIndex, 0, m_rootIndex));
        }
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void CoverFlowView::mousePressEvent(QMouseEvent* event) {
    int count = m_model ? m_model->rowCount(m_rootIndex) : 0;
    if (count == 0) return;

    // Check hit test for visible cards (from center out)
    QList<int> checkOrder;
    checkOrder.append(m_currentIndex);
    for (int i = 1; i <= 10; ++i) {
        if (m_currentIndex - i >= 0) checkOrder.append(m_currentIndex - i);
        if (m_currentIndex + i < count) checkOrder.append(m_currentIndex + i);
    }

    for (int idx : checkOrder) {
        QRect r = getCardRect(idx);
        if (r.contains(event->pos())) {
            setSelectedIndex(idx);
            updateSelection();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CoverFlowView::mouseDoubleClickEvent(QMouseEvent* event) {
    int count = m_model ? m_model->rowCount(m_rootIndex) : 0;
    if (count == 0) return;

    QRect r = getCardRect(m_currentIndex);
    if (r.contains(event->pos())) {
        emit itemDoubleClicked(m_model->index(m_currentIndex, 0, m_rootIndex));
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CoverFlowView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}
