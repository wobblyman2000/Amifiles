#include "customfilesystemmodel.h"
#include "tagmanager.h"
#include "comicthumbnailrunnable.h"
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QPixmap>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThreadPool>
#include <QCryptographicHash>
#include <QDir>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
            QIcon baseIcon = getRetroOrComicIcon(filePath);
            if (baseIcon.isNull()) {
                baseIcon = QFileSystemModel::data(index, role).value<QIcon>();
            }
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
    m_thumbnailCache.clear();
    m_pendingThumbnails.clear();
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

QIcon CustomFileSystemModel::drawRetroDiskIcon(const QString& filename, const QString& ext, int /*size*/) const {
    QIcon icon;
    QList<int> targetSizes = {16, 24, 32, 48, 64, 96, 128};
    for (int sz : targetSizes) {
        QPixmap pix(sz, sz);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);

        double s = sz / 48.0; // scale factor based on 48px baseline

        if (ext == "adf" || ext == "dms") {
            // Amiga 3.5" Disk (Option 1: Realistic 3D Glossy Mockup)
            int pad = qMax(1, qRound(2 * s));
            int w = sz - 2 * pad;
            int h = sz - 2 * pad;

            // 1. Main Disk Body Casing (Dark Charcoal Textured Plastic with authentic cuts)
            QPainterPath bodyPath;
            bodyPath.moveTo(pad, pad + qRound(2 * s)); // top-left start
            bodyPath.lineTo(pad + w - qRound(7 * s), pad); // to top-right cut start
            bodyPath.lineTo(pad + w, pad + qRound(7 * s)); // diagonal cut to right side
            bodyPath.lineTo(pad + w, pad + qRound(16 * s)); // right indentation top
            bodyPath.lineTo(pad + w - qRound(2 * s), pad + qRound(16 * s));
            bodyPath.lineTo(pad + w - qRound(2 * s), pad + qRound(24 * s));
            bodyPath.lineTo(pad + w, pad + qRound(24 * s)); // right indentation bottom
            bodyPath.lineTo(pad + w, pad + h); // bottom-right corner
            bodyPath.lineTo(pad, pad + h); // bottom-left corner
            bodyPath.lineTo(pad, pad + qRound(24 * s)); // left indentation bottom
            bodyPath.lineTo(pad + qRound(2 * s), pad + qRound(24 * s));
            bodyPath.lineTo(pad + qRound(2 * s), pad + qRound(16 * s));
            bodyPath.lineTo(pad, pad + qRound(16 * s)); // left indentation top
            bodyPath.closeSubpath();

            QLinearGradient bodyGrad(pad, pad, pad, pad + h);
            bodyGrad.setColorAt(0.0, QColor("#313244")); // Lighter grey/blue top edge
            bodyGrad.setColorAt(0.5, QColor("#1e1e2e")); // Darker middle
            bodyGrad.setColorAt(1.0, QColor("#111116")); // Deep black/charcoal bottom
            painter.setBrush(bodyGrad);
            painter.setPen(QPen(QColor("#0b0b10"), qMax(1.0, 0.5 * s)));
            painter.drawPath(bodyPath);

            // Subtle 3D inner bevel highlight along the path
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 25), qMax(1.0, 0.5 * s)));
            painter.drawPath(bodyPath);

            // Write-protect cutout at the bottom right
            painter.setBrush(QColor("#0b0b10"));
            painter.setPen(Qt::NoPen);
            painter.drawRect(pad + w - qRound(7 * s), pad + h - qRound(7 * s), qRound(5 * s), qRound(5 * s));
            // Red slider inside write-protect hole
            painter.setBrush(QColor("#f38ba8")); // Red plastic
            painter.drawRect(pad + w - qRound(6 * s), pad + h - qRound(5 * s), qRound(3 * s), qRound(2 * s));

            // 2. Metal sliding shutter (top-center/left)
            // Silver brushed metal gradient
            QLinearGradient metalGrad(pad + qRound(11 * s), pad, pad + qRound(25 * s), pad);
            metalGrad.setColorAt(0.0, QColor("#89b4fa")); // Tinted blue-silver
            metalGrad.setColorAt(0.3, QColor("#b4befe"));
            metalGrad.setColorAt(0.5, QColor("#a6adc8"));
            metalGrad.setColorAt(0.7, QColor("#cdd6f4"));
            metalGrad.setColorAt(1.0, QColor("#6c7086"));
            painter.setBrush(metalGrad);
            painter.setPen(QPen(QColor("#181825"), qMax(1.0, 0.5 * s)));
            painter.drawRoundedRect(pad + qRound(11 * s), pad, qRound(14 * s), qRound(15 * s), 0.5 * s, 0.5 * s);

            // Shutter slot cutout (black vertical slot)
            painter.setBrush(QColor("#11111b"));
            painter.setPen(Qt::NoPen);
            painter.drawRect(pad + qRound(15 * s), pad + qRound(2 * s), qRound(3 * s), qRound(11 * s));

            // 3. Recessed Label Pocket (white/cream paper sticker)
            painter.setBrush(QColor("#11111b"));
            painter.drawRoundedRect(pad + qRound(4 * s), pad + qRound(17 * s), w - qRound(8 * s), h - qRound(19 * s), 1 * s, 1 * s);

            // Paper label
            QLinearGradient labelGrad(pad + qRound(4 * s), pad + qRound(18 * s), pad + qRound(4 * s), pad + h - 1);
            labelGrad.setColorAt(0.0, QColor("#ffffff"));
            labelGrad.setColorAt(1.0, QColor("#e6e9ef"));
            painter.setBrush(labelGrad);
            painter.setPen(QPen(QColor("#313244"), qMax(1.0, 0.5 * s)));
            painter.drawRoundedRect(pad + qRound(4 * s), pad + qRound(18 * s), w - qRound(8 * s), h - qRound(20 * s), 1 * s, 1 * s);

            // Write filename (wrapped, black color, bold font, dynamically sized)
            int fontSize = qMax(4, qRound(5.0 * s));
            if (filename.length() > 12) fontSize = qMax(4, qRound(4.2 * s));
            if (filename.length() > 20) fontSize = qMax(4, qRound(3.5 * s));
            if (filename.length() > 30) fontSize = qMax(3, qRound(2.8 * s));

            QFont f("Outfit", fontSize);
            f.setBold(true);
            painter.setFont(f);
            
            painter.setPen(QColor("#11111b"));
            QRectF textRect(pad + qRound(6 * s), pad + qRound(20 * s), w - qRound(22 * s), h - qRound(28 * s));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, filename);

            // Amiga Rainbow checkmark in the bottom right corner (restored)
            QLinearGradient rainbowGrad(pad + w - qRound(16 * s), pad + h - qRound(10 * s), pad + w - qRound(6 * s), pad + h - qRound(4 * s));
            rainbowGrad.setColorAt(0.0, QColor("#e64553")); // Red
            rainbowGrad.setColorAt(0.25, QColor("#fe640b")); // Orange
            rainbowGrad.setColorAt(0.5, QColor("#df8e1d")); // Yellow
            rainbowGrad.setColorAt(0.75, QColor("#40a02b")); // Green
            rainbowGrad.setColorAt(1.0, QColor("#1e66f5")); // Blue

            QPen rainbowPen(rainbowGrad, qMax(1.5, 1.5 * s));
            rainbowPen.setCapStyle(Qt::RoundCap);
            rainbowPen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(rainbowPen);
            painter.setBrush(Qt::NoBrush);

            QPolygonF checkPoly;
            checkPoly << QPointF(pad + w - qRound(15 * s), pad + h - qRound(10 * s))
                      << QPointF(pad + w - qRound(11 * s), pad + h - qRound(5 * s))
                      << QPointF(pad + w - qRound(6 * s), pad + h - qRound(12 * s));
            painter.drawPolyline(checkPoly);

            // Mock bottom subtext ("A500 | CSI | SYNC") in small/light font
            painter.setPen(QColor("#585b70")); // medium grey
            QFont subFont("Outfit", qMax(3, qRound(3.2 * s)));
            painter.setFont(subFont);
            painter.drawText(QRectF(pad + qRound(6 * s), pad + h - qRound(7 * s), w - qRound(25 * s), qRound(5 * s)), Qt::AlignLeft | Qt::AlignBottom, "A500 | C.S.I. | SYNC");

            // 5. Diagonal Gloss Overlay (3D Shine) - linear gradient across top-left
            QLinearGradient shineGrad(0, 0, sz, sz / 2);
            shineGrad.setColorAt(0.0, QColor(255, 255, 255, 60));
            shineGrad.setColorAt(0.5, QColor(255, 255, 255, 0));
            painter.setBrush(shineGrad);
            painter.setPen(Qt::NoPen);
            painter.drawRect(0, 0, sz, sz);
        } else if (ext == "d64" || ext == "g64") {
            // C64 5.25" Disk
            QColor casingColor("#11111b"); // black floppy
            painter.setBrush(casingColor);
            painter.setPen(QPen(QColor("#313244"), qMax(1.0, 0.5 * s)));
            int pad = qMax(1, qRound(2 * s));
            int w = sz - 2 * pad;
            int h = sz - 2 * pad;
            painter.drawRect(pad, pad, w, h);

            // Write protect notch
            painter.setBrush(QColor("#1e1e2e"));
            painter.drawRect(pad + w - qRound(3 * s), pad + qRound(10 * s), qRound(4 * s), qRound(6 * s));

            // Center hub ring & hole
            int cx = sz / 2;
            int cy = sz / 2;
            painter.setBrush(QColor("#313244"));
            painter.drawEllipse(cx - qRound(7 * s), cy - qRound(7 * s), qRound(14 * s), qRound(14 * s));
            painter.setBrush(QColor("#1e1e2e"));
            painter.drawEllipse(cx - qRound(4 * s), cy - qRound(4 * s), qRound(8 * s), qRound(8 * s));

            // C64 label at the top
            painter.setBrush(QColor("#fab387")); // orange retro label
            painter.drawRect(pad + qRound(4 * s), pad + qRound(2 * s), w - qRound(8 * s), qRound(12 * s));

            // Text
            painter.setPen(QColor("#11111b"));
            QFont f("Outfit", qMax(5, qRound(5.0 * s)));
            f.setBold(true);
            painter.setFont(f);
            QRect textRect(pad + qRound(5 * s), pad + qRound(2 * s), w - qRound(10 * s), qRound(12 * s));
            painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, filename);

        } else if (ext == "t64" || ext == "tap") {
            // C64 Cassette Tape
            painter.setBrush(QColor("#313244")); // dark grey casing
            painter.setPen(QPen(QColor("#45475a"), qMax(1.0, 0.5 * s)));
            int padX = qMax(1, qRound(2 * s));
            int padY = qMax(1, qRound(6 * s));
            int w = sz - 2 * padX;
            int h = sz - 2 * padY;
            painter.drawRoundedRect(padX, padY, w, h, 2 * s, 2 * s);

            // Label strip
            painter.setBrush(QColor("#cdd6f4")); // white label
            painter.drawRect(padX + qRound(4 * s), padY + qRound(4 * s), w - qRound(8 * s), qRound(12 * s));

            // Spindle holes
            painter.setBrush(QColor("#1e1e2e"));
            painter.drawEllipse(padX + qRound(10 * s), padY + qRound(20 * s), qRound(6 * s), qRound(6 * s));
            painter.drawEllipse(padX + w - qRound(16 * s), padY + qRound(20 * s), qRound(6 * s), qRound(6 * s));

            // Text on label
            painter.setPen(QColor("#11111b"));
            QFont f("Outfit", qMax(5, qRound(5.0 * s)));
            f.setBold(true);
            painter.setFont(f);
            QRect textRect(padX + qRound(5 * s), padY + qRound(4 * s), w - qRound(10 * s), qRound(12 * s));
            painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, filename);

        } else if (ext == "crt") {
            // C64 Cartridge
            painter.setBrush(QColor("#585b70")); // C64 grey-brown cartridge
            painter.setPen(QPen(QColor("#313244"), qMax(1.0, 0.5 * s)));
            int padX = qMax(1, qRound(6 * s));
            int padY = qMax(1, qRound(2 * s));
            int w = sz - 2 * padX;
            int h = sz - 2 * padY;
            painter.drawRoundedRect(padX, padY, w, h, 3 * s, 3 * s);

            // Cartridge label
            painter.setBrush(QColor("#f9e2af")); // yellow retro label
            painter.drawRect(padX + qRound(4 * s), padY + qRound(6 * s), w - qRound(8 * s), h - qRound(12 * s));

            // Text
            painter.setPen(QColor("#11111b"));
            QFont f("Outfit", qMax(5, qRound(5.0 * s)));
            f.setBold(true);
            painter.setFont(f);
            QRect textRect(padX + qRound(5 * s), padY + qRound(6 * s), w - qRound(10 * s), h - qRound(12 * s));
            painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, filename);
        }

        painter.end();
        icon.addPixmap(pix);
    }
    return icon;
}

