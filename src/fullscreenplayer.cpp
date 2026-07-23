#include "fullscreenplayer.h"
#include <QApplication>
#include <QScreen>
#include <QTime>
#include <QStyle>

FullscreenPlayer::FullscreenPlayer(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_videoWidget(new QVideoWidget(this))
    , m_topBar(nullptr)
    , m_bottomBar(nullptr)
    , m_queueDrawer(nullptr)
    , m_queueListWidget(nullptr)
    , m_lblTitle(nullptr)
    , m_lblTime(nullptr)
    , m_seekSlider(nullptr)
    , m_volumeSlider(nullptr)
    , m_btnPlayPause(nullptr)
    , m_btnPrev(nullptr)
    , m_btnNext(nullptr)
    , m_btnClose(nullptr)
    , m_btnQueue(nullptr)
    , m_comboSubtitles(nullptr)
    , m_overlayTimer(new QTimer(this))
    , m_currentIndex(-1)
    , m_durationMs(0)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);

    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    connect(m_player, &QMediaPlayer::positionChanged, this, &FullscreenPlayer::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &FullscreenPlayer::onDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &FullscreenPlayer::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &FullscreenPlayer::onMediaStatusChanged);

    m_overlayTimer->setInterval(3500);
    m_overlayTimer->setSingleShot(true);
    connect(m_overlayTimer, &QTimer::timeout, this, &FullscreenPlayer::hideOverlayTimerTimeout);

    buildUi();
}

FullscreenPlayer::~FullscreenPlayer() {
    stopPlayback();
}

