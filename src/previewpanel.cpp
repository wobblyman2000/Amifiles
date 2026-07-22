#include "previewpanel.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QPainterPath>
#include <QLineEdit>
#include <QFormLayout>
#include <QCompleter>
#include <QStringListModel>
#include "tagmanager.h"
#include "imageeditordialog.h"
#include "fullscreenimageviewer.h"
#include "hexeditorwidget.h"
#include "pdfviewerwidget.h"
#include "iconpickerdialog.h"
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDirIterator>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QTableWidgetItem>
#include <QPainter>
#include <QTabWidget>
#include <QTabBar>
#include <QListWidget>
#include <QMenu>
#include <QContextMenuEvent>
#include <QAction>
#include <QProcess>
#include <QTimer>
#include <QLinearGradient>
#include <QPolygon>
#include <QSizePolicy>
#include <QRandomGenerator>

#include <QScreen>
#include <QKeyEvent>
#include <QMouseEvent>

static QIcon createShuffleIcon(const QColor& color) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    
    QPainterPath path1;
    path1.moveTo(4, 6);
    path1.lineTo(10, 6);
    path1.lineTo(14, 18);
    path1.lineTo(20, 18);
    p.drawPath(path1);
    
    p.setBrush(color);
    QPolygonF head1;
    head1 << QPointF(20, 15) << QPointF(20, 21) << QPointF(23, 18);
    p.drawPolygon(head1);
    
    QPainterPath path2;
    path2.moveTo(4, 18);
    path2.lineTo(10, 18);
    path2.lineTo(14, 6);
    path2.lineTo(20, 6);
    p.drawPath(path2);
    
    QPolygonF head2;
    head2 << QPointF(20, 3) << QPointF(20, 9) << QPointF(23, 6);
    p.drawPolygon(head2);
    
    p.end();
    return QIcon(pix);
}

static QIcon createRepeatIcon(const QColor& color, bool repeatOne) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    p.drawLine(8, 7, 16, 7);
    p.drawArc(12, 7, 8, 10, -90 * 16, 180 * 16);
    p.drawLine(8, 17, 16, 17);
    p.drawArc(4, 7, 8, 10, 90 * 16, 180 * 16);
    
    p.setBrush(color);
    QPolygonF head1;
    head1 << QPointF(8, 4) << QPointF(8, 10) << QPointF(11, 7);
    p.drawPolygon(head1);
    
    QPolygonF head2;
    head2 << QPointF(16, 14) << QPointF(16, 20) << QPointF(13, 17);
    p.drawPolygon(head2);
    
    if (repeatOne) {
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(color);
        p.drawLine(11, 11, 12, 10);
        p.drawLine(12, 10, 12, 14);
        p.drawLine(10, 14, 14, 14);
    }
    
    p.end();
    return QIcon(pix);
}

static QIcon createAutoFSIcon(const QColor& color) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    p.drawRoundedRect(3, 4, 18, 12, 2, 2);
    p.drawLine(10, 16, 8, 20);
    p.drawLine(14, 16, 16, 20);
    p.drawLine(8, 20, 16, 20);
    
    p.setBrush(color);
    QPolygonF arrow;
    arrow << QPointF(10, 8) << QPointF(10, 12) << QPointF(14, 10);
    p.drawPolygon(arrow);
    
    p.end();
    return QIcon(pix);
}

static QIcon createVisualizerIcon(const QColor& color) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    
    p.drawRoundedRect(4, 14, 3, 6, 1, 1);
    p.drawRoundedRect(9, 8, 3, 12, 1, 1);
    p.drawRoundedRect(14, 5, 3, 15, 1, 1);
    p.drawRoundedRect(19, 10, 3, 10, 1, 1);
    
    p.end();
    return QIcon(pix);
}

static QIcon createAutoFsIcon(const QColor& color) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(color, 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    p.setPen(pen);

    // Outer corner brackets (Fullscreen symbol)
    p.drawLine(3, 8, 3, 3);
    p.drawLine(3, 3, 8, 3);

    p.drawLine(16, 3, 21, 3);
    p.drawLine(21, 3, 21, 8);

    p.drawLine(3, 16, 3, 21);
    p.drawLine(3, 21, 8, 21);

    p.drawLine(16, 21, 21, 21);
    p.drawLine(21, 21, 21, 16);

    // Center timer clock
    p.setPen(QPen(color, 1.5));
    p.drawEllipse(QPointF(12, 12), 4.5, 4.5);

    p.drawLine(12, 12, 12, 9);
    p.drawLine(12, 12, 14.5, 12);

    p.end();
    return QIcon(pix);
}

