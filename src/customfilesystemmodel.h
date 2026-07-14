#ifndef CUSTOMFILESYSTEMMODEL_H
#define CUSTOMFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QHash>
#include "metadataextractor.h"

class CustomFileSystemModel : public QFileSystemModel {
    Q_OBJECT
public:
    explicit CustomFileSystemModel(QObject* parent = nullptr);
    ~CustomFileSystemModel() override = default;

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void clearCache();

private:
    FileMetadata getMetadata(const QString& filePath) const;

    mutable QHash<QString, FileMetadata> m_metadataCache;
};

#endif // CUSTOMFILESYSTEMMODEL_H
