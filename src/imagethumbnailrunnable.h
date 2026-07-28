#ifndef IMAGETHUMBNAILRUNNABLE_H
#define IMAGETHUMBNAILRUNNABLE_H

#include <QRunnable>
#include <QString>
#include <QPointer>
#include <QObject>

class ImageThumbnailRunnable : public QRunnable {
public:
    ImageThumbnailRunnable(const QString& filePath, const QString& cachePath, QObject* model);
    void run() override;

private:
    QString m_filePath;
    QString m_cachePath;
    QPointer<QObject> m_model;
};

#endif // IMAGETHUMBNAILRUNNABLE_H
