#include "metadataextractor.h"
#include <QFileInfo>
#include <QDateTime>
#include <QImageReader>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QTextStream>

FileMetadata MetadataExtractor::extract(const QString& filePath) {
    FileMetadata meta;
    extractBasic(filePath, meta);

    QFileInfo info(filePath);
    if (!info.exists()) {
        return meta;
    }

    QString ext = info.suffix().toLower();
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp" || ext == "webp" || ext == "svg") {
        extractImageInfo(filePath, meta);
    } else if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "flac" || ext == "m4a") {
        extractAudioInfo(filePath, meta);
    }

    return meta;
}

void MetadataExtractor::extractBasic(const QString& filePath, FileMetadata& meta) {
    QFileInfo info(filePath);
    meta.path = QDir::toNativeSeparators(info.absoluteFilePath());
    meta.name = info.fileName();
    meta.extension = info.suffix();
    meta.size = info.size();

    // Permissions string
    QString perms;
    QFile::Permissions p = info.permissions();
    perms += (p & QFileDevice::ReadOwner) ? 'r' : '-';
    perms += (p & QFileDevice::WriteOwner) ? 'w' : '-';
    perms += (p & QFileDevice::ExeOwner) ? 'x' : '-';
    perms += ' ';
    perms += (p & QFileDevice::ReadGroup) ? 'r' : '-';
    perms += (p & QFileDevice::WriteGroup) ? 'w' : '-';
    perms += (p & QFileDevice::ExeGroup) ? 'x' : '-';
    perms += ' ';
    perms += (p & QFileDevice::ReadOther) ? 'r' : '-';
    perms += (p & QFileDevice::WriteOther) ? 'w' : '-';
    perms += (p & QFileDevice::ExeOther) ? 'x' : '-';
    meta.permissions = perms;

    meta.created = info.birthTime().toString("yyyy-MM-dd hh:mm:ss");
    if (meta.created.isEmpty()) {
        meta.created = info.metadataChangeTime().toString("yyyy-MM-dd hh:mm:ss");
    }
    meta.modified = info.lastModified().toString("yyyy-MM-dd hh:mm:ss");
}

void MetadataExtractor::extractImageInfo(const QString& filePath, FileMetadata& meta) {
    QImageReader reader(filePath);
    if (reader.canRead()) {
        meta.imageDimensions = reader.size();
        meta.imageFormat = QString::fromLocal8Bit(reader.format()).toUpper();
    }
}

// Helper to decode ID3v2 text frames
static QString decodeID3v2Text(const QByteArray& data) {
    if (data.size() < 2) return QString();
    char encoding = data.at(0);
    const char* textPtr = data.constData() + 1;
    int textSize = data.size() - 1;

    QString result;
    if (encoding == 0x00) {
        // Latin-1 (ISO-8859-1)
        result = QString::fromLatin1(textPtr, textSize);
    } else if (encoding == 0x01) {
        // UTF-16 with BOM (Byte Order Mark)
        if (textSize >= 2) {
            unsigned char bom1 = textPtr[0];
            unsigned char bom2 = textPtr[1];
            const char16_t* u16ptr = reinterpret_cast<const char16_t*>(textPtr + 2);
            int u16size = (textSize - 2) / 2;
            
            if (bom1 == 0xFF && bom2 == 0xFE) {
                // Little Endian
                result = QString::fromUtf16(u16ptr, u16size);
            } else if (bom1 == 0xFE && bom2 == 0xFF) {
                // Big Endian - need to swap bytes
                QByteArray swapped;
                swapped.resize(u16size * 2);
                for (int i = 0; i < u16size * 2; i += 2) {
                    swapped[i] = textPtr[2 + i + 1];
                    swapped[i + 1] = textPtr[2 + i];
                }
                result = QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), u16size);
            } else {
                // No valid BOM, assume system endianness
                result = QString::fromUtf16(reinterpret_cast<const char16_t*>(textPtr), textSize / 2);
            }
        }
    } else if (encoding == 0x02) {
        // UTF-16BE without BOM
        int u16size = textSize / 2;
        QByteArray swapped;
        swapped.resize(u16size * 2);
        for (int i = 0; i < u16size * 2; i += 2) {
            swapped[i] = textPtr[i + 1];
            swapped[i + 1] = textPtr[i];
        }
        result = QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), u16size);
    } else if (encoding == 0x03) {
        // UTF-8
        result = QString::fromUtf8(textPtr, textSize);
    }

    // Clean trailing nulls or empty spaces
    while (!result.isEmpty() && (result.endsWith('\0') || result.endsWith(' '))) {
        result.chop(1);
    }
    return result.trimmed();
}