FullscreenWidget::FullscreenWidget(QWidget* parent) : QWidget(parent, Qt::Window | Qt::FramelessWindowHint) {
    setStyleSheet("background-color: #000000;");
    setMouseTracking(true);
    installEventFilter(this);

    // Create HUD Overlay Panel
    m_hudWidget = new QWidget(this, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    m_hudWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_hudWidget->setObjectName("hudPanel");
    m_hudWidget->setFocusPolicy(Qt::NoFocus);
    m_hudWidget->setStyleSheet(
        "QWidget#hudPanel { background-color: rgba(30, 30, 46, 220); border: 1px solid rgba(69, 71, 90, 150); border-radius: 12px; }"
        "QLabel { color: #cdd6f4; font-size: 12px; font-weight: bold; background: transparent; border: none; }"
        "QPushButton { border: none; background-color: transparent; color: #cdd6f4; padding: 4px; border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(137, 180, 250, 60); }"
        "QSlider::groove:horizontal { border: none; height: 6px; background: #313244; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #cdd6f4; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
    );

    QStyle* style = QApplication::style();

    // HUD buttons
    QPushButton* btnPrev = new QPushButton(m_hudWidget);
    btnPrev->setIcon(style->standardIcon(QStyle::SP_MediaSkipBackward));
    btnPrev->setToolTip("Previous");
    btnPrev->setFocusPolicy(Qt::NoFocus);
    connect(btnPrev, &QPushButton::clicked, this, &FullscreenWidget::prevRequested);

    m_btnPlayPause = new QPushButton(m_hudWidget);
    m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    m_btnPlayPause->setToolTip("Play/Pause");
    m_btnPlayPause->setFocusPolicy(Qt::NoFocus);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &FullscreenWidget::onHudPlayPause);

    QPushButton* btnStop = new QPushButton(m_hudWidget);
    btnStop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    btnStop->setToolTip("Stop");
    btnStop->setFocusPolicy(Qt::NoFocus);
    connect(btnStop, &QPushButton::clicked, this, &FullscreenWidget::stopRequested);

    QPushButton* btnNext = new QPushButton(m_hudWidget);
    btnNext->setIcon(style->standardIcon(QStyle::SP_MediaSkipForward));
    btnNext->setToolTip("Next");
    btnNext->setFocusPolicy(Qt::NoFocus);
    connect(btnNext, &QPushButton::clicked, this, &FullscreenWidget::nextRequested);

    m_sliderProgress = new ScrubSlider(Qt::Horizontal, m_hudWidget);
    m_sliderProgress->setRange(0, 100);
    m_sliderProgress->setFocusPolicy(Qt::StrongFocus);
    connect(m_sliderProgress, &QSlider::sliderMoved, this, &FullscreenWidget::onHudSliderMoved);

    m_lblTime = new QLabel("00:00 / 00:00", m_hudWidget);

    QLabel* lblVol = new QLabel("🔊", m_hudWidget);

    m_sliderVolume = new QSlider(Qt::Horizontal, m_hudWidget);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(70);
    m_sliderVolume->setFixedWidth(120);
    m_sliderVolume->setFocusPolicy(Qt::NoFocus);
    connect(m_sliderVolume, &QSlider::valueChanged, this, &FullscreenWidget::onHudVolumeChanged);

    m_btnSubtitles = new QPushButton(m_hudWidget);
    m_btnSubtitles->setText("CC");
    m_btnSubtitles->setToolTip("Subtitles");
    m_btnSubtitles->setFocusPolicy(Qt::NoFocus);
    m_btnSubtitles->setMaximumWidth(40);
    m_btnSubtitles->setStyleSheet("QPushButton { font-weight: bold; }");
    connect(m_btnSubtitles, &QPushButton::clicked, this, &FullscreenWidget::onHudSubtitles);

    m_btnShuffle = new QPushButton(m_hudWidget);
    m_btnShuffle->setIcon(createShuffleIcon(QColor("#cdd6f4")));
    m_btnShuffle->setToolTip("Shuffle Playlist");
    m_btnShuffle->setFocusPolicy(Qt::NoFocus);
    m_btnShuffle->setMaximumWidth(40);
    connect(m_btnShuffle, &QPushButton::clicked, this, &FullscreenWidget::onHudShuffle);

    m_btnRepeat = new QPushButton(m_hudWidget);
    m_btnRepeat->setIcon(createRepeatIcon(QColor("#cdd6f4"), false));
    m_btnRepeat->setToolTip("Repeat Mode");
    m_btnRepeat->setFocusPolicy(Qt::NoFocus);
    m_btnRepeat->setMaximumWidth(40);
    connect(m_btnRepeat, &QPushButton::clicked, this, &FullscreenWidget::onHudRepeat);

    QPushButton* btnExit = new QPushButton(m_hudWidget);
    btnExit->setIcon(style->standardIcon(QStyle::SP_TitleBarNormalButton));
    btnExit->setToolTip("Exit Fullscreen");
    btnExit->setFocusPolicy(Qt::NoFocus);
    connect(btnExit, &QPushButton::clicked, this, &FullscreenWidget::exitRequested);

    // Build two-row layout
    QVBoxLayout* hudMainLayout = new QVBoxLayout(m_hudWidget);
    hudMainLayout->setContentsMargins(15, 12, 15, 12);
    hudMainLayout->setSpacing(8);

    QHBoxLayout* row1Layout = new QHBoxLayout();
    row1Layout->setContentsMargins(0, 0, 0, 0);
    row1Layout->setSpacing(10);
    row1Layout->addWidget(m_sliderProgress, 1);
    row1Layout->addWidget(m_lblTime);
    hudMainLayout->addLayout(row1Layout);

    QHBoxLayout* row2Layout = new QHBoxLayout();
    row2Layout->setContentsMargins(0, 0, 0, 0);
    row2Layout->setSpacing(10);
    row2Layout->addWidget(btnPrev);
    row2Layout->addWidget(m_btnPlayPause);
    row2Layout->addWidget(btnStop);
    row2Layout->addWidget(btnNext);
    row2Layout->addWidget(m_btnShuffle);
    row2Layout->addWidget(m_btnRepeat);
    row2Layout->addWidget(m_btnSubtitles);
    
    row2Layout->addStretch(1);

    // Built-in doubleclick auto fullscreen control on the HUD itself
    m_btnToggleAutoFS = new QPushButton(m_hudWidget);
    m_btnToggleAutoFS->setCheckable(true);
    m_btnToggleAutoFS->setFocusPolicy(Qt::NoFocus);
    m_btnToggleAutoFS->setIconSize(QSize(18, 18));
    m_btnToggleAutoFS->setMaximumWidth(40);
    m_btnToggleAutoFS->setStyleSheet("QPushButton { background-color: transparent; border: none; }");
    connect(m_btnToggleAutoFS, &QPushButton::clicked, this, [this]() {
        emit builtinPlayerDoubleclickToggled(m_btnToggleAutoFS->isChecked());
    });

    // Query active state to set initial color/icon
    QWidget* pTemp = parentWidget();
    while (pTemp && !pTemp->inherits("MainWindow")) {
        pTemp = pTemp->parentWidget();
    }
    bool autoFS = false;
    if (pTemp) {
        QMetaObject::invokeMethod(pTemp, "isBuiltinPlayerDoubleclickActive", Q_RETURN_ARG(bool, autoFS));
    } else {
        QSettings settings("Amifiles", "Amifiles");
        autoFS = settings.value("preferences/builtin_player_doubleclick", false).toBool();
    }
    m_btnToggleAutoFS->setChecked(autoFS);
    m_btnToggleAutoFS->setIcon(createAutoFSIcon(autoFS ? QColor("#89b4fa") : QColor("#585b70")));
    m_btnToggleAutoFS->setToolTip(autoFS ? "Auto Full Screen: ON (Accent Blue)" : "Auto Full Screen: OFF (Dim Gray)");
    row2Layout->addWidget(m_btnToggleAutoFS);

    row2Layout->addSpacing(10);
    row2Layout->addWidget(lblVol);
    row2Layout->addWidget(m_sliderVolume);
    row2Layout->addSpacing(10);
    row2Layout->addWidget(btnExit);
    hudMainLayout->addLayout(row2Layout);

    // Position HUD at the bottom center of the screen
    m_hudWidget->resize(900, 90);

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
    int hudW = qMin(width() - 40, 850);
    QPoint globalPos = mapToGlobal(QPoint((width() - hudW) / 2, height() - 80));
    m_hudWidget->setGeometry(globalPos.x(), globalPos.y(), hudW, 50);
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
    int hudW = qMin(width() - 40, 850);
    QPoint globalPos = mapToGlobal(QPoint((width() - hudW) / 2, height() - 80));
    m_hudWidget->setGeometry(globalPos.x(), globalPos.y(), hudW, 50);
    m_hudWidget->show();
    m_hudWidget->raise();
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

void FullscreenWidget::onHudSubtitles() {
    if (!m_player) return;
    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #313244; }"
        "QMenu::item { padding: 6px 20px; }"
        "QMenu::item:selected { background-color: #89b4fa; color: #11111b; }"
    );
    QAction* disableAct = menu->addAction("Disable Subtitles");
    connect(disableAct, &QAction::triggered, this, [this]() {
        m_player->setActiveSubtitleTrack(-1);
    });

    auto tracks = m_player->subtitleTracks();
    for (int i = 0; i < tracks.size(); ++i) {
        QMediaMetaData meta = tracks[i];
        QString name = meta.stringValue(QMediaMetaData::Language);
        if (name.isEmpty()) name = meta.stringValue(QMediaMetaData::Title);
        if (name.isEmpty()) name = QString("Track %1").arg(i + 1);

        QAction* act = menu->addAction(name);
        connect(act, &QAction::triggered, this, [this, i]() {
            m_player->setActiveSubtitleTrack(i);
        });
    }
    menu->exec(QCursor::pos());
}

void FullscreenWidget::onHudShuffle() {
    emit shuffleToggled();
}

void FullscreenWidget::onHudRepeat() {
    emit repeatRequested();
}

void FullscreenWidget::setBuiltinPlayerDoubleclickActive(bool active) {
    if (m_btnToggleAutoFS) {
        m_btnToggleAutoFS->blockSignals(true);
        m_btnToggleAutoFS->setChecked(active);
        m_btnToggleAutoFS->setIcon(createAutoFSIcon(active ? QColor("#89b4fa") : QColor("#585b70")));
        m_btnToggleAutoFS->setToolTip(active ? "Auto Full Screen: ON (Accent Blue)" : "Auto Full Screen: OFF (Dim Gray)");
        m_btnToggleAutoFS->blockSignals(false);
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
    } else if (event->key() == Qt::Key_Left) {
        if (m_sliderProgress) {
            int step = (event->modifiers() & Qt::ShiftModifier) ? 1000 : 5000;
            int val = qMax(m_sliderProgress->minimum(), m_sliderProgress->value() - step);
            m_sliderProgress->setValue(val);
            emit m_sliderProgress->sliderMoved(val);
            event->accept();
        }
    } else if (event->key() == Qt::Key_Right) {
        if (m_sliderProgress) {
            int step = (event->modifiers() & Qt::ShiftModifier) ? 1000 : 5000;
            int val = qMin(m_sliderProgress->maximum(), m_sliderProgress->value() + step);
            m_sliderProgress->setValue(val);
            emit m_sliderProgress->sliderMoved(val);
            event->accept();
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}

void FullscreenWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    emit exitRequested();
}

void FullscreenWidget::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 20px 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #89b4fa; color: #11111b; }"
    );
    
    QAction* actExit = menu.addAction("Exit Fullscreen (Esc)");
    menu.addSeparator();
    QAction* actPlayPause = menu.addAction("Play / Pause (Space)");
    QAction* actStop = menu.addAction("Stop");
    QAction* actPrev = menu.addAction("Previous Track");
    QAction* actNext = menu.addAction("Next Track");
    
    QAction* selected = menu.exec(event->globalPos());
    if (selected == actExit) {
        emit exitRequested();
    } else if (selected == actPlayPause) {
        emit playPauseRequested();
    } else if (selected == actStop) {
        emit stopRequested();
    } else if (selected == actPrev) {
        emit prevRequested();
    } else if (selected == actNext) {
        emit nextRequested();
    }
}

