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
    bool m_coverArtVisible = true;
};

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget* parent = nullptr);
    ~PreviewPanel() override;

    void previewFile(const QString& filePath);
    void previewFolderArt(const QString& artPath, const QString& folderPath);
    void clearPreview();
    void playPlaylist(const QStringList& filePaths);
    QMediaPlayer* player() const { return m_player; }
    void setMuted(bool muted);
    bool isMuted() const;
    void setAudioCoverArtVisible(bool visible);

protected:
    void resizeEvent(QResizeEvent* event) override;

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

private:
    void setupUI();
    void showTextPreview(const QString& filePath);
    void showImagePreview(const QString& filePath);
    void showMediaPreview(const QString& filePath, bool isVideo);
    void updateMetadataDisplay(const FileMetadata& meta);
    void scaleImage();
    void updateAudioPlaceholder(const QString& filePath);
    
    QString formatDuration(qint64 ms);

    QString m_previewedFilePath;
    QString m_currentAudioPath;
    bool m_textChanged = false;
    QStringList m_playlist;
    int m_playlistIndex = -1;
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

    // Bottom half: Tabbed view for Metadata and Playlist Queue
    class QTabWidget* m_bottomTab = nullptr;
    QWidget* m_metadataContainer = nullptr;
    QTableWidget* m_metadataTable = nullptr;
    class QListWidget* m_playlistList = nullptr;

private slots:
    void onPrevTrack();
    void onNextTrack();
    void onPlaylistItemDoubleClicked(class QListWidgetItem* item);
};

#endif // PREVIEWPANEL_H
