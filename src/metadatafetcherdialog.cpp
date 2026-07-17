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

    QPushButton* btnApply = new QPushButton("Apply Tags", this);
    btnApply->setStyleSheet("background-color: #a6e3a1; color: #11111b;");
    connect(btnApply, &QPushButton::clicked, this, &MetadataFetcherDialog::onApply);
    bottomLayout->addWidget(btnApply);

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

    m_lblStatus->setText("Searching MusicBrainz...");
    m_btnSearch->setEnabled(false);
    m_tableReleases->setRowCount(0);
    m_tableTracks->setRowCount(0);
    m_tableMapping->setRowCount(0);
    m_releases.clear();
    m_lblArtworkPreview->clear();
    m_lblArtworkPreview->setText("No Artwork");
    m_artworkData.clear();

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
            }
        }
    }

    if (m_matchedResults.isEmpty()) {
        QMessageBox::warning(this, "Apply Error", "No files have been mapped to online tracks. Map at least one track to apply.");
        return;
    }

    accept();
}
