#include "filehistorymanager.h"
#include <QDirIterator>
#include <QDebug>

FileHistoryManager& FileHistoryManager::instance() {
    static FileHistoryManager inst;
    return inst;
}

QString FileHistoryManager::getHistoryDir(const QString& filePath) const {
    QFileInfo fi(filePath);
    QString dir = fi.absolutePath();
    QString histDir = QDir(dir).filePath(".amifiles_history");
    QDir().mkpath(histDir);
    return histDir;
}

bool FileHistoryManager::createSnapshot(const QString& filePath) {
    if (!QFile::exists(filePath)) return false;

    QFileInfo fi(filePath);
    QString histDir = getHistoryDir(filePath);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString snapName = QString("%1_%2.bak").arg(fi.fileName()).arg(timestamp);
    QString snapPath = QDir(histDir).filePath(snapName);

    return QFile::copy(filePath, snapPath);
}

QList<RevisionSnapshot> FileHistoryManager::getRevisions(const QString& filePath) const {
    QList<RevisionSnapshot> list;
    if (filePath.isEmpty()) return list;

    QFileInfo fi(filePath);
    QString histDir = getHistoryDir(filePath);
    QString prefix = fi.fileName() + "_";

    QDir dir(histDir);
    QFileInfoList entries = dir.entryInfoList({prefix + "*.bak"}, QDir::Files, QDir::Time);

    for (const QFileInfo& entry : entries) {
        RevisionSnapshot snap;
        snap.snapshotPath = entry.absoluteFilePath();
        snap.timestamp = entry.lastModified();
        snap.size = entry.size();
        list.append(snap);
    }

    return list;
}

bool FileHistoryManager::restoreRevision(const QString& filePath, const QString& snapshotPath) {
    if (!QFile::exists(snapshotPath)) return false;

    // Create a backup of current before restoring!
    createSnapshot(filePath);

    QFile::remove(filePath);
    return QFile::copy(snapshotPath, filePath);
}
