#include "checksumdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QFile>
#include <QStyle>
#include <QMessageBox>
#include <QByteArrayView>
#include <QTabWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QFormLayout>
#include <QDirIterator>
#include <QTextStream>

HashWorker::HashWorker(const QString& filePath, QObject* parent)
    : QThread(parent), m_filePath(filePath) {}

void HashWorker::run() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Could not open file for reading: " + file.errorString());
        return;
    }

    qint64 total = file.size();
    qint64 processed = 0;

    QCryptographicHash hashMd5(QCryptographicHash::Md5);
    QCryptographicHash hashSha1(QCryptographicHash::Sha1);
    QCryptographicHash hashSha256(QCryptographicHash::Sha256);

    char buffer[131072]; // 128KB buffer
    while (!isInterruptionRequested() && !file.atEnd()) {
        qint64 read = file.read(buffer, sizeof(buffer));
        if (read <= 0) break;

        hashMd5.addData(QByteArrayView(buffer, read));
        hashSha1.addData(QByteArrayView(buffer, read));
        hashSha256.addData(QByteArrayView(buffer, read));

        processed += read;
        if (total > 0) {
            emit progress(static_cast<int>((processed * 100) / total));
        }
    }

    if (isInterruptionRequested()) {
        return;
    }

    emit finished(
        hashMd5.result().toHex().toLower(),
        hashSha1.result().toHex().toLower(),
        hashSha256.result().toHex().toLower()
    );
}

ChecksumDialog::ChecksumDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent), m_filePath(filePath) {
    setupUI();
    if (!m_filePath.isEmpty()) {
        startHashing(m_filePath);
    }
}

ChecksumDialog::~ChecksumDialog() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->requestInterruption();
        m_worker->wait();
    }
}

