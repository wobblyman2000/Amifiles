#include "videoscraperdialog.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QPixmap>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QSettings>
#include <QTextDocument>
#include <QGroupBox>
#include <QRegularExpression>
#include <QEventLoop>
#include <QDateTime>
#include <QListWidget>
#include <algorithm>

VideoScraperDialog::VideoScraperDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Online Video Metadata Scraper");
    resize(900, 620);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; font-size: 13px; }"
                  "QLineEdit { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 6px 10px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QPushButton:disabled { color: #585b70; background-color: #1e1e2e; }"
                  "QTableWidget { background-color: #181825; gridline-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; selection-background-color: #89b4fa; selection-color: #11111b; }"
                  "QHeaderView::section { background-color: #1e1e2e; color: #89b4fa; font-weight: bold; border: 1px solid #313244; padding: 4px; }"
                  "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px 8px; }"
                  "QTextBrowser { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 8px; }"
                  "QCheckBox { color: #cdd6f4; spacing: 8px; }");

    m_networkManager = new QNetworkAccessManager(this);

    setupUI();
    loadApiKey();

    // Prefill search query
    if (!m_filePaths.isEmpty()) {
        QString first = m_filePaths.first();
        QFileInfo info(first);
        QString name;
        QFileInfo parentDir(info.absolutePath());
        QRegularExpression seasonFolderRegex(R"((?i)\bseason\s*(\d+)\b|\bs\s*(\d+)\b)");

        if (info.isDir()) {
            if (seasonFolderRegex.match(info.fileName()).hasMatch()) {
                name = parentDir.fileName();
            } else {
                name = info.fileName();
            }
        } else {
            if (seasonFolderRegex.match(parentDir.fileName()).hasMatch()) {
                name = QFileInfo(parentDir.absolutePath()).fileName();
            } else {
                name = info.completeBaseName();
            }
        }

        // Remove year in brackets like (1984) or [1984]
        name.remove(QRegularExpression(R"(\(\d{4}\))"));
        name.remove(QRegularExpression(R"(\[\d{4}\])"));
        // Clean common media tags like S01E01, 1080p, x264, web-dl, etc.
        name.remove(QRegularExpression(R"((?i)\b(s\d+e\d+|1080p|720p|2160p|4k|x264|x265|h264|h265|bluray|brrip|web-dl|webrip|dvdrip|aac|dd5\.1|dts)\b)"));
        // Replace dots, underscores, dashes with spaces
        name = name.replace(QRegularExpression(R"([\._\-])"), " ").simplified();
        m_editSearch->setText(name);

        // Auto-select type
        if (first.contains("Season", Qt::CaseInsensitive) || first.contains("TV", Qt::CaseInsensitive)) {
            m_comboType->setCurrentIndex(1); // TV Show
        }
    }
}

void VideoScraperDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Left Layout: Search & List
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    QHBoxLayout* searchBar = new QHBoxLayout();
    m_editSearch = new QLineEdit(this);
    m_editSearch->setPlaceholderText("Search movies or TV shows...");
    
    m_btnSearch = new QPushButton("Search", this);
    connect(m_btnSearch, &QPushButton::clicked, this, &VideoScraperDialog::onSearchClicked);

    searchBar->addWidget(m_editSearch, 1);
    searchBar->addWidget(m_btnSearch);
    leftLayout->addLayout(searchBar);

    QHBoxLayout* sourceBar = new QHBoxLayout();
    sourceBar->addWidget(new QLabel("Category:", this));
    m_comboType = new QComboBox(this);
    m_comboType->addItems({"Movie", "TV Show"});
    sourceBar->addWidget(m_comboType);

    sourceBar->addWidget(new QLabel("Scraper Source:", this));
    m_comboProvider = new QComboBox(this);
    m_comboProvider->addItems({"✨ Auto (Best Available)", "🍿 OMDb (Free / Movies)", "🎬 TMDb (The Movie Database)", "📺 TVmaze (Free / TV Shows)", "🌐 Wikipedia (Free)"});
    sourceBar->addWidget(m_comboProvider, 1);

    leftLayout->addLayout(sourceBar);

    m_tableResults = new QTableWidget(this);
    m_tableResults->setColumnCount(3);
    m_tableResults->setHorizontalHeaderLabels({"Title", "Year", "Type"});
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableResults->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_tableResults, &QTableWidget::itemSelectionChanged, this, &VideoScraperDialog::onTableSelectionChanged);
    leftLayout->addWidget(m_tableResults);

    // Bottom settings group inside left layout
    QGroupBox* optionsGroup = new QGroupBox("Scraping Options", this);
    optionsGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #45475a; border-radius: 6px; margin-top: 10px; color: #89b4fa; padding-top: 10px; }");
    QVBoxLayout* optLay = new QVBoxLayout(optionsGroup);
    optLay->setSpacing(8);

    m_chkCreateMovieFolder = new QCheckBox("📁 Move file & metadata into self-contained folder 'Title (Year)'", this);
    m_chkCreateMovieFolder->setChecked(true);

    m_chkSaveNfo = new QCheckBox("Save movie/show info metadata (.nfo XML file)", this);
    m_chkSaveNfo->setChecked(true);
    
    m_chkSavePoster = new QCheckBox("Download and save poster artwork (as poster.jpg & folder.jpg)", this);
    m_chkSavePoster->setChecked(true);

    m_chkRename = new QCheckBox("Rename file/folder to 'Title (Year)'", this);
    m_chkRename->setChecked(true);

    m_chkFanart = new QCheckBox("Download Fanart.tv backdrop, logo & banner", this);
    m_chkFanart->setChecked(true);

    optLay->addWidget(m_chkCreateMovieFolder);
    optLay->addWidget(m_chkSaveNfo);
    optLay->addWidget(m_chkSavePoster);
    optLay->addWidget(m_chkRename);
    optLay->addWidget(m_chkFanart);
    leftLayout->addWidget(optionsGroup);

    mainLayout->addLayout(leftLayout, 3);

    // Right Layout: Details Preview
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    m_lblPosterPreview = new QLabel("Poster Preview", this);
    m_lblPosterPreview->setFixedSize(200, 300);
    m_lblPosterPreview->setAlignment(Qt::AlignCenter);
    m_lblPosterPreview->setStyleSheet("border: 1px solid #45475a; border-radius: 6px; background-color: #181825; color: #a6adc8;");
    rightLayout->addWidget(m_lblPosterPreview, 0, Qt::AlignHCenter);

    m_galleryList = new QListWidget(this);
    m_galleryList->setFlow(QListView::LeftToRight);
    m_galleryList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_galleryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_galleryList->setIconSize(QSize(48, 72));
    m_galleryList->setFixedHeight(92);
    m_galleryList->setStyleSheet(
        "QListWidget { background-color: #11111b; border: 1px solid #313244; border-radius: 6px; padding: 2px; }"
        "QListWidget::item { background-color: #181825; border: 1px solid #313244; border-radius: 4px; margin-right: 4px; }"
        "QListWidget::item:selected { border: 2px solid #cba6f7; }"
    );
    rightLayout->addWidget(m_galleryList);

    connect(m_galleryList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_alternativePosterUrls.size()) return;
        m_selectedPosterUrl = m_alternativePosterUrls[row];
        fetchPoster(m_selectedPosterUrl);
    });

    m_lblTitle = new QLabel("<b>Title:</b> Select a show", this);
    m_lblYear = new QLabel("<b>Year:</b> ", this);
    m_lblRating = new QLabel("<b>Rating:</b> ", this);
    m_lblGenres = new QLabel("<b>Genres:</b> ", this);
    
    rightLayout->addWidget(m_lblTitle);
    rightLayout->addWidget(m_lblYear);
    rightLayout->addWidget(m_lblRating);
    rightLayout->addWidget(m_lblGenres);

    QLabel* lblPlot = new QLabel("<b>Overview / Plot:</b>", this);
    rightLayout->addWidget(lblPlot);

    m_txtOverview = new QTextBrowser(this);
    rightLayout->addWidget(m_txtOverview, 1);

    QHBoxLayout* actionButtons = new QHBoxLayout();
    m_btnApply = new QPushButton("Apply", this);
    m_btnApply->setEnabled(false);
    m_btnApply->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnApply, &QPushButton::clicked, this, &VideoScraperDialog::onApplyClicked);

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    actionButtons->addWidget(m_btnApply);
    actionButtons->addWidget(m_btnCancel);
    rightLayout->addLayout(actionButtons);

    mainLayout->addLayout(rightLayout, 2);
}

