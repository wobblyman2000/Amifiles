#include "searchdialog.h"
#include "metadataextractor.h"
#include "theme.h"
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QDirIterator>
#include <QRegularExpression>
#include <QFile>
#include <QMessageBox>

AdvancedSearchWorker::AdvancedSearchWorker(const QString& query, const QString& path, bool caseSensitive, int matchType,
                                           bool filterSize, qint64 minSize, qint64 maxSize,
                                           bool filterDate, const QDateTime& startDate, const QDateTime& endDate,
                                           bool searchInside)
    : m_query(query), m_path(path), m_caseSensitive(caseSensitive), m_matchType(matchType),
      m_filterSize(filterSize), m_minSize(minSize), m_maxSize(maxSize),
      m_filterDate(filterDate), m_startDate(startDate), m_endDate(endDate),
      m_searchInside(searchInside), m_cancel(false) {}

void AdvancedSearchWorker::cancel() {
    m_cancel = true;
}

void AdvancedSearchWorker::doSearch() {
    m_cancel = false;

    if (m_query.isEmpty() && !m_filterSize && !m_filterDate) {
        emit finished();
        return;
    }

    QDirIterator it(m_path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (m_cancel) break;

        it.next();
        QFileInfo info = it.fileInfo();
        QString name = info.fileName();

        // 1. Name Match (if query is not empty)
        if (!m_query.isEmpty()) {
            bool isMatch = false;
            if (m_matchType == 0) { // Substring
                isMatch = name.contains(m_query, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            } else if (m_matchType == 1) { // Glob / Wildcard
                QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(m_query),
                                         m_caseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
                isMatch = regex.match(name).hasMatch();
            } else if (m_matchType == 2) { // Regex
                QRegularExpression regex(m_query,
                                         m_caseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
                isMatch = regex.match(name).hasMatch();
            }
            if (!isMatch) continue;
        }

        // 2. Size Filter
        if (m_filterSize && info.isFile()) {
            qint64 sz = info.size();
            if (sz < m_minSize || sz > m_maxSize) {
                continue;
            }
        }

        // 3. Date Filter
        if (m_filterDate) {
            QDateTime modified = info.lastModified();
            if (modified < m_startDate || modified > m_endDate) {
                continue;
            }
        }

        // 4. Content / Inside Search
        if (m_searchInside && info.isFile() && !m_query.isEmpty()) {
            bool insideMatch = false;
            QFile file(info.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray chunk = file.read(65536); // Read up to 64KB to avoid freeze
                QString contentStr = QString::fromUtf8(chunk);
                insideMatch = contentStr.contains(m_query, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
                file.close();
            }

            // Check metadata as well
            if (!insideMatch) {
                QString ext = info.suffix().toLower();
                if (ext == "mp3" || ext == "flac" || ext == "wav" || ext == "ogg") {
                    FileMetadata meta = MetadataExtractor::extract(info.absoluteFilePath());
                    insideMatch = meta.title.contains(m_query, Qt::CaseInsensitive) ||
                                  meta.artist.contains(m_query, Qt::CaseInsensitive) ||
                                  meta.album.contains(m_query, Qt::CaseInsensitive) ||
                                  meta.genre.contains(m_query, Qt::CaseInsensitive) ||
                                  meta.comment.contains(m_query, Qt::CaseInsensitive);
                }
            }

            if (!insideMatch) continue;
        }

        emit resultFound(info.absoluteFilePath(), info.size(), info.isDir() ? "Folder" : info.suffix().toUpper(), info.lastModified());
    }

    emit finished();
}

// ======================== SearchDialog ========================

SearchDialog::SearchDialog(const QString& startPath, QWidget* parent)
    : QDialog(parent), m_startPath(startPath) {
    setWindowTitle("Advanced Search & Filtering");
    resize(780, 580);
    setStyleSheet(Theme::getStylesheet());

    setupUI();
    m_txtLocation->setText(QDir::toNativeSeparators(m_startPath));
}

SearchDialog::~SearchDialog() {
    onStopSearch();
}

void SearchDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Group 1: Search Criteria
    QGroupBox* grpCriteria = new QGroupBox("Search Location & Parameters", this);
    QFormLayout* form = new QFormLayout(grpCriteria);
    form->setSpacing(8);

    m_txtQuery = new QLineEdit(this);
    m_txtQuery->setPlaceholderText("Search Query (e.g. *.mp3, track_??, or regex pattern)");
    form->addRow("Search For Name:", m_txtQuery);

    QHBoxLayout* locRow = new QHBoxLayout();
    m_txtLocation = new QLineEdit(this);
    m_btnBrowse = new QPushButton("Browse...", this);
    m_btnBrowse->setMaximumWidth(80);
    locRow->addWidget(m_txtLocation);
    locRow->addWidget(m_btnBrowse);
    form->addRow("Search Location:", locRow);

    QHBoxLayout* matchRow = new QHBoxLayout();
    m_comboType = new QComboBox(this);
    m_comboType->addItems({"Substring Match", "Wildcard / Glob Match", "Regular Expression (Regex)"});
    m_chkCaseSensitive = new QCheckBox("Case Sensitive", this);
    m_chkInside = new QCheckBox("Search inside file content / metadata tags", this);
    matchRow->addWidget(m_comboType);
    matchRow->addWidget(m_chkCaseSensitive);
    matchRow->addStretch();
    form->addRow("Matching Options:", matchRow);
    form->addRow("", m_chkInside);

    mainLayout->addWidget(grpCriteria);

    // Group 2: Advanced Filters (Size & Date)
    QGroupBox* grpFilters = new QGroupBox("Advanced Size & Date Filters", this);
    QHBoxLayout* filterLayout = new QHBoxLayout(grpFilters);
    filterLayout->setSpacing(15);

    // Left size filters
    QVBoxLayout* sizeCol = new QVBoxLayout();
    m_chkSize = new QCheckBox("Filter by File Size", this);
    sizeCol->addWidget(m_chkSize);

    QHBoxLayout* sizeRange = new QHBoxLayout();
    m_spinMinSize = new QSpinBox(this);
    m_spinMinSize->setRange(0, 999999);
    m_spinMinSize->setValue(0);
    m_comboMinUnit = new QComboBox(this);
    m_comboMinUnit->addItems({"Bytes", "KB", "MB", "GB"});
    m_comboMinUnit->setCurrentIndex(1); // default KB

    m_spinMaxSize = new QSpinBox(this);
    m_spinMaxSize->setRange(0, 999999);
    m_spinMaxSize->setValue(100);
    m_comboMaxUnit = new QComboBox(this);
    m_comboMaxUnit->addItems({"Bytes", "KB", "MB", "GB"});
    m_comboMaxUnit->setCurrentIndex(2); // default MB

    sizeRange->addWidget(new QLabel("Min:", this));
    sizeRange->addWidget(m_spinMinSize);
    sizeRange->addWidget(m_comboMinUnit);
    sizeRange->addSpacing(10);
    sizeRange->addWidget(new QLabel("Max:", this));
    sizeRange->addWidget(m_spinMaxSize);
    sizeRange->addWidget(m_comboMaxUnit);

    sizeCol->addLayout(sizeRange);
    filterLayout->addLayout(sizeCol, 1);

    // Right date filters
    QVBoxLayout* dateCol = new QVBoxLayout();
    m_chkDate = new QCheckBox("Filter by Last Modified Date", this);
    dateCol->addWidget(m_chkDate);

    QHBoxLayout* dateRange = new QHBoxLayout();
    m_dateStart = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7), this);
    m_dateStart->setCalendarPopup(true);
    m_dateEnd = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_dateEnd->setCalendarPopup(true);

    dateRange->addWidget(new QLabel("From:", this));
    dateRange->addWidget(m_dateStart);
    dateRange->addSpacing(10);
    dateRange->addWidget(new QLabel("To:", this));
    dateRange->addWidget(m_dateEnd);

    dateCol->addLayout(dateRange);
    filterLayout->addLayout(dateCol, 1);

    mainLayout->addWidget(grpFilters);

    // Group 3: Table Results
    m_tableResults = new QTableWidget(0, 5, this);
    m_tableResults->setHorizontalHeaderLabels({"File Name", "Size", "Type", "Modified Date", "Location"});
    m_tableResults->setAlternatingRowColors(true);
    m_tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableResults->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableResults->setStyleSheet("QTableWidget { background-color: #11111b; border: 1px solid #313244; border-radius: 6px; }");
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tableResults->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_tableResults->setColumnWidth(0, 200);
    m_tableResults->setColumnWidth(3, 150);

    mainLayout->addWidget(m_tableResults, 1);

    // Bottom Status & Controls Bar
    QHBoxLayout* bottomBar = new QHBoxLayout();
    m_lblStatus = new QLabel("Ready to search", this);
    m_lblStatus->setStyleSheet("color: #a6adc8;");
    bottomBar->addWidget(m_lblStatus, 1);

    m_btnSearch = new QPushButton("Search", this);
    m_btnSearch->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }");
    m_btnStop = new QPushButton("Stop", this);
    m_btnStop->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; font-weight: bold; }");
    m_btnStop->setEnabled(false);

    m_btnClose = new QPushButton("Close", this);

    bottomBar->addWidget(m_btnSearch);
    bottomBar->addWidget(m_btnStop);
    bottomBar->addWidget(m_btnClose);

    mainLayout->addLayout(bottomBar);

    // Wire up events
    connect(m_btnBrowse, &QPushButton::clicked, this, &SearchDialog::onBrowsePath);
    connect(m_btnSearch, &QPushButton::clicked, this, &SearchDialog::onStartSearch);
    connect(m_btnStop, &QPushButton::clicked, this, &SearchDialog::onStopSearch);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_tableResults, &QTableWidget::cellDoubleClicked, this, &SearchDialog::onDoubleClicked);
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SearchDialog::onTypeChanged);

    onTypeChanged(0);
}

