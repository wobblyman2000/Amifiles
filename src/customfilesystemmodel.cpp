#include "customfilesystemmodel.h"
#include "tagmanager.h"
#include "comicthumbnailrunnable.h"
#include "imagethumbnailrunnable.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
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

static bool isRemotePath(const QString& path) {
    if (path.startsWith("/run/user/") && path.contains("/gvfs/")) {
        return true;
    }
    QString home = QDir::homePath();
    if (path.startsWith(home + "/CloudMounts/")) {
        return true;
    }
    return false;
}

class ChecksumRunnable : public QRunnable {
public:
    ChecksumRunnable(const QString& filePath, CustomFileSystemModel* model)
        : m_filePath(filePath), m_model(model) {
        setAutoDelete(true);
    }
    
    void run() override {
        QFile file(m_filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(m_model, "onChecksumGenerated",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, m_filePath),
                                      Q_ARG(QString, "Error"));
            return;
        }
        
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (hash.addData(&file)) {
            QString result = hash.result().toHex().toUpper();
            QMetaObject::invokeMethod(m_model, "onChecksumGenerated",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, m_filePath),
                                      Q_ARG(QString, result));
        } else {
            QMetaObject::invokeMethod(m_model, "onChecksumGenerated",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, m_filePath),
                                      Q_ARG(QString, "Error"));
        }
    }