void VideoScraperDialog::loadApiKey() {
    QSettings settings("Amifiles", "Amifiles");
    m_apiKey = settings.value("services/tmdb_api_key", "").toString();
}

void VideoScraperDialog::onSearchClicked() {
    QString query = m_editSearch->text().trimmed();
    // Clean year in brackets like (1984) or [1984] from the query
    query.remove(QRegularExpression(R"(\(\d{4}\))"));
    query.remove(QRegularExpression(R"(\[\d{4}\])"));
    query = query.simplified();
    if (query.isEmpty()) return;
    
    QString type = m_comboType->currentText();
    triggerSearch(query, type);
}

void VideoScraperDialog::triggerSearch(const QString& query, const QString& type) {
    if (m_currentSearchReply) {
        m_currentSearchReply->abort();
        m_currentSearchReply->deleteLater();
        m_currentSearchReply = nullptr;
    }

    m_btnSearch->setEnabled(false);
    m_btnSearch->setText("Searching...");

    QString provider = m_comboProvider ? m_comboProvider->currentText() : "Auto";
    QUrl url;

    if (provider.contains("TMDb")) {
        if (m_apiKey.isEmpty()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "TMDb API Key Required",
                "A TMDb API Key is required to use TMDb directly.\n\n"
                "Would you like to switch to OMDb (Free / No Key Required) now?",
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                if (m_comboProvider) m_comboProvider->setCurrentIndex(1); // OMDb
                provider = "OMDb";
            } else {
                m_btnSearch->setEnabled(true);
                m_btnSearch->setText("Search");
                return;
            }
        }
    }

    if (provider.contains("TMDb") || (provider.contains("Auto") && !m_apiKey.isEmpty())) {
        url = QUrl(type == "Movie" ? "https://api.themoviedb.org/3/search/movie" : "https://api.themoviedb.org/3/search/tv");
        QUrlQuery q;
        q.addQueryItem("api_key", m_apiKey);
        q.addQueryItem("query", query);
        url.setQuery(q);
    } else if (provider.contains("OMDb") || (provider.contains("Auto") && type == "Movie")) {
        url = QUrl("https://www.omdbapi.com/");
        QUrlQuery q;
        q.addQueryItem("apikey", "trilogy");
        q.addQueryItem("s", query);
        q.addQueryItem("type", type == "Movie" ? "movie" : "series");
        url.setQuery(q);
    } else if (provider.contains("Wikipedia")) {
        url = QUrl("https://en.wikipedia.org/w/api.php");
        QUrlQuery q;
        q.addQueryItem("action", "query");
        q.addQueryItem("list", "search");
        q.addQueryItem("srsearch", query + (type == "Movie" ? " film" : " television series"));
        q.addQueryItem("format", "json");
        url.setQuery(q);
    } else {
        // TVmaze (Free)
        url = QUrl("https://api.tvmaze.com/search/shows");
        QUrlQuery q;
        q.addQueryItem("q", query);
        url.setQuery(q);
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
    m_currentSearchReply = m_networkManager->get(req);
    connect(m_currentSearchReply, &QNetworkReply::finished, this, &VideoScraperDialog::onSearchFinished);
}

void VideoScraperDialog::onSearchFinished() {
    m_btnSearch->setEnabled(true);
    m_btnSearch->setText("Search");

    if (!m_currentSearchReply) return;
    if (m_currentSearchReply->error() != QNetworkReply::NoError) {
        if (m_currentSearchReply->error() != QNetworkReply::OperationCanceledError) {
            QMessageBox::critical(this, "Search Error", "Network request failed: " + m_currentSearchReply->errorString());
        }
        m_currentSearchReply->deleteLater();
        m_currentSearchReply = nullptr;
        return;
    }

    QByteArray data = m_currentSearchReply->readAll();
    m_currentSearchReply->deleteLater();
    m_currentSearchReply = nullptr;

    m_results.clear();
    m_tableResults->setRowCount(0);

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return;

    QString type = m_comboType->currentText();
    QJsonObject rootObj = doc.object();

    if (rootObj.contains("results")) {
        // TMDb response
        QJsonArray resultsArr = rootObj["results"].toArray();
        for (const QJsonValue& val : resultsArr) {
            QJsonObject obj = val.toObject();
            VideoSearchResult res;
            res.id = QString::number(obj["id"].toInt());
            
            if (type == "Movie") {
                res.title = obj["title"].toString();
                res.year = obj["release_date"].toString().left(4);
                res.type = "Movie";
            } else {
                res.title = obj["name"].toString();
                res.year = obj["first_air_date"].toString().left(4);
                res.type = "TV Show";
            }

            res.rating = QString::number(obj["vote_average"].toDouble(), 'f', 1);
            res.overview = obj["overview"].toString();
            
            QString posterPath = obj["poster_path"].toString();
            if (!posterPath.isEmpty()) {
                res.posterUrl = "https://image.tmdb.org/t/p/w500" + posterPath;
            }
            QString backdropPath = obj["backdrop_path"].toString();
            if (!backdropPath.isEmpty()) {
                res.backdropUrl = "https://image.tmdb.org/t/p/original" + backdropPath;
            }
            res.genres = ""; 
            m_results.append(res);
        }
    } else if (rootObj.contains("Search")) {
        // OMDb response
        QJsonArray resultsArr = rootObj["Search"].toArray();
        for (const QJsonValue& val : resultsArr) {
            QJsonObject obj = val.toObject();
            VideoSearchResult res;
            res.id = obj["imdbID"].toString();
            res.title = obj["Title"].toString();
            res.year = obj["Year"].toString().left(4);
            res.type = type;
            res.rating = "N/A";
            res.overview = QString("%1 (%2)").arg(res.title).arg(res.year);
            res.posterUrl = obj["Poster"].toString();
            if (res.posterUrl == "N/A") res.posterUrl.clear();
            m_results.append(res);
        }
    } else if (rootObj.contains("query")) {
        // Wikipedia search response
        QJsonArray resultsArr = rootObj["query"].toObject()["search"].toArray();
        for (const QJsonValue& val : resultsArr) {
            QJsonObject obj = val.toObject();
            VideoSearchResult res;
            res.id = QString::number(obj["pageid"].toInt());
            res.title = obj["title"].toString();
            res.year = "";
            res.type = type;
            res.rating = "N/A";
            
            QTextDocument docText;
            docText.setHtml(obj["snippet"].toString());
            res.overview = docText.toPlainText();
            res.posterUrl = "";
            m_results.append(res);
        }
    } else if (doc.isArray()) {
        // TVmaze response
        QJsonArray rootArr = doc.array();
        for (const QJsonValue& val : rootArr) {
            QJsonObject showObj = val.toObject()["show"].toObject();
            VideoSearchResult res;
            res.id = QString::number(showObj["id"].toInt());
            res.title = showObj["name"].toString();
            res.year = showObj["premiered"].toString().left(4);
            res.rating = QString::number(showObj["rating"].toObject()["average"].toDouble(), 'f', 1);
            
            QJsonArray genresArr = showObj["genres"].toArray();
            QStringList genres;
            for (const QJsonValue& g : genresArr) genres.append(g.toString());
            res.genres = genres.join(", ");
            
            res.overview = showObj["summary"].toString();
            QTextDocument docText;
            docText.setHtml(res.overview);
            res.overview = docText.toPlainText();

            res.posterUrl = showObj["image"].toObject()["original"].toString();
            if (res.posterUrl.isEmpty()) {
                res.posterUrl = showObj["image"].toObject()["medium"].toString();
            }
            res.type = type;
            res.studio = showObj["network"].toObject()["name"].toString();
            if (res.studio.isEmpty()) {
                res.studio = showObj["webChannel"].toObject()["name"].toString();
            }
            m_results.append(res);
        }
    }

    // Populate Results Table
    m_tableResults->setRowCount(m_results.size());
    for (int i = 0; i < m_results.size(); ++i) {
        const auto& res = m_results[i];
        m_tableResults->setItem(i, 0, new QTableWidgetItem(res.title));
        m_tableResults->setItem(i, 1, new QTableWidgetItem(res.year));
        m_tableResults->setItem(i, 2, new QTableWidgetItem(res.type));
    }

    if (m_results.size() > 0) {
        m_tableResults->selectRow(0);
    }
}

