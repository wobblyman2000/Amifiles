#ifndef DUPFINDER_H
#define DUPFINDER_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QThread>
#include <QFileInfo>
#include <QList>

struct DupGroup {
    qint64 size;
    QList<QString> filePaths;
};

class DuplicateFinderDialog : public QDialog {
    Q_OBJECT
public:
    DuplicateFinderDialog(const QString& scanDir, QWidget* parent = nullptr);
    ~DuplicateFinderDialog() override = default;

private slots:
    void onScanClicked();
    void onDeleteSelectedClicked();
    void onScanProgress(int current, int total, const QString& statusText);
    void onScanFinished(const QList<DupGroup>& dupGroups);
    void applySmartSelection();
    void updateRecoveryStats();

private:
    void setupUI();

    QString m_scanDir;
    QList<DupGroup> m_dupGroups;
    qint64 m_totalRedundantSpace = 0;

    QLabel* m_lblHeader = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_btnScan = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QProgressBar* m_progress = nullptr;

    class QComboBox* m_comboSmartSelect = nullptr;
    QLabel* m_lblSpaceStats = nullptr;
};

class DupScanWorker : public QThread {
    Q_OBJECT
public:
    DupScanWorker(const QString& scanDir, QObject* parent = nullptr);
    void run() override;

signals:
    void progress(int current, int total, const QString& statusText);
    void finished(const QList<DupGroup>& dupGroups);

private:
    QString m_scanDir;
    QString calculateMD5(const QString& filePath);
};

#endif // DUPFINDER_H
