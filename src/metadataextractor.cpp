#include "metadataextractor.h"
#include <QFileInfo>
#include <QDateTime>
#include <QImageReader>
#include <QFile>
#include <QDir>
#include <QDebug>

// Helper to read 16-bit word depending on endianness
static quint16 read16(const char* data, bool intel) {
    if (intel) {
        return (unsigned char)data[0] | ((unsigned char)data[1] << 8);
    } else {
        return ((unsigned char)data[0] << 8) | (unsigned char)data[1];
    }
}

// Helper to read 32-bit word depending on endianness
static quint32 read32(const char* data, bool intel) {
    if (intel) {
        return (unsigned char)data[0] | ((unsigned char)data[1] << 8) | ((unsigned char)data[2] << 16) | ((unsigned char)data[3] << 24);
    } else {
        return ((unsigned char)data[0] << 24) | ((unsigned char)data[1] << 16) | ((unsigned char)data[2] << 8) | (unsigned char)data[3];
    }
}

// Lightweight EXIF tag parser for JPEG files
static void parseExif(const QByteArray& exifData, QString& cameraModel, QString& dateTaken) {
    if (exifData.size() < 14) return;
    
    bool intel = false;
    if (exifData.at(0) == 'I' && exifData.at(1) == 'I') {
        intel = true;
    } else if (exifData.at(0) == 'M' && exifData.at(1) == 'M') {
        intel = false;
    } else {
        return;
    }
    
    quint16 magic = read16(exifData.constData() + 2, intel);
    if (magic != 0x002A) return;
    
    quint32 ifdOffset = read32(exifData.constData() + 4, intel);
    if (ifdOffset + 2 > (quint32)exifData.size()) return;
    
    quint16 numEntries = read16(exifData.constData() + ifdOffset, intel);
    quint32 entryOffset = ifdOffset + 2;
    
    for (int i = 0; i < numEntries; ++i) {
        if (entryOffset + 12 > (quint32)exifData.size()) break;
        
        const char* entry = exifData.constData() + entryOffset;
        quint16 tag = read16(entry, intel);
        quint16 type = read16(entry + 2, intel);
        quint32 count = read32(entry + 4, intel);
        quint32 valOffset = read32(entry + 8, intel);
        
        if (tag == 0x0110 || tag == 0x0132 || tag == 0x9003) {
            QByteArray valData;
            if (count <= 4) {
                valData = QByteArray(entry + 8, count);
            } else {
                if (valOffset + count <= (quint32)exifData.size()) {
                    valData = exifData.mid(valOffset, count);
                }
            }
            QString str = QString::fromLatin1(valData).trimmed();
            int nullIdx = str.indexOf('\0');
            if (nullIdx != -1) str = str.left(nullIdx);
            
            if (tag == 0x0110) {
                cameraModel = str;
            } else if (tag == 0x9003) {
                dateTaken = str;
            } else if (tag == 0x0132 && dateTaken.isEmpty()) {
                dateTaken = str;
            }
        }
        entryOffset += 12;
    }
}

// Scans JPEG markers to extract EXIF metadata
static void extractJpegExif(const QString& filePath, QString& cameraModel, QString& dateTaken) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    char header[2];
    if (file.read(header, 2) != 2 || (unsigned char)header[0] != 0xFF || (unsigned char)header[1] != 0xD8) {
        return;
    }
    
    while (!file.atEnd()) {
        char marker[2];
        if (file.read(marker, 2) != 2) break;
        if ((unsigned char)marker[0] != 0xFF) break;
        
        unsigned char mType = marker[1];
        if (mType == 0xD9 || mType == 0xDA) { // EOI or SOS
            break;
        }
        
        char lenBytes[2];
        if (file.read(lenBytes, 2) != 2) break;
        quint16 len = ((unsigned char)lenBytes[0] << 8) | (unsigned char)lenBytes[1];
        if (len < 2) break;
        
        QByteArray payload = file.read(len - 2);
        if (payload.size() != len - 2) break;
        
        if (mType == 0xE1) { // App1 marker (EXIF)
            if (payload.startsWith("Exif\0\0")) {
                parseExif(payload.mid(6), cameraModel, dateTaken);
                break;
            }
        }
    }
    file.close();
}

