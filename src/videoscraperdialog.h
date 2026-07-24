#ifndef VIDEOSCRAPERDIALOG_H
#define VIDEOSCRAPERDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QTextBrowser>
#include <QCheckBox>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>

struct VideoSearchResult {
    QString id;
    QString title;
    QString year;
    QString rating;
    QString genres;
    QString overview;
    QString posterUrl;
    QString type; // "Movie" or "TV Show"
    QString studio;
};

struct EpisodeInfo {
    int season = -1;
    int episode = -1;
    QString title;
    QString overview;
};

class VideoScraperDialog : public QDialog {
    Q_OBJECT
public:
    explicit VideoScraperDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~VideoScraperDialog() override = default;

private slots:
    void onSearchClicked();
    void onSearchFinished();
    void onPosterFinished();
    void onTableSelectionChanged();
    void onApplyClicked();

private:
    void setupUI();
    void loadApiKey();
    void triggerSearch(const QString& query, const QString& type);
    void fetchPoster(const QString& url);
    void writeNfoFile(const QString& targetFolder, const VideoSearchResult& res);
    QString renameTarget(const QString& path, const QString& newName);

    struct SeasonEpisode {
        int season = -1;
        int episode = -1;
    };
    SeasonEpisode parseSeasonEpisode(const QString& fileName);
    QList<EpisodeInfo> fetchEpisodesList(const QString& showId);
    void processTVShowEpisodes(const QString& targetFolder, const QString& showTitle, const QList<EpisodeInfo>& episodes);
    void writeEpisodeNfoFile(const QString& filePath, const EpisodeInfo& ep, const QString& showTitle);

    QStringList m_filePaths;
    QString m_apiKey;
    QNetworkAccessManager* m_networkManager = nullptr;
    QNetworkReply* m_currentSearchReply = nullptr;
    QNetworkReply* m_currentPosterReply = nullptr;
    QList<VideoSearchResult> m_results;

    // UI Widgets
    QLineEdit* m_editSearch = nullptr;
    QComboBox* m_comboType = nullptr;
    QPushButton* m_btnSearch = nullptr;
    QTableWidget* m_tableResults = nullptr;

    QLabel* m_lblPosterPreview = nullptr;
    QLabel* m_lblTitle = nullptr;
    QLabel* m_lblYear = nullptr;
    QLabel* m_lblRating = nullptr;
    QLabel* m_lblGenres = nullptr;
    QTextBrowser* m_txtOverview = nullptr;

    QCheckBox* m_chkSaveNfo = nullptr;
    QCheckBox* m_chkSavePoster = nullptr;
    QCheckBox* m_chkRename = nullptr;

    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnCancel = nullptr;

    QByteArray m_downloadedPosterData;
};

#endif // VIDEOSCRAPERDIALOG_H
