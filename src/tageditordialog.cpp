#include "tageditordialog.h"
#include "metadataextractor.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QGroupBox>
#include <QCheckBox>
#include <QClipboard>
#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QBuffer>

TagEditorDialog::TagEditorDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Batch Metadata Tag Editor");
    resize(580, 520);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 10px; color: #89b4fa; font-weight: bold; padding-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 6px; }"
                  "QLabel { color: #cdd6f4; }"
                  "QLineEdit { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 4px 8px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }");

    setupUI();
    loadCommonTags();
}

void TagEditorDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QLabel* header = new QLabel(QString("Editing metadata for %1 selected file(s):").arg(m_filePaths.size()), this);
    header->setStyleSheet("font-size: 13px; font-weight: bold; color: #89b4fa;");
    mainLayout->addWidget(header);

    // Group 1: Audio Metadata
    QGroupBox* audioGroup = new QGroupBox("Audio Tags (MP3 / FLAC)", this);
    QFormLayout* audioForm = new QFormLayout(audioGroup);
    audioForm->setSpacing(8);

    m_editTitle = new QLineEdit(this);
    m_editArtist = new QLineEdit(this);
    m_editAlbum = new QLineEdit(this);
    m_editGenre = new QLineEdit(this);
    m_editYear = new QLineEdit(this);
    m_editAlbumArtist = new QLineEdit(this);
    m_editDiscNumber = new QLineEdit(this);
    m_chkCompilation = new QCheckBox(this);

    audioForm->addRow("Title:", m_editTitle);
    audioForm->addRow("Artist:", m_editArtist);
    audioForm->addRow("Album Artist:", m_editAlbumArtist);
    audioForm->addRow("Album:", m_editAlbum);
    audioForm->addRow("Genre:", m_editGenre);
    audioForm->addRow("Year:", m_editYear);
    audioForm->addRow("Disc Number:", m_editDiscNumber);
    audioForm->addRow("Compilation:", m_chkCompilation);

    mainLayout->addWidget(audioGroup);

    // Group 1.5: Album Artwork Group
    QGroupBox* artworkGroup = new QGroupBox("Album Artwork", this);
    QHBoxLayout* artworkLayout = new QHBoxLayout(artworkGroup);
    artworkLayout->setSpacing(12);

    QVBoxLayout* artworkLeftLayout = new QVBoxLayout();
    artworkLeftLayout->setSpacing(6);

    m_lblArtworkPreview = new QLabel(this);
    m_lblArtworkPreview->setFixedSize(120, 120);
    m_lblArtworkPreview->setStyleSheet("border: 1px solid #45475a; border-radius: 6px; background-color: #1e1e2e; color: #a6adc8;");
    m_lblArtworkPreview->setAlignment(Qt::AlignCenter);
    artworkLeftLayout->addWidget(m_lblArtworkPreview);

    QHBoxLayout* navLayout = new QHBoxLayout();
    navLayout->setSpacing(4);

    m_btnPrevArtwork = new QPushButton("◀", this);
    m_btnPrevArtwork->setFixedSize(28, 24);
    connect(m_btnPrevArtwork, &QPushButton::clicked, this, &TagEditorDialog::onPrevArtwork);
    navLayout->addWidget(m_btnPrevArtwork);

    m_lblArtworkIndex = new QLabel("0 / 0", this);
    m_lblArtworkIndex->setAlignment(Qt::AlignCenter);
    m_lblArtworkIndex->setStyleSheet("font-size: 11px; color: #a6adc8;");
    navLayout->addWidget(m_lblArtworkIndex, 1);

    m_btnNextArtwork = new QPushButton("▶", this);
    m_btnNextArtwork->setFixedSize(28, 24);
    connect(m_btnNextArtwork, &QPushButton::clicked, this, &TagEditorDialog::onNextArtwork);
    navLayout->addWidget(m_btnNextArtwork);

    artworkLeftLayout->addLayout(navLayout);
    artworkLayout->addLayout(artworkLeftLayout);

    QVBoxLayout* artworkControlsLayout = new QVBoxLayout();
    artworkControlsLayout->setSpacing(8);

    m_lblArtworkStatus = new QLabel("Artwork: Checking...", this);
    m_lblArtworkStatus->setStyleSheet("font-weight: bold; color: #a6adc8;");
    artworkControlsLayout->addWidget(m_lblArtworkStatus);

    m_btnPasteArtwork = new QPushButton("Paste from Clipboard", this);
    m_btnPasteArtwork->setStyleSheet("QPushButton { min-height: 28px; }");
    connect(m_btnPasteArtwork, &QPushButton::clicked, this, &TagEditorDialog::onPasteArtwork);
    artworkControlsLayout->addWidget(m_btnPasteArtwork);

    m_btnExtractArtwork = new QPushButton("Extract to Folder", this);
    m_btnExtractArtwork->setStyleSheet("QPushButton { min-height: 28px; }");
    connect(m_btnExtractArtwork, &QPushButton::clicked, this, &TagEditorDialog::onExtractArtwork);
    artworkControlsLayout->addWidget(m_btnExtractArtwork);

    m_btnDeleteArtwork = new QPushButton("Delete All Artwork", this);
    m_btnDeleteArtwork->setStyleSheet("QPushButton { min-height: 28px; background-color: #f38ba8; color: #11111b; } QPushButton:hover { background-color: #eba0ac; }");
    connect(m_btnDeleteArtwork, &QPushButton::clicked, this, &TagEditorDialog::onDeleteArtwork);
    artworkControlsLayout->addWidget(m_btnDeleteArtwork);

    artworkControlsLayout->addStretch(1);
    artworkLayout->addLayout(artworkControlsLayout, 1);

    mainLayout->addWidget(artworkGroup);

    // Group 2: Image EXIF Metadata
    QGroupBox* exifGroup = new QGroupBox("Image EXIF Tags (JPEG / PNG)", this);
    QFormLayout* exifForm = new QFormLayout(exifGroup);
    exifForm->setSpacing(8);

    m_editCamera = new QLineEdit(this);
    m_editDateTaken = new QLineEdit(this);

    exifForm->addRow("Camera Model:", m_editCamera);
    exifForm->addRow("Date Taken:", m_editDateTaken);

    mainLayout->addWidget(exifGroup);

    // Check which groups are relevant
    bool hasAudio = false;
    bool hasImages = false;
    for (const QString& path : m_filePaths) {
        QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "mp3" || ext == "flac" || ext == "ogg" || ext == "wav") hasAudio = true;
        if (ext == "jpg" || ext == "jpeg" || ext == "png") hasImages = true;
    }

    audioGroup->setVisible(hasAudio || (!hasAudio && !hasImages));
    artworkGroup->setVisible(hasAudio);
    exifGroup->setVisible(hasImages);

    // Bottom layout buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    if (hasAudio) {
        QPushButton* btnAutoFetch = new QPushButton("Auto-Fetch Online", this);
        btnAutoFetch->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
        connect(btnAutoFetch, &QPushButton::clicked, this, &TagEditorDialog::onAutoFetchClicked);
        btnLayout->addWidget(btnAutoFetch);
    }

    QPushButton* btnSave = new QPushButton("Save Tags", this);
    btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(btnSave, &QPushButton::clicked, this, &TagEditorDialog::onSaveClicked);
    btnLayout->addWidget(btnSave);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(btnLayout);
}

