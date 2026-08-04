#include "previewpanel.h"
#include "tageditordialog.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <complex>
#include <vector>
#include <functional>
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
#include <QToolButton>
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
#include <QMessageBox>
#include <QDockWidget>
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
#include <QFrame>
#include <QMenu>
#include <QContextMenuEvent>
#include <QAction>
#include <QActionGroup>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QLinearGradient>
#include <QVariantAnimation>
#include <QScrollBar>
#include <QPolygon>
#include <QSizePolicy>
#include <QRandomGenerator>

#include <QScreen>
#include <QWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHelpEvent>

#include "metadatahovercard.h"

class ResumeOverlay : public QFrame {
public:
    ResumeOverlay(QWidget* parent, qint64 savedPos, const QString& formattedTime, std::function<void(bool)> callback)
        : QFrame(parent), m_callback(callback) {
        
        setObjectName("ResumeOverlay");
        setStyleSheet(
            "QFrame#ResumeOverlay { "
            "  background-color: rgba(30, 30, 46, 0.95); "
            "  border: 2px solid #313244; "
            "  border-radius: 12px; "
            "} "
            "QLabel { "
            "  color: #cdd6f4; "
            "  font-family: 'Outfit'; "
            "  font-size: 15px; "
            "} "
            "QPushButton { "
            "  background-color: #313244; "
            "  color: #cdd6f4; "
            "  font-family: 'Outfit'; "
            "  font-size: 14px; "
            "  font-weight: bold; "
            "  border: 1px solid #45475a; "
            "  border-radius: 6px; "
            "  padding: 8px 16px; "
            "} "
            "QPushButton:hover { "
            "  background-color: #89b4fa; "
            "  color: #11111b; "
            "  border: 1px solid #89b4fa; "
            "} "
            "QPushButton#resumeBtn { "
            "  background-color: #89b4fa; "
            "  color: #11111b; "
            "  border: 1px solid #89b4fa; "
            "} "
            "QPushButton#resumeBtn:hover { "
            "  background-color: #b4befe; "
            "  color: #11111b; "
            "  border: 1px solid #b4befe; "
            "}"
        );
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(16);
        
        QLabel* titleLabel = new QLabel("Resume Playback", this);
        titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #89b4fa;");
        titleLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(titleLabel);
        
        QLabel* textLabel = new QLabel(QString("Would you like to resume playing from %1?").arg(formattedTime), this);
        textLabel->setAlignment(Qt::AlignCenter);
        textLabel->setWordWrap(true);
        mainLayout->addWidget(textLabel);
        
        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(12);
        
        QPushButton* btnStartOver = new QPushButton("Start Over", this);
        btnStartOver->setCursor(Qt::PointingHandCursor);
        
        QPushButton* btnResume = new QPushButton("Resume", this);
        btnResume->setObjectName("resumeBtn");
        btnResume->setCursor(Qt::PointingHandCursor);
        
        btnLayout->addWidget(btnStartOver);
        btnLayout->addWidget(btnResume);
        mainLayout->addLayout(btnLayout);
        
        connect(btnResume, &QPushButton::clicked, this, [this]() {
            m_callback(true);
            closeAndDestroy();
        });
        
        connect(btnStartOver, &QPushButton::clicked, this, [this]() {
            m_callback(false);
            closeAndDestroy();
        });
        
        if (parent) {
            parent->installEventFilter(this);
        }
        
        adjustSize();
        centerInParent();
        
        btnResume->setFocus();
    }
    
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            centerInParent();
        }
        return QFrame::eventFilter(watched, event);
    }
    
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            m_callback(false);
            closeAndDestroy();
            event->accept();
        } else {
            QFrame::keyPressEvent(event);
        }
    }
    
private:
    void centerInParent() {
        if (parentWidget()) {
            int x = (parentWidget()->width() - width()) / 2;
            int y = (parentWidget()->height() - height()) / 2;
            move(x, y);
        }
    }
    
    void closeAndDestroy() {
        hide();
        deleteLater();
    }
    
    std::function<void(bool)> m_callback;
};

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

FullscreenWidget::~FullscreenWidget() {
    if (m_hudWidget) {
        m_hudWidget->deleteLater();
    }
}

FullscreenWidget::FullscreenWidget(QWidget* parent) : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint) {
    setStyleSheet("background-color: #000000;");
    setMouseTracking(true);
    installEventFilter(this);

    // Create HUD Overlay Panel
    m_hudWidget = new QFrame(this);
    m_hudWidget->setFixedHeight(110);
    m_hudWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_hudWidget->setObjectName("hudPanel");
    m_hudWidget->setFocusPolicy(Qt::NoFocus);
    m_hudWidget->setStyleSheet(
        "QFrame#hudPanel { background-color: #1e1e2e; border-top: 1px solid #45475a; }"
        "QLabel { color: #cdd6f4; font-size: 12px; font-weight: bold; background: transparent; border: none; }"
        "QPushButton { border: none; background-color: transparent; color: #cdd6f4; border-radius: 18px; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }"
        "QSlider::groove:horizontal { border: none; height: 6px; background: #313244; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #cdd6f4; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
    );

    QStyle* style = QApplication::style();

    // HUD buttons
    int btnSize = 36;
    QSize defaultIconSize(20, 20);

    QPushButton* btnPrev = new QPushButton(m_hudWidget);
    btnPrev->setIcon(style->standardIcon(QStyle::SP_MediaSkipBackward));
    btnPrev->setToolTip("Previous");
    btnPrev->setFocusPolicy(Qt::NoFocus);
    btnPrev->setFixedSize(btnSize, btnSize);
    btnPrev->setIconSize(defaultIconSize);
    connect(btnPrev, &QPushButton::clicked, this, &FullscreenWidget::prevRequested);

    m_btnPlayPause = new QPushButton(m_hudWidget);
    m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    m_btnPlayPause->setToolTip("Play/Pause");
    m_btnPlayPause->setFocusPolicy(Qt::NoFocus);
    m_btnPlayPause->setFixedSize(44, 44);
    m_btnPlayPause->setIconSize(QSize(28, 28));
    m_btnPlayPause->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; border-radius: 22px; } QPushButton:hover { background-color: #b4befe; }");
    connect(m_btnPlayPause, &QPushButton::clicked, this, &FullscreenWidget::onHudPlayPause);

    QPushButton* btnStop = new QPushButton(m_hudWidget);
    btnStop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    btnStop->setToolTip("Stop");
    btnStop->setFocusPolicy(Qt::NoFocus);
    btnStop->setFixedSize(btnSize, btnSize);
    btnStop->setIconSize(defaultIconSize);
    connect(btnStop, &QPushButton::clicked, this, &FullscreenWidget::stopRequested);

    QPushButton* btnNext = new QPushButton(m_hudWidget);
    btnNext->setIcon(style->standardIcon(QStyle::SP_MediaSkipForward));
    btnNext->setToolTip("Next");
    btnNext->setFocusPolicy(Qt::NoFocus);
    btnNext->setFixedSize(btnSize, btnSize);
    btnNext->setIconSize(defaultIconSize);
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
    m_btnSubtitles->setFixedSize(btnSize, btnSize);
    m_btnSubtitles->setStyleSheet("QPushButton { font-weight: bold; border-radius: 18px; }");
    connect(m_btnSubtitles, &QPushButton::clicked, this, &FullscreenWidget::onHudSubtitles);

    m_btnShuffle = new QPushButton(m_hudWidget);
    m_btnShuffle->setIcon(createShuffleIcon(QColor("#cdd6f4")));
    m_btnShuffle->setToolTip("Shuffle Playlist");
    m_btnShuffle->setFocusPolicy(Qt::NoFocus);
    m_btnShuffle->setFixedSize(btnSize, btnSize);
    m_btnShuffle->setIconSize(defaultIconSize);
    connect(m_btnShuffle, &QPushButton::clicked, this, &FullscreenWidget::onHudShuffle);

    m_btnRepeat = new QPushButton(m_hudWidget);
    m_btnRepeat->setIcon(createRepeatIcon(QColor("#cdd6f4"), false));
    m_btnRepeat->setToolTip("Repeat Mode");
    m_btnRepeat->setFocusPolicy(Qt::NoFocus);
    m_btnRepeat->setFixedSize(btnSize, btnSize);
    m_btnRepeat->setIconSize(defaultIconSize);
    connect(m_btnRepeat, &QPushButton::clicked, this, &FullscreenWidget::onHudRepeat);

    m_btnChapters = new QPushButton(m_hudWidget);
    m_btnChapters->setText("📖");
    m_btnChapters->setToolTip("Chapters List");
    m_btnChapters->setFocusPolicy(Qt::NoFocus);
    m_btnChapters->setFixedSize(btnSize, btnSize);
    m_btnChapters->setStyleSheet("QPushButton { color: #cdd6f4; font-size: 16px; background-color: transparent; border: none; } QPushButton:hover { color: #a6e3a1; }");
    connect(m_btnChapters, &QPushButton::clicked, this, &FullscreenWidget::onHudChapters);
    m_btnChapters->setVisible(false);

    m_btnTogglePlaylist = new QPushButton(m_hudWidget);
    m_btnTogglePlaylist->setText("📋");
    m_btnTogglePlaylist->setToolTip("Toggle Fullscreen Playlist");
    m_btnTogglePlaylist->setFocusPolicy(Qt::NoFocus);
    m_btnTogglePlaylist->setFixedSize(btnSize, btnSize);
    m_btnTogglePlaylist->setStyleSheet("QPushButton { color: #cdd6f4; font-size: 16px; background-color: transparent; border: none; } QPushButton:hover { color: #89b4fa; }");
    connect(m_btnTogglePlaylist, &QPushButton::clicked, this, &FullscreenWidget::onHudPlaylist);

    m_btnToggleLyrics = new QPushButton(m_hudWidget);
    m_btnToggleLyrics->setText("LRC");
    m_btnToggleLyrics->setToolTip("Toggle Fullscreen Lyrics Overlay");
    m_btnToggleLyrics->setFocusPolicy(Qt::NoFocus);
    m_btnToggleLyrics->setFixedSize(btnSize, btnSize);
    {
        bool lyricsEnabled = QSettings("Amifiles", "Amifiles").value("preview/show_lyrics", true).toBool();
        if (lyricsEnabled) {
            m_btnToggleLyrics->setStyleSheet("QPushButton { font-weight: bold; color: #a6e3a1; font-family: 'Outfit'; font-size: 11px; background-color: transparent; border: none; } QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }");
        } else {
            m_btnToggleLyrics->setStyleSheet("QPushButton { font-weight: bold; color: #cdd6f4; font-family: 'Outfit'; font-size: 11px; background-color: transparent; border: none; } QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }");
        }
    }
    // Click is handled by parent PreviewPanel directly

    QPushButton* btnExit = new QPushButton(m_hudWidget);
    btnExit->setIcon(style->standardIcon(QStyle::SP_TitleBarNormalButton));
    btnExit->setToolTip("Exit Fullscreen");
    btnExit->setFocusPolicy(Qt::NoFocus);
    btnExit->setFixedSize(btnSize, btnSize);
    btnExit->setIconSize(defaultIconSize);
    connect(btnExit, &QPushButton::clicked, this, &FullscreenWidget::exitRequested);

    // Build three-row layout
    QVBoxLayout* hudMainLayout = new QVBoxLayout(m_hudWidget);
    hudMainLayout->setContentsMargins(15, 12, 15, 12);
    hudMainLayout->setSpacing(8);

    m_lblCurrentPlaying = new QLabel("Now Playing: -", m_hudWidget);
    m_lblCurrentPlaying->setStyleSheet("QLabel { color: #89b4fa; font-size: 14px; font-weight: bold; font-family: 'Outfit'; background: transparent; border: none; }");
    m_lblCurrentPlaying->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_lblCurrentArtwork = new QLabel(m_hudWidget);
    m_lblCurrentArtwork->setFixedSize(36, 36);
    m_lblCurrentArtwork->setStyleSheet("border: 1px solid #45475a; border-radius: 4px; background-color: #11111b;");
    m_lblCurrentArtwork->setScaledContents(true);
    m_lblCurrentArtwork->hide();

    m_lblNextPlaying = new QLabel("", m_hudWidget);
    m_lblNextPlaying->setStyleSheet("QLabel { color: #a6adc8; font-size: 12px; font-family: 'Outfit'; font-style: italic; background: transparent; border: none; }");
    m_lblNextPlaying->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_lblNextArtwork = new QLabel(m_hudWidget);
    m_lblNextArtwork->setFixedSize(24, 24);
    m_lblNextArtwork->setStyleSheet("border: 1px solid #45475a; border-radius: 3px; background-color: #11111b;");
    m_lblNextArtwork->setScaledContents(true);
    m_lblNextArtwork->hide();

    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* currentLayout = new QHBoxLayout();
    currentLayout->setContentsMargins(0, 0, 0, 0);
    currentLayout->setSpacing(8);
    currentLayout->addWidget(m_lblCurrentArtwork);
    currentLayout->addWidget(m_lblCurrentPlaying, 1);
    titleLayout->addLayout(currentLayout, 1);

    QHBoxLayout* nextLayout = new QHBoxLayout();
    nextLayout->setContentsMargins(0, 0, 0, 0);
    nextLayout->setSpacing(8);
    nextLayout->addWidget(m_lblNextPlaying, 1);
    nextLayout->addWidget(m_lblNextArtwork);
    titleLayout->addLayout(nextLayout, 1);

    hudMainLayout->addLayout(titleLayout);

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
    row2Layout->addWidget(m_btnChapters);
    row2Layout->addWidget(m_btnTogglePlaylist);
    row2Layout->addWidget(m_btnToggleLyrics);
    
    row2Layout->addStretch(1);

    // Built-in doubleclick auto fullscreen control on the HUD itself
    m_btnToggleAutoFS = new QPushButton(m_hudWidget);
    m_btnToggleAutoFS->setCheckable(true);
    m_btnToggleAutoFS->setFocusPolicy(Qt::NoFocus);
    m_btnToggleAutoFS->setFixedSize(36, 36);
    m_btnToggleAutoFS->setIconSize(QSize(20, 20));
    connect(m_btnToggleAutoFS, &QPushButton::clicked, this, [this]() {
        emit builtinPlayerDoubleclickToggled(m_btnToggleAutoFS->isChecked());
    });

    // Query active state to set initial color/icon
    QWidget* pTemp = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->inherits("MainWindow")) {
            pTemp = w;
            break;
        }
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
    updateHudGeometry();
}

