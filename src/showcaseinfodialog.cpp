#include "showcaseinfodialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QTextStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollArea>
#include <QTextBrowser>

ShowcaseInfoDialog::ShowcaseInfoDialog(const QString& itemPath, QWidget* parent)
    : QDialog(parent), m_path(itemPath) {
    setWindowTitle("Media Info Sheet — Amifiles");
    resize(640, 480);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; font-size: 13px; }"
        "QLabel { color: #cdd6f4; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 7px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; color: #89b4fa; }"
        "QScrollArea { border: 1px solid #313244; border-radius: 6px; background-color: #181825; }"
    );

    setupUI();
    loadMetadata();
}

void ShowcaseInfoDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // Left column: High resolution artwork / poster
    QVBoxLayout* leftCol = new QVBoxLayout();
    leftCol->setSpacing(10);

    m_lblPoster = new QLabel(this);
    m_lblPoster->setFixedSize(220, 310);
    m_lblPoster->setStyleSheet("border: 1px solid #45475a; border-radius: 8px; background-color: #11111b;");
    m_lblPoster->setScaledContents(true);
    leftCol->addWidget(m_lblPoster);

    m_btnToggleWatch = new QPushButton("✔ Mark as Watched", this);
    connect(m_btnToggleWatch, &QPushButton::clicked, this, [this]() {
        m_isWatched = !m_isWatched;
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("theater/watch_status/" + m_path, m_isWatched);
        
        m_btnToggleWatch->setText(m_isWatched ? "✔ Watched" : "⭕ Mark as Watched");
        m_btnToggleWatch->setStyleSheet(m_isWatched ? "QPushButton { background-color: #a6e3a1; color: #11111b; border: none; font-weight: bold; padding: 7px 14px; border-radius: 6px; }"
                                                    : "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 7px 14px; font-weight: bold; }");
        emit watchStatusChanged(m_path, m_isWatched);
    });
    leftCol->addWidget(m_btnToggleWatch);
    leftCol->addStretch(1);

    mainLayout->addLayout(leftCol);

    // Right column: Detailed specs and plot synopsis
    QVBoxLayout* rightCol = new QVBoxLayout();
    rightCol->setSpacing(10);

    m_lblTitle = new QLabel("Media Title", this);
    m_lblTitle->setStyleSheet("font-size: 20px; font-weight: bold; color: #89b4fa;");
    m_lblTitle->setWordWrap(true);
    rightCol->addWidget(m_lblTitle);

    m_lblSpecs = new QLabel("Technical Specs", this);
    m_lblSpecs->setStyleSheet("font-size: 12px; color: #a6adc8; background-color: #181825; border: 1px solid #313244; border-radius: 6px; padding: 8px;");
    m_lblSpecs->setWordWrap(true);
    rightCol->addWidget(m_lblSpecs);

    QLabel* lblSynopsisHeader = new QLabel("SYNOPSIS & SUMMARY", this);
    lblSynopsisHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #f5c2e7; margin-top: 6px;");
    rightCol->addWidget(lblSynopsisHeader);

    m_lblSynopsis = new QTextBrowser(this);
    m_lblSynopsis->setReadOnly(true);
    m_lblSynopsis->setFrameStyle(QFrame::NoFrame);
    m_lblSynopsis->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #181825;"
        "  border: 1px solid #313244;"
        "  border-radius: 6px;"
        "  color: #cdd6f4;"
        "  font-size: 13px;"
        "  padding: 10px;"
        "}"
        "QScrollBar:vertical {"
        "  border: none;"
        "  background: #181825;"
        "  width: 8px;"
        "  margin: 0px 0px 0px 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #45475a;"
        "  min-height: 20px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #89b4fa;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );
    rightCol->addWidget(m_lblSynopsis, 1);

    // Action buttons bar at bottom right
    QHBoxLayout* btnBar = new QHBoxLayout();
    btnBar->setSpacing(10);

    m_btnPlay = new QPushButton("▶ Play Media", this);
    m_btnPlay->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border: none; } QPushButton:hover { background-color: #b4befe; }");
    connect(m_btnPlay, &QPushButton::clicked, this, [this]() {
        emit playRequested(m_path);
        accept();
    });

    m_btnOpenFolder = new QPushButton("📁 Open Folder", this);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, [this]() {
        emit openFolderRequested(m_path);
        accept();
    });

    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnBar->addWidget(m_btnPlay);
    btnBar->addWidget(m_btnOpenFolder);
    btnBar->addStretch(1);
    btnBar->addWidget(btnClose);

    rightCol->addLayout(btnBar);
    mainLayout->addLayout(rightCol, 1);
}

