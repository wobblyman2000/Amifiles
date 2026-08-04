#include "metadatafetcherdialog.h"
#include "metadataextractor.h"
#include "version.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QComboBox>
#include <QPixmap>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>

MetadataFetcherDialog::MetadataFetcherDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Online Metadata Fetcher");
    resize(850, 600);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QLineEdit { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 4px 8px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QTableWidget { background-color: #181825; gridline-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; selection-background-color: #89b4fa; selection-color: #11111b; }"
                  "QHeaderView::section { background-color: #1e1e2e; color: #89b4fa; font-weight: bold; border: 1px solid #313244; padding: 4px; }"
                  "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 2px 6px; }");

    m_networkManager = new QNetworkAccessManager(this);

    setupUI();

    // Prefill search boxes from the first file's metadata
    if (!m_filePaths.isEmpty()) {
        FileMetadata meta = MetadataExtractor::extract(m_filePaths.first());
        m_editArtistSearch->setText(meta.artist);
        m_editAlbumSearch->setText(meta.album);
        if (meta.artist.isEmpty() && meta.album.isEmpty()) {
            // fallback to parsing filename
            QString baseName = QFileInfo(m_filePaths.first()).completeBaseName();
            int dashIdx = baseName.indexOf('-');
            if (dashIdx != -1) {
                m_editArtistSearch->setText(baseName.left(dashIdx).trimmed());
                m_editAlbumSearch->setText(baseName.mid(dashIdx + 1).trimmed());
            } else {
                m_editArtistSearch->setText(baseName);
            }
        }
    }
}

void MetadataFetcherDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Source selection panel
    QHBoxLayout* sourceLayout = new QHBoxLayout();
    sourceLayout->setSpacing(8);

    m_comboSource = new QComboBox(this);
    m_comboSource->addItems({"MusicBrainz", "Discogs"});
    
    m_editDiscogsToken = new QLineEdit(this);
    m_editDiscogsToken->setPlaceholderText("Enter Discogs Personal Access Token...");
    m_editDiscogsToken->setEchoMode(QLineEdit::Password);
    
    QSettings settings("Amifiles", "Amifiles");
    m_editDiscogsToken->setText(settings.value("discogs_token", "").toString());
    m_editDiscogsToken->setVisible(false);

    m_lblTokenHint = new QLabel(this);
    m_lblTokenHint->setText("<a href='https://www.discogs.com/settings/developers' style='color:#89b4fa;'>Get Token</a>");
    m_lblTokenHint->setOpenExternalLinks(true);
    m_lblTokenHint->setVisible(false);

    sourceLayout->addWidget(new QLabel("Meta Source:", this));
    sourceLayout->addWidget(m_comboSource);
    sourceLayout->addWidget(m_editDiscogsToken, 1);
    sourceLayout->addWidget(m_lblTokenHint);
    
    connect(m_comboSource, &QComboBox::currentIndexChanged, this, &MetadataFetcherDialog::onSourceChanged);
    mainLayout->addLayout(sourceLayout);

    // Search inputs
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(8);

    m_editArtistSearch = new QLineEdit(this);
    m_editArtistSearch->setPlaceholderText("Artist Name");
    m_editAlbumSearch = new QLineEdit(this);
    m_editAlbumSearch->setPlaceholderText("Album Name");

    m_btnSearch = new QPushButton("Search MusicBrainz", this);
    m_btnSearch->setStyleSheet("background-color: #89b4fa; color: #11111b;");
    connect(m_btnSearch, &QPushButton::clicked, this, &MetadataFetcherDialog::onSearch);

    searchLayout->addWidget(new QLabel("Artist:", this));
    searchLayout->addWidget(m_editArtistSearch, 1);
    searchLayout->addWidget(new QLabel("Album:", this));
    searchLayout->addWidget(m_editAlbumSearch, 1);
    searchLayout->addWidget(m_btnSearch);
    mainLayout->addLayout(searchLayout);

    // Content splitter / layout
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    // Left side: Release list & Track list
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(8);

    leftLayout->addWidget(new QLabel("Step 1: Select Release / Album", this));
    m_tableReleases = new QTableWidget(this);
    m_tableReleases->setColumnCount(4);
    m_tableReleases->setHorizontalHeaderLabels({"Artist", "Album", "Year", "Tracks"});
    m_tableReleases->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableReleases->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableReleases->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(m_tableReleases, &QTableWidget::itemSelectionChanged, this, &MetadataFetcherDialog::onReleaseSelected);
    leftLayout->addWidget(m_tableReleases, 2);

    leftLayout->addWidget(new QLabel("Release Tracks", this));
    m_tableTracks = new QTableWidget(this);
    m_tableTracks->setColumnCount(2);
    m_tableTracks->setHorizontalHeaderLabels({"#", "Title"});
    m_tableTracks->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableTracks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableTracks->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    leftLayout->addWidget(m_tableTracks, 1);

    contentLayout->addLayout(leftLayout, 3);

    // Right side: Mapping & Artwork Preview
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(8);

    rightLayout->addWidget(new QLabel("Album Artwork", this));
    m_lblArtworkPreview = new QLabel(this);
    m_lblArtworkPreview->setFixedSize(160, 160);
    m_lblArtworkPreview->setStyleSheet("border: 1px solid #45475a; border-radius: 6px; background-color: #181825; color: #a6adc8;");
    m_lblArtworkPreview->setAlignment(Qt::AlignCenter);
    m_lblArtworkPreview->setText("No Artwork");
    rightLayout->addWidget(m_lblArtworkPreview, 0, Qt::AlignHCenter);

    rightLayout->addWidget(new QLabel("Step 2: Map Local Files to Release Tracks", this));
    m_tableMapping = new QTableWidget(this);
    m_tableMapping->setColumnCount(2);
    m_tableMapping->setHorizontalHeaderLabels({"Local File", "Online Track"});
    m_tableMapping->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rightLayout->addWidget(m_tableMapping, 1);

    contentLayout->addLayout(rightLayout, 2);
    mainLayout->addLayout(contentLayout, 1);

    // Status bar & actions
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_lblStatus = new QLabel("Ready", this);
    m_lblStatus->setStyleSheet("color: #a6adc8;");
    bottomLayout->addWidget(m_lblStatus, 1);

    m_btnApply = new QPushButton("Apply Tags", this);
    m_btnApply->setStyleSheet("background-color: #a6e3a1; color: #11111b;");
    connect(m_btnApply, &QPushButton::clicked, this, &MetadataFetcherDialog::onApply);
    bottomLayout->addWidget(m_btnApply);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnCancel);

    mainLayout->addLayout(bottomLayout);
}

void MetadataFetcherDialog::onSearch() {
    QString artist = m_editArtistSearch->text().trimmed();
    QString album = m_editAlbumSearch->text().trimmed();
    if (artist.isEmpty() && album.isEmpty()) {
        QMessageBox::warning(this, "Empty Query", "Please enter at least an Artist or Album name to search.");
        return;
    }

    m_btnSearch->setEnabled(false);
    m_tableReleases->setRowCount(0);
    m_tableTracks->setRowCount(0);
    m_tableMapping->setRowCount(0);
    m_releases.clear();
    m_lblArtworkPreview->clear();
    m_lblArtworkPreview->setText("No Artwork");
    m_artworkData.clear();

    if (m_comboSource->currentIndex() == 0) {
        m_lblStatus->setText("Searching MusicBrainz...");
        QUrl url("https://musicbrainz.org/ws/2/release/");
        QUrlQuery query;
        QString qStr;
        if (!artist.isEmpty()) qStr += QString("artist:\"%1\"").arg(artist);
        if (!album.isEmpty()) {
            if (!qStr.isEmpty()) qStr += " AND ";
            qStr += QString("release:\"%1\"").arg(album);
        }
        query.addQueryItem("query", qStr);
        query.addQueryItem("fmt", "json");
        url.setQuery(query);

        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
        QNetworkReply* reply = m_networkManager->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onSearchFinished(reply); });
    } else {
        QString token = m_editDiscogsToken->text().trimmed();
        if (token.isEmpty()) {
            QMessageBox::warning(this, "Discogs Token Required", "Discogs search requires a Personal Access Token.\nPlease enter it in the field above.");
            m_btnSearch->setEnabled(true);
            return;
        }

        // Save token to QSettings
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("discogs_token", token);

        m_lblStatus->setText("Searching Discogs...");
        QUrl url("https://api.discogs.com/database/search");
        QUrlQuery query;
        if (!artist.isEmpty()) query.addQueryItem("artist", artist);
        if (!album.isEmpty()) query.addQueryItem("release_title", album);
        query.addQueryItem("type", "release");
        url.setQuery(query);

        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
        req.setRawHeader("Authorization", QString("Discogs token=%1").arg(token).toUtf8());

        QNetworkReply* reply = m_networkManager->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onDiscogsSearchFinished(reply); });
    }
}