private:
    QString m_filePath;
    CustomFileSystemModel* m_model;
};

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
                if (isRemotePath(filePath)) {
                    if (fileInfo(index).isDir()) {
                        baseIcon = QIcon::fromTheme("folder");
                    } else {
                        baseIcon = QIcon::fromTheme("text-x-generic");
                    }
                } else {
                    baseIcon = QFileSystemModel::data(index, role).value<QIcon>();
                }
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
                if (k == "permissions" || k == "perms") return meta.permissions;
                if (k == "checksum" || k == "md5" || k == "md5 checksum") {
                    if (fileInfo(index).isDir()) return QVariant();
                    if (m_checksumCache.contains(filePath)) {
                        return m_checksumCache[filePath];
                    }
                    if (isRemotePath(filePath)) {
                        return "N/A";
                    }
                    if (!m_pendingChecksums.contains(filePath)) {
                        m_pendingChecksums.insert(filePath);
                        ChecksumRunnable* runnable = new ChecksumRunnable(filePath, const_cast<CustomFileSystemModel*>(this));
                        QThreadPool::globalInstance()->start(runnable);
                    }
                    return "Calculating...";
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
                if (colDef.key == "Permissions") {
                    FileMetadata meta = getMetadata(filePath);
                    return meta.permissions;
                }
                if (colDef.key == "Checksum" || colDef.key == "MD5" || colDef.key == "MD5 Checksum") {
                    if (fileInfo(index).isDir()) return QVariant();
                    if (m_checksumCache.contains(filePath)) {
                        return m_checksumCache[filePath];
                    }
                    if (isRemotePath(filePath)) {
                        return "N/A";
                    }
                    if (!m_pendingChecksums.contains(filePath)) {
                        m_pendingChecksums.insert(filePath);
                        ChecksumRunnable* runnable = new ChecksumRunnable(filePath, const_cast<CustomFileSystemModel*>(this));
                        QThreadPool::globalInstance()->start(runnable);
                    }
                    return "Calculating...";
                }
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
    if (isRemotePath(filePath)) {
        FileMetadata emptyMeta;
        m_metadataCache.insert(filePath, emptyMeta);
        return emptyMeta;
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
    
    if (isRemotePath(filePath)) {
        artworkCache[filePath] = QIcon();
        return QIcon();
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

        if (ext == "adf" || ext == "dms" || ext == "adz") {
            // Amiga 3.5" Disk (Option 1: Realistic 3D Glossy Mockup)
            int pad = qMax(1, qRound(2 * s));
            int w = sz - 2 * pad;
            int h = sz - 2 * pad;

            // Corner radius
            double cr = 2.0 * s;

            // 1. Main Disk Body Casing (Sleek straight edges, diagonal top-right cut, rounded bottom)
            QPainterPath bodyPath;
            bodyPath.moveTo(pad + cr, pad);
            bodyPath.lineTo(pad + w - qRound(8 * s), pad); // Top edge to diagonal cut start
            bodyPath.lineTo(pad + w, pad + qRound(8 * s)); // Diagonal top-right cut
            bodyPath.lineTo(pad + w, pad + h - cr); // Right edge
            bodyPath.arcTo(QRectF(pad + w - 2*cr, pad + h - 2*cr, 2*cr, 2*cr), 0, -90); // Bottom-right rounded corner
            bodyPath.lineTo(pad + cr, pad + h); // Bottom edge
            bodyPath.arcTo(QRectF(pad, pad + h - 2*cr, 2*cr, 2*cr), 270, -90); // Bottom-left rounded corner
            bodyPath.lineTo(pad, pad + cr); // Left edge
            bodyPath.arcTo(QRectF(pad, pad, 2*cr, 2*cr), 180, -90); // Top-left rounded corner
            bodyPath.closeSubpath();

            // Casing Gradient (Glossy charcoal plastic)
            QLinearGradient bodyGrad(pad, pad, pad, pad + h);
            bodyGrad.setColorAt(0.0, QColor("#303035")); // Sleek dark charcoal/blue grey
            bodyGrad.setColorAt(0.5, QColor("#1a1a1f"));
            bodyGrad.setColorAt(1.0, QColor("#0c0c0e"));
            painter.setBrush(bodyGrad);
            painter.setPen(QPen(QColor("#060608"), qMax(1.0, 0.5 * s)));
            painter.drawPath(bodyPath);

            // Subtle inner light highlight
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 30), qMax(1.0, 0.5 * s)));
            painter.drawPath(bodyPath);

            // 2. Embossed Arrow (pointing up, top-left)
            painter.save();
            QPen arrowPen(QColor(255, 255, 255, 25), qMax(1.0, 0.5 * s));
            arrowPen.setCapStyle(Qt::RoundCap);
            painter.setPen(arrowPen);
            painter.drawLine(QPointF(pad + 6 * s, pad + 6 * s), QPointF(pad + 6 * s, pad + 11 * s));
            painter.drawLine(QPointF(pad + 6 * s, pad + 6 * s), QPointF(pad + 4.5 * s, pad + 8 * s));
            painter.drawLine(QPointF(pad + 6 * s, pad + 6 * s), QPointF(pad + 7.5 * s, pad + 8 * s));

            painter.setPen(QPen(QColor(0, 0, 0, 100), qMax(1.0, 0.5 * s)));
            painter.drawLine(QPointF(pad + 6.5 * s, pad + 6.5 * s), QPointF(pad + 6.5 * s, pad + 11.5 * s));
            painter.restore();

            // 3. Write-protect holes (Bottom-left and Bottom-right)
            int holeSize = qRound(4 * s);
            int holeY = pad + h - qRound(7 * s);
            
            // Bottom-left hole
            painter.setBrush(QColor("#050507"));
            painter.setPen(QPen(QColor(255, 255, 255, 15), qMax(1.0, 0.5 * s)));
            painter.drawRect(pad + qRound(4 * s), holeY, holeSize, holeSize);

            // Bottom-right write-protect hole with red write protect slider
            painter.drawRect(pad + w - qRound(8 * s), holeY, holeSize, holeSize);
            painter.setBrush(QColor("#e64553")); // Red slider plastic
            painter.setPen(Qt::NoPen);
            painter.drawRect(pad + w - qRound(8 * s), holeY + qRound(2 * s), holeSize, qRound(2 * s));

            // 4. Metal sliding shutter (top-center/left)
            // Silver brushed metal gradient
            int shutterX = pad + qRound(11 * s);
            int shutterY = pad;
            int shutterW = qRound(16 * s);
            int shutterH = qRound(15 * s);

            QLinearGradient metalGrad(shutterX, shutterY, shutterX + shutterW, shutterY);
            metalGrad.setColorAt(0.0, QColor("#7ea7e9")); // Brushed blue-steel highlight
            metalGrad.setColorAt(0.2, QColor("#a6adc8"));
            metalGrad.setColorAt(0.4, QColor("#e6edf7")); // Strong highlight reflection
            metalGrad.setColorAt(0.6, QColor("#bac2de"));
            metalGrad.setColorAt(0.8, QColor("#e2e8f0"));
            metalGrad.setColorAt(1.0, QColor("#585b70"));
            painter.setBrush(metalGrad);
            painter.setPen(QPen(QColor("#08080a"), qMax(1.0, 0.5 * s)));
            painter.drawRoundedRect(QRectF(shutterX, shutterY, shutterW, shutterH), 0.5 * s, 0.5 * s);

            // Shutter slot cutout
            painter.setBrush(QColor("#0c0c0e"));
            painter.setPen(Qt::NoPen);
            painter.drawRect(shutterX + qRound(4 * s), shutterY + qRound(2.5 * s), qRound(3.5 * s), qRound(10 * s));

            // 5. Recessed Label Pocket
            int pocketX = pad + qRound(4 * s);
            int pocketY = pad + qRound(18 * s);
            int pocketW = w - qRound(8 * s);
            int pocketH = h - qRound(20 * s);

            painter.setBrush(QColor("#08080a"));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRectF(pocketX - 0.5 * s, pocketY - 0.5 * s, pocketW + 1 * s, pocketH + 1 * s), 1 * s, 1 * s);

            // Paper label background
            QLinearGradient labelGrad(pocketX, pocketY, pocketX, pocketY + pocketH);
            labelGrad.setColorAt(0.0, QColor("#ffffff"));
            labelGrad.setColorAt(1.0, QColor("#f1f3f9"));
            painter.setBrush(labelGrad);
            painter.setPen(QPen(QColor("#313244"), qMax(1.0, 0.5 * s)));
            painter.drawRoundedRect(QRectF(pocketX, pocketY, pocketW, pocketH), 1 * s, 1 * s);

            // 6. Label Text Layout (AMIGA DISK header + Filename body)
            // A. Header: "AMIGA DISK"
            QFont headerFont("Arial");
            headerFont.setPixelSize(qMax(4.5, 4.5 * s));
            headerFont.setBold(true);
            painter.setFont(headerFont);
            painter.setPen(QColor("#1e1e2e"));
            painter.drawText(QRectF(pocketX + 3 * s, pocketY + 2 * s, pocketW - 6 * s, 6 * s), Qt::AlignLeft | Qt::AlignVCenter, "AMIGA DISK");

            // Blue bottom border line under "AMIGA DISK"
            painter.setPen(QPen(QColor("#1e66f5"), qMax(1.0, 0.5 * s)));
            painter.drawLine(QPointF(pocketX + 3 * s, pocketY + 7.5 * s), QPointF(pocketX + pocketW - 3 * s, pocketY + 7.5 * s));

            // B. Write filename (Clean sans-serif typography, dynamically scaled to fit)
            QString displayName = filename;
            displayName.replace('_', ' ').replace('-', ' ');

            double fontSize = 5.0 * s;
            QFont f("Arial");
            f.setPointSizeF(fontSize);
            f.setBold(true);

            QRectF textRect(pocketX + 3 * s, pocketY + 9 * s, pocketW - 14 * s, pocketH - 14 * s);

            // Reduce font size progressively to fit inside bounds
            while (fontSize > 1.0) {
                f.setPointSizeF(fontSize);
                QFontMetricsF fm(f);
                QRectF bounds = fm.boundingRect(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere, displayName);
                if (bounds.height() <= textRect.height() && bounds.width() <= textRect.width()) {
                    break;
                }
                fontSize -= 0.25;
            }

            painter.setFont(f);
            painter.setPen(QColor("#11111b"));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere, displayName);

            // 7. Amiga Rainbow checkmark in the bottom right corner (large & high contrast)
            int checkX = pocketX + pocketW - qRound(11 * s);
            int checkY = pocketY + pocketH - qRound(10 * s);

            QLinearGradient rainbowGrad(checkX, checkY + 5 * s, checkX + 8 * s, checkY);
            rainbowGrad.setColorAt(0.0, QColor("#e64553")); // Red
            rainbowGrad.setColorAt(0.25, QColor("#fe640b")); // Orange
            rainbowGrad.setColorAt(0.5, QColor("#df8e1d")); // Yellow
            rainbowGrad.setColorAt(0.75, QColor("#40a02b")); // Green
            rainbowGrad.setColorAt(1.0, QColor("#1e66f5")); // Blue

            QPen rainbowPen(rainbowGrad, qMax(1.8, 1.8 * s));
            rainbowPen.setCapStyle(Qt::RoundCap);
            rainbowPen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(rainbowPen);
            painter.setBrush(Qt::NoBrush);

            QPolygonF checkPoly;
            checkPoly << QPointF(checkX - 1 * s, checkY + 3 * s)
                      << QPointF(checkX + 2 * s, checkY + 7 * s)
                      << QPointF(checkX + 7 * s, checkY + 1 * s);
            painter.drawPolyline(checkPoly);

            // 8. Mock bottom subtext ("A500 | C.S.I. | SYNC") in small/light font
            painter.setPen(QColor("#585b70"));
            QFont subFont("Outfit", qMax(3, qRound(3.0 * s)));
            painter.setFont(subFont);
            painter.drawText(QRectF(pocketX + 3 * s, pocketY + pocketH - qRound(4 * s), pocketW - 6 * s, qRound(4 * s)), Qt::AlignLeft | Qt::AlignVCenter, "A500 | C.S.I. | SYNC");

            // 9. Diagonal Gloss Reflection Overlay (Glossy 3D finish)
            painter.save();
            painter.setClipPath(bodyPath);
            QLinearGradient shineGrad(pad, pad, pad + w, pad + h);
            shineGrad.setColorAt(0.0, QColor(255, 255, 255, 75));   // Strong bright shine
            shineGrad.setColorAt(0.18, QColor(255, 255, 255, 90));
            shineGrad.setColorAt(0.22, QColor(255, 255, 255, 0));   // Sharp gloss cutoff
            shineGrad.setColorAt(0.50, QColor(255, 255, 255, 0));
            shineGrad.setColorAt(0.60, QColor(255, 255, 255, 12));  // Soft secondary reflection
            shineGrad.setColorAt(0.70, QColor(255, 255, 255, 0));
            painter.setBrush(shineGrad);
            painter.setPen(Qt::NoPen);
            painter.drawRect(0, 0, sz, sz);
            painter.restore();
        } else if (ext == "d64" || ext == "g64" || ext == "d71" || ext == "d81") {
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

    if (ext == "adf" || ext == "dms" || ext == "adz" || ext == "d64" || ext == "g64" || ext == "d71" || ext == "d81" || ext == "t64" || ext == "tap" || ext == "crt") {
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

        if (!isRemotePath(filePath)) {
            if (!m_pendingThumbnails.contains(filePath)) {
                m_pendingThumbnails.insert(filePath);
                ComicThumbnailRunnable* task = new ComicThumbnailRunnable(filePath, cachedPath, const_cast<CustomFileSystemModel*>(this));
                QThreadPool::globalInstance()->start(task);
            }
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
    
    static const QStringList imageExts = { "jpg", "jpeg", "png", "gif", "bmp", "webp", "tga", "tiff", "ico" };
    if (imageExts.contains(ext)) {
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

        if (!isRemotePath(filePath)) {
            if (!m_pendingThumbnails.contains(filePath)) {
                m_pendingThumbnails.insert(filePath);
                ImageThumbnailRunnable* task = new ImageThumbnailRunnable(filePath, cachedPath, const_cast<CustomFileSystemModel*>(this));
                QThreadPool::globalInstance()->start(task);
            }
        }

        return QIcon();
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

void CustomFileSystemModel::onChecksumGenerated(const QString& filePath, const QString& md5) {
    m_pendingChecksums.remove(filePath);
    m_checksumCache[filePath] = md5;

    QModelIndex idx = index(filePath);
    if (idx.isValid()) {
        QModelIndex leftIdx = index(idx.row(), 0, idx.parent());
        QModelIndex rightIdx = index(idx.row(), columnCount() - 1, idx.parent());
        emit dataChanged(leftIdx, rightIdx, {Qt::DisplayRole});
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
