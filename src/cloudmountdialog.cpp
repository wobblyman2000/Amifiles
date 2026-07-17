#include "cloudmountdialog.h"
#include "remotemountmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QApplication>

CloudMountDialog::CloudMountDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    if (checkRcloneInstalled()) {
        refreshRemotes();
    } else {
        m_lblStatus->setText("Status: rclone is not installed. Please install it first.");
        m_lblStatus->setStyleSheet("color: #f38ba8; font-weight: bold;");
    }
}

bool CloudMountDialog::checkRcloneInstalled() {
    QProcess proc;
    proc.start("which", {"rclone"});
    proc.waitForFinished();
    return (proc.exitCode() == 0);
}

void CloudMountDialog::setupUI() {
    setWindowTitle("Rclone Cloud Storage Integration");
    resize(600, 420);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QListWidget { background-color: #11111b; border: 1px solid #313244; color: #cdd6f4; border-radius: 4px; padding: 4px; }"
        "QListWidget::item { padding: 6px; border-radius: 4px; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
        "QLineEdit { border: 1px solid #313244; background-color: #11111b; color: #cdd6f4; padding: 6px; border-radius: 4px; }"
        "QComboBox { border: 1px solid #313244; background-color: #11111b; color: #cdd6f4; padding: 6px; border-radius: 4px; }"
        "QPushButton { border: none; background-color: #313244; color: #cdd6f4; padding: 8px 16px; border-radius: 4px; font-weight: bold; min-width: 80px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    QLabel* header = new QLabel("<b>Configure & Mount Cloud Storage via Rclone</b>", this);
    header->setStyleSheet("font-size: 16px; color: #89b4fa;");
    mainLayout->addWidget(header);

    QHBoxLayout* content = new QHBoxLayout();
    content->setSpacing(16);

    // Left column: Remote lists
    QVBoxLayout* colLeft = new QVBoxLayout();
    colLeft->addWidget(new QLabel("Configured Remotes:", this));
    m_listRemotes = new QListWidget(this);
    colLeft->addWidget(m_listRemotes);

    QHBoxLayout* listButtons = new QHBoxLayout();
    QPushButton* btnRefresh = new QPushButton("Refresh", this);
    connect(btnRefresh, &QPushButton::clicked, this, &CloudMountDialog::refreshRemotes);
    listButtons->addWidget(btnRefresh);

    QPushButton* btnUnmount = new QPushButton("Unmount FUSE", this);
    btnUnmount->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; } QPushButton:hover { background-color: #e08b8b; }");
    connect(btnUnmount, &QPushButton::clicked, this, &CloudMountDialog::onUnmountRemote);
    listButtons->addWidget(btnUnmount);
    colLeft->addLayout(listButtons);

    content->addLayout(colLeft, 1);

    // Right column: Configurations and mount tools
    QVBoxLayout* colRight = new QVBoxLayout();
    colRight->setSpacing(12);

    // 1. Create New Remote Section
    QLabel* lblNew = new QLabel("<b>Create New Remote Connection</b>", this);
    colRight->addWidget(lblNew);

    QFormLayout* formNew = new QFormLayout();
    m_txtName = new QLineEdit(this);
    m_txtName->setPlaceholderText("e.g. gdrive");
    formNew->addRow("Remote Name:", m_txtName);

    m_comboType = new QComboBox(this);
    m_comboType->addItems({"drive", "onedrive", "dropbox", "sftp", "ftp"});
    formNew->addRow("Cloud Provider:", m_comboType);
    colRight->addLayout(formNew);

    QPushButton* btnCreate = new QPushButton("Configure Remote Connection", this);
    btnCreate->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(btnCreate, &QPushButton::clicked, this, &CloudMountDialog::onCreateRemote);
    colRight->addWidget(btnCreate);

    colRight->addWidget(new QLabel("<hr style='border: 1px solid #313244;'/>", this));

    // 2. Mount Section
    QLabel* lblMount = new QLabel("<b>Mount Connection</b>", this);
    colRight->addWidget(lblMount);

    QFormLayout* formMount = new QFormLayout();
    m_txtMountPath = new QLineEdit(this);
    m_txtMountPath->setText(QDir::homePath() + "/CloudMounts");
    formMount->addRow("Local Mount Base:", m_txtMountPath);
    colRight->addLayout(formMount);

    QPushButton* btnMount = new QPushButton("Mount Chosen Cloud Path", this);
    btnMount->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
    connect(btnMount, &QPushButton::clicked, this, &CloudMountDialog::onMountRemote);
    colRight->addWidget(btnMount);

    colRight->addStretch();
    content->addLayout(colRight, 1);

    mainLayout->addLayout(content, 1);

    // Bottom Status Bar
    m_lblStatus = new QLabel("Status: Ready", this);
    m_lblStatus->setStyleSheet("color: #a6adc8;");
    mainLayout->addWidget(m_lblStatus);

    // Close button
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch();
    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(btnClose);
    mainLayout->addLayout(bottom);
}

void CloudMountDialog::refreshRemotes() {
    m_listRemotes->clear();
    QProcess proc;
    proc.start("rclone", {"listremotes"});
    if (!proc.waitForFinished(5000)) return;

    QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString r = line.trimmed();
        if (r.endsWith(":")) {
            r.chop(1);
        }
        m_listRemotes->addItem(r);
    }
    m_lblStatus->setText(QString("Status: Configured remotes list refreshed. (%1 remotes found)").arg(m_listRemotes->count()));
}