void MetadataFetcherDialog::onSearchFinished(QNetworkReply* reply) {
    m_btnSearch->setEnabled(true);
    if (reply->error() != QNetworkReply::NoError) {
        m_lblStatus->setText("Search failed: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonArray releasesArr = root["releases"].toArray();

    m_tableReleases->setRowCount(0);
    m_releases.clear();

    for (int i = 0; i < releasesArr.size(); ++i) {
        QJsonObject rel = releasesArr[i].toObject();
        ReleaseInfo info;
        info.mbid = rel["id"].toString();
        info.title = rel["title"].toString();
        
        QJsonArray artists = rel["artist-credit"].toArray();
        if (!artists.isEmpty()) {
            info.artist = artists[0].toObject()["name"].toString();
        }
        
        info.year = rel["date"].toString().left(4); // just extract year
        info.trackCount = rel["track-count"].toInt();

        m_releases.append(info);

        int row = m_tableReleases->rowCount();
        m_tableReleases->insertRow(row);
        m_tableReleases->setItem(row, 0, new QTableWidgetItem(info.artist));
        m_tableReleases->setItem(row, 1, new QTableWidgetItem(info.title));
        m_tableReleases->setItem(row, 2, new QTableWidgetItem(info.year));
        m_tableReleases->setItem(row, 3, new QTableWidgetItem(QString::number(info.trackCount)));
    }

    m_lblStatus->setText(QString("Found %1 releases").arg(m_releases.size()));
}

void MetadataFetcherDialog::onReleaseSelected() {
    int row = m_tableReleases->currentRow();
    if (row < 0 || row >= m_releases.size()) return;

    QString mbid = m_releases[row].mbid;
    m_selectedReleaseMbid = mbid;

    m_lblStatus->setText("Fetching release details & artwork...");
    m_tableTracks->setRowCount(0);
    m_currentReleaseTracks.clear();
    m_lblArtworkPreview->clear();
    m_lblArtworkPreview->setText("Loading Art...");
    m_artworkData.clear();

    if (m_comboSource->currentIndex() == 0) {
        // Fetch tracklist details
        QUrl url(QString("https://musicbrainz.org/ws/2/release/%1").arg(mbid));
        QUrlQuery query;
        query.addQueryItem("inc", "recordings");
        query.addQueryItem("fmt", "json");
        url.setQuery(query);

        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
        QNetworkReply* reply = m_networkManager->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReleaseDetailsFinished(reply); });

        // Fetch cover art info
        QUrl coverUrl(QString("https://coverartarchive.org/release/%1").arg(mbid));
        QNetworkRequest coverReq(coverUrl);
        coverReq.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
        QNetworkReply* coverReply = m_networkManager->get(coverReq);
        connect(coverReply, &QNetworkReply::finished, this, [this, coverReply]() { onCoverArtFinished(coverReply); });
    } else {
        QString token = m_editDiscogsToken->text().trimmed();
        QUrl url(QString("https://api.discogs.com/releases/%1").arg(mbid));

        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
        req.setRawHeader("Authorization", QString("Discogs token=%1").arg(token).toUtf8());

        QNetworkReply* reply = m_networkManager->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onDiscogsReleaseDetailsFinished(reply); });
    }
}

