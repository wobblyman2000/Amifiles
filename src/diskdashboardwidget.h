#pragma once

#include <QWidget>
#include <QFileInfo>
#include <QMap>
#include <QList>
#include <QTableWidget>
#include <QRunnable>
#include <QMutex>

struct DashboardData {
    QMap<QString, qint64> typeSizes;
    QMap<QString, int> tagCounts;
    QList<QFileInfo> largestFiles;
    qint64 totalScanSize = 0;
};

class DiskDashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiskDashboardWidget(QWidget* parent = nullptr);
    ~DiskDashboardWidget() override = default;

    void scanDirectory(const QString& path);
    QString currentPath() const { return m_path; }

signals:
    void scanStarted();
    void scanFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onScanFinished(const DashboardData& data);

private:
    void drawDonutChart(QPainter& painter, const QRect& rect);
    void drawTagBars(QPainter& painter, const QRect& rect);

    QString m_path;
    DashboardData m_data;
    bool m_isScanning = false;
    QTableWidget* m_largestTable = nullptr;
    mutable QMutex m_mutex;
};

class DashboardWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    DashboardWorker(const QString& path);
    void run() override;

signals:
    void scanFinished(const DashboardData& data);

private:
    QString m_path;
};
