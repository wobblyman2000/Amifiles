#include "archivedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QProcess>

ArchiveDialog::ArchiveDialog(Mode mode, const QStringList& sourcePaths, const QString& currentDir, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_sourcePaths(sourcePaths), m_currentDir(currentDir) {
    setWindowTitle("Create Archive");
    resize(500, 250);
    setupUI();
}

ArchiveDialog::ArchiveDialog(Mode mode, const QString& archivePath, const QString& currentDir, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_archivePath(archivePath), m_currentDir(currentDir) {
    setWindowTitle("Extract Archive");
    resize(500, 200);
    setupUI();
}

ArchiveDialog::~ArchiveDialog() {
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished();
    }
}

void ArchiveDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Apply Catppuccin styling
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QLineEdit { border: 1px solid #313244; background-color: #11111b; color: #cdd6f4; padding: 6px; border-radius: 4px; }"
        "QComboBox { border: 1px solid #313244; background-color: #11111b; color: #cdd6f4; padding: 6px; border-radius: 4px; min-width: 120px; }"
        "QProgressBar { border: 1px solid #313244; background-color: #11111b; text-align: center; border-radius: 4px; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }"
        "QPushButton { border: none; background-color: #313244; color: #cdd6f4; padding: 8px 16px; border-radius: 4px; font-weight: bold; min-width: 80px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );

    m_lblTitle = new QLabel(this);
    m_lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    mainLayout->addWidget(m_lblTitle);

    if (m_mode == ModeCreate) {
        m_lblTitle->setText("Create Compressed Archive");

        QFormLayout* form = new QFormLayout();
        form->setSpacing(10);

        m_txtTargetName = new QLineEdit(this);
        QString defaultName = "archive";
        if (!m_sourcePaths.isEmpty()) {
            QFileInfo firstInfo(m_sourcePaths.first());
            defaultName = firstInfo.completeBaseName();
        }
        m_txtTargetName->setText(defaultName);
        form->addRow("Archive Name:", m_txtTargetName);

        m_comboFormat = new QComboBox(this);
        m_comboFormat->addItems({".zip", ".7z", ".tar.gz", ".tar.xz", ".tar"});
        form->addRow("Format:", m_comboFormat);

        m_chkPassword = new QCheckBox("Password Protection", this);
        m_chkPassword->setStyleSheet("QCheckBox { color: #cdd6f4; }");
        form->addRow("", m_chkPassword);

        m_txtPassword = new QLineEdit(this);
        m_txtPassword->setEchoMode(QLineEdit::Password);
        m_txtPassword->setEnabled(false);
        form->addRow("Password:", m_txtPassword);

        auto updatePasswordUI = [this]() {
            QString fmt = m_comboFormat->currentText();
            bool encryptable = (fmt == ".zip" || fmt == ".7z");
            m_chkPassword->setEnabled(encryptable);
            m_txtPassword->setEnabled(encryptable && m_chkPassword->isChecked());
            if (!encryptable) {
                m_chkPassword->setChecked(false);
            }
        };

        connect(m_comboFormat, &QComboBox::currentTextChanged, this, updatePasswordUI);
        connect(m_chkPassword, &QCheckBox::toggled, this, updatePasswordUI);

        mainLayout->addLayout(form);
    } else {
        m_lblTitle->setText("Extract Archive");
        QFileInfo info(m_archivePath);
        QLabel* lblFile = new QLabel(QString("Source Archive: %1").arg(info.fileName()), this);
        lblFile->setStyleSheet("color: #a6e3a1; font-weight: bold;");
        mainLayout->addWidget(lblFile);

        QLabel* lblDest = new QLabel(QString("Destination: %1").arg(QDir::toNativeSeparators(m_currentDir)), this);
        mainLayout->addWidget(lblDest);

        m_chkPassword = new QCheckBox("Archive is Password Protected", this);
        m_chkPassword->setStyleSheet("QCheckBox { color: #cdd6f4; }");
        mainLayout->addWidget(m_chkPassword);

        QHBoxLayout* passRow = new QHBoxLayout();
        passRow->addWidget(new QLabel("Decryption Password:", this));
        m_txtPassword = new QLineEdit(this);
        m_txtPassword->setEchoMode(QLineEdit::Password);
        m_txtPassword->setEnabled(false);
        passRow->addWidget(m_txtPassword);
        mainLayout->addLayout(passRow);

        connect(m_chkPassword, &QCheckBox::toggled, m_txtPassword, &QLineEdit::setEnabled);
    }

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_lblStatus = new QLabel(this);
    m_lblStatus->setStyleSheet("color: #a6adc8;");
    mainLayout->addWidget(m_lblStatus);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnAction = new QPushButton(m_mode == ModeCreate ? "Create" : "Extract", this);
    m_btnAction->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnAction, &QPushButton::clicked, this, &ArchiveDialog::onStartClicked);

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &ArchiveDialog::onCancelClicked);

    btnLayout->addWidget(m_btnAction);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);
}

void ArchiveDialog::onStartClicked() {
    if (m_mode == ModeCreate) {
        startCreation();
    } else {
        startExtraction();
    }
}