void ChecksumDialog::setupUI() {
    setWindowTitle("File Checksum & Manifest Suite");
    resize(650, 480);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; font-family: monospace; }"
        "QProgressBar { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }"
        "QTabWidget::pane { border: 1px solid #313244; background: #1e1e2e; }"
        "QTabBar::tab { background: #181825; color: #cdd6f4; padding: 8px 16px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #313244; color: #89b4fa; font-weight: bold; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_tabWidget = new QTabWidget(this);

    // --- TAB 1: Single File Checksum ---
    QWidget* tabSingle = new QWidget(m_tabWidget);
    QVBoxLayout* singleLayout = new QVBoxLayout(tabSingle);
    singleLayout->setSpacing(10);

    QHBoxLayout* fileLayout = new QHBoxLayout();
    m_lblFile = new QLabel(tabSingle);
    m_lblFile->setText(m_filePath.isEmpty() ? "No file selected." : m_filePath);
    m_lblFile->setWordWrap(true);
    m_lblFile->setStyleSheet("font-weight: bold;");
    
    m_btnSelectFile = new QPushButton("Browse...", tabSingle);
    m_btnSelectFile->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; } QPushButton:hover { background-color: #45475a; }");
    connect(m_btnSelectFile, &QPushButton::clicked, this, &ChecksumDialog::onSelectFile);
    fileLayout->addWidget(m_lblFile, 1);
    fileLayout->addWidget(m_btnSelectFile);
    singleLayout->addLayout(fileLayout);

    m_progress = new QProgressBar(tabSingle);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    singleLayout->addWidget(m_progress);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(8);
    grid->addWidget(new QLabel("MD5:", tabSingle), 0, 0);
    m_editMd5 = new QLineEdit(tabSingle);
    m_editMd5->setReadOnly(true);
    m_btnCopyMd5 = new QPushButton("Copy", tabSingle);
    m_btnCopyMd5->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 4px 10px; } QPushButton:hover { background-color: #45475a; }");
    connect(m_btnCopyMd5, &QPushButton::clicked, this, &ChecksumDialog::onCopyMd5);
    grid->addWidget(m_editMd5, 0, 1);
    grid->addWidget(m_btnCopyMd5, 0, 2);

    grid->addWidget(new QLabel("SHA-1:", tabSingle), 1, 0);
    m_editSha1 = new QLineEdit(tabSingle);
    m_editSha1->setReadOnly(true);
    m_btnCopySha1 = new QPushButton("Copy", tabSingle);
    m_btnCopySha1->setStyleSheet(m_btnCopyMd5->styleSheet());
    connect(m_btnCopySha1, &QPushButton::clicked, this, &ChecksumDialog::onCopySha1);
    grid->addWidget(m_editSha1, 1, 1);
    grid->addWidget(m_btnCopySha1, 1, 2);

    grid->addWidget(new QLabel("SHA-256:", tabSingle), 2, 0);
    m_editSha256 = new QLineEdit(tabSingle);
    m_editSha256->setReadOnly(true);
    m_btnCopySha256 = new QPushButton("Copy", tabSingle);
    m_btnCopySha256->setStyleSheet(m_btnCopyMd5->styleSheet());
    connect(m_btnCopySha256, &QPushButton::clicked, this, &ChecksumDialog::onCopySha256);
    grid->addWidget(m_editSha256, 2, 1);
    grid->addWidget(m_btnCopySha256, 2, 2);
    singleLayout->addLayout(grid);

    singleLayout->addWidget(new QLabel("Paste hash here to verify:", tabSingle));
    m_editVerify = new QLineEdit(tabSingle);
    m_editVerify->setPlaceholderText("Paste hash to compare...");
    connect(m_editVerify, &QLineEdit::textChanged, this, &ChecksumDialog::onVerifyTextChanged);
    singleLayout->addWidget(m_editVerify);
    m_tabWidget->addTab(tabSingle, "Single File Hash");

    // --- TAB 2: Generate Manifest ---
    QWidget* tabGen = new QWidget(m_tabWidget);
    QFormLayout* formGen = new QFormLayout(tabGen);
    formGen->setSpacing(10);

    QHBoxLayout* genFoldLayout = new QHBoxLayout();
    m_editGenFolder = new QLineEdit(tabGen);
    QPushButton* btnGenFold = new QPushButton("Browse...", tabGen);
    connect(btnGenFold, &QPushButton::clicked, this, &ChecksumDialog::onBrowseGenFolder);
    genFoldLayout->addWidget(m_editGenFolder);
    genFoldLayout->addWidget(btnGenFold);
    formGen->addRow("Source Directory:", genFoldLayout);

    m_comboGenFormat = new QComboBox(tabGen);
    m_comboGenFormat->addItems({"SHA-256 Manifest (.sha256)", "MD5 Manifest (.sfv / .md5)"});
    m_comboGenFormat->setStyleSheet("QComboBox { background-color: #181825; color: #cdd6f4; padding: 4px; }");
    formGen->addRow("Manifest Format:", m_comboGenFormat);

    QHBoxLayout* genOutLayout = new QHBoxLayout();
    m_editGenOutput = new QLineEdit(tabGen);
    QPushButton* btnGenOut = new QPushButton("Save As...", tabGen);
    connect(btnGenOut, &QPushButton::clicked, this, &ChecksumDialog::onBrowseGenOutput);
    genOutLayout->addWidget(m_editGenOutput);
    genOutLayout->addWidget(btnGenOut);
    formGen->addRow("Save Manifest File:", genOutLayout);

    QPushButton* btnGen = new QPushButton("⚡ Generate Manifest File", tabGen);
    btnGen->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 16px; }");
    connect(btnGen, &QPushButton::clicked, this, &ChecksumDialog::onGenerateManifest);
    formGen->addRow("", btnGen);
    m_tabWidget->addTab(tabGen, "Generate Folder Manifest");

    // --- TAB 3: Verify Manifest ---
    QWidget* tabVerify = new QWidget(m_tabWidget);
    QVBoxLayout* verifyLayout = new QVBoxLayout(tabVerify);
    verifyLayout->setSpacing(8);

    QFormLayout* formVerify = new QFormLayout();
    QHBoxLayout* vManLayout = new QHBoxLayout();
    m_editVerifyManifest = new QLineEdit(tabVerify);
    QPushButton* btnVMan = new QPushButton("Browse...", tabVerify);
    connect(btnVMan, &QPushButton::clicked, this, &ChecksumDialog::onBrowseVerifyManifest);
    vManLayout->addWidget(m_editVerifyManifest);
    vManLayout->addWidget(btnVMan);
    formVerify->addRow("Manifest File:", vManLayout);

    QHBoxLayout* vFoldLayout = new QHBoxLayout();
    m_editVerifyFolder = new QLineEdit(tabVerify);
    QPushButton* btnVFold = new QPushButton("Browse...", tabVerify);
    connect(btnVFold, &QPushButton::clicked, this, &ChecksumDialog::onBrowseVerifyFolder);
    vFoldLayout->addWidget(m_editVerifyFolder);
    vFoldLayout->addWidget(btnVFold);
    formVerify->addRow("Target Directory:", vFoldLayout);
    verifyLayout->addLayout(formVerify);

    QPushButton* btnStartVerify = new QPushButton("🔍 Verify Integrity", tabVerify);
    btnStartVerify->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 14px; }");
    connect(btnStartVerify, &QPushButton::clicked, this, &ChecksumDialog::onVerifyManifest);
    verifyLayout->addWidget(btnStartVerify);

    m_treeVerify = new QTreeWidget(tabVerify);
    m_treeVerify->setColumnCount(3);
    m_treeVerify->setHeaderLabels({"Relative Path", "Status", "Calculated Hash"});
    m_treeVerify->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeVerify->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeVerify->setStyleSheet("QTreeWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; }");
    verifyLayout->addWidget(m_treeVerify);

    m_lblVerifyStats = new QLabel("Select a manifest file and click Verify Integrity.", tabVerify);
    m_lblVerifyStats->setStyleSheet("color: #a6adc8; font-style: italic;");
    verifyLayout->addWidget(m_lblVerifyStats);

    m_tabWidget->addTab(tabVerify, "Verify Folder Manifest");

    mainLayout->addWidget(m_tabWidget);

    QHBoxLayout* bottom = new QHBoxLayout();
    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; font-weight: bold; border-radius: 4px; padding: 6px 20px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addStretch();
    bottom->addWidget(btnClose);
    mainLayout->addLayout(bottom);
}

