#ifndef AUDIOCOVERARTRUNNABLE_H
#define AUDIOCOVERARTRUNNABLE_H

#include <QRunnable>
#include <QString>
#include <QObject>

class AudioCoverArtRunnable : public QRunnable {
public:
    AudioCoverArtRunnable(const QString& filePath, const QString& cachePath, QObject* model);
    void run() override;

private:
    QString m_filePath;
    QString m_cachePath;
    QObject* m_model;
};

#endif // AUDIOCOVERARTRUNNABLE_H