void TagEditorDialog::loadCommonTags() {
    if (m_filePaths.isEmpty()) return;

    // To extract common tags, let's load tag info for the first file
    FileMetadata firstMeta = MetadataExtractor::extract(m_filePaths.first());
    
    QString title = firstMeta.title;
    QString artist = firstMeta.artist;
    QString album = firstMeta.album;
    QString genre = firstMeta.genre;
    QString year = firstMeta.year;
    QString camera = firstMeta.cameraModel;
    QString dateTaken = firstMeta.dateTaken;
    QString albumArtist = firstMeta.albumArtist;
    QString discNumber = firstMeta.discNumber;
    bool compilation = firstMeta.compilation;
    bool hasArtwork = firstMeta.hasEmbeddedArtwork;

    // Check if other files match, else set to placeholder
    for (int i = 1; i < m_filePaths.size(); ++i) {
        FileMetadata meta = MetadataExtractor::extract(m_filePaths[i]);
        if (meta.title != title) title = "";
        if (meta.artist != artist) artist = "";
        if (meta.album != album) album = "";
        if (meta.genre != genre) genre = "";
        if (meta.year != year) year = "";
        if (meta.cameraModel != camera) camera = "";
        if (meta.dateTaken != dateTaken) dateTaken = "";
        if (meta.albumArtist != albumArtist) albumArtist = "";
        if (meta.discNumber != discNumber) discNumber = "";
        if (meta.compilation != compilation) compilation = false;
        if (meta.hasEmbeddedArtwork != hasArtwork) hasArtwork = false;
    }

    if (m_filePaths.size() > 1) {
        if (title.isEmpty()) m_editTitle->setPlaceholderText("<Multiple Values>");
        else m_editTitle->setText(title);

        if (artist.isEmpty()) m_editArtist->setPlaceholderText("<Multiple Values>");
        else m_editArtist->setText(artist);

        if (album.isEmpty()) m_editAlbum->setPlaceholderText("<Multiple Values>");
        else m_editAlbum->setText(album);

        if (genre.isEmpty()) m_editGenre->setPlaceholderText("<Multiple Values>");
        else m_editGenre->setText(genre);

        if (year.isEmpty()) m_editYear->setPlaceholderText("<Multiple Values>");
        else m_editYear->setText(year);

        if (albumArtist.isEmpty()) m_editAlbumArtist->setPlaceholderText("<Multiple Values>");
        else m_editAlbumArtist->setText(albumArtist);

        if (discNumber.isEmpty()) m_editDiscNumber->setPlaceholderText("<Multiple Values>");
        else m_editDiscNumber->setText(discNumber);

        m_chkCompilation->setChecked(compilation);

        if (camera.isEmpty()) m_editCamera->setPlaceholderText("<Multiple Values>");
        else m_editCamera->setText(camera);

        if (dateTaken.isEmpty()) m_editDateTaken->setPlaceholderText("<Multiple Values>");
        else m_editDateTaken->setText(dateTaken);
    } else {
        m_editTitle->setText(title);
        m_editArtist->setText(artist);
        m_editAlbum->setText(album);
        m_editGenre->setText(genre);
        m_editYear->setText(year);
        m_editAlbumArtist->setText(albumArtist);
        m_editDiscNumber->setText(discNumber);
        m_chkCompilation->setChecked(compilation);
        m_editCamera->setText(camera);
        m_editDateTaken->setText(dateTaken);
    }

    if (m_filePaths.size() > 1) {
        if (m_lblArtworkStatus) {
            m_lblArtworkStatus->setText("Embedded Artwork: <Mixed>");
        }
        if (m_lblArtworkPreview) {
            m_lblArtworkPreview->setText("Multiple Files");
        }
        m_availableArtworkTags.clear();
        m_currentArtworkIndex = 0;
        if (m_btnPrevArtwork) m_btnPrevArtwork->setEnabled(false);
        if (m_btnNextArtwork) m_btnNextArtwork->setEnabled(false);
        if (m_lblArtworkIndex) m_lblArtworkIndex->setText("0 / 0");
    } else {
        QString firstFile = m_filePaths.first();
        m_availableArtworkTags = getEmbeddedArtworkTags(firstFile);
        m_currentArtworkIndex = 0;
        
        if (m_lblArtworkStatus) {
            if (!m_availableArtworkTags.isEmpty()) {
                m_lblArtworkStatus->setText(QString("Embedded Artwork: Yes (%1)").arg(m_availableArtworkTags.size()));
            } else {
                m_lblArtworkStatus->setText("Embedded Artwork: No");
            }
        }
        updateArtworkDisplay();
    }
}