void MetadataFetcherDialog::onReleaseDetailsFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_lblStatus->setText("Failed to load tracks: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonArray mediaArr = root["media"].toArray();

    m_tableTracks->setRowCount(0);
    m_currentReleaseTracks.clear();

    int trackCounter = 1;
    for (int i = 0; i < mediaArr.size(); ++i) {
        QJsonObject media = mediaArr[i].toObject();
        QJsonArray tracksArr = media["tracks"].toArray();
        for (int j = 0; j < tracksArr.size(); ++j) {
            QJsonObject track = tracksArr[j].toObject();
            TrackInfo info;
            info.title = track["title"].toString();
            info.number = trackCounter++;
            m_currentReleaseTracks.append(info);

            int row = m_tableTracks->rowCount();
            m_tableTracks->insertRow(row);
            m_tableTracks->setItem(row, 0, new QTableWidgetItem(QString::number(info.number)));
            m_tableTracks->setItem(row, 1, new QTableWidgetItem(info.title));
        }
    }

    autoMapTracks();
    m_lblStatus->setText("Tracks loaded successfully.");
}

void MetadataFetcherDialog::onCoverArtFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_lblArtworkPreview->setText("No Cover Art");
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonArray images = root["images"].toArray();

    if (!images.isEmpty()) {
        QJsonObject imgObj = images[0].toObject();
        QJsonObject thumbnails = imgObj["thumbnails"].toObject();
        QString imgUrl = thumbnails["250"].toString(); // 250px thumbnail is faster to load
        if (imgUrl.isEmpty()) {
            imgUrl = imgObj["image"].toString();
        }

        if (!imgUrl.isEmpty()) {
            QNetworkRequest imgReq;
            imgReq.setUrl(QUrl(imgUrl));
            imgReq.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
            QNetworkReply* imgReply = m_networkManager->get(imgReq);
            connect(imgReply, &QNetworkReply::finished, this, [this, imgReply]() { onDownloadArtworkFinished(imgReply); });
        } else {
            m_lblArtworkPreview->setText("No Image URL");
        }
    } else {
        m_lblArtworkPreview->setText("No Cover Art");
    }
}

void MetadataFetcherDialog::onDownloadArtworkFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_lblArtworkPreview->setText("Failed to download image");
        reply->deleteLater();
        return;
    }

    m_artworkData = reply->readAll();
    m_artworkMimeType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (m_artworkMimeType.isEmpty()) m_artworkMimeType = "image/jpeg";
    reply->deleteLater();

    QPixmap pixmap;
    if (pixmap.loadFromData(m_artworkData)) {
        m_lblArtworkPreview->setPixmap(pixmap.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_lblArtworkPreview->setText("Invalid Image");
    }
}

void MetadataFetcherDialog::autoMapTracks() {
    m_tableMapping->setRowCount(0);
    if (m_filePaths.isEmpty() || m_currentReleaseTracks.isEmpty()) return;

    for (int i = 0; i < m_filePaths.size(); ++i) {
        int row = m_tableMapping->rowCount();
        m_tableMapping->insertRow(row);

        QString filename = QFileInfo(m_filePaths[i]).fileName();
        m_tableMapping->setItem(row, 0, new QTableWidgetItem(filename));

        // Create combo box listing all release tracks
        QComboBox* combo = new QComboBox(this);
        combo->addItem("<Skip Matching>", -1);
        for (int j = 0; j < m_currentReleaseTracks.size(); ++j) {
            combo->addItem(QString("%1. %2").arg(m_currentReleaseTracks[j].number).arg(m_currentReleaseTracks[j].title), j);
        }

        // Try auto-matching by index first, then by title fuzziness
        int matchIdx = -1;
        if (i < m_currentReleaseTracks.size()) {
            matchIdx = i;
        }

        // Apply best match index
        if (matchIdx != -1) {
            combo->setCurrentIndex(matchIdx + 1); // +1 because of <Skip Matching>
        }

        m_tableMapping->setCellWidget(row, 1, combo);
    }
}

