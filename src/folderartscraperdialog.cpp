#include "folderartscraperdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QPixmap>
#include <QMessageBox>
#include <QSettings>
#include <QGroupBox>

FolderArtScraperDialog::FolderArtScraperDialog(const QString& folderPath, QWidget* parent)
    : QDialog(parent), m_folderPath(folderPath) {
    setWindowTitle("Folder Cover Art & Wallpaper Scraper");
    resize(850, 520);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; font-size: 13px; }"
                  "QLineEdit { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 6px 10px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QPushButton:disabled { color: #585b70; background-color: #1e1e2e; }"
                  "QTableWidget { background-color: #181825; gridline-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; selection-background-color: #89b4fa; selection-color: #11111b; }"
                  "QHeaderView::section { background-color: #1e1e2e; color: #89b4fa; font-weight: bold; border: 1px solid #313244; padding: 4px; }"
                  "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px 8px; }"
                  "QCheckBox { color: #cdd6f4; spacing: 8px; }");

    m_networkManager = new QNetworkAccessManager(this);
    setupUI();
    loadApiKey();

    // Prefill search input
    QFileInfo fi(folderPath);
    m_editSearch->setText(fi.fileName());
}

void FolderArtScraperDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Left Layout: Search and results list
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    QHBoxLayout* searchBar = new QHBoxLayout();
    m_editSearch = new QLineEdit(this);
    m_editSearch->setPlaceholderText("Search terms (e.g. album or movie title)...");
    
    m_comboType = new QComboBox(this);
    m_comboType->addItems({"Music Album", "Movie / TV Show"});
    
    m_btnSearch = new QPushButton("Search", this);
    connect(m_btnSearch, &QPushButton::clicked, this, &FolderArtScraperDialog::onSearchClicked);

    searchBar->addWidget(m_editSearch, 1);
    searchBar->addWidget(m_comboType);
    searchBar->addWidget(m_btnSearch);
    leftLayout->addLayout(searchBar);

    m_tableResults = new QTableWidget(this);
    m_tableResults->setColumnCount(3);
    m_tableResults->setHorizontalHeaderLabels({"Title", "Artist / Year", "Type"});
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableResults->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_tableResults, &QTableWidget::itemSelectionChanged, this, &FolderArtScraperDialog::onTableSelectionChanged);
    leftLayout->addWidget(m_tableResults);

    QGroupBox* optionsGroup = new QGroupBox("Download Options", this);
    optionsGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #45475a; border-radius: 6px; margin-top: 10px; color: #89b4fa; padding-top: 10px; }");
    QVBoxLayout* optLay = new QVBoxLayout(optionsGroup);
    optLay->setSpacing(8);

    m_chkSaveAsCover = new QCheckBox("Save as cover.png / cover.jpg (Audio Front Cover)", this);
    m_chkSaveAsCover->setChecked(true);
    
    m_chkSaveAsFolder = new QCheckBox("Save as folder.jpg (Standard Folder Casing)", this);
    m_chkSaveAsFolder->setChecked(true);

    m_chkSaveAsBackdrop = new QCheckBox("Save as backdrop.jpg (Look & Feel Background)", this);
    m_chkSaveAsBackdrop->setChecked(false);

    optLay->addWidget(m_chkSaveAsCover);
    optLay->addWidget(m_chkSaveAsFolder);
    optLay->addWidget(m_chkSaveAsBackdrop);
    leftLayout->addWidget(optionsGroup);

    mainLayout->addLayout(leftLayout, 3);

    // Right Layout: Art Preview & Actions
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    m_lblArtworkPreview = new QLabel("Image Preview", this);
    m_lblArtworkPreview->setFixedSize(220, 220);
    m_lblArtworkPreview->setAlignment(Qt::AlignCenter);
    m_lblArtworkPreview->setStyleSheet("border: 1px solid #45475a; border-radius: 6px; background-color: #181825; color: #a6adc8;");
    rightLayout->addWidget(m_lblArtworkPreview, 0, Qt::AlignHCenter);

    m_lblStatus = new QLabel("Ready.", this);
    m_lblStatus->setWordWrap(true);
    rightLayout->addWidget(m_lblStatus);

    rightLayout->addStretch(1);

    QHBoxLayout* actionButtons = new QHBoxLayout();
    m_btnApply = new QPushButton("Apply", this);
    m_btnApply->setEnabled(false);
    m_btnApply->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnApply, &QPushButton::clicked, this, &FolderArtScraperDialog::onApplyClicked);

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    actionButtons->addWidget(m_btnApply);
    actionButtons->addWidget(m_btnCancel);
    rightLayout->addLayout(actionButtons);

    mainLayout->addLayout(rightLayout, 2);
}

void FolderArtScraperDialog::loadApiKey() {
    QSettings settings("Amifiles", "Amifiles");
    m_apiKey = settings.value("services/tmdb_api_key", "").toString();
}

