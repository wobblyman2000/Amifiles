#ifndef CUSTOMFILESYSTEMMODEL_H
#define CUSTOMFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QHash>
#include "metadataextractor.h"

struct CustomColumn {
    QString name;
    QString type; // "BuiltIn", "Metadata", "Annotation", "CustomText", "EmbeddedArtwork"
    QString key;
};

class CustomFileSystemModel : public QFileSystemModel {
    Q_OBJECT
public:
    explicit CustomFileSystemModel(QObject* parent = nullptr);
    ~CustomFileSystemModel() override = default;

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void clearCache();
    void loadColumnLayout();
    void saveColumnLayout();
    QList<CustomColumn> activeColumns() const { return m_activeColumns; }
    void setActiveColumns(const QList<CustomColumn>& cols);

public slots:
    void onThumbnailGenerated(const QString& filePath, const QImage& img);
    void onChecksumGenerated(const QString& filePath, const QString& md5);

private:
    FileMetadata getMetadata(const QString& filePath) const;
    QIcon getEmbeddedArtworkIcon(const QString& filePath) const;
    QIcon getRetroOrComicIcon(const QString& filePath) const;
    QIcon drawRetroDiskIcon(const QString& filename, const QString& ext, int size) const;

    mutable QHash<QString, FileMetadata> m_metadataCache;
    QList<CustomColumn> m_activeColumns;
    mutable QHash<QString, QIcon> m_thumbnailCache;
    mutable QSet<QString> m_pendingThumbnails;
    mutable QHash<QString, QString> m_checksumCache;
    mutable QSet<QString> m_pendingChecksums;
};

#endif // CUSTOMFILESYSTEMMODEL_H