void MetadataFetcherDialog::onApply() {
    int row = m_tableReleases->currentRow();
    if (row < 0 || row >= m_releases.size()) {
        QMessageBox::warning(this, "Apply Error", "Please select a release from the list first.");
        return;
    }

    ReleaseInfo relInfo = m_releases[row];

    m_matchedResults.clear();
    QList<PendingLyricsFetch> pending;

    for (int i = 0; i < m_filePaths.size(); ++i) {
        QComboBox* combo = qobject_cast<QComboBox*>(m_tableMapping->cellWidget(i, 1));
        if (combo && combo->currentIndex() > 0) {
            int trackIdx = combo->currentData().toInt();
            if (trackIdx >= 0 && trackIdx < m_currentReleaseTracks.size()) {
                TrackInfo tInfo = m_currentReleaseTracks[trackIdx];

                FetchedTrack track;
                track.title = tInfo.title;
                track.artist = relInfo.artist;
                track.album = relInfo.title;
                track.year = relInfo.year;
                track.trackNumber = tInfo.number;
                track.trackCount = relInfo.trackCount;
                track.artworkData = m_artworkData;
                track.mimeType = m_artworkMimeType;

                m_matchedResults[i] = track;
                pending.append({i, tInfo.title, relInfo.artist, relInfo.title});
            }
        }
    }

    if (m_matchedResults.isEmpty()) {
        QMessageBox::warning(this, "Apply Error", "No files have been mapped to online tracks. Map at least one track to apply.");
        return;
    }

    if (m_btnApply) m_btnApply->setEnabled(false);
    if (m_btnSearch) m_btnSearch->setEnabled(false);

    fetchNextLyrics(pending, 0);
}

void MetadataFetcherDialog::fetchNextLyrics(const QList<PendingLyricsFetch>& pending, int index) {
    if (index >= pending.size()) {
        accept();
        return;
    }

    m_lblStatus->setText(QString("Fetching lyrics from LRCLib (%1/%2)...").arg(index + 1).arg(pending.size()));

    PendingLyricsFetch item = pending[index];
    
    QUrl url("https://lrclib.net/api/get");
    QUrlQuery query;
    query.addQueryItem("artist_name", item.artist);
    query.addQueryItem("track_name", item.title);
    if (!item.album.isEmpty()) {
        query.addQueryItem("album_name", item.album);
    }
    url.setQuery(query);
    
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
    
    QNetworkReply* reply = m_networkManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pending, index, item]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            QString lyrics = obj["syncedLyrics"].toString();
            if (lyrics.isEmpty()) {
                lyrics = obj["plainLyrics"].toString();
            }
            if (!lyrics.isEmpty()) {
                m_matchedResults[item.fileIndex].lyrics = lyrics;
            }
        }
        
        QTimer::singleShot(250, this, [this, pending, index]() {
            fetchNextLyrics(pending, index + 1);
        });
    });
}

void MetadataFetcherDialog::onSourceChanged(int index) {
    bool isDiscogs = (index == 1);
    m_editDiscogsToken->setVisible(isDiscogs);
    m_lblTokenHint->setVisible(isDiscogs);
    m_btnSearch->setText(isDiscogs ? "Search Discogs" : "Search MusicBrainz");
}

