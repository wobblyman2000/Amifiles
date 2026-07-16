#include "foldersync.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDirIterator>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QFile>

FolderSyncDialog::FolderSyncDialog(const QString& leftDir, const QString& rightDir, QWidget* parent)
    : QDialog(parent), m_leftDir(leftDir), m_rightDir(rightDir) {
    setWindowTitle("Folder Compare & Sync");
    resize(850, 540);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                   "QLabel { color: #cdd6f4; }"
                   "QCheckBox { color: #cdd6f4; }"
                   "QFrame { background-color: #11111b; border: 1px solid #313244; border-radius: 6px; }"
                   "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 4px; border-radius: 4px; }"
                   "QTableWidget { background-color: #181825; color: #cdd6f4; gridline-color: #313244; border: 1px solid #313244; }"
                   "QHeaderView::section { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 4px; }"
                   "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; border: 1px solid #45475a; }"
                   "QPushButton:hover { background-color: #45475a; }"
                   "QPushButton:disabled { background-color: #11111b; color: #585b70; }"
                   "QProgressBar { border: 1px solid #313244; border-radius: 4px; text-align: center; background-color: #181825; }"
                   "QProgressBar::chunk { background-color: #a6e3a1; }");
    setupUI();
}

void FolderSyncDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* dirsLayout = new QHBoxLayout();
    m_lblLeftDir = new QLabel(QString("<b>Left:</b> %1").arg(QDir::toNativeSeparators(m_leftDir)), this);
    m_lblRightDir = new QLabel(QString("<b>Right:</b> %1").arg(QDir::toNativeSeparators(m_rightDir)), this);
    dirsLayout->addWidget(m_lblLeftDir, 1);
    dirsLayout->addWidget(m_lblRightDir, 1);
    mainLayout->addLayout(dirsLayout);

    QHBoxLayout* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(10);

    auto createStatCard = [this](const QString& title, QLabel*& lblOut, const QString& color) {
        QFrame* card = new QFrame(this);
        card->setStyleSheet(QString("QFrame { border: 1.5px solid %1; border-radius: 6px; padding: 4px; background-color: #11111b; }").arg(color));
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 4, 8, 4);
        cardLayout->setSpacing(2);

        QLabel* lblTitle = new QLabel(title, this);
        lblTitle->setStyleSheet("font-size: 11px; color: #a6adc8; font-weight: bold; border: none; background: transparent;");
        lblOut = new QLabel("0", this);
        lblOut->setStyleSheet("font-size: 18px; color: #ffffff; font-weight: bold; border: none; background: transparent;");

        cardLayout->addWidget(lblTitle);
        cardLayout->addWidget(lblOut);
        return card;
    };

    statsLayout->addWidget(createStatCard("MATCHES", m_lblStatMatch, "#a6e3a1"));
    statsLayout->addWidget(createStatCard("LEFT ONLY", m_lblStatLeftOnly, "#f9e2af"));
    statsLayout->addWidget(createStatCard("RIGHT ONLY", m_lblStatRightOnly, "#f9e2af"));
    statsLayout->addWidget(createStatCard("CONFLICTS / MISMATCH", m_lblStatConflicts, "#f38ba8"));

    mainLayout->addLayout(statsLayout);

    QHBoxLayout* optsLayout = new QHBoxLayout();
    m_chkRecursive = new QCheckBox("Recursive Comparison", this);
    m_chkRecursive->setChecked(true);
    optsLayout->addWidget(m_chkRecursive);

    optsLayout->addSpacing(20);
    optsLayout->addWidget(new QLabel("Sync Direction:", this));
    m_cmbDirection = new QComboBox(this);
    m_cmbDirection->addItems({"Left to Right (->)", "Right to Left (<-)", "Bidirectional (Sync Newer)"});
    optsLayout->addWidget(m_cmbDirection);
    optsLayout->addStretch(1);
    mainLayout->addLayout(optsLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Relative Path", "Left Mod / Size", "Right Mod / Size", "Status"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_table);

    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    QHBoxLayout* ctrlLayout = new QHBoxLayout();
    m_btnCompare = new QPushButton("Compare Folder Contents", this);
    connect(m_btnCompare, &QPushButton::clicked, this, &FolderSyncDialog::onCompareClicked);
    ctrlLayout->addWidget(m_btnCompare);

    m_btnSync = new QPushButton("Synchronize Files", this);
    m_btnSync->setEnabled(false);
    connect(m_btnSync, &QPushButton::clicked, this, &FolderSyncDialog::onSyncClicked);
    ctrlLayout->addWidget(m_btnSync);

    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    ctrlLayout->addWidget(btnClose);

    mainLayout->addLayout(ctrlLayout);
}

