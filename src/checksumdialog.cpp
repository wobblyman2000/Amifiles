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
    setWindowTitle("File Checksum Checker");
    resize(520, 360);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; font-family: monospace; }"
        "QProgressBar { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // File selection row
    QHBoxLayout* fileLayout = new QHBoxLayout();
    m_lblFile = new QLabel(this);
    m_lblFile->setText(m_filePath.isEmpty() ? "No file selected." : m_filePath);
    m_lblFile->setWordWrap(true);
    m_lblFile->setStyleSheet("font-weight: bold;");
    
    m_btnSelectFile = new QPushButton("Browse...", this);
    m_btnSelectFile->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(m_btnSelectFile, &QPushButton::clicked, this, &ChecksumDialog::onSelectFile);
    
    fileLayout->addWidget(m_lblFile, 1);
    fileLayout->addWidget(m_btnSelectFile);
    mainLayout->addLayout(fileLayout);

    // Progress bar
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    // Hash values rows
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(8);

    grid->addWidget(new QLabel("MD5:", this), 0, 0);
    m_editMd5 = new QLineEdit(this);
    m_editMd5->setReadOnly(true);
    m_btnCopyMd5 = new QPushButton("Copy", this);
    m_btnCopyMd5->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 4px 10px; } QPushButton:hover { background-color: #45475a; }");
    connect(m_btnCopyMd5, &QPushButton::clicked, this, &ChecksumDialog::onCopyMd5);
    grid->addWidget(m_editMd5, 0, 1);
    grid->addWidget(m_btnCopyMd5, 0, 2);

    grid->addWidget(new QLabel("SHA-1:", this), 1, 0);
    m_editSha1 = new QLineEdit(this);
    m_editSha1->setReadOnly(true);
    m_btnCopySha1 = new QPushButton("Copy", this);
    m_btnCopySha1->setStyleSheet(m_btnCopyMd5->styleSheet());
    connect(m_btnCopySha1, &QPushButton::clicked, this, &ChecksumDialog::onCopySha1);
    grid->addWidget(m_editSha1, 1, 1);
    grid->addWidget(m_btnCopySha1, 1, 2);

    grid->addWidget(new QLabel("SHA-256:", this), 2, 0);
    m_editSha256 = new QLineEdit(this);
    m_editSha256->setReadOnly(true);
    m_btnCopySha256 = new QPushButton("Copy", this);
    m_btnCopySha256->setStyleSheet(m_btnCopyMd5->styleSheet());
    connect(m_btnCopySha256, &QPushButton::clicked, this, &ChecksumDialog::onCopySha256);
    grid->addWidget(m_editSha256, 2, 1);
    grid->addWidget(m_btnCopySha256, 2, 2);

    mainLayout->addLayout(grid);

    // Verification row
    mainLayout->addWidget(new QLabel("Paste hash here to verify:", this));
    m_editVerify = new QLineEdit(this);
    m_editVerify->setPlaceholderText("Paste hash to compare...");
    connect(m_editVerify, &QLineEdit::textChanged, this, &ChecksumDialog::onVerifyTextChanged);
    mainLayout->addWidget(m_editVerify);

    QHBoxLayout* bottom = new QHBoxLayout();
    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet(
        "QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background-color: #b4befe; }"
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
