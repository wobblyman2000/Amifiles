#include "archivemodel.h"
#include <QProcess>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QStyle>
#include <QApplication>
#include <QDateTime>
#include <QDebug>

ArchiveModel::ArchiveModel(QObject* parent)
    : QAbstractTableModel(parent) {
    m_currentVirtualPath = "";
}

bool ArchiveModel::loadArchive(const QString& archivePath) {
    beginResetModel();
    m_archivePath = archivePath;
    m_currentVirtualPath = "";
    m_allEntries.clear();
    m_activeEntries.clear();
    endResetModel();

    QFileInfo info(archivePath);
    if (!info.exists()) return false;

    QProcess proc;
    QString ext = info.suffix().toLower();
    
    if (ext == "zip") {
        proc.start("unzip", { "-l", archivePath });
        if (proc.waitForFinished() && proc.exitCode() == 0) {
            parseZip(QString::fromUtf8(proc.readAllStandardOutput()));
        }
    } else if (ext == "tar" || ext == "gz" || ext == "xz" || ext == "bz2" || ext == "tgz") {
        proc.start("tar", { "-tvf", archivePath });
        if (proc.waitForFinished() && proc.exitCode() == 0) {
            parseTar(QString::fromUtf8(proc.readAllStandardOutput()));
        }
    } else if (ext == "rar") {
        proc.start("unrar", { "l", archivePath });
        if (!proc.waitForFinished() || proc.exitCode() != 0) {
            proc.start("rar", { "l", archivePath });
            proc.waitForFinished();
        }
        if (proc.exitCode() == 0) {
            parseRar(QString::fromUtf8(proc.readAllStandardOutput()));
        }
    } else if (ext == "7z" || ext == "iso" || ext == "img") {
        proc.start("7z", { "l", archivePath });
        if (proc.waitForFinished() && proc.exitCode() == 0) {
            parse7z(QString::fromUtf8(proc.readAllStandardOutput()));
        }
    } else if (ext == "d64") {
        proc.start("c1541", { "-attach", archivePath, "-list" });
        if (proc.waitForFinished() && proc.exitCode() == 0) {
            parseD64(QString::fromUtf8(proc.readAllStandardOutput()));
        }
    } else if (ext == "adf") {
        proc.start("xdftool", { archivePath, "list" });
        if (proc.waitForFinished() && proc.exitCode() == 0) {
            parseAdf(QString::fromUtf8(proc.readAllStandardOutput()));
        }
    }

    beginResetModel();
    rebuildActiveEntries();
    endResetModel();

    return !m_allEntries.isEmpty();
}

void ArchiveModel::parseZip(const QString& stdoutText) {
    QStringList lines = stdoutText.split('\n');
    bool start = false;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("----")) {
            start = !start;
            continue;
        }
        if (!start) continue;

        // Line format: Length  Date  Time  Name
        // e.g. "     1024  2026-07-14 10:15   dir1/file.txt"
        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        if (parts.size() < 4) continue;

        qint64 size = parts[0].toLongLong();
        QString dateStr = parts[1];
        QString timeStr = parts[2];
        
        // Reassemble name in case it contains spaces
        QString name = trimmed.section(QRegularExpression("\\s+"), 3);

        ArchiveFileEntry entry;
        entry.fullVirtualPath = QDir::cleanPath(name);
        entry.name = QFileInfo(name).fileName();
        if (entry.name.isEmpty()) {
            // It's a directory ending in '/'
            entry.name = QDir(name).dirName();
            entry.isDir = true;
        } else {
            entry.isDir = name.endsWith('/');
        }
        entry.size = size;
        entry.modified = QDateTime::fromString(dateStr + " " + timeStr, "yyyy-MM-dd hh:mm");
        if (!entry.modified.isValid()) {
            entry.modified = QFileInfo(m_archivePath).lastModified();
        }

        m_allEntries.append(entry);
    }
}

