#ifndef VIDEOTHUMBNAILRUNNABLE_H
#define VIDEOTHUMBNAILRUNNABLE_H

#include <QRunnable>
#include <QString>
#include <QObject>
#include <QPointer>
#include <QImage>

class VideoThumbnailRunnable : public QRunnable {
public:
    VideoThumbnailRunnable(const QString& filePath, const QString& cachePath, QObject* model);
    void run() override;

private:
    QString m_filePath;
    QString m_cachePath;
    QPointer<QObject> m_model;
};

#endif // VIDEOTHUMBNAILRUNNABLE_H
