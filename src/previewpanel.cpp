#include "previewpanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QImageReader>
#include <QAudioOutput>
#include <QFileInfo>
#include <QDir>
#include <QHeaderView>
#include <QMediaMetaData>
#include <QStyle>
#include <QApplication>
#include <QDateTime>
#include <QSettings>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QTableWidgetItem>

PreviewPanel::PreviewPanel(QWidget* parent) : QWidget(parent) {
    // Initialize QMediaPlayer and AudioOutput first so setupUI can configure them
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);

    setupUI();

    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    // Connect player signals
    connect(m_player, &QMediaPlayer::positionChanged, this, &PreviewPanel::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PreviewPanel::onDurationChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &PreviewPanel::onMediaMetadataChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PreviewPanel::onMediaStatusChanged);

    clearPreview();
}

PreviewPanel::~PreviewPanel() {
    m_player->stop();
}

void PreviewPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // Dynamic Stack
    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 6px;");

    // 1. Empty/Placeholder View
    m_emptyView = new QWidget(this);
    QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyView);
    QLabel* lblEmpty = new QLabel("Select a file or folder to preview", m_emptyView);
    lblEmpty->setAlignment(Qt::AlignCenter);
    lblEmpty->setStyleSheet("color: #585b70; font-size: 14px; font-weight: bold;");
    emptyLayout->addWidget(lblEmpty);
    m_stack->addWidget(m_emptyView);

    // 2. Text View
    m_textView = new QWidget(this);
    QVBoxLayout* textLayout = new QVBoxLayout(m_textView);
    textLayout->setContentsMargins(4, 4, 4, 4);
    
    m_textEdit = new QPlainTextEdit(m_textView);
    m_textEdit->setStyleSheet("QPlainTextEdit { background-color: #11111b; border: none; font-family: 'Consolas', 'Monaco', monospace; font-size: 12px; }");
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &PreviewPanel::onTextChanged);
    
    m_textControls = new QWidget(m_textView);
    QHBoxLayout* textCtrlLayout = new QHBoxLayout(m_textControls);
    textCtrlLayout->setContentsMargins(0, 4, 0, 0);
    m_btnSaveText = new QPushButton("Save Changes", m_textControls);
    m_btnSaveText->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnSaveText, &QPushButton::clicked, this, &PreviewPanel::onSaveText);
    textCtrlLayout->addStretch(1);
    textCtrlLayout->addWidget(m_btnSaveText);
    
    textLayout->addWidget(m_textEdit);
    textLayout->addWidget(m_textControls);
    m_stack->addWidget(m_textView);

    // 3. Image View
    m_imageView = new QWidget(this);
    QVBoxLayout* imageLayout = new QVBoxLayout(m_imageView);
    imageLayout->setContentsMargins(0, 0, 0, 0);

    m_imageScrollArea = new QScrollArea(m_imageView);
    m_imageScrollArea->setWidgetResizable(true);
    m_imageScrollArea->setAlignment(Qt::AlignCenter);
    m_imageScrollArea->setStyleSheet("background-color: #11111b; border: none;");

    m_imageLabel = new QLabel(m_imageScrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageScrollArea->setWidget(m_imageLabel);

    imageLayout->addWidget(m_imageScrollArea);
    m_stack->addWidget(m_imageView);

    // 4. Media View (Audio / Video)
    m_mediaView = new QWidget(this);
    QVBoxLayout* mediaLayout = new QVBoxLayout(m_mediaView);
    mediaLayout->setContentsMargins(4, 4, 4, 4);

    m_videoWidget = new QVideoWidget(m_mediaView);
    m_videoWidget->setStyleSheet("background-color: #000000; border-radius: 4px;");
    m_videoWidget->setMinimumHeight(150);

    m_audioPlaceholder = new QLabel("🎵 Audio Track", m_mediaView);
    m_audioPlaceholder->setAlignment(Qt::AlignCenter);
    m_audioPlaceholder->setStyleSheet("color: #cdd6f4; font-size: 16px; font-weight: bold; background-color: #1e1e2e; border-radius: 4px;");
    m_audioPlaceholder->setMinimumHeight(150);

    // Media Controls Layout
    QHBoxLayout* mediaCtrlLayout = new QHBoxLayout();
    mediaCtrlLayout->setSpacing(4);

    QStyle* style = QApplication::style();

    m_btnPlayPause = new QPushButton(this);
    m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    m_btnPlayPause->setToolTip("Play/Pause");
    m_btnPlayPause->setMaximumWidth(40);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &PreviewPanel::onPlayPause);

    m_btnStop = new QPushButton(this);
    m_btnStop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    m_btnStop->setToolTip("Stop");
    m_btnStop->setMaximumWidth(40);
    connect(m_btnStop, &QPushButton::clicked, this, &PreviewPanel::onStop);

    m_sliderProgress = new QSlider(Qt::Horizontal, this);
    m_sliderProgress->setRange(0, 0);
    connect(m_sliderProgress, &QSlider::sliderMoved, this, &PreviewPanel::onSliderMoved);

    m_lblProgressTime = new QLabel("00:00 / 00:00", this);
    m_lblProgressTime->setStyleSheet("color: #a6adc8; font-size: 11px;");

    QLabel* lblVol = new QLabel("🔊", this);
    lblVol->setStyleSheet("font-size: 14px;");

    m_sliderVolume = new QSlider(Qt::Horizontal, this);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(70);
    m_sliderVolume->setMaximumWidth(80);
    m_audioOutput->setVolume(0.7f);
    connect(m_sliderVolume, &QSlider::valueChanged, this, &PreviewPanel::onVolumeChanged);

    mediaCtrlLayout->addWidget(m_btnPlayPause);
    mediaCtrlLayout->addWidget(m_btnStop);
    mediaCtrlLayout->addWidget(m_sliderProgress);
    mediaCtrlLayout->addWidget(m_lblProgressTime);
    mediaCtrlLayout->addWidget(lblVol);
    mediaCtrlLayout->addWidget(m_sliderVolume);

    mediaLayout->addWidget(m_videoWidget, 1);
    mediaLayout->addWidget(m_audioPlaceholder, 1);
    mediaLayout->addLayout(mediaCtrlLayout);
    m_stack->addWidget(m_mediaView);

    // 5. Metadata Table (Bottom half of preview)
    m_metadataContainer = new QWidget(this);
    QVBoxLayout* metaLayout = new QVBoxLayout(m_metadataContainer);
    metaLayout->setContentsMargins(0, 4, 0, 0);
    metaLayout->setSpacing(2);

    QLabel* lblMetaHeader = new QLabel("File Information", m_metadataContainer);
    lblMetaHeader->setStyleSheet("font-weight: bold; color: #89b4fa; border-bottom: 1px solid #313244; padding-bottom: 2px;");

    m_metadataTable = new QTableWidget(0, 2, m_metadataContainer);
    m_metadataTable->setHorizontalHeaderLabels({"Property", "Value"});
    m_metadataTable->horizontalHeader()->setVisible(false);
    m_metadataTable->verticalHeader()->setVisible(false);
    m_metadataTable->horizontalHeader()->setStretchLastSection(true);
    m_metadataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metadataTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_metadataTable->setShowGrid(false);
    m_metadataTable->setStyleSheet(
        "QTableWidget { background-color: transparent; border: none; }"
        "QTableWidget::item { padding: 3px 6px; border: none; }"
    );

    metaLayout->addWidget(lblMetaHeader);
    metaLayout->addWidget(m_metadataTable, 1);

    mainLayout->addWidget(m_stack, 2);
    mainLayout->addWidget(m_metadataContainer, 1);
}