void ArchiveModel::parseTar(const QString& stdoutText) {
    QStringList lines = stdoutText.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Line format: permissions owner/group size date time name
        // e.g. "-rw-r--r-- dave/dave      2048 2026-07-14 10:16 dir1/file.txt"
        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        if (parts.size() < 6) continue;

        QString perms = parts[0];
        qint64 size = parts[2].toLongLong();
        QString dateStr = parts[3];
        QString timeStr = parts[4];
        
        QString name = trimmed.section(QRegularExpression("\\s+"), 5);

        ArchiveFileEntry entry;
        entry.fullVirtualPath = QDir::cleanPath(name);
        entry.name = QFileInfo(name).fileName();
        if (entry.name.isEmpty()) {
            entry.name = QDir(name).dirName();
            entry.isDir = true;
        } else {
            entry.isDir = perms.startsWith('d') || name.endsWith('/');
        }
        entry.size = size;
        entry.modified = QDateTime::fromString(dateStr + " " + timeStr, "yyyy-MM-dd hh:mm:ss");
        if (!entry.modified.isValid()) {
            entry.modified = QDateTime::fromString(dateStr + " " + timeStr, "yyyy-MM-dd hh:mm");
        }
        if (!entry.modified.isValid()) {
            entry.modified = QFileInfo(m_archivePath).lastModified();
        }

        m_allEntries.append(entry);
    }
}

void ArchiveModel::parseRar(const QString& stdoutText) {
    QStringList lines = stdoutText.split('\n');
    bool start = false;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("----")) {
            start = !start;
            continue;
        }
        if (!start) continue;

        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        if (parts.size() < 4) continue;

        // Reassemble name
        QString name = trimmed.section(QRegularExpression("\\s+"), 0, 0); // simplifed
        qint64 size = parts[1].toLongLong();

        ArchiveFileEntry entry;
        entry.fullVirtualPath = QDir::cleanPath(name);
        entry.name = QFileInfo(name).fileName();
        entry.isDir = name.endsWith('/');
        entry.size = size;
        entry.modified = QFileInfo(m_archivePath).lastModified();

        m_allEntries.append(entry);
    }
}

void ArchiveModel::parse7z(const QString& stdoutText) {
    QStringList lines = stdoutText.split('\n');
    bool start = false;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("-------------------")) {
            start = !start;
            continue;
        }
        if (!start) continue;

        // format: Date Time Attr Size Compressed Name
        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        if (parts.size() < 6) continue;

        QString dateStr = parts[0];
        QString timeStr = parts[1];
        QString attr = parts[2];
        qint64 size = parts[3].toLongLong();

        // Reassemble name
        QString name = trimmed.section(QRegularExpression("\\s+"), 5);

        ArchiveFileEntry entry;
        entry.fullVirtualPath = QDir::cleanPath(name);
        entry.name = QFileInfo(name).fileName();
        if (entry.name.isEmpty()) {
            entry.name = QDir(name).dirName();
            entry.isDir = true;
        } else {
            entry.isDir = attr.contains('D') || name.endsWith('/');
        }
        entry.size = size;
        entry.modified = QDateTime::fromString(dateStr + " " + timeStr, "yyyy-MM-dd hh:mm:ss");
        if (!entry.modified.isValid()) {
            entry.modified = QFileInfo(m_archivePath).lastModified();
        }

        m_allEntries.append(entry);
    }
}