void TagEditorDialog::onSaveClicked() {
    int successCount = 0;
    for (int idx = 0; idx < m_filePaths.size(); ++idx) {
        QString path = m_filePaths[idx];
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;

        QString title, artist, album, year, trackNum, trackCount;
        if (m_fetchedMetadataMap.contains(idx)) {
            FetchedTrack track = m_fetchedMetadataMap[idx];
            title = track.title;
            artist = track.artist.isEmpty() ? m_editArtist->text() : track.artist;
            album = track.album.isEmpty() ? m_editAlbum->text() : track.album;
            year = track.year.isEmpty() ? m_editYear->text() : track.year;
            trackNum = QString::number(track.trackNumber);
            trackCount = QString::number(track.trackCount);
        } else {
            title = m_editTitle->text();
            artist = m_editArtist->text();
            album = m_editAlbum->text();
            year = m_editYear->text();
        }

        QString genre = m_editGenre->text();
        QString albumArtist = m_editAlbumArtist->text();
        QString discNumber = m_editDiscNumber->text();
        bool compilation = m_chkCompilation->isChecked();

        if (ext == "mp3") {
            // Write MP3 ID3v2 tags
            success = writeMp3Tags(path, title, artist, album, genre, year,
                                   albumArtist, discNumber, compilation,
                                   !m_fetchedArtworkData.isEmpty(), m_fetchedArtworkData, m_fetchedArtworkMimeType,
                                   trackNum);
        } else if (ext == "flac") {
            // Write FLAC Vorbis comments
            success = writeFlacTags(path, title, artist, album, genre, year,
                                    albumArtist, discNumber, compilation,
                                    trackNum, trackCount);
            if (success && !m_fetchedArtworkData.isEmpty()) {
                writeFlacArtwork(path, m_fetchedArtworkData, m_fetchedArtworkMimeType);
            }
        } else if (ext == "jpg" || ext == "jpeg" || ext == "png") {
            // Write JPEG/PNG EXIF tags
            success = writeExifTags(path, m_editCamera->text(), m_editDateTaken->text());
        }

        if (success) successCount++;
    }

    if (successCount > 0) {
        QMessageBox::information(this, "Tags Saved", QString("Successfully updated %1 file(s).").arg(successCount));
        accept();
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not write metadata. Tag writing is supported for MP3, FLAC, and JPEG EXIF.");
        reject();
    }
}