void PreviewPanel::clearPreview() {
    m_player->stop();
    m_player->setSource(QUrl());
    m_previewedFilePath.clear();
    m_originalPixmap = QPixmap();
    m_imageLabel->clear();
    m_textEdit->clear();
    m_textChanged = false;
    m_textControls->hide();

    m_stack->setCurrentWidget(m_emptyView);

    m_metadataTable->setRowCount(0);
}

void PreviewPanel::previewFile(const QString& filePath) {
    if (!m_playlist.isEmpty() && m_playlistIndex >= 0 && m_playlistIndex < m_playlist.size()) {
        if (m_playlist[m_playlistIndex] != filePath) {
            m_playlist.clear();
            m_playlistIndex = -1;
        }
    }
    clearPreview();

    QFileInfo info(filePath);
    if (!info.exists() || info.isDir()) {
        return;
    }

    m_previewedFilePath = filePath;
    QString ext = info.suffix().toLower();

    // Check File Type
    QStringList txtExts = { "txt", "log", "ini", "cfg", "conf", "json", "xml", "html", "css", "js", 
                            "py", "cpp", "h", "sh", "md", "csv", "yml", "yaml", "properties" };
    QStringList imgExts = { "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg" };
    QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a" };
    QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm" };

    if (txtExts.contains(ext)) {
        showTextPreview(filePath);
    } else if (imgExts.contains(ext)) {
        showImagePreview(filePath);
    } else if (audioExts.contains(ext)) {
        showMediaPreview(filePath, false);
    } else if (videoExts.contains(ext)) {
        showMediaPreview(filePath, true);
    } else {
        // Unknown binary/other file - just show metadata
        m_stack->setCurrentWidget(m_emptyView);
    }

    // Load and show metadata
    FileMetadata meta = MetadataExtractor::extract(filePath);
    updateMetadataDisplay(meta);
}

