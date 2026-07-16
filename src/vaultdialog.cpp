#include "vaultdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QRandomGenerator>

VaultWorker::VaultWorker(bool encrypt, const QString& sourcePath, const QString& password, QObject* parent)
    : QThread(parent), m_encrypt(encrypt), m_sourcePath(sourcePath), m_password(password) {}

void VaultWorker::run() {
    QFileInfo info(m_sourcePath);
    QString parentDir = info.absolutePath();
    QString baseName = info.fileName();
    
    // Create random filename for temporary tarball to prevent collisions
    QString tempTar = parentDir + QString("/.vault_temp_%1.tar").arg(QRandomGenerator::global()->generate());

    if (m_encrypt) {
        emit progress(10, "Creating temporary archive...");
        
        // tar -cf tempTar -C parentDir baseName
        QStringList tarArgs = {"-cf", tempTar, "-C", parentDir, baseName};
        QByteArray err;
        if (!runProcess("tar", tarArgs, &err)) {
            emit finished(false, "Failed to create archive: " + QString::fromUtf8(err));
            return;
        }

        emit progress(50, "Encrypting archive (AES-256)...");
        
        // openssl enc -aes-256-cbc -salt -pbkdf2 -in tempTar -out destVault -k password
        QString destVault = parentDir + "/" + info.baseName() + ".vault";
        QStringList encArgs = {"enc", "-aes-256-cbc", "-salt", "-pbkdf2", "-in", tempTar, "-out", destVault, "-k", m_password};
        if (!runProcess("openssl", encArgs, &err)) {
            shredFile(tempTar);
            emit finished(false, "Failed to encrypt: " + QString::fromUtf8(err));
            return;
        }

        emit progress(90, "Cleaning up temporary files...");
        shredFile(tempTar);

        emit finished(true, "Folder/file successfully encrypted into: " + QFileInfo(destVault).fileName());
    } else {
        emit progress(15, "Decrypting vault (AES-256)...");
        
        // openssl enc -d -aes-256-cbc -salt -pbkdf2 -in sourcePath -out tempTar -k password
        QStringList decArgs = {"enc", "-d", "-aes-256-cbc", "-salt", "-pbkdf2", "-in", m_sourcePath, "-out", tempTar, "-k", m_password};
        QByteArray err;
        if (!runProcess("openssl", decArgs, &err)) {
            shredFile(tempTar);
            emit finished(false, "Decryption failed. Please verify password.");
            return;
        }

        emit progress(60, "Extracting decrypted files...");
        
        // tar -xf tempTar -C parentDir
        QStringList tarArgs = {"-xf", tempTar, "-C", parentDir};
        if (!runProcess("tar", tarArgs, &err)) {
            shredFile(tempTar);
            emit finished(false, "Failed to extract decrypted files: " + QString::fromUtf8(err));
            return;
        }

        emit progress(90, "Cleaning up temporary files...");
        shredFile(tempTar);

        emit finished(true, "Vault successfully decrypted to: " + parentDir);
    }
}

bool VaultWorker::runProcess(const QString& cmd, const QStringList& args, QByteArray* errOutput) {
    QProcess proc;
    proc.start(cmd, args);
    if (!proc.waitForFinished(30000)) {
        if (errOutput) *errOutput = "Operation timed out.";
        proc.kill();
        return false;
    }
    if (proc.exitCode() != 0) {
        if (errOutput) *errOutput = proc.readAllStandardError();
        return false;
    }
    return true;
}

bool VaultWorker::shredFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        return false;
    }

    qint64 size = file.size();
    if (size > 0) {
        // Simple 1-pass shred for temp tarball cleanup
        file.seek(0);
        QByteArray zeros(65536, 0x00);
        qint64 written = 0;
        while (written < size && !isInterruptionRequested()) {
            qint64 toWrite = qMin(size - written, static_cast<qint64>(zeros.size()));
            file.write(zeros.constData(), toWrite);
            written += toWrite;
        }
        file.flush();
    }
    file.resize(0);
    file.close();
    return file.remove();
}

VaultDialog::VaultDialog(bool encrypt, const QString& sourcePath, QWidget* parent)
    : QDialog(parent), m_encrypt(encrypt), m_sourcePath(sourcePath) {
    setupUI();
}

VaultDialog::~VaultDialog() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->requestInterruption();
        m_worker->wait();
    }
}