void FullscreenPlayer::buildUi() {
    // Main layout containing the video widget and overlays
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QStackedWidget* stack = new QStackedWidget(this);
    stack->setMouseTracking(true);

    QWidget* playerContainer = new QWidget(stack);
    playerContainer->setMouseTracking(true);
    QVBoxLayout* containerLayout = new QVBoxLayout(playerContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    containerLayout->addWidget(m_videoWidget);

    // Build Top Overlay Bar
    m_topBar = new QWidget(this);
    m_topBar->setMouseTracking(true);
    m_topBar->setStyleSheet(
        "QWidget#topBar { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(10, 12, 18, 0.92), stop:1 rgba(10, 12, 18, 0.0)); "
        "}"
    );
    m_topBar->setObjectName("topBar");
    QHBoxLayout* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(24, 16, 24, 16);

    m_lblTitle = new QLabel("Amifiles Fullscreen Player", m_topBar);
    m_lblTitle->setStyleSheet("color: #f5c2e7; font-size: 18px; font-weight: bold; font-family: 'Segoe UI', sans-serif;");
    
    m_btnClose = new QPushButton("✕ Exit Fullscreen", m_topBar);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet(
        "QPushButton { background: rgba(243, 139, 168, 0.2); color: #f38ba8; border: 1px solid rgba(243, 139, 168, 0.4); "
        "  border-radius: 6px; padding: 8px 16px; font-weight: bold; } "
        "QPushButton:hover { background: rgba(243, 139, 168, 0.4); color: #ffffff; }"
    );
    connect(m_btnClose, &QPushButton::clicked, this, &FullscreenPlayer::closePlayer);

    topLayout->addWidget(m_lblTitle, 1);
    topLayout->addWidget(m_btnClose, 0);

    // Build Bottom Overlay Bar
    m_bottomBar = new QWidget(this);
    m_bottomBar->setMouseTracking(true);
    m_bottomBar->setObjectName("bottomBar");
    m_bottomBar->setStyleSheet(
        "QWidget#bottomBar { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(10, 12, 18, 0.0), stop:1 rgba(10, 12, 18, 0.95)); "
        "}"
    );

    QVBoxLayout* bottomLayout = new QVBoxLayout(m_bottomBar);
    bottomLayout->setContentsMargins(24, 12, 24, 20);
    bottomLayout->setSpacing(10);

    // Seek Slider & Time
    QHBoxLayout* seekLayout = new QHBoxLayout();
    m_seekSlider = new QSlider(Qt::Horizontal, m_bottomBar);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setCursor(Qt::PointingHandCursor);
    m_seekSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: rgba(255, 255, 255, 0.25); border-radius: 3px; } "
        "QSlider::sub-page:horizontal { background: #cba6f7; border-radius: 3px; } "
        "QSlider::handle:horizontal { background: #ffffff; border: 2px solid #cba6f7; width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; }"
    );
    connect(m_seekSlider, &QSlider::sliderMoved, this, &FullscreenPlayer::onSliderSeek);

    m_lblTime = new QLabel("00:00 / 00:00", m_bottomBar);
    m_lblTime->setStyleSheet("color: #a6adc8; font-size: 13px; font-family: monospace; font-weight: bold;");

    seekLayout->addWidget(m_seekSlider, 1);
    seekLayout->addWidget(m_lblTime, 0);
    bottomLayout->addLayout(seekLayout);

    // Controls Row
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(16);

    QStyle* style = QApplication::style();

    m_btnPrev = new QPushButton(m_bottomBar);
    m_btnPrev->setIcon(style->standardIcon(QStyle::SP_MediaSkipBackward));
    m_btnPrev->setIconSize(QSize(22, 22));
    m_btnPrev->setCursor(Qt::PointingHandCursor);
    m_btnPrev->setToolTip("Previous Track (P)");
    m_btnPrev->setStyleSheet("QPushButton { background: rgba(255, 255, 255, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 20px; min-width: 40px; min-height: 40px; } QPushButton:hover { background: rgba(255, 255, 255, 0.25); }");
    connect(m_btnPrev, &QPushButton::clicked, this, &FullscreenPlayer::playPrevious);

    m_btnPlayPause = new QPushButton(m_bottomBar);
    m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    m_btnPlayPause->setIconSize(QSize(28, 28));
    m_btnPlayPause->setCursor(Qt::PointingHandCursor);
    m_btnPlayPause->setToolTip("Play / Pause (Space)");
    m_btnPlayPause->setStyleSheet("QPushButton { background: #cba6f7; color: #11111b; border: none; border-radius: 25px; min-width: 50px; min-height: 50px; } QPushButton:hover { background: #b4befe; }");
    connect(m_btnPlayPause, &QPushButton::clicked, this, &FullscreenPlayer::togglePlayPause);

    m_btnNext = new QPushButton(m_bottomBar);
    m_btnNext->setIcon(style->standardIcon(QStyle::SP_MediaSkipForward));
    m_btnNext->setIconSize(QSize(22, 22));
    m_btnNext->setCursor(Qt::PointingHandCursor);
    m_btnNext->setToolTip("Next Track (N)");
    m_btnNext->setStyleSheet("QPushButton { background: rgba(255, 255, 255, 0.1); border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 20px; min-width: 40px; min-height: 40px; } QPushButton:hover { background: rgba(255, 255, 255, 0.25); }");
    connect(m_btnNext, &QPushButton::clicked, this, &FullscreenPlayer::playNext);

    controlsLayout->addWidget(m_btnPrev);
    controlsLayout->addWidget(m_btnPlayPause);
    controlsLayout->addWidget(m_btnNext);
    controlsLayout->addSpacing(20);

    // Volume
    QLabel* lblVolIcon = new QLabel("🔊", m_bottomBar);
    lblVolIcon->setStyleSheet("color: #cdd6f4; font-size: 16px;");
    m_volumeSlider = new QSlider(Qt::Horizontal, m_bottomBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(100);
    m_volumeSlider->setCursor(Qt::PointingHandCursor);
    m_volumeSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: rgba(255, 255, 255, 0.2); border-radius: 2px; } "
        "QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 2px; } "
        "QSlider::handle:horizontal { background: #ffffff; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
    );
    m_audioOutput->setVolume(0.80f);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &FullscreenPlayer::onVolumeChanged);

    controlsLayout->addWidget(lblVolIcon);
    controlsLayout->addWidget(m_volumeSlider);
    controlsLayout->addStretch(1);

    // Subtitle Selector
    m_comboSubtitles = new QComboBox(m_bottomBar);
    m_comboSubtitles->addItem("💬 Subtitles: Off");
    m_comboSubtitles->setCursor(Qt::PointingHandCursor);
    m_comboSubtitles->setStyleSheet(
        "QComboBox { background: rgba(30, 30, 46, 0.85); color: #cdd6f4; border: 1px solid rgba(255, 255, 255, 0.2); "
        "  border-radius: 6px; padding: 6px 12px; font-size: 13px; } "
        "QComboBox::drop-down { border: none; } QComboBox QAbstractItemView { background: #1e1e2e; color: #cdd6f4; selection-background-color: #313244; }"
    );
    connect(m_comboSubtitles, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FullscreenPlayer::onSubtitleSelected);
    controlsLayout->addWidget(m_comboSubtitles);

    // Queue Drawer Button
    m_btnQueue = new QPushButton("≡ Playback Queue (0)", m_bottomBar);
    m_btnQueue->setCursor(Qt::PointingHandCursor);
    m_btnQueue->setStyleSheet(
        "QPushButton { background: rgba(137, 180, 250, 0.2); color: #89b4fa; border: 1px solid rgba(137, 180, 250, 0.4); "
        "  border-radius: 6px; padding: 6px 14px; font-weight: bold; } "
        "QPushButton:hover { background: rgba(137, 180, 250, 0.4); color: #ffffff; }"
    );
    connect(m_btnQueue, &QPushButton::clicked, this, &FullscreenPlayer::toggleQueueDrawer);
    controlsLayout->addWidget(m_btnQueue);

    bottomLayout->addLayout(controlsLayout);

    // Build Queue Drawer Panel (Right overlay)
    m_queueDrawer = new QWidget(this);
    m_queueDrawer->setFixedWidth(340);
    m_queueDrawer->setStyleSheet(
        "QWidget#queueDrawer { background: rgba(17, 17, 27, 0.94); border-left: 1px solid rgba(255, 255, 255, 0.15); }"
    );
    m_queueDrawer->setObjectName("queueDrawer");
    m_queueDrawer->hide();

    QVBoxLayout* drawerLayout = new QVBoxLayout(m_queueDrawer);
    drawerLayout->setContentsMargins(16, 20, 16, 20);
    
    QLabel* lblQueueTitle = new QLabel("📋 Fullscreen Playback Queue", m_queueDrawer);
    lblQueueTitle->setStyleSheet("color: #89b4fa; font-size: 15px; font-weight: bold; margin-bottom: 8px;");
    drawerLayout->addWidget(lblQueueTitle);

    m_queueListWidget = new QListWidget(m_queueDrawer);
    m_queueListWidget->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: #cdd6f4; font-size: 13px; } "
        "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid rgba(255, 255, 255, 0.05); border-radius: 4px; } "
        "QListWidget::item:hover { background: rgba(255, 255, 255, 0.1); } "
        "QListWidget::item:selected { background: #313244; color: #a6e3a1; font-weight: bold; }"
    );
    connect(m_queueListWidget, &QListWidget::itemDoubleClicked, this, &FullscreenPlayer::onQueueItemDoubleClicked);
    drawerLayout->addWidget(m_queueListWidget, 1);

    QPushButton* btnClearQueue = new QPushButton("Clear Queue", m_queueDrawer);
    btnClearQueue->setStyleSheet("QPushButton { background: rgba(243, 139, 168, 0.2); color: #f38ba8; border: 1px solid rgba(243, 139, 168, 0.3); border-radius: 4px; padding: 6px; } QPushButton:hover { background: rgba(243, 139, 168, 0.4); }");
    connect(btnClearQueue, &QPushButton::clicked, this, [this]() {
        m_playlist.clear();
        m_currentIndex = -1;
        updateQueueWidget();
        stopPlayback();
    });
    drawerLayout->addWidget(btnClearQueue);

    stack->addWidget(playerContainer);
    rootLayout->addWidget(stack);
}