PreviewPanel::PreviewPanel(QWidget* parent) : QWidget(parent) {
    // Initialize QMediaPlayer and AudioOutput first so setupUI can configure them
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);

    setupUI();
    setAcceptDrops(true);

    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    if (m_videoWidget) m_videoWidget->installEventFilter(this);
    if (m_audioPlaceholder) m_audioPlaceholder->installEventFilter(this);

    // Connect player signals
    connect(m_player, &QMediaPlayer::positionChanged, this, &PreviewPanel::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PreviewPanel::onDurationChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &PreviewPanel::onMediaMetadataChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PreviewPanel::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &PreviewPanel::onPlaybackStateChanged);

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
    } else if (watched == m_imageLabel) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            openFullscreenImage();
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
    m_imageLabel->installEventFilter(this);
    m_imageScrollArea->setWidget(m_imageLabel);

    imageLayout->addWidget(m_imageScrollArea);

    QPushButton* btnEditImage = new QPushButton("Edit Image & Annotate", m_imageView);
    btnEditImage->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; margin: 4px; font-weight: bold; } QPushButton:hover { background-color: #45475a; }");
    connect(btnEditImage, &QPushButton::clicked, this, [this]() {
        if (!m_previewedFilePath.isEmpty()) {
            ImageEditorDialog dlg(m_previewedFilePath, this);
            if (dlg.exec() == QDialog::Accepted) {
                previewFile(m_previewedFilePath);
            }
        }
    });
    imageLayout->addWidget(btnEditImage);

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
    m_spectrumVisualizerEnabled = settings.value("preview/show_spectrum_visualizer", true).toBool();

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

    m_btnShuffle = new QPushButton(this);
    m_btnShuffle->setIcon(createShuffleIcon(QColor("#cdd6f4")));
    m_btnShuffle->setToolTip("Shuffle (Off)");
    m_btnShuffle->setMaximumWidth(32);
    m_btnShuffle->setStyleSheet("QPushButton { background-color: transparent; }");
    connect(m_btnShuffle, &QPushButton::clicked, this, &PreviewPanel::onShuffleToggled);

    m_btnRepeat = new QPushButton(this);
    m_btnRepeat->setIcon(createRepeatIcon(QColor("#cdd6f4"), false));
    m_btnRepeat->setToolTip("Repeat: Off");
    m_btnRepeat->setMaximumWidth(32);
    m_btnRepeat->setStyleSheet("QPushButton { background-color: transparent; }");
    connect(m_btnRepeat, &QPushButton::clicked, this, &PreviewPanel::onRepeatClicked);

    m_btnToggleVisualizer = new QPushButton(this);
    m_btnToggleVisualizer->setCheckable(true);
    m_btnToggleVisualizer->setMaximumWidth(32);
    m_btnToggleVisualizer->setStyleSheet("QPushButton { background-color: transparent; }");
    m_btnToggleVisualizer->setChecked(m_spectrumVisualizerEnabled);
    m_btnToggleVisualizer->setIcon(createVisualizerIcon(m_spectrumVisualizerEnabled ? QColor("#89b4fa") : QColor("#cdd6f4")));
    m_btnToggleVisualizer->setToolTip(m_spectrumVisualizerEnabled ? "Spectrum Visualizer: ON (Accent Blue)" : "Spectrum Visualizer: OFF");
    connect(m_btnToggleVisualizer, &QPushButton::clicked, this, [this](bool checked) {
        setSpectrumVisualizerVisible(checked);
        emit spectrumVisualizerToggled(checked);
    });

    m_btnSubtitles = new QPushButton(this);
    m_btnSubtitles->setText("CC");
    m_btnSubtitles->setToolTip("Subtitles");
    m_btnSubtitles->setMaximumWidth(32);
    m_btnSubtitles->setStyleSheet("QPushButton { font-weight: bold; background-color: transparent; }");
    connect(m_btnSubtitles, &QPushButton::clicked, this, &PreviewPanel::onSubtitleMenuRequested);

    m_btnAutoFS20s = new QPushButton(this);
    m_btnAutoFS20s->setCheckable(true);
    m_btnAutoFS20s->setMaximumWidth(32);
    m_btnAutoFS20s->setStyleSheet("QPushButton { background-color: transparent; border: none; }");
    
    bool autoFs20sVal = settings.value("preview/auto_fs_20s", false).toBool();
    m_btnAutoFS20s->setChecked(autoFs20sVal);
    m_btnAutoFS20s->setIcon(createAutoFsIcon(autoFs20sVal ? QColor("#89b4fa") : QColor("#585b70")));
    m_btnAutoFS20s->setToolTip(autoFs20sVal ? "Auto Fullscreen (20s): ON" : "Auto Fullscreen (20s): OFF");

    connect(m_btnAutoFS20s, &QPushButton::toggled, this, [this](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preview/auto_fs_20s", checked);
        m_btnAutoFS20s->setIcon(createAutoFsIcon(checked ? QColor("#89b4fa") : QColor("#585b70")));
        m_btnAutoFS20s->setToolTip(checked ? "Auto Fullscreen (20s): ON" : "Auto Fullscreen (20s): OFF");

        if (checked && m_player && m_player->playbackState() == QMediaPlayer::PlayingState && m_autoFsTimer) {
            m_autoFsTimer->start(20000);
        } else if (!checked && m_autoFsTimer) {
            m_autoFsTimer->stop();
        }
    });

    m_autoFsTimer = new QTimer(this);
    m_autoFsTimer->setSingleShot(true);
    connect(m_autoFsTimer, &QTimer::timeout, this, [this]() {
        if (m_player && m_player->playbackState() == QMediaPlayer::PlayingState && !isFullscreen()) {
            toggleFullscreen();
        }
    });

    m_sliderProgress = new ScrubSlider(Qt::Horizontal, this);
    m_sliderProgress->setRange(0, 0);
    m_sliderProgress->setFocusPolicy(Qt::StrongFocus);
    connect(m_sliderProgress, &QSlider::sliderMoved, this, &PreviewPanel::onSliderMoved);

    m_lblProgressTime = new QLabel("00:00 / 00:00", this);
    m_lblProgressTime->setStyleSheet("color: #a6adc8; font-size: 11px;");

    QLabel* lblVol = new QLabel("🔊", this);
    lblVol->setStyleSheet("font-size: 14px;");

    int savedVolume = settings.value("preview/volume", 70).toInt();

    m_sliderVolume = new QSlider(Qt::Horizontal, this);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(savedVolume);
    m_sliderVolume->setMaximumWidth(80);
    m_audioOutput->setVolume(savedVolume / 100.0f);
    connect(m_sliderVolume, &QSlider::valueChanged, this, &PreviewPanel::onVolumeChanged);

    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(6);
    progressLayout->addWidget(m_sliderProgress, 1);
    progressLayout->addWidget(m_lblProgressTime);

    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(4);
    controlsLayout->addWidget(m_btnPrevTrack);
    controlsLayout->addWidget(m_btnPlayPause);
    controlsLayout->addWidget(m_btnStop);
    controlsLayout->addWidget(m_btnNextTrack);
    controlsLayout->addWidget(m_btnFullscreen);
    controlsLayout->addWidget(m_btnShuffle);
    controlsLayout->addWidget(m_btnRepeat);
    controlsLayout->addWidget(m_btnToggleVisualizer);
    controlsLayout->addWidget(m_btnSubtitles);
    controlsLayout->addWidget(m_btnAutoFS20s);
    controlsLayout->addStretch(1);
    controlsLayout->addWidget(lblVol);
    controlsLayout->addWidget(m_sliderVolume);

    m_visualizer = new SpectrumVisualizerWidget(m_mediaView);
    m_visualizer->setMinimumHeight(80);
    m_visualizer->setVisible(false);

    mediaLayout->addWidget(m_videoWidget, 1);
    mediaLayout->addWidget(m_audioPlaceholder, 1);
    mediaLayout->addWidget(m_visualizer, 0);
    mediaLayout->addLayout(progressLayout);
    mediaLayout->addLayout(controlsLayout);
    m_stack->addWidget(m_mediaView);

    m_pdfViewer = new PdfViewerWidget(this);
    m_stack->addWidget(m_pdfViewer);

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

    // Separator line
    QFrame* sep = new QFrame(m_metadataContainer);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setStyleSheet("background-color: #313244; max-height: 1px; margin: 4px 0;");
    metaLayout->addWidget(sep);

    // Quick Tag Editor Layout
    QFormLayout* tagForm = new QFormLayout();
    tagForm->setContentsMargins(6, 4, 6, 4);
    tagForm->setSpacing(6);

    m_tagEditorEdit = new QLineEdit(m_metadataContainer);
    m_tagEditorEdit->setPlaceholderText("Comma-separated tags (e.g. Work, Urgent)");
    m_tagEditorEdit->setClearButtonEnabled(true);
    m_tagEditorEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }");
    tagForm->addRow(new QLabel("Tags:", m_metadataContainer), m_tagEditorEdit);

    m_tagCompleter = new QCompleter(this);
    m_tagCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_tagCompleter->setCompletionMode(QCompleter::PopupCompletion);
    if (m_tagCompleter->popup()) {
        m_tagCompleter->popup()->setStyleSheet("background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a;");
    }
    m_tagEditorEdit->setCompleter(m_tagCompleter);

    connect(m_tagEditorEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        int lastComma = text.lastIndexOf(',');
        QString currentWord = (lastComma == -1) ? text.trimmed() : text.mid(lastComma + 1).trimmed();
        if (currentWord.isEmpty()) {
            m_tagCompleter->popup()->hide();
        } else {
            QStringList allTags = TagManager::instance().getAllTags();
            m_tagCompleter->setModel(new QStringListModel(allTags, m_tagCompleter));
            m_tagCompleter->setCompletionPrefix(currentWord);
            m_tagCompleter->complete();
        }
    });

    connect(m_tagCompleter, QOverload<const QString&>::of(&QCompleter::activated), this, [this](const QString& tag) {
        QString text = m_tagEditorEdit->text();
        int lastComma = text.lastIndexOf(',');
        if (lastComma == -1) {
            m_tagEditorEdit->setText(tag + ", ");
        } else {
            m_tagEditorEdit->setText(text.left(lastComma + 1) + " " + tag + ", ");
        }
    });

    m_tagColorCombo = new QComboBox(m_metadataContainer);
    m_tagColorCombo->addItems({"None", "Red", "Orange", "Yellow", "Green", "Blue", "Purple"});
    m_tagColorCombo->setStyleSheet("QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 4px; }");
    tagForm->addRow(new QLabel("Color Label:", m_metadataContainer), m_tagColorCombo);

    QHBoxLayout* overlayIconLayout = new QHBoxLayout();
    m_btnChooseOverlayIcon = new QPushButton("Select...", m_metadataContainer);
    m_btnChooseOverlayIcon->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 4px; }");
    m_btnClearOverlayIcon = new QPushButton("Clear", m_metadataContainer);
    m_btnClearOverlayIcon->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 4px; }");
    overlayIconLayout->addWidget(m_btnChooseOverlayIcon, 1);
    overlayIconLayout->addWidget(m_btnClearOverlayIcon, 0);
    tagForm->addRow(new QLabel("Icon Overlay:", m_metadataContainer), overlayIconLayout);
    connect(m_btnChooseOverlayIcon, &QPushButton::clicked, this, &PreviewPanel::onChooseOverlayIcon);
    connect(m_btnClearOverlayIcon, &QPushButton::clicked, this, &PreviewPanel::onClearOverlayIcon);

    m_btnApplyTagsColors = new QPushButton("Apply Tags & Color", m_metadataContainer);
    m_btnApplyTagsColors->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 4px; padding: 4px; }"
                                        "QPushButton:hover { background-color: #b4befe; }");
    connect(m_btnApplyTagsColors, &QPushButton::clicked, this, &PreviewPanel::onApplyTagsColors);
    tagForm->addRow(m_btnApplyTagsColors);

    metaLayout->addLayout(tagForm);

    m_bottomTab->addTab(m_metadataContainer, "Properties");

    // Tab 2: Playlist Queue
    m_playlistList = new QListWidget(this);
    m_playlistList->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; color: #cdd6f4; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background-color: #313244; color: #a6e3a1; }"
    );
    m_playlistList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistList, &QListWidget::itemDoubleClicked, this, &PreviewPanel::onPlaylistItemDoubleClicked);
    connect(m_playlistList, &QListWidget::customContextMenuRequested, this, &PreviewPanel::showPlaylistContextMenu);
    m_bottomTab->addTab(m_playlistList, "Playlist Queue");

    // Tab 3: Equalizer Container
    QWidget* eqContainer = new QWidget(this);
    QVBoxLayout* eqLayout = new QVBoxLayout(eqContainer);
    eqLayout->setContentsMargins(8, 8, 8, 8);
    eqLayout->setSpacing(8);

    m_chkShowVisualizer = new QCheckBox("Show Spectrum Visualizer", this);
    m_chkShowVisualizer->setChecked(m_spectrumVisualizerEnabled);
    m_chkShowVisualizer->setStyleSheet("QCheckBox { color: #cdd6f4; font-weight: bold; }");
    connect(m_chkShowVisualizer, &QCheckBox::toggled, this, [this](bool checked) {
        setSpectrumVisualizerVisible(checked);
        emit spectrumVisualizerToggled(checked);
    });
    eqLayout->addWidget(m_chkShowVisualizer);

    QHBoxLayout* presetRow = new QHBoxLayout();
    presetRow->addWidget(new QLabel("Preset:", this));
    m_comboEqPreset = new QComboBox(this);
    m_comboEqPreset->addItems({"Flat", "Bass Boost", "Treble Boost", "Classical", "Rock"});
    m_comboEqPreset->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    connect(m_comboEqPreset, &QComboBox::currentIndexChanged, this, &PreviewPanel::onEqPresetChanged);
    presetRow->addWidget(m_comboEqPreset, 1);
    eqLayout->addLayout(presetRow);

    QHBoxLayout* visModeRow = new QHBoxLayout();
    visModeRow->addWidget(new QLabel("Visualizer Mode:", this));
    QComboBox* comboVisMode = new QComboBox(this);
    comboVisMode->addItems({"Retro Bars", "Radial Circular", "Oscilloscope Waveform"});
    comboVisMode->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    connect(comboVisMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_visualizer) {
            m_visualizer->setVisualizerMode(static_cast<SpectrumVisualizerWidget::VisualizerMode>(idx));
        }
    });
    visModeRow->addWidget(comboVisMode, 1);
    eqLayout->addLayout(visModeRow);

    QHBoxLayout* slidersRow = new QHBoxLayout();
    slidersRow->setSpacing(16);

    auto createEqSlider = [this](const QString& labelText, QSlider*& sliderOut) {
        QVBoxLayout* col = new QVBoxLayout();
        col->setSpacing(4);

        sliderOut = new QSlider(Qt::Vertical, this);
        sliderOut->setRange(1, 100);
        sliderOut->setValue(50);
        sliderOut->setMinimumHeight(60);
        connect(sliderOut, &QSlider::valueChanged, this, &PreviewPanel::onEqSlidersChanged);

        QLabel* valLbl = new QLabel("0 dB", this);
        valLbl->setAlignment(Qt::AlignCenter);
        valLbl->setStyleSheet("font-size: 10px; color: #a6adc8;");
        connect(sliderOut, &QSlider::valueChanged, this, [valLbl](int val) {
            int db = (val - 50) / 4;
            valLbl->setText(QString("%1%2 dB").arg(db >= 0 ? "+" : "").arg(db));
        });

        QLabel* nameLbl = new QLabel(labelText, this);
        nameLbl->setAlignment(Qt::AlignCenter);
        nameLbl->setStyleSheet("font-size: 11px; font-weight: bold; color: #cdd6f4;");

        col->addWidget(sliderOut, 1, Qt::AlignHCenter);
        col->addWidget(valLbl, 0, Qt::AlignHCenter);
        col->addWidget(nameLbl, 0, Qt::AlignHCenter);
        return col;
    };

    slidersRow->addLayout(createEqSlider("Bass", m_sliderBass));
    slidersRow->addLayout(createEqSlider("Mid", m_sliderMid));
    slidersRow->addLayout(createEqSlider("Treble", m_sliderTreble));
    eqLayout->addLayout(slidersRow);

    m_bottomTab->addTab(eqContainer, "Equalizer");

    m_hexViewer = new HexEditorWidget(this);
    m_bottomTab->addTab(m_hexViewer, "Hex Viewer");

    m_pdfTextEdit = new QTextEdit(this);
    m_pdfTextEdit->setReadOnly(true);
    m_pdfTextEdit->setPlaceholderText("Select a PDF to extract text contents...");
    m_pdfTextEdit->setStyleSheet("QTextEdit { background-color: transparent; border: none; color: #cdd6f4; font-family: monospace; font-size: 11px; }");
    m_bottomTab->addTab(m_pdfTextEdit, "Document Text");

    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setStyleSheet(
        "QSplitter::handle {"
        "  background-color: #313244;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: #89b4fa;"
        "}"
    );
    splitter->setHandleWidth(4);
    splitter->addWidget(m_stack);
    splitter->addWidget(m_bottomTab);
    splitter->setSizes({400, 200});
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

