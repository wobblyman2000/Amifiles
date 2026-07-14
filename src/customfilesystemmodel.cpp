#include "customfilesystemmodel.h"

CustomFileSystemModel::CustomFileSystemModel(QObject* parent)
    : QFileSystemModel(parent) {}

int CustomFileSystemModel::columnCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;
    return QFileSystemModel::columnCount(parent) + 5;
}

QVariant CustomFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 4: return "Title";
            case 5: return "Artist";
            case 6: return "Album";
            case 7: return "Bitrate";
            case 8: return "Resolution";
            default: break;
        }
    }
    return QFileSystemModel::headerData(section, orientation, role);
}

QVariant CustomFileSystemModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();
    int col = index.column();
    if (col >= 4 && role == Qt::DisplayRole) {
        QString filePath = fileInfo(index).absoluteFilePath();
        FileMetadata meta = getMetadata(filePath);
        switch (col) {
            case 4: return meta.title;
            case 5: return meta.artist;
            case 6: return meta.album;
            case 7: return (meta.bitrate <= 0) ? QVariant() : QString("%1 kbps").arg(meta.bitrate);
            case 8: {
                if (meta.imageDimensions.isValid()) {
                    return QString("%1x%2").arg(meta.imageDimensions.width()).arg(meta.imageDimensions.height());
                }
                return QVariant();
            }
            default: break;
        }
    }
    return QFileSystemModel::data(index, role);
}

void CustomFileSystemModel::clearCache() {
    m_metadataCache.clear();
}

FileMetadata CustomFileSystemModel::getMetadata(const QString& filePath) const {
    auto it = m_metadataCache.find(filePath);
    if (it != m_metadataCache.end()) {
        return *it;
    }
    FileMetadata meta = MetadataExtractor::extract(filePath);
    m_metadataCache.insert(filePath, meta);
    return meta;
}
