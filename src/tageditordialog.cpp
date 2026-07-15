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

TagEditorDialog::TagEditorDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Batch Metadata Tag Editor");
    resize(480, 360);
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
    artworkLayout->setSpacing(8);

    m_lblArtworkStatus = new QLabel("Artwork: Checking...", this);
    m_lblArtworkStatus->setStyleSheet("font-weight: bold; color: #a6adc8;");
    artworkLayout->addWidget(m_lblArtworkStatus, 1);

    m_btnPasteArtwork = new QPushButton("Paste from Clipboard", this);
    connect(m_btnPasteArtwork, &QPushButton::clicked, this, &TagEditorDialog::onPasteArtwork);
    artworkLayout->addWidget(m_btnPasteArtwork);

    m_btnExtractArtwork = new QPushButton("Extract to Folder", this);
    connect(m_btnExtractArtwork, &QPushButton::clicked, this, &TagEditorDialog::onExtractArtwork);
    artworkLayout->addWidget(m_btnExtractArtwork);

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

    if (m_lblArtworkStatus) {
        if (m_filePaths.size() > 1) {
            m_lblArtworkStatus->setText("Embedded Artwork: <Mixed>");
        } else {
            m_lblArtworkStatus->setText(QString("Embedded Artwork: %1").arg(hasArtwork ? "Yes" : "No"));
        }
    }
}

void TagEditorDialog::onSaveClicked() {
    int successCount = 0;
    for (const QString& path : m_filePaths) {
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;

        if (ext == "mp3") {
            // Write MP3 ID3v2 tags
            success = writeMp3Tags(path, m_editTitle->text(), m_editArtist->text(), m_editAlbum->text(), m_editGenre->text(), m_editYear->text(),
                                   m_editAlbumArtist->text(), m_editDiscNumber->text(), m_chkCompilation->isChecked());
        } else if (ext == "flac") {
            // Write FLAC Vorbis comments
            success = writeFlacTags(path, m_editTitle->text(), m_editArtist->text(), m_editAlbum->text(), m_editGenre->text(), m_editYear->text(),
                                    m_editAlbumArtist->text(), m_editDiscNumber->text(), m_chkCompilation->isChecked());
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
                                   const QString& albumArtist, const QString& discNumber, bool compilation) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray fileData = file.readAll();
    file.close();

    // Check for existing ID3v2 tag
    int skipOffset = 0;
    if (fileData.startsWith("ID3")) {
        if (fileData.size() >= 10) {
            int size = ((fileData[6] & 0x7F) << 21) |
                       ((fileData[7] & 0x7F) << 14) |
                       ((fileData[8] & 0x7F) << 7)  |
                        (fileData[9] & 0x7F);
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
    if (compilation) {
        frames.append(createFrame("TCMP", "1"));
    }

    if (frames.isEmpty()) return false;

    // Header size = sum of all frame bytes
    int tagSize = frames.size();
    QByteArray tagHeader;
    tagHeader.append("ID3", 3);
    tagHeader.append((char)3); // ID3v2.3
    tagHeader.append((char)0); // Revision
    tagHeader.append((char)0); // Flags

    // Write tag size as a 32-bit syncsafe integer
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
                                   const QString& albumArtist, const QString& discNumber, bool compilation) {
    QProcess proc;
    QStringList args;
    args << "--remove-tag=TITLE" << "--remove-tag=ARTIST" << "--remove-tag=ALBUM" << "--remove-tag=GENRE" << "--remove-tag=DATE"
         << "--remove-tag=ALBUMARTIST" << "--remove-tag=DISCNUMBER" << "--remove-tag=COMPILATION";
    if (!title.isEmpty()) args << QString("--set-tag=TITLE=%1").arg(title);
    if (!artist.isEmpty()) args << QString("--set-tag=ARTIST=%1").arg(artist);
    if (!album.isEmpty()) args << QString("--set-tag=ALBUM=%1").arg(album);
    if (!genre.isEmpty()) args << QString("--set-tag=GENRE=%1").arg(genre);
    if (!year.isEmpty()) args << QString("--set-tag=DATE=%1").arg(year);
    if (!albumArtist.isEmpty()) args << QString("--set-tag=ALBUMARTIST=%1").arg(albumArtist);
    if (!discNumber.isEmpty()) args << QString("--set-tag=DISCNUMBER=%1").arg(discNumber);
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

    QString tempPath = QDir::tempPath() + "/amifiles_temp_cover.jpg";
    if (!image.save(tempPath, "JPG")) {
        QMessageBox::critical(this, "Save Failed", "Could not create temporary artwork image.");
        return;
    }

    int successCount = 0;
    for (const QString& path : m_filePaths) {
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;

        if (ext == "mp3") {
            QProcess proc;
            proc.start("exiftool", {QString("-Picture<=%1").arg(tempPath), path});
            if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
                success = true;
            }
        } else if (ext == "flac") {
            QProcess proc;
            proc.start("metaflac", {QString("--import-picture-from=%1").arg(tempPath), path});
            if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
                success = true;
            } else {
                QProcess proc2;
                proc2.start("exiftool", {QString("-Picture<=%1").arg(tempPath), path});
                if (proc2.waitForFinished(5000) && proc2.exitCode() == 0) {
                    success = true;
                }
            }
        }

        if (success) successCount++;
    }

    QFile::remove(tempPath);

    if (successCount > 0) {
        QMessageBox::information(this, "Artwork Pasted", QString("Successfully embedded artwork into %1 file(s).").arg(successCount));
        loadCommonTags();
    } else {
        QMessageBox::warning(this, "Paste Failed", "Could not embed artwork. Make sure exiftool or metaflac is installed.");
    }
}

void TagEditorDialog::onExtractArtwork() {
    if (m_filePaths.isEmpty()) return;

    QString firstFile = m_filePaths.first();
    QString defaultDir = QFileInfo(firstFile).absolutePath();
    QString defaultPath = defaultDir + "/cover.jpg";
    QString savePath = QFileDialog::getSaveFileName(this, "Extract Embedded Artwork", defaultPath, "JPEG Images (*.jpg);;All Files (*)");
    if (savePath.isEmpty()) return;

    QProcess proc;
    proc.start("exiftool", {"-Picture", "-b", firstFile});
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
