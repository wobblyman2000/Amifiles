#ifndef METADATAFETCHERDIALOG_H
#define METADATAFETCHERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>

struct FetchedTrack {
    QString title;
    QString artist;
    QString album;
    QString year;
    QString genre;
    int trackNumber = 0;
    int trackCount = 0;
    QByteArray artworkData;
    QString mimeType;
};

class MetadataFetcherDialog : public QDialog {
    Q_OBJECT
public:
    explicit MetadataFetcherDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~MetadataFetcherDialog() override = default;

    // Returns map of local file index -> fetched metadata structure
    QMap<int, FetchedTrack> getResults() const { return m_matchedResults; }
    QByteArray getArtworkData() const { return m_artworkData; }
    QString getArtworkMimeType() const { return m_artworkMimeType; }

private slots:
    void onSearch();
    void onReleaseSelected();
    void onApply();
    
    // Network reply handlers
    void onSearchFinished(QNetworkReply* reply);
    void onReleaseDetailsFinished(QNetworkReply* reply);
    void onCoverArtFinished(QNetworkReply* reply);
    void onDownloadArtworkFinished(QNetworkReply* reply);

private:
    void setupUI();
    void autoMapTracks();

    QStringList m_filePaths;
    QMap<int, FetchedTrack> m_matchedResults;
    QByteArray m_artworkData;
    QString m_artworkMimeType;

    // UI elements
    QLineEdit* m_editArtistSearch = nullptr;
    QLineEdit* m_editAlbumSearch = nullptr;
    QPushButton* m_btnSearch = nullptr;

    QTableWidget* m_tableReleases = nullptr;
    QTableWidget* m_tableTracks = nullptr;
    QTableWidget* m_tableMapping = nullptr;

    QLabel* m_lblArtworkPreview = nullptr;
    QLabel* m_lblStatus = nullptr;

    QNetworkAccessManager* m_networkManager = nullptr;
    
    // Cache for release details to avoid duplicate fetches
    struct ReleaseInfo {
        QString mbid;
        QString title;
        QString artist;
        QString year;
        int trackCount = 0;
    };
    QList<ReleaseInfo> m_releases;

    struct TrackInfo {
        QString title;
        int number = 0;
    };
    QList<TrackInfo> m_currentReleaseTracks;
    QString m_selectedReleaseMbid;
};

#endif // METADATAFETCHERDIALOG_H
