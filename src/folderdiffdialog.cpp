#include "folderdiffdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QHeaderView>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>

FolderDiffDialog::FolderDiffDialog(const QString& leftPath, const QString& rightPath, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Folder Comparator & Synchronizer");
    resize(750, 480);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-weight: bold; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QTreeWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }"
        "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
        "QProgressBar { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; text-align: center; color: #ffffff; }"
        "QProgressBar::chunk { background-color: #89b4fa; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Paths Row
    QHBoxLayout* pathsLayout = new QHBoxLayout();
    pathsLayout->addWidget(new QLabel("Left Folder:", this));
    m_leftEdit = new QLineEdit(leftPath, this);
    pathsLayout->addWidget(m_leftEdit, 1);

    pathsLayout->addWidget(new QLabel("Right Folder:", this));
    m_rightEdit = new QLineEdit(rightPath, this);
    pathsLayout->addWidget(m_rightEdit, 1);

    m_btnCompare = new QPushButton("Compare", this);
    m_btnCompare->setStyleSheet("QPushButton { background-color: #fab387; color: #11111b; }");
    connect(m_btnCompare, &QPushButton::clicked, this, &FolderDiffDialog::onCompareClicked);
    pathsLayout->addWidget(m_btnCompare);

    mainLayout->addLayout(pathsLayout);

    // Tree View listing file status
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setColumnCount(4);
    m_treeWidget->setHeaderLabels({"Relative File Path", "Sync Status", "File Size", "Last Modified"});
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_treeWidget);

    // Progress Row
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready. Select source/target folders and click Compare.", this);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);

    // Sync Action Buttons
    QHBoxLayout* syncLayout = new QHBoxLayout();
    m_btnSyncL2R = new QPushButton("◀ Mirror Left to Right", this);
    m_btnSyncL2R->setEnabled(false);
    m_btnSyncL2R->setStyleSheet("QPushButton:enabled { background-color: #89b4fa; color: #11111b; }");
    connect(m_btnSyncL2R, &QPushButton::clicked, this, &FolderDiffDialog::onMirrorLeftToRight);

    m_btnSyncR2L = new QPushButton("Mirror Right to Left ▶", this);
    m_btnSyncR2L->setEnabled(false);
    m_btnSyncR2L->setStyleSheet("QPushButton:enabled { background-color: #89b4fa; color: #11111b; }");
    connect(m_btnSyncR2L, &QPushButton::clicked, this, &FolderDiffDialog::onMirrorRightToLeft);

    m_btnSyncTwoWay = new QPushButton("Two-Way Merge Sync ⇄", this);
    m_btnSyncTwoWay->setEnabled(false);
    m_btnSyncTwoWay->setStyleSheet("QPushButton:enabled { background-color: #a6e3a1; color: #11111b; }");
    connect(m_btnSyncTwoWay, &QPushButton::clicked, this, &FolderDiffDialog::onTwoWaySync);

    m_btnExportReport = new QPushButton("📄 Export Diff Snapshot...", this);
    m_btnExportReport->setEnabled(false);
    m_btnExportReport->setStyleSheet("QPushButton:enabled { background-color: #f9e2af; color: #11111b; }");
    connect(m_btnExportReport, &QPushButton::clicked, this, &FolderDiffDialog::onExportReportClicked);

    syncLayout->addWidget(m_btnSyncL2R);
    syncLayout->addWidget(m_btnSyncR2L);
    syncLayout->addWidget(m_btnSyncTwoWay);
    syncLayout->addWidget(m_btnExportReport);
    
    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    syncLayout->addWidget(btnClose);

    mainLayout->addLayout(syncLayout);
}

void FolderDiffDialog::onCompareClicked() {
    QString left = m_leftEdit->text().trimmed();
    QString right = m_rightEdit->text().trimmed();

    if (!QDir(left).exists() || !QDir(right).exists()) {
        QMessageBox::critical(this, "Compare Folders", "One or both selected directories do not exist.");
        return;
    }

    m_statusLabel->setText("Scanning directories recursively...");
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    QApplication::processEvents();

    scanDirectories(left, right);

    m_treeWidget->clear();
    for (const SyncItem& item : m_scanResults) {
        QTreeWidgetItem* row = new QTreeWidgetItem(m_treeWidget);
        row->setText(0, item.relativePath);
        row->setText(1, item.status);
        
        double kb = item.size / 1024.0;
        double mb = kb / 1024.0;
        QString sizeStr = (mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1);
        row->setText(2, sizeStr);

        QFileInfo info(item.leftFullPath.isEmpty() ? item.rightFullPath : item.leftFullPath);
        row->setText(3, info.lastModified().toString("yyyy-MM-dd hh:mm"));

        // Visual coloring highlights
        if (item.status == "Left Only") {
            row->setBackground(1, QBrush(QColor("#2d3149")));
            row->setForeground(1, QBrush(QColor("#a6e3a1"))); // Green
        } else if (item.status == "Right Only") {
            row->setBackground(1, QBrush(QColor("#2d3149")));
            row->setForeground(1, QBrush(QColor("#f38ba8"))); // Red
        } else if (item.status.contains("Newer")) {
            row->setBackground(1, QBrush(QColor("#2d3149")));
            row->setForeground(1, QBrush(QColor("#f9e2af"))); // Yellow
        }
    }

    m_progressBar->setVisible(false);
    m_btnSyncL2R->setEnabled(true);
    m_btnSyncR2L->setEnabled(true);
    m_btnSyncTwoWay->setEnabled(true);
    m_btnExportReport->setEnabled(true);
    m_statusLabel->setText(QString("Comparison finished. %1 items analyzed.").arg(m_scanResults.size()));
}

