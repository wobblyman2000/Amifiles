#include "remotemountmanager.h"
#include <QFileInfo>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <unistd.h>
#include <sys/types.h>
#include "remotemountdialog.h"
#include "cloudmountdialog.h"
#include "mainwindow.h"
#include "filepanel.h"
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
        QString displayType = m.type;
        if (m.type.startsWith("ISO|")) {
            QStringList parts = m.type.split('|');
            if (parts.size() >= 2) {
                displayType = QString("ISO (%1)").arg(parts[1]);
            } else {
                displayType = "ISO";
            }
        }
        item->setText(QString("%1 (%2) -> %3").arg(m.name).arg(displayType).arg(m.path));
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

    // Navigate panels away from the path if they are currently displaying it, to prevent 'device busy' blocks
    MainWindow* mw = qobject_cast<MainWindow*>(parentWidget());
    if (mw) {
        QString cleanPath = QDir::cleanPath(path);
        if (cleanPath.endsWith("/")) cleanPath.chop(1);

        FilePanel* lp = mw->leftPanel();
        FilePanel* rp = mw->rightPanel();
        if (lp) {
            QString lpPath = QDir::cleanPath(lp->currentPath());
            if (lpPath.endsWith("/")) lpPath.chop(1);
            if (lpPath == cleanPath || lpPath.startsWith(cleanPath + "/", Qt::CaseInsensitive)) {
                lp->setPath(QDir::homePath());
            }
        }
        if (rp) {
            QString rpPath = QDir::cleanPath(rp->currentPath());
            if (rpPath.endsWith("/")) rpPath.chop(1);
            if (rpPath == cleanPath || rpPath.startsWith(cleanPath + "/", Qt::CaseInsensitive)) {
                rp->setPath(QDir::homePath());
            }
        }
        QCoreApplication::processEvents();
    }

    QList<ActiveMount> mounts = getActiveMounts();
    for (const auto& m : mounts) {
        if (m.path == path) {
            if (m.type.startsWith("ISO|")) {
                QStringList parts = m.type.split('|');
                if (parts.size() >= 2) {
                    QString loopDev = parts[1];
                    QProcess::execute("udisksctl", {"unmount", "-b", loopDev});
                    QProcess::execute("udisksctl", {"loop-delete", "-b", loopDev});
                }
            } else if (m.type.startsWith("VHD_LOOP|")) {
                QStringList parts = m.type.split('|');
                if (parts.size() >= 2) {
                    QString loopDev = parts[1];
                    QString deviceToMount = loopDev;
                    if (QFile::exists(loopDev + "p1")) {
                        deviceToMount = loopDev + "p1";
                    }
                    QProcess::execute("udisksctl", {"unmount", "-b", deviceToMount});
                    QProcess::execute("udisksctl", {"unmount", "-b", loopDev});
                    QProcess::execute("udisksctl", {"loop-delete", "-b", loopDev});
                }
            } else if (m.type.startsWith("VHD_GUEST|")) {
                QProcess::execute("guestunmount", {path});
                QProcess::execute("fusermount", {"-u", path});
                QDir().rmdir(path);
            } else {
                QProcess proc;
                if (path.startsWith("/run/user/") && path.contains("/gvfs/")) {
                    proc.start("gio", {"mount", "-u", "-f", path});
                } else {
                    proc.start("fusermount", {"-u", "-z", path});
                }
                if (!proc.waitForFinished() || proc.exitCode() != 0) {
                    QProcess fallback;
                    if (path.startsWith("/run/user/") && path.contains("/gvfs/")) {
                        fallback.start("gio", {"mount", "-u", path});
                    } else {
                        fallback.start("fusermount", {"-u", path});
                    }
                    if (!fallback.waitForFinished() || fallback.exitCode() != 0) {
                        QProcess fallbackUmount;
                        fallbackUmount.start("umount", {"-l", path});
                        fallbackUmount.waitForFinished();
                    }
                }
            }
            removeActiveMount(path);
            break;
        }
    }

    QMessageBox::information(this, "Unmount VFS", "Unmount command triggered successfully!");
    refreshList();
}

void RemoteMountManager::onAddRemoteClicked() {
    RemoteMountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_targetNavigatePath = dlg.mountedPath();
        refreshList();
        accept();
    }
}