void VideoScraperDialog::onTableSelectionChanged() {
    int row = m_tableResults->currentRow();
    if (row < 0 || row >= m_results.size()) return;

    const auto& res = m_results[row];
    m_lblTitle->setText("<b>Title:</b> " + res.title);
    m_lblYear->setText("<b>Year:</b> " + res.year);
    m_lblRating->setText("<b>Rating:</b> " + res.rating);
    m_lblGenres->setText("<b>Genres:</b> " + (res.genres.isEmpty() ? "N/A" : res.genres));
    m_txtOverview->setPlainText(res.overview);

    m_downloadedPosterData.clear();
    m_lblPosterPreview->setText("Loading poster...");

    m_alternativePosterUrls.clear();
    m_selectedPosterUrl = res.posterUrl;
    m_galleryList->clear();

    if (!res.posterUrl.isEmpty()) {
        fetchPoster(res.posterUrl);
    } else {
        m_lblPosterPreview->setText("No poster available");
    }

    fetchAlternativePosters(res.id, m_comboType->currentText() == "TV Shows");

    m_btnApply->setEnabled(true);
}

void VideoScraperDialog::fetchAlternativePosters(const QString& id, bool isTV) {
    if (id.isEmpty()) return;

    QString typeName = isTV ? "tv" : "movie";
    QUrl url;
    if (!m_apiKey.isEmpty()) {
        url = QUrl(QString("https://api.themoviedb.org/3/%1/%2/images").arg(typeName).arg(id));
        QUrlQuery q;
        q.addQueryItem("api_key", m_apiKey);
        url.setQuery(q);
    } else {
        if (isTV) {
            url = QUrl(QString("https://api.tvmaze.com/shows/%1/images").arg(id));
        } else {
            return;
        }
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
    QNetworkReply* reply = m_networkManager->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, isTV]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return;

        QStringList posterUrls;
        if (!m_apiKey.isEmpty()) {
            QJsonObject obj = doc.object();
            QJsonArray postersArr = obj["posters"].toArray();
            for (int i = 0; i < postersArr.size() && i < 12; ++i) {
                QString path = postersArr[i].toObject()["file_path"].toString();
                if (!path.isEmpty()) {
                    posterUrls.append("https://image.tmdb.org/t/p/w185" + path);
                }
            }
        } else {
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (int i = 0; i < arr.size() && i < 12; ++i) {
                    QJsonObject obj = arr[i].toObject();
                    QString mediumUrl = obj["resolutions"].toObject()["medium"].toObject()["url"].toString();
                    if (!mediumUrl.isEmpty()) {
                        posterUrls.append(mediumUrl);
                    }
                }
            }
        }

        m_alternativePosterUrls = posterUrls;
        
        for (int i = 0; i < m_alternativePosterUrls.size(); ++i) {
            QString url = m_alternativePosterUrls[i];
            QListWidgetItem* item = new QListWidgetItem(m_galleryList);
            item->setSizeHint(QSize(54, 76));
            m_galleryList->addItem(item);

            QNetworkReply* imgReply = m_networkManager->get(QNetworkRequest(url));
            connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, item]() {
                imgReply->deleteLater();
                if (imgReply->error() == QNetworkReply::NoError) {
                    QPixmap pix;
                    if (pix.loadFromData(imgReply->readAll())) {
                        item->setIcon(QIcon(pix));
                    }
                }
            });
        }
    });
}

void VideoScraperDialog::fetchPoster(const QString& url) {
    if (m_currentPosterReply) {
        m_currentPosterReply->abort();
        m_currentPosterReply->deleteLater();
        m_currentPosterReply = nullptr;
    }

    QNetworkRequest req(url);
    m_currentPosterReply = m_networkManager->get(req);
    connect(m_currentPosterReply, &QNetworkReply::finished, this, &VideoScraperDialog::onPosterFinished);
}