void FolderDiffDialog::scanDirectories(const QString& left, const QString& right) {
    m_scanResults.clear();
    QDir leftDir(left);
    QDir rightDir(right);

    QMap<QString, QFileInfo> leftMap;
    QDirIterator itLeft(left, QDir::Files, QDirIterator::Subdirectories);
    while (itLeft.hasNext()) {
        itLeft.next();
        leftMap[leftDir.relativeFilePath(itLeft.filePath())] = itLeft.fileInfo();
    }

    QMap<QString, QFileInfo> rightMap;
    QDirIterator itRight(right, QDir::Files, QDirIterator::Subdirectories);
    while (itRight.hasNext()) {
        itRight.next();
        rightMap[rightDir.relativeFilePath(itRight.filePath())] = itRight.fileInfo();
    }

    // Process Left relative items
    for (auto it = leftMap.begin(); it != leftMap.end(); ++it) {
        QString rel = it.key();
        QFileInfo lInfo = it.value();
        
        SyncItem item;
        item.relativePath = rel;
        item.leftFullPath = lInfo.absoluteFilePath();
        item.size = lInfo.size();

        if (rightMap.contains(rel)) {
            QFileInfo rInfo = rightMap[rel];
            item.rightFullPath = rInfo.absoluteFilePath();
            
            if (lInfo.size() == rInfo.size() && lInfo.lastModified().secsTo(rInfo.lastModified()) == 0) {
                // Completely identical, skip displaying identical items to keep scan neat
                continue;
            } else if (lInfo.lastModified() > rInfo.lastModified()) {
                item.status = "Left Newer";
            } else {
                item.status = "Right Newer";
            }
        } else {
            item.status = "Left Only";
        }
        m_scanResults.append(item);
    }

    // Process Right relative items not in Left
    for (auto it = rightMap.begin(); it != rightMap.end(); ++it) {
        QString rel = it.key();
        if (!leftMap.contains(rel)) {
            SyncItem item;
            item.relativePath = rel;
            item.rightFullPath = it.value().absoluteFilePath();
            item.status = "Right Only";
            item.size = it.value().size();
            m_scanResults.append(item);
        }
    }
}

void FolderDiffDialog::onMirrorLeftToRight() {
    if (QMessageBox::question(this, "Mirror Sync", "Are you sure you want to mirror Left -> Right?\nThis will OVERWRITE and DELETE items in Right to match Left.") != QMessageBox::Yes) {
        return;
    }
    syncFiles(m_scanResults, true, false);
}

void FolderDiffDialog::onMirrorRightToLeft() {
    if (QMessageBox::question(this, "Mirror Sync", "Are you sure you want to mirror Right -> Left?\nThis will OVERWRITE and DELETE items in Left to match Right.") != QMessageBox::Yes) {
        return;
    }
    syncFiles(m_scanResults, false, true);
}

void FolderDiffDialog::onTwoWaySync() {
    if (QMessageBox::question(this, "Two-Way Sync", "Copy newest files to both folders?") != QMessageBox::Yes) {
        return;
    }
    syncFiles(m_scanResults, true, true);
}