void ArchiveModel::rebuildActiveEntries() {
    m_activeEntries.clear();
    QStringList addedVirtualDirs;

    for (const ArchiveFileEntry& entry : m_allEntries) {
        QString path = entry.fullVirtualPath;
        
        // Match path inside current virtual directory
        if (m_currentVirtualPath.isEmpty()) {
            // Root directory matching
            QString remaining = path;
            if (remaining.startsWith('/')) remaining = remaining.mid(1);
            
            QStringList parts = remaining.split('/');
            if (parts.isEmpty() || parts[0].isEmpty()) continue;

            if (parts.size() == 1) {
                // Direct file child
                if (entry.isDir) {
                    if (!addedVirtualDirs.contains(entry.name)) {
                        m_activeEntries.append(entry);
                        addedVirtualDirs.append(entry.name);
                    }
                } else {
                    m_activeEntries.append(entry);
                }
            } else {
                // Folder child
                QString dirName = parts[0];
                if (!addedVirtualDirs.contains(dirName)) {
                    ArchiveFileEntry dirEntry;
                    dirEntry.name = dirName;
                    dirEntry.fullVirtualPath = dirName;
                    dirEntry.isDir = true;
                    dirEntry.size = 0;
                    dirEntry.modified = entry.modified;
                    m_activeEntries.append(dirEntry);
                    addedVirtualDirs.append(dirName);
                }
            }
        } else {
            // Nested directory matching
            QString prefix = m_currentVirtualPath + "/";
            if (path.startsWith(prefix, Qt::CaseInsensitive)) {
                QString remaining = path.mid(prefix.length());
                if (remaining.startsWith('/')) remaining = remaining.mid(1);
                
                QStringList parts = remaining.split('/');
                if (parts.isEmpty() || parts[0].isEmpty()) continue;

                if (parts.size() == 1) {
                    if (entry.isDir) {
                        if (!addedVirtualDirs.contains(entry.name)) {
                            m_activeEntries.append(entry);
                            addedVirtualDirs.append(entry.name);
                        }
                    } else {
                        m_activeEntries.append(entry);
                    }
                } else {
                    QString dirName = parts[0];
                    if (!addedVirtualDirs.contains(dirName)) {
                        ArchiveFileEntry dirEntry;
                        dirEntry.name = dirName;
                        dirEntry.fullVirtualPath = m_currentVirtualPath + "/" + dirName;
                        dirEntry.isDir = true;
                        dirEntry.size = 0;
                        dirEntry.modified = entry.modified;
                        m_activeEntries.append(dirEntry);
                        addedVirtualDirs.append(dirName);
                    }
                }
            }
        }
    }

    // Sort: directories first, then alphabetically
    std::sort(m_activeEntries.begin(), m_activeEntries.end(), [](const ArchiveFileEntry& a, const ArchiveFileEntry& b) {
        if (a.isDir != b.isDir) {
            return a.isDir > b.isDir;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
}

void ArchiveModel::enterDirectory(const QString& name) {
    beginResetModel();
    if (m_currentVirtualPath.isEmpty()) {
        m_currentVirtualPath = name;
    } else {
        m_currentVirtualPath += "/" + name;
    }
    rebuildActiveEntries();
    endResetModel();
}

void ArchiveModel::navigateUp() {
    if (m_currentVirtualPath.isEmpty()) return;

    beginResetModel();
    int idx = m_currentVirtualPath.lastIndexOf('/');
    if (idx == -1) {
        m_currentVirtualPath = "";
    } else {
        m_currentVirtualPath = m_currentVirtualPath.left(idx);
    }
    rebuildActiveEntries();
    endResetModel();
}

void ArchiveModel::navigateToVirtualPath(const QString& path) {
    beginResetModel();
    m_currentVirtualPath = QDir::cleanPath(path);
    if (m_currentVirtualPath == "." || m_currentVirtualPath == "/") {
        m_currentVirtualPath = "";
    }
    rebuildActiveEntries();
    endResetModel();
}

int ArchiveModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_activeEntries.size();
}

int ArchiveModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 4;
}

QVariant ArchiveModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_activeEntries.size()) return QVariant();

    const ArchiveFileEntry& entry = m_activeEntries[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return entry.name;
            case 1: {
                if (entry.isDir) return "";
                double kb = entry.size / 1024.0;
                double mb = kb / 1024.0;
                double gb = mb / 1024.0;
                if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
                if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
                if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
                return QString("%1 B").arg(entry.size);
            }
            case 2: return entry.isDir ? "Folder" : QString("%1 File").arg(QFileInfo(entry.name).suffix().toUpper());
            case 3: return entry.modified.toString("yyyy-MM-dd hh:mm");
        }
    }

    if (role == Qt::DecorationRole && index.column() == 0) {
        QStyle* style = QApplication::style();
        if (entry.isDir) {
            return style->standardIcon(QStyle::SP_DirIcon);
        } else {
            return style->standardIcon(QStyle::SP_FileIcon);
        }
    }

    return QVariant();
}

QVariant ArchiveModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "Name";
            case 1: return "Size";
            case 2: return "Type";
            case 3: return "Date Modified";
        }
    }
    return QVariant();
}

bool ArchiveModel::isDir(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= m_activeEntries.size()) return false;
    return m_activeEntries[index.row()].isDir;
}

QString ArchiveModel::entryPath(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= m_activeEntries.size()) return "";
    return m_activeEntries[index.row()].fullVirtualPath;
}

QString ArchiveModel::entryName(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= m_activeEntries.size()) return "";
    return m_activeEntries[index.row()].name;
}

