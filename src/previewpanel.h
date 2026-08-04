#ifndef PREVIEWPANEL_H
#define PREVIEWPANEL_H

#include <QWidget>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QTableWidget>
#include <QPixmap>
#include <QPainter>
#include <QToolTip>
#include <QMouseEvent>
#include <QPaintEvent>
#include "metadataextractor.h"

class ScrubSlider : public QSlider {
    Q_OBJECT
public:
    struct Chapter {
        qint64 startMs;
        QString title;
    };

    explicit ScrubSlider(Qt::Orientation orientation, QWidget* parent = nullptr)
        : QSlider(orientation, parent) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    void setChapters(const QList<Chapter>& chapters) {
        m_chapters = chapters;
        update();
    }

    QList<Chapter> chapters() const { return m_chapters; }

protected:
    QList<Chapter> m_chapters;
protected:
    void mousePressEvent(class QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            double fraction = double(event->pos().x()) / double(width());
            int val = minimum() + fraction * (maximum() - minimum());
            setValue(val);
            emit sliderMoved(val);
            event->accept();
        }
        QSlider::mousePressEvent(event);
    }
    void keyPressEvent(class QKeyEvent* event) override {
        int step = 5000;
        if (event->modifiers() & Qt::ShiftModifier) {
            step = 1000;
        }
        if (event->key() == Qt::Key_Left) {
            int val = qMax(minimum(), value() - step);
            setValue(val);
            emit sliderMoved(val);
            event->accept();
        } else if (event->key() == Qt::Key_Right) {
            int val = qMin(maximum(), value() + step);
            setValue(val);
            emit sliderMoved(val);
            event->accept();
        } else {
            QSlider::keyPressEvent(event);
        }
    }

    void paintEvent(QPaintEvent* event) override {
        QSlider::paintEvent(event);
        if (m_chapters.isEmpty() || maximum() <= minimum()) return;

        QPainter painter(this);
        painter.setPen(QPen(QColor("#f38ba8"), 2));

        qint64 range = maximum() - minimum();
        for (const auto& ch : m_chapters) {
            double fraction = double(ch.startMs - minimum()) / double(range);
            if (fraction > 0.0 && fraction < 1.0) {
                int x = fraction * width();
                painter.drawLine(x, 0, x, height());
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        QSlider::mouseMoveEvent(event);
        if (m_chapters.isEmpty() || maximum() <= minimum()) return;

        double fraction = double(event->pos().x()) / double(width());
        qint64 hoverTime = minimum() + fraction * (maximum() - minimum());

        QString chTitle = "Seeking";
        for (int i = 0; i < m_chapters.size(); ++i) {
            if (hoverTime >= m_chapters[i].startMs) {
                if (i == m_chapters.size() - 1 || hoverTime < m_chapters[i+1].startMs) {
                    chTitle = m_chapters[i].title;
                    break;
                }
            }
        }

        qint64 secs = hoverTime / 1000;
        qint64 mins = secs / 60;
        secs = secs % 60;
        QString timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));

        QToolTip::showText(event->globalPosition().toPoint(), QString("%1 (%2)").arg(chTitle).arg(timeStr), this);
    }
};

class AudioPlaceholderWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioPlaceholderWidget(QWidget* parent = nullptr);
    void setFilePath(const QString& filePath);
    void setCoverArtVisible(bool visible);
    QString filePath() const { return m_filePath; }
    bool isCoverArtVisible() const { return m_coverArtVisible; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_filePath;
    FileMetadata m_metadata;
    bool m_coverArtVisible = true;
    QPixmap m_embeddedCover;
};

class FullscreenWidget : public QWidget {
    Q_OBJECT
public:
    explicit FullscreenWidget(QWidget* parent = nullptr);
    ~FullscreenWidget() override;

    void setMediaState(bool isVideo, class QMediaPlayer* player, class QAudioOutput* audioOutput);
    void updateProgress(qint64 position, qint64 duration);
    void setTrackNames(const QString& currentPath, const QString& nextPath);
    class QWidget* hudWidget() const { return (class QWidget*)m_hudWidget; }

