#ifndef TAGEDITORDIALOG_H
#define TAGEDITORDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class TagEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit TagEditorDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~TagEditorDialog() override = default;

private slots:
    void onSaveClicked();
    void onPasteArtwork();
    void onExtractArtwork();

private:
    void setupUI();
    void loadCommonTags();
    bool writeMp3Tags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year,
                      const QString& albumArtist, const QString& discNumber, bool compilation);
    bool writeFlacTags(const QString& filePath, const QString& title, const QString& artist, const QString& album, const QString& genre, const QString& year,
                       const QString& albumArtist, const QString& discNumber, bool compilation);
    bool writeExifTags(const QString& filePath, const QString& camera, const QString& dateTaken);

    QStringList m_filePaths;

    QLineEdit* m_editTitle = nullptr;
    QLineEdit* m_editArtist = nullptr;
    QLineEdit* m_editAlbum = nullptr;
    QLineEdit* m_editGenre = nullptr;
    QLineEdit* m_editYear = nullptr;
    QLineEdit* m_editAlbumArtist = nullptr;
    QLineEdit* m_editDiscNumber = nullptr;
    class QCheckBox* m_chkCompilation = nullptr;

    // Artwork controls
    QLabel* m_lblArtworkStatus = nullptr;
    QPushButton* m_btnPasteArtwork = nullptr;
    QPushButton* m_btnExtractArtwork = nullptr;

    // Image metadata edits
    QLineEdit* m_editCamera = nullptr;
    QLineEdit* m_editDateTaken = nullptr;
};

#endif // TAGEDITORDIALOG_H