// Native ID3v2.3 MP3 tag writing helper
bool TagEditorDialog::writeMp3Tags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year,
                                   const QString& albumArtist, const QString& discNumber, bool compilation,
                                   bool stripArtwork, const QByteArray& newArtworkData, const QString& mimeType,
                                   const QString& trackNumber) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray fileData = file.readAll();
    file.close();

    // Check for existing ID3v2 tag and extract its payload
    int skipOffset = 0;
    QByteArray oldTagData;
    if (fileData.startsWith("ID3")) {
        if (fileData.size() >= 10) {
            int size = ((fileData[6] & 0x7F) << 21) |
                       ((fileData[7] & 0x7F) << 14) |
                       ((fileData[8] & 0x7F) << 7)  |
                        (fileData[9] & 0x7F);
            oldTagData = fileData.mid(10, size);
            skipOffset = 10 + size;
        }
    }

    // Build raw frames helper lambda
    auto createFrame = [](const char* id, const QString& val) -> QByteArray {
        if (val.isEmpty()) return QByteArray();
        QByteArray payload;
        payload.append((char)0); // ISO-8859-1 encoding indicator
        payload.append(val.toLocal8Bit());

        QByteArray frame;
        frame.append(id, 4);

        int size = payload.size();
        frame.append((size >> 24) & 0xFF);
        frame.append((size >> 16) & 0xFF);
        frame.append((size >> 8) & 0xFF);
        frame.append(size & 0xFF);

        frame.append((char)0); // Flags
        frame.append((char)0);

        frame.append(payload);
        return frame;
    };

    QByteArray frames;
    frames.append(createFrame("TIT2", title));
    frames.append(createFrame("TPE1", artist));
    frames.append(createFrame("TALB", album));
    frames.append(createFrame("TCON", genre));
    frames.append(createFrame("TYER", year));
    frames.append(createFrame("TPE2", albumArtist));
    frames.append(createFrame("TPOS", discNumber));
    frames.append(createFrame("TRCK", trackNumber));
    if (compilation) {
        frames.append(createFrame("TCMP", "1"));
    }

    // Parse and copy non-text frames from the old tag
    if (!oldTagData.isEmpty()) {
        int offset = 0;
        while (offset + 10 < oldTagData.size()) {
            const char* frameHeader = oldTagData.constData() + offset;
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

            int frameSize = ((unsigned char)frameHeader[4] << 24) |
                            ((unsigned char)frameHeader[5] << 16) |
                            ((unsigned char)frameHeader[6] << 8)  |
                            (unsigned char)frameHeader[7];

            if (frameSize <= 0 || offset + 10 + frameSize > oldTagData.size()) {
                break;
            }

            // Skip frames that we are overwriting
            bool shouldPreserve = true;
            if (frameId == "TIT2" || frameId == "TPE1" || frameId == "TALB" ||
                frameId == "TCON" || frameId == "TYER" || frameId == "TDRC" ||
                frameId == "TPE2" || frameId == "TPOS" || frameId == "TCMP" ||
                frameId == "TRCK") {
                shouldPreserve = false;
            }

            // Skip APIC frames if we are stripping artwork or adding new artwork
            if (frameId == "APIC" && (stripArtwork || !newArtworkData.isEmpty())) {
                shouldPreserve = false;
            }

            if (shouldPreserve) {
                frames.append(oldTagData.mid(offset, 10 + frameSize));
            }

            offset += 10 + frameSize;
        }
    }

    // Append new artwork if provided
    if (!newArtworkData.isEmpty()) {
        QByteArray payload;
        payload.append((char)0); // Text encoding: Latin1
        payload.append(mimeType.toLatin1());
        payload.append((char)0); // Null terminator
        payload.append((char)3); // Cover (front)
        payload.append((char)0); // Description: empty null-terminated
        payload.append(newArtworkData);

        QByteArray apicFrame;
        apicFrame.append("APIC", 4);

        int size = payload.size();
        apicFrame.append((size >> 24) & 0xFF);
        apicFrame.append((size >> 16) & 0xFF);
        apicFrame.append((size >> 8) & 0xFF);
        apicFrame.append(size & 0xFF);

        apicFrame.append((char)0); // Flags
        apicFrame.append((char)0);

        apicFrame.append(payload);

        frames.append(apicFrame);
    }

    if (frames.isEmpty()) return false;

    // Header size = sum of all frame bytes
    int tagSize = frames.size();
    QByteArray tagHeader;
    tagHeader.append("ID3", 3);
    tagHeader.append((char)3); // ID3v2.3
    tagHeader.append((char)0); // Revision
    tagHeader.append((char)0); // Flags

    tagHeader.append((tagSize >> 21) & 0x7F);
    tagHeader.append((tagSize >> 14) & 0x7F);
    tagHeader.append((tagSize >> 7) & 0x7F);
    tagHeader.append(tagSize & 0x7F);

    QByteArray newFileData;
    newFileData.append(tagHeader);
    newFileData.append(frames);
    newFileData.append(fileData.constData() + skipOffset, fileData.size() - skipOffset);

    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(newFileData);
    file.close();
    return true;
}

