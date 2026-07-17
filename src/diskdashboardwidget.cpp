#include "diskdashboardwidget.h"
#include "tagmanager.h"
#include <QThreadPool>
#include <QDirIterator>
#include <QDateTime>
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>

DiskDashboardWidget::DiskDashboardWidget(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(15);

    // Left side: Custom chart widget panel
    QWidget* chartsPanel = new QWidget(this);
    chartsPanel->setMinimumWidth(280);
    mainLayout->addWidget(chartsPanel, 2);

    // Right side: Table panel for largest files
    QWidget* tablePanel = new QWidget(this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(8);

    QLabel* tableTitle = new QLabel("<b>Top 20 Largest Files</b>", this);
    tableTitle->setStyleSheet("color: #a6e3a1; font-size: 13px;");
    tableLayout->addWidget(tableTitle);

    m_largestTable = new QTableWidget(this);
    m_largestTable->setColumnCount(3);
    m_largestTable->setHorizontalHeaderLabels({"File Name", "Size", "Path"});
    m_largestTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_largestTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_largestTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_largestTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_largestTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_largestTable->setStyleSheet(
        "QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 11px; }"
        "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
        "QTableWidget::item { padding: 4px; }"
    );
    tableLayout->addWidget(m_largestTable);
    mainLayout->addWidget(tablePanel, 3);

    connect(m_largestTable, &QTableWidget::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) return;
        int row = index.row();
        QTableWidgetItem* pathItem = m_largestTable->item(row, 2);
        if (pathItem) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(pathItem->text()));
        }
    });
}

void DiskDashboardWidget::scanDirectory(const QString& path) {
    m_path = path;
    m_isScanning = true;
    m_data = DashboardData();
    m_largestTable->setRowCount(0);
    emit scanStarted();
    update();

    DashboardWorker* worker = new DashboardWorker(path);
    connect(worker, &DashboardWorker::scanFinished, this, &DiskDashboardWidget::onScanFinished);
    QThreadPool::globalInstance()->start(worker);
}

void DiskDashboardWidget::onScanFinished(const DashboardData& data) {
    QMutexLocker locker(&m_mutex);
    m_data = data;
    m_isScanning = false;

    // Fill table
    m_largestTable->setRowCount(0);
    for (int i = 0; i < data.largestFiles.size(); ++i) {
        const QFileInfo& fi = data.largestFiles[i];
        m_largestTable->insertRow(i);

        QTableWidgetItem* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setForeground(QBrush(QColor("#cdd6f4")));

        double kb = fi.size() / 1024.0;
        double mb = kb / 1024.0;
        QString sizeStr = (mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1);
        QTableWidgetItem* sizeItem = new QTableWidgetItem(sizeStr);
        sizeItem->setForeground(QBrush(QColor("#f9e2af")));

        QTableWidgetItem* pathItem = new QTableWidgetItem(fi.absoluteFilePath());
        pathItem->setForeground(QBrush(QColor("#a6adc8")));

        m_largestTable->setItem(i, 0, nameItem);
        m_largestTable->setItem(i, 1, sizeItem);
        m_largestTable->setItem(i, 2, pathItem);
    }

    emit scanFinished();
    update();
}

void DiskDashboardWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor("#1e1e2e"));

    if (m_isScanning) {
        painter.setPen(QColor("#cdd6f4"));
        painter.setFont(QFont("", 12, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "Analyzing directory metrics recursively...");
        return;
    }

    QMutexLocker locker(&m_mutex);
    int topOffset = 25;

    // Draw Dashboard Header
    painter.setPen(QColor("#cdd6f4"));
    painter.setFont(QFont("", 11, QFont::Bold));
    painter.drawText(15, topOffset, QString("Metrics Dashboard: %1").arg(QFileInfo(m_path).fileName()));

    int halfHeight = height() / 2;
    int donutWidth = 250;
    drawDonutChart(painter, QRect(15, topOffset + 25, donutWidth, donutWidth));
    drawTagBars(painter, QRect(15, topOffset + donutWidth + 60, donutWidth + 30, halfHeight - 60));
}

void DiskDashboardWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void DiskDashboardWidget::drawDonutChart(QPainter& painter, const QRect& rect) {
    if (m_data.totalScanSize <= 0) {
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(rect, Qt::AlignCenter, "No files found to categorize.");
        return;
    }

    // Modern color scheme (Catppuccin flavors)
    QMap<QString, QColor> typeColors = {
        {"Videos", QColor("#f38ba8")},    // red
        {"Images", QColor("#fab387")},    // peach
        {"Audio", QColor("#f9e2af")},     // yellow
        {"Documents", QColor("#a6e3a1")}, // green
        {"Archives", QColor("#89b4fa")},  // blue
        {"Others", QColor("#cba6f7")}     // mauve
    };

    double startAngle = 90.0;
    int colY = rect.bottom() + 15;
    int colX = rect.left();

    painter.setFont(QFont("", 9));
    int legendIdx = 0;

    for (auto it = m_data.typeSizes.begin(); it != m_data.typeSizes.end(); ++it) {
        double pct = (double)it.value() / m_data.totalScanSize;
        if (pct <= 0.0) continue;

        double spanAngle = -pct * 360.0;
        QColor color = typeColors.value(it.key(), QColor("#bac2de"));

        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPie(rect, qRound(startAngle * 16), qRound(spanAngle * 16));

        startAngle += spanAngle;

        // Draw Legend item
        int lx = colX + (legendIdx % 2) * 130;
        int ly = colY + (legendIdx / 2) * 18;
        painter.fillRect(lx, ly, 10, 10, color);

        double mb = it.value() / (1024.0 * 1024.0);
        painter.setPen(QColor("#cdd6f4"));
        painter.drawText(lx + 15, ly + 9, QString("%1 (%2%)").arg(it.key()).arg(qRound(pct * 100)));
        legendIdx++;
    }

    // Draw center cutout for Donut effect
    painter.setBrush(QColor("#1e1e2e"));
    painter.setPen(Qt::NoPen);
    int cutoutRadius = rect.width() * 0.35;
    painter.drawEllipse(rect.center(), cutoutRadius, cutoutRadius);

    // Draw total size text inside cutout
    painter.setPen(QColor("#cdd6f4"));
    painter.setFont(QFont("", 9, QFont::Bold));
    double totalMB = m_data.totalScanSize / (1024.0 * 1024.0);
    QString totalStr = (totalMB >= 1024.0) ? QString("%1 GB").arg(totalMB / 1024.0, 0, 'f', 1) : QString("%1 MB").arg(totalMB, 0, 'f', 1);
    painter.drawText(QRect(rect.left(), rect.top() - 3, rect.width(), rect.height()), Qt::AlignCenter, totalStr);
}

void DiskDashboardWidget::drawTagBars(QPainter& painter, const QRect& rect) {
    painter.setPen(QColor("#cdd6f4"));
    painter.setFont(QFont("", 10, QFont::Bold));
    painter.drawText(rect.left(), rect.top() - 10, "Tag Distribution");

    if (m_data.tagCounts.isEmpty()) {
        painter.setFont(QFont("", 9, QFont::StyleItalic));
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop, "No tagged files detected in this folder.");
        return;
    }

    int maxCount = 1;
    for (int count : m_data.tagCounts) {
        if (count > maxCount) maxCount = count;
    }

    int idx = 0;
    int itemHeight = 16;
    int gap = 8;
    painter.setFont(QFont("", 9));

    for (auto it = m_data.tagCounts.begin(); it != m_data.tagCounts.end(); ++it) {
        int y = rect.top() + idx * (itemHeight + gap);
        if (y + itemHeight > height()) break;

        QString tag = it.key();
        int count = it.value();

        // Label
        painter.setPen(QColor("#cdd6f4"));
        painter.drawText(rect.left(), y + 12, tag.left(8));

        // Bar
        int maxBarWidth = rect.width() - 80;
        int barWidth = qRound(((double)count / maxCount) * maxBarWidth);
        if (barWidth < 4) barWidth = 4;

        // Custom label colors matching badges
        QColor barColor = TagManager::instance().getColorValue(TagManager::instance().getFileColor(tag));
        if (!barColor.isValid()) barColor = QColor("#89b4fa"); // default blue

        painter.fillRect(rect.left() + 60, y, barWidth, itemHeight, barColor);

        // Value text
        painter.setPen(QColor("#bac2de"));
        painter.drawText(rect.left() + 65 + barWidth, y + 12, QString("%1 files").arg(count));
        idx++;
    }
}

DashboardWorker::DashboardWorker(const QString& path) : m_path(path) {}

void DashboardWorker::run() {
    DashboardData data;
    QDateTime now = QDateTime::currentDateTime();

    QStringList imgExts = {"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg"};
    QStringList vidExts = {"mp4", "avi", "mkv", "mov", "webm", "flv", "wmv"};
    QStringList audExts = {"mp3", "wav", "flac", "ogg", "m4a", "aac"};
    QStringList docExts = {"pdf", "doc", "docx", "txt", "odt", "xls", "xlsx", "ppt", "pptx", "html"};
    QStringList archExts = {"zip", "tar", "gz", "xz", "bz2", "rar", "7z"};

    // Scan files recursively
    QDirIterator it(m_path, QDir::Files, QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext() && count < 8000) {
        it.next();
        count++;

        QFileInfo info = it.fileInfo();
        qint64 sz = info.size();
        data.totalScanSize += sz;

        // Determine category
        QString ext = info.suffix().toLower();
        QString cat = "Others";
        if (imgExts.contains(ext)) cat = "Images";
        else if (vidExts.contains(ext)) cat = "Videos";
        else if (audExts.contains(ext)) cat = "Audio";
        else if (docExts.contains(ext)) cat = "Documents";
        else if (archExts.contains(ext)) cat = "Archives";

        data.typeSizes[cat] += sz;

        // Tags distribution
        QStringList tags = TagManager::instance().getFileTags(info.absoluteFilePath());
        for (const QString& tag : tags) {
            data.tagCounts[tag]++;
        }

        // Largest files
        data.largestFiles.append(info);
        std::sort(data.largestFiles.begin(), data.largestFiles.end(), [](const QFileInfo& a, const QFileInfo& b) {
            return a.size() > b.size();
        });
        if (data.largestFiles.size() > 20) {
            data.largestFiles.removeLast();
        }
    }

    emit scanFinished(data);
}
