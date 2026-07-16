#include "treemapchartwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <algorithm>

TreeMapChartWidget::TreeMapChartWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
}

void TreeMapChartWidget::setEntries(const QList<SpaceEntry>& entries, qint64 totalSize) {
    m_entries = entries;
    m_totalSize = totalSize;
    m_hoveredIndex = -1;

    // Sort descending by size
    std::sort(m_entries.begin(), m_entries.end(), [](const SpaceEntry& a, const SpaceEntry& b) {
        return a.size > b.size;
    });

    layoutTreeMap();
    update();
}

void TreeMapChartWidget::layoutTreeMap() {
    m_nodes.clear();
    if (m_entries.isEmpty() || m_totalSize <= 0) return;

    QRect r = rect().adjusted(2, 2, -2, -2);
    if (r.width() <= 0 || r.height() <= 0) return;

    // Squarified/Slice-and-Dice: We alternate vertical/horizontal split based on aspect ratio
    qint64 subsetTotal = 0;
    for (const auto& entry : m_entries) {
        subsetTotal += entry.size;
    }

    int curX = r.x();
    int curY = r.y();
    int curW = r.width();
    int curH = r.height();

    static const QColor palette[] = {
        QColor("#89b4fa"), QColor("#f5c2e7"), QColor("#a6e3a1"), QColor("#f9e2af"),
        QColor("#fab387"), QColor("#b4befe"), QColor("#cba6f7"), QColor("#f2cdcd"),
        QColor("#94e2d5"), QColor("#89dceb"), QColor("#74c7ec"), QColor("#a6adc8")
    };

    for (int i = 0; i < m_entries.size(); ++i) {
        const auto& entry = m_entries[i];
        if (entry.size <= 0) continue;

        double ratio = (double)entry.size / subsetTotal;
        QRect childRect;

        // Alternate partition based on larger dimension to keep rectangles blocky
        if (curW > curH) {
            int w = qRound(curW * ratio);
            if (i == m_entries.size() - 1) w = curW; // fill remaining
            childRect = QRect(curX, curY, w, curH);
            curX += w;
            curW -= w;
        } else {
            int h = qRound(curH * ratio);
            if (i == m_entries.size() - 1) h = curH; // fill remaining
            childRect = QRect(curX, curY, curW, h);
            curY += h;
            curH -= h;
        }

        TreeMapNode node;
        node.name = entry.name;
        node.absolutePath = entry.absolutePath;
        node.isDir = entry.isDir;
        node.size = entry.size;
        node.rect = childRect;
        node.color = palette[i % 12];
        m_nodes.append(node);

        subsetTotal -= entry.size;
        if (subsetTotal <= 0) break;
    }
}

void TreeMapChartWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    painter.fillRect(rect(), QColor("#11111b"));

    if (m_nodes.isEmpty()) {
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(rect(), Qt::AlignCenter, "No space analyzer scan data loaded.");
        return;
    }

    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    QFontMetrics fm(font);

    for (int i = 0; i < m_nodes.size(); ++i) {
        const auto& node = m_nodes[i];
        QRect rect = node.rect.adjusted(1, 1, -1, -1);

        // Fill color
        QColor fillCol = node.color;
        if (i == m_hoveredIndex) {
            fillCol = fillCol.lighter(115);
        }
        painter.fillRect(rect, fillCol);

        // Border outline
        QPen pen(QColor("#313244"), 1);
        if (i == m_hoveredIndex) {
            pen.setColor(QColor("#cdd6f4"));
            pen.setWidth(2);
        }
        painter.setPen(pen);
        painter.drawRect(rect);

        // Render Labels if enough space
        if (rect.width() > 55 && rect.height() > 25) {
            painter.setPen(QColor("#11111b")); // Contrast against pastel blocks
            
            QString textLabel = fm.elidedText(node.name, Qt::ElideRight, rect.width() - 8);
            QString sizeLabel = fm.elidedText(formatBytes(node.size), Qt::ElideRight, rect.width() - 8);

            painter.drawText(rect.adjusted(4, 2, -4, -2), Qt::AlignTop | Qt::AlignLeft, textLabel);
            painter.drawText(rect.adjusted(4, 2, -4, -2), Qt::AlignBottom | Qt::AlignRight, sizeLabel);
        }
    }
}

void TreeMapChartWidget::mouseMoveEvent(QMouseEvent* event) {
    int idx = getNodeAt(event->pos());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        update();

        if (m_hoveredIndex >= 0 && m_hoveredIndex < m_nodes.size()) {
            const auto& node = m_nodes[m_hoveredIndex];
            double percent = m_totalSize > 0 ? (double)node.size / m_totalSize * 100.0 : 0.0;
            emit hoveredItemChanged(node.name, node.size, percent);
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1\n%2 (%3%)").arg(node.name).arg(formatBytes(node.size)).arg(QString::number(percent, 'f', 1)),
                               this);
        } else {
            emit hoveredItemChanged("", 0, 0.0);
            QToolTip::hideText();
        }
    }
}

void TreeMapChartWidget::mousePressEvent(QMouseEvent* event) {
    int idx = getNodeAt(event->pos());
    if (idx >= 0 && idx < m_nodes.size()) {
        emit itemClicked(m_nodes[idx].absolutePath, m_nodes[idx].isDir);
    }
}

void TreeMapChartWidget::leaveEvent(QEvent* /*event*/) {
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        update();
        emit hoveredItemChanged("", 0, 0.0);
    }
}

int TreeMapChartWidget::getNodeAt(const QPoint& pos) const {
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

QString TreeMapChartWidget::formatBytes(qint64 bytes) const {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;
    if (gb >= 1.0) return QString::number(gb, 'f', 1) + " GB";
    if (mb >= 1.0) return QString::number(mb, 'f', 1) + " MB";
    if (kb >= 1.0) return QString::number(kb, 'f', 1) + " KB";
    return QString::number(bytes) + " B";
}
