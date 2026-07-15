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

    audioForm->addRow("Title:", m_editTitle);
    audioForm->addRow("Artist:", m_editArtist);
    audioForm->addRow("Album:", m_editAlbum);
    audioForm->addRow("Genre:", m_editGenre);
    audioForm->addRow("Year:", m_editYear);

    mainLayout->addWidget(audioGroup);

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
        m_editCamera->setText(camera);
        m_editDateTaken->setText(dateTaken);
    }
}

void TagEditorDialog::onSaveClicked() {
    int successCount = 0;
    for (const QString& path : m_filePaths) {
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;

        if (ext == "mp3") {
            // Write MP3 ID3v2 tags
            success = writeMp3Tags(path, m_editTitle->text(), m_editArtist->text(), m_editAlbum->text(), m_editGenre->text(), m_editYear->text());
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
        QMessageBox::warning(this, "Save Failed", "Could not write metadata. Tag writing is supported for MP3 and JPEG EXIF.");
        reject();
    }
}

// Native ID3v2.3 MP3 tag writing helper
bool TagEditorDialog::writeMp3Tags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year) {
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