void FullscreenWidget::updateHudGeometry() {
    // Layout manager handles size and geometry automatically
}

void FullscreenWidget::setPlaylist(const QStringList& playlist, int currentIndex) {
    m_playlistItems = playlist;
    m_playlistCurrentIndex = currentIndex;
}

void FullscreenWidget::togglePlaylistDrawer() {
    if (m_playlistItems.isEmpty()) return;

    QMenu menu(this);
    m_activeMenu = &menu;
    m_menuCanceled = false;
    menu.installEventFilter(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 20px 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #89b4fa; color: #11111b; }"
    );

    QAction* titleAct = menu.addAction("Active Playlist");
    titleAct->setEnabled(false);
    menu.addSeparator();

    for (int i = 0; i < m_playlistItems.size(); ++i) {
        QFileInfo fi(m_playlistItems[i]);
        QString name = fi.completeBaseName();
        if (i == m_playlistCurrentIndex) {
            name = "▶  " + name;
        } else {
            name = "    " + name;
        }

        QAction* act = menu.addAction(name);
        if (i == m_playlistCurrentIndex) {
            QFont f = act->font();
            f.setBold(true);
            act->setFont(f);
        }
        connect(act, &QAction::triggered, this, [this, i]() {
            emit playlistItemSelected(i);
        });
    }

    QPoint popupPos;
    if (m_btnTogglePlaylist && m_btnTogglePlaylist->underMouse()) {
        popupPos = m_btnTogglePlaylist->mapToGlobal(QPoint(0, -menu.sizeHint().height()));
    } else {
        QPoint center = rect().center();
        popupPos = mapToGlobal(center) - QPoint(menu.sizeHint().width() / 2, menu.sizeHint().height() / 2);
    }
    menu.exec(popupPos);

    m_activeMenu = nullptr;
}

void FullscreenWidget::onHudPlaylist() {
    togglePlaylistDrawer();
}

void FullscreenWidget::setMediaState(bool isVideo, QMediaPlayer* player, QAudioOutput* audioOutput) {
    m_player = player;
    m_audioOutput = audioOutput;
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

static QPixmap getMediaArtwork(const QString& filePath) {
    if (filePath.isEmpty()) return QPixmap();

    // 1. Try exiftool to extract embedded artwork
    QProcess proc;
    proc.start("exiftool", {"-Picture", "-b", filePath});
    if (proc.waitForFinished(1500)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            QPixmap pix;
            if (pix.loadFromData(imgData)) {
                return pix;
            }
        }
    }

    // 2. Try looking in the folder
    QString dirPath = QFileInfo(filePath).absolutePath();
    QDir dir(dirPath);
    QStringList artNames = { "folder", "cover", "album", "poster" };
    QStringList artExts = { "jpg", "jpeg", "png", "webp" };
    for (const QString& name : artNames) {
        for (const QString& ext : artExts) {
            QString potential = dir.filePath(name + "." + ext);
            if (QFile::exists(potential)) {
                QPixmap pix(potential);
                if (!pix.isNull()) {
                    return pix;
                }
            }
            potential = dir.filePath(name.toUpper() + "." + ext);
            if (QFile::exists(potential)) {
                QPixmap pix(potential);
                if (!pix.isNull()) {
                    return pix;
                }
            }
        }
    }
    return QPixmap();
}

static QString getMediaDisplayTitle(const QString& filePath) {
    QFileInfo info(filePath);
    if (filePath.isEmpty()) return "";

    // Parse TV Show structures (e.g. Grandparent/Parent/File.mp4 where Parent is "Season X")
    QString parentDirName = info.dir().dirName();
    QString grandParentDirName = QDir(info.absolutePath() + "/..").dirName();

    QRegularExpression seasonRegex("^(Season\\s*\\d+|S\\d+)", QRegularExpression::CaseInsensitiveOption);
    if (seasonRegex.match(parentDirName).hasMatch() && !grandParentDirName.isEmpty() && grandParentDirName != "." && grandParentDirName != "..") {
        return QString("%1 - %2 - %3").arg(grandParentDirName).arg(parentDirName).arg(info.completeBaseName());
    }

    // Try filename patterns like "Show Name - S01E02"
    QRegularExpression epRegex("^(.*)\\s+-\\s+(S\\d+E\\d+|\\d+x\\d+)\\s+-\\s+(.*)$", QRegularExpression::CaseInsensitiveOption);
    auto epMatch = epRegex.match(info.completeBaseName());
    if (epMatch.hasMatch()) {
        return QString("%1 (%2) - %3").arg(epMatch.captured(1)).arg(epMatch.captured(2)).arg(epMatch.captured(3));
    }

    // Default to metadata title
    FileMetadata meta = MetadataExtractor::extract(filePath);
    if (!meta.title.isEmpty()) {
        if (!meta.artist.isEmpty()) {
            return QString("%1 - %2").arg(meta.artist).arg(meta.title);
        }
        return meta.title;
    }

    return info.completeBaseName();
}

void FullscreenWidget::setTrackNames(const QString& currentPath, const QString& nextPath) {
    m_currentTrackPath = currentPath;
    if (m_lblCurrentPlaying) {
        if (currentPath.isEmpty()) {
            m_lblCurrentPlaying->setText("Now Playing: -");
            m_lblCurrentPlaying->setToolTip("");
            m_lblCurrentArtwork->clear();
            m_lblCurrentArtwork->hide();
        } else {
            QString dispTitle = getMediaDisplayTitle(currentPath);
            m_lblCurrentPlaying->setText("Now Playing: " + dispTitle);
            m_lblCurrentPlaying->setToolTip(dispTitle);

            QPixmap art = getMediaArtwork(currentPath);
            if (!art.isNull()) {
                m_lblCurrentArtwork->setPixmap(art);
                m_lblCurrentArtwork->show();
            } else {
                m_lblCurrentArtwork->clear();
                m_lblCurrentArtwork->hide();
            }
        }
    }

    if (m_lblNextPlaying) {
        if (nextPath.isEmpty()) {
            m_lblNextPlaying->setText("");
            m_lblNextPlaying->setToolTip("");
            m_lblNextArtwork->clear();
            m_lblNextArtwork->hide();
        } else {
            QString dispTitle = getMediaDisplayTitle(nextPath);
            m_lblNextPlaying->setText("Up Next: " + dispTitle);
            m_lblNextPlaying->setToolTip(dispTitle);

            QPixmap art = getMediaArtwork(nextPath);
            if (!art.isNull()) {
                m_lblNextArtwork->setPixmap(art);
                m_lblNextArtwork->show();
            } else {
                m_lblNextArtwork->clear();
                m_lblNextArtwork->hide();
            }
        }
    }

    // Query chapters asynchronously
    if (m_sliderProgress) {
        m_sliderProgress->setChapters({});
    }
    if (m_btnChapters) {
        m_btnChapters->setVisible(false);
    }

    if (!currentPath.isEmpty()) {
        QProcess* proc = new QProcess(this);
        connect(proc, &QProcess::finished, this, [this, proc](int exitCode) {
            if (exitCode == 0) {
                QByteArray data = proc->readAllStandardOutput();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject root = doc.object();
                    QJsonArray chaptersArr = root["chapters"].toArray();
                    QList<ScrubSlider::Chapter> list;
                    for (const QJsonValue& val : chaptersArr) {
                        QJsonObject cObj = val.toObject();
                        ScrubSlider::Chapter ch;
                        double startTimeSecs = cObj["start_time"].toString().toDouble();
                        ch.startMs = static_cast<qint64>(startTimeSecs * 1000.0);
                        
                        QJsonObject tags = cObj["tags"].toObject();
                        ch.title = tags["title"].toString();
                        if (ch.title.isEmpty()) {
                            ch.title = QString("Chapter %1").arg(cObj["id"].toInt() + 1);
                        }
                        list.append(ch);
                    }

                    if (!list.isEmpty()) {
                        if (m_sliderProgress) {
                            m_sliderProgress->setChapters(list);
                        }
                        if (m_btnChapters) {
                            m_btnChapters->setVisible(true);
                        }
                    }
                }
            }
            proc->deleteLater();
        });
        proc->start("ffprobe", {"-print_format", "json", "-show_chapters", currentPath});
    }
}

