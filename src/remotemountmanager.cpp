#include "remotemountmanager.h"
#include "remotemountdialog.h"
#include "cloudmountdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStyle>
#include <QApplication>

RemoteMountManager::RemoteMountManager(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Remote & Cloud VFS Mounts Manager");
    resize(500, 300);
    setupUI();
    refreshList();
}

void RemoteMountManager::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnUnmount { background-color: #f38ba8; color: #11111b; }"
        "QPushButton#btnUnmount:hover { background-color: #f5e0dc; }"
    );

    QLabel* titleLabel = new QLabel("<b>Active VFS Connections & Mount Points</b>", this);
    mainLayout->addWidget(titleLabel);

    m_mountsList = new QListWidget(this);
    mainLayout->addWidget(m_mountsList, 1);

    QHBoxLayout* btnLayout = new QHBoxLayout();

    QPushButton* btnAddRemote = new QPushButton("Add FTP/SFTP/Samba...", this);
    connect(btnAddRemote, &QPushButton::clicked, this, &RemoteMountManager::onAddRemoteClicked);
    btnLayout->addWidget(btnAddRemote);

    QPushButton* btnAddCloud = new QPushButton("Add Google Drive/Cloud...", this);
    connect(btnAddCloud, &QPushButton::clicked, this, &RemoteMountManager::onAddCloudClicked);
    btnLayout->addWidget(btnAddCloud);

    btnLayout->addStretch();

    QPushButton* btnUnmount = new QPushButton("Unmount Selected", this);
    btnUnmount->setObjectName("btnUnmount");
    connect(btnUnmount, &QPushButton::clicked, this, &RemoteMountManager::onUnmountClicked);
    btnLayout->addWidget(btnUnmount);

    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnClose);

    mainLayout->addLayout(btnLayout);
}

void RemoteMountManager::refreshList() {
    m_mountsList->clear();
    QList<ActiveMount> mounts = getActiveMounts();
    QStyle* style = QApplication::style();

    for (const auto& m : mounts) {
        QListWidgetItem* item = new QListWidgetItem(m_mountsList);
        item->setText(QString("%1 (%2) -> %3").arg(m.name).arg(m.type).arg(m.path));
        item->setData(Qt::UserRole, m.path);
        item->setIcon(style->standardIcon(QStyle::SP_DriveNetIcon));
        m_mountsList->addItem(item);
    }
}

void RemoteMountManager::onUnmountClicked() {
    QListWidgetItem* item = m_mountsList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Unmount", "Please select a mount point to unmount.");
        return;
    }

    QString path = item->data(Qt::UserRole).toString();

    QProcess proc;
    proc.start("fusermount", {"-u", path});
    if (!proc.waitForFinished() || proc.exitCode() != 0) {
        QProcess fallback;
        fallback.start("umount", {path});
        fallback.waitForFinished();
    }

    removeActiveMount(path);
    QMessageBox::information(this, "Unmount VFS", "Unmount command triggered successfully!");
    refreshList();
}

void RemoteMountManager::onAddRemoteClicked() {
    RemoteMountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshList();
    }
}

void RemoteMountManager::onAddCloudClicked() {
    CloudMountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshList();
    }
}

void RemoteMountManager::addActiveMount(const QString& name, const QString& path, const QString& type) {
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized = settings.value("active_mounts/list").toStringList();
    QString record = QString("%1;%2;%3").arg(name).arg(path).arg(type);
    if (!serialized.contains(record)) {
        serialized.append(record);
        settings.setValue("active_mounts/list", serialized);
    }
}

void RemoteMountManager::removeActiveMount(const QString& path) {
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized = settings.value("active_mounts/list").toStringList();
    QStringList updated;
    for (const QString& r : serialized) {
        if (!r.contains(";" + path + ";") && !r.endsWith(";" + path)) {
            updated.append(r);
        }
    }
    settings.setValue("active_mounts/list", updated);
}

QList<ActiveMount> RemoteMountManager::getActiveMounts() {
    QList<ActiveMount> list;
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized = settings.value("active_mounts/list").toStringList();
    for (const QString& r : serialized) {
        QStringList parts = r.split(';');
        if (parts.size() >= 3) {
            ActiveMount m;
            m.name = parts[0];
            m.path = parts[1];
            m.type = parts[2];
            list.append(m);
        }
    }
    return list;
}
