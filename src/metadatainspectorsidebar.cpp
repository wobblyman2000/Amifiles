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
#include <QFileDevice>
#include <QMessageBox>
#include <QGroupBox>

MetadataInspectorSidebar::MetadataInspectorSidebar(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void MetadataInspectorSidebar::setupUI() {
    setFixedWidth(280);
    setStyleSheet("QWidget { background-color: #181825; color: #cdd6f4; font-family: sans-serif; } "
                  "QLabel { color: #cdd6f4; font-size: 12px; } "
                  "QCheckBox { color: #cdd6f4; font-size: 11px; } "
                  "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 6px; padding: 5px 10px; font-weight: bold; } "
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

    // POSIX Permission Matrix Box
    QGroupBox* permGroup = new QGroupBox("🔐 POSIX Permissions", scrollContent);
    permGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #89b4fa; border: 1px solid #313244; border-radius: 6px; margin-top: 6px; padding-top: 10px; }");
    QVBoxLayout* permLayout = new QVBoxLayout(permGroup);
    permLayout->setSpacing(4);

    m_lblOctalPerms = new QLabel("Mode: 0644 (-rw-r--r--)", permGroup);
    m_lblOctalPerms->setStyleSheet("font-family: monospace; font-weight: bold; color: #a6e3a1; font-size: 11px;");
    permLayout->addWidget(m_lblOctalPerms);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);
    grid->addWidget(new QLabel("Owner:", permGroup), 0, 0);
    m_chkOwnerR = new QCheckBox("R", permGroup);
    m_chkOwnerW = new QCheckBox("W", permGroup);
    m_chkOwnerX = new QCheckBox("X", permGroup);
    grid->addWidget(m_chkOwnerR, 0, 1);
    grid->addWidget(m_chkOwnerW, 0, 2);
    grid->addWidget(m_chkOwnerX, 0, 3);

    grid->addWidget(new QLabel("Group:", permGroup), 1, 0);
    m_chkGroupR = new QCheckBox("R", permGroup);
    m_chkGroupW = new QCheckBox("W", permGroup);
    m_chkGroupX = new QCheckBox("X", permGroup);
    grid->addWidget(m_chkGroupR, 1, 1);
    grid->addWidget(m_chkGroupW, 1, 2);
    grid->addWidget(m_chkGroupX, 1, 3);

    grid->addWidget(new QLabel("Others:", permGroup), 2, 0);
    m_chkOtherR = new QCheckBox("R", permGroup);
    m_chkOtherW = new QCheckBox("W", permGroup);
    m_chkOtherX = new QCheckBox("X", permGroup);
    grid->addWidget(m_chkOtherR, 2, 1);
    grid->addWidget(m_chkOtherW, 2, 2);
    grid->addWidget(m_chkOtherX, 2, 3);

    permLayout->addLayout(grid);

    m_btnApplyPerms = new QPushButton("Apply Chmod", permGroup);
    m_btnApplyPerms->setStyleSheet("background-color: #313244; color: #a6e3a1; font-size: 11px; border-radius: 4px; padding: 4px;");
    connect(m_btnApplyPerms, &QPushButton::clicked, this, &MetadataInspectorSidebar::onApplyPermissions);
    permLayout->addWidget(m_btnApplyPerms);

    auto updateLambda = [this]() { updateOctalDisplay(); };
    connect(m_chkOwnerR, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkOwnerW, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkOwnerX, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkGroupR, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkGroupW, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkGroupX, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkOtherR, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkOtherW, &QCheckBox::toggled, this, updateLambda);
    connect(m_chkOtherX, &QCheckBox::toggled, this, updateLambda);

    layout->addWidget(permGroup);

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

void MetadataInspectorSidebar::updateOctalDisplay() {
    int owner = (m_chkOwnerR->isChecked() ? 4 : 0) + (m_chkOwnerW->isChecked() ? 2 : 0) + (m_chkOwnerX->isChecked() ? 1 : 0);
    int group = (m_chkGroupR->isChecked() ? 4 : 0) + (m_chkGroupW->isChecked() ? 2 : 0) + (m_chkGroupX->isChecked() ? 1 : 0);
    int other = (m_chkOtherR->isChecked() ? 4 : 0) + (m_chkOtherW->isChecked() ? 2 : 0) + (m_chkOtherX->isChecked() ? 1 : 0);

    QString modeStr = QString("%1%2%3").arg(owner).arg(group).arg(other);
    QString str = QString("Mode: 0%1 (").arg(modeStr);
    str += m_chkOwnerR->isChecked() ? "r" : "-";
    str += m_chkOwnerW->isChecked() ? "w" : "-";
    str += m_chkOwnerX->isChecked() ? "x" : "-";
    str += m_chkGroupR->isChecked() ? "r" : "-";
    str += m_chkGroupW->isChecked() ? "w" : "-";
    str += m_chkGroupX->isChecked() ? "x" : "-";
    str += m_chkOtherR->isChecked() ? "r" : "-";
    str += m_chkOtherW->isChecked() ? "w" : "-";
    str += m_chkOtherX->isChecked() ? "x" : "-";
    str += ")";

    m_lblOctalPerms->setText(str);
}

void MetadataInspectorSidebar::onApplyPermissions() {
    if (m_currentFilePath.isEmpty() || !QFile::exists(m_currentFilePath)) return;

    QFileDevice::Permissions perms = {};

    if (m_chkOwnerR->isChecked()) perms |= QFileDevice::ReadOwner;
    if (m_chkOwnerW->isChecked()) perms |= QFileDevice::WriteOwner;
    if (m_chkOwnerX->isChecked()) perms |= QFileDevice::ExeOwner;

    if (m_chkGroupR->isChecked()) perms |= QFileDevice::ReadGroup;
    if (m_chkGroupW->isChecked()) perms |= QFileDevice::WriteGroup;
    if (m_chkGroupX->isChecked()) perms |= QFileDevice::ExeGroup;

    if (m_chkOtherR->isChecked()) perms |= QFileDevice::ReadOther;
    if (m_chkOtherW->isChecked()) perms |= QFileDevice::WriteOther;
    if (m_chkOtherX->isChecked()) perms |= QFileDevice::ExeOther;

    if (QFile::setPermissions(m_currentFilePath, perms)) {
        inspectFile(m_currentFilePath);
        emit filePermissionsChanged(m_currentFilePath);
    } else {
        QMessageBox::warning(this, "Permission Error", "Failed to change file permissions. Check file ownership or access rights.");
    }
}

void MetadataInspectorSidebar::clearInspection() {
    m_currentFilePath.clear();
    m_lblThumbnail->clear();
    m_lblThumbnail->setText("No Selection");
    m_lblName->setText("Select a file to inspect");
    m_lblPath->setText("");
    m_lblSize->setText("");
    m_lblDates->setText("");
    m_lblOctalPerms->setText("Mode: N/A");
    m_lblDetailsContent->setText("");
    m_lblTags->setText("No tags set");

    m_chkOwnerR->setChecked(false);
    m_chkOwnerW->setChecked(false);
    m_chkOwnerX->setChecked(false);
    m_chkGroupR->setChecked(false);
    m_chkGroupW->setChecked(false);
    m_chkGroupX->setChecked(false);
    m_chkOtherR->setChecked(false);
    m_chkOtherW->setChecked(false);
    m_chkOtherX->setChecked(false);
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

    // Set Checkboxes from POSIX permissions
    QFileDevice::Permissions perms = fi.permissions();
    m_chkOwnerR->setChecked(perms & QFileDevice::ReadOwner);
    m_chkOwnerW->setChecked(perms & QFileDevice::WriteOwner);
    m_chkOwnerX->setChecked(perms & QFileDevice::ExeOwner);

    m_chkGroupR->setChecked(perms & QFileDevice::ReadGroup);
    m_chkGroupW->setChecked(perms & QFileDevice::WriteGroup);
    m_chkGroupX->setChecked(perms & QFileDevice::ExeGroup);

    m_chkOtherR->setChecked(perms & QFileDevice::ReadOther);
    m_chkOtherW->setChecked(perms & QFileDevice::WriteOther);
    m_chkOtherX->setChecked(perms & QFileDevice::ExeOther);

    updateOctalDisplay();

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