bool FullscreenPlayer::isPlaying() const {
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

void FullscreenPlayer::playMedia(const QString& filePath) {
    playPlaylist(QStringList{ filePath });
}

void FullscreenPlayer::playPlaylist(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;
    m_playlist = filePaths;
    m_currentIndex = 0;
    updateQueueWidget();

    showFullScreen();
    raise();
    activateWindow();

    m_player->setSource(QUrl::fromLocalFile(m_playlist.at(m_currentIndex)));
    m_player->play();
    updateTitleLabel();
    resetOverlayTimer();
}

void FullscreenPlayer::enqueueMedia(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;
    bool wasEmpty = m_playlist.isEmpty();
    m_playlist.append(filePaths);
    updateQueueWidget();

    if (wasEmpty || m_player->playbackState() == QMediaPlayer::StoppedState) {
        playPlaylist(m_playlist);
    } else {
        emit queueChanged(m_playlist.size());
    }
}

void FullscreenPlayer::togglePlayPause() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void FullscreenPlayer::playNext() {
    if (m_playlist.isEmpty()) return;
    if (m_currentIndex + 1 < m_playlist.size()) {
        m_currentIndex++;
        m_player->setSource(QUrl::fromLocalFile(m_playlist.at(m_currentIndex)));
        m_player->play();
        updateTitleLabel();
        updateQueueWidget();
    }
}

void FullscreenPlayer::playPrevious() {
    if (m_playlist.isEmpty()) return;
    if (m_currentIndex > 0) {
        m_currentIndex--;
        m_player->setSource(QUrl::fromLocalFile(m_playlist.at(m_currentIndex)));
        m_player->play();
        updateTitleLabel();
        updateQueueWidget();
    }
}

void FullscreenPlayer::stopPlayback() {
    m_player->stop();
}

void FullscreenPlayer::closePlayer() {
    stopPlayback();
    hide();
    emit playbackEnded();
}

void FullscreenPlayer::onPositionChanged(qint64 position) {
    if (m_durationMs > 0 && !m_seekSlider->isSliderDown()) {
        int sliderVal = static_cast<int>((position * 1000) / m_durationMs);
        m_seekSlider->setValue(sliderVal);
    }
    m_lblTime->setText(QString("%1 / %2").arg(formatTime(position), formatTime(m_durationMs)));
}

void FullscreenPlayer::onDurationChanged(qint64 duration) {
    m_durationMs = duration;
}

void FullscreenPlayer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    QStyle* style = QApplication::style();
    if (state == QMediaPlayer::PlayingState) {
        m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPause));
    } else {
        m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    }
}

void FullscreenPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_currentIndex + 1 < m_playlist.size()) {
            playNext();
        } else {
            closePlayer();
        }
    }
}

void FullscreenPlayer::onSliderSeek(int value) {
    if (m_durationMs > 0) {
        qint64 targetPos = (static_cast<qint64>(value) * m_durationMs) / 1000;
        m_player->setPosition(targetPos);
    }
}

void FullscreenPlayer::onVolumeChanged(int value) {
    float vol = value / 100.0f;
    m_audioOutput->setVolume(vol);
}

void FullscreenPlayer::onSubtitleSelected(int index) {
    Q_UNUSED(index);
    // Subtitle track selection (if available via QMediaPlayer subtitle tracks)
}

void FullscreenPlayer::toggleQueueDrawer() {
    if (m_queueDrawer->isVisible()) {
        m_queueDrawer->hide();
    } else {
        m_queueDrawer->show();
        m_queueDrawer->raise();
    }
    resetOverlayTimer();
}

void FullscreenPlayer::onQueueItemDoubleClicked(QListWidgetItem* item) {
    int row = m_queueListWidget->row(item);
    if (row >= 0 && row < m_playlist.size()) {
        m_currentIndex = row;
        m_player->setSource(QUrl::fromLocalFile(m_playlist.at(m_currentIndex)));
        m_player->play();
        updateTitleLabel();
        updateQueueWidget();
    }
}

void FullscreenPlayer::resetOverlayTimer() {
    m_topBar->show();
    m_bottomBar->show();
    setCursor(Qt::ArrowCursor);
    m_overlayTimer->start();
}

void FullscreenPlayer::hideOverlayTimerTimeout() {
    if (!m_queueDrawer->isVisible() && isPlaying()) {
        m_topBar->hide();
        m_bottomBar->hide();
        setCursor(Qt::BlankCursor);
    }
}

void FullscreenPlayer::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
    case Qt::Key_F11:
        closePlayer();
        break;
    case Qt::Key_Space:
        togglePlayPause();
        resetOverlayTimer();
        break;
    case Qt::Key_Right:
        m_player->setPosition(m_player->position() + 5000);
        resetOverlayTimer();
        break;
    case Qt::Key_Left:
        m_player->setPosition(qMax<qint64>(0, m_player->position() - 5000));
        resetOverlayTimer();
        break;
    case Qt::Key_Up:
        m_volumeSlider->setValue(qMin(100, m_volumeSlider->value() + 5));
        resetOverlayTimer();
        break;
    case Qt::Key_Down:
        m_volumeSlider->setValue(qMax(0, m_volumeSlider->value() - 5));
        resetOverlayTimer();
        break;
    case Qt::Key_N:
        playNext();
        resetOverlayTimer();
        break;
    case Qt::Key_P:
        playPrevious();
        resetOverlayTimer();
        break;
    default:
        QWidget::keyPressEvent(event);

    }
}

void FullscreenPlayer::mouseMoveEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    resetOverlayTimer();
}

void FullscreenPlayer::mousePressEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    resetOverlayTimer();
}

void FullscreenPlayer::updateTitleLabel() {
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        QFileInfo fi(m_playlist.at(m_currentIndex));
        m_lblTitle->setText(QString("🎬 %1 (%2 of %3)")
            .arg(fi.fileName())
            .arg(m_currentIndex + 1)
            .arg(m_playlist.size()));
    } else {
        m_lblTitle->setText("Amifiles Fullscreen Player");
    }
}

void FullscreenPlayer::updateQueueWidget() {
    m_queueListWidget->clear();
    for (int i = 0; i < m_playlist.size(); ++i) {
        QFileInfo fi(m_playlist.at(i));
        QString prefix = (i == m_currentIndex) ? "▶ " : QString("%1. ").arg(i + 1);
        QListWidgetItem* item = new QListWidgetItem(prefix + fi.fileName(), m_queueListWidget);
        if (i == m_currentIndex) {
            item->setSelected(true);
        }
    }
    m_btnQueue->setText(QString("≡ Playback Queue (%1)").arg(m_playlist.size()));
    emit queueChanged(m_playlist.size());
}

QString FullscreenPlayer::formatTime(qint64 ms) const {
    qint64 seconds = (ms / 1000) % 60;
    qint64 minutes = (ms / (1000 * 60)) % 60;
    qint64 hours = (ms / (1000 * 60 * 60));

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
