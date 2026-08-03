#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QList>
#include <QMap>
#include "metadataextractor.h"

#include <QSet>

struct TrackEditInfo {
    QString originalPath;
    QString currentPath;
    FileMetadata metadata;
    bool isModified = false;
    QByteArray coverData;
    QString coverMimeType;
    bool coverChanged = false;
    QSet<QString> lockedFields;
};

#include <QPlainTextEdit>

#include <QComboBox>
#include <QTableWidget>
#include <QToolButton>

class AdvancedTagEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedTagEditorDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~AdvancedTagEditorDialog() override = default;

private slots:
    void onTableSelectionChanged();
    void onFieldEdited();
    void onBrowseArtwork();
    void onPasteArtwork();
    void onDeleteArtwork();
    void onAutoTagFromFilename();
    void onRenameFromTags();
    void onTrackNumberWizard();
    void onCaseConversion();
    void onOnlineScrape();
    void onQuickActionTriggered();
    void onFetchLyricsClicked();
    void onExtractArtwork();
    void onAddCustomTag();
    void onRemoveCustomTag();
    void onCustomTagCellChanged();
    void onApplyClicked();
    void onSaveClicked();

private:
    void setupUI();
    void loadFiles();
    void populateTable();
    void updateFormFromSelection();
    void applyFieldsToSelection();
    void saveTagsToDisk();
    
    // Helpers
    QPixmap loadArtworkPixmap(const QByteArray& data, const QString& mimeType);
    void populateCustomTagsTable(const QSet<int>& selectedRows);

    QStringList m_initialPaths;
    QList<TrackEditInfo> m_tracks;
    bool m_blockFormUpdates = false;

    // UI elements - Left Panel
    QTableWidget* m_tableFiles = nullptr;
    QLabel* m_lblStatus = nullptr;

    // UI elements - Right Panel Fields
    QLineEdit* m_editTitle = nullptr;
    QLineEdit* m_editArtist = nullptr;
    QLineEdit* m_editAlbum = nullptr;
    QComboBox* m_editGenre = nullptr;
    QLineEdit* m_editYear = nullptr;
    QLineEdit* m_editTrack = nullptr;
    QLineEdit* m_editTrackTotal = nullptr;
    QLineEdit* m_editAlbumArtist = nullptr;
    QLineEdit* m_editDisc = nullptr;
    QLineEdit* m_editDiscTotal = nullptr;
    QLineEdit* m_editComposer = nullptr;
    QLineEdit* m_editBpm = nullptr;
    QLineEdit* m_editComment = nullptr;
    QTableWidget* m_tableCustomTags = nullptr;
    QPushButton* m_btnAddCustomTag = nullptr;
    QPushButton* m_btnRemoveCustomTag = nullptr;
    QPlainTextEdit* m_editLyrics = nullptr;
    QCheckBox* m_chkCompilation = nullptr;

    // Lock buttons
    QToolButton* m_lockTitle = nullptr;
    QToolButton* m_lockArtist = nullptr;
    QToolButton* m_lockAlbum = nullptr;
    QToolButton* m_lockGenre = nullptr;
    QToolButton* m_lockYear = nullptr;
    QToolButton* m_lockTrack = nullptr;
    QToolButton* m_lockTrackTotal = nullptr;
    QToolButton* m_lockAlbumArtist = nullptr;
    QToolButton* m_lockDisc = nullptr;
    QToolButton* m_lockDiscTotal = nullptr;
    QToolButton* m_lockComposer = nullptr;
    QToolButton* m_lockBpm = nullptr;
    QToolButton* m_lockComment = nullptr;
    QToolButton* m_lockLyrics = nullptr;
    QToolButton* m_lockCompilation = nullptr;
    QToolButton* m_lockArtwork = nullptr;

    // Checkboxes for bulk edits
    QCheckBox* m_chkWTitle = nullptr;
    QCheckBox* m_chkWArtist = nullptr;
    QCheckBox* m_chkWAlbum = nullptr;
    QCheckBox* m_chkWGenre = nullptr;
    QCheckBox* m_chkWYear = nullptr;
    QCheckBox* m_chkWTrack = nullptr;
    QCheckBox* m_chkWTrackTotal = nullptr;
    QCheckBox* m_chkWAlbumArtist = nullptr;
    QCheckBox* m_chkWDisc = nullptr;
    QCheckBox* m_chkWDiscTotal = nullptr;
    QCheckBox* m_chkWComposer = nullptr;
    QCheckBox* m_chkBpm = nullptr;
    QCheckBox* m_chkWComment = nullptr;
    QCheckBox* m_chkWLyrics = nullptr;
    QCheckBox* m_chkWCompilation = nullptr;
    QCheckBox* m_chkWArtwork = nullptr;

    // Artwork
    QLabel* m_lblArtworkPreview = nullptr;
    QPushButton* m_btnBrowseArtwork = nullptr;
    QPushButton* m_btnPasteArtwork = nullptr;
    QPushButton* m_btnDeleteArtwork = nullptr;
    
    QByteArray m_currentArtworkData;
    QString m_currentArtworkMimeType;
    bool m_currentArtworkChanged = false;

    // Network manager and online lyrics helpers
    class QNetworkAccessManager* m_networkManager = nullptr;
    int m_activeLyricsQueries = 0;
    void fetchLyricsForTrack(int idx);
    void fetchLyricsOvh(int idx);
    void updateUIIfSelected(int idx);
};
