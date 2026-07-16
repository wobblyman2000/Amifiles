#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QList>
#include "syncscheduler.h"

class SchedulerDialog : public QDialog {
    Q_OBJECT
public:
    explicit SchedulerDialog(QWidget* parent = nullptr);
    ~SchedulerDialog() override = default;

private slots:
    void onAddJob();
    void onDeleteJob();
    void onBrowseSource(int row);
    void onBrowseDest(int row);
    void onSave();

private:
    void setupUI();
    void populateTable();

    QTableWidget* m_table = nullptr;
    QList<SyncJob> m_jobs;
};
