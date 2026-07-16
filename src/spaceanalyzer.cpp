#include "spaceanalyzer.h"
#include "sunburstchart.h"
#include "treemapchartwidget.h"
#include <QStackedWidget>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStyle>
#include <QApplication>
#include <QMessageBox>

// ================= SpaceAnalyzerWorker =================

SpaceAnalyzerWorker::SpaceAnalyzerWorker(const QString& dirPath, QObject* parent)
    : QThread(parent), m_dirPath(dirPath) {}

SpaceAnalyzerWorker::~SpaceAnalyzerWorker() {
    cancel();
    wait();
}

void SpaceAnalyzerWorker::cancel() {
    QMutexLocker locker(&m_mutex);
    m_cancel = true;
}

void SpaceAnalyzerWorker::run() {
    QDir dir(m_dirPath);
    if (!dir.exists()) {
        emit finished(QList<SpaceEntry>(), 0);
        return;
    }

    QList<SpaceEntry> entries;
    qint64 totalSize = 0;
    m_fileCount = 0;

    QFileInfoList infoList = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    for (const QFileInfo& info : infoList) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_cancel) break;
        }

        SpaceEntry entry;
        entry.name = info.fileName();
        entry.absolutePath = info.absoluteFilePath();
        entry.isDir = info.isDir();

        emit progressUpdate(entry.name, m_fileCount);

        if (entry.isDir) {
            entry.size = calculateSize(entry.absolutePath);
        } else {
            entry.size = info.size();
            m_fileCount++;
        }

        totalSize += entry.size;
        entries.append(entry);
    }

    emit finished(entries, totalSize);
}

qint64 SpaceAnalyzerWorker::calculateSize(const QString& path) {
    qint64 size = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_cancel) return 0;
        }

        it.next();
        size += it.fileInfo().size();
        m_fileCount++;

        if (m_fileCount % 100 == 0) {
            emit progressUpdate(it.fileName(), m_fileCount);
        }
    }
    return size;
}

// ================= SpaceAnalyzerDialog =================

SpaceAnalyzerDialog::SpaceAnalyzerDialog(const QString& startPath, QWidget* parent)
    : QDialog(parent), m_currentPath(startPath) {
    setWindowTitle("Visual Folder Space Analyzer");
    resize(700, 500);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; font-weight: bold; }"
                  "QLineEdit { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 4px 8px; }"
                  "QTreeWidget { background-color: #181825; border: 1px solid #313244; color: #cdd6f4; border-radius: 4px; padding: 4px; }"
                  "QTreeWidget::item { padding: 6px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QProgressBar { border: 1px solid #313244; border-radius: 4px; text-align: center; background-color: #181825; color: #cdd6f4; height: 16px; }"
                  "QProgressBar::chunk { background-color: #89b4fa; }");

    setupUI();
    startScan(m_currentPath);
}

SpaceAnalyzerDialog::~SpaceAnalyzerDialog() {
    if (m_worker) {
        m_worker->cancel();
        m_worker->wait();
    }
}

void SpaceAnalyzerDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Navigation and Action Bar
    QHBoxLayout* navLayout = new QHBoxLayout();
    navLayout->setSpacing(6);

    m_btnBack = new QPushButton("← Back", this);
    m_btnBack->setEnabled(false);
    connect(m_btnBack, &QPushButton::clicked, this, &SpaceAnalyzerDialog::onBackClicked);
    navLayout->addWidget(m_btnBack);

    m_btnToggleView = new QPushButton("Switch to Sunburst", this);
    connect(m_btnToggleView, &QPushButton::clicked, this, &SpaceAnalyzerDialog::onToggleViewMode);
    navLayout->addWidget(m_btnToggleView);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    navLayout->addWidget(m_pathEdit, 1);

    mainLayout->addLayout(navLayout);

    // Scan Progress details
    m_statusLabel = new QLabel("Scanning files...", this);
    m_statusLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
    mainLayout->addWidget(m_statusLabel);

    m_scanProgress = new QProgressBar(this);
    m_scanProgress->setRange(0, 0); // Indeterminate progress
    m_scanProgress->setTextVisible(false);
    mainLayout->addWidget(m_scanProgress);

    // View stack switcher (List view vs Chart view)
    m_viewStack = new QStackedWidget(this);

    // Tree View listing
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({"Name", "Size", "Usage Visualizer", "Percentage"});
    m_tree->setColumnWidth(0, 220);
    m_tree->setColumnWidth(1, 90);
    m_tree->setColumnWidth(2, 280);
    m_tree->setColumnWidth(3, 70);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setStyleSheet("QHeaderView::section { background-color: #313244; color: #cdd6f4; border: 1px solid #181825; padding: 4px; }");
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &SpaceAnalyzerDialog::onItemDoubleClicked);
    m_viewStack->addWidget(m_tree);

    // Chart View visualizer
    m_chart = new SunburstChartWidget(this);
    connect(m_chart, &SunburstChartWidget::itemClicked, this, &SpaceAnalyzerDialog::onChartItemClicked);
    connect(m_chart, &SunburstChartWidget::hoveredItemChanged, this, &SpaceAnalyzerDialog::onChartHoveredItemChanged);
    m_viewStack->addWidget(m_chart);

    m_treemap = new TreeMapChartWidget(this);
    connect(m_treemap, &TreeMapChartWidget::itemClicked, this, &SpaceAnalyzerDialog::onChartItemClicked);
    connect(m_treemap, &TreeMapChartWidget::hoveredItemChanged, this, &SpaceAnalyzerDialog::onChartHoveredItemChanged);
    m_viewStack->addWidget(m_treemap);

    mainLayout->addWidget(m_viewStack);

    // Action buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);

    m_btnNavigate = new QPushButton("Go to Selected Folder", this);
    m_btnNavigate->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    m_btnNavigate->setEnabled(false);
    connect(m_btnNavigate, &QPushButton::clicked, this, &SpaceAnalyzerDialog::onNavigateClicked);
    btnLayout->addWidget(m_btnNavigate);

    btnLayout->addStretch(1);

    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnClose);

    mainLayout->addLayout(btnLayout);
}

void SpaceAnalyzerDialog::startScan(const QString& path) {
    if (m_worker) {
        m_worker->cancel();
        m_worker->wait();
        m_worker->deleteLater();
    }

    m_currentPath = QDir::cleanPath(path);
    m_pathEdit->setText(m_currentPath);
    m_tree->clear();
    if (m_chart) m_chart->setEntries(QList<SpaceEntry>(), 0);
    if (m_treemap) m_treemap->setEntries(QList<SpaceEntry>(), 0);
    m_btnBack->setEnabled(!m_history.isEmpty());
    m_btnNavigate->setEnabled(false);

    m_statusLabel->setText("Initializing disk scan...");
    m_scanProgress->setVisible(true);

    m_worker = new SpaceAnalyzerWorker(m_currentPath, this);
    connect(m_worker, &SpaceAnalyzerWorker::progressUpdate, this, &SpaceAnalyzerDialog::onProgressUpdate);
    connect(m_worker, &SpaceAnalyzerWorker::finished, this, &SpaceAnalyzerDialog::onScanFinished);
    m_worker->start();
}

void SpaceAnalyzerDialog::onProgressUpdate(const QString& currentItem, int count) {
    m_statusLabel->setText(QString("Scanning item: %1 (Found %2 files)").arg(currentItem).arg(count));
}

