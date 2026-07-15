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
#include <QPainter>
#include <QTabWidget>
#include <QListWidget>
#include <QProcess>
#include <QTimer>
#include <QLinearGradient>
#include <QPolygon>
#include <QSizePolicy>

#include <QScreen>
#include <QKeyEvent>
#include <QMouseEvent>

FullscreenWidget::FullscreenWidget(QWidget* parent) : QWidget(parent, Qt::Window | Qt::FramelessWindowHint) {
    setStyleSheet("background-color: #000000;");
    setMouseTracking(true);
    installEventFilter(this);

    // Create HUD Overlay Panel
    m_hudWidget = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    m_hudWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_hudWidget->setObjectName("hudPanel");
    m_hudWidget->setStyleSheet(
        "QWidget#hudPanel { background-color: rgba(30, 30, 46, 220); border: 1px solid rgba(69, 71, 90, 150); border-radius: 12px; }"
        "QLabel { color: #cdd6f4; font-size: 12px; font-weight: bold; background: transparent; border: none; }"
        "QPushButton { border: none; background-color: transparent; color: #cdd6f4; padding: 4px; border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(137, 180, 250, 60); }"
        "QSlider::groove:horizontal { border: none; height: 6px; background: #313244; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #cdd6f4; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
    );

    QHBoxLayout* hudLayout = new QHBoxLayout(m_hudWidget);
    hudLayout->setContentsMargins(15, 10, 15, 10);
    hudLayout->setSpacing(10);

    QStyle* style = QApplication::style();

    // HUD buttons
    QPushButton* btnPrev = new QPushButton(m_hudWidget);
    btnPrev->setIcon(style->standardIcon(QStyle::SP_MediaSkipBackward));
    btnPrev->setToolTip("Previous");
    connect(btnPrev, &QPushButton::clicked, this, &FullscreenWidget::prevRequested);

    m_btnPlayPause = new QPushButton(m_hudWidget);
    m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    m_btnPlayPause->setToolTip("Play/Pause");
    connect(m_btnPlayPause, &QPushButton::clicked, this, &FullscreenWidget::onHudPlayPause);

    QPushButton* btnStop = new QPushButton(m_hudWidget);
    btnStop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    btnStop->setToolTip("Stop");
    connect(btnStop, &QPushButton::clicked, this, &FullscreenWidget::stopRequested);

    QPushButton* btnNext = new QPushButton(m_hudWidget);
    btnNext->setIcon(style->standardIcon(QStyle::SP_MediaSkipForward));
    btnNext->setToolTip("Next");
    connect(btnNext, &QPushButton::clicked, this, &FullscreenWidget::nextRequested);

    m_sliderProgress = new QSlider(Qt::Horizontal, m_hudWidget);
    m_sliderProgress->setRange(0, 100);
    connect(m_sliderProgress, &QSlider::sliderMoved, this, &FullscreenWidget::onHudSliderMoved);

    m_lblTime = new QLabel("00:00 / 00:00", m_hudWidget);

    QLabel* lblVol = new QLabel("🔊", m_hudWidget);

    m_sliderVolume = new QSlider(Qt::Horizontal, m_hudWidget);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(70);
    m_sliderVolume->setMaximumWidth(80);
    connect(m_sliderVolume, &QSlider::valueChanged, this, &FullscreenWidget::onHudVolumeChanged);

    QPushButton* btnExit = new QPushButton(m_hudWidget);
    btnExit->setIcon(style->standardIcon(QStyle::SP_TitleBarNormalButton));
    btnExit->setToolTip("Exit Fullscreen");
    connect(btnExit, &QPushButton::clicked, this, &FullscreenWidget::exitRequested);

    hudLayout->addWidget(btnPrev);
    hudLayout->addWidget(m_btnPlayPause);
    hudLayout->addWidget(btnStop);
    hudLayout->addWidget(btnNext);
    hudLayout->addWidget(m_sliderProgress, 1);
    hudLayout->addWidget(m_lblTime);
    hudLayout->addWidget(lblVol);
    hudLayout->addWidget(m_sliderVolume);
    hudLayout->addWidget(btnExit);

    // Position HUD at the bottom center of the screen
    m_hudWidget->resize(800, 50);

    // Auto-hide Timer (3 seconds)
    m_hideTimer = new QTimer(this);
    connect(m_hideTimer, &QTimer::timeout, this, &FullscreenWidget::onHideHud);

    m_lastMousePos = QCursor::pos();
    m_mousePollTimer = new QTimer(this);
    connect(m_mousePollTimer, &QTimer::timeout, this, &FullscreenWidget::onPollMouse);
    m_mousePollTimer->start(200);

    showHud();
}

void FullscreenWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    QScreen* scr = window()->screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    QRect screenGeom = scr->geometry();

    int hudW = qMin(screenGeom.width() - 40, 850);
    int x = screenGeom.x() + (screenGeom.width() - hudW) / 2;
    int y = screenGeom.y() + screenGeom.height() - 80;

    m_hudWidget->setGeometry(x, y, hudW, 50);
    m_hudWidget->raise();
}

void FullscreenWidget::setMediaState(bool isVideo, QMediaPlayer* player, QAudioOutput* audioOutput) {
    m_player = player;
    if (m_player) {
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_btnPlayPause->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
        } else {
            m_btnPlayPause->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
        }
    }
    if (audioOutput) {
        m_sliderVolume->setValue(qRound(audioOutput->volume() * 100));
    }
}

static QString formatTime(qint64 ms) {
    qint64 secs = ms / 1000;
    qint64 mins = secs / 60;
    secs = secs % 60;
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

void FullscreenWidget::updateProgress(qint64 position, qint64 duration) {
    if (duration > 0) {
        m_sliderProgress->setRange(0, duration);
        m_sliderProgress->setValue(position);
        m_lblTime->setText(QString("%1 / %2").arg(formatTime(position)).arg(formatTime(duration)));
    }
}

void FullscreenWidget::showHud() {
    QScreen* scr = window()->screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    QRect screenGeom = scr->geometry();

    int hudW = qMin(screenGeom.width() - 40, 850);
    int x = screenGeom.x() + (screenGeom.width() - hudW) / 2;
    int y = screenGeom.y() + screenGeom.height() - 80;

    m_hudWidget->setGeometry(x, y, hudW, 50);
    m_hudWidget->show();
    m_hudWidget->raise();
    setCursor(Qt::ArrowCursor);
    m_hideTimer->start(3000);
}

void FullscreenWidget::onPollMouse() {
    QPoint curPos = QCursor::pos();
    if (curPos != m_lastMousePos) {
        m_lastMousePos = curPos;
        showHud();
    }
}

void FullscreenWidget::onHideHud() {
    m_hudWidget->hide();
    setCursor(Qt::BlankCursor);
}

void FullscreenWidget::onHudPlayPause() {
    emit playPauseRequested();
    if (m_player) {
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_btnPlayPause->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
        } else {
            m_btnPlayPause->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
        }
    }
}

void FullscreenWidget::onHudSliderMoved(int val) {
    if (m_player) {
        m_player->setPosition(val);
    }
}

void FullscreenWidget::onHudVolumeChanged(int val) {
    if (m_player) {
        QAudioOutput* out = m_player->audioOutput();
        if (out) {
            out->setVolume(val / 100.0f);
        }
    }
}

bool FullscreenWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseMove) {
        showHud();
    }
    return QWidget::eventFilter(watched, event);
}

void FullscreenWidget::keyPressEvent(QKeyEvent* event) {
    showHud();
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_F) {
        emit exitRequested();
    } else if (event->key() == Qt::Key_Space) {
        emit playPauseRequested();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void FullscreenWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    emit exitRequested();
}

