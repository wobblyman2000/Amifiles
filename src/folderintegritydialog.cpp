#include "folderintegritydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QTextStream>

FolderIntegrityWorker::FolderIntegrityWorker(bool isAudit, const QString& dirPath, QObject* parent)
    : QThread(parent), m_isAudit(isAudit), m_dirPath(dirPath) {}

static QString computeFileHash(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return QString();
    return hash.result().toHex();
}

void FolderIntegrityWorker::run() {
    QList<IntegrityItem> results;
    QString manifestPath = QDir(m_dirPath).filePath(".amifiles_integrity.json");

    if (!m_isAudit) {
        // Generate Snapshot Mode
        QDirIterator it(m_dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        QStringList fileList;
        while (it.hasNext()) {
            it.next();
            if (it.fileName() == ".amifiles_integrity.json") continue;
            fileList.append(it.filePath());
        }

        QJsonObject rootObj;
        rootObj["version"] = "1.0";
        rootObj["generated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        QJsonArray filesArray;

        int total = fileList.size();
        for (int i = 0; i < total; ++i) {
            if (isInterruptionRequested()) return;
            QString fPath = fileList[i];
            QString relPath = QDir(m_dirPath).relativeFilePath(fPath);
            emit progress(i + 1, total, relPath);

            QFileInfo fi(fPath);
            QString hash = computeFileHash(fPath);

            QJsonObject fileObj;
            fileObj["path"] = relPath;
            fileObj["size"] = fi.size();
            fileObj["sha256"] = hash;
            fileObj["mtime"] = fi.lastModified().toString(Qt::ISODate);
            filesArray.append(fileObj);

            IntegrityItem item;
            item.relativePath = relPath;
            item.absolutePath = fPath;
            item.expectedHash = hash;
            item.actualHash = hash;
            item.size = fi.size();
            item.status = IntegrityItem::Intact;
            results.append(item);
        }

        rootObj["files"] = filesArray;
        QJsonDocument doc(rootObj);
        QFile outFile(manifestPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            emit finished(false, results, "Failed to write integrity manifest file.");
            return;
        }
        outFile.write(doc.toJson(QJsonDocument::Indented));
        outFile.close();

        emit finished(true, results, QString("Integrity snapshot generated successfully for %1 files.").arg(results.size()));
    } else {
        // Audit Snapshot Mode
        if (!QFile::exists(manifestPath)) {
            emit finished(false, results, "No integrity manifest (.amifiles_integrity.json) found in directory. Generate a snapshot first!");
            return;
        }

        QFile inFile(manifestPath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            emit finished(false, results, "Could not open integrity manifest file.");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(inFile.readAll());
        inFile.close();
        if (!doc.isObject()) {
            emit finished(false, results, "Invalid integrity manifest format.");
            return;
        }

        QJsonObject rootObj = doc.object();
        QJsonArray filesArray = rootObj["files"].toArray();

        QMap<QString, QJsonObject> manifestMap;
        for (const QJsonValue& val : filesArray) {
            QJsonObject obj = val.toObject();
            manifestMap[obj["path"].toString()] = obj;
        }

        // Scan current files on disk
        QDirIterator it(m_dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        QSet<QString> diskRelPaths;

        while (it.hasNext()) {
            it.next();
            if (it.fileName() == ".amifiles_integrity.json") continue;
            diskRelPaths.insert(QDir(m_dirPath).relativeFilePath(it.filePath()));
        }

        int total = manifestMap.size() + diskRelPaths.size();
        int step = 0;

        // Verify items in manifest
        for (auto mapIt = manifestMap.begin(); mapIt != manifestMap.end(); ++mapIt) {
            if (isInterruptionRequested()) return;
            step++;
            QString relPath = mapIt.key();
            QJsonObject obj = mapIt.value();
            emit progress(step, total, relPath);

            QString absPath = QDir(m_dirPath).filePath(relPath);
            IntegrityItem item;
            item.relativePath = relPath;
            item.absolutePath = absPath;
            item.expectedHash = obj["sha256"].toString();
            item.size = obj["size"].toVariant().toLongLong();

            if (!QFile::exists(absPath)) {
                item.status = IntegrityItem::Missing;
                item.actualHash = "MISSING";
            } else {
                QString actualHash = computeFileHash(absPath);
                item.actualHash = actualHash;
                if (actualHash.compare(item.expectedHash, Qt::CaseInsensitive) == 0) {
                    item.status = IntegrityItem::Intact;
                } else {
                    item.status = IntegrityItem::Corrupted; // Bit-rot detected!
                }
            }
            results.append(item);
        }

        // Check for new files not in manifest
        for (const QString& diskRel : diskRelPaths) {
            if (!manifestMap.contains(diskRel)) {
                IntegrityItem item;
                item.relativePath = diskRel;
                item.absolutePath = QDir(m_dirPath).filePath(diskRel);
                item.expectedHash = "NONE";
                item.actualHash = computeFileHash(item.absolutePath);
                item.size = QFileInfo(item.absolutePath).size();
                item.status = IntegrityItem::NewFile;
                results.append(item);
            }
        }

        emit finished(true, results, QString("Integrity audit finished. Scanned %1 files.").arg(results.size()));
    }
}

FolderIntegrityDialog::FolderIntegrityDialog(const QString& dirPath, bool autoAudit, QWidget* parent)
    : QDialog(parent), m_dirPath(dirPath) {
    setWindowTitle("🛡️ Directory Bit-Rot & Integrity Auditor");
    resize(850, 550);
    setupUI();

    QString manifestPath = QDir(m_dirPath).filePath(".amifiles_integrity.json");
    if (autoAudit && QFile::exists(manifestPath)) {
        onAuditIntegrity();
    }
}

void FolderIntegrityDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QTreeWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 6px; padding: 4px; }"
        "QTreeWidget::item { padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QProgressBar { background-color: #181825; border: 1px solid #313244; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }"
        "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px 8px; }"
    );

    QLabel* lblHeader = new QLabel(QString("<b>Target Directory:</b> <code>%1</code>").arg(m_dirPath), this);
    mainLayout->addWidget(lblHeader);

    QHBoxLayout* topBtns = new QHBoxLayout();
    m_btnGenerate = new QPushButton("📸 Generate Integrity Snapshot", this);
    m_btnGenerate->setToolTip("Creates or overwrites .amifiles_integrity.json hash database in target directory");
    connect(m_btnGenerate, &QPushButton::clicked, this, &FolderIntegrityDialog::onGenerateSnapshot);
    topBtns->addWidget(m_btnGenerate);

    m_btnAudit = new QPushButton("🔍 Audit Directory Integrity", this);
    m_btnAudit->setToolTip("Verifies all current files on disk against .amifiles_integrity.json to detect bit-rot");
    connect(m_btnAudit, &QPushButton::clicked, this, &FolderIntegrityDialog::onAuditIntegrity);
    topBtns->addWidget(m_btnAudit);

    topBtns->addStretch(1);

    m_comboFilter = new QComboBox(this);
    m_comboFilter->addItems({"Filter: Show All Files", "Filter: Corrupted / Bit-Rot Only", "Filter: Missing Only", "Filter: New Files Only"});
    connect(m_comboFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FolderIntegrityDialog::onFilterChanged);
    topBtns->addWidget(m_comboFilter);

    mainLayout->addLayout(topBtns);

    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->hide();
    mainLayout->addWidget(m_progress);

    m_lblStatus = new QLabel("Click 'Audit Directory Integrity' or 'Generate Snapshot' to begin.", this);
    m_lblStatus->setStyleSheet("color: #a6adc8; font-style: italic;");
    mainLayout->addWidget(m_lblStatus);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"Status", "File Path", "Size", "Expected SHA-256", "Actual SHA-256"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Interactive);
    mainLayout->addWidget(m_tree, 1);

    QHBoxLayout* bottomRow = new QHBoxLayout();
    m_lblSummary = new QLabel("Summary: Ready.", this);
    m_lblSummary->setStyleSheet("font-weight: bold; color: #89b4fa;");
    bottomRow->addWidget(m_lblSummary, 1);

    m_btnExport = new QPushButton("📄 Export Audit Report...", this);
    connect(m_btnExport, &QPushButton::clicked, this, &FolderIntegrityDialog::onExportReport);
    bottomRow->addWidget(m_btnExport);

    mainLayout->addLayout(bottomRow);
}

