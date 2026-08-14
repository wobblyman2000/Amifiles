#include "videothumbnailrunnable.h"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QMetaObject>
#include <QDebug>

VideoThumbnailRunnable::VideoThumbnailRunnable(const QString& filePath, const QString& cachePath, QObject* model)
    : m_filePath(filePath), m_cachePath(cachePath), m_model(model) {
    setAutoDelete(true);
}

void VideoThumbnailRunnable::run() {
    if (m_filePath.isEmpty() || m_cachePath.isEmpty()) return;

    // Ensure cache directory exists
    QFileInfo cacheFi(m_cachePath);
    QDir().mkpath(cacheFi.absolutePath());

    bool generated = false;

    // Method 1: Try ffmpegthumbnailer
    {
        QProcess proc;
        proc.start("ffmpegthumbnailer", QStringList() << "-i" << m_filePath << "-o" << m_cachePath << "-s" << "256");
        if (proc.waitForFinished(3000) && proc.exitCode() == 0 && QFile::exists(m_cachePath)) {
            generated = true;
        }
    }

    // Method 2: Fallback to ffmpeg
    if (!generated) {
        QProcess proc;
        proc.start("ffmpeg", QStringList() << "-ss" << "00:00:03" << "-i" << m_filePath << "-vframes" << "1" << "-s" << "256x256" << "-y" << m_cachePath);
        if (proc.waitForFinished(4000) && proc.exitCode() == 0 && QFile::exists(m_cachePath)) {
            generated = true;
        }
    }

    if (generated && QFile::exists(m_cachePath)) {
        QImage img(m_cachePath);
        if (!img.isNull()) {
            // Overlay subtle play icon badge on bottom-right corner
            QImage badgedImg = img.copy();
            QPainter painter(&badgedImg);
            painter.setRenderHint(QPainter::Antialiasing);

            int w = badgedImg.width();
            int h = badgedImg.height();
            int badgeSize = w / 5;
            if (badgeSize < 24) badgeSize = 24;

            int margin = 8;
            QRect badgeRect(w - badgeSize - margin, h - badgeSize - margin, badgeSize, badgeSize);

            // Dark semi-transparent circle background
            painter.setBrush(QColor(0, 0, 0, 160));
            painter.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
            painter.drawEllipse(badgeRect);

            // White play triangle
            QPolygon playTriangle;
            int cx = badgeRect.center().x();
            int cy = badgeRect.center().y();
            int r = badgeSize / 3;
            playTriangle << QPoint(cx - r/2 + 2, cy - r)
                         << QPoint(cx + r, cy)
                         << QPoint(cx - r/2 + 2, cy + r);

            painter.setBrush(QColor(255, 255, 255, 230));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(playTriangle);
            painter.end();

            badgedImg.save(m_cachePath, "PNG");

            if (m_model) {
                QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, m_filePath),
                                          Q_ARG(QImage, badgedImg));
            }
            return;
        }
    }

    // Fallback: Notify completion with empty image so model removes from pending
    if (m_model) {
        QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, m_filePath),
                                  Q_ARG(QImage, QImage()));
    }
}
