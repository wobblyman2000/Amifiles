#ifndef COMICTHUMBNAILRUNNABLE_H
#define COMICTHUMBNAILRUNNABLE_H

#include <QRunnable>
#include <QString>
#include <QPointer>
#include <QObject>

class ComicThumbnailRunnable : public QRunnable {
public:
    ComicThumbnailRunnable(const QString& filePath, const QString& cachePath, QObject* model);
    void run() override;

private:
    QString m_filePath;
    QString m_cachePath;
    QPointer<QObject> m_model;
};

#endif // COMICTHUMBNAILRUNNABLE_H