void SearchDialog::onTypeChanged(int index) {
    if (index == 1) { // Wildcard
        m_txtQuery->setPlaceholderText("Search Query (e.g. *.mp4, photo_??.jpg)");
    } else if (index == 2) { // Regex
        m_txtQuery->setPlaceholderText("Regular Expression (e.g. ^IMG_\\d+\\.png$)");
    } else { // Substring
        m_txtQuery->setPlaceholderText("Search substring (e.g. hello)");
    }
}

void SearchDialog::onBrowsePath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Search", m_txtLocation->text());
    if (!dir.isEmpty()) {
        m_txtLocation->setText(QDir::toNativeSeparators(dir));
    }
}

void SearchDialog::onStartSearch() {
    onStopSearch(); // Safety cancel

    m_tableResults->setRowCount(0);
    m_lblStatus->setText("Initializing search...");

    QString query = m_txtQuery->text().trimmed();
    QString path = QDir::fromNativeSeparators(m_txtLocation->text().trimmed());

    if (path.isEmpty() || !QDir(path).exists()) {
        QMessageBox::warning(this, "Location Required", "Please specify a valid folder path to scan.");
        return;
    }

    // Calc size limits
    qint64 minSize = 0;
    qint64 maxSize = 0xFFFFFFFFFFFFFFF;
    if (m_chkSize->isChecked()) {
        qint64 units[] = {1, 1024, 1024 * 1024, 1024 * 1024 * 1024};
        minSize = m_spinMinSize->value() * units[m_comboMinUnit->currentIndex()];
        maxSize = m_spinMaxSize->value() * units[m_comboMaxUnit->currentIndex()];
    }

    m_thread = new QThread(this);
    m_worker = new AdvancedSearchWorker(query, path, m_chkCaseSensitive->isChecked(), m_comboType->currentIndex(),
                                         m_chkSize->isChecked(), minSize, maxSize,
                                         m_chkDate->isChecked(), m_dateStart->dateTime(), m_dateEnd->dateTime(),
                                         m_chkInside->isChecked());

    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &AdvancedSearchWorker::doSearch);
    connect(m_worker, &AdvancedSearchWorker::resultFound, this, &SearchDialog::onResultFound);
    connect(m_worker, &AdvancedSearchWorker::finished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, this, &SearchDialog::onSearchFinished);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_btnSearch->setEnabled(false);
    m_btnStop->setEnabled(true);

    m_thread->start();
    m_lblStatus->setText("Scanning directories asynchronously...");
}

