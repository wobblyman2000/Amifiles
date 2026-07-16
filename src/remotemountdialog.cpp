#include "remotemountdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <unistd.h>

RemoteMountDialog::RemoteMountDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
}

void RemoteMountDialog::setupUI() {
    setWindowTitle("Mount Remote Share");
    resize(420, 320);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QSpinBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(10);

    // Type
    grid->addWidget(new QLabel("Protocol:", this), 0, 0);
    m_comboType = new QComboBox(this);
    m_comboType->addItems({"SFTP (SSH)", "FTP", "Samba (SMB)"});
    connect(m_comboType, &QComboBox::currentIndexChanged, this, &RemoteMountDialog::onTypeChanged);
    grid->addWidget(m_comboType, 0, 1);

    // Host
    grid->addWidget(new QLabel("Host / Server:", this), 1, 0);
    m_editHost = new QLineEdit(this);
    m_editHost->setPlaceholderText("e.g. 192.168.1.50 or ftp.example.com");
    grid->addWidget(m_editHost, 1, 1);

    // Port
    grid->addWidget(new QLabel("Port:", this), 2, 0);
    m_spinPort = new QSpinBox(this);
    m_spinPort->setRange(1, 65535);
    m_spinPort->setValue(22);
    grid->addWidget(m_spinPort, 2, 1);

    // User
    grid->addWidget(new QLabel("Username:", this), 3, 0);
    m_editUser = new QLineEdit(this);
    m_editUser->setPlaceholderText("Anonymous or login name");
    grid->addWidget(m_editUser, 3, 1);

    // Password
    grid->addWidget(new QLabel("Password:", this), 4, 0);
    m_editPassword = new QLineEdit(this);
    m_editPassword->setEchoMode(QLineEdit::Password);
    m_editPassword->setPlaceholderText("Password (optional)");
    grid->addWidget(m_editPassword, 4, 1);

    // Path
    grid->addWidget(new QLabel("Remote Path:", this), 5, 0);
    m_editPath = new QLineEdit(this);
    m_editPath->setText("/");
    grid->addWidget(m_editPath, 5, 1);

    // Name (custom label)
    grid->addWidget(new QLabel("Display Label:", this), 6, 0);
    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText("e.g. WorkServer");
    grid->addWidget(m_editName, 6, 1);

    mainLayout->addLayout(grid);

    // Action buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnMount = new QPushButton("Mount Share", this);
    m_btnMount->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(m_btnMount, &QPushButton::clicked, this, &RemoteMountDialog::onMount);

    m_btnCancel = new QPushButton("Cancel", this);
    m_btnCancel->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(m_btnCancel, &QPushButton::clicked, this, &RemoteMountDialog::onCancel);

    btnLayout->addStretch();
    btnLayout->addWidget(m_btnMount);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);
}

void RemoteMountDialog::onTypeChanged(int index) {
    if (index == 0) { // SFTP
        m_spinPort->setValue(22);
    } else if (index == 1) { // FTP
        m_spinPort->setValue(21);
    } else { // SMB
        m_spinPort->setValue(445);
    }
}

void RemoteMountDialog::onMount() {
    QString host = m_editHost->text().trimmed();
    QString user = m_editUser->text().trimmed();
    QString pass = m_editPassword->text();
    QString path = m_editPath->text().trimmed();
    QString label = m_editName->text().trimmed();
    int port = m_spinPort->value();

    if (host.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please enter a host or server address.");
        return;
    }

    if (label.isEmpty()) {
        label = host;
    }

    QString protocol;
    int typeIndex = m_comboType->currentIndex();
    if (typeIndex == 0) protocol = "sftp";
    else if (typeIndex == 1) protocol = "ftp";
    else protocol = "smb";

    // Build GVFS connection URL
    // Format: protocol://[user[:password]@]host[:port]/path
    QString url = protocol + "://";
    if (!user.isEmpty()) {
        url += user;
        if (!pass.isEmpty()) {
            url += ":" + pass;
        }
        url += "@";
    }
    url += host;
    if (typeIndex != 2) { // GVFS handles SMB ports differently or defaults are fine
        url += QString(":%1").arg(port);
    }
    if (!path.startsWith("/")) {
        url += "/";
    }
    url += path;

    // Run gio mount URL
    QProcess proc;
    proc.start("gio", {"mount", url});
    
    // Provide a visual loading cursor
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    if (!proc.waitForFinished(10000)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "Mount Timeout", "The mount operation timed out. Please verify server details and network.");
        return;
    }

    QApplication::restoreOverrideCursor();

    if (proc.exitCode() == 0) {
        // Find gvfs mount directory
        QString runtimeDir = qgetenv("XDG_RUNTIME_DIR");
        if (runtimeDir.isEmpty()) {
            runtimeDir = QString("/run/user/%1").arg(getuid());
        }
        QString gvfsPath = runtimeDir + "/gvfs";
        
        QDir gvfsDir(gvfsPath);
        QString mountedPath;
        
        // Scan for matching mount directory
        QStringList entries = gvfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            // e.g. entry matches host, user, or protocol
            if (entry.contains(host, Qt::CaseInsensitive) || entry.startsWith(protocol, Qt::CaseInsensitive)) {
                mountedPath = gvfsDir.filePath(entry);
                break;
            }
        }

        if (!mountedPath.isEmpty()) {
            // Save mount bookmark/label to settings so we can display it dynamically
            QSettings settings("Amifiles", "Amifiles");
            settings.beginGroup("RemoteMounts");
            settings.setValue(label, mountedPath);
            settings.endGroup();

            QMessageBox::information(this, "Success", QString("Remote share mounted successfully at: %1").arg(mountedPath));
            accept();
        } else {
            QMessageBox::warning(this, "Mounted", "Remote share mounted, but mount folder could not be located in GVFS.");
            reject();
        }
    } else {
        QString errMsg = proc.readAllStandardError();
        if (errMsg.isEmpty()) errMsg = proc.readAllStandardOutput();
        QMessageBox::critical(this, "Mount Failed", "Error mounting share: " + errMsg);
    }
}

void RemoteMountDialog::onCancel() {
    reject();
}