void FullscreenWidget::showHud() {
    updateHudGeometry();
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

void FullscreenWidget::onHudChapters() {
    if (!m_sliderProgress || !m_player) return;
    
    QList<ScrubSlider::Chapter> chapters = m_sliderProgress->chapters();
    if (chapters.isEmpty()) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 20px 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #a6e3a1; color: #11111b; }"
    );

    for (const auto& ch : chapters) {
        qint64 secs = ch.startMs / 1000;
        qint64 mins = secs / 60;
        secs = secs % 60;
        QString timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
        
        QAction* act = menu.addAction(QString("%1 (%2)").arg(ch.title).arg(timeStr));
        connect(act, &QAction::triggered, this, [this, startMs = ch.startMs]() {
            m_player->setPosition(startMs);
        });
    }

    if (m_btnChapters) {
        QPoint pos = m_btnChapters->mapToGlobal(QPoint(0, 0));
        pos.setY(pos.y() - menu.sizeHint().height());
        menu.exec(pos);
    }
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
    if (m_activeMenu && event->type() == QEvent::KeyPress) {
        if (watched == m_activeMenu || (watched && watched->inherits("QMenu"))) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->isAutoRepeat()) {
                return true;
            }
            QSettings settings("Amifiles", "Amifiles");
            QKeySequence shortcutMenu(settings.value("shortcuts/player_menu", "C").toString());
            QKeySequence shortcutPlaylist(settings.value("shortcuts/player_playlist", "L").toString());
            QKeySequence pressed(keyEvent->modifiers() | keyEvent->key());
            if (pressed == shortcutMenu || keyEvent->key() == Qt::Key_Menu) {
                m_menuCanceled = true;
                m_activeMenu->close();
                return true;
            }
            if (pressed == shortcutPlaylist) {
                m_activeMenu->close();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FullscreenWidget::keyPressEvent(QKeyEvent* event) {
    if (m_activeMenu) {
        event->accept();
        return;
    }
    showHud();
    
    QSettings settings("Amifiles", "Amifiles");
    bool remoteMode = settings.value("preferences/keyboard_remote_mode", false).toBool();
    
    QKeySequence pressed(event->modifiers() | event->key());
    QKeySequence shortcutPlayPause(settings.value("shortcuts/player_play_pause", "Space").toString());
    QKeySequence shortcutPrev(settings.value("shortcuts/player_prev", "P").toString());
    QKeySequence shortcutNext(settings.value("shortcuts/player_next", "N").toString());
    QKeySequence shortcutMute(settings.value("shortcuts/player_mute", "M").toString());
    QKeySequence shortcutMenu(settings.value("shortcuts/player_menu", "C").toString());
    QKeySequence shortcutNavigateBack(settings.value("shortcuts/navigate_back", "Alt+Left").toString());
    QKeySequence shortcutNavigateUp(settings.value("shortcuts/navigate_up", "Backspace").toString());
    QKeySequence shortcutPlaylist(settings.value("shortcuts/player_playlist", "L").toString());

    {
        QFile logFile("/home/dave/cpp_projects/Amifiles/menu_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << "FS KeyPress: key=" << event->key()
                << " modifiers=" << (int)event->modifiers()
                << " pressed=" << pressed.toString()
                << " shortcutNext=" << shortcutNext.toString()
                << " shortcutPrev=" << shortcutPrev.toString()
                << "\n";
        }
    }

    if (pressed == shortcutPlaylist) {
        togglePlaylistDrawer();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape || 
        event->key() == Qt::Key_F || 
        event->key() == Qt::Key_Back || 
        event->key() == Qt::Key_Backspace ||
        pressed == shortcutNavigateBack || 
        pressed == shortcutNavigateUp) {
        emit exitRequested();
    } else if (pressed == shortcutPlayPause) {
        emit playPauseRequested();
        event->accept();
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
    } else if (remoteMode && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        if (m_audioOutput) {
            float vol = m_audioOutput->volume();
            float nextVol = (event->key() == Qt::Key_Up) ? qMin(1.0f, vol + 0.05f) : qMax(0.0f, vol - 0.05f);
            m_audioOutput->setVolume(nextVol);
            if (m_sliderVolume) {
                m_sliderVolume->setValue(qRound(nextVol * 100.0f));
            }
            event->accept();
        }
    } else if (pressed == shortcutMute) {
        if (m_audioOutput) {
            m_audioOutput->setMuted(!m_audioOutput->isMuted());
            event->accept();
        }
    } else if (pressed == shortcutNext) {
        emit nextRequested();
        event->accept();
    } else if (pressed == shortcutPrev || event->key() == Qt::Key_B) {
        emit prevRequested();
        event->accept();
    } else if (pressed == shortcutMenu || event->key() == Qt::Key_Menu) {
        event->accept();
        if (!event->isAutoRepeat()) {
            QTimer::singleShot(0, this, [this]() {
                QPoint center = rect().center();
                QContextMenuEvent contextEvent(QContextMenuEvent::Keyboard, center, mapToGlobal(center));
                contextMenuEvent(&contextEvent);
            });
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
    m_activeMenu = &menu;
    m_menuCanceled = false;
    menu.installEventFilter(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 20px 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #89b4fa; color: #11111b; }"
    );
    
    QAction* actExit = menu.addAction("Exit Fullscreen (Esc)");
    menu.addSeparator();
    
    QAction* actResume = nullptr;
    QAction* actRestart = nullptr;
    qint64 savedPos = 0;
    QSettings settings("Amifiles", "Amifiles");
    if (!m_currentTrackPath.isEmpty()) {
        savedPos = settings.value(QString("watched_progress/%1").arg(m_currentTrackPath), 0).toLongLong();
        if (savedPos > 5000) {
            qint64 secs = savedPos / 1000;
            qint64 mins = secs / 60;
            secs = secs % 60;
            QString timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
            
            actResume = menu.addAction(QString("⏯ Resume Playback from %1").arg(timeStr));
            actRestart = menu.addAction("🔄 Restart Episode (Play from Beginning)");
            menu.addSeparator();
        }
    }

    QAction* actAutoQueue = menu.addAction("Auto-Queue Sibling Files");
    actAutoQueue->setCheckable(true);
    actAutoQueue->setChecked(settings.value("preview/auto_queue_sibling_files", true).toBool());
    menu.addSeparator();

    QAction* actPlayPause = menu.addAction("Play / Pause (Space)");
    QAction* actStop = menu.addAction("Stop");
    QAction* actPrev = menu.addAction("Previous Track (P/B)");
    QAction* actNext = menu.addAction("Next Track (N)");
    
    QAction* actPrevChapter = nullptr;
    QAction* actNextChapter = nullptr;
    QList<ScrubSlider::Chapter> chapters;
    if (m_sliderProgress) {
        chapters = m_sliderProgress->chapters();
        if (!chapters.isEmpty()) {
            menu.addSeparator();
            actPrevChapter = menu.addAction("⏮ Jump to Previous Chapter");
            actNextChapter = menu.addAction("⏭ Jump to Next Chapter");
        }
    }

    QMenu* submenuChapters = nullptr;
    if (m_sliderProgress) {
        if (!chapters.isEmpty()) {
            menu.addSeparator();
            submenuChapters = menu.addMenu("📖 Chapters");
            submenuChapters->setStyleSheet(menu.styleSheet());
            submenuChapters->installEventFilter(this);
            for (const auto& ch : chapters) {
                qint64 secs = ch.startMs / 1000;
                qint64 mins = secs / 60;
                secs = secs % 60;
                QString timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
                
                QAction* chAct = submenuChapters->addAction(QString("%1 (%2)").arg(ch.title).arg(timeStr));
                connect(chAct, &QAction::triggered, this, [this, start = ch.startMs]() {
                    if (m_player) m_player->setPosition(start);
                });
            }
        }
    }
    
    QAction* selected = menu.exec(event->globalPos());
    m_activeMenu = nullptr;

    if (m_menuCanceled) {
        selected = nullptr;
    }

    {
        QFile logFile("/home/dave/cpp_projects/Amifiles/menu_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << "Selected action: " << (selected ? selected->text() : "nullptr") << " (m_menuCanceled=" << m_menuCanceled << ")\n";
        }
    }

    if (selected) {
        if (selected == actExit) {
            emit exitRequested();
        } else if (selected == actResume) {
            if (m_player) {
                m_player->setPosition(savedPos);
                m_player->play();
            }
        } else if (selected == actRestart) {
            if (m_player) {
                m_player->setPosition(0);
                m_player->play();
            }
        } else if (selected == actPlayPause) {
            emit playPauseRequested();
        } else if (selected == actStop) {
            emit stopRequested();
        } else if (selected == actPrev) {
            emit prevRequested();
        } else if (selected == actNext) {
            emit nextRequested();
        } else if (selected == actPrevChapter) {
            if (m_player) {
                qint64 currentPos = m_player->position();
                int prevIdx = -1;
                for (int i = 0; i < chapters.size(); ++i) {
                    if (chapters[i].startMs < currentPos - 2000) {
                        prevIdx = i;
                    } else {
                        break;
                    }
                }
                if (prevIdx != -1) {
                    m_player->setPosition(chapters[prevIdx].startMs);
                } else {
                    m_player->setPosition(0);
                }
            }
        } else if (selected == actNextChapter) {
            if (m_player) {
                qint64 currentPos = m_player->position();
                int nextIdx = -1;
                for (int i = 0; i < chapters.size(); ++i) {
                    if (chapters[i].startMs > currentPos + 1000) {
                        nextIdx = i;
                        break;
                    }
                }
                if (nextIdx != -1) {
                    m_player->setPosition(chapters[nextIdx].startMs);
                }
            }
        } else if (selected == actAutoQueue) {
            settings.setValue("preview/auto_queue_sibling_files", actAutoQueue->isChecked());
            PreviewPanel* pPanel = qobject_cast<PreviewPanel*>(parentWidget());
            if (pPanel) {
                pPanel->loadPreferences();
            }
        }
    }
}

PreviewPanel::PreviewPanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(200);
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
    m_imageLabel->setMouseTracking(true);
    m_imageLabel->setToolTip(" ");
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
    mediaLayout->setContentsMargins(4, 4, 4, 16);

    m_videoWidget = new QVideoWidget(m_mediaView);
    m_videoWidget->setStyleSheet("background-color: #000000; border-radius: 4px;");
    m_videoWidget->setMinimumHeight(150);

    m_audioPlaceholder = new AudioPlaceholderWidget(m_mediaView);
    m_audioPlaceholder->setMinimumHeight(150);
    m_audioPlaceholder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_audioPlaceholder->setMouseTracking(true);
    m_audioPlaceholder->setToolTip(" ");

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

    m_btnAutoPreview = new QPushButton(this);
    m_btnAutoPreview->setCheckable(true);
    m_btnAutoPreview->setMaximumSize(32, 28);
    m_btnAutoPreview->setIcon(style->standardIcon(QStyle::SP_BrowserReload));
    m_btnAutoPreview->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  border: 1px solid #45475a;"
        "  border-radius: 4px;"
        "  padding: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #313244;"
        "}"
        "QPushButton:checked {"
        "  background-color: #89b4fa;"
        "  border-color: #89b4fa;"
        "}"
    );
    m_btnAutoPreview->setToolTip("Auto-Preview: OFF");

    bool autoPreviewVal = settings.value("preview/auto_preview_enabled", false).toBool();
    m_btnAutoPreview->setChecked(autoPreviewVal);
    if (autoPreviewVal) {
        m_btnAutoPreview->setToolTip("Auto-Preview: ON");
    }

    connect(m_btnAutoPreview, &QPushButton::toggled, this, [this](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preview/auto_preview_enabled", checked);
        m_btnAutoPreview->setToolTip(checked ? "Auto-Preview: ON" : "Auto-Preview: OFF");
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

    QHBoxLayout* controlsRow1 = new QHBoxLayout();
    controlsRow1->setContentsMargins(0, 0, 0, 0);
    controlsRow1->setSpacing(4);
    controlsRow1->addWidget(m_btnPrevTrack);
    controlsRow1->addWidget(m_btnPlayPause);
    controlsRow1->addWidget(m_btnStop);
    controlsRow1->addWidget(m_btnNextTrack);
    controlsRow1->addWidget(m_btnFullscreen);
    controlsRow1->addStretch(1);
    controlsRow1->addWidget(lblVol);
    controlsRow1->addWidget(m_sliderVolume);

    QHBoxLayout* controlsRow2 = new QHBoxLayout();
    controlsRow2->setContentsMargins(0, 0, 0, 0);
    controlsRow2->setSpacing(4);
    controlsRow2->addWidget(m_btnShuffle);
    controlsRow2->addWidget(m_btnRepeat);
    controlsRow2->addWidget(m_btnToggleVisualizer);
    controlsRow2->addWidget(m_btnSubtitles);
    controlsRow2->addWidget(m_btnAutoFS20s);
    controlsRow2->addWidget(m_btnAutoPreview);
    controlsRow2->addStretch(1);

    QVBoxLayout* controlsLayout = new QVBoxLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(4);
    controlsLayout->addLayout(controlsRow1);
    controlsLayout->addLayout(controlsRow2);

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
    m_bottomTab->tabBar()->setUsesScrollButtons(true);
    m_bottomTab->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: transparent; border-radius: 4px; }"
        "QTabBar::tab { background-color: #11111b; color: #a6adc8; padding: 4px 8px; font-size: 11px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
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

    // Initialize Music Tags Quick Editor (hidden/removed by default)
    m_musicTagsContainer = new QWidget(this);
    QVBoxLayout* musicTagsLayout = new QVBoxLayout(m_musicTagsContainer);
    musicTagsLayout->setContentsMargins(6, 6, 6, 6);
    musicTagsLayout->setSpacing(4);

    QFormLayout* musicTagsForm = new QFormLayout();
    musicTagsForm->setSpacing(4);

    QString editStyle = "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 3px; }";

    m_musicEditTitle = new QLineEdit(m_musicTagsContainer);
    m_musicEditTitle->setStyleSheet(editStyle);
    musicTagsForm->addRow("Title:", m_musicEditTitle);

    m_musicEditArtist = new QLineEdit(m_musicTagsContainer);
    m_musicEditArtist->setStyleSheet(editStyle);
    musicTagsForm->addRow("Artist:", m_musicEditArtist);

    m_musicEditAlbum = new QLineEdit(m_musicTagsContainer);
    m_musicEditAlbum->setStyleSheet(editStyle);
    musicTagsForm->addRow("Album:", m_musicEditAlbum);

    m_musicEditGenre = new QLineEdit(m_musicTagsContainer);
    m_musicEditGenre->setStyleSheet(editStyle);
    musicTagsForm->addRow("Genre:", m_musicEditGenre);

    m_musicEditYear = new QLineEdit(m_musicTagsContainer);
    m_musicEditYear->setStyleSheet(editStyle);
    musicTagsForm->addRow("Year:", m_musicEditYear);

    m_musicEditTrack = new QLineEdit(m_musicTagsContainer);
    m_musicEditTrack->setStyleSheet(editStyle);
    musicTagsForm->addRow("Track:", m_musicEditTrack);

    m_musicEditLyrics = new QPlainTextEdit(m_musicTagsContainer);
    m_musicEditLyrics->setStyleSheet("QPlainTextEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 3px; }");
    m_musicEditLyrics->setMaximumHeight(70);
    musicTagsForm->addRow("Lyrics:", m_musicEditLyrics);

    musicTagsLayout->addLayout(musicTagsForm);

    m_btnSaveMusicTags = new QPushButton("Save Tags", m_musicTagsContainer);
    m_btnSaveMusicTags->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px; }"
                                      "QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnSaveMusicTags, &QPushButton::clicked, this, &PreviewPanel::onSaveMusicTags);
    musicTagsLayout->addWidget(m_btnSaveMusicTags);
    musicTagsLayout->addStretch(1);

    m_bottomTab->addTab(m_metadataContainer, "Properties");
    m_bottomTab->setTabToolTip(m_bottomTab->indexOf(m_metadataContainer), "File Properties");

    // Tab 2: Playlist Queue
    QWidget* playlistTab = new QWidget(this);
    QVBoxLayout* playlistLayout = new QVBoxLayout(playlistTab);
    playlistLayout->setContentsMargins(4, 4, 4, 4);
    playlistLayout->setSpacing(4);

    QHBoxLayout* playlistToolbar = new QHBoxLayout();
    playlistToolbar->setContentsMargins(0, 0, 0, 0);
    playlistToolbar->setSpacing(4);

    QPushButton* btnPlaySelected = new QPushButton("Play", this);
    btnPlaySelected->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    btnPlaySelected->setStyleSheet("QPushButton { padding: 4px 8px; font-size: 11px; }");

    QPushButton* btnRemoveSelected = new QPushButton("Remove", this);
    btnRemoveSelected->setIcon(style->standardIcon(QStyle::SP_DialogCloseButton));
    btnRemoveSelected->setStyleSheet("QPushButton { padding: 4px 8px; font-size: 11px; }");

    QPushButton* btnClearQueue = new QPushButton("Clear", this);
    btnClearQueue->setIcon(style->standardIcon(QStyle::SP_TrashIcon));
    btnClearQueue->setStyleSheet("QPushButton { padding: 4px 8px; font-size: 11px; }");

    m_chkAutoQueue = new QCheckBox("Auto-Queue Sibling Files", this);
    m_chkAutoQueue->setStyleSheet("QCheckBox { font-size: 11px; color: #a6adc8; padding-left: 6px; }");
    m_chkAutoQueue->setChecked(settings.value("preview/auto_queue_sibling_files", true).toBool());
    connect(m_chkAutoQueue, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preview/auto_queue_sibling_files", checked);
    });

    playlistToolbar->addWidget(btnPlaySelected);
    playlistToolbar->addWidget(btnRemoveSelected);
    playlistToolbar->addWidget(btnClearQueue);
    playlistToolbar->addWidget(m_chkAutoQueue);
    playlistToolbar->addStretch(1);

    m_playlistList = new QListWidget(this);
    m_playlistList->setIconSize(QSize(40, 40));
    m_playlistList->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; color: #cdd6f4; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background-color: #313244; color: #a6e3a1; }"
    );
    m_playlistList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistList, &QListWidget::itemDoubleClicked, this, &PreviewPanel::onPlaylistItemDoubleClicked);
    connect(m_playlistList, &QListWidget::customContextMenuRequested, this, &PreviewPanel::showPlaylistContextMenu);

    playlistLayout->addLayout(playlistToolbar);
    playlistLayout->addWidget(m_playlistList);

    connect(btnPlaySelected, &QPushButton::clicked, this, [this]() {
        if (m_playlistList) {
            int row = m_playlistList->currentRow();
            if (row >= 0 && row < m_playlist.size()) {
                playPlaylistIndex(row);
                return;
            }
        }
        if (m_player) {
            m_player->play();
        }
    });

    connect(btnRemoveSelected, &QPushButton::clicked, this, [this]() {
        if (m_playlistList) {
            int row = m_playlistList->currentRow();
            if (row >= 0 && row < m_playlist.size()) {
                removeFromPlaylist(row);
            }
        }
    });

    connect(btnClearQueue, &QPushButton::clicked, this, &PreviewPanel::clearPlaylist);

    m_bottomTab->addTab(playlistTab, "Playlist Queue");
    m_bottomTab->setTabToolTip(m_bottomTab->indexOf(playlistTab), "Playlist Queue");

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
        if (m_fullscreenVisualizer) {
            m_fullscreenVisualizer->setVisualizerMode(static_cast<SpectrumVisualizerWidget::VisualizerMode>(idx));
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
    m_bottomTab->setTabToolTip(m_bottomTab->indexOf(eqContainer), "Audio Equalizer");

    m_hexViewer = new HexEditorWidget(this);
    m_bottomTab->addTab(m_hexViewer, "Hex Viewer");
    m_bottomTab->setTabToolTip(m_bottomTab->indexOf(m_hexViewer), "Hex Viewer");

    m_textContainer = new QWidget(this);
    QVBoxLayout* textContainerLayout = new QVBoxLayout(m_textContainer);
    textContainerLayout->setContentsMargins(4, 4, 4, 4);
    textContainerLayout->setSpacing(4);

    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(6);

    QLabel* searchIcon = new QLabel("🔍", m_textContainer);
    searchIcon->setStyleSheet("QLabel { font-size: 12px; color: #a6adc8; }");
    searchLayout->addWidget(searchIcon);

    m_textSearchEdit = new QLineEdit(m_textContainer);
    m_textSearchEdit->setPlaceholderText("Search inside text document...");
    m_textSearchEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #313244;"
        "  color: #cdd6f4;"
        "  border: 1px solid #45475a;"
        "  border-radius: 4px;"
        "  padding: 2px 6px;"
        "  font-size: 11px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #89b4fa;"
        "}"
    );
    searchLayout->addWidget(m_textSearchEdit, 1);

    m_lblTextSearchMatches = new QLabel(m_textContainer);
    m_lblTextSearchMatches->setStyleSheet("QLabel { font-size: 11px; color: #a6adc8; }");
    searchLayout->addWidget(m_lblTextSearchMatches);

    QToolButton* btnClear = new QToolButton(m_textContainer);
    btnClear->setText("✕");
    btnClear->setToolTip("Clear search");
    btnClear->setStyleSheet(
        "QToolButton {"
        "  background: transparent;"
        "  color: #a6adc8;"
        "  border: none;"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  padding: 2px;"
        "}"
        "QToolButton:hover {"
        "  color: #f38ba8;"
        "}"
    );
    connect(btnClear, &QToolButton::clicked, m_textSearchEdit, &QLineEdit::clear);
    searchLayout->addWidget(btnClear);

    m_textTabs = new QTabWidget(m_textContainer);
    m_textTabs->setTabsClosable(true);
    m_textTabs->setMovable(true);
    m_textTabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: #11111b; }"
        "QTabBar::tab { background-color: #181825; color: #a6adc8; border: 1px solid #313244; padding: 4px 8px; font-size: 11px; }"
        "QTabBar::tab:selected { background-color: #11111b; color: #cdd6f4; }"
    );
    m_textTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_textTabs, &QTabWidget::tabCloseRequested, this, &PreviewPanel::onTextTabCloseRequested);
    connect(m_textTabs, &QTabWidget::customContextMenuRequested, this, &PreviewPanel::showTextTabsContextMenu);
    connect(m_textTabs, &QTabWidget::currentChanged, this, [this]() {
        onTextSearchChanged(m_textSearchEdit->text());
    });

    connect(m_textSearchEdit, &QLineEdit::textChanged, this, &PreviewPanel::onTextSearchChanged);

    textContainerLayout->addLayout(searchLayout);
    textContainerLayout->addWidget(m_textTabs, 1);

    m_bottomTab->addTab(m_textContainer, "Document Text");
    m_bottomTab->setTabToolTip(m_bottomTab->indexOf(m_textContainer), "Document Text Viewer");

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
    if (m_isVideo && !m_previewedFilePath.isEmpty()) {
        QSettings settings("Amifiles", "Amifiles");
        if (settings.value("preview/resume_progress", false).toBool()) {
            qint64 pos = m_player->position();
            qint64 dur = m_player->duration();
            if (pos > 5000) {
                if (dur > 0 && pos >= dur * 0.95) {
                    settings.remove(QString("watched_progress/%1").arg(m_previewedFilePath));
                    TagManager::instance().setFileOverlayIcon(m_previewedFilePath, "emblem-ok");
                    emit tagsChanged(m_previewedFilePath);
                } else {
                    settings.setValue(QString("watched_progress/%1").arg(m_previewedFilePath), pos);
                }
            }
        }
    }

    m_player->stop();
    m_player->setSource(QUrl());
    if (m_visualizer) {
        m_visualizer->setPlayer(nullptr);
        m_visualizer->setAudioPath("");
    }
    if (m_autoFsTimer) {
        m_autoFsTimer->stop();
    }
    m_previewedFilePath.clear();
    m_isVideo = false;
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
    if (m_audioPlaceholder) {
        m_audioPlaceholder->setFilePath("");
    }

    m_stack->setCurrentWidget(m_emptyView);

    m_metadataTable->setRowCount(0);
}