QString ArchiveModel::extractFile(const QString& virtualFilePath) {
    QProcess proc;
    QString scratchDir = "/home/dave/.gemini/antigravity/scratch/extracted";
    QDir().mkpath(scratchDir);

    QString destPath = QDir(scratchDir).filePath(QFileInfo(virtualFilePath).fileName());

    if (m_archivePath.endsWith(".d64", Qt::CaseInsensitive)) {
        proc.start("c1541", { "-attach", m_archivePath, "-read", virtualFilePath, destPath });
    } else if (m_archivePath.endsWith(".adf", Qt::CaseInsensitive)) {
        proc.start("xdftool", { m_archivePath, "read", virtualFilePath, destPath });
    } else {
        proc.setStandardOutputFile(destPath);
        if (m_archivePath.endsWith(".zip", Qt::CaseInsensitive)) {
            proc.start("unzip", { "-p", m_archivePath, virtualFilePath });
        } else if (m_archivePath.endsWith(".rar", Qt::CaseInsensitive)) {
            proc.start("unrar", { "p", "-inul", m_archivePath, virtualFilePath });
        } else if (m_archivePath.endsWith(".7z", Qt::CaseInsensitive) || m_archivePath.endsWith(".iso", Qt::CaseInsensitive) || m_archivePath.endsWith(".img", Qt::CaseInsensitive)) {
            proc.start("7z", { "x", m_archivePath, virtualFilePath, "-so" });
        } else {
            proc.start("tar", { "-xOf", m_archivePath, virtualFilePath });
        }
    }

    if (proc.waitForFinished() && proc.exitCode() == 0) {
        return destPath;
    }
    return "";
}

static bool copyDirRecursively(const QString& src, const QString& dest) {
    QProcess proc;
    proc.start("cp", {"-r", src, dest});
    return proc.waitForFinished() && proc.exitCode() == 0;
}

bool ArchiveModel::deleteFiles(const QStringList& virtualPaths) {
    QFileInfo info(m_archivePath);
    QString ext = info.suffix().toLower();
    
    if (ext == "adf") {
        QProcess proc;
        bool ok = true;
        for (const QString& vp : virtualPaths) {
            proc.start("xdftool", { m_archivePath, "delete", vp });
            if (!proc.waitForFinished() || proc.exitCode() != 0) {
                ok = false;
            }
        }
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }

    if (ext == "d64") {
        QProcess proc;
        bool ok = true;
        for (const QString& vp : virtualPaths) {
            proc.start("c1541", { "-attach", m_archivePath, "-delete", vp });
            if (!proc.waitForFinished() || proc.exitCode() != 0) {
                ok = false;
            }
        }
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }

    if (ext == "zip") {
        QProcess proc;
        QStringList args = { "-d", m_archivePath };
        for (const QString& vp : virtualPaths) {
            args.append(vp);
            args.append(vp + "/*");
        }
        proc.start("zip", args);
        if (proc.waitForFinished() && proc.exitCode() == 0) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }
    
    QString compressionType;
    bool compressed = false;
    if (m_archivePath.endsWith(".tar.gz", Qt::CaseInsensitive) || m_archivePath.endsWith(".tgz", Qt::CaseInsensitive)) {
        compressed = true;
        compressionType = "gzip";
    } else if (m_archivePath.endsWith(".tar.xz", Qt::CaseInsensitive)) {
        compressed = true;
        compressionType = "xz";
    } else if (m_archivePath.endsWith(".tar.bz2", Qt::CaseInsensitive)) {
        compressed = true;
        compressionType = "bzip2";
    }
    
    QString tempTarPath = m_archivePath + ".temp_uncompressed.tar";
    if (compressed) {
        QProcess decompressProc;
        if (compressionType == "gzip") {
            decompressProc.start("sh", {"-c", QString("gunzip -c \"%1\" > \"%2\"").arg(m_archivePath).arg(tempTarPath)});
        } else if (compressionType == "xz") {
            decompressProc.start("sh", {"-c", QString("xz -d -c \"%1\" > \"%2\"").arg(m_archivePath).arg(tempTarPath)});
        } else if (compressionType == "bzip2") {
            decompressProc.start("sh", {"-c", QString("bunzip2 -c \"%1\" > \"%2\"").arg(m_archivePath).arg(tempTarPath)});
        }
        if (!decompressProc.waitForFinished() || decompressProc.exitCode() != 0) {
            QFile::remove(tempTarPath);
            return false;
        }
    } else {
        QFile::remove(tempTarPath);
        if (!QFile::copy(m_archivePath, tempTarPath)) return false;
    }
    
    QProcess deleteProc;
    QStringList tarArgs = { "--delete", "-f", tempTarPath };
    tarArgs.append(virtualPaths);
    deleteProc.start("tar", tarArgs);
    if (!deleteProc.waitForFinished() || deleteProc.exitCode() != 0) {
        QFile::remove(tempTarPath);
        return false;
    }
    
    if (compressed) {
        QProcess compressProc;
        if (compressionType == "gzip") {
            compressProc.start("sh", {"-c", QString("gzip -c \"%1\" > \"%2\"").arg(tempTarPath).arg(m_archivePath)});
        } else if (compressionType == "xz") {
            compressProc.start("sh", {"-c", QString("xz -c \"%1\" > \"%2\"").arg(tempTarPath).arg(m_archivePath)});
        } else if (compressionType == "bzip2") {
            compressProc.start("sh", {"-c", QString("bzip2 -c \"%1\" > \"%2\"").arg(tempTarPath).arg(m_archivePath)});
        }
        bool ok = compressProc.waitForFinished() && compressProc.exitCode() == 0;
        QFile::remove(tempTarPath);
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    } else {
        QFile::remove(m_archivePath);
        bool ok = QFile::copy(tempTarPath, m_archivePath);
        QFile::remove(tempTarPath);
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }
}

