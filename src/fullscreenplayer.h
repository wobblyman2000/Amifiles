#ifndef FULLSCREENPLAYER_H
#define FULLSCREENPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QDir>
#include <QStackedWidget>
#include <QPainter>
#include <QGraphicsOpacityEffect>

class FullscreenPlayer : public QWidget {
    Q_OBJECT
public:
    explicit FullscreenPlayer(QWidget* parent = nullptr);
    ~FullscreenPlayer() override;

    bool isPlaying() const;
    QStringList currentPlaylist() const { return m_playlist; }
    int currentIndex() const { return m_currentIndex; }

public slots:
    void playMedia(const QString& filePath);
    void playPlaylist(const QStringList& filePaths);
    void enqueueMedia(const QStringList& filePaths);
    void togglePlayPause();
    void playNext();
    void playPrevious();
    void stopPlayback();
    void closePlayer();

signals:
    void playbackEnded();
    void queueChanged(int newCount);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onSliderSeek(int value);
    void onVolumeChanged(int value);
    void onSubtitleSelected(int index);
    void toggleQueueDrawer();
    void onQueueItemDoubleClicked(QListWidgetItem* item);
    void hideOverlayTimerTimeout();

private:
    void buildUi();
    void updateTitleLabel();
    void updateQueueWidget();
    void resetOverlayTimer();
    QString formatTime(qint64 ms) const;
    void scanCompanionSubtitles(const QString& videoPath);

    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    QVideoWidget* m_videoWidget;

    // Overlay Widgets
    QWidget* m_topBar;
    QWidget* m_bottomBar;
    QWidget* m_queueDrawer;
    QListWidget* m_queueListWidget;

    QLabel* m_lblTitle;
    QLabel* m_lblTime;
    QSlider* m_seekSlider;
    QSlider* m_volumeSlider;

    QPushButton* m_btnPlayPause;
    QPushButton* m_btnPrev;
    QPushButton* m_btnNext;
    QPushButton* m_btnClose;
    QPushButton* m_btnQueue;
    QComboBox* m_comboSubtitles;

    QTimer* m_overlayTimer;
    QStringList m_playlist;
    int m_currentIndex;
    qint64 m_durationMs;
};

#endif // FULLSCREENPLAYER_H
