#include "videoscraperdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
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
#include <QNetworkRequest>
#include <QSettings>
#include <QTextDocument>
#include <QGroupBox>
#include <QRegularExpression>

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
        QString name = info.completeBaseName();
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
    
    m_comboType = new QComboBox(this);
    m_comboType->addItems({"Movie", "TV Show"});
    
    m_btnSearch = new QPushButton("Search", this);
    connect(m_btnSearch, &QPushButton::clicked, this, &VideoScraperDialog::onSearchClicked);

    searchBar->addWidget(m_editSearch, 1);
    searchBar->addWidget(m_comboType);
    searchBar->addWidget(m_btnSearch);
    leftLayout->addLayout(searchBar);

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

    m_chkSaveNfo = new QCheckBox("Save movie/show info metadata (.nfo XML file)", this);
    m_chkSaveNfo->setChecked(true);
    
    m_chkSavePoster = new QCheckBox("Download and save poster artwork (as poster.jpg & folder.jpg)", this);
    m_chkSavePoster->setChecked(true);

    m_chkRename = new QCheckBox("Rename file/folder to 'Title (Year)'", this);
    m_chkRename->setChecked(true);

    optLay->addWidget(m_chkSaveNfo);
    optLay->addWidget(m_chkSavePoster);
    optLay->addWidget(m_chkRename);
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

    QUrl url;
    if (type == "Movie") {
        if (m_apiKey.isEmpty()) {
            QMessageBox::warning(this, "API Key Required", "A TMDb API Key is required to search for movies. Please set it in Preferences -> Services.");
            m_btnSearch->setEnabled(true);
            m_btnSearch->setText("Search");
            return;
        }
        url = QUrl("https://api.themoviedb.org/3/search/movie");
        QUrlQuery q;
        q.addQueryItem("api_key", m_apiKey);
        q.addQueryItem("query", query);
        url.setQuery(q);
    } else {
        // TV Show: Fall back to TVmaze if TMDB key is empty, or use TMDB if available
        if (!m_apiKey.isEmpty()) {
            url = QUrl("https://api.themoviedb.org/3/search/tv");
            QUrlQuery q;
            q.addQueryItem("api_key", m_apiKey);
            q.addQueryItem("query", query);
            url.setQuery(q);
        } else {
            url = QUrl("https://api.tvmaze.com/search/shows");
            QUrlQuery q;
            q.addQueryItem("q", query);
            url.setQuery(q);
        }
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

    if (!m_apiKey.isEmpty()) {
        // Parse TMDb JSON response
        QJsonObject rootObj = doc.object();
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
            res.genres = ""; 
            m_results.append(res);
        }
    } else {
        // Parse TVmaze response (only for TV Shows)
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
            res.type = "TV Show";
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

    if (!res.posterUrl.isEmpty()) {
        fetchPoster(res.posterUrl);
    } else {
        m_lblPosterPreview->setText("No poster available");
    }

    m_btnApply->setEnabled(true);
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

    for (const QString& path : m_filePaths) {
        QFileInfo pathInfo(path);
        QString targetFolder = pathInfo.isDir() ? path : pathInfo.absolutePath();

        // 1. Save Poster Artwork
        if (m_chkSavePoster->isChecked() && !m_downloadedPosterData.isEmpty()) {
            QDir dir(targetFolder);
            // Save as both poster.jpg and folder.jpg to support DVD casing and CD casing checks
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
            // For DVD style we can copy as dvd.jpg too!
            QFile fileDvd(dir.filePath("dvd.jpg"));
            if (fileDvd.open(QIODevice::WriteOnly)) {
                fileDvd.write(m_downloadedPosterData);
                fileDvd.close();
            }
        }

        // 2. Save .nfo File
        if (m_chkSaveNfo->isChecked()) {
            writeNfoFile(path, res);
        }

        // 3. Rename File/Folder
        if (m_chkRename->isChecked()) {
            QString sanitizedTitle = res.title;
            // Remove invalid filename characters
            sanitizedTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
            QString newBaseName = QString("%1 (%2)").arg(sanitizedTitle).arg(res.year);
            renameTarget(path, newBaseName);
        }

        successCount++;
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

void VideoScraperDialog::renameTarget(const QString& path, const QString& newName) {
    QFileInfo info(path);
    QDir parentDir = info.absoluteDir();
    QString newPath;
    if (info.isDir()) {
        newPath = parentDir.filePath(newName);
    } else {
        newPath = parentDir.filePath(newName + "." + info.suffix());
    }

    if (path != newPath) {
        QFile::rename(path, newPath);
    }
}