void VideoScraperDialog::onPosterFinished() {
    if (!m_currentPosterReply) return;
    if (m_currentPosterReply->error() != QNetworkReply::NoError) {
        m_lblPosterPreview->setText("Failed to load poster");
        m_currentPosterReply->deleteLater();
        m_currentPosterReply = nullptr;
        return;
    }

    m_downloadedPosterData = m_currentPosterReply->readAll();
    m_currentPosterReply->deleteLater();
    m_currentPosterReply = nullptr;

    QPixmap pixmap;
    if (pixmap.loadFromData(m_downloadedPosterData)) {
        m_lblPosterPreview->setPixmap(pixmap.scaled(m_lblPosterPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_lblPosterPreview->setText("Invalid poster data");
    }
}

void VideoScraperDialog::onApplyClicked() {
    int row = m_tableResults->currentRow();
    if (row < 0 || row >= m_results.size()) return;

    const auto& res = m_results[row];
    int successCount = 0;

    QStringList resolvedPaths;
    for (const QString& path : m_filePaths) {
        QFileInfo pathInfo(path);
        QString currentPath = path;
        QString targetFolder = pathInfo.isDir() ? currentPath : pathInfo.absolutePath();

        // Check if self-contained movie folder should be created
        if (!pathInfo.isDir() && m_chkCreateMovieFolder->isChecked()) {
            QString sanitizedTitle = res.title;
            sanitizedTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
            QString folderName = QString("%1 (%2)").arg(sanitizedTitle).arg(res.year);
            QString movieSubfolder = QDir(pathInfo.absolutePath()).filePath(folderName);
            QDir().mkpath(movieSubfolder);

            QString newVideoPath = QDir(movieSubfolder).filePath(folderName + "." + pathInfo.suffix());
            if (QFile::rename(currentPath, newVideoPath)) {
                currentPath = newVideoPath;
                pathInfo = QFileInfo(currentPath);
            } else if (QFile::copy(currentPath, newVideoPath)) {
                QFile::remove(currentPath);
                currentPath = newVideoPath;
                pathInfo = QFileInfo(currentPath);
            }
            targetFolder = movieSubfolder;
        }

        // 1. Save Poster Artwork
        if (m_chkSavePoster->isChecked() && !m_downloadedPosterData.isEmpty()) {
            QDir dir(targetFolder);
            QFile filePoster(dir.filePath("poster.jpg"));
            if (filePoster.open(QIODevice::WriteOnly)) {
                filePoster.write(m_downloadedPosterData);
                filePoster.close();
            }
            QFile fileFolder(dir.filePath("folder.jpg"));
            if (fileFolder.open(QIODevice::WriteOnly)) {
                fileFolder.write(m_downloadedPosterData);
                fileFolder.close();
            }
            QFile fileDvd(dir.filePath("dvd.jpg"));
            if (fileDvd.open(QIODevice::WriteOnly)) {
                fileDvd.write(m_downloadedPosterData);
                fileDvd.close();
            }
        }

        // 1b. Save TMDb Backdrop/Background
        if (m_chkFanart->isChecked() && !res.backdropUrl.isEmpty()) {
            QDir dir(targetFolder);
            QUrl bdUrl(res.backdropUrl);
            QNetworkRequest bdReq(bdUrl);
            QNetworkReply* bdReply = m_networkManager->get(bdReq);
            QEventLoop bdLoop;
            connect(bdReply, &QNetworkReply::finished, &bdLoop, &QEventLoop::quit);
            bdLoop.exec();
            
            if (bdReply->error() == QNetworkReply::NoError) {
                QByteArray bdData = bdReply->readAll();
                if (!pathInfo.isDir()) {
                    QString baseName = pathInfo.completeBaseName();
                    if (m_chkRename->isChecked()) {
                        QString sanitizedTitle = res.title;
                        sanitizedTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
                        baseName = QString("%1 (%2)").arg(sanitizedTitle).arg(res.year);
                    }
                    QFile fileBd(dir.filePath(baseName + "_fanart.jpg"));
                    if (fileBd.open(QIODevice::WriteOnly)) {
                        fileBd.write(bdData);
                        fileBd.close();
                    }
                } else {
                    QFile fileBd(dir.filePath("fanart.jpg"));
                    if (fileBd.open(QIODevice::WriteOnly)) {
                        fileBd.write(bdData);
                        fileBd.close();
                    }
                }
            }
            bdReply->deleteLater();
        }

        // 2. Save .nfo File
        if (m_chkSaveNfo->isChecked()) {
            writeNfoFile(currentPath, res);
        }

        // 3. Rename File/Folder
        if (m_chkRename->isChecked()) {
            if (res.type != "TV Show" || pathInfo.isDir()) {
                QString sanitizedTitle = res.title;
                // Remove invalid filename characters
                sanitizedTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
                QString newBaseName = QString("%1 (%2)").arg(sanitizedTitle).arg(res.year);
                currentPath = renameTarget(currentPath, newBaseName);
            }
        }

        resolvedPaths.append(currentPath);
        successCount++;
    }

    // If it's a TV Show, process episodes (rename them and save episode .nfo files)
    if (res.type == "TV Show") {
        QList<EpisodeInfo> episodes = fetchEpisodesList(res.id);
        if (!episodes.isEmpty()) {
            for (const QString& path : resolvedPaths) {
                QFileInfo pathInfo(path);
                QString targetFolder = pathInfo.isDir() ? path : pathInfo.absolutePath();
                processTVShowEpisodes(targetFolder, res.title, episodes);
            }
        }
    }

    // If it's a TV Show, download season posters recursively if checked
    if (res.type == "TV Show" && m_chkSavePoster->isChecked()) {
        QUrl seasonsUrl;
        if (!m_apiKey.isEmpty()) {
            seasonsUrl = QUrl(QString("https://api.themoviedb.org/3/tv/%1").arg(res.id));
            QUrlQuery q;
            q.addQueryItem("api_key", m_apiKey);
            seasonsUrl.setQuery(q);
        } else {
            seasonsUrl = QUrl(QString("https://api.tvmaze.com/shows/%1/seasons").arg(res.id));
        }

        QNetworkRequest req(seasonsUrl);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
        QNetworkReply* reply = m_networkManager->get(req);
        
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray sData = reply->readAll();
            QJsonDocument sDoc = QJsonDocument::fromJson(sData);
            if (!sDoc.isNull()) {
                struct SeasonPoster {
                    int number;
                    QString url;
                };
                QList<SeasonPoster> seasons;

                if (!m_apiKey.isEmpty()) {
                    QJsonObject obj = sDoc.object();
                    QJsonArray seasonsArr = obj["seasons"].toArray();
                    for (const QJsonValue& val : seasonsArr) {
                        QJsonObject sobj = val.toObject();
                        int sNum = sobj["season_number"].toInt();
                        QString pPath = sobj["poster_path"].toString();
                        if (!pPath.isEmpty()) {
                            seasons.append({sNum, "https://image.tmdb.org/t/p/w500" + pPath});
                        }
                    }
                } else {
                    QJsonArray arr = sDoc.array();
                    for (const QJsonValue& val : arr) {
                        QJsonObject sobj = val.toObject();
                        int sNum = sobj["number"].toInt();
                        QString pUrl = sobj["image"].toObject()["original"].toString();
                        if (pUrl.isEmpty()) {
                            pUrl = sobj["image"].toObject()["medium"].toString();
                        }
                        if (!pUrl.isEmpty()) {
                            seasons.append({sNum, pUrl});
                        }
                    }
                }

                // Download season posters and write them
                for (const QString& path : resolvedPaths) {
                    QFileInfo pathInfo(path);
                    QString targetFolder = pathInfo.isDir() ? path : pathInfo.absolutePath();
                    QDir parentDir(targetFolder);

                    QStringList subdirs = parentDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

                    for (const auto& sPoster : seasons) {
                        if (sPoster.number == 0) continue; // Skip special season 0 folder downloads by default unless explicitly matched

                        QUrl pUrl(sPoster.url);
                        QNetworkRequest pReq(pUrl);
                        QNetworkReply* pReply = m_networkManager->get(pReq);
                        QEventLoop pLoop;
                        connect(pReply, &QNetworkReply::finished, &pLoop, &QEventLoop::quit);
                        pLoop.exec();

                        if (pReply->error() == QNetworkReply::NoError) {
                            QByteArray posterData = pReply->readAll();

                            // Save inside the corresponding season subdirectory if it exists
                            for (const QString& subdir : subdirs) {
                                QRegularExpression re(QString(R"((?i)\bseason\s*0*%1\b|\bs0*%1\b)").arg(sPoster.number));
                                if (re.match(subdir).hasMatch()) {
                                    QString subdirPath = parentDir.filePath(subdir);
                                    QDir sDir(subdirPath);
                                    
                                    QFile fPoster(sDir.filePath("poster.jpg"));
                                    if (fPoster.open(QIODevice::WriteOnly)) {
                                        fPoster.write(posterData);
                                        fPoster.close();
                                    }
                                    QFile fFolder(sDir.filePath("folder.jpg"));
                                    if (fFolder.open(QIODevice::WriteOnly)) {
                                        fFolder.write(posterData);
                                        fFolder.close();
                                    }
                                }
                            }

                            // Always save in TV Show root folder as seasonXX.jpg (e.g. season01.jpg)
                            QString rootPosterName = QString("season%1.jpg").arg(sPoster.number, 2, 10, QChar('0'));
                            QFile fRoot(parentDir.filePath(rootPosterName));
                            if (fRoot.open(QIODevice::WriteOnly)) {
                                fRoot.write(posterData);
                                fRoot.close();
                            }
                        }
                        pReply->deleteLater();
                    }
                }
            }
        }
        reply->deleteLater();
    }

    // 4. Download additional artwork from Fanart.tv if checked
    if (m_chkFanart->isChecked()) {
        QString fanartApiKey = "c8fb0e4e7c7e5ebf8b63cd944f24efb4";
        QString idToQuery = res.id;
        
        // If it's a TV show, we need the TVDB ID!
        if (res.type == "TV Show") {
            QString tvdbId;
            if (!m_apiKey.isEmpty()) {
                // Fetch external IDs from TMDB to get the TVDB ID
                QUrl extUrl(QString("https://api.themoviedb.org/3/tv/%1/external_ids").arg(res.id));
                QUrlQuery q;
                q.addQueryItem("api_key", m_apiKey);
                extUrl.setQuery(q);
                
                QNetworkRequest req(extUrl);
                req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
                QNetworkReply* reply = m_networkManager->get(req);
                QEventLoop loop;
                connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
                
                if (reply->error() == QNetworkReply::NoError) {
                    QJsonObject extObj = QJsonDocument::fromJson(reply->readAll()).object();
                    if (extObj.contains("tvdb_id") && !extObj["tvdb_id"].isNull()) {
                        tvdbId = QString::number(extObj["tvdb_id"].toInt());
                    }
                }
                reply->deleteLater();
            } else {
                // Fetch details from TVMaze to get the TVDB ID
                QUrl extUrl(QString("https://api.tvmaze.com/shows/%1").arg(res.id));
                QNetworkRequest req(extUrl);
                req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
                QNetworkReply* reply = m_networkManager->get(req);
                QEventLoop loop;
                connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
                
                if (reply->error() == QNetworkReply::NoError) {
                    QJsonObject detailsObj = QJsonDocument::fromJson(reply->readAll()).object();
                    QJsonObject externals = detailsObj["externals"].toObject();
                    if (externals.contains("thetvdb") && !externals["thetvdb"].isNull()) {
                        tvdbId = QString::number(externals["thetvdb"].toInt());
                    }
                }
                reply->deleteLater();
            }
            
            idToQuery = tvdbId;
        }
        
        if (!idToQuery.isEmpty() && idToQuery != "0") {
            QUrl fanartUrl;
            if (res.type == "TV Show") {
                fanartUrl = QUrl(QString("https://webservice.fanart.tv/v3/tv/%1").arg(idToQuery));
            } else {
                fanartUrl = QUrl(QString("https://webservice.fanart.tv/v3/movies/%1").arg(idToQuery));
            }
            
            QUrlQuery q;
            q.addQueryItem("api_key", fanartApiKey);
            fanartUrl.setQuery(q);
            
            QNetworkRequest req(fanartUrl);
            req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
            QNetworkReply* reply = m_networkManager->get(req);
            QEventLoop loop;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
                
                QString backgroundUrl;
                QString logoUrl;
                QString bannerUrl;
                
                if (res.type == "TV Show") {
                    QJsonArray backgrounds = root["showbackground"].toArray();
                    if (!backgrounds.isEmpty()) backgroundUrl = backgrounds[0].toObject()["url"].toString();
                    
                    QJsonArray logos = root["clearlogo"].toArray();
                    if (logos.isEmpty()) logos = root["hdtvlogo"].toArray();
                    if (!logos.isEmpty()) logoUrl = logos[0].toObject()["url"].toString();
                    
                    QJsonArray banners = root["tvbanner"].toArray();
                    if (!banners.isEmpty()) bannerUrl = banners[0].toObject()["url"].toString();
                } else {
                    QJsonArray backgrounds = root["moviebackground"].toArray();
                    if (!backgrounds.isEmpty()) backgroundUrl = backgrounds[0].toObject()["url"].toString();
                    
                    QJsonArray logos = root["hdmovielogo"].toArray();
                    if (logos.isEmpty()) logos = root["movielogo"].toArray();
                    if (!logos.isEmpty()) logoUrl = logos[0].toObject()["url"].toString();
                    
                    QJsonArray banners = root["moviebanner"].toArray();
                    if (!banners.isEmpty()) bannerUrl = banners[0].toObject()["url"].toString();
                }
                
                auto downloadAndSave = [this](const QString& url, const QString& destPath) {
                    if (url.isEmpty()) return;
                    QNetworkRequest r((QUrl(url)));
                    r.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
                    QNetworkReply* rep = m_networkManager->get(r);
                    QEventLoop l;
                    connect(rep, &QNetworkReply::finished, &l, &QEventLoop::quit);
                    l.exec();
                    if (rep->error() == QNetworkReply::NoError) {
                        QFile f(destPath);
                        if (f.open(QIODevice::WriteOnly)) {
                            f.write(rep->readAll());
                            f.close();
                        }
                    }
                    rep->deleteLater();
                };
                
                for (const QString& path : resolvedPaths) {
                    QFileInfo pathInfo(path);
                    QString targetFolder = pathInfo.isDir() ? path : pathInfo.absolutePath();
                    QDir dir(targetFolder);
                    
                    if (!pathInfo.isDir()) {
                        QString baseName = pathInfo.completeBaseName();
                        downloadAndSave(backgroundUrl, dir.filePath(baseName + "_fanart.jpg"));
                        downloadAndSave(logoUrl, dir.filePath(baseName + "_logo.png"));
                        downloadAndSave(bannerUrl, dir.filePath(baseName + "_banner.jpg"));
                    } else {
                        downloadAndSave(backgroundUrl, dir.filePath("fanart.jpg"));
                        downloadAndSave(logoUrl, dir.filePath("logo.png"));
                        downloadAndSave(bannerUrl, dir.filePath("banner.jpg"));
                    }
                }
            }
            reply->deleteLater();
        }
    }

    QMessageBox::information(this, "Metadata Saved", QString("Successfully applied video metadata to %1 items.").arg(successCount));
    accept();
}