void SpaceAnalyzerDialog::onScanFinished(const QList<SpaceEntry>& entries, qint64 totalSize) {
    m_scanProgress->setVisible(false);
    m_statusLabel->setText(QString("Scan completed. Total folder size: %1").arg(formatBytes(totalSize)));
    m_btnNavigate->setEnabled(true);

    QStyle* sysStyle = QApplication::style();

    // Copy entries to sort them
    QList<SpaceEntry> sortedEntries = entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const SpaceEntry& a, const SpaceEntry& b) {
        return a.size > b.size;
    });

    for (const SpaceEntry& entry : sortedEntries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_tree);
        item->setText(0, entry.name);
        item->setData(0, Qt::UserRole, entry.absolutePath);
        item->setData(0, Qt::UserRole + 1, entry.isDir);

        if (entry.isDir) {
            item->setIcon(0, sysStyle->standardIcon(QStyle::SP_DirIcon));
        } else {
            item->setIcon(0, sysStyle->standardIcon(QStyle::SP_FileIcon));
        }

        item->setText(1, formatBytes(entry.size));

        // Calculate percentage
        double pct = 0.0;
        if (totalSize > 0) {
            pct = (entry.size * 100.0) / totalSize;
        }

        item->setText(3, QString("%1%").arg(pct, 0, 'f', 1));

        // Add a beautiful custom-colored progress bar as usage visualizer
        QProgressBar* bar = new QProgressBar(m_tree);
        bar->setRange(0, 100);
        bar->setValue(static_cast<int>(pct));
        bar->setFormat(""); // No text inside bar to remain clean

        // Style bar based on space usage
        QString color = "#a6e3a1"; // Green
        if (pct >= 50.0) {
            color = "#f38ba8"; // Red
        } else if (pct >= 10.0) {
            color = "#f9e2af"; // Yellow
        }

        bar->setStyleSheet(QString("QProgressBar { border: 1px solid #313244; border-radius: 3px; background-color: #11111b; height: 12px; }"
                                   "QProgressBar::chunk { background-color: %1; }").arg(color));

        m_tree->setItemWidget(item, 2, bar);
    }
    if (m_chart) {
        m_chart->setEntries(sortedEntries, totalSize);
    }
    if (m_treemap) {
        m_treemap->setEntries(sortedEntries, totalSize);
    }
}

void SpaceAnalyzerDialog::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;

    QString itemPath = item->data(0, Qt::UserRole).toString();
    bool isDir = item->data(0, Qt::UserRole + 1).toBool();

    if (isDir) {
        m_history.append(m_currentPath);
        startScan(itemPath);
    }
}

void SpaceAnalyzerDialog::onBackClicked() {
    if (!m_history.isEmpty()) {
        QString prevPath = m_history.takeLast();
        startScan(prevPath);
    }
}

void SpaceAnalyzerDialog::onNavigateClicked() {
    QTreeWidgetItem* current = m_tree->currentItem();
    if (current) {
        QString path = current->data(0, Qt::UserRole).toString();
        bool isDir = current->data(0, Qt::UserRole + 1).toBool();
        if (isDir) {
            m_selectedPath = path;
        } else {
            m_selectedPath = QFileInfo(path).absolutePath();
        }
    } else {
        m_selectedPath = m_currentPath;
    }
    m_navigateRequested = true;
    accept();
}

QString SpaceAnalyzerDialog::formatBytes(qint64 bytes) const {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;
    if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
    if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
    if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
    return QString("%1 B").arg(bytes);
}

void SpaceAnalyzerDialog::onToggleViewMode() {
    if (!m_viewStack || !m_btnToggleView) return;
    int current = m_viewStack->currentIndex();
    if (current == 0) {
        m_viewStack->setCurrentIndex(1);
        m_btnToggleView->setText("Switch to TreeMap");
    } else if (current == 1) {
        m_viewStack->setCurrentIndex(2);
        m_btnToggleView->setText("Switch to List");
    } else {
        m_viewStack->setCurrentIndex(0);
        m_btnToggleView->setText("Switch to Sunburst");
    }
}

void SpaceAnalyzerDialog::onChartHoveredItemChanged(const QString& name, qint64 size, double percentage) {
    if (name.isEmpty()) {
        m_statusLabel->setText(QString("Analyzing: %1").arg(m_currentPath));
    } else {
        m_statusLabel->setText(QString("Hovered: %1 - %2 (%3%)")
                               .arg(name)
                               .arg(formatBytes(size))
                               .arg(QString::number(percentage, 'f', 1)));
    }
}

void SpaceAnalyzerDialog::onChartItemClicked(const QString& path, bool isDir) {
    if (isDir) {
        m_history.append(m_currentPath);
        startScan(path);
    } else {
        m_selectedPath = path;
        m_btnNavigate->setEnabled(true);
    }
}