void MetadataExtractor::extractAudioInfo(const QString& filePath, FileMetadata& meta) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    // Try parsing ID3v2 (at the beginning of the file)
    char id3Header[10];
    if (file.read(id3Header, 10) == 10) {
        if (strncmp(id3Header, "ID3", 3) == 0) {
            int majorVersion = id3Header[3];
            // Syncsafe integer parsing for tag size (4 bytes, 7 bits used per byte)
            int tagSize = ((id3Header[6] & 0x7F) << 21) |
                          ((id3Header[7] & 0x7F) << 14) |
                          ((id3Header[8] & 0x7F) << 7)  |
                          (id3Header[9] & 0x7F);

            // Read the full ID3v2 tag data
            QByteArray tagData = file.read(tagSize);
            int offset = 0;
            
            while (offset + 10 < tagData.size()) {
                const char* frameHeader = tagData.constData() + offset;
                
                // Validate Frame ID
                bool validFrameId = true;
                for (int i = 0; i < 4; ++i) {
                    char c = frameHeader[i];
                    if ((c < 'A' || c > 'Z') && (c < '0' || c > '9')) {
                        validFrameId = false;
                        break;
                    }
                }
                if (!validFrameId) break;

                QString frameId = QString::fromLatin1(frameHeader, 4);

                // Frame Size (depending on ID3 version, ID3v2.4 uses syncsafe for frame size, ID3v2.3 uses regular int)
                int frameSize = 0;
                if (majorVersion == 4) {
                    frameSize = ((frameHeader[4] & 0x7F) << 21) |
                                ((frameHeader[5] & 0x7F) << 14) |
                                ((frameHeader[6] & 0x7F) << 7)  |
                                (frameHeader[7] & 0x7F);
                } else {
                    frameSize = ((unsigned char)frameHeader[4] << 24) |
                                ((unsigned char)frameHeader[5] << 16) |
                                ((unsigned char)frameHeader[6] << 8)  |
                                (unsigned char)frameHeader[7];
                }

                if (frameSize <= 0 || offset + 10 + frameSize > tagData.size()) {
                    break;
                }

                QByteArray frameData = tagData.mid(offset + 10, frameSize);
                if (frameId == "TIT2") {
                    meta.title = decodeID3v2Text(frameData);
                } else if (frameId == "TPE1") {
                    meta.artist = decodeID3v2Text(frameData);
                } else if (frameId == "TALB") {
                    meta.album = decodeID3v2Text(frameData);
                }

                offset += 10 + frameSize;
            }
        }
    }

    // Try parsing ID3v1 as fallback if tags are empty
    if (meta.title.isEmpty() && meta.artist.isEmpty()) {
        if (file.size() >= 128) {
            file.seek(file.size() - 128);
            QByteArray v1Tag = file.read(128);
            if (v1Tag.startsWith("TAG")) {
                if (meta.title.isEmpty()) {
                    meta.title = QString::fromLatin1(v1Tag.mid(3, 30)).trimmed();
                }
                if (meta.artist.isEmpty()) {
                    meta.artist = QString::fromLatin1(v1Tag.mid(33, 30)).trimmed();
                }
                if (meta.album.isEmpty()) {
                    meta.album = QString::fromLatin1(v1Tag.mid(63, 30)).trimmed();
                }
            }
        }
    }

    file.close();
}