void ChecksumDialog::startHashing(const QString& path) {
    m_filePath = path;
    m_lblFile->setText(path);

    m_editMd5->clear();
    m_editSha1->clear();
    m_editSha256->clear();
    m_editVerify->clear();
    m_editVerify->setStyleSheet("");

    if (m_worker && m_worker->isRunning()) {
        m_worker->requestInterruption();
        m_worker->wait();
        delete m_worker;
    }

    m_progress->setValue(0);
    m_progress->setVisible(true);

    m_worker = new HashWorker(path, this);
    connect(m_worker, &HashWorker::progress, this, &ChecksumDialog::onHashProgress);
    connect(m_worker, &HashWorker::finished, this, &ChecksumDialog::onHashFinished);
    connect(m_worker, &HashWorker::errorOccurred, this, &ChecksumDialog::onHashError);
    m_worker->start();
}

void ChecksumDialog::onSelectFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select File for Checksum", QString(), "All Files (*)");
    if (!path.isEmpty()) {
        startHashing(path);
    }
}

void ChecksumDialog::onHashProgress(int percentage) {
    m_progress->setValue(percentage);
}

void ChecksumDialog::onHashFinished(const QString& md5, const QString& sha1, const QString& sha256) {
    m_progress->setVisible(false);
    m_hashMd5 = md5;
    m_hashSha1 = sha1;
    m_hashSha256 = sha256;

    m_editMd5->setText(md5);
    m_editSha1->setText(sha1);
    m_editSha256->setText(sha256);
}

void ChecksumDialog::onHashError(const QString& errorMsg) {
    m_progress->setVisible(false);
    QMessageBox::critical(this, "Hashing Error", errorMsg);
}

void ChecksumDialog::onVerifyTextChanged(const QString& text) {
    QString cleanText = text.trimmed().toLower();
    if (cleanText.isEmpty()) {
        m_editVerify->setStyleSheet("");
        return;
    }

    if (cleanText == m_hashMd5 || cleanText == m_hashSha1 || cleanText == m_hashSha256) {
        m_editVerify->setStyleSheet("background-color: #a6e3a1; color: #11111b; font-weight: bold;"); // Green highlight
    } else {
        m_editVerify->setStyleSheet("background-color: #f38ba8; color: #11111b; font-weight: bold;"); // Red highlight
    }
}

void ChecksumDialog::onCopyMd5() {
    QGuiApplication::clipboard()->setText(m_hashMd5);
}

void ChecksumDialog::onCopySha1() {
    QGuiApplication::clipboard()->setText(m_hashSha1);
}

void ChecksumDialog::onCopySha256() {
    QGuiApplication::clipboard()->setText(m_hashSha256);
}

void ChecksumDialog::onBrowseGenFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Hash", QDir::homePath());
    if (!dir.isEmpty()) {
        m_editGenFolder->setText(dir);
        if (m_editGenOutput->text().isEmpty()) {
            m_editGenOutput->setText(QDir(dir).filePath("checksum_manifest.sha256"));
        }
    }
}

void ChecksumDialog::onBrowseGenOutput() {
    QString file = QFileDialog::getSaveFileName(this, "Save Checksum Manifest", QDir::homePath() + "/manifest.sha256", "Checksum Manifest (*.sha256 *.sfv *.md5)");
    if (!file.isEmpty()) {
        m_editGenOutput->setText(file);
    }
}

