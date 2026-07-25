#include "comicthumbnailrunnable.h"
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QRegularExpression>
#include <QMetaObject>
#include <algorithm>

ComicThumbnailRunnable::ComicThumbnailRunnable(const QString& filePath, const QString& cachePath, QObject* model)
    : m_filePath(filePath), m_cachePath(cachePath), m_model(model) {
    setAutoDelete(true);
}

void ComicThumbnailRunnable::run() {
    if (!m_model) return;

    QFileInfo info(m_filePath);
    QString ext = info.suffix().toLower();
    QStringList imagePaths;

    if (ext == "cbz") {
        QProcess proc;
        proc.start("unzip", { "-l", m_filePath });
        if (proc.waitForFinished(5000)) {
            QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QStringList lines = stdoutText.split('\n');
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed.startsWith("Archive:") || trimmed.startsWith("Length") || trimmed.startsWith("---")) {
                    continue;
                }
                QStringList parts = trimmed.split(QRegularExpression("\\s+"));
                if (parts.size() >= 4) {
                    QString name = parts.mid(3).join(' ');
                    QString subExt = QFileInfo(name).suffix().toLower();
                    if (subExt == "jpg" || subExt == "jpeg" || subExt == "png" || subExt == "webp" || subExt == "bmp") {
                        imagePaths.append(name);
                    }
                }
            }
        }
    } else if (ext == "cbr") {
        QProcess proc;
        proc.start("unrar", { "lb", m_filePath });
        if (proc.waitForFinished(5000)) {
            QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QStringList lines = stdoutText.split('\n');
            for (const QString& line : lines) {
                QString name = line.trimmed();
                if (name.isEmpty()) continue;
                QString subExt = QFileInfo(name).suffix().toLower();
                if (subExt == "jpg" || subExt == "jpeg" || subExt == "png" || subExt == "webp" || subExt == "bmp") {
                    imagePaths.append(name);
                }
            }
        }
    }

    if (imagePaths.isEmpty()) {
        QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                                  Q_ARG(QString, m_filePath), Q_ARG(QImage, QImage()));
        return;
    }

    // Sort alphabetically so we get the first page
    std::sort(imagePaths.begin(), imagePaths.end());
    QString targetImage = imagePaths.first();
    QByteArray rawData;

    if (ext == "cbz") {
        QProcess proc;
        proc.start("unzip", { "-p", m_filePath, targetImage });
        if (proc.waitForFinished(5000)) {
            rawData = proc.readAllStandardOutput();
        }
    } else if (ext == "cbr") {
        QProcess proc;
        proc.start("unrar", { "p", "-inul", m_filePath, targetImage });
        if (proc.waitForFinished(5000)) {
            rawData = proc.readAllStandardOutput();
        }
    }

    QImage img;
    if (!rawData.isEmpty() && img.loadFromData(rawData)) {
        QImage thumb = img.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // Ensure cache directory exists
        QFileInfo cacheInfo(m_cachePath);
        QDir().mkpath(cacheInfo.absolutePath());
        
        if (thumb.save(m_cachePath, "PNG")) {
            QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                                      Q_ARG(QString, m_filePath), Q_ARG(QImage, thumb));
            return;
        }
    }

    // Fallback if loading failed
    QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                              Q_ARG(QString, m_filePath), Q_ARG(QImage, QImage()));
}