// Parses MPEG Audio Frame Header to extract bitrate & duration
static bool parseMpegHeader(const QByteArray& header, int& bitrate, int& sampleRate, int& durationSec, qint64 fileSize) {
    if (header.size() < 4) return false;
    unsigned char b0 = header.at(0);
    unsigned char b1 = header.at(1);
    unsigned char b2 = header.at(2);
    
    if (b0 != 0xFF || (b1 & 0xE0) != 0xE0) return false;
    
    int versionIdx = (b1 & 0x18) >> 3; // 11 = MPEG-1, 10 = MPEG-2
    int layerIdx = (b1 & 0x06) >> 1;   // 01 = Layer III
    int bitrateIdx = (b2 & 0xF0) >> 4;
    int srateIdx = (b2 & 0x0C) >> 2;
    
    if (bitrateIdx == 0 || bitrateIdx == 15) return false;
    
    int br = 0;
    if (versionIdx == 3) { // MPEG-1
        if (layerIdx == 1) { // Layer III
            static const int table[] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
            br = table[bitrateIdx];
        }
    } else if (versionIdx == 2) { // MPEG-2
        if (layerIdx == 1) { // Layer III
            static const int table[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };
            br = table[bitrateIdx];
        }
    }
    
    int sr = 0;
    if (versionIdx == 3) {
        static const int table[] = { 44100, 48000, 32000, 0 };
        sr = table[srateIdx];
    } else if (versionIdx == 2) {
        static const int table[] = { 22050, 24000, 16000, 0 };
        sr = table[srateIdx];
    }
    
    if (br > 0) {
        bitrate = br;
        sampleRate = sr;
        durationSec = (fileSize * 8) / (br * 1000);
        return true;
    }
    
    return false;
}

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
        if (ext == "jpg" || ext == "jpeg") {
            extractJpegExif(filePath, meta.cameraModel, meta.dateTaken);
        }
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
        result = QString::fromLatin1(textPtr, textSize);
    } else if (encoding == 0x01) {
        if (textSize >= 2) {
            unsigned char bom1 = textPtr[0];
            unsigned char bom2 = textPtr[1];
            const char16_t* u16ptr = reinterpret_cast<const char16_t*>(textPtr + 2);
            int u16size = (textSize - 2) / 2;
            
            if (bom1 == 0xFF && bom2 == 0xFE) {
                result = QString::fromUtf16(u16ptr, u16size);
            } else if (bom1 == 0xFE && bom2 == 0xFF) {
                QByteArray swapped;
                swapped.resize(u16size * 2);
                for (int i = 0; i < u16size * 2; i += 2) {
                    swapped[i] = textPtr[2 + i + 1];
                    swapped[i + 1] = textPtr[2 + i];
                }
                result = QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), u16size);
            } else {
                result = QString::fromUtf16(reinterpret_cast<const char16_t*>(textPtr), textSize / 2);
            }
        }
    } else if (encoding == 0x02) {
        int u16size = textSize / 2;
        QByteArray swapped;
        swapped.resize(u16size * 2);
        for (int i = 0; i < u16size * 2; i += 2) {
            swapped[i] = textPtr[i + 1];
            swapped[i + 1] = textPtr[i];
        }
        result = QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), u16size);
    } else if (encoding == 0x03) {
        result = QString::fromUtf8(textPtr, textSize);
    }

    while (!result.isEmpty() && (result.endsWith('\0') || result.endsWith(' '))) {
        result.chop(1);
    }
    return result.trimmed();
}