// Write EXIF tags using command-line tools
bool TagEditorDialog::writeExifTags(const QString& filePath, const QString& camera, const QString& dateTaken) {
    if (camera.isEmpty() && dateTaken.isEmpty()) return false;

    QProcess proc;
    QStringList args;
    if (!camera.isEmpty()) {
        args.append(QString("-Model=%1").arg(camera));
    }
    if (!dateTaken.isEmpty()) {
        args.append(QString("-DateTimeOriginal=%1").arg(dateTaken));
    }
    args.append(filePath);

    // Fall back to exiftool
    proc.start("exiftool", args);
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
        return true;
    }

    // Try jhead for simple JPEG updates
    if (!dateTaken.isEmpty()) {
        QProcess proc2;
        proc2.start("jhead", {"-ts" + dateTaken, filePath});
        if (proc2.waitForFinished(3000) && proc2.exitCode() == 0) {
            return true;
        }
    }

    return false;
}

bool TagEditorDialog::writeFlacTags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year,
                                   const QString& albumArtist, const QString& discNumber, bool compilation,
                                   const QString& trackNumber, const QString& trackTotal) {
    QProcess proc;
    QStringList args;
    args << "--remove-tag=TITLE" << "--remove-tag=ARTIST" << "--remove-tag=ALBUM" << "--remove-tag=GENRE" << "--remove-tag=DATE"
         << "--remove-tag=ALBUMARTIST" << "--remove-tag=DISCNUMBER" << "--remove-tag=COMPILATION"
         << "--remove-tag=TRACKNUMBER" << "--remove-tag=TRACKTOTAL";
    if (!title.isEmpty()) args << QString("--set-tag=TITLE=%1").arg(title);
    if (!artist.isEmpty()) args << QString("--set-tag=ARTIST=%1").arg(artist);
    if (!album.isEmpty()) args << QString("--set-tag=ALBUM=%1").arg(album);
    if (!genre.isEmpty()) args << QString("--set-tag=GENRE=%1").arg(genre);
    if (!year.isEmpty()) args << QString("--set-tag=DATE=%1").arg(year);
    if (!albumArtist.isEmpty()) args << QString("--set-tag=ALBUMARTIST=%1").arg(albumArtist);
    if (!discNumber.isEmpty()) args << QString("--set-tag=DISCNUMBER=%1").arg(discNumber);
    if (!trackNumber.isEmpty()) args << QString("--set-tag=TRACKNUMBER=%1").arg(trackNumber);
    if (!trackTotal.isEmpty()) args << QString("--set-tag=TRACKTOTAL=%1").arg(trackTotal);
    args << QString("--set-tag=COMPILATION=%1").arg(compilation ? "1" : "0");
    args << filePath;

    proc.start("metaflac", args);
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
        return true;
    }
    
    QProcess proc2;
    QStringList args2;
    if (!title.isEmpty()) args2 << QString("-Title=%1").arg(title);
    if (!artist.isEmpty()) args2 << QString("-Artist=%1").arg(artist);
    if (!album.isEmpty()) args2 << QString("-Album=%1").arg(album);
    if (!genre.isEmpty()) args2 << QString("-Genre=%1").arg(genre);
    if (!year.isEmpty()) args2 << QString("-Date=%1").arg(year);
    if (!albumArtist.isEmpty()) args2 << QString("-Band=%1").arg(albumArtist);
    if (!discNumber.isEmpty()) args2 << QString("-PartOfSet=%1").arg(discNumber);
    if (!trackNumber.isEmpty()) {
        if (!trackTotal.isEmpty()) {
            args2 << QString("-Track=%1/%2").arg(trackNumber).arg(trackTotal);
        } else {
            args2 << QString("-Track=%1").arg(trackNumber);
        }
    }
    args2 << QString("-Compilation=%1").arg(compilation ? "1" : "0");
    args2 << filePath;

    proc2.start("exiftool", args2);
    if (proc2.waitForFinished(3000) && proc2.exitCode() == 0) {
        return true;
    }
    return false;
}

