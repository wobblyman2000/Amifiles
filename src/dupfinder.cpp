#include "dupfinder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QCheckBox>
#include <QColor>
#include <QBrush>

DuplicateFinderDialog::DuplicateFinderDialog(const QString& scanDir, QWidget* parent)
    : QDialog(parent), m_scanDir(scanDir) {
    setWindowTitle("Duplicate File Finder");
    resize(700, 450);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QTableWidget { background-color: #181825; color: #cdd6f4; gridline-color: #313244; border: 1px solid #313244; }"
                  "QHeaderView::section { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 4px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; border: 1px solid #45475a; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QPushButton:disabled { background-color: #11111b; color: #585b70; }"
                  "QProgressBar { border: 1px solid #313244; border-radius: 4px; text-align: center; background-color: #181825; }"
                  "QProgressBar::chunk { background-color: #a6e3a1; }");
    setupUI();
}

void DuplicateFinderDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_lblHeader = new QLabel(QString("<b>Scan Folder:</b> %1").arg(QDir::toNativeSeparators(m_scanDir)), this);
    mainLayout->addWidget(m_lblHeader);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Delete?", "File Name", "Size", "Folder Path"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(m_table);

    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    QHBoxLayout* ctrlLayout = new QHBoxLayout();
    m_btnScan = new QPushButton("Scan Directory", this);
    connect(m_btnScan, &QPushButton::clicked, this, &DuplicateFinderDialog::onScanClicked);
    ctrlLayout->addWidget(m_btnScan);

    m_btnDelete = new QPushButton("Delete Selected Duplicates", this);
    m_btnDelete->setEnabled(false);
    connect(m_btnDelete, &QPushButton::clicked, this, &DuplicateFinderDialog::onDeleteSelectedClicked);
    ctrlLayout->addWidget(m_btnDelete);

    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    ctrlLayout->addWidget(btnClose);

    mainLayout->addLayout(ctrlLayout);
}

void DuplicateFinderDialog::onScanClicked() {
    m_btnScan->setEnabled(false);
    m_btnDelete->setEnabled(false);
    m_table->setRowCount(0);
    m_progress->setVisible(true);
    m_progress->setValue(0);

    DupScanWorker* worker = new DupScanWorker(m_scanDir, this);
    connect(worker, &DupScanWorker::progress, this, &DuplicateFinderDialog::onScanProgress);
    connect(worker, &DupScanWorker::finished, this, &DuplicateFinderDialog::onScanFinished);
    connect(worker, &DupScanWorker::finished, worker, &QThread::deleteLater);
    worker->start();
}

void DuplicateFinderDialog::onScanProgress(int current, int total, const QString& statusText) {
    Q_UNUSED(statusText);
    if (total > 0) {
        m_progress->setValue((current * 100) / total);
    }
}

void DuplicateFinderDialog::onScanFinished(const QList<DupGroup>& dupGroups) {
    m_dupGroups = dupGroups;
    m_progress->setVisible(false);

    int totalRows = 0;
    for (const DupGroup& group : dupGroups) {
        totalRows += group.filePaths.size();
    }

    m_table->setRowCount(totalRows);

    int currentRow = 0;
    bool isAlternateColor = false;

    for (const DupGroup& group : dupGroups) {
        // Distinct group color tags to improve scan visibility
        QColor bgColor = isAlternateColor ? QColor("#24273a") : QColor("#1e2030");
        isAlternateColor = !isAlternateColor;

        double kb = group.size / 1024.0;
        double mb = kb / 1024.0;
        QString sizeStr = (mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1);

        for (int i = 0; i < group.filePaths.size(); ++i) {
            const QString& path = group.filePaths.at(i);
            QFileInfo info(path);

            // Leftmost column is checkable box (to select deletion candidates)
            QTableWidgetItem* checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Unchecked);
            checkItem->setBackground(QBrush(bgColor));

            QTableWidgetItem* nameItem = new QTableWidgetItem(info.fileName());
            nameItem->setBackground(QBrush(bgColor));

            QTableWidgetItem* sizeItem = new QTableWidgetItem(sizeStr);
            sizeItem->setBackground(QBrush(bgColor));

            QTableWidgetItem* pathItem = new QTableWidgetItem(QDir::toNativeSeparators(info.absolutePath()));
            pathItem->setBackground(QBrush(bgColor));

            // To protect user files, do not check the first item by default, leaving one copy safe
            m_table->setItem(currentRow, 0, checkItem);
            m_table->setItem(currentRow, 1, nameItem);
            m_table->setItem(currentRow, 2, sizeItem);
            m_table->setItem(currentRow, 3, pathItem);

            // Store full filepath in custom role for easy deletion access
            checkItem->setData(Qt::UserRole, path);

            currentRow++;
        }
    }

    m_btnScan->setEnabled(true);
    m_btnDelete->setEnabled(totalRows > 0);

    if (totalRows == 0) {
        QMessageBox::information(this, "Scan Finished", "No duplicate files found.");
    }
}