void FolderIntegrityDialog::onGenerateSnapshot() {
    if (m_worker && m_worker->isRunning()) return;

    m_btnGenerate->setEnabled(false);
    m_btnAudit->setEnabled(false);
    m_progress->setValue(0);
    m_progress->show();
    m_lblStatus->setText("Generating integrity snapshot hashes...");

    m_worker = new FolderIntegrityWorker(false, m_dirPath, this);
    connect(m_worker, &FolderIntegrityWorker::progress, this, &FolderIntegrityDialog::onWorkerProgress);
    connect(m_worker, &FolderIntegrityWorker::finished, this, &FolderIntegrityDialog::onWorkerFinished);
    m_worker->start();
}

void FolderIntegrityDialog::onAuditIntegrity() {
    if (m_worker && m_worker->isRunning()) return;

    m_btnGenerate->setEnabled(false);
    m_btnAudit->setEnabled(false);
    m_progress->setValue(0);
    m_progress->show();
    m_lblStatus->setText("Auditing directory integrity against saved hashes...");

    m_worker = new FolderIntegrityWorker(true, m_dirPath, this);
    connect(m_worker, &FolderIntegrityWorker::progress, this, &FolderIntegrityDialog::onWorkerProgress);
    connect(m_worker, &FolderIntegrityWorker::finished, this, &FolderIntegrityDialog::onWorkerFinished);
    m_worker->start();
}