PreviewPanel::PreviewPanel(QWidget* parent) : QWidget(parent) {
    // Initialize QMediaPlayer and AudioOutput first so setupUI can configure them
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);

    setupUI();

    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    if (m_videoWidget) m_videoWidget->installEventFilter(this);
    if (m_audioPlaceholder) m_audioPlaceholder->installEventFilter(this);

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

bool PreviewPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_videoWidget || watched == m_audioPlaceholder) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            toggleFullscreen();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
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

    m_audioPlaceholder = new AudioPlaceholderWidget(m_mediaView);
    m_audioPlaceholder->setMinimumHeight(150);
    m_audioPlaceholder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    QSettings settings("Amifiles", "Amifiles");
    bool showAudioCover = settings.value("preview/show_audio_cover_art", true).toBool();
    m_audioPlaceholder->setCoverArtVisible(showAudioCover);

    // Media Controls Layout
    QHBoxLayout* mediaCtrlLayout = new QHBoxLayout();
    mediaCtrlLayout->setSpacing(4);

    QStyle* style = QApplication::style();

    m_btnPrevTrack = new QPushButton(this);
    m_btnPrevTrack->setIcon(style->standardIcon(QStyle::SP_MediaSkipBackward));
    m_btnPrevTrack->setToolTip("Previous Track");
    m_btnPrevTrack->setMaximumWidth(40);
    connect(m_btnPrevTrack, &QPushButton::clicked, this, &PreviewPanel::onPrevTrack);

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

    m_btnNextTrack = new QPushButton(this);
    m_btnNextTrack->setIcon(style->standardIcon(QStyle::SP_MediaSkipForward));
    m_btnNextTrack->setToolTip("Next Track");
    m_btnNextTrack->setMaximumWidth(40);
    connect(m_btnNextTrack, &QPushButton::clicked, this, &PreviewPanel::onNextTrack);

    m_btnFullscreen = new QPushButton(this);
    m_btnFullscreen->setIcon(style->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_btnFullscreen->setToolTip("Full Screen");
    m_btnFullscreen->setMaximumWidth(40);
    connect(m_btnFullscreen, &QPushButton::clicked, this, &PreviewPanel::toggleFullscreen);

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

    mediaCtrlLayout->addWidget(m_btnPrevTrack);
    mediaCtrlLayout->addWidget(m_btnPlayPause);
    mediaCtrlLayout->addWidget(m_btnStop);
    mediaCtrlLayout->addWidget(m_btnNextTrack);
    mediaCtrlLayout->addWidget(m_btnFullscreen);
    mediaCtrlLayout->addWidget(m_sliderProgress);
    mediaCtrlLayout->addWidget(m_lblProgressTime);
    mediaCtrlLayout->addWidget(lblVol);
    mediaCtrlLayout->addWidget(m_sliderVolume);

    mediaLayout->addWidget(m_videoWidget, 1);
    mediaLayout->addWidget(m_audioPlaceholder, 1);
    mediaLayout->addLayout(mediaCtrlLayout);
    m_stack->addWidget(m_mediaView);

    // 5. Bottom Tabbed Area
    m_bottomTab = new QTabWidget(this);
    m_bottomTab->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: transparent; border-radius: 4px; }"
        "QTabBar::tab { background-color: #11111b; color: #a6adc8; padding: 6px 12px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #313244; color: #cdd6f4; }"
    );

    // Tab 1: Metadata Container
    m_metadataContainer = new QWidget(this);
    QVBoxLayout* metaLayout = new QVBoxLayout(m_metadataContainer);
    metaLayout->setContentsMargins(4, 4, 4, 4);
    metaLayout->setSpacing(2);

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
    metaLayout->addWidget(m_metadataTable, 1);
    m_bottomTab->addTab(m_metadataContainer, "Properties");

    // Tab 2: Playlist Queue
    m_playlistList = new QListWidget(this);
    m_playlistList->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; color: #cdd6f4; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background-color: #313244; color: #a6e3a1; }"
    );
    connect(m_playlistList, &QListWidget::itemDoubleClicked, this, &PreviewPanel::onPlaylistItemDoubleClicked);
    m_bottomTab->addTab(m_playlistList, "Playlist Queue");

    mainLayout->addWidget(m_stack, 2);
    mainLayout->addWidget(m_bottomTab, 1);
}