void PreviewPanel::clearPreview() {
    m_player->stop();
    m_player->setSource(QUrl());
    if (m_autoFsTimer) {
        m_autoFsTimer->stop();
    }
    m_previewedFilePath.clear();
    m_currentAudioPath.clear();
    m_originalPixmap = QPixmap();
    m_imageLabel->clear();
    m_textEdit->clear();
    m_textChanged = false;
    m_textControls->hide();
    if (m_hexViewer) {
        m_hexViewer->clear();
    }
    if (m_pdfViewer) {
        m_pdfViewer->clear();
    }
    if (m_pdfTextEdit) {
        m_pdfTextEdit->clear();
    }
    if (m_audioPlaceholder) {
        m_audioPlaceholder->setFilePath("");
    }

    m_stack->setCurrentWidget(m_emptyView);

    m_metadataTable->setRowCount(0);
}

void PreviewPanel::previewFile(const QString& filePath, const QStringList& siblingSelections) {
    m_prePreviewPlaybackState = m_player->playbackState();
    m_previewedFilePaths = siblingSelections;
    if (m_previewedFilePaths.isEmpty() && !filePath.isEmpty()) {
        m_previewedFilePaths.append(filePath);
    }

    if (!m_playlist.isEmpty()) {
        int idx = m_playlist.indexOf(filePath);
        if (idx != -1) {
            m_playlistIndex = idx;
            if (m_playlistList) {
                m_playlistList->setCurrentRow(m_playlistIndex);
            }
        } else {
            QFileInfo fileInfo(filePath);
            QString ext = fileInfo.suffix().toLower();
            QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a" };
            QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm" };
            if (audioExts.contains(ext) || videoExts.contains(ext)) {
                m_playlist.clear();
                m_playlistIndex = -1;
                if (m_playlistList) {
                    m_playlistList->clear();
                }
                for (int r = 0; r < m_metadataTable->rowCount(); ++r) {
                    QTableWidgetItem* keyItem = m_metadataTable->item(r, 0);
                    if (keyItem && keyItem->text() == "Playlist Status") {
                        m_metadataTable->removeRow(r);
                        break;
                    }
                }
            }
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
    } else if (ext == "pdf") {
        if (m_pdfViewer) {
            m_pdfViewer->loadPdf(filePath);
            m_stack->setCurrentWidget(m_pdfViewer);
        }
        if (m_pdfTextEdit) {
            m_pdfTextEdit->setPlainText("Extracting text contents from PDF...");
            QProcess* extractProc = new QProcess(this);
            connect(extractProc, &QProcess::finished, this, [this, extractProc](int exitCode) {
                if (exitCode == 0) {
                    m_pdfTextEdit->setPlainText(QString::fromLocal8Bit(extractProc->readAllStandardOutput()));
                } else {
                    m_pdfTextEdit->setPlainText("Failed to extract text from PDF.");
                }
                extractProc->deleteLater();
            });
            extractProc->start("pdftotext", {filePath, "-"});
        }
    } else {
        // Unknown binary/other file - just show metadata
        m_stack->setCurrentWidget(m_emptyView);
    }

    // Load and show metadata
    FileMetadata meta = MetadataExtractor::extract(filePath);
    updateMetadataDisplay(meta);

    if (m_hexViewer) {
        m_hexViewer->loadFile(filePath);
    }

    if (m_fullscreenWidget) {
        updateFullscreenTrack();
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

void PreviewPanel::openFullscreenImage() {
    if (m_previewedFilePath.isEmpty()) return;

    QString dirPath = QFileInfo(m_previewedFilePath).absolutePath();
    QDir dir(dirPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.gif" << "*.bmp" << "*.webp" << "*.svg";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    QStringList absoluteFiles;
    int startIndex = -1;
    for (int i = 0; i < files.size(); ++i) {
        QString absPath = dir.absoluteFilePath(files[i]);
        absoluteFiles.append(absPath);
        if (absPath == m_previewedFilePath) {
            startIndex = i;
        }
    }

    if (startIndex != -1) {
        FullscreenImageViewer* viewer = new FullscreenImageViewer(absoluteFiles, startIndex, this);
        connect(viewer, &QDialog::finished, this, [this, viewer](int) {
            QString lastFile = viewer->currentFilePath();
            if (!lastFile.isEmpty() && QFile::exists(lastFile)) {
                previewFile(lastFile);
            }
        });
        viewer->exec();
    }
}

void PreviewPanel::showMediaPreview(const QString& filePath, bool isVideo) {
    m_videoWidget->setVisible(isVideo);
    m_audioPlaceholder->setVisible(!isVideo);
    m_visualizer->setVisible(!isVideo && m_spectrumVisualizerEnabled);

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

    if (isVisible() || m_prePreviewPlaybackState == QMediaPlayer::PlayingState) {
        m_player->play();
        m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        if (m_btnAutoFS20s && m_btnAutoFS20s->isChecked() && m_autoFsTimer) {
            m_autoFsTimer->start(20000);
        }
    } else {
        m_player->stop();
        m_btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        if (m_autoFsTimer) {
            m_autoFsTimer->stop();
        }
    }
}

void PreviewPanel::scaleImage() {
    if (m_originalPixmap.isNull()) return;

    QSize viewSize = m_imageScrollArea->size();
    if (viewSize.width() < 100 || viewSize.height() < 100) {
        viewSize = m_stack->size();
    }
    if (viewSize.width() < 50) viewSize.setWidth(400);
    if (viewSize.height() < 50) viewSize.setHeight(400);

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

void PreviewPanel::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left) {
        if (m_player) {
            int step = (event->modifiers() & Qt::ShiftModifier) ? 1000 : 5000;
            m_player->setPosition(qMax(qint64(0), m_player->position() - step));
            event->accept();
        }
    } else if (event->key() == Qt::Key_Right) {
        if (m_player) {
            int step = (event->modifiers() & Qt::ShiftModifier) ? 1000 : 5000;
            m_player->setPosition(qMin(m_player->duration(), m_player->position() + step));
            event->accept();
        }
    } else if (event->key() == Qt::Key_Space) {
        onPlayPause();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PreviewPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void PreviewPanel::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void PreviewPanel::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) return;

    QStringList playableFiles;
    QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a" };
    QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm" };
    QStringList allMedia = audioExts + videoExts;

    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (path.isEmpty()) continue;

        QFileInfo info(path);
        if (info.isDir()) {
            QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString file = it.next();
                QString ext = QFileInfo(file).suffix().toLower();
                if (allMedia.contains(ext)) {
                    playableFiles.append(file);
                }
            }
        } else {
            QString ext = info.suffix().toLower();
            if (allMedia.contains(ext)) {
                playableFiles.append(path);
            }
        }
    }

    std::sort(playableFiles.begin(), playableFiles.end());

    if (!playableFiles.isEmpty()) {
        event->acceptProposedAction();
        playPlaylist(playableFiles);
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
    } else {
        m_player->play();
    }
}

void PreviewPanel::onStop() {
    m_player->stop();
    m_sliderProgress->setValue(0);
}

void PreviewPanel::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    QStyle* style = QApplication::style();
    if (state == QMediaPlayer::PlayingState) {
        m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPause));
        if (m_visualizer) m_visualizer->setPlaying(true);
        if (m_btnAutoFS20s && m_btnAutoFS20s->isChecked() && m_autoFsTimer) {
            m_autoFsTimer->start(20000);
        }
    } else {
        m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
        if (m_visualizer) m_visualizer->setPlaying(false);
        if (m_autoFsTimer) {
            m_autoFsTimer->stop();
        }
    }
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
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preview/volume", value);
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

    // Load and fill tag inputs
    QStringList tags = TagManager::instance().getFileTags(meta.path);
    m_tagEditorEdit->setText(tags.join(", "));

    QString col = TagManager::instance().getFileColor(meta.path);
    if (!col.isEmpty()) {
        col = col.left(1).toUpper() + col.mid(1).toLower();
    } else {
        col = "None";
    }
    int colIdx = m_tagColorCombo->findText(col, Qt::MatchFixedString);
    if (colIdx != -1) {
        m_tagColorCombo->setCurrentIndex(colIdx);
    } else {
        m_tagColorCombo->setCurrentIndex(0);
    }

    m_selectedOverlayIconName = TagManager::instance().getFileOverlayIcon(meta.path);
    if (!m_selectedOverlayIconName.isEmpty()) {
        m_btnChooseOverlayIcon->setText(m_selectedOverlayIconName);
        m_btnChooseOverlayIcon->setIcon(QIcon::fromTheme(m_selectedOverlayIconName));
    } else {
        m_btnChooseOverlayIcon->setText("Select...");
        m_btnChooseOverlayIcon->setIcon(QIcon());
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

void PreviewPanel::setBuiltinPlayerDoubleclickActive(bool active) {
    if (m_fullscreenWidget) {
        m_fullscreenWidget->setBuiltinPlayerDoubleclickActive(active);
    }
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

void PreviewPanel::addToPlaylist(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;

    bool wasEmpty = m_playlist.isEmpty();

    for (const QString& path : filePaths) {
        m_playlist.append(path);
        m_playlistList->addItem(QFileInfo(path).fileName());
    }

    if (wasEmpty) {
        m_playlistIndex = 0;
        previewFile(m_playlist[0]);
        m_playlistList->setCurrentRow(0);

        if (m_player) {
            m_player->play();
        }
    } else {
        int statusRow = -1;
        for (int i = 0; i < m_metadataTable->rowCount(); ++i) {
            if (m_metadataTable->item(i, 0) && m_metadataTable->item(i, 0)->text() == "Playlist Status") {
                statusRow = i;
                break;
            }
        }
        if (statusRow == -1) {
            statusRow = m_metadataTable->rowCount();
            m_metadataTable->insertRow(statusRow);
            m_metadataTable->setItem(statusRow, 0, new QTableWidgetItem("Playlist Status"));
        }
        m_metadataTable->setItem(statusRow, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    }

    m_bottomTab->setCurrentIndex(1); // Switch to Playlist Queue tab
}

void PreviewPanel::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_repeatMode == 1) {
            m_player->setPosition(0);
            m_player->play();
        } else {
            onNextTrack();
        }
    }
}

