#ifndef SUNBURSTCHART_H
#define SUNBURSTCHART_H

#include <QWidget>
#include "spaceanalyzer.h"

struct ChartSegment {
    QString name;
    QString absolutePath;
    bool isDir = false;
    qint64 size = 0;
    double startAngle = 0.0;
    double spanAngle = 0.0;
    QColor color;
};

class SunburstChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit SunburstChartWidget(QWidget* parent = nullptr);
    ~SunburstChartWidget() override = default;

    void setEntries(const QList<SpaceEntry>& entries, qint64 totalSize);

signals:
    void itemClicked(const QString& path, bool isDir);
    void hoveredItemChanged(const QString& name, qint64 size, double percentage);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int getSegmentAt(const QPoint& pos) const;

    QList<ChartSegment> m_segments;
    qint64 m_totalSize = 0;
    int m_hoveredIndex = -1;
};

#endif // SUNBURSTCHART_H