void PreviewPanel::previewFolderArt(const QString& artPath, const QString& folderPath) {
    clearPreview();
    
    m_previewedFilePath = folderPath;
    
    // Show the art in the image display
    m_originalPixmap.load(artPath);
    if (!m_originalPixmap.isNull()) {
        scaleImage();
        m_stack->setCurrentWidget(m_imageView);
    } else {
        m_stack->setCurrentWidget(m_emptyView);
    }

    // Build Folder Metadata
    FileMetadata meta = MetadataExtractor::extract(folderPath);
    // Explicitly add image info about the artwork
    QImageReader reader(artPath);
    if (reader.canRead()) {
        meta.imageDimensions = reader.size();
        meta.imageFormat = QString("Folder Art (%1)").arg(QString::fromLocal8Bit(reader.format()).toUpper());
    }
    updateMetadataDisplay(meta);
}

void PreviewPanel::showTextPreview(const QString& filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Safeguard against loading massive files
        if (file.size() > 5 * 1024 * 1024) {
            m_textEdit->setPlainText("[File is too large to preview (>5MB)]");
            m_textEdit->setReadOnly(true);
        } else {
            QTextStream in(&file);
            m_textEdit->setPlainText(in.readAll());
            m_textEdit->setReadOnly(false);
        }
        file.close();
    }
    m_textChanged = false;
    m_textControls->hide();
    m_stack->setCurrentWidget(m_textView);
}

void PreviewPanel::showImagePreview(const QString& filePath) {
    m_originalPixmap.load(filePath);
    scaleImage();
    m_stack->setCurrentWidget(m_imageView);
}

void PreviewPanel::showMediaPreview(const QString& filePath, bool isVideo) {
    m_videoWidget->setVisible(isVideo);
    m_audioPlaceholder->setVisible(!isVideo);

    if (!isVideo) {
        m_audioPlaceholder->setText(QString("🎵 Playing Audio\n\n%1").arg(QFileInfo(filePath).fileName()));
    }

    m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_stack->setCurrentWidget(m_mediaView);

    m_player->setSource(QUrl::fromLocalFile(filePath));
    
    QSettings settings("Amifiles", "Amifiles");
    bool muted = settings.value("preview/muted", false).toBool();
    setMuted(muted);

    if (isVisible()) {
        m_player->play();
        m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    } else {
        m_player->stop();
        m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
}

void PreviewPanel::scaleImage() {
    if (m_originalPixmap.isNull()) return;
    
    // Scale image to fit the container scroll viewport size (minus standard margins)
    QSize viewSize = m_imageScrollArea->size();
    if (viewSize.width() < 50) viewSize.setWidth(200);
    if (viewSize.height() < 50) viewSize.setHeight(200);

    QPixmap scaled = m_originalPixmap.scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
}

void PreviewPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_stack->currentWidget() == m_imageView) {
        scaleImage();
    }
}

void PreviewPanel::onSaveText() {
    if (m_previewedFilePath.isEmpty()) return;

    QFile file(m_previewedFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_textEdit->toPlainText();
        file.close();
        
        m_textChanged = false;
        m_textControls->hide();
    }
}

void PreviewPanel::onTextChanged() {
    if (!m_textChanged && !m_textEdit->isReadOnly()) {
        m_textChanged = true;
        m_textControls->show();
    }
}

void PreviewPanel::onPlayPause() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        m_player->play();
        m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void PreviewPanel::onStop() {
    m_player->stop();
    m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_sliderProgress->setValue(0);
}

void PreviewPanel::onPositionChanged(qint64 position) {
    if (!m_sliderProgress->isSliderDown()) {
        m_sliderProgress->setValue(position);
    }
    m_lblProgressTime->setText(QString("%1 / %2")
                               .arg(formatDuration(position))
                               .arg(formatDuration(m_player->duration())));
}

void PreviewPanel::onDurationChanged(qint64 duration) {
    m_sliderProgress->setRange(0, duration);
    m_lblProgressTime->setText(QString("%1 / %2")
                               .arg(formatDuration(m_player->position()))
                               .arg(formatDuration(duration)));
}

void PreviewPanel::onVolumeChanged(int value) {
    m_audioOutput->setVolume(value / 100.0f);
}

void PreviewPanel::onSliderMoved(int value) {
    m_player->setPosition(value);
}