void TagEditorDialog::onPasteArtwork() {
    QClipboard* clipboard = QApplication::clipboard();
    QImage image = clipboard->image();
    if (image.isNull()) {
        QMessageBox::warning(this, "Paste Failed", "No image found in the clipboard. Copy an image first!");
        return;
    }

    QByteArray imgBytes;
    {
        QBuffer buffer(&imgBytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG");
    }

    int successCount = 0;
    for (const QString& path : m_filePaths) {
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;

        if (ext == "mp3") {
            success = writeMp3Tags(path, m_editTitle->text(), m_editArtist->text(), m_editAlbum->text(), m_editGenre->text(), m_editYear->text(),
                                   m_editAlbumArtist->text(), m_editDiscNumber->text(), m_chkCompilation->isChecked(),
                                   true, imgBytes, "image/jpeg");
        } else if (ext == "flac") {
            success = writeFlacArtwork(path, imgBytes, "image/jpeg");
        }

        if (success) successCount++;
    }

    if (successCount > 0) {
        QMessageBox::information(this, "Artwork Pasted", QString("Successfully embedded artwork into %1 file(s).").arg(successCount));
        loadCommonTags();
    } else {
        QMessageBox::warning(this, "Paste Failed", "Could not embed artwork. Make sure exiftool or metaflac is installed.");
    }
}

void TagEditorDialog::onExtractArtwork() {
    if (m_filePaths.isEmpty()) return;

    if (m_availableArtworkTags.isEmpty()) {
        QMessageBox::warning(this, "Extraction Failed", "No embedded artwork found to extract.");
        return;
    }

    QString firstFile = m_filePaths.first();
    QString defaultDir = QFileInfo(firstFile).absolutePath();
    QString defaultPath = defaultDir + "/cover.jpg";
    QString savePath = QFileDialog::getSaveFileName(this, "Extract Embedded Artwork", defaultPath, "JPEG Images (*.jpg);;All Files (*)");
    if (savePath.isEmpty()) return;

    QString tag = m_availableArtworkTags.at(m_currentArtworkIndex);

    QProcess proc;
    proc.start("exiftool", {"-" + tag, "-b", firstFile});
    if (proc.waitForFinished(5000)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            QFile outFile(savePath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(imgData);
                outFile.close();
                QMessageBox::information(this, "Extraction Successful", "Artwork successfully saved to:\n" + savePath);
                return;
            }
        }
    }

    QMessageBox::warning(this, "Extraction Failed", "No embedded artwork found or exiftool is not installed.");
}

QPixmap TagEditorDialog::loadEmbeddedArtwork(const QString& filePath) {
    QProcess proc;
    proc.start("exiftool", {"-Picture", "-b", filePath});
    if (proc.waitForFinished(5000)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            QPixmap pix;
            if (pix.loadFromData(imgData)) {
                return pix;
            }
        }
    }
    return QPixmap();
}

QPixmap TagEditorDialog::loadEmbeddedArtworkAtIndex(const QString& filePath, const QString& tag) {
    QProcess proc;
    proc.start("exiftool", {"-" + tag, "-b", filePath});
    if (proc.waitForFinished(5000)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            QPixmap pix;
            if (pix.loadFromData(imgData)) {
                return pix;
            }
        }
    }
    return QPixmap();
}

QStringList TagEditorDialog::getEmbeddedArtworkTags(const QString& filePath) {
    QStringList tags;
    QStringList queryArgs;
    queryArgs << "-a" << "-s";
    for (int i = 1; i <= 10; ++i) {
        if (i == 1) queryArgs << "-Picture";
        else queryArgs << QString("-Picture%1").arg(i);
    }
    queryArgs << filePath;

    QProcess proc;
    proc.start("exiftool", queryArgs);
    if (proc.waitForFinished(5000)) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            int colonIdx = line.indexOf(':');
            if (colonIdx != -1) {
                QString key = line.left(colonIdx).trimmed();
                if (key.startsWith("Picture", Qt::CaseInsensitive)) {
                    tags << key;
                }
            }
        }
    }
    return tags;
}