void PreviewPanel::onPrevTrack() {
    if (m_playlist.isEmpty()) return;
    if (m_shuffleEnabled) {
        if (m_playlist.size() > 1) {
            int prevIndex = m_playlistIndex;
            while (prevIndex == m_playlistIndex) {
                prevIndex = QRandomGenerator::global()->bounded(m_playlist.size());
            }
            m_playlistIndex = prevIndex;
        }
    } else {
        if (m_playlistIndex > 0) {
            m_playlistIndex--;
        } else {
            if (m_repeatMode == 2) {
                m_playlistIndex = m_playlist.size() - 1;
            } else {
                return;
            }
        }
    }

    previewFile(m_playlist[m_playlistIndex]);
    m_playlistList->setCurrentRow(m_playlistIndex);

    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
}

void PreviewPanel::onNextTrack() {
    if (m_playlist.isEmpty()) return;
    if (m_shuffleEnabled) {
        if (m_playlist.size() > 1) {
            int nextIndex = m_playlistIndex;
            while (nextIndex == m_playlistIndex) {
                nextIndex = QRandomGenerator::global()->bounded(m_playlist.size());
            }
            m_playlistIndex = nextIndex;
        }
    } else {
        if (m_playlistIndex < m_playlist.size() - 1) {
            m_playlistIndex++;
        } else {
            if (m_repeatMode == 2) {
                m_playlistIndex = 0;
            } else {
                m_player->stop();
                return;
            }
        }
    }

    previewFile(m_playlist[m_playlistIndex]);
    m_playlistList->setCurrentRow(m_playlistIndex);

    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
}

