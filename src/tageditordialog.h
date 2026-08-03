#ifndef TAGEDITORDIALOG_H
#define TAGEDITORDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "metadatafetcherdialog.h"

class TagEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit TagEditorDialog(const QStringList& filePaths, QWidget* parent = nullptr, bool autoStartFetch = false);
    ~TagEditorDialog() override = default;

    static bool writeMp3Tags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year,
                             const QString& albumArtist, const QString& discNumber, bool compilation,
                             bool stripArtwork = false, const QByteArray& newArtworkData = QByteArray(), const QString& mimeType = "image/jpeg",
                             const QString& trackNumber = QString(), const QString& trackTotal = QString(),
                             const QString& discTotal = QString(), const QString& composer = QString(),
                             const QString& bpm = QString(), const QString& comment = QString(),
                             const QString& lyrics = QString(),
                             const QMap<QString, QString>& customTags = QMap<QString, QString>());
    static bool writeFlacTags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year,
                              const QString& albumArtist, const QString& discNumber, bool compilation,
                              const QString& trackNumber = QString(), const QString& trackTotal = QString(),
                              const QString& discTotal = QString(), const QString& composer = QString(),
                              const QString& bpm = QString(), const QString& comment = QString(),
                              const QString& lyrics = QString(),
                              const QMap<QString, QString>& customTags = QMap<QString, QString>());

    static bool stripFlacArtwork(const QString& filePath);
    static bool writeFlacArtwork(const QString& filePath, const QByteArray& imgData, const QString& mimeType = "image/jpeg");

private slots:
    void onSaveClicked();
    void onPasteArtwork();
    void onExtractArtwork();
    void onDeleteArtwork();
    void onPrevArtwork();
    void onNextArtwork();
    void onAutoFetchClicked();
    void onBrowseArtwork();

private:
    void setupUI();
    void loadCommonTags();
    void updateArtworkDisplay();
    QStringList getEmbeddedArtworkTags(const QString& filePath);
    class QPixmap loadEmbeddedArtwork(const QString& filePath);
    class QPixmap loadEmbeddedArtworkAtIndex(const QString& filePath, const QString& tag);
    bool writeExifTags(const QString& filePath, const QString& camera, const QString& dateTaken);

    QStringList m_filePaths;

    QLineEdit* m_editTitle = nullptr;
    QLineEdit* m_editArtist = nullptr;
    QLineEdit* m_editAlbum = nullptr;
    QLineEdit* m_editGenre = nullptr;
    QLineEdit* m_editYear = nullptr;
    QLineEdit* m_editAlbumArtist = nullptr;
    QLineEdit* m_editDiscNumber = nullptr;
    QLineEdit* m_editDiscTotal = nullptr;
    QLineEdit* m_editTrackNumber = nullptr;
    QLineEdit* m_editTrackTotal = nullptr;
    QLineEdit* m_editComposer = nullptr;
    QLineEdit* m_editBpm = nullptr;
    QLineEdit* m_editComment = nullptr;
    class QCheckBox* m_chkCompilation = nullptr;

    // Artwork controls
    QLabel* m_lblArtworkStatus = nullptr;
    QLabel* m_lblArtworkPreview = nullptr;
    QPushButton* m_btnPasteArtwork = nullptr;
    QPushButton* m_btnBrowseArtwork = nullptr;
    QPushButton* m_btnExtractArtwork = nullptr;
    QPushButton* m_btnDeleteArtwork = nullptr;
    QPushButton* m_btnPrevArtwork = nullptr;
    QPushButton* m_btnNextArtwork = nullptr;
    QLabel* m_lblArtworkIndex = nullptr;

    int m_currentArtworkIndex = 0;
    QStringList m_availableArtworkTags;

    QByteArray m_fetchedArtworkData;
    QString m_fetchedArtworkMimeType;
    QMap<int, FetchedTrack> m_fetchedMetadataMap;

    // Image metadata edits
    QLineEdit* m_editCamera = nullptr;
    QLineEdit* m_editDateTaken = nullptr;
};

#endif // TAGEDITORDIALOG_H