void TagEditorDialog::updateArtworkDisplay() {
    if (m_availableArtworkTags.isEmpty()) {
        if (m_lblArtworkPreview) m_lblArtworkPreview->setText("No Artwork");
        if (m_lblArtworkIndex) m_lblArtworkIndex->setText("0 / 0");
        if (m_btnPrevArtwork) m_btnPrevArtwork->setEnabled(false);
        if (m_btnNextArtwork) m_btnNextArtwork->setEnabled(false);
        return;
    }

    if (m_currentArtworkIndex < 0) m_currentArtworkIndex = 0;
    if (m_currentArtworkIndex >= m_availableArtworkTags.size()) {
        m_currentArtworkIndex = m_availableArtworkTags.size() - 1;
    }

    QString tag = m_availableArtworkTags.at(m_currentArtworkIndex);
    QPixmap pix = loadEmbeddedArtworkAtIndex(m_filePaths.first(), tag);

    if (m_lblArtworkPreview) {
        if (!pix.isNull()) {
            m_lblArtworkPreview->setPixmap(pix.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_lblArtworkPreview->setText("No Image");
        }
    }

    if (m_lblArtworkIndex) {
        m_lblArtworkIndex->setText(QString("%1 / %2").arg(m_currentArtworkIndex + 1).arg(m_availableArtworkTags.size()));
    }

    if (m_btnPrevArtwork) {
        m_btnPrevArtwork->setEnabled(m_currentArtworkIndex > 0);
    }
    if (m_btnNextArtwork) {
        m_btnNextArtwork->setEnabled(m_currentArtworkIndex < m_availableArtworkTags.size() - 1);
    }
}

void TagEditorDialog::onPrevArtwork() {
    if (m_currentArtworkIndex > 0) {
        m_currentArtworkIndex--;
        updateArtworkDisplay();
    }
}

void TagEditorDialog::onNextArtwork() {
    if (m_currentArtworkIndex < m_availableArtworkTags.size() - 1) {
        m_currentArtworkIndex++;
        updateArtworkDisplay();
    }
}

void TagEditorDialog::onDeleteArtwork() {
    if (m_filePaths.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Deletion",
        "Are you sure you want to delete ALL embedded artwork from the selected file(s)?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int successCount = 0;
    for (const QString& path : m_filePaths) {
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;

        if (ext == "mp3") {
            success = writeMp3Tags(path, m_editTitle->text(), m_editArtist->text(), m_editAlbum->text(), m_editGenre->text(), m_editYear->text(),
                                   m_editAlbumArtist->text(), m_editDiscNumber->text(), m_chkCompilation->isChecked(),
                                   true);
        } else if (ext == "flac") {
            success = stripFlacArtwork(path);
        }

        if (success) successCount++;
    }

    if (successCount > 0) {
        QMessageBox::information(this, "Artwork Deleted", QString("Successfully removed all artwork from %1 file(s).").arg(successCount));
        loadCommonTags();
    } else {
        QMessageBox::warning(this, "Delete Failed", "Could not remove artwork. Make sure the files are not write-protected.");
    }
}

bool TagEditorDialog::stripFlacArtwork(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() < 4 || strncmp(fileData.constData(), "fLaC", 4) != 0) {
        return false;
    }

    QByteArray newFileData;
    newFileData.append("fLaC", 4);

    int offset = 4;
    bool isLast = false;

    struct Block {
        char header[4];
        QByteArray payload;
        int type;
        bool last;
    };

    QList<Block> keptBlocks;

    while (!isLast && offset + 4 <= fileData.size()) {
        char header[4];
        memcpy(header, fileData.constData() + offset, 4);

        isLast = (header[0] & 0x80) != 0;
        int blockType = header[0] & 0x7F;
        int length = ((unsigned char)header[1] << 16) |
                     ((unsigned char)header[2] << 8)  |
                     (unsigned char)header[3];

        offset += 4;
        if (offset + length > fileData.size()) return false;

        QByteArray payload = fileData.mid(offset, length);
        offset += length;

        if (blockType != 6) { // Skip PICTURE
            Block b;
            memcpy(b.header, header, 4);
            b.payload = payload;
            b.type = blockType;
            b.last = isLast;
            keptBlocks.append(b);
        }
    }

    if (keptBlocks.isEmpty()) return false;

    // Ensure the last block is marked as last
    for (int i = 0; i < keptBlocks.size(); ++i) {
        keptBlocks[i].header[0] &= 0x7F;
    }
    keptBlocks.last().header[0] |= 0x80;

    for (const Block& b : keptBlocks) {
        newFileData.append(b.header, 4);
        newFileData.append(b.payload);
    }

    if (offset < fileData.size()) {
        newFileData.append(fileData.constData() + offset, fileData.size() - offset);
    }

    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(newFileData);
    file.close();
    return true;
}

bool TagEditorDialog::writeFlacArtwork(const QString& filePath, const QByteArray& imgData, const QString& mimeType) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() < 4 || strncmp(fileData.constData(), "fLaC", 4) != 0) {
        return false;
    }

    QByteArray newFileData;
    newFileData.append("fLaC", 4);

    int offset = 4;
    bool isLast = false;

    struct Block {
        char header[4];
        QByteArray payload;
        int type;
        bool last;
    };

    QList<Block> keptBlocks;

    while (!isLast && offset + 4 <= fileData.size()) {
        char header[4];
        memcpy(header, fileData.constData() + offset, 4);

        isLast = (header[0] & 0x80) != 0;
        int blockType = header[0] & 0x7F;
        int length = ((unsigned char)header[1] << 16) |
                     ((unsigned char)header[2] << 8)  |
                     (unsigned char)header[3];

        offset += 4;
        if (offset + length > fileData.size()) return false;

        QByteArray payload = fileData.mid(offset, length);
        offset += length;

        if (blockType != 6) { // Strip old PICTURE blocks
            Block b;
            memcpy(b.header, header, 4);
            b.payload = payload;
            b.type = blockType;
            b.last = isLast;
            keptBlocks.append(b);
        }
    }

    // Build new PICTURE block payload
    QByteArray picPayload;
    picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)3);

    QByteArray mimeBytes = mimeType.toLatin1();
    int mimeLen = mimeBytes.size();
    picPayload.append((mimeLen >> 24) & 0xFF);
    picPayload.append((mimeLen >> 16) & 0xFF);
    picPayload.append((mimeLen >> 8) & 0xFF);
    picPayload.append(mimeLen & 0xFF);
    picPayload.append(mimeBytes);

    picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0);

    picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0);
    picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0);
    picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0);
    picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0); picPayload.append((char)0);

    int imgLen = imgData.size();
    picPayload.append((imgLen >> 24) & 0xFF);
    picPayload.append((imgLen >> 16) & 0xFF);
    picPayload.append((imgLen >> 8) & 0xFF);
    picPayload.append(imgLen & 0xFF);
    picPayload.append(imgData);

    Block picBlock;
    picBlock.type = 6;
    picBlock.payload = picPayload;
    picBlock.last = false;

    int picLen = picPayload.size();
    picBlock.header[0] = 6;
    picBlock.header[1] = (picLen >> 16) & 0xFF;
    picBlock.header[2] = (picLen >> 8) & 0xFF;
    picBlock.header[3] = picLen & 0xFF;

    keptBlocks.insert(keptBlocks.size(), picBlock);

    for (int i = 0; i < keptBlocks.size(); ++i) {
        keptBlocks[i].header[0] &= 0x7F;
    }
    keptBlocks.last().header[0] |= 0x80;

    for (const Block& b : keptBlocks) {
        newFileData.append(b.header, 4);
        newFileData.append(b.payload);
    }

    if (offset < fileData.size()) {
        newFileData.append(fileData.constData() + offset, fileData.size() - offset);
    }

    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(newFileData);
    file.close();
    return true;
}