    void setPlaylist(const QStringList& playlist, int currentIndex);
    void togglePlaylistDrawer();

signals:
    void exitRequested();
    void prevRequested();
    void playPauseRequested();
    void stopRequested();
    void nextRequested();
    void shuffleToggled();
    void repeatRequested();
    void builtinPlayerDoubleclickToggled(bool active);
    void playlistItemSelected(int index);
    void lyricsToggled(bool visible);

public slots:
    void setBuiltinPlayerDoubleclickActive(bool active);
    void onHudPlaylist();

protected:
    void keyPressEvent(class QKeyEvent* event) override;
    void mouseDoubleClickEvent(class QMouseEvent* event) override;
    void contextMenuEvent(class QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, class QEvent* event) override;
    void resizeEvent(class QResizeEvent* event) override;

private slots:
    void onHideHud();
    void onHudPlayPause();
    void onHudSliderMoved(int val);
    void onHudVolumeChanged(int val);
    void onPollMouse();
    void onHudSubtitles();
    void onHudShuffle();
    void onHudRepeat();
    void onHudChapters();

private:
    void showHud();
    void updateHudGeometry();

    class QFrame* m_hudWidget = nullptr;
    class QPushButton* m_btnPlayPause = nullptr;
    class QPushButton* m_btnSubtitles = nullptr;
    class QPushButton* m_btnShuffle = nullptr;
    class QPushButton* m_btnRepeat = nullptr;
    class QPushButton* m_btnChapters = nullptr;
    class ScrubSlider* m_sliderProgress = nullptr;
    class QLabel* m_lblTime = nullptr;
    class QSlider* m_sliderVolume = nullptr;
    class QPushButton* m_btnToggleAutoFS = nullptr;
    class QTimer* m_hideTimer = nullptr;
    class QTimer* m_mousePollTimer = nullptr;
    QPoint m_lastMousePos;
    class QMediaPlayer* m_player = nullptr;
    class QAudioOutput* m_audioOutput = nullptr;
    QString m_currentTrackPath;
    class QMenu* m_activeMenu = nullptr;
    bool m_menuCanceled = false;
    class QLabel* m_lblCurrentPlaying = nullptr;
    class QLabel* m_lblNextPlaying = nullptr;
    class QLabel* m_lblCurrentArtwork = nullptr;
    class QLabel* m_lblNextArtwork = nullptr;

    class QPushButton* m_btnTogglePlaylist = nullptr;
    class QPushButton* m_btnToggleLyrics = nullptr;
    QStringList m_playlistItems;
    int m_playlistCurrentIndex = -1;
public:
    QPushButton* hudShuffleButton() const { return m_btnShuffle; }
    QPushButton* hudRepeatButton() const { return m_btnRepeat; }
    QPushButton* hudLyricsButton() const { return m_btnToggleLyrics; }
};

class SpectrumVisualizerWidget : public QWidget {
    Q_OBJECT
public:
    enum VisualizerMode {
        VisualizerBars = 0,
        VisualizerRadial = 1,
        VisualizerWaveform = 2
    };

    explicit SpectrumVisualizerWidget(QWidget* parent = nullptr);
    ~SpectrumVisualizerWidget() override;
    void setPlaying(bool playing);
    void setBoost(double bass, double mid, double treble);
    void setVisualizerMode(VisualizerMode mode);
    VisualizerMode visualizerMode() const { return m_mode; }
    void setPlayer(class QMediaPlayer* player);
    void setAudioPath(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimate();

private:
    void loadWavData(const QString& wavPath);

    QTimer* m_timer = nullptr;
    bool m_playing = false;
    double m_bassBoost = 1.0;
    double m_midBoost = 1.0;
    double m_trebleBoost = 1.0;
    VisualizerMode m_mode = VisualizerBars;
    QVector<double> m_barHeights;
    QVector<double> m_targetHeights;
    QList<double> m_waveformHistory;

    class QMediaPlayer* m_player = nullptr;
    QString m_loadedAudioPath;
    QByteArray m_audioData;
    const int16_t* m_samples = nullptr;
    int m_numSamples = 0;
    int m_sampleRate = 22050;
    int m_numChannels = 1;
};

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget* parent = nullptr);
    ~PreviewPanel() override;

    void previewFile(const QString& filePath, const QStringList& siblingSelections = QStringList(), bool startPlaying = true, bool keepCurrentPlaylist = false);
    void previewFolderArt(const QString& artPath, const QString& folderPath);
    void clearPreview();
    void playPlaylist(const QStringList& filePaths);
    void prepareForFullscreenPlayback(const QStringList& filePaths);
    void addToPlaylist(const QStringList& filePaths);
    void setPlaylistMode(bool audio);
    QMediaPlayer* player() const { return m_player; }
    void setVolume(int value);
    bool isFullscreen() const { return m_fullscreenWidget != nullptr; }
    void setMuted(bool muted);
    bool isMuted() const;
    void setAudioCoverArtVisible(bool visible);
    void setSpectrumVisualizerVisible(bool visible);
    bool isSpectrumVisualizerEnabled() const { return m_spectrumVisualizerEnabled; }
    void setZenMode(bool enabled);
    bool isAutoPreviewEnabled() const;
    void loadPreferences();
    
    QStringList playlist() const { return m_playlist; }
    int playlistIndex() const { return m_playlistIndex; }
    void playPlaylistIndex(int index);
    void removeFromPlaylist(int index);
    void clearPlaylist();