void PreviewPanel::onShuffleToggled() {
    m_shuffleEnabled = !m_shuffleEnabled;

    m_btnShuffle->setIcon(createShuffleIcon(m_shuffleEnabled ? QColor("#a6e3a1") : QColor("#cdd6f4")));
    if (m_shuffleEnabled) {
        m_btnShuffle->setStyleSheet("QPushButton { color: #a6e3a1; font-weight: bold; background-color: transparent; }");
        m_btnShuffle->setToolTip("Shuffle (On)");
    } else {
        m_btnShuffle->setStyleSheet("QPushButton { background-color: transparent; }");
        m_btnShuffle->setToolTip("Shuffle (Off)");
    }

    if (m_fullscreenWidget && m_fullscreenWidget->hudShuffleButton()) {
        m_fullscreenWidget->hudShuffleButton()->setIcon(createShuffleIcon(m_shuffleEnabled ? QColor("#a6e3a1") : QColor("#cdd6f4")));
        m_fullscreenWidget->hudShuffleButton()->setStyleSheet(m_shuffleEnabled ? "QPushButton { color: #a6e3a1; font-weight: bold; }" : "");
    }
}

void PreviewPanel::onRepeatClicked() {
    m_repeatMode = (m_repeatMode + 1) % 3;

    m_btnRepeat->setIcon(createRepeatIcon(m_repeatMode > 0 ? QColor("#a6e3a1") : QColor("#cdd6f4"), m_repeatMode == 1));
    if (m_repeatMode == 0) {
        m_btnRepeat->setToolTip("Repeat: Off");
        m_btnRepeat->setStyleSheet("QPushButton { background-color: transparent; }");
    } else if (m_repeatMode == 1) {
        m_btnRepeat->setToolTip("Repeat: One");
        m_btnRepeat->setStyleSheet("QPushButton { color: #a6e3a1; font-weight: bold; background-color: transparent; }");
    } else if (m_repeatMode == 2) {
        m_btnRepeat->setToolTip("Repeat: All");
        m_btnRepeat->setStyleSheet("QPushButton { color: #a6e3a1; font-weight: bold; background-color: transparent; }");
    }

    if (m_fullscreenWidget && m_fullscreenWidget->hudRepeatButton()) {
        m_fullscreenWidget->hudRepeatButton()->setIcon(createRepeatIcon(m_repeatMode > 0 ? QColor("#a6e3a1") : QColor("#cdd6f4"), m_repeatMode == 1));
        m_fullscreenWidget->hudRepeatButton()->setStyleSheet(m_repeatMode > 0 ? "QPushButton { color: #a6e3a1; font-weight: bold; }" : "");
    }
}

void PreviewPanel::onSubtitleMenuRequested() {
    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #313244; }"
        "QMenu::item { padding: 6px 20px; }"
        "QMenu::item:selected { background-color: #89b4fa; color: #11111b; }"
    );
    QAction* disableAct = menu->addAction("Disable Subtitles");
    connect(disableAct, &QAction::triggered, this, [this]() {
        m_player->setActiveSubtitleTrack(-1);
    });

    auto tracks = m_player->subtitleTracks();
    for (int i = 0; i < tracks.size(); ++i) {
        QMediaMetaData meta = tracks[i];
        QString name = meta.stringValue(QMediaMetaData::Language);
        if (name.isEmpty()) name = meta.stringValue(QMediaMetaData::Title);
        if (name.isEmpty()) name = QString("Track %1").arg(i + 1);

        QAction* act = menu->addAction(name);
        connect(act, &QAction::triggered, this, [this, i]() {
            m_player->setActiveSubtitleTrack(i);
        });
    }
    menu->exec(QCursor::pos());
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