void CloudMountDialog::onCreateRemote() {
    QString name = m_txtName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Required Input", "Please enter a name for the remote.");
        return;
    }
    QString type = m_comboType->currentText();

    m_lblStatus->setText(QString("Status: Running rclone config create for '%1'...").arg(name));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Start asynchronously so we don't freeze if browser auth is required
    QProcess* proc = new QProcess(this);
    proc->start("rclone", {"config", "create", name, type});
    
    QApplication::restoreOverrideCursor();

    QMessageBox::information(this, "Remote Authorization", 
        "Rclone remote connection process started. If OAuth authorization is needed, a browser window will open automatically.\n\n"
        "Click OK once authorization is complete, then click 'Refresh' in Amifiles to display the remote.");

    m_txtName->clear();
    refreshRemotes();
}

void CloudMountDialog::onMountRemote() {
    QListWidgetItem* item = m_listRemotes->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Selection Required", "Please select a configured remote from the list.");
        return;
    }

    QString remote = item->text();
    QString baseDir = m_txtMountPath->text().trimmed();
    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath() + "/CloudMounts";
    }

    QString mountPath = QDir(baseDir).filePath(remote);

    // Create folder structure
    QDir().mkpath(mountPath);

    m_lblStatus->setText(QString("Status: Mounting '%1' to '%2'...").arg(remote).arg(mountPath));
    
    // Mount using daemon mode
    QProcess proc;
    proc.start("rclone", {"mount", remote + ":", mountPath, "--vfs-cache-mode", "writes", "--daemon"});
    proc.waitForFinished();

    if (proc.exitCode() == 0) {
        RemoteMountManager::addActiveMount(remote, mountPath, "CLOUD");
        QMessageBox::information(this, "Success", QString("Cloud connection '%1' mounted successfully to:\n%2").arg(remote).arg(mountPath));
        m_lblStatus->setText("Status: Mount successful.");
        // Notify main window to reload drives (via updating QStorageInfo)
        QWidget* w = parentWidget();
        while (w && !w->inherits("MainWindow")) {
            w = w->parentWidget();
        }
        if (w) {
            QMetaObject::invokeMethod(w, "updateDrivesList");
        }
    } else {
        QString error = QString::fromLocal8Bit(proc.readAllStandardError());
        QMessageBox::critical(this, "Mount Failed", QString("Failed to mount cloud connection:\n%1").arg(error));
        m_lblStatus->setText("Status: Mount failed.");
    }
}

void CloudMountDialog::onUnmountRemote() {
    QListWidgetItem* item = m_listRemotes->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Selection Required", "Please select a remote to unmount.");
        return;
    }

    QString remote = item->text();
    QString baseDir = m_txtMountPath->text().trimmed();
    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath() + "/CloudMounts";
    }

    QString mountPath = QDir(baseDir).filePath(remote);

    m_lblStatus->setText(QString("Status: Unmounting '%1'...").arg(mountPath));

    QProcess proc;
    proc.start("fusermount", {"-u", mountPath});
    proc.waitForFinished();

    if (proc.exitCode() == 0) {
        QMessageBox::information(this, "Success", "Remote storage unmounted successfully.");
        m_lblStatus->setText("Status: Unmount successful.");
        
        // Notify main window to update drives list
        QWidget* w = parentWidget();
        while (w && !w->inherits("MainWindow")) {
            w = w->parentWidget();
        }
        if (w) {
            QMetaObject::invokeMethod(w, "updateDrivesList");
        }
    } else {
        QString error = QString::fromLocal8Bit(proc.readAllStandardError());
        QMessageBox::critical(this, "Unmount Failed", QString("Failed to unmount folder:\n%1").arg(error));
        m_lblStatus->setText("Status: Unmount failed.");
    }
}
