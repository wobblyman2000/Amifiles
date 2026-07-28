#include "imagethumbnailrunnable.h"
#include <QImageReader>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

ImageThumbnailRunnable::ImageThumbnailRunnable(const QString& filePath, const QString& cachePath, QObject* model)
    : m_filePath(filePath), m_cachePath(cachePath), m_model(model) {
    setAutoDelete(true);
}

void ImageThumbnailRunnable::run() {
    if (!m_model) return;

    QImageReader reader(m_filePath);
    reader.setAutoTransform(true);

    QSize sz = reader.size();
    if (sz.isValid()) {
        sz.scale(256, 256, Qt::KeepAspectRatio);
        reader.setScaledSize(sz);
    }

    QImage img = reader.read();
    if (!img.isNull()) {
        QFileInfo cacheInfo(m_cachePath);
        QDir().mkpath(cacheInfo.absolutePath());

        if (img.save(m_cachePath, "PNG")) {
            QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                                      Q_ARG(QString, m_filePath), Q_ARG(QImage, img));
            return;
        }
    }

    // Fallback
    QMetaObject::invokeMethod(m_model.data(), "onThumbnailGenerated",
                              Q_ARG(QString, m_filePath), Q_ARG(QImage, QImage()));
}
