#ifndef SPACEANALYZER_H
#define SPACEANALYZER_H

#include <QDialog>
#include <QThread>
#include <QTreeWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMutex>

struct SpaceEntry {
    QString name;
    QString absolutePath;
    bool isDir = false;
    qint64 size = 0;
};

// Thread worker to recursively calculate child sizes
class SpaceAnalyzerWorker : public QThread {
    Q_OBJECT
public:
    explicit SpaceAnalyzerWorker(const QString& dirPath, QObject* parent = nullptr);
    ~SpaceAnalyzerWorker() override;

    void cancel();

signals:
    void progressUpdate(const QString& currentItem, int count);
    void finished(const QList<SpaceEntry>& entries, qint64 totalSize);

protected:
    void run() override;

private:
    qint64 calculateSize(const QString& path);

    QString m_dirPath;
    bool m_cancel = false;
    QMutex m_mutex;
    int m_fileCount = 0;
};

// Visual Space Analyzer Dialog
class SpaceAnalyzerDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpaceAnalyzerDialog(const QString& startPath, QWidget* parent = nullptr);
    ~SpaceAnalyzerDialog() override;

    QString selectedPath() const { return m_selectedPath; }
    bool navigateRequested() const { return m_navigateRequested; }

private slots:
    void onScanFinished(const QList<SpaceEntry>& entries, qint64 totalSize);
    void onProgressUpdate(const QString& currentItem, int count);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onBackClicked();
    void onNavigateClicked();
    void onChartItemClicked(const QString& path, bool isDir);
    void onChartHoveredItemChanged(const QString& name, qint64 size, double percentage);
    void onToggleViewMode();

private:
    void startScan(const QString& path);
    void setupUI();
    QString formatBytes(qint64 bytes) const;

    QString m_currentPath;
    QString m_selectedPath;
    bool m_navigateRequested = false;
    QStringList m_history;

    SpaceAnalyzerWorker* m_worker = nullptr;

    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_scanProgress = nullptr;
    QTreeWidget* m_tree = nullptr;
    class SunburstChartWidget* m_chart = nullptr;
    class TreeMapChartWidget* m_treemap = nullptr;
    class QStackedWidget* m_viewStack = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnNavigate = nullptr;
    QPushButton* m_btnToggleView = nullptr;
};

#endif // SPACEANALYZER_H