void PreviewPanel::clearPreview() {
    m_player->stop();
    m_player->setSource(QUrl());
    m_previewedFilePath.clear();
    m_currentAudioPath.clear();
    m_originalPixmap = QPixmap();
    m_imageLabel->clear();
    m_textEdit->clear();
    m_textChanged = false;
    m_textControls->hide();
    if (m_audioPlaceholder) {
        m_audioPlaceholder->setFilePath("");
    }

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

    if (m_fullscreenWidget) {
        exitFullscreen();
        toggleFullscreen();
    }
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
        m_currentAudioPath = filePath;
        updateAudioPlaceholder(filePath);
    } else {
        m_currentAudioPath.clear();
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
    if (!m_currentAudioPath.isEmpty()) {
        updateAudioPlaceholder(m_currentAudioPath);
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
    if (m_fullscreenWidget) {
        m_fullscreenWidget->setMediaState(m_videoWidget->isVisible(), m_player, m_audioOutput);
    }
}

void PreviewPanel::onStop() {
    m_player->stop();
    m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_sliderProgress->setValue(0);
    if (m_fullscreenWidget) {
        m_fullscreenWidget->setMediaState(m_videoWidget->isVisible(), m_player, m_audioOutput);
    }
}

void PreviewPanel::onPositionChanged(qint64 position) {
    if (!m_sliderProgress->isSliderDown()) {
        m_sliderProgress->setValue(position);
    }
    m_lblProgressTime->setText(QString("%1 / %2")
                               .arg(formatDuration(position))
                               .arg(formatDuration(m_player->duration())));
    if (m_fullscreenWidget) {
        m_fullscreenWidget->updateProgress(position, m_player->duration());
    }
}

void PreviewPanel::onDurationChanged(qint64 duration) {
    m_sliderProgress->setRange(0, duration);
    m_lblProgressTime->setText(QString("%1 / %2")
                               .arg(formatDuration(m_player->position()))
                               .arg(formatDuration(duration)));
    if (m_fullscreenWidget) {
        m_fullscreenWidget->updateProgress(m_player->position(), duration);
    }
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

void PreviewPanel::setAudioCoverArtVisible(bool visible) {
    if (m_audioPlaceholder) {
        m_audioPlaceholder->setCoverArtVisible(visible);
    }
}

void PreviewPanel::playPlaylist(const QStringList& filePaths) {
    m_playlist = filePaths;
    m_playlistIndex = 0;
    
    m_playlistList->clear();
    for (const QString& path : m_playlist) {
        m_playlistList->addItem(QFileInfo(path).fileName());
    }

    if (m_playlist.isEmpty()) {
        clearPreview();
        return;
    }

    previewFile(m_playlist[0]);
    m_playlistList->setCurrentRow(0);
    m_bottomTab->setCurrentIndex(1); // Switch to Playlist Queue tab

    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(1).arg(m_playlist.size())));
}

void PreviewPanel::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        onNextTrack();
    }
}

void PreviewPanel::onPrevTrack() {
    if (m_playlist.isEmpty()) return;
    if (m_playlistIndex > 0) {
        m_playlistIndex--;
        previewFile(m_playlist[m_playlistIndex]);
        m_playlistList->setCurrentRow(m_playlistIndex);

        int row = m_metadataTable->rowCount();
        m_metadataTable->insertRow(row);
        m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
        m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    }
}

void PreviewPanel::onNextTrack() {
    if (m_playlist.isEmpty()) return;
    if (m_playlistIndex < m_playlist.size() - 1) {
        m_playlistIndex++;
        previewFile(m_playlist[m_playlistIndex]);
        m_playlistList->setCurrentRow(m_playlistIndex);

        int row = m_metadataTable->rowCount();
        m_metadataTable->insertRow(row);
        m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
        m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    } else {
        m_playlistIndex = 0;
        previewFile(m_playlist[0]);
        m_playlistList->setCurrentRow(0);
    }
}