void FolderDiffDialog::onExportReportClicked() {
    if (m_scanResults.isEmpty()) {
        QMessageBox::information(this, "Export Report", "No comparison results available to export.");
        return;
    }

    QString defaultName = QDir(m_leftEdit->text()).dirName() + "_vs_" + QDir(m_rightEdit->text()).dirName() + "_Diff_Report.md";
    QString fileName = QFileDialog::getSaveFileName(this, "Export Comparison Report", QDir::homePath() + "/" + defaultName, "Markdown Report (*.md);;HTML Report (*.html)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Report", "Could not open file for writing.");
        return;
    }

    QTextStream out(&file);

    int leftOnly = 0, rightOnly = 0, leftNewer = 0, rightNewer = 0, identical = 0;
    for (const auto& item : m_scanResults) {
        if (item.status == "Left Only") leftOnly++;
        else if (item.status == "Right Only") rightOnly++;
        else if (item.status == "Left Newer") leftNewer++;
        else if (item.status == "Right Newer") rightNewer++;
        else if (item.status == "Identical") identical++;
    }

    if (fileName.endsWith(".html", Qt::CaseInsensitive)) {
        out << "<html><head><title>Directory Comparison Report</title>";
        out << "<style>body{font-family:sans-serif;background:#1e1e2e;color:#cdd6f4;padding:20px;}"
            << "table{border-collapse:collapse;width:100%;margin-top:15px;}"
            << "th,td{border:1px solid #45475a;padding:8px;text-align:left;}"
            << "th{background:#313244;color:#a6e3a1;}</style></head><body>";
        out << "<h1>Directory Comparison Report</h1>";
        out << "<p><b>Left Directory:</b> " << m_leftEdit->text() << "<br>";
        out << "<b>Right Directory:</b> " << m_rightEdit->text() << "<br>";
        out << "<b>Generated:</b> " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "</p>";
        out << "<h3>Summary Stats</h3><ul>"
            << "<li>Total Files Analyzed: " << m_scanResults.size() << "</li>"
            << "<li>Left Only: " << leftOnly << "</li>"
            << "<li>Right Only: " << rightOnly << "</li>"
            << "<li>Left Newer: " << leftNewer << "</li>"
            << "<li>Right Newer: " << rightNewer << "</li>"
            << "<li>Identical: " << identical << "</li></ul>";
        out << "<h3>Detailed File Comparison</h3><table>"
            << "<tr><th>Relative Path</th><th>Status</th><th>Size</th></tr>";
        for (const auto& item : m_scanResults) {
            out << "<tr><td>" << item.relativePath << "</td><td>" << item.status << "</td><td>" << item.size << " B</td></tr>";
        }
        out << "</table></body></html>";
    } else {
        out << "# Directory Comparison Report\n\n";
        out << "- **Left Directory:** `" << m_leftEdit->text() << "`\n";
        out << "- **Right Directory:** `" << m_rightEdit->text() << "`\n";
        out << "- **Generated:** " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n\n";
        out << "## Summary Stats\n\n";
        out << "| Metric | Count |\n";
        out << "| :--- | :--- |\n";
        out << "| Total Files Analyzed | " << m_scanResults.size() << " |\n";
        out << "| Left Only | " << leftOnly << " |\n";
        out << "| Right Only | " << rightOnly << " |\n";
        out << "| Left Newer | " << leftNewer << " |\n";
        out << "| Right Newer | " << rightNewer << " |\n";
        out << "| Identical | " << identical << " |\n\n";
        out << "## Detailed File Comparison\n\n";
        out << "| Relative Path | Sync Status | File Size (Bytes) |\n";
        out << "| :--- | :--- | :--- |\n";
        for (const auto& item : m_scanResults) {
            out << "| `" << item.relativePath << "` | " << item.status << " | " << item.size << " |\n";
        }
    }

    QMessageBox::information(this, "Export Completed", QString("Comparison report exported successfully to:\n%1").arg(fileName));
}

void FolderDiffDialog::syncFiles(const QList<SyncItem>& items, bool leftToRight, bool rightToLeft) {
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, items.size());
    m_progressBar->setValue(0);

    int copied = 0;
    int deleted = 0;

    QString leftRoot = m_leftEdit->text().trimmed();
    QString rightRoot = m_rightEdit->text().trimmed();

    for (int i = 0; i < items.size(); ++i) {
        m_progressBar->setValue(i + 1);
        const SyncItem& item = items[i];

        if (leftToRight) {
            // Left to Right Synchronization
            if (item.status == "Left Only" || item.status == "Left Newer") {
                QString targetPath = rightRoot + "/" + item.relativePath;
                QDir().mkpath(QFileInfo(targetPath).absolutePath());
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (QFile::copy(item.leftFullPath, targetPath)) {
                    copied++;
                }
            } else if (item.status == "Right Only" && !rightToLeft) {
                // Orphan in target: delete it to achieve mirror
                QFile::remove(item.rightFullPath);
                deleted++;
            }
        }

        if (rightToLeft) {
            // Right to Left Synchronization
            if (item.status == "Right Only" || item.status == "Right Newer") {
                QString targetPath = leftRoot + "/" + item.relativePath;
                QDir().mkpath(QFileInfo(targetPath).absolutePath());
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (QFile::copy(item.rightFullPath, targetPath)) {
                    copied++;
                }
            } else if (item.status == "Left Only" && !leftToRight) {
                // Orphan in target: delete it to achieve mirror
                QFile::remove(item.leftFullPath);
                deleted++;
            }
        }

        // Two Way sync newer copies
        if (leftToRight && rightToLeft) {
            if (item.status == "Left Newer") {
                QString targetPath = rightRoot + "/" + item.relativePath;
                if (QFile::exists(targetPath)) QFile::remove(targetPath);
                QFile::copy(item.leftFullPath, targetPath);
                copied++;
            } else if (item.status == "Right Newer") {
                QString targetPath = leftRoot + "/" + item.relativePath;
                if (QFile::exists(targetPath)) QFile::remove(targetPath);
                QFile::copy(item.rightFullPath, targetPath);
                copied++;
            }
        }

        QApplication::processEvents();
    }

    m_progressBar->setVisible(false);
    QMessageBox::information(this, "Sync Completed", QString("Successfully synchronized folders:\n- Copied: %1 files\n- Deleted: %2 orphans").arg(copied).arg(deleted));
    onCompareClicked(); // Refresh diffs list
}