bool ArchiveModel::addFiles(const QStringList& localPaths) {
    if (localPaths.isEmpty()) return false;
    
    QFileInfo info(m_archivePath);
    QString ext = info.suffix().toLower();

    if (ext == "adf") {
        QProcess proc;
        bool ok = true;
        for (const QString& lp : localPaths) {
            QFileInfo lInfo(lp);
            QString destVirtualPath = m_currentVirtualPath.isEmpty() ? lInfo.fileName() : m_currentVirtualPath + "/" + lInfo.fileName();
            proc.start("xdftool", { m_archivePath, "write", lp, destVirtualPath });
            if (!proc.waitForFinished() || proc.exitCode() != 0) {
                ok = false;
            }
        }
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }

    if (ext == "d64") {
        QProcess proc;
        bool ok = true;
        for (const QString& lp : localPaths) {
            QFileInfo lInfo(lp);
            proc.start("c1541", { "-attach", m_archivePath, "-write", lp, lInfo.fileName().toUpper() });
            if (!proc.waitForFinished() || proc.exitCode() != 0) {
                ok = false;
            }
        }
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }
    
    QString tempAddDir = "/home/dave/cpp_projects/Amifiles/build/tmp_archive_add";
    QDir(tempAddDir).removeRecursively();
    QDir().mkpath(tempAddDir);
    
    QString targetSubdir = tempAddDir;
    if (!m_currentVirtualPath.isEmpty()) {
        targetSubdir = tempAddDir + "/" + m_currentVirtualPath;
        QDir().mkpath(targetSubdir);
    }
    
    QStringList relativePathsToZip;
    for (const QString& lp : localPaths) {
        QFileInfo lInfo(lp);
        QString destPath = targetSubdir + "/" + lInfo.fileName();
        if (lInfo.isDir()) {
            copyDirRecursively(lp, destPath);
        } else {
            QFile::copy(lp, destPath);
        }
        
        if (m_currentVirtualPath.isEmpty()) {
            relativePathsToZip.append(lInfo.fileName());
        }
    }
    
    if (!m_currentVirtualPath.isEmpty()) {
        QString firstSeg = m_currentVirtualPath.split('/').first();
        relativePathsToZip.append(firstSeg);
    }
    
    if (ext == "zip") {
        QProcess proc;
        proc.setWorkingDirectory(tempAddDir);
        QStringList args = { "-u", "-r", m_archivePath };
        args.append(relativePathsToZip);
        proc.start("zip", args);
        bool ok = proc.waitForFinished() && proc.exitCode() == 0;
        QDir(tempAddDir).removeRecursively();
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }
    
    QString compressionType;
    bool compressed = false;
    if (m_archivePath.endsWith(".tar.gz", Qt::CaseInsensitive) || m_archivePath.endsWith(".tgz", Qt::CaseInsensitive)) {
        compressed = true;
        compressionType = "gzip";
    } else if (m_archivePath.endsWith(".tar.xz", Qt::CaseInsensitive)) {
        compressed = true;
        compressionType = "xz";
    } else if (m_archivePath.endsWith(".tar.bz2", Qt::CaseInsensitive)) {
        compressed = true;
        compressionType = "bzip2";
    }
    
    QString tempTarPath = m_archivePath + ".temp_uncompressed.tar";
    if (compressed) {
        QProcess decompressProc;
        if (compressionType == "gzip") {
            decompressProc.start("sh", {"-c", QString("gunzip -c \"%1\" > \"%2\"").arg(m_archivePath).arg(tempTarPath)});
        } else if (compressionType == "xz") {
            decompressProc.start("sh", {"-c", QString("xz -d -c \"%1\" > \"%2\"").arg(m_archivePath).arg(tempTarPath)});
        } else if (compressionType == "bzip2") {
            decompressProc.start("sh", {"-c", QString("bunzip2 -c \"%1\" > \"%2\"").arg(m_archivePath).arg(tempTarPath)});
        }
        if (!decompressProc.waitForFinished() || decompressProc.exitCode() != 0) {
            QFile::remove(tempTarPath);
            QDir(tempAddDir).removeRecursively();
            return false;
        }
    } else {
        QFile::remove(tempTarPath);
        if (!QFile::copy(m_archivePath, tempTarPath)) {
            QDir(tempAddDir).removeRecursively();
            return false;
        }
    }
    
    QProcess addProc;
    addProc.setWorkingDirectory(tempAddDir);
    QStringList tarArgs = { "-rf", tempTarPath };
    tarArgs.append(relativePathsToZip);
    addProc.start("tar", tarArgs);
    if (!addProc.waitForFinished() || addProc.exitCode() != 0) {
        QFile::remove(tempTarPath);
        QDir(tempAddDir).removeRecursively();
        return false;
    }
    
    if (compressed) {
        QProcess compressProc;
        if (compressionType == "gzip") {
            compressProc.start("sh", {"-c", QString("gzip -c \"%1\" > \"%2\"").arg(tempTarPath).arg(m_archivePath)});
        } else if (compressionType == "xz") {
            compressProc.start("sh", {"-c", QString("xz -c \"%1\" > \"%2\"").arg(tempTarPath).arg(m_archivePath)});
        } else if (compressionType == "bzip2") {
            compressProc.start("sh", {"-c", QString("bzip2 -c \"%1\" > \"%2\"").arg(tempTarPath).arg(m_archivePath)});
        }
        bool ok = compressProc.waitForFinished() && compressProc.exitCode() == 0;
        QFile::remove(tempTarPath);
        QDir(tempAddDir).removeRecursively();
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    } else {
        QFile::remove(m_archivePath);
        bool ok = QFile::copy(tempTarPath, m_archivePath);
        QFile::remove(tempTarPath);
        QDir(tempAddDir).removeRecursively();
        if (ok) {
            loadArchive(m_archivePath);
            return true;
        }
        return false;
    }
}