static void parseFlacMetadata(const QString& filePath, FileMetadata& meta) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    char sig[4];
    if (file.read(sig, 4) != 4 || strncmp(sig, "fLaC", 4) != 0) {
        file.close();
        return;
    }

    bool isLast = false;
    while (!isLast && !file.atEnd()) {
        char header[4];
        if (file.read(header, 4) != 4) break;

        isLast = (header[0] & 0x80) != 0;
        int blockType = header[0] & 0x7F;
        int length = ((unsigned char)header[1] << 16) |
                     ((unsigned char)header[2] << 8)  |
                     (unsigned char)header[3];

        if (length <= 0) break;

        QByteArray payload = file.read(length);
        if (payload.size() != length) break;

        if (blockType == 4) { // VORBIS_COMMENT
            int offset = 0;
            if (offset + 4 > length) break;
            
            quint32 vendorLen = (unsigned char)payload[offset] |
                                ((unsigned char)payload[offset+1] << 8) |
                                ((unsigned char)payload[offset+2] << 16) |
                                ((unsigned char)payload[offset+3] << 24);
            offset += 4 + vendorLen;
            if (offset + 4 > length) break;

            quint32 numComments = (unsigned char)payload[offset] |
                                  ((unsigned char)payload[offset+1] << 8) |
                                  ((unsigned char)payload[offset+2] << 16) |
                                  ((unsigned char)payload[offset+3] << 24);
            offset += 4;

            for (quint32 i = 0; i < numComments; ++i) {
                if (offset + 4 > length) break;
                quint32 commentLen = (unsigned char)payload[offset] |
                                     ((unsigned char)payload[offset+1] << 8) |
                                     ((unsigned char)payload[offset+2] << 16) |
                                     ((unsigned char)payload[offset+3] << 24);
                offset += 4;

                if (offset + commentLen > (quint32)length) break;
                QString commentStr = QString::fromUtf8(payload.constData() + offset, commentLen);
                offset += commentLen;

                int eqIdx = commentStr.indexOf('=');
                if (eqIdx != -1) {
                    QString key = commentStr.left(eqIdx).toUpper();
                    QString val = commentStr.mid(eqIdx + 1);
                    if (key == "TITLE") meta.title = val;
                    else if (key == "ARTIST") meta.artist = val;
                    else if (key == "ALBUM") meta.album = val;
                    else if (key == "GENRE") meta.genre = val;
                    else if (key == "DATE") meta.year = val;
                    else if (key == "TRACKNUMBER") meta.track = val;
                }
            }
        }
    }
    file.close();
}

void MetadataExtractor::extractAudioInfo(const QString& filePath, FileMetadata& meta) {
    if (filePath.endsWith(".flac", Qt::CaseInsensitive)) {
        parseFlacMetadata(filePath, meta);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    int tagSize = 0;
    bool hasId3v2 = false;

    // Try parsing ID3v2
    char id3Header[10];
    if (file.read(id3Header, 10) == 10) {
        if (strncmp(id3Header, "ID3", 3) == 0) {
            hasId3v2 = true;
            int majorVersion = id3Header[3];
            tagSize = ((id3Header[6] & 0x7F) << 21) |
                      ((id3Header[7] & 0x7F) << 14) |
                      ((id3Header[8] & 0x7F) << 7)  |
                      (id3Header[9] & 0x7F);

            QByteArray tagData = file.read(tagSize);
            int offset = 0;
            
            while (offset + 10 < tagData.size()) {
                const char* frameHeader = tagData.constData() + offset;
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
                } else if (frameId == "TCON") {
                    meta.genre = decodeID3v2Text(frameData);
                } else if (frameId == "TYER" || frameId == "TDRC") {
                    meta.year = decodeID3v2Text(frameData);
                } else if (frameId == "TRCK") {
                    meta.track = decodeID3v2Text(frameData);
                }

                offset += 10 + frameSize;
            }
        }
    }

    // Attempt MPEG Frame Parsing for MP3 files (bitrate/duration)
    qint64 startOffset = hasId3v2 ? (10 + tagSize) : 0;
    if (file.seek(startOffset)) {
        // Read first 4096 bytes to find frame sync
        QByteArray buffer = file.read(4096);
        for (int i = 0; i < buffer.size() - 4; ++i) {
            if ((unsigned char)buffer.at(i) == 0xFF && ((unsigned char)buffer.at(i + 1) & 0xE0) == 0xE0) {
                int bitrate = 0;
                int sampleRate = 0;
                int durationSec = 0;
                if (parseMpegHeader(buffer.mid(i, 4), bitrate, sampleRate, durationSec, file.size() - startOffset)) {
                    meta.bitrate = bitrate;
                    meta.durationMs = static_cast<qint64>(durationSec) * 1000;
                    
                    int mins = durationSec / 60;
                    int secs = durationSec % 60;
                    meta.durationStr = QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
                    break;
                }
            }
        }
    }

    // Try parsing ID3v1 as fallback
    if (meta.title.isEmpty() && meta.artist.isEmpty()) {
        if (file.size() >= 128) {
            file.seek(file.size() - 128);
            QByteArray v1Tag = file.read(128);
            if (v1Tag.startsWith("TAG")) {
                meta.title = QString::fromLatin1(v1Tag.mid(3, 30)).trimmed();
                meta.artist = QString::fromLatin1(v1Tag.mid(33, 30)).trimmed();
                meta.album = QString::fromLatin1(v1Tag.mid(63, 30)).trimmed();
                if (meta.year.isEmpty()) {
                    meta.year = QString::fromLatin1(v1Tag.mid(93, 4)).trimmed();
                }
            }
        }
    }

    file.close();
}