QIcon CustomFileSystemModel::getRetroOrComicIcon(const QString& filePath) const {
    if (m_thumbnailCache.contains(filePath)) {
        return m_thumbnailCache[filePath];
    }

    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    QString filename = info.baseName();

    if (ext == "adf" || ext == "dms" || ext == "d64" || ext == "g64" || ext == "t64" || ext == "tap" || ext == "crt") {
        QIcon icon = drawRetroDiskIcon(filename, ext, 48);
        m_thumbnailCache[filePath] = icon;
        return icon;
    }

    if (ext == "cbz" || ext == "cbr") {
        QString appDir = "/home/dave/.gemini/antigravity";
        QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Md5);
        QString cachedName = hash.toHex() + ".png";
        QString cachedPath = QDir(appDir).filePath("thumbnails/" + cachedName);

        if (QFile::exists(cachedPath)) {
            QPixmap pix(cachedPath);
            if (!pix.isNull()) {
                QIcon icon(pix);
                m_thumbnailCache[filePath] = icon;
                return icon;
            }
        }

        if (!m_pendingThumbnails.contains(filePath)) {
            m_pendingThumbnails.insert(filePath);
            ComicThumbnailRunnable* task = new ComicThumbnailRunnable(filePath, cachedPath, const_cast<CustomFileSystemModel*>(this));
            QThreadPool::globalInstance()->start(task);
        }

        QIcon placeholder;
        QList<int> targetSizes = {16, 24, 32, 48, 64, 96, 128};
        for (int sz : targetSizes) {
            QPixmap pix(sz, sz);
            pix.fill(Qt::transparent);
            QPainter painter(&pix);
            painter.setRenderHint(QPainter::Antialiasing);
            double s = sz / 48.0;

            painter.setBrush(QColor("#f38ba8")); // rose red book cover
            painter.setPen(QPen(QColor("#11111b"), qMax(1.0, 0.5 * s)));
            painter.drawRect(qMax(1, qRound(4 * s)), qMax(1, qRound(2 * s)), sz - qMax(2, qRound(8 * s)), sz - qMax(2, qRound(4 * s)));

            painter.setBrush(QColor("#f9e2af")); // yellow burst
            QPolygonF star;
            double cx = sz / 2.0;
            double cy = sz / 2.0;
            double r1 = 12.0 * s;
            double r2 = 5.0 * s;
            for (int i = 0; i < 16; ++i) {
                double angle = i * M_PI / 8.0;
                double r = (i % 2 == 0) ? r1 : r2;
                star << QPointF(cx + r * cos(angle), cy + r * sin(angle));
            }
            painter.drawPolygon(star);

            painter.setPen(QColor("#11111b"));
            QFont f("Outfit", qMax(4, qRound(4.5 * s)));
            f.setBold(true);
            painter.setFont(f);
            painter.drawText(QRect(0, 0, sz, sz), Qt::AlignCenter, "COMIC");

            painter.end();
            placeholder.addPixmap(pix);
        }
        return placeholder;
    }

    return QIcon();
}

void CustomFileSystemModel::onThumbnailGenerated(const QString& filePath, const QImage& img) {
    m_pendingThumbnails.remove(filePath);
    if (!img.isNull()) {
        QIcon icon(QPixmap::fromImage(img));
        m_thumbnailCache[filePath] = icon;
    }

    QModelIndex idx = index(filePath);
    if (idx.isValid()) {
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
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