void PreviewPanel::onMediaMetadataChanged() {
    // Media player updated its metadata (useful for audio formats supported natively)
    QMediaMetaData meta = m_player->metaData();
    if (!meta.isEmpty()) {
        // We can extract tags and merge them into the table
        // Find existing metadata values or rewrite
        FileMetadata fileMeta = MetadataExtractor::extract(m_previewedFilePath);
        
        if (meta.value(QMediaMetaData::Title).isValid()) {
            fileMeta.title = meta.value(QMediaMetaData::Title).toString();
        }
        if (meta.value(QMediaMetaData::Author).isValid()) {
            fileMeta.artist = meta.value(QMediaMetaData::Author).toString();
        }
        if (meta.value(QMediaMetaData::AlbumTitle).isValid()) {
            fileMeta.album = meta.value(QMediaMetaData::AlbumTitle).toString();
        }
        
        updateMetadataDisplay(fileMeta);
    }
}

void PreviewPanel::updateMetadataDisplay(const FileMetadata& meta) {
    m_metadataTable->setRowCount(0);
    m_metadataTable->setColumnCount(2);
    m_metadataTable->setColumnWidth(0, 110);
    
    auto addMetaRow = [this](const QString& prop, const QString& val) {
        if (val.isEmpty()) return;
        int row = m_metadataTable->rowCount();
        m_metadataTable->insertRow(row);
        
        QTableWidgetItem* itemProp = new QTableWidgetItem(prop);
        itemProp->setForeground(QBrush(QColor("#a6adc8")));
        itemProp->setFont(QFont("", -1, QFont::Bold));
        
        QTableWidgetItem* itemVal = new QTableWidgetItem(val);
        itemVal->setForeground(QBrush(QColor("#cdd6f4")));
        
        m_metadataTable->setItem(row, 0, itemProp);
        m_metadataTable->setItem(row, 1, itemVal);
    };

    // Standard File Props
    QFileInfo info(meta.path);
    addMetaRow("File Name:", meta.name);
    addMetaRow("Folder:", info.absolutePath());
    
    // Format Size
    qint64 size = meta.size;
    QString sizeStr;
    if (size < 1024) {
        sizeStr = QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(QString::number(size / 1024.0, 'f', 1));
    } else {
        sizeStr = QString("%1 MB").arg(QString::number(size / (1024.0 * 1024.0), 'f', 1));
    }
    addMetaRow("Size:", sizeStr);
    addMetaRow("Permissions:", meta.permissions);
    addMetaRow("Modified:", meta.modified);
    addMetaRow("Created:", meta.created);

    // Image-specific
    if (meta.imageDimensions.isValid()) {
        addMetaRow("Dimensions:", QString("%1 x %2 pixels")
                   .arg(meta.imageDimensions.width())
                   .arg(meta.imageDimensions.height()));
        addMetaRow("Format:", meta.imageFormat);
    }

    // Audio-specific
    if (!meta.title.isEmpty() || !meta.artist.isEmpty() || !meta.album.isEmpty()) {
        addMetaRow("Title:", meta.title);
        addMetaRow("Artist:", meta.artist);
        addMetaRow("Album:", meta.album);
    }
    
    // Duration formatting
    qint64 durationMs = m_player->duration();
    if (durationMs > 0) {
        addMetaRow("Duration:", formatDuration(durationMs));
    }
}

QString PreviewPanel::formatDuration(qint64 ms) {
    qint64 totalSec = ms / 1000;
    qint64 min = totalSec / 60;
    qint64 sec = totalSec % 60;
    return QString("%1:%2")
        .arg(min, 2, 10, QChar('0'))
        .arg(sec, 2, 10, QChar('0'));
}

void PreviewPanel::setMuted(bool muted) {
    if (m_audioOutput) {
        m_audioOutput->setMuted(muted);
    }
}

bool PreviewPanel::isMuted() const {
    if (m_audioOutput) {
        return m_audioOutput->isMuted();
    }
    return false;
}

void PreviewPanel::playPlaylist(const QStringList& filePaths) {
    m_playlist = filePaths;
    m_playlistIndex = 0;
    
    if (m_playlist.isEmpty()) {
        clearPreview();
        return;
    }

    previewFile(m_playlist[0]);
    
    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(1).arg(m_playlist.size())));
}

void PreviewPanel::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_playlistIndex >= 0 && m_playlistIndex < m_playlist.size() - 1) {
            m_playlistIndex++;
            
            previewFile(m_playlist[m_playlistIndex]);
            
            int row = m_metadataTable->rowCount();
            m_metadataTable->insertRow(row);
            m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
            m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
        } else {
            m_playlist.clear();
            m_playlistIndex = -1;
        }
    }
}
