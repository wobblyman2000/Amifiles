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
#include "metadataextractor.h"

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
    ~FullscreenWidget() override = default;

    void setMediaState(bool isVideo, class QMediaPlayer* player, class QAudioOutput* audioOutput);
    void updateProgress(qint64 position, qint64 duration);

signals:
    void exitRequested();
    void prevRequested();
    void playPauseRequested();
    void stopRequested();
    void nextRequested();
    void shuffleToggled();
    void repeatRequested();

protected:
    void keyPressEvent(class QKeyEvent* event) override;
    void mouseDoubleClickEvent(class QMouseEvent* event) override;
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

private:
    void showHud();

    class QWidget* m_hudWidget = nullptr;
    class QPushButton* m_btnPlayPause = nullptr;
    class QPushButton* m_btnSubtitles = nullptr;
    class QPushButton* m_btnShuffle = nullptr;
    class QPushButton* m_btnRepeat = nullptr;
    class QSlider* m_sliderProgress = nullptr;
    class QLabel* m_lblTime = nullptr;
    class QSlider* m_sliderVolume = nullptr;
    class QTimer* m_hideTimer = nullptr;
    class QTimer* m_mousePollTimer = nullptr;
    QPoint m_lastMousePos;
    class QMediaPlayer* m_player = nullptr;
public:
    QPushButton* hudShuffleButton() const { return m_btnShuffle; }
    QPushButton* hudRepeatButton() const { return m_btnRepeat; }
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
    void setPlaying(bool playing);
    void setBoost(double bass, double mid, double treble);
    void setVisualizerMode(VisualizerMode mode);
    VisualizerMode visualizerMode() const { return m_mode; }

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimate();

private:
    QTimer* m_timer = nullptr;
    bool m_playing = false;
    double m_bassBoost = 1.0;
    double m_midBoost = 1.0;
    double m_trebleBoost = 1.0;
    VisualizerMode m_mode = VisualizerBars;
    QVector<double> m_barHeights;
    QVector<double> m_targetHeights;
    QList<double> m_waveformHistory;
};

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget* parent = nullptr);
    ~PreviewPanel() override;

    void previewFile(const QString& filePath, const QStringList& siblingSelections = QStringList());
    void previewFolderArt(const QString& artPath, const QString& folderPath);
    void clearPreview();
    void playPlaylist(const QStringList& filePaths);
    QMediaPlayer* player() const { return m_player; }
    bool isFullscreen() const { return m_fullscreenWidget != nullptr; }
    void setMuted(bool muted);
    bool isMuted() const;
    void setAudioCoverArtVisible(bool visible);
    void setSpectrumVisualizerVisible(bool visible);
    void setZenMode(bool enabled);

public slots:
    void toggleFullscreen();
    void exitFullscreen();
    void updateFullscreenTrack();

signals:
    void tagsChanged(const QString& filePath);
    void spectrumVisualizerToggled(bool checked);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, class QEvent* event) override;
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dragMoveEvent(class QDragMoveEvent* event) override;
    void dropEvent(class QDropEvent* event) override;

private slots:
    void onSaveText();
    void onTextChanged();
    
    // Media Player Slots
    void onPlayPause();
    void onStop();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onVolumeChanged(int value);
    void onSliderMoved(int value);
    void onMediaMetadataChanged();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onSubtitleMenuRequested();
    void onShuffleToggled();
    void onRepeatClicked();
    void onEqPresetChanged(int index);
    void onEqSlidersChanged();
    void showPlaylistContextMenu(const QPoint& pos);

private:
    void setupUI();
    void showTextPreview(const QString& filePath);
    void showImagePreview(const QString& filePath);
    void showMediaPreview(const QString& filePath, bool isVideo);
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
    bool m_shuffleEnabled = false;
    int m_repeatMode = 0; // 0 = Off, 1 = Repeat One, 2 = Repeat All
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
    class QTextEdit* m_pdfTextEdit = nullptr;

    // Media View
    QWidget* m_mediaView = nullptr;
    QVideoWidget* m_videoWidget = nullptr;
    AudioPlaceholderWidget* m_audioPlaceholder = nullptr;
    QPushButton* m_btnPlayPause = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnPrevTrack = nullptr;
    QPushButton* m_btnNextTrack = nullptr;
    QSlider* m_sliderProgress = nullptr;
    QLabel* m_lblProgressTime = nullptr;
    QSlider* m_sliderVolume = nullptr;
    QPushButton* m_btnFullscreen = nullptr;
    QPushButton* m_btnSubtitles = nullptr;
    QPushButton* m_btnShuffle = nullptr;
    QPushButton* m_btnRepeat = nullptr;

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
    class HexEditorWidget* m_hexViewer = nullptr;

    // Fullscreen support
    FullscreenWidget* m_fullscreenWidget = nullptr;
    QVideoWidget* m_fullscreenVideoWidget = nullptr;
    QLabel* m_fullscreenAudioLabel = nullptr;
    QLabel* m_fullscreenTextLabel = nullptr;

private slots:
    void onPrevTrack();
    void onNextTrack();
    void onPlaylistItemDoubleClicked(class QListWidgetItem* item);
    void onChooseOverlayIcon();
    void onClearOverlayIcon();
    void onApplyTagsColors();
};

#endif // PREVIEWPANEL_H