void VaultDialog::setupUI() {
    setWindowTitle(m_encrypt ? "Encrypt into Secure Vault" : "Decrypt Secure Vault");
    resize(420, 240);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QProgressBar { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #cba6f7; border-radius: 3px; }" // Mauve chunk for vault!
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Target Selection Row
    QHBoxLayout* targetRow = new QHBoxLayout();
    m_lblFile = new QLabel(m_sourcePath.isEmpty() ? "No path selected." : m_sourcePath, this);
    m_lblFile->setWordWrap(true);
    m_lblFile->setStyleSheet("font-weight: bold;");
    targetRow->addWidget(m_lblFile, 1);

    m_btnBrowse = new QPushButton("Browse...", this);
    m_btnBrowse->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; } QPushButton:hover { background-color: #45475a; }");
    connect(m_btnBrowse, &QPushButton::clicked, this, &VaultDialog::onBrowse);
    targetRow->addWidget(m_btnBrowse);
    mainLayout->addLayout(targetRow);

    // Password fields
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(8);

    grid->addWidget(new QLabel("Password:", this), 0, 0);
    m_editPassword = new QLineEdit(this);
    m_editPassword->setEchoMode(QLineEdit::Password);
    m_editPassword->setPlaceholderText("Enter vault password...");
    grid->addWidget(m_editPassword, 0, 1);

    if (m_encrypt) {
        grid->addWidget(new QLabel("Confirm Password:", this), 1, 0);
        m_editConfirmPassword = new QLineEdit(this);
        m_editConfirmPassword->setEchoMode(QLineEdit::Password);
        m_editConfirmPassword->setPlaceholderText("Confirm password...");
        grid->addWidget(m_editConfirmPassword, 1, 1);
    }

    mainLayout->addLayout(grid);

    // Progress
    m_lblStatus = new QLabel("Ready.", this);
    m_lblStatus->setStyleSheet("color: #a6adc8; font-size: 11px;");
    mainLayout->addWidget(m_lblStatus);

    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    // Action buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnAction = new QPushButton(m_encrypt ? "Lock (Encrypt)" : "Unlock (Decrypt)", this);
    m_btnAction->setStyleSheet(
        "QPushButton { background-color: #cba6f7; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(m_btnAction, &QPushButton::clicked, this, &VaultDialog::onAction);

    m_btnCancel = new QPushButton("Cancel", this);
    m_btnCancel->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch();
    btnLayout->addWidget(m_btnAction);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);
}

void VaultDialog::onBrowse() {
    QString path;
    if (m_encrypt) {
        path = QFileDialog::getExistingDirectory(this, "Select Directory to Encrypt");
        if (path.isEmpty()) {
            path = QFileDialog::getOpenFileName(this, "Select File to Encrypt", QString(), "All Files (*)");
        }
    } else {
        path = QFileDialog::getOpenFileName(this, "Select Secure Vault to Decrypt", QString(), "Secure Vaults (*.vault)");
    }

    if (!path.isEmpty()) {
        m_sourcePath = path;
        m_lblFile->setText(path);
    }
}

void VaultDialog::onAction() {
    if (m_sourcePath.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please select a file or folder first.");
        return;
    }

    QString pass = m_editPassword->text();
    if (pass.isEmpty()) {
        QMessageBox::warning(this, "Password Required", "Please enter a vault password.");
        return;
    }

    if (m_encrypt) {
        if (pass != m_editConfirmPassword->text()) {
            QMessageBox::warning(this, "Passwords Match", "Passwords do not match. Please verify.");
            return;
        }
    }

    m_btnAction->setEnabled(false);
    m_btnBrowse->setEnabled(false);
    m_btnCancel->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);

    m_worker = new VaultWorker(m_encrypt, m_sourcePath, pass, this);
    connect(m_worker, &VaultWorker::progress, this, &VaultDialog::onWorkerProgress);
    connect(m_worker, &VaultWorker::finished, this, &VaultDialog::onWorkerFinished);
    m_worker->start();
}

void VaultDialog::onWorkerProgress(int percentage, const QString& statusText) {
    m_progress->setValue(percentage);
    m_lblStatus->setText(statusText);
}

void VaultDialog::onWorkerFinished(bool success, const QString& resultMsg) {
    m_progress->setVisible(false);
    
    if (success) {
        QMessageBox::information(this, "Vault Complete", resultMsg);
        accept();
    } else {
        m_btnAction->setEnabled(true);
        m_btnBrowse->setEnabled(true);
        m_btnCancel->setEnabled(true);
        QMessageBox::critical(this, "Vault Error", resultMsg);
    }
}