void ChecksumDialog::onGenerateManifest() {
    QString srcDir = m_editGenFolder->text().trimmed();
    QString outFile = m_editGenOutput->text().trimmed();

    if (!QDir(srcDir).exists() || outFile.isEmpty()) {
        QMessageBox::warning(this, "Generate Manifest", "Please select a valid source directory and output file.");
        return;
    }

    QFile file(outFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Generate Manifest", "Could not open destination manifest file for writing.");
        return;
    }

    bool isSha256 = m_comboGenFormat->currentIndex() == 0;
    QTextStream out(&file);

    int count = 0;
    QDirIterator it(srcDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString fullPath = it.next();
        QString relPath = QDir(srcDir).relativeFilePath(fullPath);

        QFile targetFile(fullPath);
        if (targetFile.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(isSha256 ? QCryptographicHash::Sha256 : QCryptographicHash::Md5);
            hash.addData(&targetFile);
            out << hash.result().toHex().toLower() << "  " << relPath << "\n";
            count++;
        }
    }

    QMessageBox::information(this, "Manifest Generated", QString("Successfully generated manifest for %1 files:\n%2").arg(count).arg(outFile));
}

void ChecksumDialog::onBrowseVerifyManifest() {
    QString file = QFileDialog::getOpenFileName(this, "Select Checksum Manifest", QDir::homePath(), "Manifest Files (*.sha256 *.sfv *.md5);;All Files (*)");
    if (!file.isEmpty()) {
        m_editVerifyManifest->setText(file);
        if (m_editVerifyFolder->text().isEmpty()) {
            m_editVerifyFolder->setText(QFileInfo(file).absolutePath());
        }
    }
}

void ChecksumDialog::onBrowseVerifyFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Verify", QDir::homePath());
    if (!dir.isEmpty()) {
        m_editVerifyFolder->setText(dir);
    }
}

void ChecksumDialog::onVerifyManifest() {
    QString manFile = m_editVerifyManifest->text().trimmed();
    QString targetDir = m_editVerifyFolder->text().trimmed();

    if (!QFile::exists(manFile) || !QDir(targetDir).exists()) {
        QMessageBox::warning(this, "Verify Manifest", "Please select a valid manifest file and target directory.");
        return;
    }

    QFile file(manFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Verify Manifest", "Could not open manifest file.");
        return;
    }

    m_treeVerify->clear();
    int passed = 0, failed = 0, missing = 0;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) continue;

        QString expectedHash;
        QString relPath;

        if (line.contains("  ")) {
            QStringList parts = line.split("  ");
            expectedHash = parts[0].trimmed().toLower();
            relPath = parts.mid(1).join("  ").trimmed();
        } else if (line.contains(" ")) {
            int firstSpace = line.indexOf(' ');
            expectedHash = line.left(firstSpace).trimmed().toLower();
            relPath = line.mid(firstSpace + 1).trimmed();
        }

        if (relPath.isEmpty()) continue;

        QString fullPath = QDir(targetDir).filePath(relPath);
        QTreeWidgetItem* item = new QTreeWidgetItem(m_treeVerify);
        item->setText(0, relPath);

        if (!QFile::exists(fullPath)) {
            item->setText(1, "❌ MISSING");
            item->setForeground(1, QBrush(QColor("#f38ba8")));
            item->setText(2, "N/A");
            missing++;
        } else {
            QFile target(fullPath);
            if (target.open(QIODevice::ReadOnly)) {
                QCryptographicHash::Algorithm alg = (expectedHash.length() == 64) ? QCryptographicHash::Sha256 : QCryptographicHash::Md5;
                QCryptographicHash hash(alg);
                hash.addData(&target);
                QString calcHash = hash.result().toHex().toLower();
                item->setText(2, calcHash);

                if (calcHash == expectedHash) {
                    item->setText(1, "✓ PASSED");
                    item->setForeground(1, QBrush(QColor("#a6e3a1")));
                    passed++;
                } else {
                    item->setText(1, "❌ CORRUPTED");
                    item->setForeground(1, QBrush(QColor("#f38ba8")));
                    failed++;
                }
            } else {
                item->setText(1, "❌ UNREADABLE");
                item->setForeground(1, QBrush(QColor("#f38ba8")));
                failed++;
            }
        }
    }

    m_lblVerifyStats->setText(QString("Verification Result: %1 Passed, %2 Corrupted, %3 Missing.")
                               .arg(passed).arg(failed).arg(missing));
}