public slots:
    void toggleFullscreen();
    void exitFullscreen();
    void updateFullscreenTrack();
    void setBuiltinPlayerDoubleclickActive(bool active);
    void onPrevTrack();
    void onNextTrack();
    void onPlayPause();
    void onStop();
    void onShuffleToggled();
    void onRepeatClicked();
    
    bool isShuffleEnabled() const { return m_shuffleEnabled; }
    int repeatMode() const { return m_repeatMode; }

signals:
    void fileSaved(const QString& filePath);
    void tagsChanged(const QString& filePath);
    void spectrumVisualizerToggled(bool checked);
    void builtinPlayerDoubleclickToggled(bool active);
    void fullscreenExited();
    void playlistChanged();
    void shuffleStateChanged(bool enabled);
    void repeatStateChanged(int mode);

public:
    QSize minimumSizeHint() const override { return QSize(220, 200); }
    QSize sizeHint() const override { return QSize(280, 500); }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(class QKeyEvent* event) override;
    bool eventFilter(QObject* watched, class QEvent* event) override;
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dragMoveEvent(class QDragMoveEvent* event) override;
    void dropEvent(class QDropEvent* event) override;

private slots:
    void onSaveText();
    void onTextChanged();
    
    // Media Player Slots
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onVolumeChanged(int value);
    void onSliderMoved(int value);
    void onMediaMetadataChanged();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onSubtitleMenuRequested();
    void onEqPresetChanged(int index);
    void onEqSlidersChanged();
    void showPlaylistContextMenu(const QPoint& pos);
    void onTextTabCloseRequested(int index);
    void showTextTabsContextMenu(const QPoint& pos);
    void onTextSearchChanged(const QString& text);

