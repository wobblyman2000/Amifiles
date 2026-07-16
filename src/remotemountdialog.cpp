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
    resize(720, 390);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QSpinBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QListWidget::item { padding: 6px 8px; border-radius: 4px; color: #cdd6f4; }"
        "QListWidget::item:hover { background-color: #313244; color: #f5c2e7; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
    );

    QHBoxLayout* horizontalLayout = new QHBoxLayout(this);
    horizontalLayout->setContentsMargins(16, 16, 16, 16);
    horizontalLayout->setSpacing(16);

    // LEFT COLUMN: Address Book Bookmarks
    QVBoxLayout* leftCol = new QVBoxLayout();
    leftCol->setSpacing(8);
    
    QLabel* lblBookmarks = new QLabel("<b>Address Book</b>", this);
    leftCol->addWidget(lblBookmarks);

    m_listAddresses = new QListWidget(this);
    m_listAddresses->setFocusPolicy(Qt::NoFocus);
    m_listAddresses->setMinimumWidth(220);
    connect(m_listAddresses, &QListWidget::currentRowChanged, this, &RemoteMountDialog::onAddressSelected);
    leftCol->addWidget(m_listAddresses, 1);

    m_btnSaveAddress = new QPushButton("Save Bookmark", this);
    m_btnSaveAddress->setStyleSheet(
        "QPushButton { background-color: #f5c2e7; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(m_btnSaveAddress, &QPushButton::clicked, this, &RemoteMountDialog::onSaveAddress);
    leftCol->addWidget(m_btnSaveAddress);

    m_btnDeleteAddress = new QPushButton("Delete Bookmark", this);
    m_btnDeleteAddress->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #f38ba8; color: #11111b; }"
    );
    connect(m_btnDeleteAddress, &QPushButton::clicked, this, &RemoteMountDialog::onDeleteAddress);
    leftCol->addWidget(m_btnDeleteAddress);

    horizontalLayout->addLayout(leftCol, 1);

    // RIGHT COLUMN: Connection Fields
    QVBoxLayout* rightCol = new QVBoxLayout();
    rightCol->setSpacing(12);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(8);

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
    m_editName->setPlaceholderText("e.g. WorkServer (Required to save)");
    grid->addWidget(m_editName, 6, 1);

    rightCol->addLayout(grid);

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
    rightCol->addLayout(btnLayout);

    horizontalLayout->addLayout(rightCol, 2);

    loadAddresses();
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

void RemoteMountDialog::onSaveAddress() {
    QString label = m_editName->text().trimmed();
    if (label.isEmpty()) {
        QMessageBox::warning(this, "Label Required", "Please enter a Display Label before saving.");
        return;
    }

    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("RemoteAddresses");
    settings.beginGroup(label);
    settings.setValue("protocol", m_comboType->currentIndex());
    settings.setValue("host", m_editHost->text().trimmed());
    settings.setValue("port", m_spinPort->value());
    settings.setValue("user", m_editUser->text().trimmed());
    settings.setValue("password", m_editPassword->text());
    settings.setValue("path", m_editPath->text().trimmed());
    settings.endGroup();
    settings.endGroup();

    loadAddresses();

    // Select the saved address in list
    for (int i = 0; i < m_listAddresses->count(); ++i) {
        if (m_listAddresses->item(i)->text() == label) {
            m_listAddresses->setCurrentRow(i);
            break;
        }
    }
}

void RemoteMountDialog::onDeleteAddress() {
    int curRow = m_listAddresses->currentRow();
    if (curRow < 0) return;

    QString label = m_listAddresses->currentItem()->text();
    if (QMessageBox::question(this, "Delete Bookmark", QString("Are you sure you want to delete '%1' from the Address Book?").arg(label)) == QMessageBox::Yes) {
        QSettings settings("Amifiles", "Amifiles");
        settings.beginGroup("RemoteAddresses");
        settings.remove(label);
        settings.endGroup();

        loadAddresses();

        // Clear details
        m_editName->clear();
        m_editHost->clear();
        m_editUser->clear();
        m_editPassword->clear();
        m_editPath->setText("/");
        m_comboType->setCurrentIndex(0);
        m_spinPort->setValue(22);
    }
}

void RemoteMountDialog::onAddressSelected(int row) {
    if (row < 0 || !m_listAddresses->item(row)) return;

    QString label = m_listAddresses->item(row)->text();

    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("RemoteAddresses");
    settings.beginGroup(label);
    m_comboType->setCurrentIndex(settings.value("protocol", 0).toInt());
    m_editHost->setText(settings.value("host").toString());
    m_spinPort->setValue(settings.value("port", 22).toInt());
    m_editUser->setText(settings.value("user").toString());
    m_editPassword->setText(settings.value("password").toString());
    m_editPath->setText(settings.value("path", "/").toString());
    m_editName->setText(label);
    settings.endGroup();
    settings.endGroup();
}

void RemoteMountDialog::loadAddresses() {
    m_listAddresses->clear();

    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("RemoteAddresses");
    QStringList groups = settings.childGroups();
    settings.endGroup();

    for (const QString& label : groups) {
        m_listAddresses->addItem(label);
    }
}
