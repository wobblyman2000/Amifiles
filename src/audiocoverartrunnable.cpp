#include "audiocoverartrunnable.h"
#include <QProcess>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QMetaObject>

AudioCoverArtRunnable::AudioCoverArtRunnable(const QString& filePath, const QString& cachePath, QObject* model)
    : m_filePath(filePath), m_cachePath(cachePath), m_model(model) {
    setAutoDelete(true);
}

void AudioCoverArtRunnable::run() {
    QImage coverImg;

    // 1. Try extracting embedded picture using exiftool
    QProcess proc;
    proc.start("exiftool", {"-Picture", "-b", m_filePath});
    if (proc.waitForFinished(1500)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            coverImg.loadFromData(imgData);
        }
    }

    // 2. If exiftool failed or didn't find artwork, try ffmpeg
    if (coverImg.isNull()) {
        QProcess ffmpegProc;
        ffmpegProc.start("ffmpeg", {"-i", m_filePath, "-an", "-vcodec", "copy", "-f", "image2pipe", "-v", "error", "-"});
        if (ffmpegProc.waitForFinished(1500)) {
            QByteArray imgData = ffmpegProc.readAllStandardOutput();
            if (!imgData.isEmpty()) {
                coverImg.loadFromData(imgData);
            }
        }
    }

    // 3. If no embedded cover art found, search directory for folder/cover/album art files
    if (coverImg.isNull()) {
        QString dirPath = QFileInfo(m_filePath).absolutePath();
        QDir dir(dirPath);
        QStringList artNames = { "folder", "cover", "album", "poster", "front" };
        QStringList artExts = { "jpg", "jpeg", "png", "webp" };

        for (const QString& name : artNames) {
            for (const QString& ext : artExts) {
                QString path = dir.filePath(name + "." + ext);
                if (QFile::exists(path)) {
                    QImage p(path);
                    if (!p.isNull()) {
                        coverImg = p;
                        break;
                    }
                }
            }
            if (!coverImg.isNull()) break;
        }
    }

    // 4. Save thumbnail cache if image loaded successfully
    if (!coverImg.isNull()) {
        QImage scaled = coverImg.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QDir().mkpath(QFileInfo(m_cachePath).absolutePath());
        scaled.save(m_cachePath, "PNG");

        if (m_model) {
            QMetaObject::invokeMethod(m_model, "onThumbnailGenerated",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, m_filePath),
                                      Q_ARG(QImage, scaled));
        }
    } else {
        if (m_model) {
            QMetaObject::invokeMethod(m_model, "onThumbnailGenerated",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, m_filePath),
                                      Q_ARG(QImage, QImage()));
        }
    }
}
