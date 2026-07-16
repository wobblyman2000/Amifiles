#ifndef FOLDERSYNC_H
#define FOLDERSYNC_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QFileInfo>
#include <QThread>

struct SyncItem {
    QString relativePath;
    QFileInfo leftInfo;
    QFileInfo rightInfo;
    QString status; // Match, Left Only, Right Only, Left Newer, Right Newer, Size Mismatch
};

class FolderSyncDialog : public QDialog {
    Q_OBJECT
public:
    FolderSyncDialog(const QString& leftDir, const QString& rightDir, QWidget* parent = nullptr);
    ~FolderSyncDialog() override = default;

private slots:
    void onCompareClicked();
    void onSyncClicked();
    void onCompareFinished(const QList<SyncItem>& items);
    void onSyncProgress(int current, int total, const QString& filename);
    void onSyncFinished();

private:
    void setupUI();

    QString m_leftDir;
    QString m_rightDir;

    QLabel* m_lblLeftDir = nullptr;
    QLabel* m_lblRightDir = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_btnCompare = nullptr;
    QPushButton* m_btnSync = nullptr;
    QProgressBar* m_progress = nullptr;
    QCheckBox* m_chkRecursive = nullptr;
    QComboBox* m_cmbDirection = nullptr;
    QLabel* m_lblStatMatch = nullptr;
    QLabel* m_lblStatLeftOnly = nullptr;
    QLabel* m_lblStatRightOnly = nullptr;
    QLabel* m_lblStatConflicts = nullptr;

    QList<SyncItem> m_syncItems;
};

class CompareWorker : public QThread {
    Q_OBJECT
public:
    CompareWorker(const QString& left, const QString& right, bool recursive, QObject* parent = nullptr);
    void run() override;

signals:
    void finished(const QList<SyncItem>& items);

private:
    QString m_left;
    QString m_right;
    bool m_recursive;
};

class SyncWorker : public QThread {
    Q_OBJECT
public:
    SyncWorker(const QString& left, const QString& right, const QList<SyncItem>& items, int direction, QObject* parent = nullptr);
    void run() override;

signals:
    void progress(int current, int total, const QString& filename);
    void finished();

private:
    QString m_left;
    QString m_right;
    QList<SyncItem> m_items;
    int m_direction;
};

#endif // FOLDERSYNC_H
