#include "customfilesystemmodel.h"
#include "tagmanager.h"
#include <QPainter>
#include <QIcon>
#include <QPixmap>

CustomFileSystemModel::CustomFileSystemModel(QObject* parent)
    : QFileSystemModel(parent) {}

int CustomFileSystemModel::columnCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;
    return QFileSystemModel::columnCount(parent) + 12;
}

QVariant CustomFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 4: return "Title";
            case 5: return "Artist";
            case 6: return "Album";
            case 7: return "Bitrate";
            case 8: return "Resolution";
            case 9: return "Date Taken";
            case 10: return "Camera Model";
            case 11: return "Genre";
            case 12: return "Year";
            case 13: return "Track";
            case 14: return "Duration";
            case 15: return "Codec";
            default: break;
        }
    }
    return QFileSystemModel::headerData(section, orientation, role);
}

QVariant CustomFileSystemModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();
    int col = index.column();

    if (col == 0) {
        QString filePath = fileInfo(index).absoluteFilePath();
        if (role == Qt::DecorationRole) {
            QString colorName = TagManager::instance().getFileColor(filePath);
            if (!colorName.isEmpty()) {
                QColor colVal = TagManager::instance().getColorValue(colorName);
                QIcon baseIcon = QFileSystemModel::data(index, role).value<QIcon>();
                QPixmap pix = baseIcon.pixmap(16, 16);
                if (!pix.isNull()) {
                    QPainter painter(&pix);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setBrush(colVal);
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(10, 10, 6, 6);
                    painter.end();
                    return QIcon(pix);
                }
            }
        } else if (role == Qt::DisplayRole) {
            QString baseName = QFileSystemModel::data(index, role).toString();
            QStringList tags = TagManager::instance().getFileTags(filePath);
            if (!tags.isEmpty()) {
                return QString("%1 [%2]").arg(baseName).arg(tags.join(", "));
            }
        }
    }

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
            case 9: return meta.dateTaken;
            case 10: return meta.cameraModel;
            case 11: return meta.genre;
            case 12: return meta.year;
            case 13: return meta.track;
            case 14: return meta.durationStr;
            case 15: {
                if (!meta.codec.isEmpty()) return meta.codec;
                if (!meta.imageFormat.isEmpty()) return meta.imageFormat;
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
