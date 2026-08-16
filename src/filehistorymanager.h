#ifndef FILEHISTORYMANAGER_H
#define FILEHISTORYMANAGER_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QFile>

struct RevisionSnapshot {
    QString snapshotPath;
    QDateTime timestamp;
    qint64 size = 0;
};

class FileHistoryManager {
public:
    static FileHistoryManager& instance();

    bool createSnapshot(const QString& filePath);
    QList<RevisionSnapshot> getRevisions(const QString& filePath) const;
    bool restoreRevision(const QString& filePath, const QString& snapshotPath);

private:
    FileHistoryManager() = default;
    QString getHistoryDir(const QString& filePath) const;
};

#endif // FILEHISTORYMANAGER_H
