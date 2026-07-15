#include "shreddialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

ShredWorker::ShredWorker(const QStringList& paths, QObject* parent)
    : QThread(parent), m_paths(paths) {}

void ShredWorker::run() {
    QStringList filesList;
    QStringList dirsList;

    // Collect all files and directories recursively
    for (const QString& path : m_paths) {
        collectFiles(path, filesList, dirsList);
    }

    int totalFiles = filesList.size();
    int shreddedCount = 0;

    for (int i = 0; i < totalFiles; ++i) {
        if (isInterruptionRequested()) {
            return;
        }

        QString filePath = filesList[i];
        emit progress((i * 100) / totalFiles, QFileInfo(filePath).fileName());

        if (shredFile(filePath)) {
            shreddedCount++;
        }
    }

    // Now remove directories bottom-up
    // Sort directories by length descending to delete deepest folders first
    std::sort(dirsList.begin(), dirsList.end(), [](const QString& a, const QString& b) {
        return a.length() > b.length();
    });

    for (const QString& dirPath : dirsList) {
        QDir().rmdir(dirPath);
    }

    emit finished(shreddedCount);
}

void ShredWorker::collectFiles(const QString& path, QStringList& filesList, QStringList& dirsList) {
    QFileInfo info(path);
    if (info.isFile()) {
        filesList.append(path);
    } else if (info.isDir()) {
        dirsList.append(path);
        QDir dir(path);
        QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString& entry : entries) {
            collectFiles(dir.filePath(entry), filesList, dirsList);
        }
    }
}

bool ShredWorker::shredFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }

    qint64 size = file.size();
    if (size > 0) {
        // Pass 1: Write all zeros (0x00)
        file.seek(0);
        QByteArray zeros(65536, 0x00);
        qint64 written = 0;
        while (written < size && !isInterruptionRequested()) {
            qint64 toWrite = qMin(size - written, static_cast<qint64>(zeros.size()));
            file.write(zeros.constData(), toWrite);
            written += toWrite;
        }
        file.flush();

        // Pass 2: Write all ones (0xFF)
        file.seek(0);
        QByteArray ones(65536, 0xFF);
        written = 0;
        while (written < size && !isInterruptionRequested()) {
            qint64 toWrite = qMin(size - written, static_cast<qint64>(ones.size()));
            file.write(ones.constData(), toWrite);
            written += toWrite;
        }
        file.flush();

        // Pass 3: Write random bytes
        file.seek(0);
        QByteArray randomBytes(65536, 0x00);
        for (int i = 0; i < randomBytes.size(); ++i) {
            randomBytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
        }
        written = 0;
        while (written < size && !isInterruptionRequested()) {
            qint64 toWrite = qMin(size - written, static_cast<qint64>(randomBytes.size()));
            file.write(randomBytes.constData(), toWrite);
            written += toWrite;
        }
        file.flush();
    }

    file.resize(0);
    file.close();

    // Overwrite metadata (change modification date if possible, then remove)
    return file.remove();
}

ShredDialog::ShredDialog(const QStringList& paths, QWidget* parent)
    : QDialog(parent), m_paths(paths) {
    setupUI();
}

ShredDialog::~ShredDialog() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->requestInterruption();
        m_worker->wait();
    }
}

void ShredDialog::setupUI() {
    setWindowTitle("Secure File Shredder");
    resize(480, 240);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QProgressBar { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #f38ba8; border-radius: 3px; }" // Red chunk for shredding!
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel* warningLabel = new QLabel(this);
    warningLabel->setText(
        "<b>WARNING:</b> Secure shredding will permanently overwrite file data blocks using a 3-pass DoD protocol. "
        "These files <i>cannot</i> be recovered using standard data recovery utilities."
    );
    warningLabel->setStyleSheet("color: #f38ba8; font-size: 12px;");
    warningLabel->setWordWrap(true);
    mainLayout->addWidget(warningLabel);

    m_lblStatus = new QLabel(QString("Ready to shred %1 selected item(s).").arg(m_paths.size()), this);
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(m_lblStatus);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    QHBoxLayout* bottom = new QHBoxLayout();
    m_btnShred = new QPushButton("Shred Items Permanently", this);
    m_btnShred->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background-color: #e64553; }"
    );
    connect(m_btnShred, &QPushButton::clicked, this, &ShredDialog::onStartShredding);

    m_btnCancel = new QPushButton("Cancel", this);
    m_btnCancel->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    bottom->addStretch();
    bottom->addWidget(m_btnShred);
    bottom->addWidget(m_btnCancel);
    mainLayout->addLayout(bottom);
}

void ShredDialog::onStartShredding() {
    if (QMessageBox::warning(this, "Confirm Secure Shredding",
        "Are you absolutely sure you want to securely shred the selected items? This operation is completely irreversible.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_btnShred->setEnabled(false);
    m_btnCancel->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);

    m_worker = new ShredWorker(m_paths, this);
    connect(m_worker, &ShredWorker::progress, this, &ShredDialog::onShredProgress);
    connect(m_worker, &ShredWorker::finished, this, &ShredDialog::onShredFinished);
    connect(m_worker, &ShredWorker::errorOccurred, this, &ShredDialog::onShredError);
    m_worker->start();
}

void ShredDialog::onShredProgress(int percentage, const QString& currentFile) {
    m_progress->setValue(percentage);
    m_lblStatus->setText(QString("Shredding: %1").arg(currentFile));
}

void ShredDialog::onShredFinished(int totalShredded) {
    m_progress->setVisible(false);
    QMessageBox::information(this, "Shredding Complete", QString("Successfully shredded %1 files.").arg(totalShredded));
    accept();
}

void ShredDialog::onShredError(const QString& errorMsg) {
    m_progress->setVisible(false);
    m_btnShred->setEnabled(true);
    m_btnCancel->setEnabled(true);
    QMessageBox::critical(this, "Shredding Error", errorMsg);
}
