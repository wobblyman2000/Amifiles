#ifndef FOLDERARTSCRAPERDIALOG_H
#define FOLDERARTSCRAPERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QCheckBox>

class FolderArtScraperDialog : public QDialog {
    Q_OBJECT
public:
    explicit FolderArtScraperDialog(const QString& folderPath, QWidget* parent = nullptr);
    ~FolderArtScraperDialog() override = default;

private slots:
    void onSearchClicked();
    void onTableSelectionChanged();
    void onApplyClicked();

    // Network callbacks
    void onMusicSearchFinished(QNetworkReply* reply);
    void onMusicCoverArtFinished(QNetworkReply* reply);
    void onVideoSearchFinished(QNetworkReply* reply);
    void onDownloadArtworkFinished(QNetworkReply* reply);

private:
    void setupUI();
    void loadApiKey();
    void triggerMusicSearch(const QString& query);
    void triggerVideoSearch(const QString& query);

    QString m_folderPath;
    QString m_apiKey;
    QNetworkAccessManager* m_networkManager = nullptr;
    QByteArray m_downloadedArtwork;
    QString m_artworkMimeType;

    // UI elements
    QLineEdit* m_editSearch = nullptr;
    QComboBox* m_comboType = nullptr;
    QPushButton* m_btnSearch = nullptr;
    QTableWidget* m_tableResults = nullptr;
    QLabel* m_lblArtworkPreview = nullptr;
    QLabel* m_lblStatus = nullptr;
    QCheckBox* m_chkSaveAsCover = nullptr;
    QCheckBox* m_chkSaveAsFolder = nullptr;
    QCheckBox* m_chkSaveAsBackdrop = nullptr;
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnCancel = nullptr;

    struct ScrapeResult {
        QString id;
        QString title;
        QString artistOrYear;
        QString type;
        QString backdropUrl;
    };
    QList<ScrapeResult> m_results;
};

#endif // FOLDERARTSCRAPERDIALOG_H