void PreviewPanel::onPlaylistItemDoubleClicked(QListWidgetItem* item) {
    int row = m_playlistList->row(item);
    if (row >= 0 && row < m_playlist.size()) {
        m_playlistIndex = row;
        previewFile(m_playlist[m_playlistIndex]);
        m_playlistList->setCurrentRow(m_playlistIndex);

        int r = m_metadataTable->rowCount();
        m_metadataTable->insertRow(r);
        m_metadataTable->setItem(r, 0, new QTableWidgetItem("Playlist Status"));
        m_metadataTable->setItem(r, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    }
}

void PreviewPanel::updateAudioPlaceholder(const QString& filePath) {
    if (m_audioPlaceholder) {
        m_audioPlaceholder->setFilePath(filePath);
    }
}

// ================= AudioPlaceholderWidget =================

AudioPlaceholderWidget::AudioPlaceholderWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
}

void AudioPlaceholderWidget::setFilePath(const QString& filePath) {
    m_filePath = filePath;
    update();
}

void AudioPlaceholderWidget::setCoverArtVisible(bool visible) {
    m_coverArtVisible = visible;
    update();
}

void AudioPlaceholderWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();
    painter.fillRect(r, QColor("#1e1e2e"));

    if (m_filePath.isEmpty()) {
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(r, Qt::AlignCenter, "Select an audio track to play");
        return;
    }

    QString artPath;
    if (m_coverArtVisible) {
        QString dirPath = QFileInfo(m_filePath).absolutePath();
        QDir dir(dirPath);
        QStringList artNames = { "folder", "cover", "album", "poster" };
        QStringList artExts = { "jpg", "jpeg", "png", "webp" };
        for (const QString& name : artNames) {
            for (const QString& ext : artExts) {
                QString path = dir.filePath(name + "." + ext);
                if (QFile::exists(path)) {
                    artPath = path;
                    break;
                }
            }
            if (!artPath.isEmpty()) break;
        }
    }

    if (!artPath.isEmpty()) {
        QPixmap cover(artPath);
        if (!cover.isNull()) {
            QPixmap scaledCover = cover.scaled(r.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int x = (r.width() - scaledCover.width()) / 2;
            int y = (r.height() - scaledCover.height()) / 2;
            painter.drawPixmap(x, y, scaledCover);
            painter.fillRect(r, QColor(30, 30, 46, 200));
        }
    }

    painter.setPen(QColor("#cdd6f4"));
    QFont f = font();
    f.setPointSize(12);
    f.setBold(true);
    painter.setFont(f);

    QString text = QString("🎵 Playing Audio\n\n%1").arg(QFileInfo(m_filePath).fileName());
    painter.drawText(r.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void PreviewPanel::toggleFullscreen() {
    if (m_fullscreenWidget) {
        exitFullscreen();
        return;
    }

    QString activePath = !m_currentAudioPath.isEmpty() ? m_currentAudioPath : m_previewedFilePath;
    if (activePath.isEmpty()) return;

    bool isVideo = m_videoWidget->isVisible();

    // Create the borderless fullscreen widget
    m_fullscreenWidget = new FullscreenWidget();
    connect(m_fullscreenWidget, &FullscreenWidget::exitRequested, this, &PreviewPanel::exitFullscreen);
    connect(m_fullscreenWidget, &FullscreenWidget::prevRequested, this, &PreviewPanel::onPrevTrack);
    connect(m_fullscreenWidget, &FullscreenWidget::playPauseRequested, this, &PreviewPanel::onPlayPause);
    connect(m_fullscreenWidget, &FullscreenWidget::stopRequested, this, &PreviewPanel::onStop);
    connect(m_fullscreenWidget, &FullscreenWidget::nextRequested, this, &PreviewPanel::onNextTrack);

    QVBoxLayout* layout = new QVBoxLayout(m_fullscreenWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    if (isVideo) {
        m_fullscreenVideoWidget = new QVideoWidget(m_fullscreenWidget);
        m_fullscreenVideoWidget->setStyleSheet("background-color: #000000;");
        layout->addWidget(m_fullscreenVideoWidget);

        m_player->setVideoOutput(m_fullscreenVideoWidget);
        m_fullscreenVideoWidget->setMouseTracking(true);
        m_fullscreenVideoWidget->installEventFilter(m_fullscreenWidget);
    } else {
        m_fullscreenAudioLabel = new QLabel(m_fullscreenWidget);
        m_fullscreenAudioLabel->setAlignment(Qt::AlignCenter);

        // Load and render cover art in large format
        QPixmap cover;
        QProcess proc;
        proc.start("exiftool", {"-Picture", "-b", activePath});
        if (proc.waitForFinished(5000)) {
            QByteArray imgData = proc.readAllStandardOutput();
            if (!imgData.isEmpty()) {
                cover.loadFromData(imgData);
            }
        }
        
        if (cover.isNull()) {
            QDir dir(QFileInfo(activePath).absolutePath());
            QStringList coverNames = {"folder.jpg", "folder.png", "cover.jpg", "cover.png", "album.jpg", "album.png"};
            for (const QString& name : coverNames) {
                QString path = dir.filePath(name);
                if (QFile::exists(path)) {
                    cover.load(path);
                    break;
                }
            }
        }

        if (cover.isNull()) {
            cover = QPixmap(512, 512);
            cover.fill(QColor("#11111b"));
            QPainter painter(&cover);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QBrush(QColor("#313244")));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(16, 16, 480, 480, 64, 64);
            painter.setPen(QPen(QColor("#cdd6f4"), 4));
            QFont font("Outfit", 90, QFont::Bold);
            painter.setFont(font);
            painter.drawText(QRect(16, 16, 480, 480), Qt::AlignCenter, "🎵");
        }

        int screenH = QGuiApplication::primaryScreen()->geometry().height();
        int size = qMin(600, screenH - 250);
        m_fullscreenAudioLabel->setPixmap(cover.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(m_fullscreenAudioLabel);

        m_fullscreenTextLabel = new QLabel(m_fullscreenWidget);
        m_fullscreenTextLabel->setAlignment(Qt::AlignCenter);
        m_fullscreenTextLabel->setStyleSheet("color: #cdd6f4; font-size: 24px; font-weight: bold; padding: 20px;");
        
        FileMetadata meta = MetadataExtractor::extract(activePath);
        QString displayTitle = !meta.title.isEmpty() ? meta.title : QFileInfo(activePath).completeBaseName();
        QString displayArtist = !meta.artist.isEmpty() ? meta.artist : "Unknown Artist";
        m_fullscreenTextLabel->setText(QString("%1\n%2").arg(displayTitle).arg(displayArtist));
        layout->addWidget(m_fullscreenTextLabel);

        m_fullscreenAudioLabel->setMouseTracking(true);
        m_fullscreenTextLabel->setMouseTracking(true);
        m_fullscreenAudioLabel->installEventFilter(m_fullscreenWidget);
        m_fullscreenTextLabel->installEventFilter(m_fullscreenWidget);
    }

    m_fullscreenWidget->setMediaState(isVideo, m_player, m_audioOutput);
    m_fullscreenWidget->updateProgress(m_player->position(), m_player->duration());
    m_fullscreenWidget->showFullScreen();
}

void PreviewPanel::exitFullscreen() {
    if (!m_fullscreenWidget) return;

    m_player->setVideoOutput(m_videoWidget);

    m_fullscreenWidget->close();
    m_fullscreenWidget->deleteLater();
    m_fullscreenWidget = nullptr;
    m_fullscreenVideoWidget = nullptr;
    m_fullscreenAudioLabel = nullptr;
    m_fullscreenTextLabel = nullptr;
}