void PreviewPanel::showPlaylistContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_playlistList->itemAt(pos);
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; border: 1px solid #313244; color: #cdd6f4; }"
        "QMenu::item:selected { background-color: #313244; color: #89b4fa; }"
    );

    QAction* actPlay = menu.addAction("▶ Play Selected Track");
    QAction* actRemove = menu.addAction("✖ Remove Selected Track");
    menu.addSeparator();
    QAction* actClearCurrent = menu.addAction("⏹ Stop & Clear Current Track");
    QAction* actClearQueue = menu.addAction("🗑 Clear Entire Queue");

    // Enable/disable actions
    actPlay->setEnabled(item != nullptr);
    actRemove->setEnabled(item != nullptr);
    actClearQueue->setEnabled(!m_playlist.isEmpty());
    actClearCurrent->setEnabled(!m_currentAudioPath.isEmpty() || m_player->playbackState() != QMediaPlayer::StoppedState);

    QAction* selected = menu.exec(m_playlistList->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actPlay && item) {
        onPlaylistItemDoubleClicked(item);
    } else if (selected == actRemove && item) {
        int row = m_playlistList->row(item);
        if (row >= 0 && row < m_playlist.size()) {
            m_playlist.removeAt(row);
            delete m_playlistList->takeItem(row);
            
            // Adjust index if we removed an item before or at the current playing track
            if (row == m_playlistIndex) {
                // If it was the playing track, stop playback and play next or clear
                if (m_playlist.isEmpty()) {
                    clearPreview();
                } else {
                    if (m_playlistIndex >= m_playlist.size()) {
                        m_playlistIndex = m_playlist.size() - 1;
                    }
                    previewFile(m_playlist[m_playlistIndex]);
                    m_playlistList->setCurrentRow(m_playlistIndex);
                }
            } else if (row < m_playlistIndex) {
                m_playlistIndex--;
            }
            
            // Update playlist status in metadata table if still active
            for (int r = 0; r < m_metadataTable->rowCount(); ++r) {
                QTableWidgetItem* keyItem = m_metadataTable->item(r, 0);
                if (keyItem && keyItem->text() == "Playlist Status") {
                    if (m_playlist.isEmpty()) {
                        m_metadataTable->removeRow(r);
                    } else {
                        m_metadataTable->setItem(r, 1, new QTableWidgetItem(
                            QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())
                        ));
                    }
                    break;
                }
            }
        }
    } else if (selected == actClearCurrent) {
        m_player->stop();
        m_currentAudioPath.clear();
        m_audioPlaceholder->setFilePath("");
        if (m_visualizer) m_visualizer->setPlaying(false);
    } else if (selected == actClearQueue) {
        m_player->stop();
        clearPreview();
        m_playlist.clear();
        m_playlistIndex = -1;
        m_playlistList->clear();
        
        // Remove Playlist Status from metadata table
        for (int r = 0; r < m_metadataTable->rowCount(); ++r) {
            QTableWidgetItem* keyItem = m_metadataTable->item(r, 0);
            if (keyItem && keyItem->text() == "Playlist Status") {
                m_metadataTable->removeRow(r);
                break;
            }
        }
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
    m_metadata = MetadataExtractor::extract(filePath);
    m_embeddedCover = QPixmap();
    
    if (!filePath.isEmpty() && m_coverArtVisible) {
        QProcess proc;
        proc.start("exiftool", {"-Picture", "-b", filePath});
        if (proc.waitForFinished(1500)) {
            QByteArray imgData = proc.readAllStandardOutput();
            if (!imgData.isEmpty()) {
                m_embeddedCover.loadFromData(imgData);
            }
        }
    }
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

    bool hasCover = false;
    QPixmap cover;
    if (m_coverArtVisible) {
        if (!m_embeddedCover.isNull()) {
            cover = m_embeddedCover;
            hasCover = true;
        } else {
            QString dirPath = QFileInfo(m_filePath).absolutePath();
            QDir dir(dirPath);
            QStringList artNames = { "folder", "cover", "album", "poster" };
            QStringList artExts = { "jpg", "jpeg", "png", "webp" };
            for (const QString& name : artNames) {
                for (const QString& ext : artExts) {
                    QString path = dir.filePath(name + "." + ext);
                    if (QFile::exists(path)) {
                        QPixmap p(path);
                        if (!p.isNull()) {
                            cover = p;
                            hasCover = true;
                            break;
                        }
                    }
                }
                if (hasCover) break;
            }
        }
    }

    if (hasCover) {
        if (!cover.isNull()) {
            // Draw cover art filling the widget area (preserving aspect ratio)
            QRect fgRect = r.adjusted(12, 12, -12, -12);
            if (fgRect.width() > 10 && fgRect.height() > 10) {
                QPixmap fgCover = cover.scaled(fgRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                int fgX = fgRect.left() + (fgRect.width() - fgCover.width()) / 2;
                int fgY = fgRect.top() + (fgRect.height() - fgCover.height()) / 2;
                painter.drawPixmap(fgX, fgY, fgCover);

                // Subtle border
                painter.setPen(QPen(QColor("#313244"), 1));
                painter.drawRect(fgX, fgY, fgCover.width(), fgCover.height());
            }
        }

        // Draw semi-transparent HUD overlay card on top of the artwork
        QRect hudRect(12, r.height() - 75, r.width() - 24, 63);
        if (hudRect.height() > 20 && hudRect.width() > 50) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(17, 17, 27, 210)));
            painter.drawRoundedRect(hudRect, 6, 6);

            QString displayTitle = !m_metadata.title.isEmpty() ? m_metadata.title : QFileInfo(m_filePath).completeBaseName();
            QString displayArtist = !m_metadata.artist.isEmpty() ? m_metadata.artist : "Unknown Artist";
            QString displayAlbum = m_metadata.album;

            // Draw Title (Line 1)
            painter.setPen(QColor("#cdd6f4"));
            QFont fTitle = font();
            fTitle.setPointSize(10);
            fTitle.setBold(true);
            painter.setFont(fTitle);
            painter.drawText(QRect(hudRect.left() + 8, hudRect.top() + 8, hudRect.width() - 16, 20), Qt::AlignLeft | Qt::AlignVCenter | Qt::ElideRight, displayTitle);

            // Draw Artist & Album (Line 2)
            painter.setPen(QColor("#a6adc8"));
            QFont fDetails = font();
            fDetails.setPointSize(8);
            fDetails.setBold(false);
            painter.setFont(fDetails);
            QString detailsText = displayArtist;
            if (!displayAlbum.isEmpty()) {
                detailsText += " — " + displayAlbum;
            }
            painter.drawText(QRect(hudRect.left() + 8, hudRect.top() + 32, hudRect.width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter | Qt::ElideRight, detailsText);
        }
    } else {
        // Fallback when no cover art exists - display tags in center
        QString displayTitle = !m_metadata.title.isEmpty() ? m_metadata.title : QFileInfo(m_filePath).completeBaseName();
        QString displayArtist = !m_metadata.artist.isEmpty() ? m_metadata.artist : "Unknown Artist";
        QString displayAlbum = m_metadata.album;

        painter.setPen(QColor("#a6adc8"));
        QFont fHead = font();
        fHead.setPointSize(10);
        fHead.setBold(true);
        painter.setFont(fHead);
        painter.drawText(QRect(10, 40, r.width() - 20, 25), Qt::AlignCenter, "🎵 Playing Audio");

        painter.setPen(QColor("#cdd6f4"));
        QFont fTitle = font();
        fTitle.setPointSize(14);
        fTitle.setBold(true);
        painter.setFont(fTitle);
        painter.drawText(QRect(10, r.height() / 2 - 30, r.width() - 20, 35), Qt::AlignCenter | Qt::ElideRight, displayTitle);

        painter.setPen(QColor("#a6adc8"));
        QFont fDetails = font();
        fDetails.setPointSize(10);
        painter.setFont(fDetails);
        QString detailsText = displayArtist;
        if (!displayAlbum.isEmpty()) {
            detailsText += " — " + displayAlbum;
        }
        painter.drawText(QRect(10, r.height() / 2 + 10, r.width() - 20, 25), Qt::AlignCenter | Qt::ElideRight, detailsText);
    }
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
    m_fullscreenWidget = new FullscreenWidget(this);
    connect(m_fullscreenWidget, &FullscreenWidget::exitRequested, this, &PreviewPanel::exitFullscreen);
    connect(m_fullscreenWidget, &FullscreenWidget::prevRequested, this, &PreviewPanel::onPrevTrack);
    connect(m_fullscreenWidget, &FullscreenWidget::playPauseRequested, this, &PreviewPanel::onPlayPause);
    connect(m_fullscreenWidget, &FullscreenWidget::stopRequested, this, &PreviewPanel::onStop);
    connect(m_fullscreenWidget, &FullscreenWidget::nextRequested, this, &PreviewPanel::onNextTrack);
    connect(m_fullscreenWidget, &FullscreenWidget::shuffleToggled, this, &PreviewPanel::onShuffleToggled);
    connect(m_fullscreenWidget, &FullscreenWidget::repeatRequested, this, &PreviewPanel::onRepeatClicked);
    connect(m_fullscreenWidget, &FullscreenWidget::builtinPlayerDoubleclickToggled, this, &PreviewPanel::builtinPlayerDoubleclickToggled);

    // Synchronize initial Auto-FS toggle state to HUD
    bool autoFS = false;
    QWidget* pTemp = parentWidget();
    while (pTemp && !pTemp->inherits("MainWindow")) {
        pTemp = pTemp->parentWidget();
    }
    if (pTemp) {
        QMetaObject::invokeMethod(pTemp, "isBuiltinPlayerDoubleclickActive", Q_RETURN_ARG(bool, autoFS));
    } else {
        QSettings settings("Amifiles", "Amifiles");
        autoFS = settings.value("preferences/builtin_player_doubleclick", false).toBool();
    }
    m_fullscreenWidget->setBuiltinPlayerDoubleclickActive(autoFS);

    // Synchronize initial styles to HUD buttons
    if (m_fullscreenWidget->hudShuffleButton()) {
        m_fullscreenWidget->hudShuffleButton()->setIcon(createShuffleIcon(m_shuffleEnabled ? QColor("#a6e3a1") : QColor("#cdd6f4")));
        m_fullscreenWidget->hudShuffleButton()->setStyleSheet(m_shuffleEnabled ? "QPushButton { color: #a6e3a1; font-weight: bold; }" : "");
    }
    if (m_fullscreenWidget->hudRepeatButton()) {
        m_fullscreenWidget->hudRepeatButton()->setIcon(createRepeatIcon(m_repeatMode > 0 ? QColor("#a6e3a1") : QColor("#cdd6f4"), m_repeatMode == 1));
        m_fullscreenWidget->hudRepeatButton()->setStyleSheet(m_repeatMode > 0 ? "QPushButton { color: #a6e3a1; font-weight: bold; }" : "");
    }

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

        m_fullscreenTextLabel = new QLabel(m_fullscreenWidget);
        m_fullscreenTextLabel->setAlignment(Qt::AlignCenter);
        m_fullscreenTextLabel->setStyleSheet("color: #cdd6f4; font-size: 24px; font-weight: bold; padding: 20px;");
        
        FileMetadata meta = MetadataExtractor::extract(activePath);
        QString displayTitle = !meta.title.isEmpty() ? meta.title : QFileInfo(activePath).completeBaseName();
        QString displayArtist = !meta.artist.isEmpty() ? meta.artist : "Unknown Artist";
        m_fullscreenTextLabel->setText(QString("%1\n%2").arg(displayTitle).arg(displayArtist));

        layout->addStretch();
        layout->addWidget(m_fullscreenAudioLabel);
        layout->addSpacing(10);
        layout->addWidget(m_fullscreenTextLabel);
        layout->addStretch();

        m_fullscreenAudioLabel->setMouseTracking(true);
        m_fullscreenTextLabel->setMouseTracking(true);
        m_fullscreenAudioLabel->installEventFilter(m_fullscreenWidget);
        m_fullscreenTextLabel->installEventFilter(m_fullscreenWidget);
    }

    m_fullscreenWidget->setMediaState(isVideo, m_player, m_audioOutput);
    m_fullscreenWidget->updateProgress(m_player->position(), m_player->duration());
    m_fullscreenWidget->showFullScreen();
    m_fullscreenWidget->setFocus();
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

static void clearLayoutOfFullscreen(QLayout* layout) {
    if (!layout) return;
    QLayoutItem* item;
    while ((item = layout->takeAt(0))) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void PreviewPanel::updateFullscreenTrack() {
    if (!m_fullscreenWidget) return;

    QString activePath = !m_currentAudioPath.isEmpty() ? m_currentAudioPath : m_previewedFilePath;
    if (activePath.isEmpty()) return;

    bool isVideo = m_videoWidget->isVisible();

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_fullscreenWidget->layout());
    if (layout) {
        clearLayoutOfFullscreen(layout);
    } else {
        layout = new QVBoxLayout(m_fullscreenWidget);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    m_fullscreenVideoWidget = nullptr;
    m_fullscreenAudioLabel = nullptr;
    m_fullscreenTextLabel = nullptr;

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

        m_fullscreenTextLabel = new QLabel(m_fullscreenWidget);
        m_fullscreenTextLabel->setAlignment(Qt::AlignCenter);
        m_fullscreenTextLabel->setStyleSheet("color: #cdd6f4; font-size: 24px; font-weight: bold; padding: 20px;");

        FileMetadata meta = MetadataExtractor::extract(activePath);
        QString displayTitle = !meta.title.isEmpty() ? meta.title : QFileInfo(activePath).completeBaseName();
        QString displayArtist = !meta.artist.isEmpty() ? meta.artist : "Unknown Artist";
        m_fullscreenTextLabel->setText(QString("%1\n%2").arg(displayTitle).arg(displayArtist));

        layout->addStretch();
        layout->addWidget(m_fullscreenAudioLabel);
        layout->addSpacing(10);
        layout->addWidget(m_fullscreenTextLabel);
        layout->addStretch();

        m_fullscreenAudioLabel->setMouseTracking(true);
        m_fullscreenTextLabel->setMouseTracking(true);
        m_fullscreenAudioLabel->installEventFilter(m_fullscreenWidget);
        m_fullscreenTextLabel->installEventFilter(m_fullscreenWidget);
    }

    m_fullscreenWidget->setMediaState(isVideo, m_player, m_audioOutput);
    m_fullscreenWidget->updateProgress(m_player->position(), m_player->duration());
}

#include <QRandomGenerator>
#include <QTimer>