void SearchDialog::onStopSearch() {
    if (m_worker) {
        m_worker->cancel();
    }
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
    m_thread = nullptr;
    m_worker = nullptr;
}

void SearchDialog::onResultFound(const QString& filePath, qint64 size, const QString& type, const QDateTime& modified) {
    int row = m_tableResults->rowCount();
    m_tableResults->insertRow(row);

    QFileInfo info(filePath);

    QTableWidgetItem* nameItem = new QTableWidgetItem(info.fileName());
    nameItem->setData(Qt::UserRole, filePath); // Store absolute path

    QString sizeStr;
    if (info.isDir()) {
        sizeStr = "";
    } else {
        double sz = size;
        QStringList units = {"B", "KB", "MB", "GB"};
        int u = 0;
        while (sz >= 1024 && u < units.size() - 1) {
            sz /= 1024.0;
            u++;
        }
        sizeStr = QString("%1 %2").arg(sz, 0, 'f', 1).arg(units[u]);
    }

    QTableWidgetItem* sizeItem = new QTableWidgetItem(sizeStr);
    QTableWidgetItem* typeItem = new QTableWidgetItem(type);
    QTableWidgetItem* dateItem = new QTableWidgetItem(modified.toString("yyyy-MM-dd hh:mm:ss"));
    QTableWidgetItem* locItem = new QTableWidgetItem(QDir::toNativeSeparators(info.absolutePath()));

    m_tableResults->setItem(row, 0, nameItem);
    m_tableResults->setItem(row, 1, sizeItem);
    m_tableResults->setItem(row, 2, typeItem);
    m_tableResults->setItem(row, 3, dateItem);
    m_tableResults->setItem(row, 4, locItem);

    m_lblStatus->setText(QString("Scanning... Found %1 matches").arg(row + 1));
}

void SearchDialog::onSearchFinished() {
    m_btnSearch->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_thread = nullptr;
    m_worker = nullptr;

    int count = m_tableResults->rowCount();
    m_lblStatus->setText(QString("Search complete. Found %1 matching items.").arg(count));
}

void SearchDialog::onDoubleClicked(int row, int /*column*/) {
    QTableWidgetItem* item = m_tableResults->item(row, 0);
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();

    // Call back into our active main window file panel navigation helper!
    // Since parent is MainWindow, we can find it
    QWidget* p = parentWidget();
    while (p) {
        // Try casting to MainWindow
        auto* mainWin = qobject_cast<class MainWindow*>(p);
        if (mainWin) {
            // Find active panel and select path!
            // Let's see: we can declare navigateto in main window or use public API
            mainWin->navigateToPathAndSelect(path);
            break;
        }
        p = p->parentWidget();
    }
}