void VideoScraperDialog::writeNfoFile(const QString& path, const VideoSearchResult& res) {
    QFileInfo pathInfo(path);
    QString nfoPath;
    QString tag = (res.type == "Movie") ? "movie" : "tvshow";

    if (pathInfo.isDir()) {
        nfoPath = QDir(path).filePath(tag + ".nfo");
    } else {
        nfoPath = QDir(pathInfo.absolutePath()).filePath(pathInfo.completeBaseName() + ".nfo");
    }

    QFile file(nfoPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n";
        out << "<" << tag << ">\n";
        out << "    <title>" << res.title.toHtmlEscaped() << "</title>\n";
        out << "    <year>" << res.year.toHtmlEscaped() << "</year>\n";
        out << "    <rating>" << res.rating.toHtmlEscaped() << "</rating>\n";
        out << "    <plot>" << res.overview.toHtmlEscaped() << "</plot>\n";
        if (!res.genres.isEmpty()) {
            out << "    <genre>" << res.genres.toHtmlEscaped() << "</genre>\n";
        }
        if (!res.studio.isEmpty()) {
            out << "    <studio>" << res.studio.toHtmlEscaped() << "</studio>\n";
        }
        out << "</" << tag << ">\n";
        file.close();
    }
}

QString VideoScraperDialog::renameTarget(const QString& path, const QString& newName) {
    QFileInfo info(path);
    QDir parentDir = info.absoluteDir();
    QString newPath;
    if (info.isDir()) {
        newPath = parentDir.filePath(newName);
    } else {
        newPath = parentDir.filePath(newName + "." + info.suffix());
    }

    if (path != newPath) {
        bool destExists = QFile::exists(newPath);
        bool sameFile = false;
        if (destExists) {
            QFileInfo fiNew(newPath);
            if (info.canonicalFilePath() == fiNew.canonicalFilePath() && !info.canonicalFilePath().isEmpty()) {
                sameFile = true;
            }
        }

        if (sameFile) {
            QString tempPath = newPath + ".tmp_rename";
            if (QFile::rename(path, tempPath)) {
                if (QFile::rename(tempPath, newPath)) {
                    QFile(newPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                    return newPath;
                } else {
                    QFile::rename(tempPath, path);
                }
            }
        } else if (!destExists) {
            if (QFile::rename(path, newPath)) {
                QFile(newPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                return newPath;
            } else {
                qWarning() << "Failed to rename folder/file:" << path << "to" << newPath << "Error:" << QFile(path).errorString();
            }
        } else {
            // Destination already exists (different file), silently bypass it
            qDebug() << "Bypassing renameTarget since destination already exists:" << newPath;
        }
    }
    return path;
}

VideoScraperDialog::SeasonEpisode VideoScraperDialog::parseSeasonEpisode(const QFileInfo& fileInfo) {
    SeasonEpisode se;
    QString fileName = fileInfo.fileName();

    // 1. Try S01E02 pattern first
    QRegularExpression reSxE(R"([Ss](\d+)\s*[Ee](\d+))");
    QRegularExpressionMatch matchSxE = reSxE.match(fileName);
    if (matchSxE.hasMatch()) {
        se.season = matchSxE.captured(1).toInt();
        se.episode = matchSxE.captured(2).toInt();
        return se;
    }
    
    // 2. Try 1x02 pattern
    QRegularExpression reCross(R"(\b(\d+)[Xx](\d+)\b)");
    QRegularExpressionMatch matchCross = reCross.match(fileName);
    if (matchCross.hasMatch()) {
        se.season = matchCross.captured(1).toInt();
        se.episode = matchCross.captured(2).toInt();
        return se;
    }

    // 3. Try S01.E02 pattern
    QRegularExpression reDotSxE(R"(\b[Ss](\d+)\.[Ee](\d+)\b)");
    QRegularExpressionMatch matchDotSxE = reDotSxE.match(fileName);
    if (matchDotSxE.hasMatch()) {
        se.season = matchDotSxE.captured(1).toInt();
        se.episode = matchDotSxE.captured(2).toInt();
        return se;
    }

    // 4. Try Season 1 Episode 2 pattern
    QRegularExpression reText(R"((?i)\bseason\s*(\d+)\s*episode\s*(\d+)\b)");
    QRegularExpressionMatch matchText = reText.match(fileName);
    if (matchText.hasMatch()) {
        se.season = matchText.captured(1).toInt();
        se.episode = matchText.captured(2).toInt();
        return se;
    }

    // 5. Check if inside a Season folder (search up to 3 levels)
    int folderSeason = -1;
    QRegularExpression seasonFolderRegex(R"((?i)\bseason\s*(\d+)\b|\bs\s*(\d+)\b)");
    QDir d = fileInfo.absoluteDir();
    for (int depth = 0; depth < 3; ++depth) {
        QRegularExpressionMatch folderMatch = seasonFolderRegex.match(d.dirName());
        if (folderMatch.hasMatch()) {
            folderSeason = !folderMatch.captured(1).isEmpty() ? folderMatch.captured(1).toInt() : folderMatch.captured(2).toInt();
            break;
        }
        if (d.isRoot()) break;
        d.cdUp();
    }

    if (folderSeason != -1) {
        se.season = folderSeason;
        // Search for episode number
        QRegularExpression reEp(R"((?i)\b(?:ep|episode|e)?\s*(\d+)\b)");
        QRegularExpressionMatchIterator it = reEp.globalMatch(fileName);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            int val = m.captured(1).toInt();
            if (val > 0 && val < 100) {
                se.episode = val;
                return se;
            }
        }
    }

    // 6. Default to Season 1 if we find a clear episode indicator like "Episode 01"
    QRegularExpression reEpOnly(R"((?i)\b(?:episode|ep)\s*(\d+)\b)");
    QRegularExpressionMatch matchEp = reEpOnly.match(fileName);
    if (matchEp.hasMatch()) {
        se.season = 1;
        se.episode = matchEp.captured(1).toInt();
        return se;
    }

    return se;
}

QList<EpisodeInfo> VideoScraperDialog::fetchEpisodesList(const QString& showId) {
    // 1. Try reading from cache first
    QString cachePath = "/home/dave/.gemini/antigravity/scraper_cache.json";
    QFile cacheFile(cachePath);
    QJsonObject cacheObj;
    if (cacheFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(cacheFile.readAll());
        if (!doc.isNull()) {
            cacheObj = doc.object();
        }
        cacheFile.close();
    }

    if (cacheObj.contains(showId)) {
        QJsonObject showEntry = cacheObj[showId].toObject();
        qint64 timestamp = showEntry["timestamp"].toVariant().toLongLong();
        qint64 currentSecs = QDateTime::currentSecsSinceEpoch();
        if (currentSecs - timestamp < 86400) { // 24 hours
            QJsonArray epsArr = showEntry["episodes"].toArray();
            QList<EpisodeInfo> cachedList;
            for (const QJsonValue& val : epsArr) {
                QJsonObject epObj = val.toObject();
                EpisodeInfo ep;
                ep.season = epObj["season"].toInt();
                ep.episode = epObj["episode"].toInt();
                ep.title = epObj["title"].toString();
                ep.overview = epObj["overview"].toString();
                cachedList.append(ep);
            }
            return cachedList;
        }
    }

    QList<EpisodeInfo> list;
    if (!m_apiKey.isEmpty()) {
        // Fetch TMDB show details to get seasons list
        QUrl showUrl(QString("https://api.themoviedb.org/3/tv/%1").arg(showId));
        QUrlQuery q;
        q.addQueryItem("api_key", m_apiKey);
        showUrl.setQuery(q);

        QNetworkRequest req(showUrl);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
        QNetworkReply* reply = m_networkManager->get(req);
        
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                QJsonArray seasonsArr = obj["seasons"].toArray();
                for (const QJsonValue& val : seasonsArr) {
                    QJsonObject sobj = val.toObject();
                    int sNum = sobj["season_number"].toInt();
                    
                    // Fetch details for each season
                    QUrl seasonUrl(QString("https://api.themoviedb.org/3/tv/%1/season/%2").arg(showId).arg(sNum));
                    QUrlQuery sq;
                    sq.addQueryItem("api_key", m_apiKey);
                    seasonUrl.setQuery(sq);

                    QNetworkRequest sReq(seasonUrl);
                    sReq.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
                    QNetworkReply* sReply = m_networkManager->get(sReq);
                    
                    QEventLoop sLoop;
                    connect(sReply, &QNetworkReply::finished, &sLoop, &QEventLoop::quit);
                    sLoop.exec();

                    if (sReply->error() == QNetworkReply::NoError) {
                        QByteArray sData = sReply->readAll();
                        QJsonDocument sDoc = QJsonDocument::fromJson(sData);
                        if (!sDoc.isNull()) {
                            QJsonObject sObj = sDoc.object();
                            QJsonArray epArr = sObj["episodes"].toArray();
                            for (const QJsonValue& epVal : epArr) {
                                QJsonObject epObj = epVal.toObject();
                                EpisodeInfo ep;
                                ep.season = sNum;
                                ep.episode = epObj["episode_number"].toInt();
                                ep.title = epObj["name"].toString();
                                ep.overview = epObj["overview"].toString();
                                list.append(ep);
                            }
                        }
                    }
                    sReply->deleteLater();
                }
            }
        }
        reply->deleteLater();
    } else {
        // Fetch TVmaze episodes list in one API call
        QUrl epUrl(QString("https://api.tvmaze.com/shows/%1/episodes").arg(showId));
        QNetworkRequest req(epUrl);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Amifiles Video Scraper");
        QNetworkReply* reply = m_networkManager->get(req);
        
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const QJsonValue& val : arr) {
                    QJsonObject obj = val.toObject();
                    EpisodeInfo ep;
                    ep.season = obj["season"].toInt();
                    ep.episode = obj["number"].toInt();
                    ep.title = obj["name"].toString();
                    
                    QString summary = obj["summary"].toString();
                    QTextDocument docText;
                    docText.setHtml(summary);
                    ep.overview = docText.toPlainText();
                    
                    list.append(ep);
                }
            }
        }
        reply->deleteLater();
    }

    // 2. Save fetched list to cache
    if (!list.isEmpty()) {
        QJsonObject showEntry;
        showEntry["timestamp"] = QDateTime::currentSecsSinceEpoch();
        QJsonArray epsArr;
        for (const EpisodeInfo& ep : list) {
            QJsonObject epObj;
            epObj["season"] = ep.season;
            epObj["episode"] = ep.episode;
            epObj["title"] = ep.title;
            epObj["overview"] = ep.overview;
            epsArr.append(epObj);
        }
        showEntry["episodes"] = epsArr;

        QJsonObject fullCache;
        QFile cacheFileRead(cachePath);
        if (cacheFileRead.open(QIODevice::ReadOnly)) {
            fullCache = QJsonDocument::fromJson(cacheFileRead.readAll()).object();
            cacheFileRead.close();
        }
        fullCache[showId] = showEntry;

        QFile cacheFileWrite(cachePath);
        if (cacheFileWrite.open(QIODevice::WriteOnly)) {
            cacheFileWrite.write(QJsonDocument(fullCache).toJson());
            cacheFileWrite.close();
        }
    }

    return list;
}