SpectrumVisualizerWidget::SpectrumVisualizerWidget(QWidget* parent)
    : QWidget(parent), m_barHeights(15, 0.0), m_targetHeights(15, 0.0) {
    setMinimumHeight(80);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SpectrumVisualizerWidget::onAnimate);
    m_timer->start(50);
}

void SpectrumVisualizerWidget::setPlaying(bool playing) {
    m_playing = playing;
}

void SpectrumVisualizerWidget::setBoost(double bass, double mid, double treble) {
    m_bassBoost = bass;
    m_midBoost = mid;
    m_trebleBoost = treble;
}

void SpectrumVisualizerWidget::setVisualizerMode(VisualizerMode mode) {
    m_mode = mode;
    update();
}

void SpectrumVisualizerWidget::onAnimate() {
    double averageHeight = 0.0;
    for (int i = 0; i < 15; ++i) {
        if (m_playing) {
            double boost = 1.0;
            if (i < 5) boost = m_bassBoost;
            else if (i < 10) boost = m_midBoost;
            else boost = m_trebleBoost;

            m_targetHeights[i] = QRandomGenerator::global()->bounded(10, 95) * boost;
            if (m_targetHeights[i] > 100.0) m_targetHeights[i] = 100.0;
        } else {
            m_targetHeights[i] = 0.0;
        }

        m_barHeights[i] = m_barHeights[i] * 0.6 + m_targetHeights[i] * 0.4;
        averageHeight += m_barHeights[i];
    }
    averageHeight /= 15.0;

    double value = 0.0;
    if (m_playing) {
        static double phase = 0.0;
        phase += 0.4;
        value = (qSin(phase) * 0.6 + (QRandomGenerator::global()->bounded(100) / 100.0 - 0.5) * 0.4) * averageHeight;
    }
    m_waveformHistory.append(value);
    while (m_waveformHistory.size() > 80) {
        m_waveformHistory.removeFirst();
    }

    update();
}

void SpectrumVisualizerWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), QColor("#11111b"));

    if (m_mode == VisualizerBars) {
        int numBars = 15;
        double spacing = 4.0;
        double totalSpacing = spacing * (numBars + 1);
        double barW = (width() - totalSpacing) / numBars;

        QLinearGradient grad(0, height(), 0, 0);
        grad.setColorAt(0.0, QColor("#89b4fa"));
        grad.setColorAt(0.5, QColor("#cba6f7"));
        grad.setColorAt(1.0, QColor("#f38ba8"));

        for (int i = 0; i < numBars; ++i) {
            double barH = (m_barHeights[i] / 100.0) * (height() - 8.0);
            if (barH < 2.0) barH = 2.0;

            double x = spacing + i * (barW + spacing);
            double y = height() - barH - 4.0;

            painter.fillRect(QRectF(x, y, barW, barH), grad);
        }
    } else if (m_mode == VisualizerRadial) {
        double centerX = width() / 2.0;
        double centerY = height() / 2.0;
        double radius = qMin(width(), height()) / 4.0;

        QConicalGradient grad(centerX, centerY, 0);
        grad.setColorAt(0.0, QColor("#89b4fa"));
        grad.setColorAt(0.33, QColor("#cba6f7"));
        grad.setColorAt(0.66, QColor("#f38ba8"));
        grad.setColorAt(1.0, QColor("#89b4fa"));
        painter.setPen(QPen(grad, 4, Qt::SolidLine, Qt::RoundCap));

        int numBars = 15;
        for (int i = 0; i < numBars; ++i) {
            double angle = (2.0 * M_PI / numBars) * i;
            double barLen = (m_barHeights[i] / 100.0) * (radius * 1.5);
            if (barLen < 2.0) barLen = 2.0;

            double startX = centerX + radius * qCos(angle);
            double startY = centerY + radius * qSin(angle);
            double endX = centerX + (radius + barLen) * qCos(angle);
            double endY = centerY + (radius + barLen) * qSin(angle);

            painter.drawLine(QPointF(startX, startY), QPointF(endX, endY));
        }

        painter.setBrush(QColor("#181825"));
        painter.setPen(QPen(QColor("#313244"), 2));
        painter.drawEllipse(QPointF(centerX, centerY), radius, radius);
    } else if (m_mode == VisualizerWaveform) {
        if (m_waveformHistory.isEmpty()) return;

        QPainterPath path;
        double step = (double)width() / 80.0;
        double centerY = height() / 2.0;

        painter.setPen(QPen(QColor("#181825"), 1, Qt::DashLine));
        painter.drawLine(0, centerY, width(), centerY);

        QLinearGradient grad(0, 0, width(), 0);
        grad.setColorAt(0.0, QColor("#89b4fa"));
        grad.setColorAt(0.5, QColor("#a6e3a1"));
        grad.setColorAt(1.0, QColor("#f38ba8"));
        painter.setPen(QPen(grad, 2, Qt::SolidLine, Qt::RoundCap));

        for (int i = 0; i < m_waveformHistory.size(); ++i) {
            double x = i * step;
            double y = centerY - (m_waveformHistory[i] / 100.0) * (height() / 2.0 - 4.0);
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        painter.drawPath(path);
    }
}

void PreviewPanel::onEqPresetChanged(int index) {
    if (index == 0) {
        m_sliderBass->setValue(50);
        m_sliderMid->setValue(50);
        m_sliderTreble->setValue(50);
    } else if (index == 1) {
        m_sliderBass->setValue(90);
        m_sliderMid->setValue(50);
        m_sliderTreble->setValue(40);
    } else if (index == 2) {
        m_sliderBass->setValue(30);
        m_sliderMid->setValue(50);
        m_sliderTreble->setValue(90);
    } else if (index == 3) {
        m_sliderBass->setValue(70);
        m_sliderMid->setValue(40);
        m_sliderTreble->setValue(70);
    } else if (index == 4) {
        m_sliderBass->setValue(85);
        m_sliderMid->setValue(65);
        m_sliderTreble->setValue(80);
    }
    onEqSlidersChanged();
}

void PreviewPanel::onEqSlidersChanged() {
    double bass = m_sliderBass->value() / 50.0;
    double mid = m_sliderMid->value() / 50.0;
    double treble = m_sliderTreble->value() / 50.0;
    if (m_visualizer) {
        m_visualizer->setBoost(bass, mid, treble);
    }
}

void PreviewPanel::setSpectrumVisualizerVisible(bool visible) {
    m_spectrumVisualizerEnabled = visible;
    if (m_chkShowVisualizer && m_chkShowVisualizer->isChecked() != visible) {
        m_chkShowVisualizer->setChecked(visible);
    }
    if (m_btnToggleVisualizer) {
        m_btnToggleVisualizer->blockSignals(true);
        m_btnToggleVisualizer->setChecked(visible);
        m_btnToggleVisualizer->setIcon(createVisualizerIcon(visible ? QColor("#89b4fa") : QColor("#cdd6f4")));
        m_btnToggleVisualizer->setToolTip(visible ? "Spectrum Visualizer: ON (Accent Blue)" : "Spectrum Visualizer: OFF");
        m_btnToggleVisualizer->blockSignals(false);
    }
    if (m_visualizer) {
        bool shouldBeVisible = visible && (m_stack->currentWidget() == m_mediaView) && !m_videoWidget->isVisible();
        m_visualizer->setVisible(shouldBeVisible);
    }
}

void PreviewPanel::setZenMode(bool enabled) {
    if (!m_bottomTab) return;
    
    QTabBar* bar = m_bottomTab->findChild<QTabBar*>();
    if (enabled) {
        m_bottomTab->setCurrentIndex(1); // Playlist Queue index
        if (bar) bar->setVisible(false);
    } else {
        if (bar) bar->setVisible(true);
    }
}

void PreviewPanel::onChooseOverlayIcon() {
    IconPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedOverlayIconName = dlg.selectedIconName();
        if (!m_selectedOverlayIconName.isEmpty()) {
            m_btnChooseOverlayIcon->setText(m_selectedOverlayIconName);
            m_btnChooseOverlayIcon->setIcon(QIcon::fromTheme(m_selectedOverlayIconName));
        }
    }
}

void PreviewPanel::onClearOverlayIcon() {
    m_selectedOverlayIconName = "";
    m_btnChooseOverlayIcon->setText("Select...");
    m_btnChooseOverlayIcon->setIcon(QIcon());
}

void PreviewPanel::onApplyTagsColors() {
    if (m_previewedFilePaths.isEmpty() && m_previewedFilePath.isEmpty()) return;

    QString tagsText = m_tagEditorEdit->text();
    QStringList tagsList = tagsText.split(',', Qt::SkipEmptyParts);
    for (QString& tag : tagsList) {
        tag = tag.trimmed();
    }

    QString colorName = m_tagColorCombo->currentText().toLower();
    if (colorName == "none") {
        colorName = "";
    }

    QStringList targets = m_previewedFilePaths;
    if (targets.isEmpty()) {
        targets.append(m_previewedFilePath);
    }

    for (const QString& path : targets) {
        TagManager::instance().setFileTags(path, tagsList);
        TagManager::instance().setFileColor(path, colorName);
        TagManager::instance().setFileOverlayIcon(path, m_selectedOverlayIconName);
    }

    emit tagsChanged(m_previewedFilePath);
}