void FolderArtScraperDialog::onSearchClicked() {
    QString query = m_editSearch->text().trimmed();
    if (query.isEmpty()) return;

    m_btnSearch->setEnabled(false);
    m_btnSearch->setText("Searching...");
    m_tableResults->setRowCount(0);
    m_results.clear();
    m_lblArtworkPreview->clear();
    m_lblArtworkPreview->setText("Image Preview");
    m_btnApply->setEnabled(false);
    m_downloadedArtwork.clear();

    if (m_comboType->currentIndex() == 0) {
        triggerMusicSearch(query);
    } else {
        triggerVideoSearch(query);
    }
}

void FolderArtScraperDialog::triggerMusicSearch(const QString& query) {
    QUrl url("https://musicbrainz.org/ws/2/release/");
    QUrlQuery q;
    q.addQueryItem("query", query);
    q.addQueryItem("fmt", "json");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Amifiles/1.0.0 ( dave@example.com )");
    QNetworkReply* reply = m_networkManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onMusicSearchFinished(reply); });
}

void FolderArtScraperDialog::onMusicSearchFinished(QNetworkReply* reply) {
    m_btnSearch->setEnabled(true);
    m_btnSearch->setText("Search");

    if (reply->error() != QNetworkReply::NoError) {
        m_lblStatus->setText("Search failed: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonArray releases = root["releases"].toArray();

    for (int i = 0; i < releases.size(); ++i) {
        QJsonObject rel = releases[i].toObject();
        ScrapeResult r;
        r.id = rel["id"].toString();
        r.title = rel["title"].toString();
        
        QJsonArray artists = rel["artist-credit"].toArray();
        if (!artists.isEmpty()) {
            r.artistOrYear = artists[0].toObject()["name"].toString();
        }
        r.type = "Music";
        m_results.append(r);

        int row = m_tableResults->rowCount();
        m_tableResults->insertRow(row);
        m_tableResults->setItem(row, 0, new QTableWidgetItem(r.title));
        m_tableResults->setItem(row, 1, new QTableWidgetItem(r.artistOrYear));
        m_tableResults->setItem(row, 2, new QTableWidgetItem("Album"));
    }

    m_lblStatus->setText(QString("Found %1 releases").arg(m_results.size()));
}

void FolderArtScraperDialog::triggerVideoSearch(const QString& query) {
    // Fall back to TVmaze if TMDb API key is empty
    QUrl url;
    if (m_apiKey.isEmpty()) {
        url = QUrl("https://api.tvmaze.com/search/shows");
        QUrlQuery q;
        q.addQueryItem("q", query);
        url.setQuery(q);
    } else {
        url = QUrl("https://api.themoviedb.org/3/search/multi");
        QUrlQuery q;
        q.addQueryItem("api_key", m_apiKey);
        q.addQueryItem("query", query);
        url.setQuery(q);
    }

    QNetworkRequest req(url);
    QNetworkReply* reply = m_networkManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onVideoSearchFinished(reply); });
}

void FolderArtScraperDialog::onVideoSearchFinished(QNetworkReply* reply) {
    m_btnSearch->setEnabled(true);
    m_btnSearch->setText("Search");

    if (reply->error() != QNetworkReply::NoError) {
        m_lblStatus->setText("Search failed: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (m_apiKey.isEmpty()) {
        // TVmaze parsing
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject item = arr[i].toObject();
            QJsonObject show = item["show"].toObject();
            QJsonObject imageObj = show["image"].toObject();

            ScrapeResult r;
            r.id = QString::number(show["id"].toInt());
            r.title = show["name"].toString();
            r.artistOrYear = show["premiered"].toString().left(4);
            r.type = "TV Show";
            r.backdropUrl = imageObj["original"].toString(); // Save image directly in backdropUrl

            m_results.append(r);

            int row = m_tableResults->rowCount();
            m_tableResults->insertRow(row);
            m_tableResults->setItem(row, 0, new QTableWidgetItem(r.title));
            m_tableResults->setItem(row, 1, new QTableWidgetItem(r.artistOrYear));
            m_tableResults->setItem(row, 2, new QTableWidgetItem("TV Show"));
        }
    } else {
        // TMDB parsing
        QJsonObject root = doc.object();
        QJsonArray results = root["results"].toArray();
        for (int i = 0; i < results.size(); ++i) {
            QJsonObject obj = results[i].toObject();
            QString mediaType = obj["media_type"].toString();
            if (mediaType != "movie" && mediaType != "tv") continue;

            ScrapeResult r;
            r.id = QString::number(obj["id"].toInt());
            r.title = mediaType == "movie" ? obj["title"].toString() : obj["name"].toString();
            r.artistOrYear = mediaType == "movie" ? obj["release_date"].toString().left(4) : obj["first_air_date"].toString().left(4);
            r.type = mediaType == "movie" ? "Movie" : "TV Show";

            QString posterPath = obj["poster_path"].toString();
            if (!posterPath.isEmpty()) {
                r.backdropUrl = "https://image.tmdb.org/t/p/w500" + posterPath;
            }

            m_results.append(r);

            int row = m_tableResults->rowCount();
            m_tableResults->insertRow(row);
            m_tableResults->setItem(row, 0, new QTableWidgetItem(r.title));
            m_tableResults->setItem(row, 1, new QTableWidgetItem(r.artistOrYear));
            m_tableResults->setItem(row, 2, new QTableWidgetItem(r.type));
        }
    }

    m_lblStatus->setText(QString("Found %1 video results").arg(m_results.size()));
}