void FolderSyncDialog::onCompareClicked() {
    m_btnCompare->setEnabled(false);
    m_btnSync->setEnabled(false);
    m_table->setRowCount(0);

    CompareWorker* worker = new CompareWorker(m_leftDir, m_rightDir, m_chkRecursive->isChecked(), this);
    connect(worker, &CompareWorker::finished, this, &FolderSyncDialog::onCompareFinished);
    connect(worker, &CompareWorker::finished, worker, &QThread::deleteLater);
    worker->start();
}

void FolderSyncDialog::onCompareFinished(const QList<SyncItem>& items) {
    m_syncItems = items;
    m_table->setRowCount(items.size());

    int countMatch = 0;
    int countLeftOnly = 0;
    int countRightOnly = 0;
    int countConflicts = 0;

    for (int i = 0; i < items.size(); ++i) {
        const SyncItem& item = items.at(i);
        
        QTableWidgetItem* itemPath = new QTableWidgetItem(item.relativePath);
        
        QString leftDetails = "";
        if (item.leftInfo.exists()) {
            leftDetails = QString("%1 (%2)")
                .arg(item.leftInfo.lastModified().toString("yyyy-MM-dd hh:mm"))
                .arg(item.leftInfo.isFile() ? QString::number(item.leftInfo.size()) : "DIR");
        }
        QTableWidgetItem* itemLeft = new QTableWidgetItem(leftDetails);

        QString rightDetails = "";
        if (item.rightInfo.exists()) {
            rightDetails = QString("%1 (%2)")
                .arg(item.rightInfo.lastModified().toString("yyyy-MM-dd hh:mm"))
                .arg(item.rightInfo.isFile() ? QString::number(item.rightInfo.size()) : "DIR");
        }
        QTableWidgetItem* itemRight = new QTableWidgetItem(rightDetails);

        QString statusText = item.status;
        QColor statusColor = QColor("#cdd6f4");

        if (item.status == "Match") {
            statusText = "✔ Match";
            statusColor = QColor("#a6e3a1");
            countMatch++;
        } else if (item.status == "Left Only") {
            statusText = "➔ Left Only";
            statusColor = QColor("#f9e2af");
            countLeftOnly++;
        } else if (item.status == "Right Only") {
            statusText = "◀ Right Only";
            statusColor = QColor("#f9e2af");
            countRightOnly++;
        } else if (item.status == "Left Newer") {
            statusText = "☀ Left Newer";
            statusColor = QColor("#fab387");
            countConflicts++;
        } else if (item.status == "Right Newer") {
            statusText = "☀ Right Newer";
            statusColor = QColor("#fab387");
            countConflicts++;
        } else if (item.status == "Size Mismatch") {
            statusText = "✖ Mismatch";
            statusColor = QColor("#f38ba8");
            countConflicts++;
        } else {
            countConflicts++;
        }

        QTableWidgetItem* itemStatus = new QTableWidgetItem(statusText);
        itemStatus->setForeground(QBrush(statusColor));
        itemStatus->setFont(QFont("", -1, QFont::Bold));

        m_table->setItem(i, 0, itemPath);
        m_table->setItem(i, 1, itemLeft);
        m_table->setItem(i, 2, itemRight);
        m_table->setItem(i, 3, itemStatus);
    }

    if (m_lblStatMatch) m_lblStatMatch->setText(QString::number(countMatch));
    if (m_lblStatLeftOnly) m_lblStatLeftOnly->setText(QString::number(countLeftOnly));
    if (m_lblStatRightOnly) m_lblStatRightOnly->setText(QString::number(countRightOnly));
    if (m_lblStatConflicts) m_lblStatConflicts->setText(QString::number(countConflicts));

    m_btnCompare->setEnabled(true);
    m_btnSync->setEnabled(!items.isEmpty());
}

void FolderSyncDialog::onSyncClicked() {
    m_btnCompare->setEnabled(false);
    m_btnSync->setEnabled(false);
    m_progress->setVisible(true);
    m_progress->setValue(0);

    SyncWorker* worker = new SyncWorker(m_leftDir, m_rightDir, m_syncItems, m_cmbDirection->currentIndex(), this);
    connect(worker, &SyncWorker::progress, this, &FolderSyncDialog::onSyncProgress);
    connect(worker, &SyncWorker::finished, this, &FolderSyncDialog::onSyncFinished);
    connect(worker, &SyncWorker::finished, worker, &QThread::deleteLater);
    worker->start();
}

void FolderSyncDialog::onSyncProgress(int current, int total, const QString& filename) {
    Q_UNUSED(filename);
    if (total > 0) {
        m_progress->setValue((current * 100) / total);
    }
}

void FolderSyncDialog::onSyncFinished() {
    m_progress->setVisible(false);
    m_btnCompare->setEnabled(true);
    QMessageBox::information(this, "Synchronization", "Folder synchronization completed successfully.");
    onCompareClicked();
}