void FolderIntegrityDialog::onWorkerProgress(int current, int total, const QString& currentFile) {
    m_progress->setMaximum(total);
    m_progress->setValue(current);
    m_lblStatus->setText(QString("Processing (%1/%2): %3").arg(current).arg(total).arg(currentFile));
}

void FolderIntegrityDialog::onWorkerFinished(bool success, const QList<IntegrityItem>& results, const QString& message) {
    m_btnGenerate->setEnabled(true);
    m_btnAudit->setEnabled(true);
    m_progress->hide();
    m_lblStatus->setText(message);

    if (!success) {
        QMessageBox::warning(this, "Integrity Tool", message);
        return;
    }

    m_currentResults = results;
    onFilterChanged(m_comboFilter->currentIndex());
    updateSummaryLabels();
}

void FolderIntegrityDialog::onFilterChanged(int index) {
    m_tree->clear();

    for (const IntegrityItem& item : m_currentResults) {
        if (index == 1 && item.status != IntegrityItem::Corrupted) continue;
        if (index == 2 && item.status != IntegrityItem::Missing) continue;
        if (index == 3 && item.status != IntegrityItem::NewFile) continue;

        QTreeWidgetItem* row = new QTreeWidgetItem(m_tree);
        row->setText(1, item.relativePath);

        double szKb = item.size / 1024.0;
        row->setText(2, QString("%1 KB").arg(szKb, 0, 'f', 1));
        row->setText(3, item.expectedHash.left(12) + "...");
        row->setText(4, item.actualHash.left(12) + "...");

        switch (item.status) {
            case IntegrityItem::Intact:
                row->setText(0, "🟩 Intact");
                row->setForeground(0, QBrush(QColor("#a6e3a1")));
                break;
            case IntegrityItem::Corrupted:
                row->setText(0, "🟥 CORRUPTED (BIT-ROT)");
                row->setForeground(0, QBrush(QColor("#f38ba8")));
                row->setBackground(0, QBrush(QColor("#451a24")));
                break;
            case IntegrityItem::Missing:
                row->setText(0, "⚠️ Missing");
                row->setForeground(0, QBrush(QColor("#fab387")));
                break;
            case IntegrityItem::NewFile:
                row->setText(0, "🆕 New File");
                row->setForeground(0, QBrush(QColor("#89b4fa")));
                break;
        }
    }
}

void FolderIntegrityDialog::updateSummaryLabels() {
    int intact = 0, corrupted = 0, missing = 0, newFiles = 0;
    for (const IntegrityItem& item : m_currentResults) {
        if (item.status == IntegrityItem::Intact) intact++;
        else if (item.status == IntegrityItem::Corrupted) corrupted++;
        else if (item.status == IntegrityItem::Missing) missing++;
        else if (item.status == IntegrityItem::NewFile) newFiles++;
    }

    QString text = QString("Total: %1 files | Intact: %2 | Corrupted (Bit-Rot): %3 | Missing: %4 | New: %5")
                       .arg(m_currentResults.size()).arg(intact).arg(corrupted).arg(missing).arg(newFiles);

    if (corrupted > 0) {
        m_lblSummary->setStyleSheet("font-weight: bold; color: #f38ba8; font-size: 13px;");
    } else {
        m_lblSummary->setStyleSheet("font-weight: bold; color: #a6e3a1; font-size: 13px;");
    }
    m_lblSummary->setText(text);
}

void FolderIntegrityDialog::onExportReport() {
    if (m_currentResults.isEmpty()) {
        QMessageBox::information(this, "Export Report", "No audit results to export.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Export Audit Report", QDir(m_dirPath).filePath("integrity_report.txt"), "Text Files (*.txt);;All Files (*)");
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Report", "Failed to open output file for writing.");
        return;
    }

    QTextStream out(&file);
    out << "Directory Bit-Rot & Integrity Audit Report\n";
    out << "Directory: " << m_dirPath << "\n";
    out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "======================================================================\n\n";

    for (const IntegrityItem& item : m_currentResults) {
        QString statusStr = (item.status == IntegrityItem::Intact ? "INTACT" :
                            item.status == IntegrityItem::Corrupted ? "CORRUPTED" :
                            item.status == IntegrityItem::Missing ? "MISSING" : "NEW");
        out << QString("[%1] %2\n").arg(statusStr, -10).arg(item.relativePath);
        out << "  Expected: " << item.expectedHash << "\n";
        out << "  Actual:   " << item.actualHash << "\n\n";
    }

    file.close();
    QMessageBox::information(this, "Export Report", "Report exported successfully to: " + savePath);
}