void RemoteMountManager::onAddCloudClicked() {
    CloudMountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_targetNavigatePath = dlg.mountedPath();
        refreshList();
        accept();
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
    
    // First, load from settings and validate directories
    for (const QString& r : serialized) {
        QStringList parts = r.split(';');
        if (parts.size() >= 3) {
            ActiveMount m;
            m.name = parts[0];
            m.path = parts[1];
            m.type = parts[2];
            
            // If the directory doesn't exist, it means the connection was lost/unmounted. Skip it.
            if (m.type == "FTP" || m.type == "SFTP" || m.type == "SAMBA" || m.type == "CLOUD" || m.type == "REMOTE") {
                bool isActive = false;
                if (m.path.startsWith("/run/user/") && m.path.contains("/gvfs/")) {
                    int gvfsIdx = m.path.indexOf("/gvfs/");
                    if (gvfsIdx != -1) {
                        QString gvfsRoot = m.path.left(gvfsIdx + 6);
                        QString sub = m.path.mid(gvfsIdx + 6);
                        QString firstPart = sub.split('/').first();
                        if (!firstPart.isEmpty()) {
                            QDir gvfsDir(gvfsRoot);
                            if (gvfsDir.exists() && gvfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).contains(firstPart)) {
                                isActive = true;
                            }
                        }
                    }
                } else {
                    isActive = QDir(m.path).exists();
                }

                if (!isActive) {
                    continue;
                }
            }
            list.append(m);
        }
    }

    // Second, dynamically scan GVFS mount points to auto-detect mounts from the OS
    QString runtimeDir = qgetenv("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QString("/run/user/%1").arg(getuid());
    }
    QString gvfsPath = runtimeDir + "/gvfs";
    QDir gvfsDir(gvfsPath);
    if (gvfsDir.exists()) {
        QStringList entries = gvfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            QString fullPath = gvfsDir.filePath(entry);
            bool alreadyInList = false;
            for (const auto& am : list) {
                if (am.path == fullPath) {
                    alreadyInList = true;
                    break;
                }
            }
            if (!alreadyInList) {
                ActiveMount m;
                m.path = fullPath;
                m.name = entry;
                if (entry.startsWith("ftp:", Qt::CaseInsensitive)) {
                    m.type = "FTP";
                } else if (entry.startsWith("sftp:", Qt::CaseInsensitive)) {
                    m.type = "SFTP";
                } else if (entry.startsWith("smb:", Qt::CaseInsensitive) || entry.contains("smb", Qt::CaseInsensitive)) {
                    m.type = "SAMBA";
                } else {
                    m.type = "REMOTE";
                }
                list.append(m);
            }
        }
    }

    return list;
}

bool RemoteMountManager::isIsoMounted(const QString& isoPath, QString& mountPath) {
    QList<ActiveMount> mounts = getActiveMounts();
    for (const auto& m : mounts) {
        if (m.type.startsWith("ISO|")) {
            QStringList parts = m.type.split('|');
            if (parts.size() >= 3 && parts[2] == isoPath) {
                mountPath = m.path;
                return true;
            }
        }
    }
    return false;
}

bool RemoteMountManager::mountIso(const QString& isoPath, QString& errorMsg, QString& mountPath) {
    // 1. Setup loop device
    QProcess setupProc;
    setupProc.start("udisksctl", {"loop-setup", "-f", isoPath});
    if (!setupProc.waitForFinished()) {
        errorMsg = "udisksctl loop-setup timed out.";
        return false;
    }
    if (setupProc.exitCode() != 0) {
        errorMsg = setupProc.readAllStandardError();
        if (errorMsg.isEmpty()) errorMsg = "udisksctl loop-setup failed with exit code " + QString::number(setupProc.exitCode());
        return false;
    }

    QString setupStdout = setupProc.readAllStandardOutput().trimmed();
    // Expected output: "Mapped file /path/to/file.iso as /dev/loopX."
    QString loopDevice;
    int asIndex = setupStdout.lastIndexOf("as ");
    if (asIndex != -1) {
        loopDevice = setupStdout.mid(asIndex + 3).trimmed();
        if (loopDevice.endsWith('.')) {
            loopDevice.chop(1);
        }
    }

    if (loopDevice.isEmpty()) {
        errorMsg = "Could not parse loop device from loop-setup output: " + setupStdout;
        return false;
    }

    // 2. Mount the loop device
    QProcess mountProc;
    mountProc.start("udisksctl", {"mount", "-b", loopDevice});
    if (!mountProc.waitForFinished()) {
        errorMsg = "udisksctl mount timed out.";
        QProcess::execute("udisksctl", {"loop-delete", "-b", loopDevice});
        return false;
    }
    if (mountProc.exitCode() != 0) {
        errorMsg = mountProc.readAllStandardError();
        if (errorMsg.isEmpty()) errorMsg = "udisksctl mount failed with exit code " + QString::number(mountProc.exitCode());
        QProcess::execute("udisksctl", {"loop-delete", "-b", loopDevice});
        return false;
    }

    QString mountStdout = mountProc.readAllStandardOutput().trimmed();
    // Expected output: "Mounted /dev/loopX at /run/media/dave/VolumeName."
    QString parsedMountPath;
    int atIndex = mountStdout.lastIndexOf("at ");
    if (atIndex != -1) {
        parsedMountPath = mountStdout.mid(atIndex + 3).trimmed();
        if (parsedMountPath.endsWith('.')) {
            parsedMountPath.chop(1);
        }
    }

    if (parsedMountPath.isEmpty()) {
        errorMsg = "Could not parse mount path from mount output: " + mountStdout;
        QProcess::execute("udisksctl", {"unmount", "-b", loopDevice});
        QProcess::execute("udisksctl", {"loop-delete", "-b", loopDevice});
        return false;
    }

    mountPath = parsedMountPath;
    addActiveMount(QFileInfo(isoPath).fileName(), mountPath, QString("ISO|%1|%2").arg(loopDevice).arg(isoPath));
    return true;
}

