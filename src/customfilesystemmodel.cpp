#include "customfilesystemmodel.h"
#include "tagmanager.h"
#include <QPainter>
#include <QIcon>
#include <QPixmap>

CustomFileSystemModel::CustomFileSystemModel(QObject* parent)
    : QFileSystemModel(parent) {}

int CustomFileSystemModel::columnCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;
    return QFileSystemModel::columnCount(parent) + 13;
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
            case 16: return "Tags";
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
            QIcon baseIcon = QFileSystemModel::data(index, role).value<QIcon>();
            if (fileInfo(index).isDir()) {
                QString dirName = fileInfo(index).fileName();
                QString dirNameLower = dirName.toLower();
                QString themeName;
                if (dirNameLower.contains("game")) {
                    themeName = "applications-games";
                } else if (dirNameLower.contains("code") || dirNameLower.contains("prog") || dirNameLower.contains("dev") || dirNameLower.contains("src")) {
                    themeName = "applications-development";
                } else if (dirNameLower.contains("music") || dirNameLower.contains("song") || dirNameLower.contains("audio")) {
                    themeName = "folder-music";
                } else if (dirNameLower.contains("video") || dirNameLower.contains("movie") || dirNameLower.contains("film")) {
                    themeName = "folder-videos";
                } else if (dirNameLower.contains("picture") || dirNameLower.contains("photo") || dirNameLower.contains("image")) {
                    themeName = "folder-pictures";
                } else if (dirNameLower.contains("doc") || dirNameLower.contains("paper") || dirNameLower.contains("text")) {
                    themeName = "folder-documents";
                } else if (dirNameLower.contains("download")) {
                    themeName = "folder-download";
                }

                if (!themeName.isEmpty()) {
                    QIcon customIcon = QIcon::fromTheme(themeName);
                    if (!customIcon.isNull()) {
                        baseIcon = customIcon;
                    }
                }
            }

            QString colorName = TagManager::instance().getFileColor(filePath);
            if (!colorName.isEmpty() && colorName != "none") {
                QColor colVal = TagManager::instance().getColorValue(colorName);
                QIcon iconResult;
                QList<int> targetSizes = {16, 24, 32, 48, 64, 96, 128};
                for (int sz : targetSizes) {
                    QPixmap pix = baseIcon.pixmap(sz, sz);
                    if (!pix.isNull()) {
                        QPainter painter(&pix);
                        painter.setRenderHint(QPainter::Antialiasing);
                        painter.setBrush(colVal);
                        painter.setPen(Qt::NoPen);
                        
                        int dotSize = qMax(4, qRound(sz * 0.3));
                        int padding = qMax(1, qRound(sz * 0.05));
                        int x = sz - dotSize - padding;
                        int y = sz - dotSize - padding;
                        
                        painter.drawEllipse(x, y, dotSize, dotSize);
                        painter.end();
                        iconResult.addPixmap(pix);
                    }
                }
                if (!iconResult.isNull()) {
                    return iconResult;
                }
            }
            return baseIcon;
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
            case 16: {
                QStringList tags = TagManager::instance().getFileTags(filePath);
                return tags.isEmpty() ? QVariant() : tags.join(", ");
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