void ArchiveModel::parseD64(const QString& stdoutText) {
    QStringList lines = stdoutText.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        int firstQuote = trimmed.indexOf('"');
        int lastQuote = trimmed.lastIndexOf('"');
        if (firstQuote != -1 && lastQuote > firstQuote) {
            QString name = trimmed.mid(firstQuote + 1, lastQuote - firstQuote - 1).trimmed();
            QString beforeQuote = trimmed.left(firstQuote).trimmed();
            qint64 size = beforeQuote.toLongLong() * 256;

            ArchiveFileEntry entry;
            entry.name = name;
            entry.fullVirtualPath = name;
            entry.size = size;
            entry.isDir = false;
            entry.modified = QFileInfo(m_archivePath).lastModified();
            m_allEntries.append(entry);
        }
    }
}

void ArchiveModel::parseAdf(const QString& stdoutText) {
    QStringList lines = stdoutText.split('\n');
    QStringList pathStack;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.contains("VOLUME") || trimmed.startsWith("sum:") || trimmed.startsWith("data:") || trimmed.startsWith("fs:")) {
            continue;
        }

        int leadingSpaces = 0;
        while (leadingSpaces < line.length() && line[leadingSpaces] == ' ') {
            leadingSpaces++;
        }

        // xdftool uses 2 spaces indentation per subdirectory level
        int depth = leadingSpaces / 2;
        while (pathStack.size() > depth - 1 && !pathStack.isEmpty()) {
            pathStack.removeLast();
        }

        QStringList parts = trimmed.split(QRegularExpression("\\s+"));
        if (parts.size() < 2) continue;

        QString name = parts[0];
        bool isDirectory = (parts[1] == "DIR");
        qint64 size = 0;
        if (!isDirectory) {
            size = parts[1].toLongLong();
        }

        QString fullPath;
        if (pathStack.isEmpty()) {
            fullPath = name;
        } else {
            fullPath = pathStack.join("/") + "/" + name;
        }

        ArchiveFileEntry entry;
        entry.name = name;
        entry.fullVirtualPath = fullPath;
        entry.size = size;
        entry.isDir = isDirectory;
        entry.modified = QFileInfo(m_archivePath).lastModified();
        m_allEntries.append(entry);

        if (isDirectory) {
            pathStack.append(name);
        }
    }
}
