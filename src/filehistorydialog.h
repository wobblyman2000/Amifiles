#ifndef FILEHISTORYDIALOG_H
#define FILEHISTORYDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include "filehistorymanager.h"

class FileHistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileHistoryDialog(const QString& filePath, QWidget* parent = nullptr);
    ~FileHistoryDialog() override = default;

private slots:
    void onCompareWithCurrent();
    void onRestoreSelected();
    void onCreateSnapshotNow();
    void refreshList();

private:
    void setupUI();

    QString m_filePath;
    QListWidget* m_list = nullptr;
    QLabel* m_lblHeader = nullptr;
    QPushButton* m_btnCompare = nullptr;
    QPushButton* m_btnRestore = nullptr;
    QPushButton* m_btnCreate = nullptr;

    QList<RevisionSnapshot> m_snapshots;
};

#endif // FILEHISTORYDIALOG_H
