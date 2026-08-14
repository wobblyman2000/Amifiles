#include "metadatainspectorsidebar.h"
#include "metadataextractor.h"
#include "tagmanager.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QImageReader>
#include <QPixmap>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>

MetadataInspectorSidebar::MetadataInspectorSidebar(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void MetadataInspectorSidebar::setupUI() {
    setFixedWidth(280);
    setStyleSheet("QWidget { background-color: #181825; color: #cdd6f4; font-family: sans-serif; } "
                  "QLabel { color: #cdd6f4; font-size: 12px; } "
                  "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 6px; padding: 6px 12px; font-weight: bold; } "
                  "QPushButton:hover { background-color: #45475a; color: #89b4fa; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Header Title
    QLabel* title = new QLabel("<b>🔍 File Inspector</b>", this);
    title->setStyleSheet("font-size: 14px; color: #89b4fa; border-bottom: 1px solid #313244; padding-bottom: 6px;");
    mainLayout->addWidget(title);

    // Scrollable Content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");

    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Thumbnail Preview Frame
    m_lblThumbnail = new QLabel(scrollContent);
    m_lblThumbnail->setFixedHeight(160);
    m_lblThumbnail->setAlignment(Qt::AlignCenter);
    m_lblThumbnail->setStyleSheet("background-color: #1e1e2e; border: 1px solid #313244; border-radius: 8px;");
    layout->addWidget(m_lblThumbnail);

    // Primary Info Labels
    m_lblName = new QLabel("Select a file to inspect", scrollContent);
    m_lblName->setWordWrap(true);
    m_lblName->setStyleSheet("font-weight: bold; color: #f9e2af; font-size: 13px;");
    layout->addWidget(m_lblName);

    m_lblPath = new QLabel("", scrollContent);
    m_lblPath->setWordWrap(true);
    m_lblPath->setStyleSheet("color: #a6adc8; font-size: 11px;");
    layout->addWidget(m_lblPath);

    m_lblSize = new QLabel("", scrollContent);
    layout->addWidget(m_lblSize);

    m_lblDates = new QLabel("", scrollContent);
    m_lblDates->setWordWrap(true);
    layout->addWidget(m_lblDates);

    m_lblPermissions = new QLabel("", scrollContent);
    layout->addWidget(m_lblPermissions);

    // Dynamic Metadata Section
    m_lblDetailsHeader = new QLabel("<b>Metadata Properties</b>", scrollContent);
    m_lblDetailsHeader->setStyleSheet("color: #b4befe; margin-top: 8px;");
    layout->addWidget(m_lblDetailsHeader);

    m_lblDetailsContent = new QLabel("", scrollContent);
    m_lblDetailsContent->setWordWrap(true);
    m_lblDetailsContent->setStyleSheet("background-color: #1e1e2e; border-radius: 6px; padding: 6px; font-family: monospace; font-size: 11px;");
    layout->addWidget(m_lblDetailsContent);

    // Tags Section
    QLabel* tagsHeader = new QLabel("<b>File Tags</b>", scrollContent);
    tagsHeader->setStyleSheet("color: #a6e3a1; margin-top: 8px;");
    layout->addWidget(tagsHeader);

    m_lblTags = new QLabel("No tags set", scrollContent);
    m_lblTags->setWordWrap(true);
    layout->addWidget(m_lblTags);

    layout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // Action Buttons
    m_btnOpen = new QPushButton("🚀 Open File", this);
    connect(m_btnOpen, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilePath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentFilePath));
        }
    });
    mainLayout->addWidget(m_btnOpen);
}

void MetadataInspectorSidebar::clearInspection() {
    m_currentFilePath.clear();
    m_lblThumbnail->clear();
    m_lblThumbnail->setText("No Selection");
    m_lblName->setText("Select a file to inspect");
    m_lblPath->setText("");
    m_lblSize->setText("");
    m_lblDates->setText("");
    m_lblPermissions->setText("");
    m_lblDetailsContent->setText("");
    m_lblTags->setText("No tags set");
}

void MetadataInspectorSidebar::inspectFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        clearInspection();
        return;
    }

    m_currentFilePath = filePath;
    QFileInfo fi(filePath);

    m_lblName->setText(fi.fileName());
    m_lblPath->setText(fi.absoluteFilePath());

    // Format Size
    qint64 sz = fi.size();
    QString sizeStr;
    if (sz < 1024) sizeStr = QString("%1 B").arg(sz);
    else if (sz < 1024*1024) sizeStr = QString("%1 KB (%2 B)").arg(sz/1024.0, 0, 'f', 1).arg(sz);
    else if (sz < 1024*1024*1024) sizeStr = QString("%1 MB").arg(sz/(1024.0*1024.0), 0, 'f', 2);
    else sizeStr = QString("%1 GB").arg(sz/(1024.0*1024*1024.0), 0, 'f', 2);
    m_lblSize->setText(QString("<b>Size:</b> %1").arg(sizeStr));

    // Dates
    m_lblDates->setText(QString("<b>Modified:</b> %1<br><b>Created:</b> %2")
                        .arg(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss"))
                        .arg(fi.birthTime().isValid() ? fi.birthTime().toString("yyyy-MM-dd hh:mm:ss") : "N/A"));

    // Permissions
    QString perms;
    perms += fi.isReadable() ? "r" : "-";
    perms += fi.isWritable() ? "w" : "-";
    perms += fi.isExecutable() ? "x" : "-";
    m_lblPermissions->setText(QString("<b>Permissions:</b> %1").arg(perms));

    // Tags
    QStringList tags = TagManager::instance().getFileTags(filePath);
    m_lblTags->setText(tags.isEmpty() ? "<i>No tags assigned</i>" : tags.join(", "));

    // Thumbnail Preview
    QImageReader reader(filePath);
    if (reader.canRead()) {
        QImage img = reader.read();
        if (!img.isNull()) {
            m_lblThumbnail->setPixmap(QPixmap::fromImage(img).scaled(m_lblThumbnail->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_lblDetailsContent->setText(QString("Format: %1\nResolution: %2 x %3\nDepth: %4-bit")
                                         .arg(reader.format().toUpper())
                                         .arg(img.width()).arg(img.height())
                                         .arg(img.depth()));
            return;
        }
    }

    // Default icon preview
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    m_lblThumbnail->setPixmap(icon.pixmap(64, 64));

    // Extract EXIF / Audio / Video / Document metadata
    FileMetadata meta = MetadataExtractor::extract(filePath);
    QStringList lines;
    if (!meta.title.isEmpty()) lines.append("Title: " + meta.title);
    if (!meta.artist.isEmpty()) lines.append("Artist: " + meta.artist);
    if (!meta.album.isEmpty()) lines.append("Album: " + meta.album);
    if (!meta.year.isEmpty()) lines.append("Year: " + meta.year);
    if (!meta.genre.isEmpty()) lines.append("Genre: " + meta.genre);
    if (!meta.durationStr.isEmpty()) lines.append("Duration: " + meta.durationStr);
    if (!meta.cameraModel.isEmpty()) lines.append("Camera: " + meta.cameraModel);
    if (!meta.codec.isEmpty()) lines.append("Codec: " + meta.codec);

    if (!lines.isEmpty()) {
        m_lblDetailsContent->setText(lines.join("\n"));
    } else {
        m_lblDetailsContent->setText(QString("Extension: .%1\nType: File").arg(fi.suffix().toUpper()));
    }
}
