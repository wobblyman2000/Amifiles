#ifndef METADATAEXTRACTOR_H
#define METADATAEXTRACTOR_H

#include <QString>
#include <QSize>

struct FileMetadata {
    QString path;
    QString name;
    QString extension;
    qint64 size = 0;
    QString permissions;
    QString created;
    QString modified;

    // Image Specific
    QSize imageDimensions;
    QString imageFormat;

    // Audio Specific
    QString title;
    QString artist;
    QString album;
    QString durationStr;
    int bitrate = 0; // kbps
};

class MetadataExtractor {
public:
    static FileMetadata extract(const QString& filePath);

private:
    static void extractBasic(const QString& filePath, FileMetadata& meta);
    static void extractImageInfo(const QString& filePath, FileMetadata& meta);
    static void extractAudioInfo(const QString& filePath, FileMetadata& meta);
};

#endif // METADATAEXTRACTOR_H
