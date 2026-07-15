#include "sunburstchart.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SunburstChartWidget::SunburstChartWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true); // Enable mouse move notifications
    resize(300, 300);
}

void SunburstChartWidget::setEntries(const QList<SpaceEntry>& entries, qint64 totalSize) {
    m_totalSize = totalSize;
    m_segments.clear();
    m_hoveredIndex = -1;

    if (m_totalSize <= 0) {
        update();
        return;
    }

    // Curated Catppuccin Mocha color map for segments
    QList<QColor> colors = {
        QColor("#89b4fa"), // Blue
        QColor("#a6e3a1"), // Green
        QColor("#f9e2af"), // Yellow
        QColor("#fab387"), // Orange
        QColor("#f38ba8"), // Red
        QColor("#cba6f7"), // Mauve
        QColor("#89dceb"), // Sky
        QColor("#f5c2e7"), // Pink
        QColor("#b4befe")  // Lavender
    };

    double currentAngle = 0.0;
    int colorIdx = 0;

    for (const SpaceEntry& entry : entries) {
        double span = (entry.size * 360.0) / m_totalSize;
        if (span < 0.5) continue; // Skip extremely tiny slices to avoid clutter

        ChartSegment seg;
        seg.name = entry.name;
        seg.absolutePath = entry.absolutePath;
        seg.isDir = entry.isDir;
        seg.size = entry.size;
        seg.startAngle = currentAngle;
        seg.spanAngle = span;
        seg.color = colors[colorIdx % colors.size()];

        m_segments.append(seg);
        currentAngle += span;
        colorIdx++;
    }

    update();
}

void SunburstChartWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPointF center(width() / 2.0, height() / 2.0);
    double outerRadius = qMin(width(), height()) / 2.0 - 15.0;
    double innerRadius = outerRadius * 0.45;

    if (m_segments.isEmpty()) {
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(rect(), Qt::AlignCenter, "No files or folder sizes to visualize.");
        return;
    }

    QRectF boundingStyleRect(center.x() - outerRadius, center.y() - outerRadius, outerRadius * 2, outerRadius * 2);

    // 1. Draw Donut Segments
    for (int i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        
        QBrush brush(seg.color);
        painter.setBrush(brush);
        
        if (m_hoveredIndex == i) {
            // Draw slightly highlighted/exploded for visual feedback
            painter.setPen(QPen(Qt::white, 2));
            QRectF hoverRect = boundingStyleRect.adjusted(-3, -3, 3, 3);
            painter.drawPie(hoverRect, qRound(seg.startAngle * 16), qRound(seg.spanAngle * 16));
        } else {
            painter.setPen(QPen(QColor("#1e1e2e"), 1)); // Clean separation line
            painter.drawPie(boundingStyleRect, qRound(seg.startAngle * 16), qRound(seg.spanAngle * 16));
        }
    }

    // 2. Draw Center Donut Hole to display HUD info
    painter.setBrush(QColor("#181825"));
    painter.setPen(QPen(QColor("#313244"), 2));
    painter.drawEllipse(center, innerRadius, innerRadius);

    // Helper text formatter for bytes
    auto formatBytes = [](qint64 bytes) -> QString {
        double kb = bytes / 1024.0;
        double mb = kb / 1024.0;
        double gb = mb / 1024.0;
        if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
        if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
        if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
        return QString("%1 B").arg(bytes);
    };

    // 3. Draw HUD Labels inside the donut hole
    QRectF hudRect(center.x() - innerRadius, center.y() - innerRadius, innerRadius * 2, innerRadius * 2);
    painter.setPen(QColor("#cdd6f4"));

    if (m_hoveredIndex != -1 && m_hoveredIndex < m_segments.size()) {
        const auto& seg = m_segments[m_hoveredIndex];
        double pct = (seg.size * 100.0) / m_totalSize;

        QFont fTitle = painter.font();
        fTitle.setPointSize(9);
        fTitle.setBold(true);
        painter.setFont(fTitle);
        // Truncate long names inside HUD
        QString name = seg.name;
        if (name.length() > 14) name = name.left(12) + "..";
        painter.drawText(hudRect.adjusted(5, 10, -5, -innerRadius + 10), Qt::AlignCenter | Qt::ElideRight, name);

        QFont fVal = painter.font();
        fVal.setPointSize(8);
        fVal.setBold(false);
        painter.setFont(fVal);
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(hudRect, Qt::AlignCenter, formatBytes(seg.size));

        painter.setPen(seg.color);
        painter.drawText(hudRect.adjusted(5, innerRadius - 10, -5, -5), Qt::AlignCenter, QString("%1%").arg(pct, 0, 'f', 1));
    } else {
        QFont fTitle = painter.font();
        fTitle.setPointSize(8);
        fTitle.setBold(false);
        painter.setFont(fTitle);
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(hudRect.adjusted(0, 10, 0, -innerRadius + 15), Qt::AlignCenter, "Total Size");

        QFont fVal = painter.font();
        fVal.setPointSize(10);
        fVal.setBold(true);
        painter.setFont(fVal);
        painter.setPen(QColor("#89b4fa"));
        painter.drawText(hudRect, Qt::AlignCenter, formatBytes(m_totalSize));
    }
}

int SunburstChartWidget::getSegmentAt(const QPoint& pos) const {
    QPoint center(width() / 2, height() / 2);
    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();
    double dist = sqrt(dx*dx + dy*dy);

    double outerRadius = qMin(width(), height()) / 2.0 - 15.0;
    double innerRadius = outerRadius * 0.45;

    if (dist < innerRadius || dist > outerRadius) {
        return -1;
    }

    // Compute angle in degrees (atan2 dy is inverted because y increases downward)
    double angle = atan2(-dy, dx) * 180.0 / M_PI;
    if (angle < 0) angle += 360.0;

    for (int i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        if (angle >= seg.startAngle && angle < (seg.startAngle + seg.spanAngle)) {
            return i;
        }
    }
    return -1;
}

void SunburstChartWidget::mouseMoveEvent(QMouseEvent* event) {
    int idx = getSegmentAt(event->pos());
    if (m_hoveredIndex != idx) {
        m_hoveredIndex = idx;
        if (m_hoveredIndex != -1 && m_hoveredIndex < m_segments.size()) {
            const auto& seg = m_segments[m_hoveredIndex];
            double pct = (seg.size * 100.0) / m_totalSize;
            emit hoveredItemChanged(seg.name, seg.size, pct);
        }
        update();
    }
}

void SunburstChartWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int idx = getSegmentAt(event->pos());
        if (idx != -1 && idx < m_segments.size()) {
            const auto& seg = m_segments[idx];
            emit itemClicked(seg.absolutePath, seg.isDir);
        }
    }
}

void SunburstChartWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        update();
    }
}
