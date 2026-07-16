#include "schedulerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QUuid>

SchedulerDialog::SchedulerDialog(QWidget* parent) : QDialog(parent) {
    m_jobs = SyncScheduler::instance().jobs();
    setupUI();
    populateTable();
}

void SchedulerDialog::setupUI() {
    setWindowTitle("Configure Folder Backup Sync Jobs");
    resize(850, 450);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel* title = new QLabel("Configure background folders mirror-synchronization and automated backup routines:", this);
    title->setStyleSheet("font-weight: bold; font-size: 13px; color: #fba387; margin-bottom: 6px;");
    mainLayout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Job Name", "Source Directory", "Destination / Backup Directory", "Sync Interval", "Active"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setStyleSheet("QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; gridline-color: #313244; }"
                           "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    mainLayout->addWidget(m_table);

    QHBoxLayout* controlLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("Add New Job", this);
    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    QPushButton* btnSave = new QPushButton("Save Schedule", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnDelete->setStyleSheet("QPushButton { background-color: #313244; color: #f38ba8; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #f38ba8; color: #11111b; }");
    btnSave->setStyleSheet("QPushButton { background-color: #fab387; color: #11111b; font-weight: bold; border: 1px solid #fab387; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #f9e2af; }");
    btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #45475a; }");

    connect(btnAdd, &QPushButton::clicked, this, &SchedulerDialog::onAddJob);
    connect(btnDelete, &QPushButton::clicked, this, &SchedulerDialog::onDeleteJob);
    connect(btnSave, &QPushButton::clicked, this, &SchedulerDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    controlLayout->addWidget(btnAdd);
    controlLayout->addWidget(btnDelete);
    controlLayout->addStretch();
    controlLayout->addWidget(btnSave);
    controlLayout->addWidget(btnCancel);
    mainLayout->addLayout(controlLayout);
}

void SchedulerDialog::populateTable() {
    m_table->clearContents();
    m_table->setRowCount(m_jobs.size());

    for (int i = 0; i < m_jobs.size(); ++i) {
        const SyncJob& job = m_jobs[i];

        // 0. Name
        QLineEdit* nameEdit = new QLineEdit(this);
        nameEdit->setText(job.name);
        nameEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 2px; }");
        m_table->setCellWidget(i, 0, nameEdit);

        // 1. Source Layout
        QWidget* srcWidget = new QWidget(this);
        QHBoxLayout* srcLayout = new QHBoxLayout(srcWidget);
        srcLayout->setContentsMargins(0, 0, 0, 0);
        srcLayout->setSpacing(2);
        QLineEdit* srcEdit = new QLineEdit(srcWidget);
        srcEdit->setText(job.sourcePath);
        srcEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 2px; }");
        QPushButton* srcBtn = new QPushButton("Browse...", srcWidget);
        srcBtn->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; padding: 2px 6px; }");
        connect(srcBtn, &QPushButton::clicked, this, [this, i]() { onBrowseSource(i); });
        srcLayout->addWidget(srcEdit, 1);
        srcLayout->addWidget(srcBtn);
        m_table->setCellWidget(i, 1, srcWidget);

        // 2. Dest Layout
        QWidget* destWidget = new QWidget(this);
        QHBoxLayout* destLayout = new QHBoxLayout(destWidget);
        destLayout->setContentsMargins(0, 0, 0, 0);
        destLayout->setSpacing(2);
        QLineEdit* destEdit = new QLineEdit(destWidget);
        destEdit->setText(job.destPath);
        destEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 2px; }");
        QPushButton* destBtn = new QPushButton("Browse...", destWidget);
        destBtn->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; padding: 2px 6px; }");
        connect(destBtn, &QPushButton::clicked, this, [this, i]() { onBrowseDest(i); });
        destLayout->addWidget(destEdit, 1);
        destLayout->addWidget(destBtn);
        m_table->setCellWidget(i, 2, destWidget);

        // 3. Interval ComboBox
        QComboBox* intCombo = new QComboBox(this);
        intCombo->addItem("Every 5 mins", 5);
        intCombo->addItem("Every 15 mins", 15);
        intCombo->addItem("Every 30 mins", 30);
        intCombo->addItem("Every 1 hr", 60);
        intCombo->addItem("Every 6 hrs", 360);
        intCombo->addItem("Every 12 hrs", 720);
        intCombo->addItem("Every 24 hrs", 1440);
        intCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        
        int findIdx = intCombo->findData(job.intervalMinutes);
        if (findIdx != -1) {
            intCombo->setCurrentIndex(findIdx);
        } else {
            intCombo->setCurrentIndex(3); // Default 1 hour
        }
        m_table->setCellWidget(i, 3, intCombo);

        // 4. Active CheckBox
        QCheckBox* actCheck = new QCheckBox(this);
        actCheck->setChecked(job.active);
        actCheck->setStyleSheet("QCheckBox::indicator { width: 16px; height: 16px; }");
        m_table->setCellWidget(i, 4, actCheck);
    }
}

void SchedulerDialog::onAddJob() {
    SyncJob job;
    job.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job.name = "New Backup Job";
    job.intervalMinutes = 60;
    job.active = true;
    m_jobs.append(job);
    populateTable();
}

void SchedulerDialog::onDeleteJob() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_jobs.size()) {
        QMessageBox::warning(this, "No Selection", "Please click a row to delete.");
        return;
    }
    m_jobs.removeAt(row);
    populateTable();
}

void SchedulerDialog::onBrowseSource(int row) {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Source Sync Folder");
    if (!dir.isEmpty()) {
        QWidget* w = m_table->cellWidget(row, 1);
        QLineEdit* edit = w->findChild<QLineEdit*>();
        if (edit) {
            edit->setText(dir);
        }
    }
}

void SchedulerDialog::onBrowseDest(int row) {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Backup Folder");
    if (!dir.isEmpty()) {
        QWidget* w = m_table->cellWidget(row, 2);
        QLineEdit* edit = w->findChild<QLineEdit*>();
        if (edit) {
            edit->setText(dir);
        }
    }
}

void SchedulerDialog::onSave() {
    QList<SyncJob> nextJobs;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        SyncJob job;
        job.id = m_jobs[i].id;
        job.lastRun = m_jobs[i].lastRun;

        QLineEdit* nameEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 0));
        job.name = nameEdit ? nameEdit->text().trimmed() : "Job";

        QWidget* srcW = m_table->cellWidget(i, 1);
        QLineEdit* srcEdit = srcW ? srcW->findChild<QLineEdit*>() : nullptr;
        job.sourcePath = srcEdit ? srcEdit->text().trimmed() : "";

        QWidget* destW = m_table->cellWidget(i, 2);
        QLineEdit* destEdit = destW ? destW->findChild<QLineEdit*>() : nullptr;
        job.destPath = destEdit ? destEdit->text().trimmed() : "";

        QComboBox* intCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 3));
        job.intervalMinutes = intCombo ? intCombo->currentData().toInt() : 60;

        QCheckBox* actCheck = qobject_cast<QCheckBox*>(m_table->cellWidget(i, 4));
        job.active = actCheck ? actCheck->isChecked() : true;

        if (job.name.isEmpty() || job.sourcePath.isEmpty() || job.destPath.isEmpty()) {
            QMessageBox::warning(this, "Invalid Job Fields", QString("Backup job #%1 contains empty fields. Please set them or delete the job.").arg(i + 1));
            return;
        }

        nextJobs.append(job);
    }

    SyncScheduler::instance().setJobs(nextJobs);
    accept();
}