void DuplicateFinderDialog::onDeleteSelectedClicked() {
    int deletedCount = 0;
    QList<int> rowsToDelete;

    // First collect all checked items to prevent indexing shift conflicts during runtime deletion
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* item = m_table->item(r, 0);
        if (item && item->checkState() == Qt::Checked) {
            rowsToDelete.append(r);
        }
    }

    if (rowsToDelete.isEmpty()) {
        QMessageBox::warning(this, "Delete Duplicates", "Please select at least one file to delete.");
        return;
    }

    auto confirm = QMessageBox::question(this, "Confirm Deletion",
                                         QString("Are you sure you want to permanently delete these %1 duplicate files?").arg(rowsToDelete.size()),
                                         QMessageBox::Yes | QMessageBox::No);

    if (confirm != QMessageBox::Yes) return;

    // Delete in reverse order to preserve visual indices
    for (int i = rowsToDelete.size() - 1; i >= 0; --i) {
        int row = rowsToDelete.at(i);
        QTableWidgetItem* item = m_table->item(row, 0);
        QString path = item->data(Qt::UserRole).toString();

        if (QFile::remove(path)) {
            m_table->removeRow(row);
            deletedCount++;
        }
    }

    QMessageBox::information(this, "Deletion Completed", QString("Successfully deleted %1 duplicate files.").arg(deletedCount));
}

// DupScanWorker Implementation

DupScanWorker::DupScanWorker(const QString& scanDir, QObject* parent)
    : QThread(parent), m_scanDir(scanDir) {}

void DupScanWorker::run() {
    QList<QFileInfo> allFiles;
    QDirIterator it(m_scanDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        allFiles.append(it.fileInfo());
    }

    QMap<qint64, QList<QString>> sizeGroups;
    for (const QFileInfo& info : allFiles) {
        sizeGroups[info.size()].append(info.absoluteFilePath());
    }

    QMap<qint64, QList<QString>> candidateGroups;
    for (auto it = sizeGroups.begin(); it != sizeGroups.end(); ++it) {
        if (it.value().size() > 1 && it.key() > 0) {
            candidateGroups.insert(it.key(), it.value());
        }
    }

    QList<DupGroup> finalGroups;
    int processed = 0;
    int totalCandidates = 0;
    for (const auto& list : candidateGroups) totalCandidates += list.size();

    for (auto it = candidateGroups.begin(); it != candidateGroups.end(); ++it) {
        qint64 size = it.key();
        const QList<QString>& paths = it.value();
        QMap<QString, QList<QString>> hashGroups;

        for (const QString& path : paths) {
            emit progress(processed++, totalCandidates, path);
            QString hash = calculateMD5(path);
            if (!hash.isEmpty()) {
                hashGroups[hash].append(path);
            }
        }

        for (const auto& dupList : hashGroups) {
            if (dupList.size() > 1) {
                DupGroup group;
                group.size = size;
                group.filePaths = dupList;
                finalGroups.append(group);
            }
        }
    }

    emit finished(finalGroups);
}

QString DupScanWorker::calculateMD5(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return "";
    QCryptographicHash hash(QCryptographicHash::Md5);
    if (hash.addData(&file)) {
        return hash.result().toHex();
    }
    return "";
}
