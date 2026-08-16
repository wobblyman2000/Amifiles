#ifndef FOLDERINTEGRITYDIALOG_H
#define FOLDERINTEGRITYDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QThread>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QComboBox>

struct IntegrityItem {
    QString relativePath;
    QString absolutePath;
    QString expectedHash;
    QString actualHash;
    qint64 size = 0;
    enum Status { Intact, Corrupted, Missing, NewFile } status;
};

class FolderIntegrityWorker : public QThread {
    Q_OBJECT
public:
    FolderIntegrityWorker(bool isAudit, const QString& dirPath, QObject* parent = nullptr);
    void run() override;

signals:
    void progress(int current, int total, const QString& currentFile);
    void finished(bool success, const QList<IntegrityItem>& results, const QString& message);

private:
    bool m_isAudit;
    QString m_dirPath;
};

class FolderIntegrityDialog : public QDialog {
    Q_OBJECT
public:
    explicit FolderIntegrityDialog(const QString& dirPath, bool autoAudit = false, QWidget* parent = nullptr);
    ~FolderIntegrityDialog() override = default;

private slots:
    void onGenerateSnapshot();
    void onAuditIntegrity();
    void onWorkerProgress(int current, int total, const QString& currentFile);
    void onWorkerFinished(bool success, const QList<IntegrityItem>& results, const QString& message);
    void onFilterChanged(int index);
    void onExportReport();

private:
    void setupUI();
    void updateSummaryLabels();

    QString m_dirPath;
    QTreeWidget* m_tree = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_lblStatus = nullptr;
    QLabel* m_lblSummary = nullptr;
    QPushButton* m_btnGenerate = nullptr;
    QPushButton* m_btnAudit = nullptr;
    QPushButton* m_btnExport = nullptr;
    QComboBox* m_comboFilter = nullptr;

    FolderIntegrityWorker* m_worker = nullptr;
    QList<IntegrityItem> m_currentResults;
};

#endif // FOLDERINTEGRITYDIALOG_H