static bool isLikelyTextFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QByteArray buffer = file.read(1024);
    file.close();
    
    if (buffer.isEmpty()) return true; // empty file can be treated as text
    
    // Check for null bytes which indicate binary files
    if (buffer.contains('\0')) return false;
    
    // Check for a high ratio of printable characters
    int printable = 0;
    for (char c : buffer) {
        // Tab, carriage return, line feed, or printable ASCII (32-126)
        if (c == '\t' || c == '\n' || c == '\r' || (c >= 32 && c <= 126) || (unsigned char)c >= 128) {
            printable++;
        }
    }
    return (double)printable / buffer.size() > 0.9;
}

void PreviewPanel::previewFile(const QString& filePath, const QStringList& siblingSelections, bool startPlaying, bool keepCurrentPlaylist) {
    if (m_forcePlayNext) {
        m_prePreviewPlaybackState = QMediaPlayer::PlayingState;
        m_forcePlayNext = false;
    } else {
        m_prePreviewPlaybackState = startPlaying ? m_player->playbackState() : QMediaPlayer::StoppedState;
    }
    m_previewedFilePaths = siblingSelections;
    if (m_previewedFilePaths.isEmpty() && !filePath.isEmpty()) {
        m_previewedFilePaths.append(filePath);
    }

    if (!this->isFullscreen() && !filePath.isEmpty()) {
        // Embedded preview panel folder cycling logic
        QFileInfo fileInfo(filePath);
        QString ext = fileInfo.suffix().toLower();
        QString parentDir = fileInfo.absolutePath();
        
        QStringList txtExts = { "txt", "log", "ini", "cfg", "conf", "json", "xml", "html", "css", "js", 
                                "py", "cpp", "h", "sh", "md", "csv", "yml", "yaml", "properties" };
        QStringList imgExts = { "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg" };
        QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a", "mod", "xm", "s3m", "it", "sid" };
        QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
        
        QStringList filterExtensions;
        if (txtExts.contains(ext)) filterExtensions = txtExts;
        else if (imgExts.contains(ext)) filterExtensions = imgExts;
        else if (audioExts.contains(ext)) filterExtensions = audioExts;
        else if (videoExts.contains(ext)) filterExtensions = videoExts;
        
        bool autoQueue = m_chkAutoQueue && m_chkAutoQueue->isChecked();
        if (!keepCurrentPlaylist) {
            if (!autoQueue) {
                m_playlist.clear();
                m_playlist.append(filePath);
                m_playlistIndex = 0;
                if (m_playlistList) {
                    refreshPlaylistUI();
                }
            } else {
                // Only reconstruct playlist if current file is not already in the active playlist
                int existingIdx = m_playlist.indexOf(filePath);
                if (existingIdx != -1) {
                    m_playlistIndex = existingIdx;
                    if (m_playlistList) {
                        m_playlistList->setCurrentRow(m_playlistIndex);
                    }
                } else if (!filterExtensions.isEmpty()) {
                    QDir dir(parentDir);
                    QStringList filters;
                    for (const QString& fExt : filterExtensions) {
                        filters << "*." + fExt;
                    }
                    QStringList folderFiles = dir.entryList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase);
                    
                    QStringList fullPaths;
                    for (const QString& name : folderFiles) {
                        fullPaths.append(dir.filePath(name));
                    }
                    
                    int idx = fullPaths.indexOf(filePath);
                    if (idx != -1) {
                        m_playlist = fullPaths;
                        m_playlistIndex = idx;
                        
                        if (m_playlistList) {
                            refreshPlaylistUI();
                        }
                    }
                }
            }
        }
    } else if (!m_playlist.isEmpty()) {
        int idx = m_playlist.indexOf(filePath);
        if (idx != -1) {
            m_playlistIndex = idx;
            if (m_playlistList) {
                m_playlistList->setCurrentRow(m_playlistIndex);
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
    QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a", "mod", "xm", "s3m", "it", "sid" };
    QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };

    if (txtExts.contains(ext)) {
        showTextPreview(filePath);
    } else if (imgExts.contains(ext)) {
        showImagePreview(filePath);
    } else if (audioExts.contains(ext)) {
        showMediaPreview(filePath, false, startPlaying);
    } else if (videoExts.contains(ext)) {
        showMediaPreview(filePath, true, startPlaying);
    } else if (isLikelyTextFile(filePath)) {
        showTextPreview(filePath);
    } else if (ext == "pdf") {
        if (m_pdfViewer) {
            m_pdfViewer->loadPdf(filePath);
            m_stack->setCurrentWidget(m_pdfViewer);
        }
        if (m_textTabs) {
            QString fileName = info.fileName() + " (Text)";
            addOrActivateTextTab(fileName, "Extracting text contents from PDF...");
            QProcess* extractProc = new QProcess(this);
            connect(extractProc, &QProcess::finished, this, [this, extractProc, fileName](int exitCode) {
                QString content;
                if (exitCode == 0) {
                    content = QString::fromLocal8Bit(extractProc->readAllStandardOutput());
                } else {
                    content = "Failed to extract text from PDF.";
                }
                addOrActivateTextTab(fileName, content);
                extractProc->deleteLater();
            });
            extractProc->start("pdftotext", {filePath, "-"});
        }
    } else if (ext == "docx" || ext == "odt" || ext == "rtf") {
        if (m_textTabs) {
            QString fileName = info.fileName() + " (Text)";
            addOrActivateTextTab(fileName, "Extracting text contents from document...");
            QProcess* extractProc = new QProcess(this);
            connect(extractProc, &QProcess::finished, this, [this, extractProc, fileName](int exitCode) {
                QString content;
                if (exitCode == 0) {
                    content = QString::fromLocal8Bit(extractProc->readAllStandardOutput());
                } else {
                    content = "Failed to extract text from document.";
                }
                addOrActivateTextTab(fileName, content);
                extractProc->deleteLater();
            });
            extractProc->start("python3", QStringList() << "scripts/extract_doc_text.py" << filePath);
        }
    } else if (ext == "doc") {
        if (m_textTabs) {
            QString fileName = info.fileName() + " (Text)";
            addOrActivateTextTab(fileName, "Extracting text contents using catdoc...");
            QProcess* extractProc = new QProcess(this);
            connect(extractProc, &QProcess::finished, this, [this, extractProc, fileName](int exitCode) {
                QString content;
                if (exitCode == 0) {
                    content = QString::fromLocal8Bit(extractProc->readAllStandardOutput());
                } else {
                    content = "Failed to extract text from Word document. Please convert it to .docx.";
                }
                addOrActivateTextTab(fileName, content);
                extractProc->deleteLater();
            });
            extractProc->start("catdoc", QStringList() << filePath);
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
        QString content;
        // Safeguard against loading massive files
        if (file.size() > 5 * 1024 * 1024) {
            content = "[File is too large to preview (>5MB)]";
            m_textEdit->setPlainText(content);
            m_textEdit->setReadOnly(true);
        } else {
            QTextStream in(&file);
            content = in.readAll();
            m_textEdit->setPlainText(content);
            m_textEdit->setReadOnly(false);
        }
        file.close();
        
        QString fileName = QFileInfo(filePath).fileName();
        addOrActivateTextTab(fileName, content);
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

void PreviewPanel::showMediaPreview(const QString& filePath, bool isVideo, bool startPlaying) {
    m_isVideo = isVideo;
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

    QString suffix = QFileInfo(filePath).suffix().toLower();
    bool isRetro = (suffix == "mod" || suffix == "xm" || suffix == "s3m" || suffix == "it" || suffix == "sid");
    QString activePlayPath = filePath;

    if (isRetro) {
        QString tempWav = QDir::temp().filePath(QString("amifiles_retro_%1.wav").arg(qApp->applicationPid()));
        QProcess proc;
        QString program;
        QStringList arguments;

        if (suffix == "sid") {
            QString sid2wavPath = QStandardPaths::findExecutable("sid2wav");
            if (sid2wavPath.isEmpty()) {
                sid2wavPath = QDir(qApp->applicationDirPath()).filePath("sid2wav");
            }
            if (sid2wavPath.isEmpty() || !QFileInfo::exists(sid2wavPath)) {
                if (QFileInfo::exists("/usr/bin/sid2wav")) {
                    sid2wavPath = "/usr/bin/sid2wav";
                } else if (QFileInfo::exists("/usr/local/bin/sid2wav")) {
                    sid2wavPath = "/usr/local/bin/sid2wav";
                } else {
                    sid2wavPath = "./sid2wav";
                }
            }
            program = sid2wavPath;
            arguments << filePath << tempWav << "60";
        } else {
            program = "openmpt123";
            arguments << "-q" << "--no-float" << "--force" << "-o" << tempWav << filePath;
        }

        proc.start(program, arguments);
        if (proc.waitForFinished(3000)) {
            if (QFileInfo::exists(tempWav) && QFileInfo(tempWav).size() > 1024) {
                activePlayPath = tempWav;
            } else {
                qDebug() << "Retro music render output file is invalid or empty! Exit code:" << proc.exitCode() << "Error:" << proc.errorString();
            }
        } else {
            qDebug() << "Retro music conversion timed out! Error:" << proc.errorString();
        }
    }

    if (!isVideo) {
        m_visualizer->setPlayer(m_player);
        m_visualizer->setAudioPath(activePlayPath);
    } else {
        m_visualizer->setPlayer(nullptr);
        m_visualizer->setAudioPath("");
    }

    m_player->setSource(QUrl::fromLocalFile(activePlayPath));
    
    QSettings settings("Amifiles", "Amifiles");
    bool muted = settings.value("preview/muted", false).toBool();
    setMuted(muted);

    if (startPlaying && (isVisible() || isFullscreen() || m_prePreviewPlaybackState == QMediaPlayer::PlayingState)) {
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

    if (isVideo && startPlaying) {
        m_lastProgressSaveTime = 0;
        QSettings settings("Amifiles", "Amifiles");
        if (settings.value("preview/resume_progress", false).toBool()) {
            QTimer::singleShot(250, this, [this, filePath]() {
                QSettings settings("Amifiles", "Amifiles");
                qint64 savedPos = settings.value(QString("watched_progress/%1").arg(filePath), 0).toLongLong();
                if (savedPos > 5000) {
                    m_player->pause();

                    QWidget* parentWidget = m_fullscreenWidget ? (QWidget*)m_fullscreenWidget : (QWidget*)this;
                    ResumeOverlay* overlay = new ResumeOverlay(parentWidget, savedPos, formatDuration(savedPos), [this, savedPos](bool resume) {
                        if (resume) {
                            m_player->setPosition(savedPos);
                        } else {
                            m_player->setPosition(0);
                        }
                        m_player->play();
                    });
                    overlay->show();
                }
            });
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

    if (event->mimeData()->urls().size() == 1) {
        QString path = event->mimeData()->urls().first().toLocalFile();
        if (!path.isEmpty()) {
            QFileInfo info(path);
            if (info.isFile()) {
                event->acceptProposedAction();
                previewFile(path, {}, true);
                return;
            }
        }
    }

    QStringList playableFiles;
    QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a", "mod", "xm", "s3m", "it", "sid" };
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
        emit fileSaved(m_previewedFilePath);
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

        if (m_isVideo && !m_previewedFilePath.isEmpty()) {
            QSettings settings("Amifiles", "Amifiles");
            if (settings.value("preview/resume_progress", false).toBool()) {
                qint64 pos = m_player->position();
                qint64 dur = m_player->duration();
                if (pos > 5000) {
                    if (dur > 0 && pos >= dur * 0.95) {
                        settings.remove(QString("watched_progress/%1").arg(m_previewedFilePath));
                        TagManager::instance().setFileOverlayIcon(m_previewedFilePath, "emblem-ok");
                        emit tagsChanged(m_previewedFilePath);
                    } else {
                        settings.setValue(QString("watched_progress/%1").arg(m_previewedFilePath), pos);
                    }
                }
            }
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
        updateLyricsPosition(position);
    }

    if (m_isVideo && !m_previewedFilePath.isEmpty() && m_player->playbackState() == QMediaPlayer::PlayingState) {
        QSettings settings("Amifiles", "Amifiles");
        if (settings.value("preview/resume_progress", false).toBool()) {
            qint64 curTime = QDateTime::currentMSecsSinceEpoch();
            if (curTime - m_lastProgressSaveTime >= 5000) {
                m_lastProgressSaveTime = curTime;
                qint64 dur = m_player->duration();
                if (dur > 0 && position >= dur * 0.95) {
                    settings.remove(QString("watched_progress/%1").arg(m_previewedFilePath));
                    TagManager::instance().setFileOverlayIcon(m_previewedFilePath, "emblem-ok");
                    emit tagsChanged(m_previewedFilePath);
                } else if (position > 5000) {
                    settings.setValue(QString("watched_progress/%1").arg(m_previewedFilePath), position);
                }
            }
        }
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
    m_activeMeta = meta;
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

    // Dynamic Tab Management for Music Tagging
    QString ext = QFileInfo(meta.path).suffix().toLower();
    QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a", "mod", "xm", "s3m", "it", "sid" };
    if (audioExts.contains(ext) && (ext == "mp3" || ext == "flac")) {
        m_musicEditTitle->setText(meta.title);
        m_musicEditArtist->setText(meta.artist);
        m_musicEditAlbum->setText(meta.album);
        m_musicEditGenre->setText(meta.genre);
        m_musicEditYear->setText(meta.year);
        m_musicEditTrack->setText(meta.track);
        m_musicEditLyrics->setPlainText(meta.lyrics);

        if (m_bottomTab->indexOf(m_musicTagsContainer) == -1) {
            m_bottomTab->insertTab(1, m_musicTagsContainer, "Music Tags");
            m_bottomTab->setTabToolTip(m_bottomTab->indexOf(m_musicTagsContainer), "Quick Audio Tag Editor");
        }
    } else {
        int idx = m_bottomTab->indexOf(m_musicTagsContainer);
        if (idx != -1) {
            m_bottomTab->removeTab(idx);
        }
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
    if (!filePaths.isEmpty()) {
        QFileInfo fi(filePaths.first());
        QString ext = fi.suffix().toLower();
        QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
        bool isVideo = videoExts.contains(ext);
        setPlaylistMode(!isVideo);
    }

    m_playlist = filePaths;
    m_playlistIndex = 0;
    
    refreshPlaylistUI();

    if (m_playlist.isEmpty()) {
        clearPreview();
        return;
    }

    m_forcePlayNext = true;
    previewFile(m_playlist[0], {}, true, true);
    m_playlistList->setCurrentRow(0);
    m_bottomTab->setCurrentIndex(1); // Switch to Playlist Queue tab

    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(1).arg(m_playlist.size())));
    emit playlistChanged();
}

void PreviewPanel::prepareForFullscreenPlayback(const QStringList& filePaths) {

    
    // 2. Clear preview to stop current playing media
    clearPreview();
    
    // 3. Determine if the first file is video
    bool isVideo = false;
    if (!filePaths.isEmpty()) {
        QFileInfo info(filePaths.first());
        QString ext = info.suffix().toLower();
        static const QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
        isVideo = videoExts.contains(ext);
    }
    m_isVideo = isVideo;
    
    // 4. Load the correct fullscreen playlist
    m_playlist = filePaths;
    m_playlistIndex = 0;
    
    if (m_playlistList) {
        refreshPlaylistUI();
    }
    
    // 5. Start playing the first track
    if (!m_playlist.isEmpty()) {
        m_forcePlayNext = true;
        previewFile(m_playlist[0], QStringList(), true, true);
    }
}

void PreviewPanel::addToPlaylist(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;

    QFileInfo fi(filePaths.first());
    QString ext = fi.suffix().toLower();
    QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
    bool isVideo = videoExts.contains(ext);
    setPlaylistMode(!isVideo);

    bool wasEmpty = m_playlist.isEmpty();

    for (const QString& path : filePaths) {
        m_playlist.append(path);
        QString filename = QFileInfo(path).fileName();
        QString folderName = QFileInfo(QFileInfo(path).absolutePath()).fileName();
        QString displayName = filename;
        if (!folderName.isEmpty() && folderName.toLower() != "music" && folderName.toLower() != "audio" && folderName.toLower() != "download" && folderName.toLower() != "downloads") {
            displayName = QString("%1 (%2)").arg(filename).arg(folderName);
        }
        QListWidgetItem* item = new QListWidgetItem(displayName, m_playlistList);
        item->setIcon(getTrackArtworkIcon(path));
    }

    if (wasEmpty) {
        m_playlistIndex = 0;
        previewFile(m_playlist[0], QStringList(), false, true);
        m_playlistList->setCurrentRow(0);
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
    emit playlistChanged();
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

    m_forcePlayNext = true;
    previewFile(m_playlist[m_playlistIndex], {}, true, true);
    m_playlistList->setCurrentRow(m_playlistIndex);

    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    emit playlistChanged();
}

void PreviewPanel::onNextTrack() {
    {
        QFile logFile("/home/dave/cpp_projects/Amifiles/menu_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << "onNextTrack called: size=" << m_playlist.size() << " index=" << m_playlistIndex << "\n";
        }
    }
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

    m_forcePlayNext = true;
    previewFile(m_playlist[m_playlistIndex], {}, true, true);
    m_playlistList->setCurrentRow(m_playlistIndex);

    int row = m_metadataTable->rowCount();
    m_metadataTable->insertRow(row);
    m_metadataTable->setItem(row, 0, new QTableWidgetItem("Playlist Status"));
    m_metadataTable->setItem(row, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    emit playlistChanged();
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
    emit shuffleStateChanged(m_shuffleEnabled);
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
    emit repeatStateChanged(m_repeatMode);
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
        m_forcePlayNext = true;
        previewFile(m_playlist[m_playlistIndex], {}, true, true);
        m_playlistList->setCurrentRow(m_playlistIndex);

        int r = m_metadataTable->rowCount();
        m_metadataTable->insertRow(r);
        m_metadataTable->setItem(r, 0, new QTableWidgetItem("Playlist Status"));
        m_metadataTable->setItem(r, 1, new QTableWidgetItem(QString("Playing track %1 of %2").arg(m_playlistIndex + 1).arg(m_playlist.size())));
    }
    emit playlistChanged();
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
                    previewFile(m_playlist[m_playlistIndex], {}, true, true);
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

    QString activePath = m_player ? m_player->source().toLocalFile() : "";
    if (activePath.isEmpty()) {
        activePath = !m_currentAudioPath.isEmpty() ? m_currentAudioPath : m_previewedFilePath;
    }
    if (activePath.isEmpty()) return;

    QFileInfo fileInfo(activePath);
    QString ext = fileInfo.suffix().toLower();
    static const QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
    bool isVideo = videoExts.contains(ext);
    m_isVideo = isVideo;



    if (m_playlistList) {
        refreshPlaylistUI();
    }

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
    connect(m_fullscreenWidget, &FullscreenWidget::playlistItemSelected, this, &PreviewPanel::playPlaylistIndex);
    connect(m_fullscreenWidget->hudLyricsButton(), &QPushButton::clicked, this, &PreviewPanel::onShowLyricsMenu);

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
    m_fullscreenWidget->setPlaylist(m_playlist, m_playlistIndex);

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
    layout->setSpacing(0);

    buildFullscreenContent(isVideo, activePath, layout);

    layout->addWidget(m_fullscreenWidget->hudWidget());

    m_fullscreenWidget->setMediaState(isVideo, m_player, m_audioOutput);
    m_fullscreenWidget->updateProgress(m_player->position(), m_player->duration());

    QString currentPath = activePath;
    QString nextPath;
    if (!m_playlist.isEmpty() && m_playlistIndex >= 0 && m_playlistIndex < m_playlist.size()) {
        if (m_playlistIndex < m_playlist.size() - 1) {
            nextPath = m_playlist[m_playlistIndex + 1];
        } else if (m_repeatMode == 2) {
            nextPath = m_playlist[0];
        }
    }
    m_fullscreenWidget->setTrackNames(currentPath, nextPath);

    m_fullscreenWidget->showFullScreen();
    m_fullscreenWidget->setFocus();
}

void PreviewPanel::exitFullscreen() {
    if (!m_fullscreenWidget) return;


    if (m_player) {
        bool previewDockVisible = false;
        QWidget* pTemp = parentWidget();
        while (pTemp && !pTemp->inherits("MainWindow")) {
            pTemp = pTemp->parentWidget();
        }
        if (pTemp) {
            QDockWidget* dock = pTemp->findChild<QDockWidget*>("previewDockWidget");
            if (dock && dock->isVisible()) {
                previewDockVisible = true;
            }
        }
        if (!previewDockVisible) {
            m_player->pause();
        }
    }

    m_player->setVideoOutput(m_videoWidget);

    m_fullscreenWidget->close();
    m_fullscreenWidget->deleteLater();
    m_fullscreenWidget = nullptr;
    m_fullscreenVideoWidget = nullptr;
    m_fullscreenAudioLabel = nullptr;
    m_fullscreenTextLabel = nullptr;
    m_fullscreenVisualizer = nullptr;
    m_fullscreenLyricsScroll = nullptr;
    m_fullscreenLyricsLabel = nullptr;
    m_fullscreenLyricsPanel = nullptr;
    m_btnToggleLyricMode = nullptr;
    m_lyricContainerWidget = nullptr;

    if (m_playlistList) {
        refreshPlaylistUI();
    }

    emit fullscreenExited();
}

static void clearLayoutOfFullscreen(QLayout* layout, QWidget* hudWidget) {
    if (!layout) return;
    QLayoutItem* item;
    QList<QLayoutItem*> itemsToKeep;
    while ((item = layout->takeAt(0))) {
        if (item->widget()) {
            if (item->widget() == hudWidget) {
                itemsToKeep.append(item);
                continue;
            }
            item->widget()->deleteLater();
        }
        delete item;
    }
    for (QLayoutItem* kept : itemsToKeep) {
        delete kept;
    }
}

void PreviewPanel::buildFullscreenContent(bool isVideo, const QString& activePath, QVBoxLayout* mainLayout) {
    m_fullscreenVideoWidget = nullptr;
    m_fullscreenAudioLabel = nullptr;
    m_fullscreenTextLabel = nullptr;
    m_fullscreenVisualizer = nullptr;
    m_fullscreenLyricsScroll = nullptr;
    m_fullscreenLyricsLabel = nullptr;

    if (isVideo) {
        m_fullscreenVideoWidget = new QVideoWidget(m_fullscreenWidget);
        m_fullscreenVideoWidget->setStyleSheet("background-color: #000000;");
        mainLayout->addWidget(m_fullscreenVideoWidget);

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

        m_fullscreenVisualizer = new SpectrumVisualizerWidget(m_fullscreenWidget);
        m_fullscreenVisualizer->setPlayer(m_player);
        m_fullscreenVisualizer->setAudioPath(activePath);
        if (m_visualizer) {
            m_fullscreenVisualizer->setVisualizerMode(m_visualizer->visualizerMode());
            m_fullscreenVisualizer->setBoost(m_sliderBass->value() / 50.0, m_sliderMid->value() / 50.0, m_sliderTreble->value() / 50.0);
        }
        m_fullscreenVisualizer->setVisible(m_spectrumVisualizerEnabled);

        // Build split content layout: Left (art, text, viz) and Right (lyrics)
        QHBoxLayout* contentHorizontal = new QHBoxLayout();
        contentHorizontal->setContentsMargins(40, 20, 40, 10);
        contentHorizontal->setSpacing(40);

        QWidget* leftPanel = new QWidget(m_fullscreenWidget);
        m_fullscreenLeftPanel = leftPanel;
        QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->addStretch();
        leftLayout->addWidget(m_fullscreenAudioLabel);
        leftLayout->addSpacing(10);
        leftLayout->addWidget(m_fullscreenTextLabel);
        leftLayout->addSpacing(20);

        m_fullscreenBottomLyricsWidget = new QWidget(leftPanel);
        QVBoxLayout* bottomLyricsLayout = new QVBoxLayout(m_fullscreenBottomLyricsWidget);
        bottomLyricsLayout->setContentsMargins(10, 10, 10, 10);
        bottomLyricsLayout->setSpacing(8);
        m_lblBottomLyricsCurrent = new QLabel(m_fullscreenBottomLyricsWidget);
        m_lblBottomLyricsCurrent->setWordWrap(true);
        m_lblBottomLyricsCurrent->setAlignment(Qt::AlignCenter);
        m_lblBottomLyricsCurrent->setStyleSheet("QLabel { color: #a6e3a1; font-size: 26px; font-family: 'Outfit'; font-weight: bold; background: transparent; }");
        m_lblBottomLyricsNext = new QLabel(m_fullscreenBottomLyricsWidget);
        m_lblBottomLyricsNext->setWordWrap(true);
        m_lblBottomLyricsNext->setAlignment(Qt::AlignCenter);
        m_lblBottomLyricsNext->setStyleSheet("QLabel { color: rgba(205, 214, 244, 0.4); font-size: 18px; font-family: 'Outfit'; font-weight: 500; background: transparent; }");
        bottomLyricsLayout->addWidget(m_lblBottomLyricsCurrent);
        bottomLyricsLayout->addWidget(m_lblBottomLyricsNext);
        leftLayout->addWidget(m_fullscreenBottomLyricsWidget);

        leftLayout->addWidget(m_fullscreenVisualizer);
        leftLayout->addStretch();
        contentHorizontal->addWidget(leftPanel, 1);

        // Set up scrolling lyrics structures
        m_rawLyricsText = meta.lyrics;
        m_syncedLyrics.clear();
        m_lyricLabels.clear();
        m_currentLyricLineIndex = -1;
        m_hasSyncData = false;

        QRegularExpression timeReg("\\[(\\d+):(\\d+(?:\\.\\d+)?)\\]");
        if (!m_rawLyricsText.isEmpty() && m_rawLyricsText.contains(timeReg)) {
            m_hasSyncData = true;
            QStringList rawLines = m_rawLyricsText.split('\n');
            for (const QString& line : rawLines) {
                QString text = line;
                text.remove(timeReg);
                text = text.trimmed();
                
                auto it = timeReg.globalMatch(line);
                while (it.hasNext()) {
                    auto match = it.next();
                    int min = match.captured(1).toInt();
                    double sec = match.captured(2).toDouble();
                    qint64 ms = (min * 60 + sec) * 1000;
                    m_syncedLyrics.append({ms, text});
                }
            }
            std::sort(m_syncedLyrics.begin(), m_syncedLyrics.end(), [](const SyncedLyricLine& a, const SyncedLyricLine& b) {
                return a.timestampMs < b.timestampMs;
            });
        }

        m_fullscreenLyricsPanel = new QWidget(m_fullscreenWidget);
        QVBoxLayout* rightLayout = new QVBoxLayout(m_fullscreenLyricsPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(10);

        QHBoxLayout* lyricHeader = new QHBoxLayout();
        lyricHeader->setContentsMargins(20, 0, 20, 0);
        
        QLabel* lblLyricsTitle = new QLabel("Lyrics", m_fullscreenLyricsPanel);
        lblLyricsTitle->setStyleSheet("color: #cdd6f4; font-size: 20px; font-weight: bold; font-family: 'Outfit';");
        lyricHeader->addWidget(lblLyricsTitle);
        lyricHeader->addStretch();

        m_btnToggleLyricMode = new QPushButton(m_fullscreenLyricsPanel);
        m_btnToggleLyricMode->setCursor(Qt::PointingHandCursor);
        m_btnToggleLyricMode->setStyleSheet(
            "QPushButton { background-color: rgba(49, 50, 68, 180); color: #cdd6f4; border: 1px solid #45475a; border-radius: 12px; padding: 4px 12px; font-family: 'Outfit'; font-size: 11px; font-weight: bold; } "
            "QPushButton:hover { background-color: rgba(69, 71, 90, 200); color: #ffffff; } "
            "QPushButton:disabled { color: rgba(205, 214, 244, 100); background-color: transparent; border: none; }"
        );
        connect(m_btnToggleLyricMode, &QPushButton::clicked, this, &PreviewPanel::onToggleLyricsMode);
        lyricHeader->addWidget(m_btnToggleLyricMode);
        rightLayout->addLayout(lyricHeader);

        m_fullscreenLyricsScroll = new QScrollArea(m_fullscreenLyricsPanel);
        m_fullscreenLyricsScroll->setWidgetResizable(true);
        m_fullscreenLyricsScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
        rightLayout->addWidget(m_fullscreenLyricsScroll);

        rebuildLyricsView();

        contentHorizontal->addWidget(m_fullscreenLyricsPanel, 1);

        // Update to initial layout from settings
        updateLyricsLayout();

        mainLayout->addLayout(contentHorizontal);

        m_fullscreenAudioLabel->setMouseTracking(true);
        m_fullscreenTextLabel->setMouseTracking(true);
        m_fullscreenVisualizer->setMouseTracking(true);
        m_fullscreenAudioLabel->installEventFilter(m_fullscreenWidget);
        m_fullscreenTextLabel->installEventFilter(m_fullscreenWidget);
        m_fullscreenVisualizer->installEventFilter(m_fullscreenWidget);
    }
}

void PreviewPanel::updateFullscreenTrack() {
    if (!m_fullscreenWidget) return;

    QString activePath = m_player ? m_player->source().toLocalFile() : "";
    if (activePath.isEmpty()) {
        activePath = !m_currentAudioPath.isEmpty() ? m_currentAudioPath : m_previewedFilePath;
    }
    if (activePath.isEmpty()) return;

    QFileInfo fileInfo(activePath);
    QString ext = fileInfo.suffix().toLower();
    static const QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
    bool isVideo = videoExts.contains(ext);

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_fullscreenWidget->layout());
    if (layout) {
        clearLayoutOfFullscreen(layout, m_fullscreenWidget->hudWidget());
        layout->setSpacing(0);
    } else {
        layout = new QVBoxLayout(m_fullscreenWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    buildFullscreenContent(isVideo, activePath, layout);

    layout->addWidget(m_fullscreenWidget->hudWidget());

    m_fullscreenWidget->setMediaState(isVideo, m_player, m_audioOutput);
    m_fullscreenWidget->updateProgress(m_player->position(), m_player->duration());

    QString currentPath = activePath;
    QString nextPath;
    if (!m_playlist.isEmpty() && m_playlistIndex >= 0 && m_playlistIndex < m_playlist.size()) {
        if (m_playlistIndex < m_playlist.size() - 1) {
            nextPath = m_playlist[m_playlistIndex + 1];
        } else if (m_repeatMode == 2) {
            nextPath = m_playlist[0];
        }
    }
    m_fullscreenWidget->setTrackNames(currentPath, nextPath);
    m_fullscreenWidget->setPlaylist(m_playlist, m_playlistIndex);
}

#include <QRandomGenerator>
#include <QTimer>

static void fft_iterative(std::vector<std::complex<double>>& a) {
    int n = a.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * 3.14159265358979323846 / len;
        std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (int j = 0; j < len / 2; j++) {
                std::complex<double> u = a[i + j];
                std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

SpectrumVisualizerWidget::SpectrumVisualizerWidget(QWidget* parent)
    : QWidget(parent), m_barHeights(15, 0.0), m_targetHeights(15, 0.0) {
    setMinimumHeight(80);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SpectrumVisualizerWidget::onAnimate);
    m_timer->start(30);
}

SpectrumVisualizerWidget::~SpectrumVisualizerWidget() {
    QString tempPath = QDir::temp().filePath(QString("amifiles_analysis_%1.wav").arg(qApp->applicationPid()));
    QFile::remove(tempPath);
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

void SpectrumVisualizerWidget::setPlayer(QMediaPlayer* player) {
    m_player = player;
}

void SpectrumVisualizerWidget::setAudioPath(const QString& path) {
    if (m_loadedAudioPath == path) return;
    m_loadedAudioPath = path;

    m_audioData.clear();
    m_samples = nullptr;
    m_numSamples = 0;

    if (path.isEmpty()) return;

    QFileInfo info(path);
    QString ext = info.suffix().toLower();

    if (ext == "wav") {
        loadWavData(path);
    } else {
        QString tempPath = QDir::temp().filePath(QString("amifiles_analysis_%1.wav").arg(qApp->applicationPid()));
        QProcess* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, tempPath, proc](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                loadWavData(tempPath);
            }
            proc->deleteLater();
        });
        proc->start("ffmpeg", QStringList() << "-y" << "-i" << path 
                                            << "-map_metadata" << "-1" 
                                            << "-ac" << "1" 
                                            << "-ar" << "22050" 
                                            << "-acodec" << "pcm_s16le" 
                                            << tempPath);
    }
}

void SpectrumVisualizerWidget::loadWavData(const QString& wavPath) {
    QFile file(wavPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    m_audioData = file.readAll();
    file.close();

    if (m_audioData.size() < 44) return;
    if (m_audioData.mid(0, 4) != "RIFF" || m_audioData.mid(8, 4) != "WAVE") return;

    int pos = 12;
    int dataOffset = -1;
    int dataSize = 0;
    int sampleRate = 22050;
    int numChannels = 1;

    while (pos + 8 <= m_audioData.size()) {
        QByteArray chunkId = m_audioData.mid(pos, 4);
        int chunkSize = *reinterpret_cast<const int*>(m_audioData.constData() + pos + 4);
        if (chunkId == "fmt ") {
            if (chunkSize >= 16) {
                numChannels = *reinterpret_cast<const int16_t*>(m_audioData.constData() + pos + 8 + 2);
                sampleRate = *reinterpret_cast<const int*>(m_audioData.constData() + pos + 8 + 4);
            }
        } else if (chunkId == "data") {
            dataOffset = pos + 8;
            dataSize = chunkSize;
            break;
        }
        pos += 8 + chunkSize;
    }

    if (dataOffset != -1 && dataOffset + dataSize <= m_audioData.size()) {
        m_samples = reinterpret_cast<const int16_t*>(m_audioData.constData() + dataOffset);
        m_numSamples = dataSize / 2;
        m_sampleRate = sampleRate;
        m_numChannels = numChannels;
    }
}

void SpectrumVisualizerWidget::onAnimate() {
    m_playing = m_player && (m_player->playbackState() == QMediaPlayer::PlayingState);

    qint64 positionMs = m_player ? m_player->position() : 0;
    int sampleIndex = static_cast<int>((positionMs / 1000.0) * m_sampleRate);

    if (m_mode == VisualizerBars || m_mode == VisualizerRadial) {
        int fftSize = 512;
        std::vector<std::complex<double>> fftInput(fftSize, 0.0);

        if (m_playing && m_samples && m_numSamples > 0) {
            int start = sampleIndex - fftSize / 2;
            for (int i = 0; i < fftSize; ++i) {
                int idx = (start + i) * m_numChannels;
                if (idx >= 0 && idx < m_numSamples) {
                    double window = 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * i / (fftSize - 1)));
                    double sampleVal = m_samples[idx] / 32768.0;
                    fftInput[i] = sampleVal * window;
                } else {
                    fftInput[i] = 0.0;
                }
            }
        }

        fft_iterative(fftInput);

        std::vector<double> magnitudes(fftSize / 2, 0.0);
        for (int i = 0; i < fftSize / 2; ++i) {
            magnitudes[i] = std::abs(fftInput[i]);
        }

        int numBars = 15;
        double startFreq = 20.0;
        double endFreq = 8000.0;

        for (int b = 0; b < numBars; ++b) {
            if (m_playing) {
                double fStart = startFreq * std::pow(endFreq / startFreq, static_cast<double>(b) / numBars);
                double fEnd = startFreq * std::pow(endFreq / startFreq, static_cast<double>(b + 1) / numBars);

                int binStart = qBound(0, static_cast<int>(fStart * fftSize / m_sampleRate), fftSize / 2 - 1);
                int binEnd = qBound(binStart + 1, static_cast<int>(fEnd * fftSize / m_sampleRate), fftSize / 2);

                double sum = 0.0;
                for (int i = binStart; i < binEnd; ++i) {
                    sum += magnitudes[i];
                }
                double avg = sum / (binEnd - binStart);

                double boost = 1.0;
                if (b < 5) boost = m_bassBoost;
                else if (b < 10) boost = m_midBoost;
                else boost = m_trebleBoost;

                double height = avg * 300.0 * boost;
                if (height > 100.0) height = 100.0;
                if (height < 2.0) height = 2.0;

                m_targetHeights[b] = height;
            } else {
                m_targetHeights[b] = 0.0;
            }

            m_barHeights[b] = m_barHeights[b] * 0.6 + m_targetHeights[b] * 0.4;
        }
    } else if (m_mode == VisualizerWaveform) {
        m_waveformHistory.clear();
        if (m_playing && m_samples && m_numSamples > 0) {
            int spacing = 5;
            for (int i = 0; i < 80; ++i) {
                int idx = (sampleIndex + i * spacing) * m_numChannels;
                if (idx >= 0 && idx < m_numSamples) {
                    double sampleVal = (m_samples[idx] / 32768.0) * 80.0;
                    m_waveformHistory.append(sampleVal);
                } else {
                    m_waveformHistory.append(0.0);
                }
            }
        } else {
            for (int i = 0; i < 80; ++i) {
                m_waveformHistory.append(0.0);
            }
        }
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
    if (m_fullscreenVisualizer) {
        m_fullscreenVisualizer->setBoost(bass, mid, treble);
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
    if (m_fullscreenVisualizer) {
        m_fullscreenVisualizer->setVisible(visible && !m_isVideo);
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

void PreviewPanel::playPlaylistIndex(int index) {
    if (index < 0 || index >= m_playlist.size()) return;
    m_playlistIndex = index;
    m_forcePlayNext = true;
    previewFile(m_playlist[m_playlistIndex], {}, true, true);
    if (m_playlistList) {
        m_playlistList->setCurrentRow(m_playlistIndex);
    }
    emit playlistChanged();
}

void PreviewPanel::removeFromPlaylist(int index) {
    if (index < 0 || index >= m_playlist.size()) return;
    m_playlist.removeAt(index);
    if (m_playlistList) {
        delete m_playlistList->takeItem(index);
    }
    if (m_playlist.isEmpty()) {
        clearPreview();
        emit playlistChanged();
    } else {
        if (m_playlistIndex >= m_playlist.size()) {
            m_playlistIndex = m_playlist.size() - 1;
        }
        if (index == m_playlistIndex) {
            playPlaylistIndex(m_playlistIndex);
        } else {
            emit playlistChanged();
        }
    }
}

void PreviewPanel::clearPlaylist() {
    m_playlist.clear();
    m_playlistIndex = -1;
    if (m_playlistList) {
        m_playlistList->clear();
    }
    clearPreview();
    emit playlistChanged();
}

void PreviewPanel::setVolume(int value) {
    onVolumeChanged(value);
}

void PreviewPanel::setPlaylistMode(bool audio) {
    if (m_isAudioMode == audio) return;

    // Save current playlist state
    if (m_isAudioMode) {
        m_audioPlaylist = m_playlist;
        m_audioPlaylistIndex = m_playlistIndex;
    } else {
        m_videoPlaylist = m_playlist;
        m_videoPlaylistIndex = m_playlistIndex;
    }

    m_isAudioMode = audio;

    // Restore new playlist state
    if (m_isAudioMode) {
        m_playlist = m_audioPlaylist;
        m_playlistIndex = m_audioPlaylistIndex;
    } else {
        m_playlist = m_videoPlaylist;
        m_playlistIndex = m_videoPlaylistIndex;
    }

    // Refresh m_playlistList widget
    if (m_playlistList) {
        refreshPlaylistUI();
    }

    emit playlistChanged();
}

bool PreviewPanel::isAutoPreviewEnabled() const {
    return m_btnAutoPreview && m_btnAutoPreview->isChecked();
}

void PreviewPanel::onAutoPreviewToggled() {
    if (!m_btnAutoPreview) return;
    bool active = m_btnAutoPreview->isChecked();
    m_btnAutoPreview->setToolTip(active ? "Auto-Preview: ON" : "Auto-Preview: OFF");
    // Save setting
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preview/auto_preview_enabled", active);
}

void PreviewPanel::addOrActivateTextTab(const QString& title, const QString& content) {
    if (!m_textTabs) return;
    
    // Check if a tab with this title is already open
    for (int i = 0; i < m_textTabs->count(); ++i) {
        if (m_textTabs->tabText(i) == title) {
            m_textTabs->setCurrentIndex(i);
            QTextEdit* textEdit = qobject_cast<QTextEdit*>(m_textTabs->widget(i));
            if (textEdit) {
                textEdit->setPlainText(content);
            }
            m_bottomTab->setCurrentWidget(m_textContainer);
            return;
        }
    }
    
    // Create new tab
    QTextEdit* textEdit = new QTextEdit(m_textTabs);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(content);
    textEdit->setStyleSheet("QTextEdit { background-color: transparent; border: none; color: #cdd6f4; font-family: monospace; font-size: 11px; }");
    
    int newIdx = m_textTabs->addTab(textEdit, title);
    m_textTabs->setCurrentIndex(newIdx);
    
    m_bottomTab->setCurrentWidget(m_textContainer);
}

void PreviewPanel::onTextTabCloseRequested(int index) {
    if (m_textTabs) {
        QWidget* w = m_textTabs->widget(index);
        m_textTabs->removeTab(index);
        if (w) w->deleteLater();
    }
}

void PreviewPanel::showTextTabsContextMenu(const QPoint& pos) {
    if (!m_textTabs) return;
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 6px; }"
        "QMenu::item { padding: 4px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #313244; color: #89b4fa; }"
    );
    QAction* actClear = menu.addAction("Clear All Tabs");
    QAction* selected = menu.exec(m_textTabs->mapToGlobal(pos));
    if (selected == actClear) {
        while (m_textTabs->count() > 0) {
            onTextTabCloseRequested(0);
        }
    }
}

void PreviewPanel::onTextSearchChanged(const QString& text) {
    if (!m_textTabs) return;
    QWidget* curr = m_textTabs->currentWidget();
    QTextEdit* textEdit = qobject_cast<QTextEdit*>(curr);
    if (!textEdit) {
        if (m_lblTextSearchMatches) m_lblTextSearchMatches->setText("");
        return;
    }

    QList<QTextEdit::ExtraSelection> extraSelections;

    if (text.isEmpty()) {
        textEdit->setExtraSelections(extraSelections);
        if (m_lblTextSearchMatches) m_lblTextSearchMatches->setText("");
        return;
    }

    QTextDocument* doc = textEdit->document();
    QTextCursor cursor(doc);
    int matchCount = 0;

    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(QColor("#f9e2af")); // Catppuccin yellow
    highlightFormat.setForeground(QColor("#11111b")); // Dark text

    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = doc->find(text, cursor);
        if (!cursor.isNull()) {
            matchCount++;
            QTextEdit::ExtraSelection selection;
            selection.format = highlightFormat;
            selection.cursor = cursor;
            extraSelections.append(selection);
        }
    }

    textEdit->setExtraSelections(extraSelections);

    if (matchCount > 0) {
        if (m_lblTextSearchMatches) {
            m_lblTextSearchMatches->setText(QString("%1 matches").arg(matchCount));
        }
        // Scroll to the first match
        QTextCursor firstCursor(doc);
        firstCursor = doc->find(text, firstCursor);
        if (!firstCursor.isNull()) {
            textEdit->setTextCursor(firstCursor);
        }
    } else {
        if (m_lblTextSearchMatches) {
            m_lblTextSearchMatches->setText("No matches");
        }
    }
}

QIcon PreviewPanel::getTrackArtworkIcon(const QString& trackPath) {
    static QHash<QString, QIcon> trackArtCache;
    if (trackArtCache.contains(trackPath)) {
        return trackArtCache[trackPath];
    }

    QString dirPath = QFileInfo(trackPath).absolutePath();
    static QHash<QString, QIcon> folderArtCache;
    if (folderArtCache.contains(dirPath)) {
        QIcon icon = folderArtCache[dirPath];
        trackArtCache[trackPath] = icon;
        return icon;
    }

    // Try finding local folder cover art
    QDir dir(dirPath);
    QStringList artNames = { "folder", "cover", "album", "poster", "front" };
    QStringList artExts = { "jpg", "jpeg", "png", "webp" };
    for (const QString& name : artNames) {
        for (const QString& ext : artExts) {
            QString path = dir.filePath(name + "." + ext);
            if (QFile::exists(path)) {
                QPixmap p(path);
                if (!p.isNull()) {
                    QIcon icon(p.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    folderArtCache[dirPath] = icon;
                    trackArtCache[trackPath] = icon;
                    return icon;
                }
            }
        }
    }

    // Check parent directory (in case of CD 1 / CD 2 folders)
    QDir parentDir = dir;
    if (parentDir.cdUp()) {
        for (const QString& name : artNames) {
            for (const QString& ext : artExts) {
                QString path = parentDir.filePath(name + "." + ext);
                if (QFile::exists(path)) {
                    QPixmap p(path);
                    if (!p.isNull()) {
                        QIcon icon(p.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        folderArtCache[dirPath] = icon;
                        trackArtCache[trackPath] = icon;
                        return icon;
                    }
                }
            }
        }
    }

    // Try embedded artwork via exiftool as fallback
    QProcess proc;
    proc.start("exiftool", {"-Picture", "-b", trackPath});
    if (proc.waitForFinished(800)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            QPixmap p;
            if (p.loadFromData(imgData)) {
                QIcon icon(p.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                trackArtCache[trackPath] = icon;
                return icon;
            }
        }
    }

    // Default fallback icon
    QIcon fallbackIcon = QIcon::fromTheme("audio-x-generic");
    trackArtCache[trackPath] = fallbackIcon;
    return fallbackIcon;
}

void PreviewPanel::refreshPlaylistUI() {
    if (!m_playlistList) return;
    m_playlistList->clear();
    for (int i = 0; i < m_playlist.size(); ++i) {
        QString path = m_playlist[i];
        QString filename = QFileInfo(path).fileName();
        QString folderName = QFileInfo(QFileInfo(path).absolutePath()).fileName();
        QString displayName = filename;
        if (!folderName.isEmpty() && folderName.toLower() != "music" && folderName.toLower() != "audio" && folderName.toLower() != "download" && folderName.toLower() != "downloads") {
            displayName = QString("%1 (%2)").arg(filename).arg(folderName);
        }
        
        QListWidgetItem* item = new QListWidgetItem(displayName, m_playlistList);
        item->setIcon(getTrackArtworkIcon(path));
    }
    if (m_playlistIndex >= 0 && m_playlistIndex < m_playlist.size()) {
        m_playlistList->setCurrentRow(m_playlistIndex);
    }
}

void PreviewPanel::loadPreferences() {
    QSettings settings("Amifiles", "Amifiles");
    if (m_chkAutoQueue) {
        m_chkAutoQueue->blockSignals(true);
        m_chkAutoQueue->setChecked(settings.value("preview/auto_queue_sibling_files", true).toBool());
        m_chkAutoQueue->blockSignals(false);
    }
}

void PreviewPanel::onSaveMusicTags() {
    if (m_previewedFilePath.isEmpty()) return;

    QString ext = QFileInfo(m_previewedFilePath).suffix().toLower();
    bool success = false;

    QString title = m_musicEditTitle->text();
    QString artist = m_musicEditArtist->text();
    QString album = m_musicEditAlbum->text();
    QString genre = m_musicEditGenre->text();
    QString year = m_musicEditYear->text();
    QString track = m_musicEditTrack->text();
    QString lyrics = m_musicEditLyrics->toPlainText();

    if (ext == "mp3") {
        success = TagEditorDialog::writeMp3Tags(
            m_previewedFilePath, title, artist, album, genre, year,
            "", "", false, false, QByteArray(), "image/jpeg", track,
            "", "", "", "", "", lyrics
        );
    } else if (ext == "flac") {
        success = TagEditorDialog::writeFlacTags(
            m_previewedFilePath, title, artist, album, genre, year,
            "", "", false, track, "", "", "", "", "", lyrics
        );
    }

    if (success) {
        QMessageBox::information(this, "Tags Saved", "Music tags successfully updated!");
        emit tagsChanged(m_previewedFilePath);
        
        // Reload metadata and update display
        FileMetadata updatedMeta = MetadataExtractor::extract(m_previewedFilePath);
        updateMetadataDisplay(updatedMeta);
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not save tags. Ensure that the file is not write-protected.");
    }
}

void PreviewPanel::rebuildLyricsView() {
    if (!m_fullscreenLyricsScroll) return;

    if (m_lyricContainerWidget) {
        m_lyricContainerWidget->deleteLater();
        m_lyricContainerWidget = nullptr;
    }
    m_lyricLabels.clear();
    m_currentLyricLineIndex = -1;

    m_lyricContainerWidget = new QWidget(m_fullscreenLyricsScroll);
    m_lyricContainerWidget->setObjectName("lyricContainerWidget");
    m_lyricContainerWidget->setStyleSheet("background: transparent;");

    QVBoxLayout* scrollLayout = new QVBoxLayout(m_lyricContainerWidget);

    scrollLayout->addStretch(1);

    if (m_rawLyricsText.trimmed().isEmpty()) {
        if (m_btnToggleLyricMode) m_btnToggleLyricMode->setVisible(false);
        m_fullscreenLyricsLabel = new QLabel("No lyrics available.", m_lyricContainerWidget);
        m_fullscreenLyricsLabel->setWordWrap(true);
        m_fullscreenLyricsLabel->setAlignment(Qt::AlignCenter);
        m_fullscreenLyricsLabel->setStyleSheet("QLabel { color: #6c7086; font-size: 24px; font-family: 'Outfit'; font-weight: 500; }");
        scrollLayout->addWidget(m_fullscreenLyricsLabel);
    }
    else if (m_hasSyncData && m_useScrollingLyrics) {
        if (m_btnToggleLyricMode) {
            m_btnToggleLyricMode->setVisible(true);
            m_btnToggleLyricMode->setText("🎤 Synced");
        }
        
        scrollLayout->setSpacing(24);
        scrollLayout->setContentsMargins(20, 20, 20, 20);

        for (int i = 0; i < m_syncedLyrics.size(); ++i) {
            QLabel* lineLabel = new QLabel(m_syncedLyrics[i].text, m_lyricContainerWidget);
            lineLabel->setWordWrap(true);
            lineLabel->setAlignment(Qt::AlignCenter);
            lineLabel->setStyleSheet("QLabel { color: rgba(205, 214, 244, 0.35); font-size: 22px; font-family: 'Outfit'; font-weight: 500; background: transparent; padding: 4px; }");
            scrollLayout->addWidget(lineLabel);
            m_lyricLabels.append(lineLabel);
        }
    }
    else {
        if (m_btnToggleLyricMode) {
            m_btnToggleLyricMode->setVisible(true);
            if (m_hasSyncData) {
                m_btnToggleLyricMode->setText("📝 Plain");
            } else {
                m_btnToggleLyricMode->setText("Static");
                m_btnToggleLyricMode->setEnabled(false);
            }
        }
        
        scrollLayout->setContentsMargins(30, 20, 30, 20);
        m_fullscreenLyricsLabel = new QLabel(m_lyricContainerWidget);
        m_fullscreenLyricsLabel->setWordWrap(true);
        m_fullscreenLyricsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_fullscreenLyricsLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 24px; font-family: 'Outfit'; font-weight: 500; line-height: 1.6; background: transparent; }");
        
        QRegularExpression timeReg("\\[(\\d+):(\\d+(?:\\.\\d+)?)\\]");
        QRegularExpression headerReg("\\[[a-zA-Z]+:[^\\]]*\\]");
        QString cleanText = m_rawLyricsText;
        cleanText.remove(timeReg);
        cleanText.remove(headerReg);
        cleanText = cleanText.trimmed();
        
        m_fullscreenLyricsLabel->setText(cleanText);
        scrollLayout->addWidget(m_fullscreenLyricsLabel);
    }

    scrollLayout->addStretch(1);
    m_fullscreenLyricsScroll->setWidget(m_lyricContainerWidget);
}

void PreviewPanel::onToggleLyricsMode() {
    m_useScrollingLyrics = !m_useScrollingLyrics;
    rebuildLyricsView();
    if (m_player) {
        updateLyricsPosition(m_player->position());
    }
}

void PreviewPanel::updateLyricsPosition(qint64 positionMs) {
    if (!m_fullscreenWidget || m_syncedLyrics.isEmpty() || !m_hasSyncData) {
        return;
    }

    int activeIdx = -1;
    for (int i = 0; i < m_syncedLyrics.size(); ++i) {
        if (positionMs >= m_syncedLyrics[i].timestampMs) {
            activeIdx = i;
        } else {
            break;
        }
    }

    if (activeIdx != m_currentLyricLineIndex) {
        m_currentLyricLineIndex = activeIdx;

        // 1. Update side panel scrolling labels (if visible/used)
        if (m_useScrollingLyrics && !m_lyricLabels.isEmpty()) {
            for (int i = 0; i < m_lyricLabels.size(); ++i) {
                QLabel* lbl = m_lyricLabels[i];
                if (i == activeIdx) {
                    lbl->setStyleSheet("QLabel { color: #a6e3a1; font-size: 30px; font-family: 'Outfit'; font-weight: bold; background: transparent; padding: 6px; }");
                } else {
                    lbl->setStyleSheet("QLabel { color: rgba(205, 214, 244, 0.35); font-size: 22px; font-family: 'Outfit'; font-weight: 500; background: transparent; padding: 4px; }");
                }
            }

            if (activeIdx >= 0 && activeIdx < m_lyricLabels.size()) {
                QLabel* activeLabel = m_lyricLabels[activeIdx];
                int labelCenterY = activeLabel->geometry().center().y();
                int viewportHeight = m_fullscreenLyricsScroll->viewport()->height();
                int targetY = labelCenterY - (viewportHeight / 2);
                
                QScrollBar* scrollBar = m_fullscreenLyricsScroll->verticalScrollBar();
                if (scrollBar) {
                    targetY = qMax(0, qMin(targetY, scrollBar->maximum()));
                    
                    QVariantAnimation* anim = new QVariantAnimation(this);
                    anim->setDuration(350);
                    anim->setStartValue(scrollBar->value());
                    anim->setEndValue(targetY);
                    connect(anim, &QVariantAnimation::valueChanged, this, [scrollBar](const QVariant& val) {
                        scrollBar->setValue(val.toInt());
                    });
                    anim->start(QAbstractAnimation::DeleteWhenStopped);
                }
            }
        }

        // 2. Update bottom panel labels (if visible/used)
        if (m_lblBottomLyricsCurrent && m_lblBottomLyricsNext) {
            QString curText = (activeIdx >= 0 && activeIdx < m_syncedLyrics.size()) ? m_syncedLyrics[activeIdx].text : "";
            QString nextText = (activeIdx + 1 >= 0 && activeIdx + 1 < m_syncedLyrics.size()) ? m_syncedLyrics[activeIdx + 1].text : "";
            
            m_lblBottomLyricsCurrent->setText(curText);
            m_lblBottomLyricsNext->setText(nextText);
        }
    }
}

void PreviewPanel::updateLyricsLayout() {
    if (!m_fullscreenWidget) return;

    QSettings settings("Amifiles", "Amifiles");
    bool showLyrics = settings.value("preview/show_lyrics", true).toBool();
    QString layoutType = settings.value("preview/lyrics_layout", "side").toString();

    // 1. Update HUD button stylesheet to reflect active status
    if (m_fullscreenWidget->hudLyricsButton()) {
        if (showLyrics) {
            m_fullscreenWidget->hudLyricsButton()->setStyleSheet("QPushButton { font-weight: bold; color: #a6e3a1; font-family: 'Outfit'; font-size: 11px; background-color: transparent; border: none; } QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }");
        } else {
            m_fullscreenWidget->hudLyricsButton()->setStyleSheet("QPushButton { font-weight: bold; color: #cdd6f4; font-family: 'Outfit'; font-size: 11px; background-color: transparent; border: none; } QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }");
        }
    }

    // 2. Apply show/hide to side panel, bottom widget, and left panel
    if (!showLyrics) {
        if (m_fullscreenLeftPanel) m_fullscreenLeftPanel->setVisible(true);
        if (m_fullscreenLyricsPanel) m_fullscreenLyricsPanel->setVisible(false);
        if (m_fullscreenBottomLyricsWidget) m_fullscreenBottomLyricsWidget->setVisible(false);
    } else {
        if (layoutType == "bottom") {
            if (m_fullscreenLeftPanel) m_fullscreenLeftPanel->setVisible(true);
            if (m_fullscreenLyricsPanel) m_fullscreenLyricsPanel->setVisible(false);
            if (m_fullscreenBottomLyricsWidget) {
                m_fullscreenBottomLyricsWidget->setVisible(true);
                if (m_rawLyricsText.trimmed().isEmpty()) {
                    m_lblBottomLyricsCurrent->setText("No lyrics available.");
                    m_lblBottomLyricsNext->setText("");
                } else if (!m_hasSyncData) {
                    m_lblBottomLyricsCurrent->setText("No timing sync data available.");
                    m_lblBottomLyricsNext->setText("Switch to 'Side' layout to read plain text lyrics.");
                } else if (m_player) {
                    m_currentLyricLineIndex = -2; // Force refresh
                    updateLyricsPosition(m_player->position());
                }
            }
        } else if (layoutType == "entire") {
            if (m_fullscreenLeftPanel) m_fullscreenLeftPanel->setVisible(false);
            if (m_fullscreenLyricsPanel) m_fullscreenLyricsPanel->setVisible(true);
            if (m_fullscreenBottomLyricsWidget) m_fullscreenBottomLyricsWidget->setVisible(false);
        } else { // "side"
            if (m_fullscreenLeftPanel) m_fullscreenLeftPanel->setVisible(true);
            if (m_fullscreenLyricsPanel) m_fullscreenLyricsPanel->setVisible(true);
            if (m_fullscreenBottomLyricsWidget) m_fullscreenBottomLyricsWidget->setVisible(false);
        }
    }
}

void PreviewPanel::onShowLyricsMenu() {
    if (!m_fullscreenWidget || !m_fullscreenWidget->hudLyricsButton()) return;

    QMenu menu(m_fullscreenWidget);
    menu.setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
        "QMenu::item:checked { color: #a6e3a1; }"
    );

    QSettings settings("Amifiles", "Amifiles");
    bool showLyrics = settings.value("preview/show_lyrics", true).toBool();
    QString layoutType = settings.value("preview/lyrics_layout", "side").toString();

    QAction* lblLayoutHeader = menu.addAction("Style Layout:");
    lblLayoutHeader->setEnabled(false);
    QFont f = lblLayoutHeader->font();
    f.setBold(true);
    lblLayoutHeader->setFont(f);

    QActionGroup* layoutGroup = new QActionGroup(&menu);
    
    QAction* actSide = menu.addAction("📋 To the Side");
    actSide->setCheckable(true);
    actSide->setActionGroup(layoutGroup);
    
    QAction* actBottom = menu.addAction("👇 To the Bottom");
    actBottom->setCheckable(true);
    actBottom->setActionGroup(layoutGroup);
    
    QAction* actEntire = menu.addAction("📖 Entire Lyrics");
    actEntire->setCheckable(true);
    actEntire->setActionGroup(layoutGroup);

    QAction* actHide = menu.addAction("❌ Hide Lyrics");
    actHide->setCheckable(true);
    actHide->setActionGroup(layoutGroup);

    if (!showLyrics) {
        actHide->setChecked(true);
    } else if (layoutType == "bottom") {
        actBottom->setChecked(true);
    } else if (layoutType == "entire") {
        actEntire->setChecked(true);
    } else {
        actSide->setChecked(true);
    }

    menu.addSeparator();

    QAction* lblSyncHeader = menu.addAction("Sync Mode:");
    lblSyncHeader->setEnabled(false);
    lblSyncHeader->setFont(f);

    QActionGroup* syncGroup = new QActionGroup(&menu);
    
    QAction* actSync = menu.addAction("🎤 Synced (Scrolling)");
    actSync->setCheckable(true);
    actSync->setActionGroup(syncGroup);
    actSync->setEnabled(m_hasSyncData);

    QAction* actPlain = menu.addAction("📝 Plain Text");
    actPlain->setCheckable(true);
    actPlain->setActionGroup(syncGroup);

    if (m_useScrollingLyrics && m_hasSyncData) {
        actSync->setChecked(true);
    } else {
        actPlain->setChecked(true);
    }

    QPoint pos = m_fullscreenWidget->hudLyricsButton()->mapToGlobal(QPoint(0, 0));
    pos.setY(pos.y() - 210);
    QAction* selected = menu.exec(pos);
    if (!selected) return;

    if (selected == actHide) {
        settings.setValue("preview/show_lyrics", false);
    } else if (selected == actSide) {
        settings.setValue("preview/show_lyrics", true);
        settings.setValue("preview/lyrics_layout", "side");
    } else if (selected == actBottom) {
        settings.setValue("preview/show_lyrics", true);
        settings.setValue("preview/lyrics_layout", "bottom");
    } else if (selected == actEntire) {
        settings.setValue("preview/show_lyrics", true);
        settings.setValue("preview/lyrics_layout", "entire");
    } else if (selected == actSync) {
        m_useScrollingLyrics = true;
        rebuildLyricsView();
    } else if (selected == actPlain) {
        m_useScrollingLyrics = false;
        rebuildLyricsView();
    }

    updateLyricsLayout();
}