// CompareWorker Implementation

CompareWorker::CompareWorker(const QString& left, const QString& right, bool recursive, QObject* parent)
    : QThread(parent), m_left(left), m_right(right), m_recursive(recursive) {}

void CompareWorker::run() {
    QHash<QString, SyncItem> matches;
    QDir leftDir(m_left);
    QDir rightDir(m_right);

    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    if (m_recursive) flags = QDirIterator::Subdirectories;

    QDirIterator itLeft(m_left, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, flags);
    while (itLeft.hasNext()) {
        itLeft.next();
        QString relPath = leftDir.relativeFilePath(itLeft.filePath());
        SyncItem item;
        item.relativePath = relPath;
        item.leftInfo = itLeft.fileInfo();
        matches.insert(relPath, item);
    }

    QDirIterator itRight(m_right, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, flags);
    while (itRight.hasNext()) {
        itRight.next();
        QString relPath = rightDir.relativeFilePath(itRight.filePath());
        if (matches.contains(relPath)) {
            matches[relPath].rightInfo = itRight.fileInfo();
        } else {
            SyncItem item;
            item.relativePath = relPath;
            item.rightInfo = itRight.fileInfo();
            matches.insert(relPath, item);
        }
    }

    QList<SyncItem> result;
    for (auto it = matches.begin(); it != matches.end(); ++it) {
        SyncItem& item = it.value();
        if (item.leftInfo.exists() && !item.rightInfo.exists()) {
            item.status = "Left Only";
        } else if (!item.leftInfo.exists() && item.rightInfo.exists()) {
            item.status = "Right Only";
        } else if (item.leftInfo.isDir() && item.rightInfo.isDir()) {
            item.status = "Match";
        } else if (item.leftInfo.isFile() && item.rightInfo.isFile()) {
            if (item.leftInfo.size() != item.rightInfo.size()) {
                item.status = "Size Mismatch";
            } else {
                qint64 diff = item.leftInfo.lastModified().secsTo(item.rightInfo.lastModified());
                if (qAbs(diff) < 2) {
                    item.status = "Match";
                } else if (diff > 0) {
                    item.status = "Right Newer";
                } else {
                    item.status = "Left Newer";
                }
            }
        } else {
            item.status = "Type Mismatch";
        }

        if (item.status != "Match") {
            result.append(item);
        }
    }

    emit finished(result);
}

// SyncWorker Implementation

SyncWorker::SyncWorker(const QString& left, const QString& right, const QList<SyncItem>& items, int direction, QObject* parent)
    : QThread(parent), m_left(left), m_right(right), m_items(items), m_direction(direction) {}

void SyncWorker::run() {
    int total = m_items.size();
    for (int i = 0; i < total; ++i) {
        const SyncItem& item = m_items.at(i);
        emit progress(i, total, item.relativePath);

        QString leftPath = m_left + "/" + item.relativePath;
        QString rightPath = m_right + "/" + item.relativePath;

        bool copyLeftToRight = false;
        bool copyRightToLeft = false;

        if (m_direction == 0) {
            if (item.leftInfo.exists()) {
                copyLeftToRight = true;
            } else if (item.rightInfo.exists()) {
                QFile::remove(rightPath);
            }
        } else if (m_direction == 1) {
            if (item.rightInfo.exists()) {
                copyRightToLeft = true;
            } else if (item.leftInfo.exists()) {
                QFile::remove(leftPath);
            }
        } else if (m_direction == 2) {
            if (item.status == "Left Only" || item.status == "Left Newer") {
                copyLeftToRight = true;
            } else if (item.status == "Right Only" || item.status == "Right Newer") {
                copyRightToLeft = true;
            } else if (item.status == "Size Mismatch") {
                copyLeftToRight = true;
            }
        }

        if (copyLeftToRight) {
            if (item.leftInfo.isDir()) {
                QDir().mkpath(rightPath);
            } else {
                QDir().mkpath(QFileInfo(rightPath).absolutePath());
                QFile::remove(rightPath);
                QFile::copy(leftPath, rightPath);
                QFile(rightPath).setFileTime(item.leftInfo.lastModified(), QFileDevice::FileModificationTime);
            }
        } else if (copyRightToLeft) {
            if (item.rightInfo.isDir()) {
                QDir().mkpath(leftPath);
            } else {
                QDir().mkpath(QFileInfo(leftPath).absolutePath());
                QFile::remove(leftPath);
                QFile::copy(rightPath, leftPath);
                QFile(leftPath).setFileTime(item.rightInfo.lastModified(), QFileDevice::FileModificationTime);
            }
        }
    }
    emit progress(total, total, "");
    emit finished();
}