void VideoScraperDialog::processTVShowEpisodes(const QString& targetFolder, const QString& showTitle, const QList<EpisodeInfo>& episodes) {
    QDir dir(targetFolder);
    
    QString sanitizedShowTitle = showTitle;
    sanitizedShowTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
    
    // Scan recursively for video files to match and prepare tasks
    QDirIterator it(targetFolder, { "*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.flv", "*.wmv", "*.m4v", "*.mpg", "*.mpeg" }, QDir::Files, QDirIterator::Subdirectories);
    QList<FileRenameTask> tasks;
    struct LocalFile {
        QString filePath;
        QFileInfo fileInfo;
        int detectedSeason = -1;
        int detectedEpisode = -1;
        bool matched = false;
    };

    QList<LocalFile> localFiles;
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        SeasonEpisode se = parseSeasonEpisode(fileInfo);

        LocalFile lf;
        lf.filePath = filePath;
        lf.fileInfo = fileInfo;
        lf.detectedSeason = se.season;
        lf.detectedEpisode = se.episode;
        lf.matched = false;

        localFiles.append(lf);
    }

    // Pass 1: Explicit matching (SxxExx etc.)
    for (LocalFile& lf : localFiles) {
        if (lf.detectedSeason != -1 && lf.detectedEpisode != -1) {
            for (const EpisodeInfo& ep : episodes) {
                if (ep.season == lf.detectedSeason && ep.episode == lf.detectedEpisode) {
                    lf.matched = true;

                    QString sStr = QString("S%1").arg(ep.season, 2, 10, QChar('0'));
                    QString eStr = QString("E%1").arg(ep.episode, 2, 10, QChar('0'));
                    QString sanitizedEpTitle = ep.title;
                    sanitizedEpTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));

                    QString newFileName = QString("%1 - %2%3 - %4.%5")
                        .arg(sanitizedShowTitle)
                        .arg(sStr)
                        .arg(eStr)
                        .arg(sanitizedEpTitle)
                        .arg(lf.fileInfo.suffix());

                    FileRenameTask task;
                    task.oldPath = lf.filePath;
                    task.fileName = newFileName;
                    task.showTitle = showTitle;
                    task.ep = ep;
                    task.newPath = dir.filePath(newFileName);
                    tasks.append(task);
                    break;
                }
            }
        }
    }

    // Pass 2: Sequential matching fallback for remaining files grouped by season
    QMap<int, QList<LocalFile*>> unmatchedBySeason;
    for (LocalFile& lf : localFiles) {
        if (!lf.matched && lf.detectedSeason != -1) {
            unmatchedBySeason[lf.detectedSeason].append(&lf);
        }
    }

    auto naturalCompare = [](const QString& s1, const QString& s2) -> bool {
        int i = 0, j = 0;
        while (i < s1.length() && j < s2.length()) {
            if (s1[i].isDigit() && s2[j].isDigit()) {
                QString n1, n2;
                while (i < s1.length() && s1[i].isDigit()) {
                    n1 += s1[i++];
                }
                while (j < s2.length() && s2[j].isDigit()) {
                    n2 += s2[j++];
                }
                int val1 = n1.toInt();
                int val2 = n2.toInt();
                if (val1 != val2) {
                    return val1 < val2;
                }
            } else {
                if (s1[i].toLower() != s2[j].toLower()) {
                    return s1[i].toLower() < s2[j].toLower();
                }
                i++;
                j++;
            }
        }
        return s1.length() < s2.length();
    };

    QMapIterator<int, QList<LocalFile*>> seasonIt(unmatchedBySeason);
    while (seasonIt.hasNext()) {
        seasonIt.next();
        int seasonNum = seasonIt.key();
        QList<LocalFile*> files = seasonIt.value();

        // Sort files naturally by filename
        std::sort(files.begin(), files.end(), [&](LocalFile* a, LocalFile* b) {
            return naturalCompare(a->fileInfo.fileName(), b->fileInfo.fileName());
        });

        // Find unmatched online episodes for this season
        QList<EpisodeInfo> unmatchedEpisodes;
        for (const EpisodeInfo& ep : episodes) {
            if (ep.season == seasonNum) {
                bool alreadyMatched = false;
                for (const FileRenameTask& t : tasks) {
                    if (t.ep.season == ep.season && t.ep.episode == ep.episode) {
                        alreadyMatched = true;
                        break;
                    }
                }
                if (!alreadyMatched) {
                    unmatchedEpisodes.append(ep);
                }
            }
        }

        std::sort(unmatchedEpisodes.begin(), unmatchedEpisodes.end(), [](const EpisodeInfo& a, const EpisodeInfo& b) {
            return a.episode < b.episode;
        });

        int count = std::min(files.size(), unmatchedEpisodes.size());
        for (int i = 0; i < count; ++i) {
            LocalFile* lf = files[i];
            const EpisodeInfo& ep = unmatchedEpisodes[i];
            lf->matched = true;

            QString sStr = QString("S%1").arg(ep.season, 2, 10, QChar('0'));
            QString eStr = QString("E%1").arg(ep.episode, 2, 10, QChar('0'));
            QString sanitizedEpTitle = ep.title;
            sanitizedEpTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));

            QString newFileName = QString("%1 - %2%3 - %4.%5")
                .arg(sanitizedShowTitle)
                .arg(sStr)
                .arg(eStr)
                .arg(sanitizedEpTitle)
                .arg(lf->fileInfo.suffix());

            FileRenameTask task;
            task.oldPath = lf->filePath;
            task.fileName = newFileName;
            task.showTitle = showTitle;
            task.ep = ep;
            task.newPath = dir.filePath(newFileName);
            tasks.append(task);
        }
    }

    // Sort tasks chronologically by Season, then Episode
    std::sort(tasks.begin(), tasks.end(), [](const FileRenameTask& a, const FileRenameTask& b) {
        if (a.ep.season != b.ep.season) {
            return a.ep.season < b.ep.season;
        }
        return a.ep.episode < b.ep.episode;
    });

    if (tasks.isEmpty()) {
        if (m_chkRename->isChecked()) {
            QMessageBox::information(this, "No Matches", "No matching video files containing season/episode patterns (e.g. S01E02) were found.");
        }
        return;
    }

    if (m_chkRename->isChecked()) {
        // Open Rename Preview Dialog
        RenamePreviewDialog previewDlg(tasks, targetFolder, episodes, this);
        if (previewDlg.exec() == QDialog::Accepted) {
            QList<FileRenameTask> confirmed = previewDlg.selectedTasks();
            bool useSeasonFolders = previewDlg.organizeIntoSeasonFolders();

            // 1. Rename existing Season subdirectories if they match but are named differently
            if (useSeasonFolders) {
                QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for (const QString& subdir : subdirs) {
                    QRegularExpression re(R"((?i)\bseason\s*(\d+)\b|\bs\s*(\d+)\b)");
                    QRegularExpressionMatch m = re.match(subdir);
                    if (m.hasMatch()) {
                        int sNum = !m.captured(1).isEmpty() ? m.captured(1).toInt() : m.captured(2).toInt();
                        QString correctName = QString("Season %1").arg(sNum, 2, 10, QChar('0'));
                        if (subdir != correctName) {
                            QString oldPath = dir.filePath(subdir);
                            QString newPath = dir.filePath(correctName);
                            if (!QFile::exists(newPath)) {
                                QDir().rename(oldPath, newPath);
                            }
                        }
                    }
                }
            }

            int renamedCount = 0;
            QStringList failedRenames;
            for (const auto& task : confirmed) {
                QString finalDestPath;
                if (useSeasonFolders) {
                    // Ensure Season Folder exists
                    QString sDirName = QString("Season %1").arg(task.ep.season, 2, 10, QChar('0'));
                    QString sDirPath = dir.filePath(sDirName);
                    QDir().mkpath(sDirPath);
                    finalDestPath = QDir(sDirPath).filePath(task.fileName);
                } else {
                    finalDestPath = dir.filePath(task.fileName);
                }

                if (task.oldPath != finalDestPath) {
                    bool destExists = QFile::exists(finalDestPath);
                    bool sameFile = false;
                    if (destExists) {
                        QFileInfo fiOld(task.oldPath);
                        QFileInfo fiNew(finalDestPath);
                        if (fiOld.canonicalFilePath() == fiNew.canonicalFilePath() && !fiOld.canonicalFilePath().isEmpty()) {
                            sameFile = true;
                        }
                    }

                    if (sameFile) {
                        // It is the same file (case-only rename on case-insensitive filesystems)
                        QString tempPath = finalDestPath + ".tmp_rename";
                        if (QFile::rename(task.oldPath, tempPath)) {
                            if (QFile::rename(tempPath, finalDestPath)) {
                                QFile(finalDestPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                                renamedCount++;
                            } else {
                                QFile::rename(tempPath, task.oldPath);
                                failedRenames.append(QString("%1 -> %2 (Error: Case-rename failed at second step)")
                                    .arg(QFileInfo(task.oldPath).fileName())
                                    .arg(QFileInfo(finalDestPath).fileName()));
                            }
                        } else {
                            failedRenames.append(QString("%1 -> %2 (Error: Case-rename failed at first step)")
                                .arg(QFileInfo(task.oldPath).fileName())
                                .arg(QFileInfo(finalDestPath).fileName()));
                        }
                    } else if (!destExists) {
                        if (QFile::rename(task.oldPath, finalDestPath)) {
                            QFile(finalDestPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                            renamedCount++;
                        } else {
                            failedRenames.append(QString("%1 -> %2 (Error: %3)")
                                .arg(QFileInfo(task.oldPath).fileName())
                                .arg(QFileInfo(finalDestPath).fileName())
                                .arg(QFile(task.oldPath).errorString()));
                        }
                    } else {
                        // Destination already exists (different file), silently bypass renaming
                        qDebug() << "Bypassing episode rename since destination already exists:" << finalDestPath;
                    }
                }

                if (m_chkSaveNfo->isChecked()) {
                    writeEpisodeNfoFile(finalDestPath, task.ep, showTitle);
                }
            }

            if (renamedCount > 0) {
                QMessageBox::information(this, "Renaming Complete", QString("Successfully renamed and organized %1 episodes.").arg(renamedCount));
            }
            if (!failedRenames.isEmpty()) {
                QMessageBox::warning(this, "Rename Failures",
                    "The following files could not be renamed:\n\n" + failedRenames.join("\n"));
            }
        }
    } else {
        // If rename is unchecked, just write NFO files to existing episode file paths directly!
        int nfoCount = 0;
        for (const auto& task : tasks) {
            if (m_chkSaveNfo->isChecked()) {
                writeEpisodeNfoFile(task.oldPath, task.ep, showTitle);
                nfoCount++;
            }
        }
        if (nfoCount > 0) {
            QMessageBox::information(this, "NFO Export Complete", QString("Successfully created NFO files for %1 episodes.").arg(nfoCount));
        }
    }
}

// ==================== RenamePreviewDialog ====================

RenamePreviewDialog::RenamePreviewDialog(const QList<FileRenameTask>& tasks, const QString& targetFolder, const QList<EpisodeInfo>& allEpisodes, QWidget* parent)
    : QDialog(parent), m_tasks(tasks), m_targetFolder(targetFolder), m_allEpisodes(allEpisodes) {
    setWindowTitle("Rename Preview & Organization");
    resize(850, 480);
    setStyleSheet(Theme::getStylesheet());
    setupUI();
}

void RenamePreviewDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QLabel* infoLabel = new QLabel("The following files match identified episodes. Confirm the renames below:", this);
    infoLabel->setStyleSheet("font-weight: bold; color: #cdd6f4;");
    mainLayout->addWidget(infoLabel);

    // Option: season folders
    m_chkSeasonFolders = new QCheckBox("Organize episodes into Season folders (e.g. 'Season 01')", this);
    m_chkSeasonFolders->setChecked(true);
    m_chkSeasonFolders->setStyleSheet("color: #cdd6f4;");
    connect(m_chkSeasonFolders, &QCheckBox::toggled, this, &RenamePreviewDialog::onSeasonFoldersToggled);
    mainLayout->addWidget(m_chkSeasonFolders);

    // Results table (4 columns)
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"Apply?", "Original File Path", "Matched Episode", "Proposed New Path"});
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setStyleSheet("QTableWidget { background-color: #11111b; border: 1px solid #313244; border-radius: 6px; }");
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    mainLayout->addWidget(m_table, 1);

    // Bottom controls
    QHBoxLayout* bottomBar = new QHBoxLayout();
    QPushButton* btnSelectAll = new QPushButton("Select All", this);
    QPushButton* btnDeselectAll = new QPushButton("Deselect All", this);
    bottomBar->addWidget(btnSelectAll);
    bottomBar->addWidget(btnDeselectAll);
    bottomBar->addStretch();

    QPushButton* btnConfirm = new QPushButton("Confirm & Rename", this);
    btnConfirm->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }");
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    bottomBar->addWidget(btnConfirm);
    bottomBar->addWidget(btnCancel);
    mainLayout->addLayout(bottomBar);

    connect(btnSelectAll, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_table->rowCount(); ++i) {
            if (auto* item = m_table->item(i, 0)) {
                item->setCheckState(Qt::Checked);
            }
        }
    });

    connect(btnDeselectAll, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_table->rowCount(); ++i) {
            if (auto* item = m_table->item(i, 0)) {
                item->setCheckState(Qt::Unchecked);
            }
        }
    });

    connect(btnConfirm, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    updateTable();
}