void MetadataFetcherDialog::onDiscogsSearchFinished(QNetworkReply* reply) {
    m_btnSearch->setEnabled(true);
    if (reply->error() != QNetworkReply::NoError) {
        m_lblStatus->setText("Discogs search failed: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonArray resultsArr = root["results"].toArray();

    m_tableReleases->setRowCount(0);
    m_releases.clear();

    for (int i = 0; i < resultsArr.size(); ++i) {
        QJsonObject rel = resultsArr[i].toObject();
        ReleaseInfo info;
        
        info.mbid = QString::number(rel["id"].toInt()); 
        
        QString fullTitle = rel["title"].toString();
        int dashIdx = fullTitle.indexOf(" - ");
        if (dashIdx != -1) {
            info.artist = fullTitle.left(dashIdx).trimmed();
            info.title = fullTitle.mid(dashIdx + 3).trimmed();
        } else {
            info.artist = m_editArtistSearch->text().trimmed();
            info.title = fullTitle;
        }

        info.year = rel["year"].toString();
        if (info.year.isEmpty()) {
            info.year = QString::number(rel["year"].toInt());
            if (info.year == "0") info.year = "";
        }
        
        info.trackCount = 0; 

        m_releases.append(info);

        int row = m_tableReleases->rowCount();
        m_tableReleases->insertRow(row);
        m_tableReleases->setItem(row, 0, new QTableWidgetItem(info.artist));
        m_tableReleases->setItem(row, 1, new QTableWidgetItem(info.title));
        m_tableReleases->setItem(row, 2, new QTableWidgetItem(info.year));
        m_tableReleases->setItem(row, 3, new QTableWidgetItem("N/A"));
    }

    m_lblStatus->setText(QString("Found %1 Discogs releases").arg(m_releases.size()));
}

void MetadataFetcherDialog::onDiscogsReleaseDetailsFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_lblStatus->setText("Failed to fetch release details: " + reply->errorString());
        m_lblArtworkPreview->setText("Load Failed");
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    QJsonArray tracklistArr = root["tracklist"].toArray();
    int trackIndex = 1;
    for (int i = 0; i < tracklistArr.size(); ++i) {
        QJsonObject trackObj = tracklistArr[i].toObject();
        QString type = trackObj["type_"].toString();
        if (!type.isEmpty() && type != "track") continue;

        TrackInfo info;
        info.title = trackObj["title"].toString();
        info.number = trackIndex++;
        m_currentReleaseTracks.append(info);

        int row = m_tableTracks->rowCount();
        m_tableTracks->insertRow(row);
        m_tableTracks->setItem(row, 0, new QTableWidgetItem(trackObj["position"].toString()));
        m_tableTracks->setItem(row, 1, new QTableWidgetItem(info.title));
    }

    m_lblStatus->setText(QString("Fetched %1 tracks").arg(m_currentReleaseTracks.size()));

    int currRow = m_tableReleases->currentRow();
    if (currRow >= 0 && currRow < m_releases.size()) {
        m_releases[currRow].trackCount = m_currentReleaseTracks.size();
        m_tableReleases->setItem(currRow, 3, new QTableWidgetItem(QString::number(m_currentReleaseTracks.size())));
    }

    autoMapTracks();

    QString coverUrl;
    QJsonArray imagesArr = root["images"].toArray();
    for (int i = 0; i < imagesArr.size(); ++i) {
        QJsonObject imgObj = imagesArr[i].toObject();
        QString imgType = imgObj["type"].toString();
        if (imgType == "primary") {
            coverUrl = imgObj["resource_url"].toString();
            break;
        }
    }
    if (coverUrl.isEmpty() && !imagesArr.isEmpty()) {
        coverUrl = imagesArr[0].toObject()["resource_url"].toString();
    }

    if (!coverUrl.isEmpty()) {
        QString token = m_editDiscogsToken->text().trimmed();
        QNetworkRequest imgReq(coverUrl);
        imgReq.setRawHeader("User-Agent", "Amifiles/" AMIFILES_VERSION_STRING " ( dave@example.com )");
        imgReq.setRawHeader("Authorization", QString("Discogs token=%1").arg(token).toUtf8());

        QNetworkReply* imgReply = m_networkManager->get(imgReq);
        connect(imgReply, &QNetworkReply::finished, this, [this, imgReply]() { onDownloadArtworkFinished(imgReply); });
    } else {
        m_lblArtworkPreview->setText("No Artwork");
    }
}