private:
    void setupUI();
    void showTextPreview(const QString& filePath);
    void addOrActivateTextTab(const QString& title, const QString& content);
    void showImagePreview(const QString& filePath);
    void showMediaPreview(const QString& filePath, bool isVideo, bool startPlaying = true);
    void updateMetadataDisplay(const FileMetadata& meta);
    void scaleImage();
    void updateAudioPlaceholder(const QString& filePath);
    
    QString formatDuration(qint64 ms);
    void openFullscreenImage();

    QString m_previewedFilePath;
    QString m_currentAudioPath;
    QStringList m_previewedFilePaths;
    QString m_selectedOverlayIconName;
    bool m_textChanged = false;
    QStringList m_playlist;
    int m_playlistIndex = -1;
    bool m_isAudioMode = true;
    QStringList m_audioPlaylist;
    int m_audioPlaylistIndex = -1;
    QStringList m_videoPlaylist;
    int m_videoPlaylistIndex = -1;


    bool m_shuffleEnabled = false;
    int m_repeatMode = 0; // 0 = Off, 1 = Repeat One, 2 = Repeat All
    qint64 m_lastProgressSaveTime = 0;
    bool m_forcePlayNext = false;
    bool m_isVideo = false;
    QMediaPlayer::PlaybackState m_prePreviewPlaybackState = QMediaPlayer::StoppedState;
    QPixmap m_originalPixmap;

    // Media Player Backend (Qt6)
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;

    // UI Layout Components
    QStackedWidget* m_stack = nullptr;

    // Empty View
    QWidget* m_emptyView = nullptr;

    // Text View
    QWidget* m_textView = nullptr;
    QPlainTextEdit* m_textEdit = nullptr;
    QWidget* m_textControls = nullptr;
    QPushButton* m_btnSaveText = nullptr;

    // Image View
    QWidget* m_imageView = nullptr;
    QLabel* m_imageLabel = nullptr;
    QScrollArea* m_imageScrollArea = nullptr;
    class PdfViewerWidget* m_pdfViewer = nullptr;
    class QTabWidget* m_textTabs = nullptr;

    // Media View
    QWidget* m_mediaView = nullptr;
    QVideoWidget* m_videoWidget = nullptr;
    AudioPlaceholderWidget* m_audioPlaceholder = nullptr;
    QPushButton* m_btnPlayPause = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnPrevTrack = nullptr;
    QPushButton* m_btnNextTrack = nullptr;
    ScrubSlider* m_sliderProgress = nullptr;
    QLabel* m_lblProgressTime = nullptr;
    QSlider* m_sliderVolume = nullptr;
    QPushButton* m_btnFullscreen = nullptr;
    QPushButton* m_btnSubtitles = nullptr;
    QPushButton* m_btnShuffle = nullptr;
    QPushButton* m_btnRepeat = nullptr;
    QPushButton* m_btnToggleVisualizer = nullptr;
    QPushButton* m_btnAutoFS20s = nullptr;
    QPushButton* m_btnAutoPreview = nullptr;
    QTimer* m_autoFsTimer = nullptr;

    // EQ and Visualizer Elements
    SpectrumVisualizerWidget* m_visualizer = nullptr;
    class QComboBox* m_comboEqPreset = nullptr;
    QSlider* m_sliderBass = nullptr;
    QSlider* m_sliderMid = nullptr;
    QSlider* m_sliderTreble = nullptr;
    bool m_spectrumVisualizerEnabled = true;
    class QCheckBox* m_chkShowVisualizer = nullptr;

    // Bottom half: Tabbed view for Metadata and Playlist Queue
    class QTabWidget* m_bottomTab = nullptr;
    QWidget* m_metadataContainer = nullptr;
    QTableWidget* m_metadataTable = nullptr;
    class QLineEdit* m_tagEditorEdit = nullptr;
    class QComboBox* m_tagColorCombo = nullptr;
    class QPushButton* m_btnChooseOverlayIcon = nullptr;
    class QPushButton* m_btnClearOverlayIcon = nullptr;
    class QPushButton* m_btnApplyTagsColors = nullptr;
    class QCompleter* m_tagCompleter = nullptr;
    class QListWidget* m_playlistList = nullptr;
    class QCheckBox* m_chkAutoQueue = nullptr;
    class HexEditorWidget* m_hexViewer = nullptr;
    class QWidget* m_textContainer = nullptr;
    class QLineEdit* m_textSearchEdit = nullptr;
    class QLabel* m_lblTextSearchMatches = nullptr;
 
    // Quick Music Tag Editor elements
    QWidget* m_musicTagsContainer = nullptr;
    class QLineEdit* m_musicEditTitle = nullptr;
    class QLineEdit* m_musicEditArtist = nullptr;
    class QLineEdit* m_musicEditAlbum = nullptr;
    class QLineEdit* m_musicEditGenre = nullptr;
    class QLineEdit* m_musicEditYear = nullptr;
    class QLineEdit* m_musicEditTrack = nullptr;
    class QPlainTextEdit* m_musicEditLyrics = nullptr;
    class QPushButton* m_btnSaveMusicTags = nullptr;
    FileMetadata m_activeMeta;

    // Fullscreen support
    FullscreenWidget* m_fullscreenWidget = nullptr;
    QVideoWidget* m_fullscreenVideoWidget = nullptr;
    QLabel* m_fullscreenAudioLabel = nullptr;
    QLabel* m_fullscreenTextLabel = nullptr;
    SpectrumVisualizerWidget* m_fullscreenVisualizer = nullptr;
    QScrollArea* m_fullscreenLyricsScroll = nullptr;
    QLabel* m_fullscreenLyricsLabel = nullptr;
    QWidget* m_fullscreenLyricsPanel = nullptr;

    struct SyncedLyricLine {
        qint64 timestampMs;
        QString text;
    };
    QList<SyncedLyricLine> m_syncedLyrics;
    QList<QLabel*> m_lyricLabels;
    int m_currentLyricLineIndex = -1;
    bool m_useScrollingLyrics = true;
    QPushButton* m_btnToggleLyricMode = nullptr;
    QWidget* m_lyricContainerWidget = nullptr;
    bool m_hasSyncData = false;
    QString m_rawLyricsText;
    QWidget* m_fullscreenBottomLyricsWidget = nullptr;
    QLabel* m_lblBottomLyricsCurrent = nullptr;
    QLabel* m_lblBottomLyricsNext = nullptr;
    QWidget* m_fullscreenLeftPanel = nullptr;

    void updateLyricsPosition(qint64 positionMs);
    void onToggleLyricsMode();
    void rebuildLyricsView();
    void updateLyricsLayout();
    void onShowLyricsMenu();

    void buildFullscreenContent(bool isVideo, const QString& activePath, class QVBoxLayout* mainLayout);

    QIcon getTrackArtworkIcon(const QString& trackPath);
    void refreshPlaylistUI();

private slots:
    void onPlaylistItemDoubleClicked(class QListWidgetItem* item);
    void onChooseOverlayIcon();
    void onClearOverlayIcon();
    void onApplyTagsColors();
    void onAutoPreviewToggled();
    void onSaveMusicTags();
};

#endif // PREVIEWPANEL_H