bool RemoteMountManager::unmountIso(const QString& isoPath, QString& errorMsg) {
    QList<ActiveMount> mounts = getActiveMounts();
    for (const auto& m : mounts) {
        if (m.type.startsWith("ISO|")) {
            QStringList parts = m.type.split('|');
            if (parts.size() >= 3 && parts[2] == isoPath) {
                QString loopDevice = parts[1];
                QString mountPath = m.path;

                // 1. Unmount
                QProcess unmountProc;
                unmountProc.start("udisksctl", {"unmount", "-b", loopDevice});
                if (!unmountProc.waitForFinished() || unmountProc.exitCode() != 0) {
                    errorMsg = unmountProc.readAllStandardError();
                    if (errorMsg.isEmpty()) errorMsg = "Failed to unmount " + loopDevice;
                    return false;
                }

                // 2. Loop-delete
                QProcess deleteProc;
                deleteProc.start("udisksctl", {"loop-delete", "-b", loopDevice});
                if (!deleteProc.waitForFinished() || deleteProc.exitCode() != 0) {
                    errorMsg = deleteProc.readAllStandardError();
                    if (errorMsg.isEmpty()) errorMsg = "Failed to delete loop device " + loopDevice;
                    return false;
                }

                removeActiveMount(mountPath);
                return true;
            }
        }
    }
    errorMsg = "ISO is not currently mounted.";
    return false;
}

bool RemoteMountManager::isVhdMounted(const QString& vhdPath, QString& mountPath) {
    QList<ActiveMount> mounts = getActiveMounts();
    for (const auto& m : mounts) {
        if (m.type.startsWith("VHD_GUEST|") || m.type.startsWith("VHD_LOOP|")) {
            QStringList parts = m.type.split('|');
            if (parts.size() >= 2) {
                QString pathInType = (parts.size() == 2) ? parts[1] : parts[2];
                if (pathInType == vhdPath) {
                    mountPath = m.path;
                    return true;
                }
            }
        }
    }
    return false;
}

