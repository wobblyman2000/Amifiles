#ifndef ARCHIVEMODEL_H
#define ARCHIVEMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QStringList>
#include <QList>
#include <QIcon>
#include <QMimeData>
#include <QMap>

struct ArchiveFileEntry {
    QString name;
    QString fullVirtualPath;
    qint64 size = 0;
    bool isDir = false;
    QDateTime modified;
};

class ArchiveModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ArchiveModel(QObject* parent = nullptr);
    ~ArchiveModel() override;

    bool loadArchive(const QString& archivePath);
    QString archivePath() const { return m_archivePath; }
    QString currentVirtualPath() const { return m_currentVirtualPath; }

    void enterDirectory(const QString& name);
    void navigateUp();
    void navigateToVirtualPath(const QString& path);

    bool addFiles(const QStringList& localPaths);
    bool deleteFiles(const QStringList& virtualPaths);
    bool createDirectory(const QString& name);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool isDir(const QModelIndex& index) const;
    QString entryPath(const QModelIndex& index) const;
    QString entryName(const QModelIndex& index) const;

    QString extractFile(const QString& virtualFilePath);
    bool extractFileToPath(const QString& virtualFilePath, const QString& localDestPath);
    QString extractDirRecursively(const QString& virtualDirPath);

    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDragActions() const override;

private:
    void rebuildActiveEntries();
    void parseZip(const QString& stdoutText);
    void parseTar(const QString& stdoutText);
    void parseRar(const QString& stdoutText);
    void parse7z(const QString& stdoutText);
    void parseD64(const QString& stdoutText);
    void parseAdf(const QString& stdoutText);

    QString m_archivePath;
    QString m_currentVirtualPath; // Empty means root
    QList<ArchiveFileEntry> m_allEntries;
    QList<ArchiveFileEntry> m_activeEntries; // Filtered to current subfolder
    QMap<QString, QString> m_tempAdfPaths;
};

#endif // ARCHIVEMODEL_H
