#include "customfilesystemmodel.h"
#include "tagmanager.h"
#include <QPainter>
#include <QIcon>
#include <QPixmap>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

CustomFileSystemModel::CustomFileSystemModel(QObject* parent)
    : QFileSystemModel(parent) {
    loadColumnLayout();
}

int CustomFileSystemModel::columnCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;
    return QFileSystemModel::columnCount(parent) + m_activeColumns.size();
}

QVariant CustomFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section >= 4 && section < 4 + m_activeColumns.size()) {
            return m_activeColumns[section - 4].name;
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
            QString overlayIconName = TagManager::instance().getFileOverlayIcon(filePath);

            bool hasColor = (!colorName.isEmpty() && colorName != "none");
            bool hasOverlay = (!overlayIconName.isEmpty());

            if (hasColor || hasOverlay) {
                QColor colVal = hasColor ? TagManager::instance().getColorValue(colorName) : QColor(Qt::transparent);
                QIcon overlayIcon = hasOverlay ? QIcon::fromTheme(overlayIconName) : QIcon();
                
                QIcon iconResult;
                QList<int> targetSizes = {16, 24, 32, 48, 64, 96, 128};
                for (int sz : targetSizes) {
                    QPixmap pix = baseIcon.pixmap(sz, sz);
                    if (!pix.isNull()) {
                        QPainter painter(&pix);
                        painter.setRenderHint(QPainter::Antialiasing);
                        
                        if (hasOverlay && !overlayIcon.isNull()) {
                            int subSize = qMax(8, qRound(sz * 0.4));
                            int padding = qMax(1, qRound(sz * 0.05));
                            int x = sz - subSize - padding;
                            int y = sz - subSize - padding;
                            
                            QPixmap subPix = overlayIcon.pixmap(subSize, subSize);
                            if (!subPix.isNull()) {
                                painter.setBrush(hasColor ? colVal : QColor("#11111b"));
                                painter.setPen(QPen(hasColor ? QColor("#ffffff") : QColor("#89b4fa"), 1));
                                painter.drawRoundedRect(x - 1, y - 1, subSize + 2, subSize + 2, 2, 2);
                                painter.drawPixmap(x, y, subPix);
                            }
                        } else if (hasColor) {
                            painter.setBrush(colVal);
                            painter.setPen(Qt::NoPen);
                            
                            int dotSize = qMax(4, qRound(sz * 0.3));
                            int padding = qMax(1, qRound(sz * 0.05));
                            int x = sz - dotSize - padding;
                            int y = sz - dotSize - padding;
                            
                            painter.drawEllipse(x, y, dotSize, dotSize);
                        }
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

    if (col >= 4 && col < 4 + m_activeColumns.size()) {
        const CustomColumn& colDef = m_activeColumns[col - 4];
        QString filePath = fileInfo(index).absoluteFilePath();

        if (role == Qt::DisplayRole) {
            if (colDef.type == "Metadata") {
                FileMetadata meta = getMetadata(filePath);
                QString k = colDef.key.toLower();
                if (k == "title") return meta.title;
                if (k == "artist") return meta.artist;
                if (k == "album") return meta.album;
                if (k == "bitrate") return (meta.bitrate <= 0) ? QVariant() : QString("%1 kbps").arg(meta.bitrate);
                if (k == "resolution") {
                    if (meta.imageDimensions.isValid()) {
                        return QString("%1x%2").arg(meta.imageDimensions.width()).arg(meta.imageDimensions.height());
                    }
                    return QVariant();
                }
                if (k == "date taken" || k == "datetaken") return meta.dateTaken;
                if (k == "camera model" || k == "cameramodel") return meta.cameraModel;
                if (k == "genre") return meta.genre;
                if (k == "year") return meta.year;
                if (k == "track") return meta.track;
                if (k == "duration") return meta.durationStr;
                if (k == "codec") {
                    if (!meta.codec.isEmpty()) return meta.codec;
                    if (!meta.imageFormat.isEmpty()) return meta.imageFormat;
                    return QVariant();
                }
                return QVariant();
            } else if (colDef.type == "Annotation") {
                QString k = colDef.key.toLower();
                if (k == "tags") {
                    QStringList tags = TagManager::instance().getFileTags(filePath);
                    return tags.isEmpty() ? QVariant() : tags.join(", ");
                } else if (k == "rating") {
                    int r = TagManager::instance().getFileRating(filePath);
                    if (r <= 0) return QVariant();
                    return QString(r, QChar(0x2605)) + QString(5 - r, QChar(0x2606));
                } else if (k == "comment") {
                    QString comment = TagManager::instance().getFileComment(filePath);
                    return comment.isEmpty() ? QVariant() : comment;
                }
                return QVariant();
            } else if (colDef.type == "CustomText") {
                QString val = TagManager::instance().getCustomAttribute(filePath, colDef.key);
                return val.isEmpty() ? QVariant() : val;
            } else if (colDef.type == "BuiltIn") {
                if (colDef.key == "Size") return QFileSystemModel::data(index, role);
                if (colDef.key == "Type") return QFileSystemModel::data(index, role);
                if (colDef.key == "Date") return QFileSystemModel::data(index, role);
            }
        } else if (role == Qt::DecorationRole) {
            if (colDef.type == "EmbeddedArtwork") {
                return getEmbeddedArtworkIcon(filePath);
            }
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

QIcon CustomFileSystemModel::getEmbeddedArtworkIcon(const QString& filePath) const {
    static QHash<QString, QIcon> artworkCache;
    if (artworkCache.contains(filePath)) {
        return artworkCache[filePath];
    }
    
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    if (ext == "jpg" || ext == "jpeg" || ext == "png") {
        QPixmap pix(filePath);
        if (!pix.isNull()) {
            QIcon icon(pix.scaled(32, 32, Qt::KeepAspectRatio));
            artworkCache[filePath] = icon;
            return icon;
        }
    }
    
    artworkCache[filePath] = QIcon();
    return QIcon();
}

void CustomFileSystemModel::loadColumnLayout() {
    m_activeColumns.clear();
    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("columns/layout").toString();
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isNull() && doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                CustomColumn col;
                col.name = obj["name"].toString();
                col.type = obj["type"].toString();
                col.key = obj["key"].toString();
                m_activeColumns.append(col);
            }
        }
    }

    if (m_activeColumns.isEmpty()) {
        m_activeColumns = {
            {"Title", "Metadata", "Title"},
            {"Artist", "Metadata", "Artist"},
            {"Album", "Metadata", "Album"},
            {"Genre", "Metadata", "Genre"},
            {"Duration", "Metadata", "Duration"},
            {"Tags", "Annotation", "Tags"},
            {"Rating", "Annotation", "Rating"},
            {"Comment", "Annotation", "Comment"}
        };
    }
}

void CustomFileSystemModel::saveColumnLayout() {
    QJsonArray arr;
    for (const auto& col : m_activeColumns) {
        QJsonObject obj;
        obj["name"] = col.name;
        obj["type"] = col.type;
        obj["key"] = col.key;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("columns/layout", QString(doc.toJson(QJsonDocument::Compact)));
}

void CustomFileSystemModel::setActiveColumns(const QList<CustomColumn>& cols) {
    beginResetModel();
    m_activeColumns = cols;
    saveColumnLayout();
    endResetModel();
}