void ArchiveDialog::startExtraction() {
    if (m_archivePath.isEmpty() || !QFile::exists(m_archivePath)) {
        QMessageBox::warning(this, "Extraction Failed", "Invalid archive source file.");
        return;
    }

    m_btnAction->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate busy indicator during extraction
    m_lblStatus->setText("Extracting files...");
    m_isRunning = true;

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_currentDir);

    QString ext = QFileInfo(m_archivePath).suffix().toLower();
    QStringList args;

    if (m_archivePath.endsWith(".tar.gz") || m_archivePath.endsWith(".tgz")) {
        m_process->start("tar", {"-xzvf", m_archivePath, "-C", m_currentDir});
    } else if (m_archivePath.endsWith(".tar.xz")) {
        m_process->start("tar", {"-xJvf", m_archivePath, "-C", m_currentDir});
    } else if (m_archivePath.endsWith(".tar.bz2")) {
        m_process->start("tar", {"-xjvf", m_archivePath, "-C", m_currentDir});
    } else if (ext == "tar") {
        m_process->start("tar", {"-xvf", m_archivePath, "-C", m_currentDir});
    } else if (ext == "zip") {
        if (m_chkPassword->isChecked() && !m_txtPassword->text().isEmpty()) {
            m_process->start("unzip", {"-P", m_txtPassword->text(), "-o", m_archivePath, "-d", m_currentDir});
        } else {
            m_process->start("unzip", {"-o", m_archivePath, "-d", m_currentDir});
        }
    } else if (ext == "7z") {
        QStringList args;
        args << "x";
        if (m_chkPassword->isChecked() && !m_txtPassword->text().isEmpty()) {
            args << QString("-p%1").arg(m_txtPassword->text());
        }
        args << m_archivePath << QString("-o%1").arg(m_currentDir) << "-y";
        m_process->start("7z", args);
    } else {
        // Double fallback: try 7z first for unknown format, then tar
        if (ext == "7z" || ext == "rar") {
            QStringList args;
            args << "x" << m_archivePath << QString("-o%1").arg(m_currentDir) << "-y";
            m_process->start("7z", args);
        } else {
            m_process->start("tar", {"-xvf", m_archivePath, "-C", m_currentDir});
        }
    }

    connect(m_process, &QProcess::readyReadStandardOutput, this, &ArchiveDialog::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &ArchiveDialog::onProcessReadyRead);
    connect(m_process, &QProcess::finished, this, &ArchiveDialog::onProcessFinished);
}

void ArchiveDialog::startCreation() {
    QString targetName = m_txtTargetName->text().trimmed();
    if (targetName.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please enter a name for the archive.");
        return;
    }

    QString format = m_comboFormat->currentText();
    QString archiveFileName = targetName + format;
    m_archivePath = QDir(m_currentDir).filePath(archiveFileName);

    if (QFile::exists(m_archivePath)) {
        auto reply = QMessageBox::question(this, "File Exists", 
            QString("An archive named '%1' already exists. Overwrite?").arg(archiveFileName),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        QFile::remove(m_archivePath);
    }

    m_btnAction->setEnabled(false);
    m_txtTargetName->setEnabled(false);
    m_comboFormat->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Busy indicator
    m_lblStatus->setText("Creating archive...");
    m_isRunning = true;

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_currentDir);

    // Collect relative paths for clean tar/zip structures
    QStringList relativePaths;
    for (const QString& path : m_sourcePaths) {
        relativePaths << QDir(m_currentDir).relativeFilePath(path);
    }

    if (format == ".zip") {
        QStringList args;
        if (m_chkPassword->isChecked() && !m_txtPassword->text().isEmpty()) {
            args << "-r" << "-P" << m_txtPassword->text() << m_archivePath << relativePaths;
        } else {
            args << "-r" << m_archivePath << relativePaths;
        }
        m_process->start("zip", args);
    } else if (format == ".7z") {
        QStringList args;
        args << "a";
        if (m_chkPassword->isChecked() && !m_txtPassword->text().isEmpty()) {
            args << QString("-p%1").arg(m_txtPassword->text()) << "-mhe=on";
        }
        args << m_archivePath << relativePaths;
        m_process->start("7z", args);
    } else if (format == ".tar.gz") {
        QStringList args;
        args << "-czvf" << m_archivePath << relativePaths;
        m_process->start("tar", args);
    } else if (format == ".tar.xz") {
        QStringList args;
        args << "-cJvf" << m_archivePath << relativePaths;
        m_process->start("tar", args);
    } else if (format == ".tar") {
        QStringList args;
        args << "-cvf" << m_archivePath << relativePaths;
        m_process->start("tar", args);
    }

    connect(m_process, &QProcess::readyReadStandardOutput, this, &ArchiveDialog::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &ArchiveDialog::onProcessReadyRead);
    connect(m_process, &QProcess::finished, this, &ArchiveDialog::onProcessFinished);
}

void ArchiveDialog::onProcessReadyRead() {
    if (!m_process) return;

    QString output = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    QString err = QString::fromLocal8Bit(m_process->readAllStandardError());

    QString text = output.isEmpty() ? err : output;
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
        QString lastLine = lines.last().trimmed();
        if (lastLine.length() > 60) {
            lastLine = "..." + lastLine.right(60);
        }
        m_lblStatus->setText(lastLine);
    }
}

void ArchiveDialog::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    m_isRunning = false;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);

    if (status == QProcess::NormalExit && exitCode == 0) {
        m_lblStatus->setText("Operation completed successfully!");
        QMessageBox::information(this, "Success", "Archive operation finished successfully.");
        accept();
    } else {
        m_lblStatus->setText("Operation failed.");
        QMessageBox::critical(this, "Failed", "Archive operation encountered errors. Make sure utility packages are installed.");
        reject();
    }
}

void ArchiveDialog::onCancelClicked() {
    if (m_isRunning && m_process) {
        m_process->kill();
        m_process->waitForFinished();
    }
    reject();
}
