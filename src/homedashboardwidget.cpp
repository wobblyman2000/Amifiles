#include "homedashboardwidget.h"
#include "mainwindow.h"
#include <QStorageInfo>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QFrame>
#include <QToolButton>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>

#include "archivedialog.h"
#include "videoscraperdialog.h"
#include "bulkrename.h"
#include "favoritesmanager.h"

// Custom double-clickable card widget
class ClickableCardFrame : public QFrame {
    Q_OBJECT
public:
    explicit ClickableCardFrame(const QString& path, int layoutIndex = -1, QWidget* parent = nullptr)
        : QFrame(parent), m_path(path), m_layoutIndex(layoutIndex) {
        setObjectName("cardFrame");
        setProperty("class", "cardFrame");
    }

signals:
    void doubleClicked(const QString& path);
    void doubleClickedWithLayout(const QString& path, int layoutIndex);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (m_layoutIndex != -1) {
                emit doubleClickedWithLayout(m_path, m_layoutIndex);
            } else {
                emit doubleClicked(m_path);
            }
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    QString m_path;
    int m_layoutIndex;
};

static void clearLayout(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

HomeDashboardWidget::HomeDashboardWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshDashboard();
}

void HomeDashboardWidget::setupUi() {
    // Style the overall widget with a dark Catppuccin theme
    setStyleSheet(
        "QWidget { background-color: #1e1e2e; color: #cdd6f4; font-family: 'Segoe UI', 'Inter', 'sans-serif'; }"
        "QLabel#sectionTitle { color: #89b4fa; font-size: 14px; font-weight: bold; padding-top: 15px; padding-bottom: 5px; }"
        "QFrame#cardFrame { background-color: #181825; border: 1px solid #313244; border-radius: 8px; }"
        "QFrame#cardFrame:hover { border: 1px solid #89b4fa; background-color: #252538; }"
        "QProgressBar { border: 1px solid #45475a; border-radius: 4px; background: #11111b; text-align: center; color: #cdd6f4; font-size: 10px; font-weight: bold; }"
        "QProgressBar::chunk { background-color: #a6e3a1; border-radius: 3px; }"
        "QToolButton.dashboardButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 10px; font-size: 11px; font-weight: bold; text-align: left; }"
        "QToolButton.dashboardButton:hover { background-color: #45475a; border-color: #89b4fa; color: #89b4fa; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { border: none; background: #181825; width: 8px; margin: 0px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #585b70; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QPushButton.unpinButton { background: transparent; border: none; color: #f38ba8; font-size: 14px; font-weight: bold; }"
        "QPushButton.unpinButton:hover { color: #f2cdcd; background: #313244; border-radius: 4px; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left scroll area for dashboard content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    QWidget* scrollContent = new QWidget(scrollArea);
    scrollArea->setWidget(scrollContent);

    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(10);

    // Welcome Banner Header
    QFrame* bannerFrame = new QFrame(scrollContent);
    bannerFrame->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 10px; padding: 15px;");
    QVBoxLayout* bannerLayout = new QVBoxLayout(bannerFrame);
    bannerLayout->setContentsMargins(15, 10, 15, 10);
    bannerLayout->setSpacing(4);

    QLabel* titleLabel = new QLabel("Welcome to Amifiles", bannerFrame);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #cba6f7;");
    bannerLayout->addWidget(titleLabel);

    QLabel* subLabel = new QLabel("Your ultimate dual-pane Amiga & Linux file manager. Double-click storage cards or folders to jump straight in.", bannerFrame);
    subLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
    bannerLayout->addWidget(subLabel);

    contentLayout->addWidget(bannerFrame);

    // 1. Storage Drives Section
    QLabel* drivesTitle = new QLabel("💾 Storage Drives", scrollContent);
    drivesTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(drivesTitle);

    QFrame* drivesContainer = new QFrame(scrollContent);
    m_drivesLayout = new QGridLayout(drivesContainer);
    m_drivesLayout->setContentsMargins(0, 5, 0, 5);
    m_drivesLayout->setSpacing(10);
    contentLayout->addWidget(drivesContainer);

    // 2. Quick Access Section
    QLabel* quickTitle = new QLabel("📁 Quick Access Folders", scrollContent);
    quickTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(quickTitle);

    QFrame* quickContainer = new QFrame(scrollContent);
    m_quickAccessLayout = new QGridLayout(quickContainer);
    m_quickAccessLayout->setContentsMargins(0, 5, 0, 5);
    m_quickAccessLayout->setSpacing(10);
    contentLayout->addWidget(quickContainer);

    // 3. Pinned Folders Section
    QLabel* pinnedTitle = new QLabel("📌 Pinned Folders with Layout Memory", scrollContent);
    pinnedTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(pinnedTitle);

    QFrame* pinnedContainer = new QFrame(scrollContent);
    m_pinnedLayout = new QGridLayout(pinnedContainer);
    m_pinnedLayout->setContentsMargins(0, 5, 0, 5);
    m_pinnedLayout->setSpacing(10);
    contentLayout->addWidget(pinnedContainer);

    contentLayout->addStretch(1);
    mainLayout->addWidget(scrollArea, 1);

    // Right Sidebar Toolbox (Width: 230px)
    QFrame* sidebar = new QFrame(this);
    sidebar->setObjectName("sidebarFrame");
    sidebar->setStyleSheet("background-color: #11111b; border-left: 1px solid #313244;");
    sidebar->setFixedWidth(230);

    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(15, 20, 15, 20);
    sidebarLayout->setSpacing(12);

    QLabel* toolboxTitle = new QLabel("🔧 Power Toolbox", sidebar);
    toolboxTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #fab387; padding-bottom: 5px;");
    sidebarLayout->addWidget(toolboxTitle);

    QToolButton* btnRename = new QToolButton(sidebar);
    btnRename->setText("📝  Bulk Rename Tool...");
    btnRename->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnRename->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnRename->setProperty("class", "dashboardButton");
    connect(btnRename, &QToolButton::clicked, this, [this]() { onToolButtonClicked("bulk_rename"); });
    sidebarLayout->addWidget(btnRename);

    QToolButton* btnScraper = new QToolButton(sidebar);
    btnScraper->setText("🎬  Video Scraper...");
    btnScraper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnScraper->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnScraper->setProperty("class", "dashboardButton");
    connect(btnScraper, &QToolButton::clicked, this, [this]() { onToolButtonClicked("video_scraper"); });
    sidebarLayout->addWidget(btnScraper);

    QToolButton* btnArchive = new QToolButton(sidebar);
    btnArchive->setText("📦  Archive Creator...");
    btnArchive->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnArchive->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnArchive->setProperty("class", "dashboardButton");
    connect(btnArchive, &QToolButton::clicked, this, [this]() { onToolButtonClicked("archive_creator"); });
    sidebarLayout->addWidget(btnArchive);

    QToolButton* btnDiff = new QToolButton(sidebar);
    btnDiff->setText("⚖️  Visual Diff Tool...");
    btnDiff->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnDiff->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnDiff->setProperty("class", "dashboardButton");
    connect(btnDiff, &QToolButton::clicked, this, [this]() { onToolButtonClicked("visual_diff"); });
    sidebarLayout->addWidget(btnDiff);

    QToolButton* btnDup = new QToolButton(sidebar);
    btnDup->setText("🔍  Duplicate Finder...");
    btnDup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnDup->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnDup->setProperty("class", "dashboardButton");
    connect(btnDup, &QToolButton::clicked, this, [this]() { onToolButtonClicked("dup_finder"); });
    sidebarLayout->addWidget(btnDup);

    sidebarLayout->addStretch(1);

    QFrame* separator = new QFrame(sidebar);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background-color: #313244; max-height: 1px;");
    sidebarLayout->addWidget(separator);

    QToolButton* btnSettings = new QToolButton(sidebar);
    btnSettings->setText("⚙️  System Preferences...");
    btnSettings->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnSettings->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnSettings->setProperty("class", "dashboardButton");
    connect(btnSettings, &QToolButton::clicked, this, [this]() { onToolButtonClicked("preferences"); });
    sidebarLayout->addWidget(btnSettings);

    mainLayout->addWidget(sidebar);
}

void HomeDashboardWidget::refreshDashboard() {
    populateDrives();
    populateQuickAccess();
    populatePinnedFolders();
}

void HomeDashboardWidget::populateDrives() {
    clearLayout(m_drivesLayout);

    int row = 0;
    int col = 0;

    for (const QStorageInfo& storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        if (storage.bytesTotal() == 0) continue;

        QString type = storage.fileSystemType();
        if (type == "tmpfs" || type == "devtmpfs" || type == "sysfs" || type == "proc" || type == "cgroup" || type == "squashfs") continue;

        ClickableCardFrame* card = new ClickableCardFrame(storage.rootPath(), -1, this);
        connect(card, &ClickableCardFrame::doubleClicked, this, &HomeDashboardWidget::onDriveDoubleClicked);

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);

        QLabel* iconLabel = new QLabel(card);
        iconLabel->setText(storage.isReadOnly() ? "🔒" : "🖴");
        iconLabel->setStyleSheet("font-size: 24px; color: #89b4fa;");
        layout->addWidget(iconLabel);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(2);

        QString name = storage.displayName();
        if (name.isEmpty() || name == "/") {
            name = QString("OS Root (/)");
        }
        QLabel* nameLabel = new QLabel(name, card);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #cdd6f4;");
        details->addWidget(nameLabel);

        QLabel* pathLabel = new QLabel(storage.rootPath(), card);
        pathLabel->setStyleSheet("font-size: 9px; color: #a6adc8;");
        details->addWidget(pathLabel);

        qint64 total = storage.bytesTotal();
        qint64 free = storage.bytesAvailable();
        qint64 used = total - free;
        double pct = (double)used / (double)total * 100.0;

        QString usageText = QString("%1 GB free of %2 GB")
            .arg(QString::number((double)free / (1024.0 * 1024.0 * 1024.0), 'f', 1))
            .arg(QString::number((double)total / (1024.0 * 1024.0 * 1024.0), 'f', 1));

        QLabel* usageLabel = new QLabel(usageText, card);
        usageLabel->setStyleSheet("font-size: 10px; color: #bac2de; margin-top: 3px;");
        details->addWidget(usageLabel);

        QProgressBar* progress = new QProgressBar(card);
        progress->setValue((int)pct);
        progress->setFixedHeight(12);
        details->addWidget(progress);

        layout->addLayout(details, 1);

        m_drivesLayout->addWidget(card, row, col);

        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
}

void HomeDashboardWidget::populateQuickAccess() {
    clearLayout(m_quickAccessLayout);

    struct QAEntry {
        QString name;
        QString path;
        QString icon;
    };

    QList<QAEntry> entries = {
        {"Home Directory", QDir::homePath(), "🏠"},
        {"Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "📥"},
        {"Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "📄"},
        {"Music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "🎵"},
        {"Videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), "🎬"},
        {"Pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), "🖼️"},
        {"Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "🖥️"}
    };

    int row = 0;
    int col = 0;

    for (const auto& entry : entries) {
        if (entry.path.isEmpty() || !QDir(entry.path).exists()) continue;

        ClickableCardFrame* card = new ClickableCardFrame(entry.path, -1, this);
        connect(card, &ClickableCardFrame::doubleClicked, this, &HomeDashboardWidget::onQuickAccessClicked);

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        QLabel* iconLabel = new QLabel(card);
        iconLabel->setText(entry.icon);
        iconLabel->setStyleSheet("font-size: 20px; color: #fab387;");
        layout->addWidget(iconLabel);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(1);

        QLabel* nameLabel = new QLabel(entry.name, card);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #cdd6f4;");
        details->addWidget(nameLabel);

        QLabel* pathLabel = new QLabel(QDir::toNativeSeparators(entry.path), card);
        pathLabel->setStyleSheet("font-size: 9px; color: #a6adc8;");
        details->addWidget(pathLabel);

        layout->addLayout(details, 1);

        m_quickAccessLayout->addWidget(card, row, col);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }
}

void HomeDashboardWidget::populatePinnedFolders() {
    clearLayout(m_pinnedLayout);

    QSettings settings("Amifiles", "Amifiles");
    QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();

    if (pinned.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No folders pinned yet. Right-click any directory in standard view and click '📌 Pin to Home Screen' to create shortcuts with view memory.", this);
        emptyLabel->setStyleSheet("color: #a6adc8; font-size: 11px; font-style: italic; padding: 10px;");
        m_pinnedLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int row = 0;
    int col = 0;

    QStringList viewModeNames = {
        "Details Table", "Grid / Icons", "Card / Tiles", "Miller Columns",
        "Chronological Timeline", "Filmstrip View", "Audio Showcase",
        "Video Showcase", "Movies Full Screen", "TV Shows Full Screen", "Music Full Screen"
    };

    for (const QString& item : pinned) {
        QStringList parts = item.split(';');
        if (parts.size() < 2) continue;

        QString path = parts[0];
        QString name = parts[1];
        int layoutIndex = 0;
        if (parts.size() >= 3) {
            layoutIndex = parts[2].toInt();
        }

        ClickableCardFrame* card = new ClickableCardFrame(path, layoutIndex, this);
        connect(card, &ClickableCardFrame::doubleClickedWithLayout, this, &HomeDashboardWidget::onPinnedFolderClicked);

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(8);

        QLabel* iconLabel = new QLabel("📌", card);
        iconLabel->setStyleSheet("font-size: 18px;");
        layout->addWidget(iconLabel);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(1);

        QLabel* nameLabel = new QLabel(name, card);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #a6e3a1;");
        details->addWidget(nameLabel);

        QString layoutName = "Details View";
        if (layoutIndex >= 0 && layoutIndex < viewModeNames.size()) {
            layoutName = viewModeNames[layoutIndex];
        }
        QLabel* layoutBadge = new QLabel(QString("Layout: %1").arg(layoutName), card);
        layoutBadge->setStyleSheet("font-size: 9px; color: #89dceb; font-weight: bold;");
        details->addWidget(layoutBadge);

        QLabel* pathLabel = new QLabel(QDir::toNativeSeparators(path), card);
        pathLabel->setStyleSheet("font-size: 9px; color: #a6adc8;");
        details->addWidget(pathLabel);

        layout->addLayout(details, 1);

        // Delete/Unpin button
        QPushButton* btnUnpin = new QPushButton("✖", card);
        btnUnpin->setProperty("class", "unpinButton");
        btnUnpin->setCursor(Qt::PointingHandCursor);
        btnUnpin->setToolTip("Unpin Folder");
        btnUnpin->setFixedSize(18, 18);
        connect(btnUnpin, &QPushButton::clicked, this, [this, path]() { onUnpinFolderClicked(path); });
        layout->addWidget(btnUnpin);

        m_pinnedLayout->addWidget(card, row, col);

        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
}

void HomeDashboardWidget::onDriveDoubleClicked(const QString& path) {
    emit navigateRequested(path);
}

void HomeDashboardWidget::onQuickAccessClicked(const QString& path) {
    emit navigateRequested(path);
}

void HomeDashboardWidget::onPinnedFolderClicked(const QString& path, int layoutIndex) {
    emit navigateWithLayoutRequested(path, layoutIndex);
}

void HomeDashboardWidget::onUnpinFolderClicked(const QString& path) {
    QSettings settings("Amifiles", "Amifiles");
    QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
    for (int i = 0; i < pinned.size(); ++i) {
        if (pinned[i].startsWith(path + ";")) {
            pinned.removeAt(i);
            break;
        }
    }
    settings.setValue("dashboard/pinned_folders", pinned);
    populatePinnedFolders();
}

void HomeDashboardWidget::onToolButtonClicked(const QString& action) {
    // Traverse parent widgets to retrieve MainWindow
    QWidget* parentW = parentWidget();
    while (parentW && !parentW->inherits("MainWindow")) {
        parentW = parentW->parentWidget();
    }
    MainWindow* mw = qobject_cast<MainWindow*>(parentW);
    if (!mw) return;

    if (action == "bulk_rename") {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Bulk Rename", QDir::homePath(), "All Files (*)");
        if (!files.isEmpty()) {
            BulkRenameDialog dlg(files, this);
            dlg.exec();
        }
    } else if (action == "video_scraper") {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Videos to Scrape Metadata", QDir::homePath(), "Video Files (*.mp4 *.mkv *.avi *.mov *.webm *.mpeg *.mpg)");
        if (!files.isEmpty()) {
            VideoScraperDialog dlg(files, this);
            dlg.exec();
        }
    } else if (action == "archive_creator") {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Compress", QDir::homePath(), "All Files (*)");
        if (!files.isEmpty()) {
            ArchiveDialog dlg(ArchiveDialog::ModeCreate, files, QDir::homePath(), false, this);
            dlg.exec();
        }
    } else if (action == "visual_diff") {
        QString file1 = QFileDialog::getOpenFileName(this, "Select First File for Comparison", QDir::homePath(), "All Files (*)");
        if (!file1.isEmpty()) {
            QString file2 = QFileDialog::getOpenFileName(this, "Select Second File for Comparison", QFileInfo(file1).absolutePath(), "All Files (*)");
            if (!file2.isEmpty()) {
                // VisualDiffDialog is a widget or dialog?
                // Let's invoke the visual diff through the MainWindow slot or QMetaObject
                QMetaObject::invokeMethod(mw, "onCompareSyncAction");
            }
        }
    } else if (action == "dup_finder") {
        QMetaObject::invokeMethod(mw, "onDuplicateFinderAction");
    } else if (action == "preferences") {
        QMetaObject::invokeMethod(mw, "onOpenPreferences");
    }
}

#include "homedashboardwidget.moc"
