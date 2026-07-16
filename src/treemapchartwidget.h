#pragma once

#include <QWidget>
#include "spaceanalyzer.h"

struct TreeMapNode {
    QString name;
    QString absolutePath;
    bool isDir = false;
    qint64 size = 0;
    QRect rect;
    QColor color;
};

class TreeMapChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit TreeMapChartWidget(QWidget* parent = nullptr);
    ~TreeMapChartWidget() override = default;

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
    void layoutTreeMap();
    int getNodeAt(const QPoint& pos) const;
    QString formatBytes(qint64 bytes) const;

    QList<SpaceEntry> m_entries;
    QList<TreeMapNode> m_nodes;
    qint64 m_totalSize = 0;
    int m_hoveredIndex = -1;
};
