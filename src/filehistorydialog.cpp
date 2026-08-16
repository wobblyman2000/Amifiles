#include "filehistorydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include "filediffdialog.h"

FileHistoryDialog::FileHistoryDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent), m_filePath(filePath) {
    setWindowTitle("⏪ File Revision History Snapshots");
    resize(650, 420);
    setupUI();
    refreshList();
}

void FileHistoryDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 6px; padding: 4px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #313244; }"
        "QListWidget::item:selected { background-color: #45475a; color: #89b4fa; border-radius: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
    );

    QFileInfo fi(m_filePath);
    m_lblHeader = new QLabel(QString("<b>File Revisions for:</b> <code>%1</code>").arg(fi.fileName()), this);
    mainLayout->addWidget(m_lblHeader);

    m_list = new QListWidget(this);
    mainLayout->addWidget(m_list, 1);

    QHBoxLayout* btnLayout = new QHBoxLayout();

    m_btnCreate = new QPushButton("📸 Save New Snapshot Now", this);
    connect(m_btnCreate, &QPushButton::clicked, this, &FileHistoryDialog::onCreateSnapshotNow);
    btnLayout->addWidget(m_btnCreate);

    m_btnCompare = new QPushButton("👁️ Compare Diff with Current", this);
    connect(m_btnCompare, &QPushButton::clicked, this, &FileHistoryDialog::onCompareWithCurrent);
    btnLayout->addWidget(m_btnCompare);

    m_btnRestore = new QPushButton("⏪ Restore Selected Version", this);
    m_btnRestore->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnRestore, &QPushButton::clicked, this, &FileHistoryDialog::onRestoreSelected);
    btnLayout->addWidget(m_btnRestore);

    mainLayout->addLayout(btnLayout);
}

void FileHistoryDialog::refreshList() {
    m_list->clear();
    m_snapshots = FileHistoryManager::instance().getRevisions(m_filePath);

    if (m_snapshots.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem("No historical revisions found for this file yet.", m_list);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const auto& snap : m_snapshots) {
        double szKb = snap.size / 1024.0;
        QString text = QString("🕒 Snapshot: %1  (%2 KB)")
                           .arg(snap.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                           .arg(szKb, 0, 'f', 1);
        QListWidgetItem* item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, snap.snapshotPath);
    }
}

void FileHistoryDialog::onCreateSnapshotNow() {
    if (FileHistoryManager::instance().createSnapshot(m_filePath)) {
        refreshList();
        QMessageBox::information(this, "File Revision", "Snapshot saved successfully.");
    } else {
        QMessageBox::warning(this, "File Revision", "Failed to save snapshot.");
    }
}

void FileHistoryDialog::onCompareWithCurrent() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_snapshots.size()) {
        QMessageBox::information(this, "Compare Diff", "Please select a revision snapshot from the list.");
        return;
    }

    QString snapPath = m_snapshots[row].snapshotPath;
    FileDiffDialog dlg(snapPath, m_filePath, this);
    dlg.exec();
}

void FileHistoryDialog::onRestoreSelected() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_snapshots.size()) {
        QMessageBox::information(this, "Restore Revision", "Please select a revision snapshot to restore.");
        return;
    }

    QString snapPath = m_snapshots[row].snapshotPath;
    if (QMessageBox::question(this, "Restore Revision", "Are you sure you want to restore this historical version? Current file content will be replaced.", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        if (FileHistoryManager::instance().restoreRevision(m_filePath, snapPath)) {
            QMessageBox::information(this, "Restore Revision", "File restored successfully!");
            accept();
        } else {
            QMessageBox::critical(this, "Restore Revision", "Failed to restore file revision.");
        }
    }
}