void RenamePreviewDialog::updateTable() {
    m_table->setRowCount(0);
    QDir dir(m_targetFolder);

    for (int i = 0; i < m_tasks.size(); ++i) {
        const auto& task = m_tasks[i];
        m_table->insertRow(i);

        // Column 0: Checkbox
        QTableWidgetItem* checkItem = new QTableWidgetItem();
        checkItem->setFlags(checkItem->flags() | Qt::ItemIsUserCheckable);
        checkItem->setCheckState(Qt::Checked);
        m_table->setItem(i, 0, checkItem);

        // Column 1: Original Path (relative)
        QString origRel = dir.relativeFilePath(task.oldPath);
        QTableWidgetItem* origItem = new QTableWidgetItem(origRel);
        m_table->setItem(i, 1, origItem);

        // Column 2: Matched Episode (ComboBox Override)
        QComboBox* combo = new QComboBox(m_table);
        combo->setStyleSheet("QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 2px 4px; }");
        int activeIdx = -1;
        for (int j = 0; j < m_allEpisodes.size(); ++j) {
            const EpisodeInfo& ep = m_allEpisodes[j];
            QString text = QString("S%1E%2 - %3")
                .arg(ep.season, 2, 10, QChar('0'))
                .arg(ep.episode, 2, 10, QChar('0'))
                .arg(ep.title);
            combo->addItem(text, j);
            if (ep.season == task.ep.season && ep.episode == task.ep.episode) {
                activeIdx = j;
            }
        }
        combo->blockSignals(true);
        if (activeIdx != -1) {
            combo->setCurrentIndex(activeIdx);
        }
        combo->blockSignals(false);
        m_table->setCellWidget(i, 2, combo);

        // Column 3: Proposed Path
        QString proposedRel;
        if (m_chkSeasonFolders->isChecked()) {
            QString sDir = QString("Season %1").arg(task.ep.season, 2, 10, QChar('0'));
            proposedRel = sDir + "/" + task.fileName;
        } else {
            proposedRel = task.fileName;
        }
        QTableWidgetItem* destItem = new QTableWidgetItem(proposedRel);
        m_table->setItem(i, 3, destItem);

        // Connect combo box changes
        connect(combo, &QComboBox::currentIndexChanged, this, [this, i, combo]() {
            int epIdx = combo->currentIndex();
            if (epIdx < 0 || epIdx >= m_allEpisodes.size()) return;

            const EpisodeInfo& ep = m_allEpisodes[epIdx];
            m_tasks[i].ep = ep;

            // Re-generate proposed name
            QString sanitizedShowTitle = m_tasks[i].showTitle;
            sanitizedShowTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
            QString sStr = QString("S%1").arg(ep.season, 2, 10, QChar('0'));
            QString eStr = QString("E%1").arg(ep.episode, 2, 10, QChar('0'));
            QString sanitizedEpTitle = ep.title;
            sanitizedEpTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));

            QFileInfo fi(m_tasks[i].oldPath);
            m_tasks[i].fileName = QString("%1 - %2%3 - %4.%5")
                .arg(sanitizedShowTitle)
                .arg(sStr)
                .arg(eStr)
                .arg(sanitizedEpTitle)
                .arg(fi.suffix());

            m_tasks[i].newPath = QDir(m_targetFolder).filePath(m_tasks[i].fileName);

            // Update proposed new path column
            QString proposedRelNew;
            if (m_chkSeasonFolders->isChecked()) {
                QString sDir = QString("Season %1").arg(ep.season, 2, 10, QChar('0'));
                proposedRelNew = sDir + "/" + m_tasks[i].fileName;
            } else {
                proposedRelNew = m_tasks[i].fileName;
            }
            if (auto* destIt = m_table->item(i, 3)) {
                destIt->setText(proposedRelNew);
            }
        });
    }
}