void FolderArtScraperDialog::onTableSelectionChanged() {
    int row = m_tableResults->currentRow();
    if (row < 0 || row >= m_results.size()) return;

    m_lblStatus->setText("Loading artwork info...");
    m_lblArtworkPreview->setText("Loading...");
    m_btnApply->setEnabled(false);
    m_downloadedArtwork.clear();

    const ScrapeResult& r = m_results[row];

    if (r.type == "Music") {
        QUrl coverUrl(QString("https://coverartarchive.org/release/%1").arg(r.id));
        QNetworkRequest coverReq(coverUrl);
        coverReq.setRawHeader("User-Agent", "Amifiles/1.0.0 ( dave@example.com )");
        QNetworkReply* coverReply = m_networkManager->get(coverReq);
        connect(coverReply, &QNetworkReply::finished, this, [this, coverReply]() { onMusicCoverArtFinished(coverReply); });
    } else {
        if (!r.backdropUrl.isEmpty()) {
            QNetworkRequest imgReq(r.backdropUrl);
            QNetworkReply* imgReply = m_networkManager->get(imgReq);
            connect(imgReply, &QNetworkReply::finished, this, [this, imgReply]() { onDownloadArtworkFinished(imgReply); });
        } else {
            m_lblArtworkPreview->setText("No Artwork URL");
            m_lblStatus->setText("No poster path found for this item.");
        }
    }
}

void FolderArtScraperDialog::onMusicCoverArtFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_lblArtworkPreview->setText("No Cover Art");
        m_lblStatus->setText("Cover art metadata query failed.");
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
        QString imgUrl = thumbnails["250"].toString();
        if (imgUrl.isEmpty()) {
            imgUrl = imgObj["image"].toString();
        }

        if (!imgUrl.isEmpty()) {
            QNetworkRequest imgReq(imgUrl);
            imgReq.setRawHeader("User-Agent", "Amifiles/1.0.0 ( dave@example.com )");
            QNetworkReply* imgReply = m_networkManager->get(imgReq);
            connect(imgReply, &QNetworkReply::finished, this, [this, imgReply]() { onDownloadArtworkFinished(imgReply); });
        } else {
            m_lblArtworkPreview->setText("No Image URL");
        }
    } else {
        m_lblArtworkPreview->setText("No Cover Art");
    }
}

void FolderArtScraperDialog::onDownloadArtworkFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_lblArtworkPreview->setText("Download Failed");
        m_lblStatus->setText("Failed to download image: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    m_downloadedArtwork = reply->readAll();
    m_artworkMimeType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (m_artworkMimeType.isEmpty()) m_artworkMimeType = "image/jpeg";
    reply->deleteLater();

    QPixmap pix;
    if (pix.loadFromData(m_downloadedArtwork)) {
        m_lblArtworkPreview->setPixmap(pix.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_lblStatus->setText("Artwork downloaded. Click Apply to save to folder.");
        m_btnApply->setEnabled(true);
    } else {
        m_lblArtworkPreview->setText("Invalid Image");
        m_lblStatus->setText("Failed to parse downloaded image data.");
    }
}

void FolderArtScraperDialog::onApplyClicked() {
    if (m_downloadedArtwork.isEmpty()) return;

    QDir dir(m_folderPath);
    if (!dir.exists()) {
        QMessageBox::critical(this, "Save Failed", "Folder path does not exist!");
        return;
    }

    QString ext = m_artworkMimeType == "image/png" ? ".png" : ".jpg";

    bool savedAny = false;

    if (m_chkSaveAsCover->isChecked()) {
        QString path = dir.filePath("cover" + ext);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_downloadedArtwork);
            file.close();
            savedAny = true;
        }
    }

    if (m_chkSaveAsFolder->isChecked()) {
        QString path = dir.filePath("folder.jpg");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_downloadedArtwork);
            file.close();
            savedAny = true;
        }
    }

    if (m_chkSaveAsBackdrop->isChecked()) {
        QString path = dir.filePath("backdrop.jpg");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_downloadedArtwork);
            file.close();
            savedAny = true;
        }
    }

    if (savedAny) {
        QMessageBox::information(this, "Save Successful", "Folder artwork saved successfully!");
        accept();
    } else {
        QMessageBox::warning(this, "No Option Selected", "Please check at least one download target option.");
    }
}