bool RemoteMountManager::mountVhd(const QString& vhdPath, QString& errorMsg, QString& mountPath) {
    bool hasGuestmount = !QStandardPaths::findExecutable("guestmount").isEmpty();
    if (hasGuestmount) {
        QString hash = QString::number(qHash(vhdPath), 16);
        QString targetDir = QString("/tmp/amifiles_vhd_%1").arg(hash);
        QDir().mkpath(targetDir);

        QProcess proc;
        proc.start("guestmount", {"-a", vhdPath, "-i", "--rw", targetDir});
        if (proc.waitForFinished()) {
            if (proc.exitCode() == 0) {
                mountPath = targetDir;
                addActiveMount(QFileInfo(vhdPath).fileName(), mountPath, QString("VHD_GUEST|%1").arg(vhdPath));
                return true;
            } else {
                QProcess roProc;
                roProc.start("guestmount", {"-a", vhdPath, "-i", "--ro", targetDir});
                if (roProc.waitForFinished() && roProc.exitCode() == 0) {
                    mountPath = targetDir;
                    addActiveMount(QFileInfo(vhdPath).fileName(), mountPath, QString("VHD_GUEST|%1").arg(vhdPath));
                    return true;
                }
            }
        }
        QDir().rmdir(targetDir);
    }

    QProcess setupProc;
    setupProc.start("udisksctl", {"loop-setup", "-f", vhdPath});
    if (!setupProc.waitForFinished()) {
        errorMsg = "udisksctl loop-setup timed out.";
        return false;
    }
    if (setupProc.exitCode() != 0) {
        errorMsg = setupProc.readAllStandardError();
        if (errorMsg.isEmpty()) errorMsg = "udisksctl loop-setup failed with exit code " + QString::number(setupProc.exitCode());
        if (!hasGuestmount) {
            errorMsg += "\nNote: Dynamic VHDs require 'guestmount' to mount in user space. Please install 'libguestfs'.";
        }
        return false;
    }

    QString setupStdout = setupProc.readAllStandardOutput().trimmed();
    QString loopDevice;
    int asIndex = setupStdout.lastIndexOf("as ");
    if (asIndex != -1) {
        loopDevice = setupStdout.mid(asIndex + 3).trimmed();
        if (loopDevice.endsWith('.')) {
            loopDevice.chop(1);
        }
    }

    if (loopDevice.isEmpty()) {
        errorMsg = "Could not parse loop device from loop-setup output: " + setupStdout;
        return false;
    }

    QThread::msleep(500);

    QString deviceToMount = loopDevice;
    if (QFile::exists(loopDevice + "p1")) {
        deviceToMount = loopDevice + "p1";
    }

    QProcess mountProc;
    mountProc.start("udisksctl", {"mount", "-b", deviceToMount});
    if (!mountProc.waitForFinished()) {
        errorMsg = "udisksctl mount timed out.";
        QProcess::execute("udisksctl", {"loop-delete", "-b", loopDevice});
        return false;
    }
    if (mountProc.exitCode() != 0) {
        errorMsg = mountProc.readAllStandardError();
        if (errorMsg.isEmpty()) errorMsg = "udisksctl mount failed with exit code " + QString::number(mountProc.exitCode());
        if (!hasGuestmount) {
            errorMsg += "\nNote: Dynamic VHDs require 'guestmount' to mount in user space. Please install 'libguestfs'.";
        }
        QProcess::execute("udisksctl", {"loop-delete", "-b", loopDevice});
        return false;
    }

    QString mountStdout = mountProc.readAllStandardOutput().trimmed();
    QString parsedMountPath;
    int atIndex = mountStdout.lastIndexOf("at ");
    if (atIndex != -1) {
        parsedMountPath = mountStdout.mid(atIndex + 3).trimmed();
        if (parsedMountPath.endsWith('.')) {
            parsedMountPath.chop(1);
        }
    }

    if (parsedMountPath.isEmpty()) {
        errorMsg = "Could not parse mount path from mount output: " + mountStdout;
        QProcess::execute("udisksctl", {"unmount", "-b", deviceToMount});
        QProcess::execute("udisksctl", {"loop-delete", "-b", loopDevice});
        return false;
    }

    mountPath = parsedMountPath;
    addActiveMount(QFileInfo(vhdPath).fileName(), mountPath, QString("VHD_LOOP|%1|%2").arg(loopDevice).arg(vhdPath));
    return true;
}

bool RemoteMountManager::unmountVhd(const QString& vhdPath, QString& errorMsg) {
    QList<ActiveMount> mounts = getActiveMounts();
    for (const auto& m : mounts) {
        if (m.type.startsWith("VHD_GUEST|") || m.type.startsWith("VHD_LOOP|")) {
            QStringList parts = m.type.split('|');
            if (parts.size() >= 2) {
                QString pathInType = (parts.size() == 2) ? parts[1] : parts[2];
                if (pathInType == vhdPath) {
                    QString mountPath = m.path;
                    if (m.type.startsWith("VHD_GUEST|")) {
                        QProcess unmountProc;
                        unmountProc.start("guestunmount", {mountPath});
                        if (!unmountProc.waitForFinished() || unmountProc.exitCode() != 0) {
                            QProcess fallback;
                            fallback.start("fusermount", {"-u", mountPath});
                            if (!fallback.waitForFinished() || fallback.exitCode() != 0) {
                                errorMsg = "Failed to unmount guest FUSE point " + mountPath;
                                return false;
                            }
                        }
                        QDir().rmdir(mountPath);
                        removeActiveMount(mountPath);
                        return true;
                    } else {
                        QString loopDevice = parts[1];
                        QString deviceToMount = loopDevice;
                        if (QFile::exists(loopDevice + "p1")) {
                            deviceToMount = loopDevice + "p1";
                        }

                        QProcess unmountProc;
                        unmountProc.start("udisksctl", {"unmount", "-b", deviceToMount});
                        if (!unmountProc.waitForFinished() || unmountProc.exitCode() != 0) {
                            QProcess fallback;
                            fallback.start("udisksctl", {"unmount", "-b", loopDevice});
                            fallback.waitForFinished();
                        }

                        QProcess deleteProc;
                        deleteProc.start("udisksctl", {"loop-delete", "-b", loopDevice});
                        if (!deleteProc.waitForFinished() || deleteProc.exitCode() != 0) {
                            errorMsg = deleteProc.readAllStandardError();
                            if (errorMsg.isEmpty()) errorMsg = "Failed to delete loop device " + loopDevice;
                            return false;
                        }

                        removeActiveMount(mountPath);
                        return true;
                    }
                }
            }
        }
    }
    errorMsg = "VHD is not currently mounted.";
    return false;
}