void RenamePreviewDialog::onSeasonFoldersToggled(bool /*checked*/) {
    updateTable();
}

QList<FileRenameTask> RenamePreviewDialog::selectedTasks() const {
    QList<FileRenameTask> selected;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (m_table->item(i, 0)->checkState() == Qt::Checked) {
            selected.append(m_tasks[i]);
        }
    }
    return selected;
}

bool RenamePreviewDialog::organizeIntoSeasonFolders() const {
    return m_chkSeasonFolders->isChecked();
}

void VideoScraperDialog::writeEpisodeNfoFile(const QString& filePath, const EpisodeInfo& ep, const QString& showTitle) {
    QFileInfo info(filePath);
    QString nfoPath = info.absoluteDir().filePath(info.completeBaseName() + ".nfo");

    QFile file(nfoPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n";
        out << "<episodedetails>\n";
        out << "    <title>" << ep.title.toHtmlEscaped() << "</title>\n";
        out << "    <showtitle>" << showTitle.toHtmlEscaped() << "</showtitle>\n";
        out << "    <season>" << ep.season << "</season>\n";
        out << "    <episode>" << ep.episode << "</episode>\n";
        out << "    <plot>" << ep.overview.toHtmlEscaped() << "</plot>\n";
        out << "</episodedetails>\n";
        file.close();
    }
}