void ShowcaseInfoDialog::loadMetadata() {
    QFileInfo fi(m_path);
    m_lblTitle->setText(fi.fileName());

    QSettings settings("Amifiles", "Amifiles");
    m_isWatched = settings.value("theater/watch_status/" + m_path, false).toBool();
    m_btnToggleWatch->setText(m_isWatched ? "✔ Watched" : "⭕ Mark as Watched");
    if (m_isWatched) {
        m_btnToggleWatch->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; border: none; font-weight: bold; padding: 7px 14px; border-radius: 6px; }");
    }

    // 1. Artwork Scanner
    QString targetDir = fi.isDir() ? m_path : fi.absolutePath();
    QStringList artCandidates = { "poster.jpg", "poster.png", "cover.jpg", "cover.png", "folder.jpg", "folder.png", "fanart.jpg", "banner.jpg" };
    
    QPixmap posterPix;
    for (const QString& candidate : artCandidates) {
        QString fullP = QDir(targetDir).filePath(candidate);
        if (QFile::exists(fullP)) {
            posterPix.load(fullP);
            if (!posterPix.isNull()) break;
        }
    }

    if (posterPix.isNull() && !fi.isDir()) {
        // Check if item itself is an image
        QString ext = fi.suffix().toLower();
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp") {
            posterPix.load(m_path);
        }
    }

    if (!posterPix.isNull()) {
        m_lblPoster->setPixmap(posterPix);
    } else {
        m_lblPoster->setText("No Artwork\nAvailable");
        m_lblPoster->setAlignment(Qt::AlignCenter);
    }

    // 2. Technical Specs
    QString resTag = "Standard Definition";
    QString lowerName = m_path.toLower();
    if (lowerName.contains("2160p") || lowerName.contains("4k") || lowerName.contains("uhd")) {
        resTag = "4K UHD (2160p)";
    } else if (lowerName.contains("1080p") || lowerName.contains("fhd")) {
        resTag = "Full HD (1080p)";
    } else if (lowerName.contains("720p")) {
        resTag = "HD (720p)";
    }

    QString specsStr = QString("<b>Quality:</b> %1<br>"
                               "<b>Type:</b> %2<br>"
                               "<b>Size / Content:</b> %3<br>"
                               "<b>Modified:</b> %4")
        .arg(resTag)
        .arg(fi.isDir() ? "Directory Container" : (fi.suffix().isEmpty() ? "File" : fi.suffix().toUpper() + " Media File"))
        .arg(fi.isDir() ? QString("%1 items").arg(QDir(m_path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size()) : QString("%1 MB").arg(QString::number(fi.size() / (1024.0 * 1024.0), 'f', 2)))
        .arg(fi.lastModified().toString("yyyy-MM-dd hh:mm"));

    m_lblSpecs->setText(specsStr);

    // 3. Synopsis / NFO Scanner
    QStringList nfoCandidates = { "movie.nfo", "tvshow.nfo", "summary.txt", "description.txt", "info.txt" };
    QDir dir(targetDir);
    QFileInfoList allFiles = dir.entryInfoList(QDir::Files);
    
    QString synopsisText;
    for (const QFileInfo& nfoFi : allFiles) {
        if (nfoFi.suffix().toLower() == "nfo" || nfoCandidates.contains(nfoFi.fileName().toLower())) {
            QFile file(nfoFi.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                synopsisText = in.readAll().trimmed();
                file.close();
                if (!synopsisText.isEmpty()) break;
            }
        }
    }

    if (!synopsisText.isEmpty()) {
        m_lblSynopsis->setText(synopsisText);
    } else {
        m_lblSynopsis->setText(QString("No .nfo or summary.txt file found in directory:\n%1\n\nAdd a summary.txt or .nfo file to automatically render plot synopses here.").arg(targetDir));
    }
}