void TagEditorDialog::onAutoFetchClicked() {
    MetadataFetcherDialog dlg(m_filePaths, this);
    if (dlg.exec() == QDialog::Accepted) {
        auto results = dlg.getResults();

        // Cache the fetched artwork
        m_fetchedArtworkData = dlg.getArtworkData();
        m_fetchedArtworkMimeType = dlg.getArtworkMimeType();

        // Update the artwork preview layout in TagEditorDialog if artwork was fetched
        if (!m_fetchedArtworkData.isEmpty()) {
            QPixmap pixmap;
            if (pixmap.loadFromData(m_fetchedArtworkData)) {
                m_lblArtworkPreview->setPixmap(pixmap.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                if (m_lblArtworkStatus) {
                    m_lblArtworkStatus->setText("Artwork: Fetched (pending save)");
                }
            }
        }

        // Update the LineEdits with common/single values
        if (m_filePaths.size() == 1) {
            FetchedTrack track = results.value(0);
            m_editTitle->setText(track.title);
            m_editArtist->setText(track.artist);
            m_editAlbum->setText(track.album);
            m_editYear->setText(track.year);
            if (!track.genre.isEmpty()) {
                m_editGenre->setText(track.genre);
            }
        } else {
            FetchedTrack firstTrack = results.value(0);
            m_editArtist->setText(firstTrack.artist);
            m_editAlbum->setText(firstTrack.album);
            m_editYear->setText(firstTrack.year);
            if (!firstTrack.genre.isEmpty()) {
                m_editGenre->setText(firstTrack.genre);
            }
        }

        // Store the full map of results so onSaveClicked can write track-specific tags
        m_fetchedMetadataMap = results;
    }
}
