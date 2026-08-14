#include "filepanel.h"
#include "tagmanager.h"
#include "metadatahovercard.h"
static bool isPathLockedPersistent(const QString& path);
#include <QWidgetAction>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include "mainwindow.h"
#include "remotemountmanager.h"
#include <QDebug>
#include "theme.h"
#include "favoritesmanager.h"
#include "archivemodel.h"
#include "smartfoldermodel.h"
#include "diskdashboardwidget.h"
#include "homedashboardwidget.h"
#include "millercolumnsview.h"
#include "timelineview.h"
#include "filmstripview.h"
#include "coverflowview.h"
#include "cardviewdelegate.h"
#include "groupproxymodel.h"
#include "columnscustomizerdialog.h"
#include <QComboBox>
#include "searchworker.h"
#include "metadataextractor.h"
#include "metadatacasingdialog.h"
#include "diskspaceanalyzerdialog.h"
#include "bulkrename.h"
#include "diffdialog.h"
#include "tageditordialog.h"
#include "advancedtageditordialog.h"
#include "filetagsdialog.h"
#include "comicbookviewerdialog.h"
#include "iconpickerdialog.h"
#include "theaterviewdelegate.h"
#include "renameitemdelegate.h"
#include "theaterlistview.h"
#include "videoscraperdialog.h"
#include "advancednewfolderdialog.h"
#include <QDirIterator>
#include <QGuiApplication>
#include "pathbarwidget.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include "folderartscraperdialog.h"
#include <QProcess>
#include "showcaseinfodialog.h"
#include "showcasesettingsdialog.h"
#include <QAbstractItemView>
#include <QScreen>
#include <QHelpEvent>
#include "copyqueue.h"
#include "archivedialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QScrollArea>
#include <QPushButton>
#include <QHeaderView>
#include <QEvent>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>
#include <QTextStream>
#include <QTextBrowser>
#include <QStyle>
#include <QApplication>
#include <QDir>
#include <QSet>

static void scanMediaFilesRecursively(const QString& folderPath, QStringList& playlistPaths, int mediaTypeFilter = 0, int depth = 0);
static bool hasAudioFilesRecursively(const QString& folderPath, int depth = 0);
#include <QFileInfo>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include "checksumdialog.h"
#include "shreddialog.h"
#include "imageconverterdialog.h"
#include "vaultdialog.h"
#include <QMessageBox>
#include <QCheckBox>
#include <QInputDialog>
#include <QLabel>
#include <QDialog>
#include <QClipboard>
#include <QMimeData>
#include <QButtonGroup>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

static int comboIndexToInternal(int idx) {
    if (idx >= 0 && idx <= 5) return idx;
    if (idx == 6) return 8;  // Movies Full Screen
    if (idx == 7) return 9;  // TV Shows Full Screen
    if (idx == 8) return 10; // Music Full Screen
    if (idx == 9) return 11; // Cover Flow Carousel
    return 0;
}

static int internalToComboIndex(int idx) {
    if (idx >= 0 && idx <= 5) return idx;
    if (idx == 8) return 6;
    if (idx == 9) return 7;
    if (idx == 10) return 8;
    if (idx == 11) return 9;
    return 0;
}

static QIcon createSearchIcon(const QColor& color) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    
    p.drawEllipse(4, 4, 10, 10);
    p.drawLine(12, 12, 19, 19);
    
    p.end();
    return QIcon(pix);
}

static QIcon createFilterIcon(const QColor& color) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(color);
    
    QPolygonF funnel;
    funnel << QPointF(4, 5) << QPointF(20, 5) << QPointF(14, 12) << QPointF(14, 18) << QPointF(10, 20) << QPointF(10, 12);
    p.drawPolygon(funnel);
    
    p.end();
    return QIcon(pix);
}

FilePanel::FilePanel(const QString& initialPath, QWidget* parent)
    : QWidget(parent) {
    setupUI();
    
    // Connect to FavoritesManager changes to update UI
    connect(&FavoritesManager::instance(), &FavoritesManager::favoritesChanged,
            this, &FilePanel::updateFavoritesUI);

    // Asynchronously update UI when folder sizes finish calculating
    connect(&FolderSizeCalculator::instance(), &FolderSizeCalculator::sizeCalculated, this, [this](const QString& path, qint64 size) {
        Q_UNUSED(size);
        if (m_fileModel && m_proxyModel) {
            QModelIndex srcIndex = m_fileModel->index(path);
            if (srcIndex.isValid()) {
                QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
                if (proxyIndex.isValid()) {
                    QModelIndex sizeIndex = m_proxyModel->index(proxyIndex.row(), 1, proxyIndex.parent());
                    if (sizeIndex.isValid()) {
                        emit m_proxyModel->dataChanged(sizeIndex, sizeIndex, {Qt::DisplayRole});
                    }
                }
            }
        }
    });

    // Auto-refresh when background file copy/move tasks finish
    connect(CopyQueueManager::instance().worker(), &CopyQueueWorker::jobFinished, this, [this](const QString& src, const QString& dest, bool success) {
        Q_UNUSED(src);
        Q_UNUSED(dest);
        Q_UNUSED(success);
        refresh();
    });

    m_searchUpdateTimer = new QTimer(this);
    m_searchUpdateTimer->setInterval(150);
    m_searchUpdateTimer->setSingleShot(true);
    connect(m_searchUpdateTimer, &QTimer::timeout, this, &FilePanel::onSearchUpdateTimeout);

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
        QSettings settings("Amifiles", "Amifiles");
        if (!settings.value("preview/show_metadata_hover_card", true).toBool()) {
            return;
        }
        if (!m_pendingHoverIndex.isValid()) return;
        QString filePath = filePathFromIndex(m_pendingHoverIndex);
        if (!filePath.isEmpty() && QFile::exists(filePath)) {
            FileMetadata meta = MetadataExtractor::extract(filePath);
            
            if (!m_hoverCard) {
                m_hoverCard = new MetadataHoverCard(nullptr);
            }
            m_hoverCard->setMetadata(meta, filePath);
            m_hoverCard->adjustSize();
            
            QPoint pos = m_pendingHoverPos;
            pos.setX(pos.x() + 15);
            pos.setY(pos.y() + 15);
            
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) {
                QRect screenGeom = screen->geometry();
                if (pos.x() + m_hoverCard->width() > screenGeom.right()) {
                    pos.setX(m_pendingHoverPos.x() - m_hoverCard->width() - 15);
                }
                if (pos.y() + m_hoverCard->height() > screenGeom.bottom()) {
                    pos.setY(m_pendingHoverPos.y() - m_hoverCard->height() - 15);
                }
            }
            m_hoverCard->move(pos);
            m_hoverCard->show();
            m_hoverCard->raise();
        }
    });

    navigateTo(initialPath, true);
}

FilePanel::~FilePanel() {
    if (m_searchWorker) {
        m_searchWorker->cancel();
    }
    if (m_searchThread) {
        m_searchThread->quit();
        m_searchThread->wait();
    }
    if (m_hoverCard) {
        m_hoverCard->deleteLater();
    }
}

void FilePanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top Navigation Bar
    m_navContainer = new QWidget(this);
    QHBoxLayout* navLayout = new QHBoxLayout(m_navContainer);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(2);

    QStyle* style = QApplication::style();

    m_btnBack = new QToolButton(this);
    m_btnBack->setIcon(style->standardIcon(QStyle::SP_ArrowBack));
    m_btnBack->setToolTip("Back");
    connect(m_btnBack, &QToolButton::clicked, this, &FilePanel::onNavigateBack);

    m_btnForward = new QToolButton(this);
    m_btnForward->setIcon(style->standardIcon(QStyle::SP_ArrowForward));
    m_btnForward->setToolTip("Forward");
    connect(m_btnForward, &QToolButton::clicked, this, &FilePanel::onNavigateForward);

    m_btnUp = new QToolButton(this);
    m_btnUp->setIcon(style->standardIcon(QStyle::SP_FileDialogToParent));
    m_btnUp->setToolTip("Up to parent directory");
    connect(m_btnUp, &QToolButton::clicked, this, &FilePanel::onNavigateUp);

    m_pathBar = new PathBarWidget(this);
    m_pathEdit = m_pathBar->lineEdit();
    connect(m_pathBar, &PathBarWidget::pathEntered, this, [this](const QString& path) {
        navigateTo(path);
    });

    m_btnGo = new QToolButton(this);
    m_btnGo->setIcon(style->standardIcon(QStyle::SP_BrowserReload));
    m_btnGo->setToolTip("Go");
    connect(m_btnGo, &QToolButton::clicked, this, &FilePanel::onPathEntered);

    m_btnFavorite = new QToolButton(this);
    m_btnFavorite->setText("☆");
    m_btnFavorite->setToolTip("Add/Remove Favorite");
    m_btnFavorite->setStyleSheet("QToolButton { font-size: 16px; font-weight: bold; color: #f9e2af; }");
    connect(m_btnFavorite, &QToolButton::clicked, this, &FilePanel::onFavoriteClicked);

    m_btnHome = new QToolButton(this);
    m_btnHome->setIcon(style->standardIcon(QStyle::SP_DirHomeIcon));
    m_btnHome->setToolTip("Go Home (Right-click for options)");
    m_btnHome->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_btnHome, &QToolButton::clicked, this, &FilePanel::onHomeClicked);
    connect(m_btnHome, &QToolButton::customContextMenuRequested, this, &FilePanel::onHomeContextMenu);

    m_btnClonePath = new QToolButton(this);
    m_btnClonePath->setIcon(style->standardIcon(QStyle::SP_ArrowRight));
    m_btnClonePath->setToolTip("Clone current path to opposite sibling panel");
    connect(m_btnClonePath, &QToolButton::clicked, this, &FilePanel::onClonePathClicked);

    m_btnFlatView = new QToolButton(this);
    m_btnFlatView->setCheckable(true);
    m_btnFlatView->setIcon(style->standardIcon(QStyle::SP_DirIcon));
    m_btnFlatView->setToolTip("Enable Flat View (Recurse all subfolders)");
    m_btnFlatView->setStyleSheet(
        "QToolButton {"
        "  font-weight: bold;"
        "  color: #a6e3a1;"
        "  background-color: transparent;"
        "  border: 1px solid #a6e3a1;"
        "  border-radius: 4px;"
        "  padding: 2px 6px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #313244;"
        "}"
        "QToolButton:checked {"
        "  background-color: #a6e3a1;"
        "  color: #11111b;"
        "}"
    );
    connect(m_btnFlatView, &QToolButton::toggled, this, &FilePanel::setFlatViewEnabled);
    m_comboViewMode = new QComboBox(this);
    m_comboViewMode->addItems({
        "Details Table",
        "Grid / Icons",
        "Card / Tiles",
        "Miller Columns",
        "Chronological Timeline",
        "Filmstrip View",
        "Movies Full Screen",
        "TV Shows Full Screen",
        "Music Full Screen",
        "Cover Flow Carousel"
    });
    m_comboViewMode->setToolTip("Switch active file listing visual layout view mode");
    m_comboViewMode->setStyleSheet("QComboBox { background-color: #313244; color: #89b4fa; border: 1px solid #45475a; border-radius: 4px; padding: 2px 6px; font-weight: bold; }");
    connect(m_comboViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilePanel::onViewModeChanged);

    m_btnToggleSidePane = new QToolButton(this);
    m_btnToggleSidePane->setText("📋 Playlist");
    m_btnToggleSidePane->setCheckable(true);
    m_btnToggleSidePane->setChecked(false);
    m_btnToggleSidePane->setToolTip("Toggle Playlist Drawer");
    m_btnToggleSidePane->setStyleSheet("QToolButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 6px; font-weight: bold; } QToolButton:hover { background-color: #45475a; } QToolButton:checked { background-color: #a6e3a1; color: #11111b; }");
    connect(m_btnToggleSidePane, &QToolButton::toggled, this, &FilePanel::onToggleSidePane);

    m_playlistCollapseTimer = new QTimer(this);
    m_playlistCollapseTimer->setSingleShot(true);
    connect(m_playlistCollapseTimer, &QTimer::timeout, this, [this]() {
        if (m_btnToggleSidePane && m_btnToggleSidePane->isChecked()) {
            m_btnToggleSidePane->setChecked(false);
        }
    });

    navLayout->addWidget(m_btnBack);
    navLayout->addWidget(m_btnForward);
    navLayout->addWidget(m_btnUp);
    navLayout->addWidget(m_btnHome);
    navLayout->addWidget(m_btnFavorite);
    navLayout->addWidget(m_pathBar, 1);
    navLayout->addWidget(m_btnGo);
    navLayout->addWidget(m_btnClonePath);
    navLayout->addWidget(m_btnFlatView);
    navLayout->addWidget(m_comboViewMode);
    navLayout->addWidget(m_btnToggleSidePane);

    // Central Tree View
    m_treeView = new QTreeView(this);
    m_treeView->setMinimumHeight(50);
    m_treeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_treeView->setItemDelegate(new RenameItemDelegate(m_treeView));
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSortingEnabled(true);
    QSettings prefSettings("Amifiles", "Amifiles");
    bool detailsFullRowSelect = prefSettings.value("preferences/details_full_row_select", true).toBool();
    m_treeView->setSelectionBehavior(detailsFullRowSelect ? QAbstractItemView::SelectRows : QAbstractItemView::SelectItems);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu); // Enable context menu
    m_treeView->installEventFilter(this); // Install event filter to capture focus events
    if (m_treeView->viewport()) m_treeView->viewport()->installEventFilter(this);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragDrop);

    // Icon Grid List View
    m_listView = new QListView(this);
    m_listView->setMinimumHeight(50);
    m_listView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_defaultDelegate = new RenameItemDelegate(m_listView);
    m_listView->setItemDelegate(m_defaultDelegate);
    m_cardDelegate = new CardViewDelegate(m_listView);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setWordWrap(true);
    m_listView->setUniformItemSizes(true);
    m_listView->setSpacing(10);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->installEventFilter(this);
    if (m_listView->viewport()) m_listView->viewport()->installEventFilter(this);
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    m_listView->setDragDropMode(QAbstractItemView::DragDrop);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->setMinimumHeight(50);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->addWidget(m_listView);

    // Global Search UI Setup

    m_searchResultsView = new QListView(this);
    m_searchResultsView->setMinimumHeight(50);
    m_searchResultsView->setVisible(false);
    m_searchResultsView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_searchResultsView->installEventFilter(this);
    m_searchResultModel = new QStringListModel(this);
    m_searchResultsView->setModel(m_searchResultModel);
    connect(m_searchResultsView, &QListView::clicked, this, &FilePanel::onSearchResultSelected);
    connect(m_searchResultsView, &QListView::doubleClicked, this, &FilePanel::onSearchResultDoubleClicked);
    connect(m_searchResultsView, &QListView::customContextMenuRequested, this, &FilePanel::onSearchContextMenu);

    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(300);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &FilePanel::startSearch);

    m_searchThread = new QThread(this);
    m_searchWorker = new SearchWorker();
    m_searchWorker->moveToThread(m_searchThread);
    connect(m_searchThread, &QThread::finished, m_searchWorker, &QObject::deleteLater);
    connect(this, &FilePanel::sigStartSearch, m_searchWorker, &SearchWorker::doSearch);
    connect(m_searchWorker, &SearchWorker::resultsReady, this, &FilePanel::onSearchResultsReady);
    connect(m_searchWorker, &SearchWorker::searchFinished, this, &FilePanel::onSearchFinished);
    m_searchThread->start();

    // File Model & Proxy Model Setup
    m_fileModel = new CustomFileSystemModel(this);
    m_fileModel->setReadOnly(false);
    updateFileSystemFilters();
    m_fileModel->setRootPath("");
    connect(m_fileModel, &QFileSystemModel::fileRenamed, this, [](const QString& path, const QString& oldName, const QString& newName) {
        Q_UNUSED(oldName);
        QString parentDir = QFileInfo(path).absolutePath();
        QString newFilePath = QDir(parentDir).filePath(newName);
        QFile(newFilePath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
    });

    connect(m_fileModel, &QFileSystemModel::directoryLoaded, this, [this](const QString& path) {
        if (path == m_currentPath) {
            focusFirstItemInActiveView();
        }
    });

    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileModel);

    updateHideSettings();

    m_rebuildGroupsTimer = new QTimer(this);
    m_rebuildGroupsTimer->setSingleShot(true);
    connect(m_rebuildGroupsTimer, &QTimer::timeout, this, &FilePanel::rebuildTheaterGroups);

    m_groupProxy = new GroupProxyModel(this);
    m_groupProxy->setSourceModel(m_proxyModel);
    connect(m_groupProxy, &QAbstractItemModel::modelReset, this, &FilePanel::queueRebuildTheaterGroups);

    m_flatModel = new FlatFileSystemModel(this);
    m_flatProxyModel = new FileFilterProxyModel(this);
    m_flatProxyModel->setSourceModel(m_flatModel);
    m_flatProxyModel->setFilterKeyColumn(0);
    m_flatProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_smartModel = new SmartFolderModel(this);
    m_archiveModel = new ArchiveModel(this);
    m_dashboardWidget = new DiskDashboardWidget(this);
    m_homeDashboardWidget = new HomeDashboardWidget(this);
    connect(m_homeDashboardWidget, &HomeDashboardWidget::navigateRequested, this, [this](const QString& path) {
        navigateTo(path);
    });
    connect(m_homeDashboardWidget, &HomeDashboardWidget::navigateWithLayoutRequested, this, [this](const QString& path, int layoutIdx) {
        navigateTo(path);
        
        // Traverse parents to find MainWindow and check if a custom folder profile rule was applied
        QWidget* parentW = parentWidget();
        while (parentW && !parentW->inherits("MainWindow")) {
            parentW = parentW->parentWidget();
        }
        MainWindow* mw = qobject_cast<MainWindow*>(parentW);
        if (mw) {
            if (mw->hasActiveFolderRule()) {
                QString ruleName = mw->activeFolderRule().name.toLower();
                if (ruleName != "default" && !ruleName.isEmpty()) {
                    // Custom folder profile matched and was applied, so obey it instead of pinned layout memory
                    return;
                }
            }
        }
        
        setViewModeIndex(layoutIdx);
    });
    connect(m_homeDashboardWidget, &HomeDashboardWidget::applyProfileRequested, this, [this](const QString& profileName) {
        QWidget* parentW = parentWidget();
        while (parentW && !parentW->inherits("MainWindow")) {
            parentW = parentW->parentWidget();
        }
        MainWindow* mw = qobject_cast<MainWindow*>(parentW);
        if (mw) {
            FolderLayoutRule targetRule;
            bool found = false;
            for (const auto& r : mw->folderRules()) {
                if (r.name.trimmed().compare(profileName, Qt::CaseInsensitive) == 0) {
                    targetRule = r;
                    found = true;
                    break;
                }
            }
            if (found) {
                if (targetRule.ruleType == "Path" && !targetRule.value.isEmpty()) {
                    mw->setApplyingFolderProfile(true);
                    setPath(targetRule.value);
                    mw->applyProfile(targetRule, this);
                } else {
                    mw->applyProfile(targetRule, this);
                }
            }
        }
    });
    connect(m_homeDashboardWidget, &HomeDashboardWidget::backRequested, this, &FilePanel::onNavigateBack);
    m_theaterListView = new TheaterListView(this);
    m_theaterListView->setMinimumHeight(50);
    m_theaterListView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_theaterListView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_theaterDelegate = new TheaterViewDelegate(m_theaterListView);
    m_theaterListView->setItemDelegate(m_theaterDelegate);
    m_theaterListView->installEventFilter(this);
    if (m_theaterListView->viewport()) m_theaterListView->viewport()->installEventFilter(this);
    m_theaterListView->setDragEnabled(false);
    m_theaterListView->setAcceptDrops(false);
    m_theaterListView->setDropIndicatorShown(false);
    m_theaterListView->setDragDropMode(QAbstractItemView::NoDragDrop);

    // Slide-out tracks drawer container
    m_theaterContainer = new QWidget(this);
    m_theaterContainer->setMinimumHeight(50);
    QVBoxLayout* theaterLayout = new QVBoxLayout(m_theaterContainer);
    theaterLayout->setContentsMargins(0, 0, 0, 0);
    theaterLayout->setSpacing(0);

    // Upper container with QSplitter for resizable views and side info panel
    m_theaterSplitter = new QSplitter(Qt::Horizontal, m_theaterContainer);
    m_theaterSplitter->setStyleSheet("QSplitter::handle { background-color: #313244; width: 4px; } QSplitter::handle:hover { background-color: #89b4fa; }");

    m_theaterSplitter->addWidget(m_theaterListView);

    m_theaterScrollArea = new QScrollArea(m_theaterSplitter);
    m_theaterScrollArea->setWidgetResizable(true);
    m_theaterScrollArea->setFrameShape(QFrame::NoFrame);
    m_theaterScrollArea->setStyleSheet("QScrollArea { background: transparent; }");
    m_theaterScrollArea->setVisible(false);

    m_theaterScrollWidget = new QWidget(m_theaterScrollArea);
    m_theaterScrollWidget->setObjectName("theaterScrollWidget");
    m_theaterScrollWidget->setStyleSheet("QWidget#theaterScrollWidget { background: transparent; }");
    m_theaterScrollLayout = new QVBoxLayout(m_theaterScrollWidget);
    m_theaterScrollLayout->setContentsMargins(0, 0, 0, 0);
    m_theaterScrollLayout->setSpacing(10);
    m_theaterScrollLayout->addStretch(1);
    m_theaterScrollArea->setWidget(m_theaterScrollWidget);

    m_theaterSplitter->addWidget(m_theaterScrollArea);

    m_theaterSideContainer = new QWidget(m_theaterSplitter);
    QVBoxLayout* sideLayout = new QVBoxLayout(m_theaterSideContainer);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    m_theaterSplitter->addWidget(m_theaterSideContainer);
    m_theaterSplitter->setStretchFactor(0, 7);
    m_theaterSplitter->setStretchFactor(1, 7);
    m_theaterSplitter->setStretchFactor(2, 3);

    theaterLayout->addWidget(m_theaterSplitter, 1);

    // Bottom Info Panel
    m_bottomInfoPanel = new QWidget(m_theaterContainer);
    m_bottomInfoPanel->setFixedHeight(72);
    m_bottomInfoPanel->setStyleSheet("QWidget { background-color: #11111b; border-top: 1px solid #313244; } QLabel { border: none; background: transparent; }");
    m_bottomInfoPanel->setVisible(false);

    QHBoxLayout* bottomLayout = new QHBoxLayout(m_bottomInfoPanel);
    bottomLayout->setContentsMargins(12, 6, 12, 6);
    bottomLayout->setSpacing(12);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_bottomTitle = new QLabel(m_bottomInfoPanel);
    m_bottomTitle->setStyleSheet("font-weight: bold; font-size: 13px; color: #cdd6f4;");

    m_bottomMeta = new QLabel(m_bottomInfoPanel);
    m_bottomMeta->setStyleSheet("font-size: 11px; color: #a6adc8;");

    textLayout->addWidget(m_bottomTitle);
    textLayout->addWidget(m_bottomMeta);

    // Initialize Music v2 Playback Controls Bar
    m_musicControlsWidget = new QWidget(m_bottomInfoPanel);
    QHBoxLayout* musicCtrlLayout = new QHBoxLayout(m_musicControlsWidget);
    musicCtrlLayout->setContentsMargins(0, 4, 0, 0);
    musicCtrlLayout->setSpacing(6);

    m_btnShuffle = new QToolButton(m_musicControlsWidget);
    m_btnShuffle->setText("🔀");
    m_btnShuffle->setCheckable(true);
    m_btnShuffle->setToolTip("Shuffle Playback");
    m_btnShuffle->setFixedSize(28, 28);
    
    m_btnPrev = new QToolButton(m_musicControlsWidget);
    m_btnPrev->setText("⏮");
    m_btnPrev->setToolTip("Previous Track");
    m_btnPrev->setFixedSize(28, 28);

    m_btnPlayPause = new QToolButton(m_musicControlsWidget);
    m_btnPlayPause->setText("▶");
    m_btnPlayPause->setToolTip("Play / Pause");
    m_btnPlayPause->setFixedSize(30, 30);

    m_btnNext = new QToolButton(m_musicControlsWidget);
    m_btnNext->setText("⏭");
    m_btnNext->setToolTip("Next Track");
    m_btnNext->setFixedSize(28, 28);

    m_btnRepeat = new QToolButton(m_musicControlsWidget);
    m_btnRepeat->setText("🔁");
    m_btnRepeat->setCheckable(true);
    m_btnRepeat->setToolTip("Repeat Album");
    m_btnRepeat->setFixedSize(28, 28);

    m_musicVolumeSlider = new QSlider(Qt::Horizontal, m_musicControlsWidget);
    m_musicVolumeSlider->setRange(0, 100);
    m_musicVolumeSlider->setValue(85);
    m_musicVolumeSlider->setFixedWidth(60);
    m_musicVolumeSlider->setToolTip("Volume");

    m_musicProgressLabel = new QLabel("00:00 / 00:00", m_musicControlsWidget);
    m_musicProgressLabel->setStyleSheet("color: #bac2de; font-size: 11px; margin-left: 6px; font-weight: bold;");

    musicCtrlLayout->addWidget(m_btnShuffle);
    musicCtrlLayout->addWidget(m_btnPrev);
    musicCtrlLayout->addWidget(m_btnPlayPause);
    musicCtrlLayout->addWidget(m_btnNext);
    musicCtrlLayout->addWidget(m_btnRepeat);
    musicCtrlLayout->addSpacing(4);
    musicCtrlLayout->addWidget(m_musicVolumeSlider);
    musicCtrlLayout->addWidget(m_musicProgressLabel);
    musicCtrlLayout->addStretch(1);

    QString toolBtnStyle = "QToolButton { background-color: #313244; color: #cdd6f4; border: none; padding: 4px; border-radius: 4px; } QToolButton:hover { background-color: #45475a; } QToolButton:checked { background-color: #fab387; color: #11111b; }";
    m_btnShuffle->setStyleSheet(toolBtnStyle);
    m_btnPrev->setStyleSheet(toolBtnStyle);
    m_btnPlayPause->setStyleSheet("QToolButton { background-color: #a6e3a1; color: #11111b; border: none; padding: 6px; border-radius: 12px; font-weight: bold; } QToolButton:hover { background-color: #94e2d5; }");
    m_btnNext->setStyleSheet(toolBtnStyle);
    m_btnRepeat->setStyleSheet(toolBtnStyle);
    m_musicVolumeSlider->setStyleSheet("QSlider::groove:horizontal { border: none; height: 4px; background: #313244; border-radius: 2px; } QSlider::handle:horizontal { background: #89b4fa; width: 10px; margin: -3px 0; border-radius: 5px; }");

    textLayout->addWidget(m_musicControlsWidget);
    m_musicControlsWidget->setVisible(false);

    bottomLayout->addLayout(textLayout, 3);

    m_bottomSynopsis = new QLabel(m_bottomInfoPanel);
    m_bottomSynopsis->setStyleSheet("font-size: 11px; color: #bac2de;");
    m_bottomSynopsis->setWordWrap(true);
    m_bottomSynopsis->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    bottomLayout->addWidget(m_bottomSynopsis, 5);

    // Initialize Audio Visualizer in the center
    m_visualizerWidget = new AudioVisualizerWidget(m_bottomInfoPanel);
    bottomLayout->addWidget(m_visualizerWidget, 2, Qt::AlignVCenter);
    m_visualizerWidget->setVisible(false);

    // Initialize Playlist Track Drawer on the right (placed inside theaterSideContainer)
    m_trackListWidget = new QListWidget(m_theaterSideContainer);
    m_trackListWidget->setStyleSheet("QListWidget { background-color: rgba(24, 24, 37, 150); color: #cdd6f4; border: none; font-size: 12px; } QListWidget::item { padding: 8px 10px; border-radius: 4px; } QListWidget::item:hover { background-color: #313244; color: #f5c2e7; } QListWidget::item:selected { background-color: #89b4fa; color: #11111b; }");
    m_trackListWidget->installEventFilter(this);
    if (m_theaterSideContainer && m_theaterSideContainer->layout()) {
        m_theaterSideContainer->layout()->addWidget(m_trackListWidget);
        
        m_drawerBtnContainer = new QWidget(m_theaterSideContainer);
        QHBoxLayout* drawerBtnLayout = new QHBoxLayout(m_drawerBtnContainer);
        drawerBtnLayout->setContentsMargins(6, 6, 12, 6);
        drawerBtnLayout->setSpacing(6);
        
        QPushButton* btnDrawerPlay = new QPushButton("▶ Play", m_drawerBtnContainer);
        QPushButton* btnDrawerRemove = new QPushButton("✖ Remove", m_drawerBtnContainer);
        QPushButton* btnDrawerClear = new QPushButton("🗑 Clear", m_drawerBtnContainer);

        btnDrawerPlay->setFixedWidth(78);
        btnDrawerRemove->setFixedWidth(78);
        btnDrawerClear->setFixedWidth(78);
        
        btnDrawerPlay->setFixedHeight(26);
        btnDrawerRemove->setFixedHeight(26);
        btnDrawerClear->setFixedHeight(26);
        
        QString btnStyle = 
            "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 4px; font-weight: bold; font-family: 'Outfit'; font-size: 10px; } "
            "QPushButton:hover { background-color: #45475a; color: #ffffff; } "
            "QPushButton:pressed { background-color: #585b70; }";
            
        btnDrawerPlay->setStyleSheet(btnStyle);
        btnDrawerRemove->setStyleSheet(btnStyle);
        btnDrawerClear->setStyleSheet(btnStyle);
        
        drawerBtnLayout->addStretch(1);
        drawerBtnLayout->addWidget(btnDrawerPlay);
        drawerBtnLayout->addWidget(btnDrawerRemove);
        drawerBtnLayout->addWidget(btnDrawerClear);
        drawerBtnLayout->addStretch(1);
        
        m_theaterSideContainer->layout()->addWidget(m_drawerBtnContainer);
        m_drawerBtnContainer->setVisible(false);

        connect(btnDrawerPlay, &QPushButton::clicked, this, [this]() {
            QList<QListWidgetItem*> selected = m_trackListWidget->selectedItems();
            int idx = -1;
            if (!selected.isEmpty()) {
                idx = m_trackListWidget->row(selected.first());
            } else {
                idx = m_trackListWidget->currentRow();
            }
            if (idx < 0) idx = 0;
            
            QWidget* w = parentWidget();
            MainWindow* mainWin = nullptr;
            while (w) {
                MainWindow* mw = qobject_cast<MainWindow*>(w);
                if (mw) { mainWin = mw; break; }
                w = w->parentWidget();
            }
            if (mainWin && mainWin->previewPanel() && idx >= 0 && idx < mainWin->previewPanel()->playlist().size()) {
                mainWin->previewPanel()->playPlaylistIndex(idx);
                int vm = viewModeIndex();
                if ((vm == 8 || vm == 9) && !mainWin->previewPanel()->isFullscreen()) {
                    mainWin->previewPanel()->toggleFullscreen();
                }
            }
        });
        
        connect(btnDrawerRemove, &QPushButton::clicked, this, [this]() {
            QList<QListWidgetItem*> selected = m_trackListWidget->selectedItems();
            int idx = -1;
            if (!selected.isEmpty()) {
                idx = m_trackListWidget->row(selected.first());
            } else {
                idx = m_trackListWidget->currentRow();
            }
            QWidget* w = parentWidget();
            MainWindow* mainWin = nullptr;
            while (w) {
                MainWindow* mw = qobject_cast<MainWindow*>(w);
                if (mw) { mainWin = mw; break; }
                w = w->parentWidget();
            }
            if (mainWin && mainWin->previewPanel() && idx >= 0 && idx < mainWin->previewPanel()->playlist().size()) {
                mainWin->previewPanel()->removeFromPlaylist(idx);
            }
        });
        
        connect(btnDrawerClear, &QPushButton::clicked, this, [this]() {
            QWidget* w = parentWidget();
            MainWindow* mainWin = nullptr;
            while (w) {
                MainWindow* mw = qobject_cast<MainWindow*>(w);
                if (mw) { mainWin = mw; break; }
                w = w->parentWidget();
            }
            if (mainWin && mainWin->previewPanel()) {
                mainWin->previewPanel()->clearPlaylist();
            }
        });
    }
    m_trackListWidget->setVisible(false);

    // Playback control connections
    connect(m_btnPrev, &QToolButton::clicked, this, [this]() {
        if (viewModeIndex() == 10) {
            emit prevTrackRequested();
        } else {
            int row = m_trackListWidget->currentRow();
            if (row > 0) {
                m_trackListWidget->setCurrentRow(row - 1);
                QString path = m_trackListWidget->currentItem()->data(Qt::UserRole).toString();
                emit playMediaBuiltinRequested({path});
            }
        }
    });
    connect(m_btnNext, &QToolButton::clicked, this, [this]() {
        if (viewModeIndex() == 10) {
            emit nextTrackRequested();
        } else {
            int row = m_trackListWidget->currentRow();
            if (row >= 0 && row < m_trackListWidget->count() - 1) {
                m_trackListWidget->setCurrentRow(row + 1);
                QString path = m_trackListWidget->currentItem()->data(Qt::UserRole).toString();
                emit playMediaBuiltinRequested({path});
            }
        }
    });
    connect(m_btnPlayPause, &QToolButton::clicked, this, [this]() {
        if (viewModeIndex() == 10) {
            emit playPauseRequested();
        } else {
            if (m_btnPlayPause->text() == "▶") {
                m_btnPlayPause->setText("⏸");
                if (m_themePlayer) m_themePlayer->play();
            } else {
                m_btnPlayPause->setText("▶");
                if (m_themePlayer) m_themePlayer->pause();
            }
        }
    });
    connect(m_btnShuffle, &QToolButton::clicked, this, [this]() {
        if (viewModeIndex() == 10) {
            emit shuffleToggledRequested();
        }
    });
    connect(m_btnRepeat, &QToolButton::clicked, this, [this]() {
        if (viewModeIndex() == 10) {
            emit repeatClickedRequested();
        }
    });
    connect(m_musicVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (viewModeIndex() == 10) {
            emit volumeChangedRequested(value);
        } else {
            if (m_themeAudio) {
                m_themeAudio->setVolume((float)value / 100.0f);
            }
        }
    });
    connect(m_trackListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        QWidget* w = parentWidget();
        MainWindow* mainWin = nullptr;
        while (w) {
            MainWindow* mw = qobject_cast<MainWindow*>(w);
            if (mw) {
                mainWin = mw;
                break;
            }
            w = w->parentWidget();
        }
        if (mainWin && mainWin->previewPanel()) {
            int idx = m_trackListWidget->row(item);
            if (idx >= 0 && idx < mainWin->previewPanel()->playlist().size()) {
                mainWin->previewPanel()->playPlaylistIndex(idx);
                int vm = viewModeIndex();
                if ((vm == 8 || vm == 9 || vm == 10) && !mainWin->previewPanel()->isFullscreen()) {
                    mainWin->previewPanel()->toggleFullscreen();
                }
            }
        }
    });

    m_bottomPlayBtn = new QPushButton("▶ Play Media", m_bottomInfoPanel);
    m_bottomPlayBtn->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: none; min-width: 90px; } QPushButton:hover { background-color: #94e2d5; }");
    bottomLayout->addWidget(m_bottomPlayBtn, 0, Qt::AlignVCenter);

    m_bottomEnterBtn = new QPushButton("📂 Enter Folder", m_bottomInfoPanel);
    m_bottomEnterBtn->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: none; min-width: 90px; } QPushButton:hover { background-color: #b4befe; }");
    m_bottomEnterBtn->setVisible(false);
    bottomLayout->addWidget(m_bottomEnterBtn, 0, Qt::AlignVCenter);

    // Initialize Cinema Buttons Widget for Trailer & Metadata Edit
    m_cinemaButtonsWidget = new QWidget(m_bottomInfoPanel);
    QVBoxLayout* cinemaButtonsLayout = new QVBoxLayout(m_cinemaButtonsWidget);
    cinemaButtonsLayout->setContentsMargins(0, 0, 0, 0);
    cinemaButtonsLayout->setSpacing(4);

    m_btnWatchTrailer = new QPushButton("📺 Watch Trailer", m_cinemaButtonsWidget);
    m_btnWatchTrailer->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; padding: 4px 8px; border-radius: 4px; border: none; font-size: 11px; } QPushButton:hover { background-color: #b4befe; }");

    m_btnEditMetadata = new QPushButton("🏷 Edit Metadata", m_cinemaButtonsWidget);
    m_btnEditMetadata->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; font-weight: bold; padding: 4px 8px; border-radius: 4px; border: none; font-size: 11px; } QPushButton:hover { background-color: #45475a; }");

    cinemaButtonsLayout->addWidget(m_btnWatchTrailer);
    cinemaButtonsLayout->addWidget(m_btnEditMetadata);
    bottomLayout->addWidget(m_cinemaButtonsWidget, 0, Qt::AlignVCenter);
    m_cinemaButtonsWidget->setVisible(false);

    connect(m_btnWatchTrailer, &QPushButton::clicked, this, [this]() {
        if (m_bottomPanelPath.isEmpty()) return;
        QFileInfo info(m_bottomPanelPath);
        QString targetDir = info.isDir() ? m_bottomPanelPath : info.absolutePath();
        QDir dir(targetDir);
        QFileInfoList files = dir.entryInfoList(QDir::Files);
        QString trailerPath;
        for (const QFileInfo& fi : files) {
            if (fi.fileName().contains("trailer", Qt::CaseInsensitive)) {
                trailerPath = fi.absoluteFilePath();
                break;
            }
        }
        if (!trailerPath.isEmpty()) {
            if (viewModeIndex() == 8 || viewModeIndex() == 9 || viewModeIndex() == 10) {
                emit playMediaFullscreenRequested({trailerPath});
            } else {
                emit playMediaBuiltinRequested({trailerPath});
            }
        } else {
            QDesktopServices::openUrl(QUrl("https://www.youtube.com/results?search_query=" + QUrl::toPercentEncoding(info.baseName() + " trailer")));
        }
    });

    connect(m_btnEditMetadata, &QPushButton::clicked, this, [this]() {
        if (!m_bottomPanelPath.isEmpty()) {
            ShowcaseInfoDialog infoDlg(m_bottomPanelPath, this);
            connect(&infoDlg, &ShowcaseInfoDialog::playRequested, this, [this](const QString& path) {
                if (viewModeIndex() == 8 || viewModeIndex() == 9 || viewModeIndex() == 10) {
                    emit playMediaFullscreenRequested({path});
                } else {
                    emit playMediaBuiltinRequested({path});
                }
            });
            infoDlg.exec();
        }
    });

    theaterLayout->addWidget(m_bottomInfoPanel);

    connect(m_bottomEnterBtn, &QPushButton::clicked, this, [this]() {
        if (!m_bottomPanelPath.isEmpty() && QFileInfo(m_bottomPanelPath).isDir()) {
            navigateTo(m_bottomPanelPath, true);
        }
    });

    connect(m_bottomPlayBtn, &QPushButton::clicked, this, [this]() {
        if (m_bottomPanelPath.isEmpty()) return;
        QFileInfo info(m_bottomPanelPath);
        QStringList playlistPaths;
        if (info.isDir()) {
            int filter = 0;
            int vm = viewModeIndex();
            if (vm == 6 || vm == 10) filter = 1;
            else if (vm == 7 || vm == 8 || vm == 9) filter = 2;
            scanMediaFilesRecursively(m_bottomPanelPath, playlistPaths, filter);
        } else {
            playlistPaths.append(m_bottomPanelPath);
        }

        if (!playlistPaths.isEmpty()) {
            if (viewModeIndex() == 8 || viewModeIndex() == 9) {
                emit playMediaFullscreenRequested(playlistPaths);
            } else {
                emit playMediaBuiltinRequested(playlistPaths);
            }
        }
    });

    m_theaterContainer->installEventFilter(this);
    m_bottomInfoPanel->installEventFilter(this);

    m_millerView = new MillerColumnsView(m_fileModel, this);
    m_millerView->setMinimumHeight(50);
    m_millerView->installEventFilter(this);
    m_millerView->setAcceptDrops(true);
    if (m_millerView->viewport()) m_millerView->viewport()->installEventFilter(this);

    m_timelineView = new TimelineView(this);
    m_timelineView->setMinimumHeight(50);
    m_timelineView->installEventFilter(this);
    m_timelineView->setAcceptDrops(true);
    if (m_timelineView->viewport()) m_timelineView->viewport()->installEventFilter(this);

    m_filmstripView = new FilmstripView(m_fileModel, this);
    m_filmstripView->setMinimumHeight(50);
    m_filmstripView->installEventFilter(this);
    m_filmstripView->setAcceptDrops(true);

    m_coverFlowView = new CoverFlowView(this);
    m_coverFlowView->setModel(m_proxyModel);
    m_coverFlowView->setSelectionModel(m_treeView->selectionModel());
    m_coverFlowView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_coverFlowView, &CoverFlowView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);

    m_viewStack->addWidget(m_theaterContainer);
    m_viewStack->addWidget(m_millerView);
    m_viewStack->addWidget(m_timelineView);
    m_viewStack->addWidget(m_filmstripView);
    m_viewStack->addWidget(m_coverFlowView);
    m_viewStack->addWidget(m_dashboardWidget);
    m_viewStack->addWidget(m_homeDashboardWidget);

    connect(m_coverFlowView, &CoverFlowView::itemDoubleClicked, this, [this](const QModelIndex& index) {
        onDoubleClicked(index);
    });
    connect(m_coverFlowView, &CoverFlowView::currentIndexChanged, this, [this](int index) {
        if (m_proxyModel) {
            QModelIndex modelIdx = m_proxyModel->index(index, 0, m_listView->rootIndex());
            QString path = m_proxyModel->data(modelIdx, Qt::UserRole + 1).toString();
            emit fileSelected(path);
        }
    });

    connect(m_millerView, &MillerColumnsView::fileSelected, this, &FilePanel::fileSelected);
    connect(m_millerView, &MillerColumnsView::fileDoubleClicked, this, &FilePanel::onDoubleClickedPath);
    connect(m_millerView, &MillerColumnsView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);

    connect(m_timelineView, &TimelineView::fileSelected, this, &FilePanel::fileSelected);
    connect(m_timelineView, &TimelineView::fileDoubleClicked, this, &FilePanel::onDoubleClickedPath);
    connect(m_timelineView, &TimelineView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);

    connect(m_filmstripView, &FilmstripView::fileSelected, this, &FilePanel::fileSelected);
    connect(m_filmstripView, &FilmstripView::fileDoubleClicked, this, &FilePanel::onDoubleClickedPath);
    connect(m_filmstripView, &FilmstripView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);

    updateActiveViewModel();

    // Header formatting
    QHeaderView* header = m_treeView->header();
    header->setSectionsMovable(true);
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);

    connect(header, &QHeaderView::sectionResized, this, [this](int logicalIndex, int /*oldSize*/, int newSize) {
        saveColumnWidth(logicalIndex, newSize);
    });
    connect(header, &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order) {
        m_sortColumn = logicalIndex;
        m_sortOrder = order;
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("file_panel/sort_column", m_sortColumn);
        settings.setValue("file_panel/sort_order", (int)m_sortOrder);
    });

    connect(m_treeView, &QTreeView::doubleClicked, this, &FilePanel::onDoubleClicked);
    connect(m_treeView, &QTreeView::activated, this, &FilePanel::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_listView, &QListView::doubleClicked, this, &FilePanel::onDoubleClicked);
    connect(m_listView, &QListView::activated, this, &FilePanel::onDoubleClicked);
    connect(m_listView, &QListView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_theaterListView, &QListView::doubleClicked, this, &FilePanel::onDoubleClicked);
    connect(m_theaterListView, &QListView::activated, this, &FilePanel::onDoubleClicked);
    connect(m_theaterListView, &QListView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);

    // Restore view mode choice from settings
    QSettings settings("Amifiles", "Amifiles");
    int viewModeIdx = settings.value("file_panel/view_mode_index", 0).toInt();
    m_comboViewMode->setCurrentIndex(viewModeIdx);
    onViewModeChanged(viewModeIdx);

    m_treeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView->header(), &QHeaderView::customContextMenuRequested, this, &FilePanel::onHeaderContextMenu);
    for (int i = 0; i < 17; ++i) {
        bool defaultHidden = (i >= 4 && i != 16);
        bool hidden = settings.value(QString("columns/hidden_%1").arg(i), defaultHidden).toBool();
        m_treeView->header()->setSectionHidden(i, hidden);
    }


    // Bottom Filter and Status bar

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter files & folders (contains)...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FilePanel::onFilterChanged);

    m_globalSearchEdit = new QLineEdit(this);
    m_globalSearchEdit->setPlaceholderText("🔍 Search files & subfolders... (Esc to clear)");
    m_globalSearchEdit->setClearButtonEnabled(true);
    m_globalSearchEdit->installEventFilter(this);
    m_globalSearchEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    m_globalSearchEdit->setVisible(false);
    connect(m_globalSearchEdit, &QLineEdit::textChanged, this, &FilePanel::onGlobalSearchChanged);
    connect(m_globalSearchEdit, &QLineEdit::customContextMenuRequested, this, &FilePanel::onSearchEditContextMenu);

    // Flat Category Buttons
    m_btnFilterAll = new QToolButton(this);
    m_btnFilterAll->setText("All");
    m_btnFilterAll->setCheckable(true);
    m_btnFilterAll->setChecked(true);
    m_btnFilterAll->setToolTip("Show all files");
    m_btnFilterAll->setStyleSheet("QToolButton { padding: 4px 8px; font-weight: bold; }");

    m_btnFilterAudio = new QToolButton(this);
    m_btnFilterAudio->setText("🎵 Audio");
    m_btnFilterAudio->setCheckable(true);
    m_btnFilterAudio->setToolTip("Filter audio music files");
    m_btnFilterAudio->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterVideos = new QToolButton(this);
    m_btnFilterVideos->setText("🎬 Videos");
    m_btnFilterVideos->setCheckable(true);
    m_btnFilterVideos->setToolTip("Filter video files");
    m_btnFilterVideos->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterPictures = new QToolButton(this);
    m_btnFilterPictures->setText("🖼️ Pictures");
    m_btnFilterPictures->setCheckable(true);
    m_btnFilterPictures->setToolTip("Filter picture/image files");
    m_btnFilterPictures->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterDocs = new QToolButton(this);
    m_btnFilterDocs->setText("📄 Docs");
    m_btnFilterDocs->setCheckable(true);
    m_btnFilterDocs->setToolTip("Filter documents and text files");
    m_btnFilterDocs->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterArchive = new QToolButton(this);
    m_btnFilterArchive->setText("📦 Archives");
    m_btnFilterArchive->setCheckable(true);
    m_btnFilterArchive->setToolTip("Filter compressed archive files");
    m_btnFilterArchive->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterThreeD = new QToolButton(this);
    m_btnFilterThreeD->setText("🧊 3D Models");
    m_btnFilterThreeD->setCheckable(true);
    m_btnFilterThreeD->setToolTip("Filter 3D model files (.obj, .fbx, .stl, etc.)");
    m_btnFilterThreeD->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterFiles = new QToolButton(this);
    m_btnFilterFiles->setText("📄 Files Only");
    m_btnFilterFiles->setCheckable(true);
    m_btnFilterFiles->setToolTip("Show only files, hiding folders");
    m_btnFilterFiles->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterFolders = new QToolButton(this);
    m_btnFilterFolders->setText("📁 Folders");
    m_btnFilterFolders->setCheckable(true);
    m_btnFilterFolders->setToolTip("Show only directories, hiding files");
    m_btnFilterFolders->setStyleSheet("QToolButton { padding: 4px 8px; }");

    m_btnFilterRecent = new QToolButton(this);
    m_btnFilterRecent->setText("🔥 Recent (24h)");
    m_btnFilterRecent->setCheckable(true);
    m_btnFilterRecent->setToolTip("Show only files modified in the last 24 hours");
    m_btnFilterRecent->setStyleSheet("QToolButton { padding: 4px 8px; }");
    connect(m_btnFilterRecent, &QToolButton::toggled, this, &FilePanel::onRecentFilterToggled);

    m_btnStickyFilters = new QToolButton(this);
    m_btnStickyFilters->setText("📌 Sticky");
    m_btnStickyFilters->setCheckable(true);
    m_btnStickyFilters->setToolTip("Keep active filters when changing directories");
    m_btnStickyFilters->setStyleSheet("QToolButton { padding: 4px 8px; }");
    connect(m_btnStickyFilters, &QToolButton::toggled, this, [](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preferences/sticky_filters", checked);
    });
    m_btnStickyFilters->setChecked(settings.value("preferences/sticky_filters", false).toBool());

    m_btnRecentPlaces = new QToolButton(this);
    m_btnRecentPlaces->setText("🕒 Recent Places");
    m_btnRecentPlaces->setPopupMode(QToolButton::InstantPopup);
    m_btnRecentPlaces->setToolTip("Quickly jump to recently visited folder locations");
    m_btnRecentPlaces->setStyleSheet("QToolButton { padding: 4px 8px; color: #a6e3a1; }");
    QMenu* menuRecentPlaces = new QMenu(m_btnRecentPlaces);
    menuRecentPlaces->setStyleSheet(
        "QMenu { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
    );
    m_btnRecentPlaces->setMenu(menuRecentPlaces);
    connect(menuRecentPlaces, &QMenu::aboutToShow, this, &FilePanel::onRecentPlacesMenuAboutToShow);

    // Button group to manage checks, not exclusive to allow multi-selection
    QButtonGroup* filterGroup = new QButtonGroup(this);
    filterGroup->setExclusive(false);
    filterGroup->addButton(m_btnFilterAll);
    filterGroup->addButton(m_btnFilterAudio);
    filterGroup->addButton(m_btnFilterVideos);
    filterGroup->addButton(m_btnFilterPictures);
    filterGroup->addButton(m_btnFilterDocs);
    filterGroup->addButton(m_btnFilterArchive);
    filterGroup->addButton(m_btnFilterThreeD);
    filterGroup->addButton(m_btnFilterFiles);
    filterGroup->addButton(m_btnFilterFolders);
    filterGroup->addButton(m_btnFilterRecent);

    connect(m_btnFilterAll, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterAudio, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterVideos, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterPictures, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterDocs, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterArchive, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterThreeD, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterFiles, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterFolders, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);

    m_statusLabel = new QLabel("0 items", this);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 11px; margin-left: 6px;");

    // Wrap Category buttons row in a container widget to make it toggleable
    m_categoryWidget = new QWidget(this);
    QHBoxLayout* categoryLayout = new QHBoxLayout(m_categoryWidget);
    categoryLayout->setSpacing(4);
    categoryLayout->setContentsMargins(0, 0, 0, 0);
    categoryLayout->addWidget(m_btnFilterAll);
    categoryLayout->addWidget(m_btnFilterAudio);
    categoryLayout->addWidget(m_btnFilterVideos);
    categoryLayout->addWidget(m_btnFilterPictures);
    categoryLayout->addWidget(m_btnFilterDocs);
    categoryLayout->addWidget(m_btnFilterArchive);
    categoryLayout->addWidget(m_btnFilterThreeD);
    categoryLayout->addWidget(m_btnFilterFiles);
    categoryLayout->addWidget(m_btnFilterFolders);
    categoryLayout->addWidget(m_btnFilterRecent);
    categoryLayout->addWidget(m_btnStickyFilters);
    categoryLayout->addWidget(m_btnRecentPlaces);

    m_btnToggleSearchMode = new QToolButton(this);
    m_btnToggleSearchMode->setIcon(createSearchIcon(QColor("#cdd6f4")));
    m_btnToggleSearchMode->setToolTip("Switch to Search Mode");
    m_btnToggleSearchMode->setStyleSheet("QToolButton { background-color: transparent; border: none; }");
    connect(m_btnToggleSearchMode, &QToolButton::clicked, this, &FilePanel::onToggleSearchFilterMode);
    categoryLayout->addWidget(m_btnToggleSearchMode);

    m_btnToggleTRFilter = new QToolButton(this);
    m_btnToggleTRFilter->setText("★/🏷️");
    m_btnToggleTRFilter->setCheckable(true);
    m_btnToggleTRFilter->setToolTip("Toggle Tags & Ratings Filter Bar");
    m_btnToggleTRFilter->setStyleSheet("QToolButton { padding: 4px 8px; font-weight: bold; color: #f9e2af; }");
    connect(m_btnToggleTRFilter, &QToolButton::toggled, this, &FilePanel::onToggleTagsRatingsFilterBar);
    categoryLayout->addWidget(m_btnToggleTRFilter);

    categoryLayout->addStretch(1); // Push buttons to the left

    // Wrap Text filter row in a container widget to make it toggleable
    m_filterTextWidget = new QWidget(this);
    QHBoxLayout* filterTextLayout = new QHBoxLayout(m_filterTextWidget);
    filterTextLayout->setSpacing(6);
    filterTextLayout->setContentsMargins(0, 0, 0, 0);
    filterTextLayout->addWidget(m_filterEdit, 1);
    filterTextLayout->addWidget(m_globalSearchEdit, 1);

    // Status bar row (Always Visible)
    m_statusWidget = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(m_statusWidget);
    statusLayout->setContentsMargins(0, 4, 0, 4);
    statusLayout->setSpacing(6);
    statusLayout->addWidget(m_statusLabel, 1);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(0, 6);
    m_zoomSlider->setValue(1);
    m_zoomSlider->setFixedWidth(100);
    m_zoomSlider->setToolTip("Zoom Display Icons and Text");
    connect(m_zoomSlider, &QSlider::valueChanged, this, &FilePanel::onZoomChanged);
    statusLayout->addWidget(m_zoomSlider);

    m_comboGrouping = new QComboBox(this);
    m_comboGrouping->addItems({
        "No Grouping",
        "Group by Artist",
        "Group by Album",
        "Group by Genre",
        "Group by Type",
        "Group by Rating",
        "Group by Year",
        "Group by Decade",
        "Group by Custom Text..."
    });
    m_comboGrouping->setFixedWidth(160);
    m_comboGrouping->setToolTip("Group Files in List View");
    connect(m_comboGrouping, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilePanel::onGroupingChanged);
    statusLayout->addWidget(m_comboGrouping);

    // 5. Smart Tag & Rating Filter Bar
    m_tagsRatingsFilterWidget = new QWidget(this);
    m_tagsRatingsFilterWidget->setStyleSheet("QWidget { background-color: #242535; border-radius: 6px; }");
    QHBoxLayout* trLayout = new QHBoxLayout(m_tagsRatingsFilterWidget);
    trLayout->setSpacing(6);
    trLayout->setContentsMargins(6, 4, 6, 4);

    QLabel* lblRate = new QLabel("★ Rating:", this);
    lblRate->setStyleSheet("font-weight: bold; color: #f9e2af;");
    trLayout->addWidget(lblRate);

    QString rateAllStyle = 
        "QToolButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 8px; font-weight: bold; }"
        "QToolButton:hover { background-color: #45475a; color: #cdd6f4; }"
        "QToolButton:checked { background-color: #a6e3a1; color: #11111b; border: 1px solid #a6e3a1; }";

    m_btnRateAll = new QToolButton(this);
    m_btnRateAll->setText("All");
    m_btnRateAll->setCheckable(true);
    m_btnRateAll->setChecked(true);
    m_btnRateAll->setToolTip("Show all ratings");
    m_btnRateAll->setStyleSheet(rateAllStyle);
    connect(m_btnRateAll, &QToolButton::clicked, this, &FilePanel::onRatingFilterClicked);
    trLayout->addWidget(m_btnRateAll);

    QButtonGroup* rateGroup = new QButtonGroup(this);
    rateGroup->setExclusive(false);
    rateGroup->addButton(m_btnRateAll);

    QString btnStyle = 
        "QToolButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; border-radius: 4px; padding: 2px 6px; font-weight: bold; }"
        "QToolButton:hover { background-color: #45475a; color: #f9e2af; }"
        "QToolButton:checked { background-color: #f9e2af; color: #11111b; border: 1px solid #f9e2af; }";

    m_btnStars.clear();
    for (int i = 1; i <= 5; ++i) {
        QToolButton* btn = new QToolButton(this);
        btn->setText(QString("★").repeated(i));
        btn->setCheckable(true);
        btn->setToolTip(QString("Filter by %1 Stars").arg(i));
        btn->setStyleSheet(btnStyle);
        btn->setProperty("ratingValue", i);
        connect(btn, &QToolButton::clicked, this, &FilePanel::onRatingFilterClicked);
        trLayout->addWidget(btn);
        m_btnStars.append(btn);
        rateGroup->addButton(btn);
    }

    trLayout->addSpacing(10);

    QLabel* lblTag = new QLabel("🏷️ Tag:", this);
    lblTag->setStyleSheet("font-weight: bold; color: #89b4fa;");
    trLayout->addWidget(lblTag);

    m_comboFilterTag = new QComboBox(this);
    m_comboFilterTag->addItem("All Tags", "");
    m_comboFilterTag->setFixedWidth(130);
    connect(m_comboFilterTag, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilePanel::onTagFilterComboChanged);
    trLayout->addWidget(m_comboFilterTag);

    trLayout->addSpacing(10);

    QLabel* lblComment = new QLabel("💬 Comment:", this);
    lblComment->setStyleSheet("font-weight: bold; color: #a6e3a1;");
    trLayout->addWidget(lblComment);

    m_editFilterComment = new QLineEdit(this);
    m_editFilterComment->setPlaceholderText("Filter by comment...");
    m_editFilterComment->setFixedWidth(130);
    m_editFilterComment->setStyleSheet("QLineEdit { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 6px; }");
    connect(m_editFilterComment, &QLineEdit::textChanged, this, &FilePanel::onCommentFilterChanged);
    trLayout->addWidget(m_editFilterComment);

    trLayout->addStretch(1);

    // Close button
    QToolButton* btnCloseTR = new QToolButton(this);
    btnCloseTR->setText("✕");
    btnCloseTR->setToolTip("Close Tags & Ratings Filter Bar");
    btnCloseTR->setStyleSheet("QToolButton { background-color: transparent; border: none; font-weight: bold; color: #a6adc8; } QToolButton:hover { color: #f38ba8; }");
    connect(btnCloseTR, &QToolButton::clicked, this, &FilePanel::onCloseTagsRatingsFilterBar);
    trLayout->addWidget(btnCloseTR);

    // Initial state: load preferences
    bool showTR = settings.value("preferences/show_tags_ratings_filter_bar", false).toBool();
    m_tagsRatingsFilterWidget->setVisible(showTR);
    if (m_btnToggleTRFilter) {
        m_btnToggleTRFilter->setChecked(showTR);
    }

    QVBoxLayout* panelBottomLayout = new QVBoxLayout();
    panelBottomLayout->setContentsMargins(0, 0, 0, 0);
    panelBottomLayout->setSpacing(4);
    panelBottomLayout->addWidget(m_tagsRatingsFilterWidget);
    panelBottomLayout->addWidget(m_categoryWidget);
    panelBottomLayout->addWidget(m_filterTextWidget);
    panelBottomLayout->addWidget(m_statusWidget);

    mainLayout->addWidget(m_navContainer);
    mainLayout->addWidget(m_searchResultsView, 1);
    mainLayout->addWidget(m_viewStack, 1);
    mainLayout->addLayout(panelBottomLayout);

    int initialZoom = settings.value("file_panel/zoom_level", 1).toInt();
    m_zoomSlider->setValue(initialZoom);
    onZoomChanged(initialZoom);

    setActive(false);
    updateNavigationButtons();
}

bool FilePanel::eventFilter(QObject* watched, QEvent* event) {
    if (viewModeIndex() >= 8) {
        if (event->type() == QEvent::ToolTip || event->type() == QEvent::Leave || event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
            m_hoverTimer->stop();
            m_pendingHoverIndex = QModelIndex();
            if (m_hoverCard && m_hoverCard->isVisible()) {
                m_hoverCard->hide();
            }
        }
    } else {
        if (event->type() == QEvent::ToolTip) {
            QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
            QAbstractItemView* view = qobject_cast<QAbstractItemView*>(watched);
            if (!view && watched) {
                view = qobject_cast<QAbstractItemView*>(watched->parent());
            }
            
            if (view) {
                QPoint viewportPos = view->viewport()->mapFromGlobal(helpEvent->globalPos());
                QModelIndex index = view->indexAt(viewportPos);
                if (index.isValid()) {
                    m_pendingHoverIndex = index;
                    m_pendingHoverPos = helpEvent->globalPos();
                    m_hoverTimer->start(800); // 800ms delay!
                    return true;
                }
            }
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress) {
            m_hoverTimer->stop();
            m_pendingHoverIndex = QModelIndex();
            if (m_hoverCard && m_hoverCard->isVisible()) {
                m_hoverCard->hide();
            }
        } else if (event->type() == QEvent::MouseMove) {
            QAbstractItemView* view = qobject_cast<QAbstractItemView*>(watched);
            if (!view && watched) {
                view = qobject_cast<QAbstractItemView*>(watched->parent());
            }
            if (view) {
                QPoint viewportPos = view->viewport()->mapFromGlobal(QCursor::pos());
                QModelIndex index = view->indexAt(viewportPos);
                if (index.isValid()) {
                    QString filePath = filePathFromIndex(index);
                    if (m_hoverCard && m_hoverCard->isVisible()) {
                        if (filePath != m_hoverCard->currentFilePath()) {
                            m_hoverCard->hide();
                            m_pendingHoverIndex = index;
                            m_pendingHoverPos = QCursor::pos();
                            m_hoverTimer->start(800);
                        }
                    } else {
                        if (index != m_pendingHoverIndex) {
                            m_pendingHoverIndex = index;
                            m_pendingHoverPos = QCursor::pos();
                            m_hoverTimer->start(800);
                        }
                    }
                } else {
                    m_hoverTimer->stop();
                    m_pendingHoverIndex = QModelIndex();
                    if (m_hoverCard) m_hoverCard->hide();
                }
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        QListView* grid = nullptr;
        for (QListView* g : m_theaterGrids) {
            if (watched == g || (g && watched == g->viewport())) {
                grid = g;
                break;
            }
        }
        if (grid) {
            int gridIdx = m_theaterGrids.indexOf(grid);
            int key = ke->key();
            int currentRow = grid->currentIndex().row();
            int totalItems = grid->model()->rowCount(grid->rootIndex());

            if (gridIdx != -1 && currentRow >= 0 && totalItems > 0) {
                int gw = 135;
                if (m_zoomLevel >= 0) {
                    gw = 100 + m_zoomLevel * 35;
                }
                int cols = qMax(1, grid->width() / gw);

                QListView* targetGrid = nullptr;
                int targetRow = -1;

                if (key == Qt::Key_Down) {
                    if (currentRow >= totalItems - cols) {
                        if (gridIdx < m_theaterGrids.size() - 1) {
                            targetGrid = m_theaterGrids[gridIdx + 1];
                            int nextTotal = targetGrid->model()->rowCount(targetGrid->rootIndex());
                            targetRow = qMin(currentRow % cols, nextTotal - 1);
                        }
                    }
                } else if (key == Qt::Key_Up) {
                    if (currentRow < cols) {
                        if (gridIdx > 0) {
                            targetGrid = m_theaterGrids[gridIdx - 1];
                            int prevTotal = targetGrid->model()->rowCount(targetGrid->rootIndex());
                            int prevLastRowStart = (prevTotal - 1) / cols * cols;
                            targetRow = qMin(prevLastRowStart + (currentRow % cols), prevTotal - 1);
                        }
                    }
                } else if (key == Qt::Key_Right) {
                    if (currentRow == totalItems - 1) {
                        if (gridIdx < m_theaterGrids.size() - 1) {
                            targetGrid = m_theaterGrids[gridIdx + 1];
                            targetRow = 0;
                        }
                    }
                } else if (key == Qt::Key_Left) {
                    if (currentRow == 0) {
                        if (gridIdx > 0) {
                            targetGrid = m_theaterGrids[gridIdx - 1];
                            int prevTotal = targetGrid->model()->rowCount(targetGrid->rootIndex());
                            targetRow = prevTotal - 1;
                        }
                    }
                }

                if (targetGrid && targetRow >= 0) {
                    disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                    m_treeView->selectionModel()->clearSelection();
                    for (QListView* g : m_theaterGrids) {
                        g->selectionModel()->clearSelection();
                    }
                    QModelIndex targetIdx = targetGrid->model()->index(targetRow, 0, targetGrid->rootIndex());
                    if (targetIdx.isValid()) {
                        targetGrid->setCurrentIndex(targetIdx);
                        targetGrid->selectionModel()->select(targetIdx, QItemSelectionModel::ClearAndSelect);
                        targetGrid->setFocus();
                        m_theaterScrollArea->ensureWidgetVisible(targetGrid);
                        m_treeView->selectionModel()->select(targetIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                    }
                    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                    onSelectionChanged();
                    return true;
                }
            }
        }
    }

    if (watched == m_trackListWidget || (m_trackListWidget && watched == m_trackListWidget->viewport())) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Space) {
                emit playPauseRequested();
                return true;
            }
        }
    }
    if (watched == m_globalSearchEdit) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            setActive(true);
            emit panelActivated(this);
        }
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_globalSearchEdit->clear();
                return true;
            }
        }
    }
    if (watched == m_searchResultsView) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            setActive(true);
            emit panelActivated(this);
        }
    }
    // QRubberBand Mouse Selection Handling for item views
    QWidget* targetW = qobject_cast<QWidget*>(watched);
    QAbstractItemView* itemView = nullptr;
    if (targetW) {
        if (m_treeView && targetW == m_treeView->viewport()) itemView = m_treeView;
        else if (m_listView && targetW == m_listView->viewport()) itemView = m_listView;
        else if (m_theaterListView && targetW == m_theaterListView->viewport()) itemView = m_theaterListView;
        else if (qobject_cast<QAbstractItemView*>(targetW->parentWidget())) itemView = qobject_cast<QAbstractItemView*>(targetW->parentWidget());
        else if (qobject_cast<QAbstractItemView*>(targetW)) itemView = qobject_cast<QAbstractItemView*>(targetW);
    }

    if (itemView && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            setActive(true);
            emit panelActivated(this);
            QPoint pos = me->pos();
            QModelIndex idx = itemView->indexAt(pos);
            if (!idx.isValid()) {
                m_rubberBandOrigin = pos;
                m_rubberBandTargetView = targetW;
                if (!m_rubberBand) {
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, targetW);
                }
                m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                m_rubberBand->show();
                m_isRubberBandActive = true;
                if (!(me->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                    if (itemView->selectionModel()) {
                        itemView->selectionModel()->clearSelection();
                    }
                }
            }
        }
    } else if (m_isRubberBandActive && targetW && targetW == m_rubberBandTargetView && event->type() == QEvent::MouseMove) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (m_rubberBand) {
            QRect rect = QRect(m_rubberBandOrigin, me->pos()).normalized();
            m_rubberBand->setGeometry(rect);
            if (itemView && itemView->model()) {
                QItemSelection selection;
                int rows = itemView->model()->rowCount(itemView->rootIndex());
                for (int r = 0; r < rows; ++r) {
                    QModelIndex idx = itemView->model()->index(r, 0, itemView->rootIndex());
                    if (idx.isValid()) {
                        QRect vr = itemView->visualRect(idx);
                        if (vr.intersects(rect)) {
                            selection.select(idx, idx);
                        }
                    }
                }
                if (itemView->selectionModel()) {
                    QItemSelectionModel::SelectionFlags flags = QItemSelectionModel::Select;
                    if (itemView->selectionBehavior() == QAbstractItemView::SelectRows) {
                        flags |= QItemSelectionModel::Rows;
                    }
                    itemView->selectionModel()->select(selection, flags);
                }
            }
        }
    } else if (m_isRubberBandActive && (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::FocusOut)) {
        m_isRubberBandActive = false;
        if (m_rubberBand) {
            m_rubberBand->hide();
        }
    }

    bool isGrid = false;
    for (QListView* grid : m_theaterGrids) {
        if (watched == grid || (grid && watched == grid->viewport())) {
            isGrid = true;
            break;
        }
    }

    bool isWatched = (watched == m_treeView || (m_treeView && watched == m_treeView->viewport()) ||
                      watched == m_listView || (m_listView && watched == m_listView->viewport()) ||
                      watched == m_searchResultsView || (m_searchResultsView && watched == m_searchResultsView->viewport()) ||
                      watched == m_theaterListView || (m_theaterListView && watched == m_theaterListView->viewport()) ||
                      watched == m_millerView || (m_millerView && watched == m_millerView->viewport()) ||
                      watched == m_timelineView || (m_timelineView && watched == m_timelineView->viewport()) ||
                      watched == m_filmstripView ||
                      watched == m_theaterContainer || watched == m_bottomInfoPanel ||
                      isGrid);

    if (!isWatched && (m_millerView || m_filmstripView)) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        while (w) {
            if (m_millerView && w == m_millerView) {
                isWatched = true;
                break;
            }
            if (m_filmstripView && w == m_filmstripView) {
                isWatched = true;
                break;
            }
            w = w->parentWidget();
        }
    }

    if (isWatched) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            setActive(true);
            emit panelActivated(this);
        }

        if (event->type() == QEvent::DragEnter) {
            QSettings settings("Amifiles", "Amifiles");
            QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();
            if (preset != "System Theme") {
                Theme::ThemeColors colors = Theme::getThemeColors();
                QString bg = m_customBgColor.isEmpty() ? colors.bg : m_customBgColor;
                m_viewStack->setStyleSheet(QString("QStackedWidget { border: 2px dashed #a6e3a1; border-radius: 4px; background-color: %1; }").arg(bg));
            }
        }
        if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
            QSettings settings("Amifiles", "Amifiles");
            QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();
            if (preset != "System Theme") {
                Theme::ThemeColors colors = Theme::getThemeColors();
                QString bg = m_customBgColor.isEmpty() ? colors.bg : m_customBgColor;
                QString borderColor = m_isActive ? colors.accent : colors.border;
                m_viewStack->setStyleSheet(QString("QStackedWidget { border: 2px solid %1; border-radius: 4px; background-color: %2; }").arg(borderColor).arg(bg));
            }
        }

        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            int key = keyEvent->key();

            // Keyboard Focus Navigation between Grouped Grids
            if (isGrid && (key == Qt::Key_Up || key == Qt::Key_Down)) {
                QListView* grid = nullptr;
                QWidget* targetW = qobject_cast<QWidget*>(watched);
                if (qobject_cast<QListView*>(targetW)) {
                    grid = qobject_cast<QListView*>(targetW);
                } else if (targetW && qobject_cast<QListView*>(targetW->parentWidget())) {
                    grid = qobject_cast<QListView*>(targetW->parentWidget());
                }

                if (grid) {
                    int gridIdx = m_theaterGrids.indexOf(grid);
                    if (gridIdx >= 0) {
                        if (key == Qt::Key_Down) {
                            QModelIndex currentIdx = grid->currentIndex();
                            if (currentIdx.isValid()) {
                                QRect currentRect = grid->visualRect(currentIdx);
                                bool hasItemBelow = false;
                                int rowCount = grid->model()->rowCount(grid->rootIndex());
                                for (int r = 0; r < rowCount; ++r) {
                                    QModelIndex testIdx = grid->model()->index(r, 0, grid->rootIndex());
                                    if (testIdx.isValid() && testIdx != currentIdx) {
                                        QRect testRect = grid->visualRect(testIdx);
                                        if (testRect.top() >= currentRect.bottom() - 5) {
                                            hasItemBelow = true;
                                            break;
                                        }
                                    }
                                }

                                if (!hasItemBelow) {
                                    if (gridIdx + 1 < m_theaterGrids.size()) {
                                        QListView* nextGrid = m_theaterGrids[gridIdx + 1];
                                        int nextCount = nextGrid->model()->rowCount(nextGrid->rootIndex());
                                        if (nextCount > 0) {
                                            int minTop = 999999;
                                            for (int r = 0; r < nextCount; ++r) {
                                                QModelIndex testIdx = nextGrid->model()->index(r, 0, nextGrid->rootIndex());
                                                QRect testRect = nextGrid->visualRect(testIdx);
                                                if (testRect.top() < minTop) {
                                                    minTop = testRect.top();
                                                }
                                            }

                                            QModelIndex targetIdx;
                                            int minDiff = 999999;
                                            int currentCenterX = currentRect.center().x();
                                            for (int r = 0; r < nextCount; ++r) {
                                                QModelIndex testIdx = nextGrid->model()->index(r, 0, nextGrid->rootIndex());
                                                QRect testRect = nextGrid->visualRect(testIdx);
                                                if (qAbs(testRect.top() - minTop) < 10) {
                                                    int diff = qAbs(testRect.center().x() - currentCenterX);
                                                    if (diff < minDiff) {
                                                        minDiff = diff;
                                                        targetIdx = testIdx;
                                                    }
                                                }
                                            }

                                            if (targetIdx.isValid()) {
                                                grid->selectionModel()->clearSelection();
                                                nextGrid->setCurrentIndex(targetIdx);
                                                nextGrid->selectionModel()->select(targetIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                                                nextGrid->setFocus();
                                                nextGrid->scrollTo(targetIdx, QAbstractItemView::EnsureVisible);
                                                return true;
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (key == Qt::Key_Up) {
                            QModelIndex currentIdx = grid->currentIndex();
                            if (currentIdx.isValid()) {
                                QRect currentRect = grid->visualRect(currentIdx);
                                bool hasItemAbove = false;
                                int rowCount = grid->model()->rowCount(grid->rootIndex());
                                for (int r = 0; r < rowCount; ++r) {
                                    QModelIndex testIdx = grid->model()->index(r, 0, grid->rootIndex());
                                    if (testIdx.isValid() && testIdx != currentIdx) {
                                        QRect testRect = grid->visualRect(testIdx);
                                        if (testRect.bottom() <= currentRect.top() + 5) {
                                            hasItemAbove = true;
                                            break;
                                        }
                                    }
                                }

                                if (!hasItemAbove) {
                                    if (gridIdx - 1 >= 0) {
                                        QListView* prevGrid = m_theaterGrids[gridIdx - 1];
                                        int prevCount = prevGrid->model()->rowCount(prevGrid->rootIndex());
                                        if (prevCount > 0) {
                                            int maxBottom = -999999;
                                            for (int r = 0; r < prevCount; ++r) {
                                                QModelIndex testIdx = prevGrid->model()->index(r, 0, prevGrid->rootIndex());
                                                QRect testRect = prevGrid->visualRect(testIdx);
                                                if (testRect.bottom() > maxBottom) {
                                                    maxBottom = testRect.bottom();
                                                }
                                            }

                                            QModelIndex targetIdx;
                                            int minDiff = 999999;
                                            int currentCenterX = currentRect.center().x();
                                            for (int r = 0; r < prevCount; ++r) {
                                                QModelIndex testIdx = prevGrid->model()->index(r, 0, prevGrid->rootIndex());
                                                QRect testRect = prevGrid->visualRect(testIdx);
                                                if (qAbs(testRect.bottom() - maxBottom) < 10) {
                                                    int diff = qAbs(testRect.center().x() - currentCenterX);
                                                    if (diff < minDiff) {
                                                        minDiff = diff;
                                                        targetIdx = testIdx;
                                                    }
                                                }
                                            }

                                            if (targetIdx.isValid()) {
                                                grid->selectionModel()->clearSelection();
                                                prevGrid->setCurrentIndex(targetIdx);
                                                prevGrid->selectionModel()->select(targetIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                                                prevGrid->setFocus();
                                                prevGrid->scrollTo(targetIdx, QAbstractItemView::EnsureVisible);
                                                return true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            QSettings settings("Amifiles", "Amifiles");
            bool remoteMode = settings.value("preferences/keyboard_remote_mode", false).toBool();
            
            if (remoteMode) {
                QKeySequence pressed(keyEvent->modifiers() | keyEvent->key());
                
                QKeySequence shortcutPlayCollection(settings.value("shortcuts/play_collection", "Ctrl+Space").toString());
                QKeySequence shortcutInfoSheet(settings.value("shortcuts/info_sheet", "I").toString());
                QKeySequence shortcutScrapeMeta(settings.value("shortcuts/scrape_meta", "M").toString());
                QKeySequence shortcutApplyCasing(settings.value("shortcuts/apply_casing", "D").toString());
                QKeySequence shortcutToggleDrawer(settings.value("shortcuts/toggle_drawer", "P").toString());
                QKeySequence shortcutNavigateUp(settings.value("shortcuts/navigate_up", "Backspace").toString());
                QKeySequence shortcutNavigateBack(settings.value("shortcuts/navigate_back", "Alt+Left").toString());

                if (pressed == QKeySequence(Qt::Key_Return) || pressed == QKeySequence(Qt::Key_Enter)) {
                    QModelIndex currentIdx;
                    if (isGrid) {
                        QListView* grid = nullptr;
                        QWidget* targetW = qobject_cast<QWidget*>(watched);
                        if (qobject_cast<QListView*>(targetW)) {
                            grid = qobject_cast<QListView*>(targetW);
                        } else if (targetW && qobject_cast<QListView*>(targetW->parentWidget())) {
                            grid = qobject_cast<QListView*>(targetW->parentWidget());
                        }
                        if (grid) currentIdx = grid->currentIndex();
                    } else {
                        currentIdx = m_theaterListView->currentIndex();
                    }
                    if (currentIdx.isValid()) {
                        onDoubleClicked(currentIdx);
                        return true;
                    }
                }
                
                if (pressed == shortcutPlayCollection) {
                    playCollection();
                    return true;
                }
                if (pressed == shortcutInfoSheet) {
                    QStringList curSelected = selectedPaths();
                    QString selectedPath = curSelected.isEmpty() ? "" : curSelected.first();
                    if (!selectedPath.isEmpty()) {
                        showInfoSheet(selectedPath);
                        return true;
                    }
                }
                if (pressed == shortcutToggleDrawer) {
                    if (m_btnToggleSidePane) {
                        m_btnToggleSidePane->toggle();
                        return true;
                    }
                }
                if (pressed == shortcutNavigateUp || key == Qt::Key_Escape) {
                    onNavigateUp();
                    return true;
                }
                if (pressed == shortcutNavigateBack || key == Qt::Key_Back) {
                    onNavigateBack();
                    return true;
                }
                if (pressed == shortcutScrapeMeta) {
                    QStringList curSelected = selectedPaths();
                    QString selectedPath = curSelected.isEmpty() ? "" : curSelected.first();
                    if (!selectedPath.isEmpty()) {
                        if (viewModeIndex() == 10) {
                            FolderArtScraperDialog dlg(selectedPath, this);
                            dlg.exec();
                            refresh();
                        } else {
                            VideoScraperDialog scraperDlg({selectedPath}, this);
                            if (scraperDlg.exec() == QDialog::Accepted) {
                                refresh();
                                onSelectionChanged();
                            }
                        }
                        return true;
                    }
                }
                if (pressed == shortcutApplyCasing) {
                    QStringList curSelected = selectedPaths();
                    QString selectedPath = curSelected.isEmpty() ? "" : curSelected.first();
                    if (!selectedPath.isEmpty()) {
                        QFileInfo info(selectedPath);
                        if (!info.isDir()) {
                            QString dirPath = info.absolutePath();
                            QString baseName = info.completeBaseName();
                            QStringList genericImages = { "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png" };
                            QString foundGeneric;
                            for (const QString& name : genericImages) {
                                QFileInfo fi(QDir(dirPath).filePath(name));
                                if (fi.exists()) {
                                    foundGeneric = fi.absoluteFilePath();
                                    break;
                                }
                            }
                            if (!foundGeneric.isEmpty()) {
                                QString ext = QFileInfo(foundGeneric).suffix();
                                QString destPath = QDir(dirPath).filePath(baseName + "_cover." + ext);
                                if (QFile::rename(foundGeneric, destPath)) {
                                    m_proxyModel->clearCasingCache();
                                    refresh();
                                }
                            }
                        }
                        return true;
                    }
                }
            }

            if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
                emit tabPressed();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                onNavigateUp();
                return true;
            }
            if (keyEvent->modifiers() == Qt::AltModifier) {
                if (keyEvent->key() == Qt::Key_Left) {
                    onNavigateBack();
                    return true;
                } else if (keyEvent->key() == Qt::Key_Right) {
                    onNavigateForward();
                    return true;
                }
            }
        }
        
        if (event->type() == QEvent::Wheel) {
            QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
            if (wheel->modifiers() & Qt::ControlModifier) {
                if (wheel->angleDelta().y() > 0) {
                    zoomIn();
                } else {
                    zoomOut();
                }
                return true; // Consume event
            }
        }

        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
        }
        if (event->type() == QEvent::DragMove) {
            QDragMoveEvent* dragMoveEvent = static_cast<QDragMoveEvent*>(event);
            if (dragMoveEvent->mimeData()->hasUrls()) {
                dragMoveEvent->acceptProposedAction();
                return true;
            }
        }
        if (event->type() == QEvent::Drop) {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            const QMimeData* mime = dropEvent->mimeData();
            if (mime->hasUrls()) {
                QStringList srcPaths;
                for (const QUrl& url : mime->urls()) {
                    QString local = url.toLocalFile();
                    if (!local.isEmpty() && QFile::exists(local)) {
                        srcPaths << local;
                    }
                }
                
                if (!srcPaths.isEmpty()) {
                    if (m_archiveViewActive) {
                        QSettings settings("Amifiles", "Amifiles");
                        bool archiveWriteEnabled = settings.value("preferences/archive_write", false).toBool();
                        if (!archiveWriteEnabled) {
                            QMessageBox::warning(this, "Write Mode Disabled", "Archive Write Mode is currently disabled. You can enable read-write permissions for archives and disk images in the View menu.");
                            return true;
                        }
                        dropEvent->acceptProposedAction();
                        if (m_archiveModel->addFiles(srcPaths)) {
                            refresh();
                        } else {
                            QMessageBox::warning(this, "Archive Add Failed", "Could not add selected files to the archive.");
                        }
                        return true;
                    }

                    QString destDir = m_currentPath;
                    
                    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(watched);
                    if (!view) {
                        QWidget* viewport = qobject_cast<QWidget*>(watched);
                        if (viewport) {
                            view = qobject_cast<QAbstractItemView*>(viewport->parent());
                        }
                    }
                    if (view) {
                        QModelIndex index = view->indexAt(dropEvent->position().toPoint());
                        if (index.isValid()) {
                            QString path = filePathFromIndex(index);
                            if (!path.isEmpty() && QFileInfo(path).isDir()) {
                                destDir = path;
                            }
                        } else {
                            QModelIndex rootIdx = view->rootIndex();
                            if (rootIdx.isValid()) {
                                QString path = filePathFromIndex(rootIdx);
                                if (!path.isEmpty() && QFileInfo(path).isDir()) {
                                    destDir = path;
                                }
                            }
                        }
                    } else if (watched == m_timelineView || (m_timelineView && watched == m_timelineView->viewport())) {
                        QTreeWidgetItem* item = m_timelineView->itemAt(dropEvent->position().toPoint());
                        if (item) {
                            QString path = item->data(0, Qt::UserRole).toString();
                            if (!path.isEmpty() && QFileInfo(path).isDir()) {
                                destDir = path;
                            }
                        }
                    }

                    QMenu menu(this);
                    QAction* actCopy = menu.addAction("Copy Here");
                    QAction* actMove = menu.addAction("Move Here");
                    menu.addSeparator();
                    QAction* actCancel = menu.addAction("Cancel");
                    
                    QAction* chosen = menu.exec(QCursor::pos());
                    if (chosen == actCopy) {
                        CopyQueueManager::instance().queueCopy(srcPaths, destDir, false, this);
                    } else if (chosen == actMove) {
                        bool containsLocked = false;
                        QString lockedPath;
                        for (const QString& path : srcPaths) {
                            if (!QFileInfo(path).isWritable() || isPathLockedPersistent(path)) {
                                containsLocked = true;
                                lockedPath = path;
                                break;
                            }
                            QFileInfo info(path);
                            if (info.isDir()) {
                                QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
                                while (it.hasNext()) {
                                    QString childPath = it.next();
                                    if (!QFileInfo(childPath).isWritable() || isPathLockedPersistent(childPath)) {
                                        containsLocked = true;
                                        lockedPath = childPath;
                                        break;
                                    }
                                }
                            }
                            if (containsLocked) break;
                        }

                        if (containsLocked) {
                            QMessageBox::warning(this, "Operation Blocked",
                                                 QString("One or more files/directories being moved (Drag & Drop Move) are locked (read-only) or contain locked items.\n"
                                                         "Blocked item: %1\n\n"
                                                         "Please unlock all files/folders first before moving them.")
                                                 .arg(QDir::toNativeSeparators(lockedPath)));
                        } else {
                            CopyQueueManager::instance().queueCopy(srcPaths, destDir, true, this);
                        }
                    }
                }
                dropEvent->acceptProposedAction();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FilePanel::setActive(bool active) {
    m_isActive = active;
    updateStyles();
    if (m_globalSearchEdit) {
        QString borderColor = active ? "#89b4fa" : "#313244";
        m_globalSearchEdit->setStyleSheet(QString("QLineEdit { border: 2px solid %1; background-color: #1e1e2e; color: #cdd6f4; padding: 4px 8px; border-radius: 4px; }").arg(borderColor));
    }
}

QString FilePanel::currentPath() const {
    return m_currentPath;
}

bool FilePanel::isSecondPane() const {
    QWidget* p = parentWidget();
    while (p) {
        if (p->objectName() == "rightTabWidget") {
            return true;
        }
        if (p->objectName() == "leftTabWidget") {
            return false;
        }
        p = p->parentWidget();
    }
    return false;
}

void FilePanel::setCustomBgColor(const QString& hexColor) {
    if (m_customBgColor != hexColor) {
        m_customBgColor = hexColor;
        updateStyles();
    }
}

void FilePanel::setCustomBgImage(const QString& imagePath) {
    if (m_customBgImage != imagePath) {
        m_customBgImage = imagePath;
        updateStyles();
    }
}

void FilePanel::setCustomBgOpacity(double opacity) {
    if (m_customBgOpacity != opacity) {
        m_customBgOpacity = opacity;
        updateStyles();
    }
}

void FilePanel::setPath(const QString& path) {
    navigateTo(path, true);
}

QAbstractItemModel* FilePanel::activeBaseModel() const {
    if (m_archiveViewActive) return m_archiveModel;
    if (m_smartViewActive) return m_smartModel;
    if (m_flatViewEnabled) return m_flatProxyModel;
    return m_proxyModel;
}

void FilePanel::updateActiveViewModel() {
    if (m_treeView->selectionModel()) {
        disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
    }
    
    QAbstractItemModel* base = activeBaseModel();
    if (m_groupProxy && m_groupProxy->isGroupingActive()) {
        m_groupProxy->setSourceModel(base);
        m_treeView->setModel(m_groupProxy);
        m_listView->setModel(m_groupProxy);
        m_theaterListView->setModel(m_groupProxy);
        m_filmstripView->setModel(m_groupProxy);
        if (m_coverFlowView) m_coverFlowView->setModel(m_groupProxy);
    } else {
        m_treeView->setModel(base);
        m_listView->setModel(base);
        m_theaterListView->setModel(base);
        m_filmstripView->setModel(base);
        if (m_coverFlowView) m_coverFlowView->setModel(base);
    }
    m_listView->setSelectionModel(m_treeView->selectionModel());
    m_theaterListView->setSelectionModel(m_treeView->selectionModel());
    if (m_coverFlowView) {
        m_coverFlowView->setSelectionModel(m_treeView->selectionModel());
        m_coverFlowView->setRootIndex(m_listView->rootIndex());
    }
    
    if (m_treeView->selectionModel()) {
        connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
    }
    if (m_filmstripView) {
        m_filmstripView->setRootPath(m_currentPath);
    }
}

void FilePanel::focusActiveView() {
    QWidget* view = nullptr;
    if (m_viewStack && (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer)) {
        view = m_theaterListView;
    } else if (m_flatViewEnabled && m_listView) {
        view = m_listView;
    } else if (m_smartViewActive && m_treeView) {
        view = m_treeView;
    } else if (m_treeView) {
        view = m_treeView;
    }
    
    if (view) {
        view->setFocus();
    } else {
        setFocus();
    }
}

QScrollBar* FilePanel::activeVerticalScrollBar() const {
    QAbstractItemView* view = nullptr;
    if (m_viewStack && (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer)) {
        view = m_theaterListView;
    } else if (m_flatViewEnabled && m_listView) {
        view = m_listView;
    } else if (m_smartViewActive && m_treeView) {
        view = m_treeView;
    } else if (m_treeView) {
        view = m_treeView;
    }
    return view ? view->verticalScrollBar() : nullptr;
}

void FilePanel::selectFilePath(const QString& filePath) {
    QFileInfo info(filePath);
    navigateTo(info.absolutePath(), true);

    QModelIndex srcIndex = m_fileModel->index(filePath);
    if (srcIndex.isValid()) {
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
        if (proxyIndex.isValid()) {
            m_treeView->setCurrentIndex(proxyIndex);
            m_treeView->scrollTo(proxyIndex);
            m_listView->setCurrentIndex(proxyIndex);
            m_listView->scrollTo(proxyIndex);
        }
    }
}

void FilePanel::navigateTo(const QString& path, bool addHistory) {
    if (m_isPathLocked && path != m_lockedPath) {
        emit openNewTabRequested(path);
        return;
    }

    if (m_isPathLockedWithSubdirs && !path.startsWith(m_lockedPath, Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Locked Path", "This tab is locked to the current folder hierarchy.");
        return;
    }

    QString prevPath = m_currentPath;

    // Pre-apply folder rules to configure view layout and casing settings before loading files
    QWidget* parentW = parentWidget();
    while (parentW && !parentW->inherits("MainWindow")) {
        parentW = parentW->parentWidget();
    }
    MainWindow* mw = qobject_cast<MainWindow*>(parentW);
    if (mw) {
        mw->applyFolderRules(path, this);
    }

    if (m_isSearchModeActive) {
        onToggleSearchFilterMode();
    }

    if (path == "smart://home") {
        QSettings settings("Amifiles", "Amifiles");
        bool enableSmartHome = settings.value("preferences/enable_smart_home", true).toBool();
        if (!enableSmartHome) {
            navigateTo(QDir::homePath(), addHistory);
            return;
        }
        if (isSecondPane()) {
            bool enableSmartHomeSecond = settings.value("preferences/enable_smart_home_second_pane", true).toBool();
            if (!enableSmartHomeSecond) {
                navigateTo(QDir::homePath(), addHistory);
                return;
            }
        }
        m_smartHomeEnabled = true;

        if (m_treeView->selectionModel()) {
            disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
        }
        m_homeDashboardActive = true;
        m_dashboardActive = false;
        m_smartViewActive = false;
        m_archiveViewActive = false;

        m_viewStack->setCurrentWidget(m_homeDashboardWidget);
        m_homeDashboardWidget->refreshDashboard();
        m_homeDashboardWidget->setFocus();

        m_currentPath = path;
        m_pathEdit->setText(path);
        emit pathChanged(m_currentPath);
        updateStatusText();

        if (addHistory) {
            if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1) {
                m_history = m_history.mid(0, m_historyIndex + 1);
            }
            m_history.append(m_currentPath);
            m_historyIndex = m_history.size() - 1;
        }
        updateNavigationButtons();
        return;
    }

    if (path == "smart://disk_dashboard") {
        QString scanDir = (m_currentPath.isEmpty() || m_currentPath.startsWith("smart://")) ? QDir::homePath() : m_currentPath;
        if (m_treeView->selectionModel()) {
            disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
        }
        m_dashboardActive = true;
        m_smartViewActive = false;
        m_archiveViewActive = false;
        m_viewStack->setCurrentWidget(m_dashboardWidget);
        m_dashboardWidget->scanDirectory(scanDir);
        m_currentPath = path;
        m_pathEdit->setText(path);
        emit pathChanged(m_currentPath);
        updateStatusText();

        if (addHistory) {
            if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1) {
                m_history = m_history.mid(0, m_historyIndex + 1);
            }
            m_history.append(m_currentPath);
            m_historyIndex = m_history.size() - 1;
        }
        updateNavigationButtons();
        return;
    }

    if (path.startsWith("smart://")) {
        QString ruleName = path.mid(8);
        DynamicFavoriteRule matchedRule;
        bool found = false;
        for (const auto& r : FavoritesManager::instance().getDynamicRules()) {
            if (r.name == ruleName) {
                matchedRule = r;
                found = true;
                break;
            }
        }
        if (found) {
            m_smartViewActive = true;
            m_archiveViewActive = false;
            updateActiveViewModel();
            m_smartModel->setQueryRule(matchedRule);
            m_currentPath = path;
            m_pathEdit->setText(path);
            emit pathChanged(m_currentPath);
            updateStatusText();

            if (addHistory) {
                if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1) {
                    m_history = m_history.mid(0, m_historyIndex + 1);
                }
                m_history.append(m_currentPath);
                m_historyIndex = m_history.size() - 1;
            }
            updateNavigationButtons();
            return;
        }
    }

    if (path.contains("//")) {
        int sepIdx = path.indexOf("//");
        QString archiveFile = path.left(sepIdx);
        QString virtualSubpath = path.mid(sepIdx + 2);

        QFileInfo archInfo(archiveFile);
        if (archInfo.exists() && archInfo.isFile()) {
            m_archiveViewActive = true;
            m_smartViewActive = false;
            updateActiveViewModel();

            if (m_categoryWidget) m_categoryWidget->hide();

            // Enable interactive resizing for all columns
            for (int i = 0; i < 4; ++i) {
                m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
            }

            m_archiveModel->loadArchive(archiveFile);
            m_archiveModel->navigateToVirtualPath(virtualSubpath);
            m_pathEdit->setText(QDir::toNativeSeparators(archiveFile) + "//" + m_archiveModel->currentVirtualPath());
            emit pathChanged(m_currentPath);
            updateStatusText();
            return;
        }
    } else if (m_archiveViewActive || m_smartViewActive || m_dashboardActive || m_homeDashboardActive) {
        m_archiveViewActive = false;
        m_smartViewActive = false;
        m_dashboardActive = false;
        m_homeDashboardActive = false;

        // Restore active view stack widget
        onViewModeChanged(m_comboViewMode ? m_comboViewMode->currentIndex() : 0);

        updateActiveViewModel();

        if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);

        // Restore interactive resizing for all columns
        for (int i = 0; i < 4; ++i) {
            m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
        }
    }

    QString cleanPath = path;
    // Skip blocking exists() check for remote/network paths
    bool isRemote = false;
    if (path.startsWith("/run/user/") && path.contains("/gvfs/")) {
        isRemote = true;
    } else if (path.contains("CloudMounts") || path.startsWith(QDir::homePath() + "/CloudMounts")) {
        isRemote = true;
    } else if (path.startsWith("ftp://") || path.startsWith("sftp://") || path.startsWith("smb://")) {
        isRemote = true;
    }

    if (isRemote) {
        if (cleanPath.startsWith("/")) {
            // Run a non-blocking responsiveness check on the remote share
            QProcess checkProc;
            checkProc.start("timeout", {"1s", "ls", "-d", cleanPath});
            if (!checkProc.waitForFinished(1200) || checkProc.exitCode() != 0) {
                QMessageBox::warning(this, "Remote Connection", "The remote share is not responding. Please verify that the server is online.");
                return;
            }
        }
    } else {
        QDir dir(path);
        if (!dir.exists()) {
            return;
        }
        cleanPath = dir.absolutePath();
    }

    m_currentPath = cleanPath;
    m_pathEdit->setText(QDir::toNativeSeparators(m_currentPath));

    // Sticky filters logic on folder navigation
    QSettings settings("Amifiles", "Amifiles");
    bool stickyFilters = settings.value("preferences/sticky_filters", false).toBool();
    if (!stickyFilters) {
        if (m_btnFilterAll) {
            m_btnFilterAll->setChecked(true);
            m_btnFilterAudio->setChecked(false);
            m_btnFilterVideos->setChecked(false);
            m_btnFilterPictures->setChecked(false);
            m_btnFilterDocs->setChecked(false);
            m_btnFilterArchive->setChecked(false);
            m_btnFilterThreeD->setChecked(false);
            m_btnFilterFiles->setChecked(false);
            m_btnFilterFolders->setChecked(false);
            if (m_btnFilterRecent) {
                m_btnFilterRecent->setChecked(false);
            }
        }
        QSet<FileFilterProxyModel::FilterType> allFilter = { FileFilterProxyModel::FilterAll };
        m_proxyModel->setFilterTypes(allFilter);
        m_proxyModel->setShowRecentOnly(false);
        if (m_flatProxyModel) {
            m_flatProxyModel->setShowRecentOnly(false);
        }
    }

    // Update tree view root
    if (m_flatViewEnabled) {
        m_flatModel->setRootPath(m_currentPath);
        m_groupProxy->rebuildGroups();
        m_treeView->setRootIndex(QModelIndex());
        m_listView->setRootIndex(QModelIndex());
        m_theaterListView->setRootIndex(QModelIndex());
        if (m_coverFlowView) m_coverFlowView->setRootIndex(QModelIndex());
    } else {
        m_proxyModel->setCurrentPath(m_currentPath);

        // Optimize remote directory listing to prevent blocking and slow file/symlink resolution
        auto isRemotePath = [](const QString& p) {
            if (p.startsWith("/run/user/") && p.contains("/gvfs/")) {
                return true;
            }
            QString home = QDir::homePath();
            if (p.startsWith(home + "/CloudMounts/")) {
                return true;
            }
            return false;
        };

        if (isRemotePath(m_currentPath)) {
            m_fileModel->setOption(QFileSystemModel::DontWatchForChanges, true);
            m_fileModel->setResolveSymlinks(false);
        } else {
            m_fileModel->setOption(QFileSystemModel::DontWatchForChanges, false);
            m_fileModel->setResolveSymlinks(true);
        }

        QModelIndex srcIndex = m_fileModel->setRootPath(m_currentPath);
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            m_groupProxy->rebuildGroups();
            m_treeView->setRootIndex(QModelIndex());
            m_listView->setRootIndex(QModelIndex());
            m_theaterListView->setRootIndex(QModelIndex());
            if (m_coverFlowView) m_coverFlowView->setRootIndex(QModelIndex());
            
            if (m_viewStack->currentWidget() == m_theaterContainer) {
                m_theaterListView->setVisible(false);
                m_theaterScrollArea->setVisible(true);
                if (m_trackListWidget) m_trackListWidget->setVisible(false);
                if (m_drawerBtnContainer) m_drawerBtnContainer->setVisible(false);
                queueRebuildTheaterGroups();
            }
        } else {
            m_treeView->setRootIndex(proxyIndex);
            m_listView->setRootIndex(proxyIndex);
            m_theaterListView->setRootIndex(proxyIndex);
            if (m_coverFlowView) m_coverFlowView->setRootIndex(proxyIndex);
            
            m_theaterListView->setVisible(true);
            m_theaterScrollArea->setVisible(false);
            if (m_trackListWidget) m_trackListWidget->setVisible(viewModeIndex() >= 7 && viewModeIndex() <= 10);
            if (m_drawerBtnContainer) m_drawerBtnContainer->setVisible(viewModeIndex() >= 7 && viewModeIndex() <= 10);
        }

        if (m_btnToggleSidePane) {
            int index = viewModeIndex();
            bool groupingActive = m_groupProxy && m_groupProxy->isGroupingActive();
            m_btnToggleSidePane->setVisible((index >= 7 && index <= 10) || groupingActive);
            if (m_theaterSideContainer) {
                updateDrawerVisibility();
            }
            if (m_trackListWidget) {
                m_trackListWidget->setVisible(index >= 7 && index <= 10 && !groupingActive);
            }
            if (m_drawerBtnContainer) {
                m_drawerBtnContainer->setVisible(index >= 7 && index <= 10 && !groupingActive);
            }
        }
    }

    // Update History
    if (addHistory) {
        if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1) {
            // Cut off forward history
            m_history = m_history.mid(0, m_historyIndex + 1);
        }
        m_history.append(m_currentPath);
        m_historyIndex = m_history.size() - 1;
    }

    // Load column widths and sort settings
    loadColumnWidths();
    loadSortSettings();
    m_treeView->sortByColumn(m_sortColumn, m_sortOrder);

    m_millerView->setRootPath(m_currentPath);
    m_timelineView->setRootPath(m_currentPath);
    m_filmstripView->setRootPath(m_currentPath);

    updateNavigationButtons();
    updateFavoritesUI();
    checkFolderArt();

    // If we navigated up, collapse and select the folder we just exited so it doesn't stay expanded
    if (!prevPath.isEmpty() && prevPath.length() > m_currentPath.length() && prevPath.startsWith(m_currentPath, Qt::CaseInsensitive)) {
        QModelIndex srcIdx = m_fileModel->index(prevPath);
        if (srcIdx.isValid()) {
            QModelIndex proxyIdx = m_proxyModel ? m_proxyModel->mapFromSource(srcIdx) : srcIdx;
            if (proxyIdx.isValid()) {
                if (m_treeView) {
                    m_treeView->setCurrentIndex(proxyIdx);
                    if (m_treeView->selectionModel()) {
                        m_treeView->selectionModel()->select(proxyIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    }
                    m_treeView->setExpanded(proxyIdx, false);
                }
                if (m_listView) {
                    m_listView->setCurrentIndex(proxyIdx);
                    if (m_listView->selectionModel()) {
                        m_listView->selectionModel()->select(proxyIdx, QItemSelectionModel::ClearAndSelect);
                    }
                }
            }
        } else {
            m_bottomPanelPath.clear();
            if (m_bottomEnterBtn) {
                m_bottomEnterBtn->setVisible(false);
            }
        }
    }

    updateStatusText();

    emit pathChanged(m_currentPath);

    // Since we navigated, trigger selection check
    onSelectionChanged();
    focusFirstItemInActiveView();
}

void FilePanel::onNavigateUp() {
    if (m_archiveViewActive) {
        if (!m_archiveModel->currentVirtualPath().isEmpty()) {
            m_archiveModel->navigateUp();
            m_pathEdit->setText(QDir::toNativeSeparators(m_archiveModel->archivePath()) + "//" + m_archiveModel->currentVirtualPath());
            emit pathChanged(m_currentPath);
            updateStatusText();
        } else {
            m_archiveViewActive = false;
            updateActiveViewModel();

            if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);

            // Enable interactive resizing for main columns after exiting archive view
            for (int i = 0; i < 4; ++i) {
                m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
            }

            QString parentDir = QFileInfo(m_archiveModel->archivePath()).absolutePath();
            navigateTo(parentDir, true);
        }
        return;
    }

    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath(), true);
    }
}

void FilePanel::onNavigateBack() {
    if (m_historyIndex > 0) {
        m_historyIndex--;
        navigateTo(m_history.at(m_historyIndex), false);
    }
}

void FilePanel::onNavigateForward() {
    if (m_historyIndex < m_history.size() - 1) {
        m_historyIndex++;
        navigateTo(m_history.at(m_historyIndex), false);
    }
}

void FilePanel::onPathEntered() {
    QString target = m_pathEdit->text().trimmed();
    navigateTo(target, true);
}

void FilePanel::onFavoriteClicked() {
    FavoritesManager& fm = FavoritesManager::instance();
    if (fm.isFavorite(m_currentPath)) {
        fm.removeFavorite(m_currentPath);
    } else {
        fm.addFavorite(m_currentPath);
    }
    updateFavoritesUI();
}

void FilePanel::onHomeClicked() {
    QSettings settings("Amifiles", "Amifiles");
    QString homePath = settings.value("preferences/home_path", "smart://home").toString();
    navigateTo(homePath);
}

void FilePanel::onHomeContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
    );
    QAction* actSmart = menu.addAction(style()->standardIcon(QStyle::SP_ComputerIcon), "🏠 Go to Smart Home Dashboard");
    QAction* actPhys = menu.addAction(style()->standardIcon(QStyle::SP_DirHomeIcon), "📁 Go to User Home Directory (/home/dave)");
    menu.addSeparator();
    QAction* actSetHome = menu.addAction("Set Current Folder as Home");

    QAction* selected = menu.exec(m_btnHome->mapToGlobal(pos));
    if (selected == actSmart) {
        setSmartHomeEnabled(true);
        setPath("smart://home");
    } else if (selected == actPhys) {
        setPath(QDir::homePath());
    } else if (selected == actSetHome) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preferences/home_path", m_currentPath);
        QMessageBox::information(this, "Set Home Directory", QString("Home directory set to:\n%1").arg(m_currentPath));
    }
}

void FilePanel::onClonePathClicked() {
    emit clonePathRequested(m_currentPath);
}

void FilePanel::updateFavoritesUI() {
    FavoritesManager& fm = FavoritesManager::instance();
    if (fm.isFavorite(m_currentPath)) {
        m_btnFavorite->setText("★");
    } else {
        m_btnFavorite->setText("☆");
    }
}

void FilePanel::onFilterChanged(const QString& filterText) {
    if (m_siblingPanel && !m_isActive && !m_siblingPanel->isFilterTextBarVisible()) {
        if (m_siblingPanel->isFlatViewEnabled()) {
            m_siblingPanel->m_flatProxyModel->setFilterText(filterText);
        } else {
            m_siblingPanel->proxyModel()->setFilterText(filterText);
        }
        m_siblingPanel->updateStatusText();
        m_siblingPanel->syncFilterText(filterText);
    } else {
        if (m_flatViewEnabled) {
            m_flatProxyModel->setFilterText(filterText);
        } else {
            m_proxyModel->setFilterText(filterText);
        }
        updateStatusText();
    }
}

void FilePanel::onFilterTypeChanged() {
    QObject* clickedButton = sender();
    if (clickedButton == m_btnFilterAll) {
        m_btnFilterAll->setChecked(true);
        m_btnFilterAudio->setChecked(false);
        m_btnFilterVideos->setChecked(false);
        m_btnFilterPictures->setChecked(false);
        m_btnFilterDocs->setChecked(false);
        m_btnFilterArchive->setChecked(false);
        m_btnFilterThreeD->setChecked(false);
        m_btnFilterFiles->setChecked(false);
        m_btnFilterFolders->setChecked(false);
    } else if (clickedButton) {
        m_btnFilterAll->setChecked(false);
        bool anyChecked = m_btnFilterAudio->isChecked() ||
                          m_btnFilterVideos->isChecked() ||
                          m_btnFilterPictures->isChecked() ||
                          m_btnFilterDocs->isChecked() ||
                          m_btnFilterArchive->isChecked() ||
                          m_btnFilterThreeD->isChecked() ||
                          m_btnFilterFiles->isChecked() ||
                          m_btnFilterFolders->isChecked();
        if (!anyChecked) {
            m_btnFilterAll->setChecked(true);
        }
    }

    QSet<FileFilterProxyModel::FilterType> activeTypes;
    if (m_btnFilterAll->isChecked()) {
        activeTypes.insert(FileFilterProxyModel::FilterAll);
    } else {
        if (m_btnFilterAudio->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterAudio);
        if (m_btnFilterVideos->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterVideos);
        if (m_btnFilterPictures->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterPictures);
        if (m_btnFilterDocs->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterDocs);
        if (m_btnFilterArchive->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterArchive);
        if (m_btnFilterThreeD->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterThreeD);
        if (m_btnFilterFiles->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterFiles);
        if (m_btnFilterFolders->isChecked()) activeTypes.insert(FileFilterProxyModel::FilterFolders);
    }

    FilePanel* targetPanel = this;
    if (m_siblingPanel && !m_isActive && !m_siblingPanel->isCategoryButtonsVisible()) {
        targetPanel = m_siblingPanel;
    }

    targetPanel->proxyModel()->setFilterTypes(activeTypes);
    if (targetPanel->m_flatProxyModel) {
        targetPanel->m_flatProxyModel->setFilterTypes(activeTypes);
    }
    targetPanel->updateStatusText();

    if (targetPanel != this) {
        targetPanel->syncFilterTypes(activeTypes);
    }
}

void FilePanel::onRecentFilterToggled(bool checked) {
    if (m_proxyModel) {
        m_proxyModel->setShowRecentOnly(checked);
    }
    if (m_flatProxyModel) {
        m_flatProxyModel->setShowRecentOnly(checked);
    }
    updateStatusText();
}

void FilePanel::onRecentPlacesMenuAboutToShow() {
    if (!m_btnRecentPlaces || !m_btnRecentPlaces->menu()) return;
    QMenu* menu = m_btnRecentPlaces->menu();
    menu->clear();

    QSettings settings("Amifiles", "Amifiles");
    QStringList recents = settings.value("recents/folders").toStringList();

    if (recents.isEmpty()) {
        QAction* actEmpty = menu->addAction("(No Recent Places)");
        actEmpty->setEnabled(false);
        return;
    }

    for (const QString& path : recents) {
        QAction* act = menu->addAction(QApplication::style()->standardIcon(QStyle::SP_DirIcon), QDir::toNativeSeparators(path));
        connect(act, &QAction::triggered, this, [this, path]() {
            setPath(path);
        });
    }

    menu->addSeparator();
    QAction* actClear = menu->addAction(QIcon::fromTheme("edit-clear"), "🧹 Clear Recent Places History");
    connect(actClear, &QAction::triggered, this, [this]() {
        QSettings settings("Amifiles", "Amifiles");
        settings.remove("recents/folders");
        settings.sync();
        
        QWidget* parentW = parentWidget();
        while (parentW && !parentW->inherits("MainWindow")) {
            parentW = parentW->parentWidget();
        }
        MainWindow* mw = qobject_cast<MainWindow*>(parentW);
        if (mw) {
            QMetaObject::invokeMethod(mw, "refreshRecentsSidebar");
            QMetaObject::invokeMethod(mw, "refreshAllDashboards");
        }
    });
}

static bool containsMediaFilesDirectly(const QString& folderPath) {
    QDir dir(folderPath);
    QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg", "mod", "sid", "s3m", "xm", "it" };
    QFileInfoList files = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& fInfo : files) {
        if (fInfo.isSymLink()) continue;
        if (mediaExts.contains(fInfo.suffix().toLower())) {
            return true;
        }
    }
    return false;
}

static bool isMultiDiscAlbumFolder(const QString& folderPath) {
    QDir dir(folderPath);
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (subdirs.isEmpty()) return false;

    QRegularExpression discRegex("^(cd|disc|disk|dvd)[\\s_\\-]*[0-9]+$", QRegularExpression::CaseInsensitiveOption);
    bool hasDiscSubdir = false;
    for (const QFileInfo& sub : subdirs) {
        if (discRegex.match(sub.fileName()).hasMatch()) {
            hasDiscSubdir = true;
            break;
        }
    }
    return hasDiscSubdir;
}

static bool isPlayableAlbumFolder(const QString& folderPath) {
    if (containsMediaFilesDirectly(folderPath)) {
        return true;
    }
    return isMultiDiscAlbumFolder(folderPath);
}

void FilePanel::onDoubleClicked(const QModelIndex& index) {
    if (m_archiveViewActive) {
        if (m_archiveModel->isDir(index)) {
            QString name = m_archiveModel->entryName(index);
            m_archiveModel->enterDirectory(name);
            m_pathEdit->setText(QDir::toNativeSeparators(m_archiveModel->archivePath()) + "//" + m_archiveModel->currentVirtualPath());
            emit pathChanged(m_currentPath);
            updateStatusText();
        } else {
            QString vPath = m_archiveModel->entryPath(index);
            m_statusLabel->setText("Extracting file...");
            QApplication::processEvents();
            QString localPath = m_archiveModel->extractFile(vPath);
            updateStatusText();
            if (!localPath.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
            } else {
                QMessageBox::warning(this, "Extract File", "Failed to extract file from archive.");
            }
        }
        return;
    }

    QString path;
    if (m_flatViewEnabled) {
        QModelIndex srcIndex = m_flatProxyModel->mapToSource(index);
        path = m_flatModel->filePath(srcIndex);
    } else if (m_smartViewActive) {
        path = m_smartModel->filePath(index);
    } else {
        QModelIndex mappedIndex = index;
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            if (index.internalId() > 0 && index.internalId() <= 10000) {
                if (m_viewStack->currentWidget() == m_treeView) {
                    m_treeView->setExpanded(index, !m_treeView->isExpanded(index));
                }
                return;
            }
            mappedIndex = m_groupProxy->mapToSource(index);
        }
        QModelIndex srcIndex = m_proxyModel->mapToSource(mappedIndex);
        path = m_fileModel->filePath(srcIndex);
    }

    onDoubleClickedPath(path);
}

struct CinemaMetadata {
    QString title;
    QString plot;
    QString rating;
    QString genre;
    QString studio;
    QString year;
};

static CinemaMetadata parseNfoFile(const QString& filePath) {
    CinemaMetadata meta;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return meta;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Check if it looks like XML
    if (content.trimmed().startsWith("<?xml") || content.contains("<movie") || content.contains("<tvshow")) {
        auto matchTag = [](const QString& tag, const QString& src) -> QString {
            QRegularExpression re(QString("<%1>(.*?)</%1>").arg(tag), QRegularExpression::DotMatchesEverythingOption);
            auto m = re.match(src);
            if (m.hasMatch()) {
                QString val = m.captured(1).trimmed();
                val.replace("&amp;", "&")
                   .replace("&lt;", "<")
                   .replace("&gt;", ">")
                   .replace("&quot;", "\"")
                   .replace("&apos;", "'");
                return val;
            }
            return QString();
        };

        meta.title = matchTag("title", content);
        meta.plot = matchTag("plot", content);
        meta.rating = matchTag("rating", content);
        meta.genre = matchTag("genre", content);
        meta.studio = matchTag("studio", content);
        meta.year = matchTag("year", content);
    } else {
        meta.plot = content.trimmed();
    }
    return meta;
}

void FilePanel::onSelectionChanged() {
    updateStatusText();
    QStringList paths = selectedPaths();

    if (m_viewStack->currentWidget() == m_theaterContainer) {
        QString backdropPath;
        QString dirPath = paths.isEmpty() ? m_currentPath : (QFileInfo(paths.first()).isDir() ? paths.first() : QFileInfo(paths.first()).absolutePath());
        
        QStringList checks = { "backdrop.jpg", "backdrop.jpeg", "backdrop.png", "fanart.jpg", "fanart.jpeg", "fanart.png" };
        for (const QString& check : checks) {
            QString test = QDir(dirPath).filePath(check);
            if (QFile::exists(test)) {
                backdropPath = test;
                break;
            }
        }
        m_theaterListView->setBackdropPath(backdropPath);

        // Update bottom info panel
        if (!paths.isEmpty()) {
            QString path = paths.first();
            QFileInfo pathInfo(path);
            m_bottomPanelPath = path;

            if (m_bottomEnterBtn && m_bottomEnterBtn->isVisible() != pathInfo.isDir()) {
                m_bottomEnterBtn->setVisible(pathInfo.isDir());
            }

            QSettings settings("Amifiles", "Amifiles");
            int modeIndex = viewModeIndex(); // 6 = Music Showcase, 7 = Cinema Showcase, 8 = Movie, 9 = TV, 10 = Music (v2)

            // Dynamic bottom panel sizing
            int panelHeight = 72;
            if (modeIndex == 10) {
                panelHeight = 120;
            } else if (modeIndex == 8 || modeIndex == 9) {
                panelHeight = 100;
            }
            if (m_bottomInfoPanel->height() != panelHeight) {
                m_bottomInfoPanel->setFixedHeight(panelHeight);
            }

            // Hide/Show widgets based on view mode index
            if (m_musicControlsWidget && m_musicControlsWidget->isVisible() != (modeIndex == 10)) {
                m_musicControlsWidget->setVisible(modeIndex == 10);
            }
            if (m_visualizerWidget && m_visualizerWidget->isVisible() != (modeIndex == 10)) {
                m_visualizerWidget->setVisible(modeIndex == 10);
            }
            if (m_trackListWidget && m_trackListWidget->isVisible() != (modeIndex >= 7 && modeIndex <= 10)) {
                m_trackListWidget->setVisible(modeIndex >= 7 && modeIndex <= 10);
            }
            if (m_drawerBtnContainer && m_drawerBtnContainer->isVisible() != (modeIndex >= 7 && modeIndex <= 10)) {
                m_drawerBtnContainer->setVisible(modeIndex >= 7 && modeIndex <= 10);
            }
            if (m_cinemaButtonsWidget && m_cinemaButtonsWidget->isVisible() != (modeIndex == 8 || modeIndex == 9)) {
                m_cinemaButtonsWidget->setVisible(modeIndex == 8 || modeIndex == 9);
            }

            if (modeIndex == 7 || modeIndex == 8 || modeIndex == 9) {
                // Video Showcase
                if (m_bottomSynopsis && !m_bottomSynopsis->isVisible()) {
                    m_bottomSynopsis->setVisible(true);
                }
                if (m_bottomPlayBtn && !m_bottomPlayBtn->isVisible()) {
                    m_bottomPlayBtn->setVisible(true);
                }

                bool isTvShow = false;
                bool isSeason = false;
                if (pathInfo.isDir()) {
                    QDir dir(path);
                    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QFileInfo& sub : subDirs) {
                        QString name = sub.fileName().toLower();
                        if (name.contains("season") || name.contains("series")) {
                            isTvShow = true;
                            break;
                        }
                    }
                    if (!isTvShow) {
                        QString name = pathInfo.fileName().toLower();
                        if (name.contains("season") || name.contains("series")) {
                            isSeason = true;
                        }
                    }
                }

                if (pathInfo.isDir()) {
                    if (isTvShow) {
                        m_bottomPlayBtn->setText("▶ Play Entire TV Show");
                    } else if (isSeason) {
                        m_bottomPlayBtn->setText("▶ Play Season");
                    } else {
                        m_bottomPlayBtn->setText("▶ Play Folder");
                    }
                } else {
                    if (modeIndex == 9) {
                        m_bottomPlayBtn->setText("▶ Play Episode");
                    } else {
                        m_bottomPlayBtn->setText("▶ Play Video");
                    }
                }

                QString targetDir = pathInfo.isDir() ? path : pathInfo.absolutePath();
                CinemaMetadata meta;
                QString foundNfoPath;
                QStringList nfoCandidates = { "movie.nfo", "tvshow.nfo", "summary.txt", "description.txt", "info.txt" };
                
                if (!pathInfo.isDir()) {
                    QString fileNfo = QDir(targetDir).filePath(pathInfo.completeBaseName() + ".nfo");
                    if (QFile::exists(fileNfo)) {
                        foundNfoPath = fileNfo;
                    }
                }
                
                if (foundNfoPath.isEmpty()) {
                    QDir dir(targetDir);
                    QFileInfoList dirFiles = dir.entryInfoList(QDir::Files);
                    for (const QFileInfo& nfoFi : dirFiles) {
                        if (nfoFi.suffix().toLower() == "nfo" || nfoCandidates.contains(nfoFi.fileName().toLower())) {
                            foundNfoPath = nfoFi.absoluteFilePath();
                            break;
                        }
                    }
                }

                if (!foundNfoPath.isEmpty()) {
                    meta = parseNfoFile(foundNfoPath);
                }

                QString titleToShow = meta.title.isEmpty() ? pathInfo.fileName() : meta.title;
                m_bottomTitle->setText(titleToShow);

                QString metaStr;
                if (!meta.year.isEmpty()) metaStr += QString("📅 %1  ").arg(meta.year);
                if (!meta.rating.isEmpty()) metaStr += QString("⭐ %1  ").arg(meta.rating);
                if (!meta.genre.isEmpty()) metaStr += QString("🏷 %1").arg(meta.genre);
                if (metaStr.isEmpty()) metaStr = "Video / Series Folder";
                m_bottomMeta->setText(metaStr);

                if (!meta.plot.isEmpty()) {
                    m_bottomSynopsis->setText(meta.plot);
                } else {
                    m_bottomSynopsis->setText("No plot summary (.nfo) found inside this directory.");
                }

                QString key = (modeIndex == 8) ? "movie_showcase/show_info_panel" : 
                              ((modeIndex == 9) ? "tv_showcase/show_info_panel" : "video_showcase/show_info_panel");
                bool showInfoPanel = settings.value(key, true).toBool();
                m_bottomInfoPanel->setVisible(showInfoPanel);
            } else if (modeIndex == 6 || modeIndex == 10) {
                // Audio Showcase
                m_bottomSynopsis->setVisible(false);
                m_bottomPlayBtn->setText("▶ Play Album");
                m_bottomPlayBtn->setVisible(true);

                bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool();
                QString folderName = pathInfo.fileName();
                QString cleanedTitle = groupMultiDisc ? FileFilterProxyModel::cleanAlbumFolderName(folderName) : folderName;

                m_bottomTitle->setText(cleanedTitle);
                
                QString artistName = "Unknown Artist";
                QDir parentDir(pathInfo.absolutePath());
                if (parentDir.dirName() != "Music" && parentDir.dirName() != "Amifiles") {
                    artistName = parentDir.dirName();
                }
                m_bottomMeta->setText(artistName);

                // For Music Showcase v2, scan the selected directory for tracks and populate tracklist drawer
                if (modeIndex == 10 && m_trackListWidget) {
                    QSettings settings("Amifiles", "Amifiles");
                    bool autoQueue = settings.value("preview/auto_queue_sibling_files", true).toBool();
                    if (autoQueue) {
                        m_trackListWidget->clear();
                        m_trackListWidget->setIconSize(QSize(40, 40));
                        QString targetDir = pathInfo.isDir() ? path : pathInfo.absolutePath();
                        QDir dir(targetDir);
                        QStringList audioExts = { "mp3", "flac", "wav", "ogg", "m4a", "wma", "aac", "mod", "sid", "s3m", "xm", "it" };
                        
                        QFileInfoList trackFiles = dir.entryInfoList(QDir::Files, QDir::Name);
                        
                        // Check if the top-level directory contains any playable audio files
                        bool hasAudioFiles = false;
                        for (const QFileInfo& fi : trackFiles) {
                            if (audioExts.contains(fi.suffix().toLower())) {
                                hasAudioFiles = true;
                                break;
                            }
                        }

                        QFileInfoList allTracks;
                        if (hasAudioFiles) {
                            allTracks = trackFiles;
                        } else {
                            // Go down one level to subdirectories (e.g. CD1, CD2) to populate the queue
                            QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                            for (const QFileInfo& subDirFi : subDirs) {
                                QDir subDir(subDirFi.absoluteFilePath());
                                QFileInfoList subFiles = subDir.entryInfoList(QDir::Files, QDir::Name);
                                for (const QFileInfo& fi : subFiles) {
                                    if (audioExts.contains(fi.suffix().toLower())) {
                                        allTracks.append(fi);
                                    }
                                }
                            }
                        }

                        int selectIndex = -1;
                        for (const QFileInfo& trackFi : allTracks) {
                            if (audioExts.contains(trackFi.suffix().toLower())) {
                                QString pathVal = trackFi.absoluteFilePath();
                                QString filename = trackFi.fileName();
                                QString folderName = QFileInfo(trackFi.absolutePath()).fileName();
                                QString displayName = filename;
                                if (!folderName.isEmpty() && folderName.toLower() != "music" && folderName.toLower() != "audio" && folderName.toLower() != "download" && folderName.toLower() != "downloads") {
                                    displayName = QString("%1 (%2)").arg(filename).arg(folderName);
                                }
                                
                                QListWidgetItem* item = new QListWidgetItem(displayName, m_trackListWidget);
                                item->setData(Qt::UserRole, pathVal);
                                item->setIcon(getTrackArtworkIcon(pathVal));
                                if (pathVal == path) {
                                    selectIndex = m_trackListWidget->count() - 1;
                                }
                            }
                        }
                        if (selectIndex != -1) {
                            m_trackListWidget->setCurrentRow(selectIndex);
                        } else if (m_trackListWidget->count() > 0) {
                            m_trackListWidget->setCurrentRow(0);
                        }
                        updateDrawerVisibility();
                    }
                }

                QString key = (modeIndex == 10) ? "music_showcase/show_info_panel" : "audio_showcase/show_info_panel";
                bool showInfoPanel = settings.value(key, true).toBool();
                m_bottomInfoPanel->setVisible(showInfoPanel);
            } else {
                m_bottomInfoPanel->setVisible(false);
            }
        } else {
            m_bottomInfoPanel->setVisible(false);
        }
    }

    if (paths.isEmpty()) {
        if (!m_folderArtPath.isEmpty()) {
            emit folderArtDetected(m_folderArtPath);
        } else {
            emit fileSelected("");
        }
    } else {
        emit fileSelected(paths.first());
    }
}

QString FilePanel::filePathFromIndex(const QModelIndex& index) const {
    if (!index.isValid()) return "";
    
    QModelIndex col0 = index.sibling(index.row(), 0);
    QModelIndex srcIdx = col0;
    const QAbstractItemModel* m = col0.model();
    while (m) {
        if (const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>(m)) {
            srcIdx = proxy->mapToSource(srcIdx);
            m = proxy->sourceModel();
        } else {
            break;
        }
    }
    
    if (m == m_fileModel) {
        return m_fileModel->filePath(srcIdx);
    } else if (m == m_flatModel) {
        return m_flatModel->filePath(srcIdx);
    } else if (m == m_smartModel) {
        return m_smartModel->filePath(srcIdx);
    } else if (m == m_archiveModel) {
        return m_archiveModel->entryPath(srcIdx);
    }
    return "";
}

QStringList FilePanel::selectedPaths() const {
    QWidget* active = m_viewStack->currentWidget();
    if (active == m_millerView) {
        return m_millerView->selectedPaths();
    } else if (active == m_timelineView) {
        return m_timelineView->selectedPaths();
    } else if (active == m_filmstripView) {
        return m_filmstripView->selectedPaths();
    }

    QStringList paths;
    QItemSelectionModel* selModel = nullptr;
    if (active == m_theaterListView) {
        selModel = m_theaterListView->selectionModel();
    } else if (active == m_theaterContainer) {
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            QListView* activeGrid = nullptr;
            for (QListView* grid : m_theaterGrids) {
                if (grid && (grid->hasFocus() || (grid->viewport() && grid->viewport()->hasFocus()))) {
                    activeGrid = grid;
                    break;
                }
            }
            if (!activeGrid) {
                for (QListView* grid : m_theaterGrids) {
                    if (grid && grid->selectionModel() && grid->selectionModel()->hasSelection()) {
                        activeGrid = grid;
                        break;
                    }
                }
            }
            if (activeGrid) {
                selModel = activeGrid->selectionModel();
            } else {
                selModel = m_treeView->selectionModel();
            }
        } else {
            selModel = m_theaterListView->selectionModel();
        }
    } else if (active == m_listView) {
        selModel = m_listView->selectionModel();
    } else {
        selModel = m_treeView->selectionModel();
    }
    if (!selModel) return paths;
    QModelIndexList selectedIndexes = selModel->selectedIndexes();
    QModelIndexList selectedRows;
    QSet<int> rowsSeen;
    for (const QModelIndex& idx : selectedIndexes) {
        if (idx.column() == 0 && !rowsSeen.contains(idx.row())) {
            rowsSeen.insert(idx.row());
            selectedRows.append(idx);
        }
    }
    for (const QModelIndex& index : selectedRows) {
        QModelIndex mappedIndex = index;
        if (m_groupProxy->isGroupingActive()) {
            if (index.internalId() > 0 && index.internalId() <= 10000) {
                continue; // Skip group headers
            }
            mappedIndex = m_groupProxy->mapToSource(index);
        }

        if (m_archiveViewActive) {
            if (!m_archiveModel->isDir(mappedIndex)) {
                QString vPath = m_archiveModel->entryPath(mappedIndex);
                QString tempPath = const_cast<ArchiveModel*>(m_archiveModel)->extractFile(vPath);
                if (!tempPath.isEmpty()) {
                    paths.append(tempPath);
                } else {
                    paths.append(vPath);
                }
            } else {
                QString vPath = m_archiveModel->entryPath(mappedIndex);
                QString tempDirPath = const_cast<ArchiveModel*>(m_archiveModel)->extractDirRecursively(vPath);
                if (!tempDirPath.isEmpty()) {
                    paths.append(tempDirPath);
                } else {
                    paths.append(vPath);
                }
            }
        } else if (m_flatViewEnabled) {
            QModelIndex srcIndex = m_flatProxyModel->mapToSource(mappedIndex);
            paths.append(m_flatModel->filePath(srcIndex));
        } else if (m_smartViewActive) {
            paths.append(m_smartModel->filePath(mappedIndex));
        } else {
            QModelIndex srcIndex = m_proxyModel->mapToSource(mappedIndex);
            paths.append(m_fileModel->filePath(srcIndex));
        }
    }
    return paths;
}

QString FilePanel::activeFilePath() const {
    QStringList paths = selectedPaths();
    if (!paths.isEmpty()) {
        return paths.first();
    }
    return m_currentPath;
}

void FilePanel::checkFolderArt() {
    m_folderArtPath.clear();
    QStringList checks = { "folder.jpg", "folder.png", "poster.jpg", "poster.png", 
                           "folder.JPEG", "folder.PNG", "poster.JPEG", "poster.PNG" };
    
    QDir dir(m_currentPath);
    for (const QString& file : checks) {
        if (dir.exists(file)) {
            m_folderArtPath = dir.absoluteFilePath(file);
            break;
        }
    }
}

void FilePanel::updateNavigationButtons() {
    m_btnBack->setEnabled(m_historyIndex > 0);
    m_btnForward->setEnabled(m_historyIndex < m_history.size() - 1);
}

void FilePanel::updateStatusText() {
    if (!m_statusLabel) return;
    if (m_archiveViewActive) {
        int totalItems = m_archiveModel->rowCount();
        int selectedItems = m_treeView->selectionModel() ? m_treeView->selectionModel()->selectedRows().size() : 0;
        if (selectedItems == 0) {
            m_statusLabel->setText(QString("%1 items (Archive)").arg(totalItems));
        } else {
            m_statusLabel->setText(QString("%1 of %2 items selected (Archive)").arg(selectedItems).arg(totalItems));
        }
        return;
    }

    QAbstractItemModel* activeModel = m_treeView->model();
    if (!activeModel) return;

    int totalItems = activeModel->rowCount(m_treeView->rootIndex());
    QModelIndexList selectedRows = m_treeView->selectionModel()->selectedRows();
    int selectedItems = selectedRows.size();

    if (selectedItems == 0) {
        m_statusLabel->setText(QString("%1 items").arg(totalItems));
    } else {
        qint64 totalSize = 0;
        for (const QModelIndex& index : selectedRows) {
            QString path;
            if (m_flatViewEnabled) {
                QModelIndex srcIndex = m_flatProxyModel->mapToSource(index);
                path = m_flatModel->filePath(srcIndex);
            } else {
                QModelIndex srcIndex = m_proxyModel->mapToSource(index);
                path = m_fileModel->filePath(srcIndex);
            }
            QFileInfo info(path);
            if (info.isFile()) {
                totalSize += info.size();
            }
        }

        QString sizeStr;
        if (totalSize < 1024) {
            sizeStr = QString("%1 B").arg(totalSize);
        } else if (totalSize < 1024 * 1024) {
            sizeStr = QString("%1 KB").arg(QString::number(totalSize / 1024.0, 'f', 1));
        } else {
            sizeStr = QString("%1 MB").arg(QString::number(totalSize / (1024.0 * 1024.0), 'f', 1));
        }

        m_statusLabel->setText(QString("%1 items | %2 selected (%3)")
                               .arg(totalItems)
                               .arg(selectedItems)
                               .arg(sizeStr));
    }
}

void FilePanel::refresh() {
    updateTheaterGridSize();
    QSettings settings("Amifiles", "Amifiles");
    bool detailsFullRowSelect = settings.value("preferences/details_full_row_select", true).toBool();
    if (m_treeView) {
        m_treeView->setSelectionBehavior(detailsFullRowSelect ? QAbstractItemView::SelectRows : QAbstractItemView::SelectItems);
    }

    if (m_fileModel) {
        updateFileSystemFilters();
        m_fileModel->clearCache();
    }
    if (m_proxyModel) {
        m_proxyModel->clearCasingCache();

        updateHideSettings();
    }
    if (m_flatViewEnabled) {
        m_flatModel->setRootPath(m_currentPath);
    } else {
        QModelIndex srcIndex = m_fileModel->index(m_currentPath);
        m_fileModel->setRootPath("");
        m_fileModel->setRootPath(m_currentPath);
    }
    checkFolderArt();
    populateFilterTagsCombo();
    updateStatusText();
}

void FilePanel::refreshHomeDashboard() {
    if (m_homeDashboardWidget) {
        m_homeDashboardWidget->refreshDashboard();
    }
}

void FilePanel::writeTempFileToArchive(const QString& tempPath) {
    if (!m_archiveViewActive || !m_archiveModel) return;

    QSettings settings("Amifiles", "Amifiles");
    bool archiveWriteEnabled = settings.value("preferences/archive_write", false).toBool();
    if (!archiveWriteEnabled) {
        QMessageBox::warning(this, "Write Mode Disabled", 
                             "Archive Write Mode is currently disabled. You can enable read-write permissions for archives and disk images in the View menu to save your edits back to the disk image.");
        return;
    }

    if (m_archiveModel->addFiles({ tempPath })) {
        refresh();
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not write modified file back to the archive.");
    }
}

// ================= Clipboard & File Operations =================

void FilePanel::onCopy() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    QList<QUrl> urls;
    for (const QString& path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);
}

void FilePanel::onCut() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    QList<QUrl> urls;
    for (const QString& path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    mimeData->setUrls(urls);
    
    // Custom formats to indicate cut (move) operation
    mimeData->setData("application/amifiles-cut", "1");
    mimeData->setData("application/x-kde-cutselection", "1"); // Standard on Linux desktop

    clipboard->setMimeData(mimeData);
}

void FilePanel::onPaste() {
    QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData) return;

    // 1. Paste Image Data from Clipboard (browser, screenshot, photo editor)
    if (mimeData->hasImage() && !mimeData->hasUrls()) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (image.isNull()) {
            QPixmap pix = qvariant_cast<QPixmap>(mimeData->imageData());
            image = pix.toImage();
        }
        if (!image.isNull()) {
            QString baseName = "clipboard";
            QString ext = ".png";
            QString targetPath = QDir(m_currentPath).filePath(baseName + ext);
            int count = 1;
            while (QFile::exists(targetPath)) {
                targetPath = QDir(m_currentPath).filePath(QString("%1_%2%3").arg(baseName).arg(count++).arg(ext));
            }
            if (image.save(targetPath, "PNG")) {
                refresh();
                QModelIndex idx = m_proxyModel ? m_proxyModel->mapFromSource(m_fileModel->index(targetPath)) : m_fileModel->index(targetPath);
                if (idx.isValid() && m_treeView) {
                    m_treeView->setCurrentIndex(idx);
                }
                if (m_statusLabel) m_statusLabel->setText(QString("Pasted clipboard image to %1").arg(QFileInfo(targetPath).fileName()));
                return;
            }
        }
    }

    // 2. Paste Plain Text Data from Clipboard (web page, editor, terminal)
    if (mimeData->hasText() && !mimeData->hasUrls()) {
        QString text = mimeData->text();
        if (!text.isEmpty()) {
            QString baseName = "clipboard";
            QString ext = ".txt";
            QString targetPath = QDir(m_currentPath).filePath(baseName + ext);
            int count = 1;
            while (QFile::exists(targetPath)) {
                targetPath = QDir(m_currentPath).filePath(QString("%1_%2%3").arg(baseName).arg(count++).arg(ext));
            }
            QFile file(targetPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << text;
                file.close();
                refresh();
                QModelIndex idx = m_proxyModel ? m_proxyModel->mapFromSource(m_fileModel->index(targetPath)) : m_fileModel->index(targetPath);
                if (idx.isValid() && m_treeView) {
                    m_treeView->setCurrentIndex(idx);
                }
                if (m_statusLabel) m_statusLabel->setText(QString("Pasted clipboard text to %1").arg(QFileInfo(targetPath).fileName()));
                return;
            }
        }
    }

    // 3. Standard File / Directory Copy & Move Operations
    if (!mimeData->hasUrls()) return;

    QList<QUrl> urls = mimeData->urls();
    QStringList srcPaths;
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            srcPaths.append(url.toLocalFile());
        }
    }

    if (srcPaths.isEmpty()) return;

    if (m_archiveViewActive) {
        QSettings settings("Amifiles", "Amifiles");
        bool enableArchiveNav = settings.value("preferences/archive_nav", true).toBool();
        if (enableArchiveNav) {
            bool archiveWriteEnabled = settings.value("preferences/archive_write", false).toBool();
            if (!archiveWriteEnabled) {
                QMessageBox::warning(this, "Write Mode Disabled", "Archive Write Mode is currently disabled. You can enable read-write permissions for archives and disk images in the View menu.");
                return;
            }
            if (m_archiveModel->addFiles(srcPaths)) {
                refresh();
            } else {
                QMessageBox::warning(this, "Archive Add Failed", "Could not add selected files to the archive.");
            }
            return;
        }
    }

    bool isCut = mimeData->hasFormat("application/amifiles-cut") || 
                 mimeData->hasFormat("application/x-kde-cutselection");

    if (isCut) {
        bool containsLocked = false;
        QString lockedPath;
        for (const QString& path : srcPaths) {
            if (!QFileInfo(path).isWritable() || isPathLockedPersistent(path)) {
                containsLocked = true;
                lockedPath = path;
                break;
            }
            QFileInfo info(path);
            if (info.isDir()) {
                QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString childPath = it.next();
                    if (!QFileInfo(childPath).isWritable() || isPathLockedPersistent(childPath)) {
                        containsLocked = true;
                        lockedPath = childPath;
                        break;
                    }
                }
            }
            if (containsLocked) break;
        }

        if (containsLocked) {
            QMessageBox::warning(this, "Operation Blocked",
                                 QString("One or more files/directories being moved (Cut) are locked (read-only) or contain locked items.\n"
                                         "Blocked item: %1\n\n"
                                         "Please unlock all files/folders first before moving them.")
                                 .arg(QDir::toNativeSeparators(lockedPath)));
            return;
        }
    }

    // Queue copy/move operations
    CopyQueueManager::instance().queueCopy(srcPaths, m_currentPath, isCut, this);

    if (isCut) {
        clipboard->clear();
    }
}

static bool isPathLockedPersistent(const QString& path) {
    QString p = QDir::cleanPath(path);
    while (!p.isEmpty() && p != "/" && p != ".") {
        if (TagManager::instance().isFileLocked(p)) {
            return true;
        }
        QFileInfo info(p);
        QString parent = info.absolutePath();
        if (parent == p) break;
        p = parent;
    }
    return false;
}

void FilePanel::onDelete() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    // Check if any selected paths or nested files/directories are locked (read-only)
    bool containsLocked = false;
    QString lockedPath;
    for (const QString& path : paths) {
        if (!QFileInfo(path).isWritable() || isPathLockedPersistent(path)) {
            containsLocked = true;
            lockedPath = path;
            break;
        }
        if (QFileInfo(path).isDir()) {
            QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString childPath = it.next();
                if (!QFileInfo(childPath).isWritable() || isPathLockedPersistent(childPath)) {
                    containsLocked = true;
                    lockedPath = childPath;
                    break;
                }
            }
        }
        if (containsLocked) break;
    }

    if (containsLocked) {
        QMessageBox::warning(this, "Deletion Blocked",
            QString("Cannot delete: One or more selected items (or their nested files/folders) are locked.\n\nLocked item found:\n%1\n\nPlease unlock it first.")
            .arg(lockedPath));
        return;
    }

    QString msg = QString("Are you sure you want to permanently delete the %1 selected item(s)?")
                  .arg(paths.size());
    
    auto button = QMessageBox::question(this, "Confirm Delete", msg, 
                                       QMessageBox::Yes | QMessageBox::No);
    
    if (button == QMessageBox::Yes) {
        if (m_archiveViewActive) {
            QSettings settings("Amifiles", "Amifiles");
            bool enableArchiveNav = settings.value("preferences/archive_nav", true).toBool();
            if (enableArchiveNav) {
                bool archiveWriteEnabled = settings.value("preferences/archive_write", false).toBool();
                if (!archiveWriteEnabled) {
                    QMessageBox::warning(this, "Write Mode Disabled", "Archive Write Mode is currently disabled. You can enable read-write permissions for archives and disk images in the View menu.");
                    return;
                }
                if (m_archiveModel->deleteFiles(paths)) {
                    refresh();
                } else {
                    QMessageBox::warning(this, "Archive Delete Failed", "Could not delete selected items from the archive.");
                }
                return;
            }
        }

        QStringList failedPaths;
        for (const QString& path : paths) {
            QFileInfo info(path);
            bool ok = false;
            if (info.isDir()) {
                ok = QDir(path).removeRecursively();
            } else {
                ok = QFile::remove(path);
            }
            if (!ok) {
                failedPaths.append(path);
            }
        }
        if (!failedPaths.isEmpty()) {
            QMessageBox::warning(this, "Delete Failed",
                                 QString("Could not delete some items. Please check if they are locked or read-only:\n- %1")
                                 .arg(failedPaths.first() + (failedPaths.size() > 1 ? QString(" and %1 others").arg(failedPaths.size() - 1) : "")));
        }
        refresh();
    }
}

void FilePanel::onRename() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    QString oldPath = paths.first();
    QFileInfo info(oldPath);
    QString oldName = info.fileName();
    QString oldExt = info.suffix();

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename File", 
                                            "Enter new name:", QLineEdit::Normal,
                                            oldName, &ok);
    if (ok && !newName.isEmpty() && newName != oldName) {
        QSettings settings("Amifiles", "Amifiles");
        bool keepExt = settings.value("behavior/keep_extension_on_rename", true).toBool();
        if (keepExt && !oldExt.isEmpty() && !info.isDir()) {
            QString dotExt = "." + oldExt;
            if (!newName.endsWith(dotExt, Qt::CaseInsensitive)) {
                newName += dotExt;
            }
        }
        QString newPath = info.dir().filePath(newName);
        if (QFile::rename(oldPath, newPath)) {
            TagManager::instance().renamePathInDatabase(oldPath, newPath);
            QFile(newPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
            refresh();
        } else {
            QMessageBox::warning(this, "Rename Failed", "Could not rename the selected item.");
        }
    }
}

void FilePanel::onNewFolder() {
    bool ok;
    QString folderName = QInputDialog::getText(this, "New Folder",
                                               "Enter folder name:", QLineEdit::Normal,
                                               "New Folder", &ok);
    if (ok && !folderName.isEmpty()) {
        if (m_archiveViewActive) {
            if (m_archiveModel->createDirectory(folderName)) {
                refresh();
            } else {
                QMessageBox::warning(this, "Error", "Could not create folder in archive. Note: D64 archives do not support directories.");
            }
        } else {
            QDir dir(m_currentPath);
            QString finalName = folderName;
            if (dir.exists(finalName)) {
                int index = 2;
                while (dir.exists(QString("%1 (%2)").arg(folderName).arg(index))) {
                    index++;
                }
                finalName = QString("%1 (%2)").arg(folderName).arg(index);
            }
            if (dir.mkdir(finalName)) {
                refresh();
            } else {
                QMessageBox::warning(this, "Error", "Could not create folder.");
            }
        }
    }
}

void FilePanel::onAdvancedNewFolder() {
    AdvancedNewFolderDialog dlg(m_currentPath, this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
    }
}

void FilePanel::onCopyFileName() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;
    QStringList names;
    for (const QString& path : paths) {
        names.append(QFileInfo(path).fileName());
    }
    QGuiApplication::clipboard()->setText(names.join("\n"));
}

void FilePanel::onCopyPath() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) {
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(m_currentPath));
    } else {
        QStringList nativePaths;
        for (const QString& path : paths) {
            nativePaths.append(QDir::toNativeSeparators(path));
        }
        QGuiApplication::clipboard()->setText(nativePaths.join("\n"));
    }
}

void FilePanel::onCopyFolderContents() {
    QStringList paths = selectedPaths();
    QStringList targets;
    if (paths.isEmpty()) {
        targets.append(m_currentPath);
    } else {
        for (const QString& p : paths) {
            if (QFileInfo(p).isDir()) {
                targets.append(p);
            }
        }
    }
    if (targets.isEmpty()) return;

    QStringList results;
    for (const QString& target : targets) {
        QDirIterator it(target, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            results.append(QDir::toNativeSeparators(it.next()));
        }
    }
    QGuiApplication::clipboard()->setText(results.join("\n"));
}

void FilePanel::onShowProperties() {
    QStringList paths = selectedPaths();
    QString targetPath = paths.isEmpty() ? m_currentPath : paths.first();

    FileMetadata meta = MetadataExtractor::extract(targetPath);
    
    QString details = QString(
        "Name: \t%1\n"
        "Path: \t%2\n"
        "Size: \t%3 bytes\n"
        "Permissions: \t%4\n"
        "Created: \t%5\n"
        "Modified: \t%6\n"
    ).arg(meta.name)
     .arg(meta.path)
     .arg(meta.size)
     .arg(meta.permissions)
     .arg(meta.created)
     .arg(meta.modified);

    if (meta.imageDimensions.isValid()) {
        details += QString("Dimensions: \t%1 x %2\nFormat: \t%3\n")
            .arg(meta.imageDimensions.width())
            .arg(meta.imageDimensions.height())
            .arg(meta.imageFormat);
    }
    if (!meta.title.isEmpty() || !meta.artist.isEmpty()) {
        details += QString("Audio Title: \t%1\nArtist: \t%2\nAlbum: \t%3\n")
            .arg(meta.title)
            .arg(meta.artist)
            .arg(meta.album);
    }

    QMessageBox::information(this, "Properties", details);
}

static void scanMediaFilesRecursively(const QString& folderPath, QStringList& playlistPaths, int mediaTypeFilter, int depth) {
    if (depth > 3) return;
    QFileInfo fi(folderPath);
    if (fi.isSymLink()) return;

    QDir dir(folderPath);
    QStringList mediaExts;
    if (mediaTypeFilter == 1) { // Audio only
        mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "mod", "sid", "s3m", "xm", "it" };
    } else if (mediaTypeFilter == 2) { // Video only
        mediaExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
    } else { // All
        mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "mod", "sid", "s3m", "xm", "it", "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
    }
    
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fInfo : files) {
        if (fInfo.isSymLink()) continue;
        if (mediaExts.contains(fInfo.suffix().toLower())) {
            playlistPaths.append(fInfo.absoluteFilePath());
        }
    }
    
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& sub : subdirs) {
        scanMediaFilesRecursively(sub.absoluteFilePath(), playlistPaths, mediaTypeFilter, depth + 1);
    }
}

static bool hasAudioFilesRecursively(const QString& folderPath, int depth) {
    if (depth > 3) return false;
    QFileInfo fi(folderPath);
    if (fi.isSymLink()) return false;

    QDir dir(folderPath);
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fInfo : files) {
        if (fInfo.isSymLink()) continue;
        QString ext = fInfo.suffix().toLower();
        if (ext == "mp3" || ext == "flac" || ext == "wav" || ext == "ogg" || ext == "m4a" || ext == "aac" || ext == "wma" || ext == "mod" || ext == "sid" || ext == "s3m" || ext == "xm" || ext == "it") return true;
    }

    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& sub : subdirs) {
        if (hasAudioFilesRecursively(sub.absoluteFilePath(), depth + 1)) return true;
    }
    return false;
}

void FilePanel::onCustomContextMenu(const QPoint& pos) {
    QAbstractItemView* grid = qobject_cast<QAbstractItemView*>(sender());
    if (grid) {
        QModelIndex index = grid->indexAt(pos);
        if (index.isValid()) {
            if (!grid->selectionModel()->isSelected(index)) {
                grid->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                grid->setCurrentIndex(index);
            }
        }
    }

    int vMode = viewModeIndex();
    if (vMode == 6) {
        showAudioShowcaseContextMenu(pos);
        return;
    } else if (vMode == 10 || vMode == 11) {
        showMusicShowcaseContextMenu(pos);
        return;
    } else if (vMode == 7 || vMode == 8 || vMode == 9) {
        showVideoShowcaseContextMenu(pos);
        return;
    }
    QModelIndex index;
    if (m_viewStack->currentWidget() == m_listView) {
        index = m_listView->indexAt(pos);
    } else if (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer) {
        index = m_theaterListView->indexAt(pos);
    } else if (m_viewStack->currentWidget() == m_coverFlowView) {
        index = m_coverFlowView->indexAt(pos);
    } else {
        index = m_treeView->indexAt(pos);
    }
    
    if (index.isValid()) {
        QWidget* cur = m_viewStack->currentWidget();
        if (cur == m_coverFlowView) {
            m_coverFlowView->setSelectedIndex(index.row());
        } else {
            QAbstractItemView* activeView = nullptr;
            if (cur == m_theaterContainer) {
                activeView = m_theaterListView;
            } else {
                activeView = qobject_cast<QAbstractItemView*>(cur);
            }
            if (activeView && activeView->selectionModel()) {
                if (!activeView->selectionModel()->isSelected(index)) {
                    activeView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    activeView->setCurrentIndex(index);
                }
            }
        }
    }
    
    // Dynamic context menu initialization

    QMenu menu(this);
    QStyle* style = QApplication::style();
    bool isTheater = (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer);

    QStringList curSelected = selectedPaths();
    QString selectedPath;
    bool isFolder = false;
    bool isFavorite = false;
    if (!curSelected.isEmpty()) {
        selectedPath = curSelected.first();
        QFileInfo fi(selectedPath);
        isFolder = fi.isDir();
        isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
    }

    QWidget* activeViewWidget = m_viewStack->currentWidget();
    bool isNewView = (activeViewWidget == m_millerView || activeViewWidget == m_timelineView || activeViewWidget == m_filmstripView || activeViewWidget == m_theaterListView || activeViewWidget == m_theaterContainer);

    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_context_menu_v4").toString();
    QJsonArray arr;
    if (jsonStr.isEmpty()) {
        arr = getDefaultContextMenuJson();
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        arr = doc.array();
    }

    QMap<QAction*, QString> actionCommands;
    for (int i = 0; i < arr.size(); ++i) {
        createContextMenuAction(&menu, arr[i].toObject(), curSelected, index, actionCommands);
    }

    if (!isFolder && !selectedPath.isEmpty()) {
        QString ext = QFileInfo(selectedPath).suffix().toLower();
        QStringList archiveExts = { "zip", "tar", "gz", "xz", "bz2", "tgz", "rar", "7z", "adf", "adz", "d64", "d71", "d81", "g64", "iso", "img" };
        if (archiveExts.contains(ext)) {
            bool hasExtract = false;
            for (QAction* act : menu.actions()) {
                if (actionCommands.value(act) == "app.extract_archive") {
                    hasExtract = true;
                    break;
                }
            }
            if (!hasExtract) {
                QAction* extractHereAct = new QAction(QIcon::fromTheme("package-x-generic"), "Extract Here", &menu);
                actionCommands[extractHereAct] = "app.extract_here";

                QFileInfo info(selectedPath);
                QString baseName = info.completeBaseName();
                if (baseName.endsWith(".tar", Qt::CaseInsensitive)) baseName.chop(4);

                QAction* extractSubAct = new QAction(QIcon::fromTheme("package-x-generic"), QString("Extract to '%1/'").arg(baseName), &menu);
                actionCommands[extractSubAct] = "app.extract_subfolder";

                QAction* extractAct = new QAction(QIcon::fromTheme("package-x-generic"), "Extract Archive...", &menu);
                actionCommands[extractAct] = "app.extract_archive";

                QAction* openAct = nullptr;
                for (QAction* act : menu.actions()) {
                    if (actionCommands.value(act) == "app.open" || actionCommands.value(act) == "app.open_fullscreen") {
                        openAct = act;
                        break;
                    }
                }
                if (openAct) {
                    int idx = menu.actions().indexOf(openAct);
                    if (idx != -1) {
                        QAction* nextAct = (idx + 1 < menu.actions().size()) ? menu.actions().at(idx + 1) : nullptr;
                        if (nextAct) {
                            menu.insertAction(nextAct, extractAct);
                            menu.insertAction(nextAct, extractSubAct);
                            menu.insertAction(nextAct, extractHereAct);
                            menu.insertSeparator(nextAct);
                        } else {
                            menu.addAction(extractAct);
                            menu.addAction(extractSubAct);
                            menu.addAction(extractHereAct);
                        }
                    }
                } else {
                    if (!menu.actions().isEmpty()) {
                        QAction* firstAct = menu.actions().first();
                        menu.insertAction(firstAct, extractAct);
                        menu.insertAction(firstAct, extractSubAct);
                        menu.insertAction(firstAct, extractHereAct);
                    }
                }
            }
        }
    }

    if (isFolder && !selectedPath.isEmpty()) {
        QSettings settings("Amifiles", "Amifiles");
        QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
        bool isPinned = false;
        for (const QString& item : pinned) {
            if (item.startsWith(selectedPath + ";")) {
                isPinned = true;
                break;
            }
        }
        QAction* pinAct = new QAction(QIcon::fromTheme("bookmark-new"), isPinned ? "📌 Unpin from Home Screen" : "📌 Pin to Home Screen", &menu);
        actionCommands[pinAct] = "app.pin_home";

        QAction* insertBeforeAct = nullptr;
        for (QAction* act : menu.actions()) {
            if (actionCommands.value(act) == "app.open" || actionCommands.value(act) == "app.open_fullscreen") {
                insertBeforeAct = act;
            }
        }
        if (insertBeforeAct) {
            int idx = menu.actions().indexOf(insertBeforeAct);
            QAction* nextAct = (idx + 1 < menu.actions().size()) ? menu.actions().at(idx + 1) : nullptr;
            if (nextAct) {
                menu.insertAction(nextAct, pinAct);
                menu.insertSeparator(nextAct);
            } else {
                menu.addAction(pinAct);
            }
        } else {
            if (!menu.actions().isEmpty()) {
                menu.insertAction(menu.actions().first(), pinAct);
                menu.insertSeparator(menu.actions().first());
            } else {
                menu.addAction(pinAct);
            }
        }
    }

    if (selectedPath.isEmpty() && !m_currentPath.isEmpty() && m_currentPath != "smart://home") {
        QSettings settings("Amifiles", "Amifiles");
        QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
        bool isPinned = false;
        for (const QString& item : pinned) {
            if (item.startsWith(m_currentPath + ";")) {
                isPinned = true;
                break;
            }
        }
        QAction* pinAct = new QAction(QIcon::fromTheme("bookmark-new"), isPinned ? "📌 Unpin Current Directory from Home Screen" : "📌 Pin Current Directory to Home Screen", &menu);
        actionCommands[pinAct] = "app.pin_home";

        QAction* pasteAct = nullptr;
        for (QAction* act : menu.actions()) {
            if (actionCommands.value(act) == "app.paste") {
                pasteAct = act;
                break;
            }
        }
        if (pasteAct) {
            menu.insertAction(pasteAct, pinAct);
            menu.insertSeparator(pasteAct);
        } else {
            menu.addAction(pinAct);
        }
    }

    if (!curSelected.isEmpty()) {
        bool hasCreate = false;
        for (QAction* act : menu.actions()) {
            if (actionCommands.value(act) == "app.create_archive") {
                hasCreate = true;
                break;
            }
        }
        if (!hasCreate) {
            QAction* createAct = new QAction(QIcon::fromTheme("package-x-generic"), "Create Archive...", &menu);
            actionCommands[createAct] = "app.create_archive";
            
            QAction* insertAfterAct = nullptr;
            for (QAction* act : menu.actions()) {
                QString cmd = actionCommands.value(act);
                if (cmd == "app.extract_archive") {
                    insertAfterAct = act;
                    break;
                }
            }
            if (!insertAfterAct) {
                for (QAction* act : menu.actions()) {
                    QString cmd = actionCommands.value(act);
                    if (cmd == "app.open" || cmd == "app.open_fullscreen") {
                        insertAfterAct = act;
                        break;
                    }
                }
            }
            
            if (insertAfterAct) {
                int idx = menu.actions().indexOf(insertAfterAct);
                if (idx != -1) {
                    if (idx + 1 < menu.actions().size()) {
                        menu.insertAction(menu.actions().at(idx + 1), createAct);
                    } else {
                        menu.addAction(createAct);
                    }
                }
            } else {
                if (!menu.actions().isEmpty()) {
                    menu.insertAction(menu.actions().first(), createAct);
                } else {
                    menu.addAction(createAct);
                }
            }
        }
    }

    // Execute menu on the active view layout widget
    QPoint globalPos = QCursor::pos();
    QAction* selected = menu.exec(globalPos);
    if (!selected) return;

    QString command = actionCommands.value(selected);
    if (command.isEmpty()) return;

    if (command.startsWith("app.apply_profile:")) {
        QString profileName = command.mid(QString("app.apply_profile:").length());
        QWidget* parentW = parentWidget();
        while (parentW && !parentW->inherits("MainWindow")) {
            parentW = parentW->parentWidget();
        }
        MainWindow* mw = qobject_cast<MainWindow*>(parentW);
        if (mw) {
            mw->onApplyProfileToCurrentFolder(profileName);
        }
        return;
    }

    if (command == "app.folder_layouts" || command == "app.save_folder_profile" ||
        command == "app.save_default_profile" || command == "app.load_default_profile") {
        QWidget* parentW = parentWidget();
        while (parentW && !parentW->inherits("MainWindow")) {
            parentW = parentW->parentWidget();
        }
        MainWindow* mw = qobject_cast<MainWindow*>(parentW);
        if (mw) {
            if (command == "app.folder_layouts") {
                mw->onConfigureFolderLayouts();
            } else if (command == "app.save_folder_profile") {
                mw->onSaveFolderProfileForCurrentDir();
            } else if (command == "app.save_default_profile") {
                mw->onSaveDefaultProfile();
            } else if (command == "app.load_default_profile") {
                mw->onLoadDefaultProfile();
            }
        }
        return;
    }

    if (command == "app.open") {
        if (isNewView) {
            if (!selectedPath.isEmpty()) {
                QFileInfo fi(selectedPath);
                if (fi.isDir()) {
                    navigateTo(selectedPath, true);
                } else {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(selectedPath));
                }
            }
        } else {
            onDoubleClicked(index);
        }
    } else if (command == "app.open_fullscreen") {
        if (!selectedPath.isEmpty()) {
            int targetViewMode = 10; // Default to Music Full Screen (Showcase v2)
            bool containsVideo = false;
            QStringList videoExts = { "mp4", "mkv", "avi", "mov", "webm", "mpeg", "mpg" };
            QDir dir(selectedPath);
            QFileInfoList fileList = dir.entryInfoList(QDir::Files);
            for (const QFileInfo& fi : fileList) {
                if (videoExts.contains(fi.suffix().toLower())) {
                    containsVideo = true;
                    break;
                }
            }
            if (containsVideo) {
                bool isTv = false;
                for (const QString& subDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    if (subDir.toLower().contains("season") || subDir.toLower().contains("series")) {
                        isTv = true;
                        break;
                    }
                }
                targetViewMode = isTv ? 9 : 8;
            }
            m_comboViewMode->setCurrentIndex(targetViewMode);
            navigateTo(selectedPath, true);
        }
    } else if (command == "app.copy") {
        onCopy();
    } else if (command == "app.cut") {
        onCut();
    } else if (command == "app.paste") {
        onPaste();
    } else if (command == "app.copy_filename") {
        onCopyFileName();
    } else if (command == "app.copy_path") {
        onCopyPath();
    } else if (command == "app.copy_folder_contents") {
        onCopyFolderContents();
    } else if (command == "app.copy_sibling") {
        QWidget* p = parentWidget();
        while (p && !p->inherits("MainWindow")) {
            p = p->parentWidget();
        }
        if (p) {
            QMetaObject::invokeMethod(p, "onCopyToSiblingAction");
        }
    } else if (command == "app.move_sibling") {
        QWidget* p = parentWidget();
        while (p && !p->inherits("MainWindow")) {
            p = p->parentWidget();
        }
        if (p) {
            QMetaObject::invokeMethod(p, "onMoveToSiblingAction");
        }
    } else if (command == "app.delete") {
        onDelete();
    } else if (command == "app.rename") {
        QStringList paths = selectedPaths();
        if (paths.size() > 1) {
            BulkRenameDialog dlg(paths, this);
            if (dlg.exec() == QDialog::Accepted) {
                refresh();
            }
        } else {
            onRename();
        }
    } else if (command == "app.bulk_rename") {
        QStringList paths = selectedPaths();
        if (!paths.isEmpty()) {
            BulkRenameDialog dlg(paths, this);
            if (dlg.exec() == QDialog::Accepted) {
                refresh();
            }
        }
    } else if (command == "app.new_folder") {
        onNewFolder();
    } else if (command == "app.advanced_new_folder") {
        onAdvancedNewFolder();
    } else if (command == "app.play_preview") {
        QString folderToCheck = isFolder ? selectedPath : m_currentPath;
        QStringList playlistPaths;
        if (!folderToCheck.isEmpty()) {
            if (isFolder) {
                QSettings settings("Amifiles", "Amifiles");
                bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool() && (m_viewStack->currentWidget() == m_theaterContainer);
                if (groupMultiDisc) {
                    QFileInfo folderInfo(folderToCheck);
                    QString parentDir = folderInfo.absolutePath();
                    QDir dir(parentDir);
                    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                    QString currentCleaned = FileFilterProxyModel::cleanAlbumFolderName(folderInfo.fileName());
                    for (const QString& subDirName : subDirs) {
                        if (FileFilterProxyModel::cleanAlbumFolderName(subDirName) == currentCleaned) {
                            scanMediaFilesRecursively(dir.filePath(subDirName), playlistPaths, 1);
                        }
                    }
                } else {
                    scanMediaFilesRecursively(folderToCheck, playlistPaths, 1);
                }
            } else {
                QDir dir(folderToCheck);
                QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg", "mod", "sid", "s3m", "xm", "it" };
                QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
                for (const QFileInfo& fInfo : files) {
                    if (mediaExts.contains(fInfo.suffix().toLower())) {
                        playlistPaths.append(fInfo.absoluteFilePath());
                    }
                }
            }
        }
        emit playlistPlayRequested(playlistPaths);
    } else if (command == "app.play_fullscreen_playlist") {
        QString folderToCheck = isFolder ? selectedPath : m_currentPath;
        QStringList playlistPaths;
        if (!folderToCheck.isEmpty()) {
            if (isFolder) {
                QSettings settings("Amifiles", "Amifiles");
                bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool() && (m_viewStack->currentWidget() == m_theaterContainer);
                if (groupMultiDisc) {
                    QFileInfo folderInfo(folderToCheck);
                    QString parentDir = folderInfo.absolutePath();
                    QDir dir(parentDir);
                    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                    QString currentCleaned = FileFilterProxyModel::cleanAlbumFolderName(folderInfo.fileName());
                    for (const QString& subDirName : subDirs) {
                        if (FileFilterProxyModel::cleanAlbumFolderName(subDirName) == currentCleaned) {
                            scanMediaFilesRecursively(dir.filePath(subDirName), playlistPaths, 1);
                        }
                    }
                } else {
                    scanMediaFilesRecursively(folderToCheck, playlistPaths, 1);
                }
            } else {
                QDir dir(folderToCheck);
                QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg", "mod", "sid", "s3m", "xm", "it" };
                QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
                for (const QFileInfo& fInfo : files) {
                    if (mediaExts.contains(fInfo.suffix().toLower())) {
                        playlistPaths.append(fInfo.absoluteFilePath());
                    }
                }
            }
        }
        emit playMediaBuiltinRequested(playlistPaths);
    } else if (command == "app.favorites") {
        if (isFolder && !selectedPath.isEmpty()) {
            FavoritesManager& fm = FavoritesManager::instance();
            if (isFavorite) {
                fm.removeFavorite(selectedPath);
            } else {
                fm.addFavorite(selectedPath);
            }
            updateFavoritesUI();
        } else {
            onFavoriteClicked();
        }
    } else if (command == "app.pin_home") {
        QString pathTarget = !selectedPath.isEmpty() ? selectedPath : m_currentPath;
        if (!pathTarget.isEmpty() && pathTarget != "smart://home") {
            QSettings settings("Amifiles", "Amifiles");
            QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
            int existingIdx = -1;
            for (int i = 0; i < pinned.size(); ++i) {
                if (pinned[i].startsWith(pathTarget + ";")) {
                    existingIdx = i;
                    break;
                }
            }
            if (existingIdx != -1) {
                pinned.removeAt(existingIdx);
                settings.setValue("dashboard/pinned_folders", pinned);
                QMessageBox::information(this, "Unpinned Folder", "Folder successfully unpinned from Home Screen.");
            } else {
                QString entry = QString("%1;%2;%3").arg(pathTarget).arg(QFileInfo(pathTarget).fileName()).arg(viewModeIndex());
                pinned.append(entry);
                settings.setValue("dashboard/pinned_folders", pinned);
                QMessageBox::information(this, "Pinned Folder", "Folder successfully pinned to Home Screen with layout memory!");
            }
            if (m_homeDashboardWidget) {
                m_homeDashboardWidget->refreshDashboard();
            }
        }
    } else if (command == "app.compare_selected") {
        VisualDiffDialog dlg(curSelected[0], curSelected[1], this);
        dlg.exec();
    } else if (command == "app.compare_sibling") {
        QString sibSelectedPath;
        if (m_siblingPanel) {
            QStringList sibSelected = m_siblingPanel->selectedPaths();
            if (!sibSelected.isEmpty()) {
                sibSelectedPath = sibSelected.first();
            }
        }
        VisualDiffDialog dlg(curSelected[0], sibSelectedPath, this);
        dlg.exec();
    } else if (command == "app.media_info_sheet") {
        ShowcaseInfoDialog infoDlg(selectedPath, this);
        connect(&infoDlg, &ShowcaseInfoDialog::playRequested, this, [this](const QString& path) {
            emit playMediaBuiltinRequested({path});
        });
        connect(&infoDlg, &ShowcaseInfoDialog::openFolderRequested, this, [this](const QString& path) {
            navigateTo(path, true);
        });
        connect(&infoDlg, &ShowcaseInfoDialog::watchStatusChanged, this, [this](const QString& p, bool) {
            notifyPathDataChanged(p);
        });
        infoDlg.exec();
    } else if (command == "app.toggle_watch") {
        QSettings settings("Amifiles", "Amifiles");
        bool isWatched = settings.value("watched/" + selectedPath, false).toBool();
        settings.setValue("watched/" + selectedPath, !isWatched);
        notifyPathDataChanged(selectedPath);
    } else if (command == "app.metadata_casing_wizard") {
        MetadataCasingDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.edit_tags") {
        TagEditorDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.advanced_tag_editor") {
        AdvancedTagEditorDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.fetch_musicbrainz") {
        TagEditorDialog dlg(curSelected, this, true);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.scrape_video") {
        VideoScraperDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.fetch_folder_art") {
        QString folderPath = isFolder ? selectedPath : m_currentPath;
        if (!folderPath.isEmpty()) {
            FolderArtScraperDialog dlg(folderPath, this);
            dlg.exec();
            refresh();
        }
    } else if (command == "app.file_tags") {
        FileTagsDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
            QWidget* p = parentWidget();
            while (p && !p->inherits("MainWindow")) {
                p = p->parentWidget();
            }
            if (p) {
                QMetaObject::invokeMethod(p, "refreshTagsSidebar");
            }
        }
    } else if (command == "app.encrypt_vault") {
        VaultDialog dlg(true, selectedPath, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        }
    } else if (command == "app.decrypt_vault") {
        VaultDialog dlg(false, selectedPath, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        }
    } else if (command == "app.mount_iso") {
        QString errorMsg, mountPath;
        if (RemoteMountManager::mountIso(selectedPath, errorMsg, mountPath)) {
            QMessageBox::information(this, "Mount ISO", QString("ISO mounted successfully at:\n%1").arg(mountPath));
            QWidget* parentW = parentWidget();
            while (parentW && !parentW->inherits("MainWindow")) {
                parentW = parentW->parentWidget();
            }
            if (MainWindow* mw = qobject_cast<MainWindow*>(parentW)) {
                mw->updateDrivesList();
            }
            if (!mountPath.isEmpty()) {
                setPath(mountPath);
            } else {
                refresh();
            }
        } else {
            QMessageBox::critical(this, "Mount ISO Error", QString("Failed to mount ISO:\n%1").arg(errorMsg));
        }
    } else if (command == "app.unmount_iso") {
        QString errorMsg;
        if (RemoteMountManager::unmountIso(selectedPath, errorMsg)) {
            QMessageBox::information(this, "Unmount ISO", "ISO unmounted successfully.");
            QWidget* parentW = parentWidget();
            while (parentW && !parentW->inherits("MainWindow")) {
                parentW = parentW->parentWidget();
            }
            if (MainWindow* mw = qobject_cast<MainWindow*>(parentW)) {
                mw->updateDrivesList();
            }
            refresh();
        } else {
            QMessageBox::critical(this, "Unmount ISO Error", QString("Failed to unmount ISO:\n%1").arg(errorMsg));
        }
    } else if (command == "app.mount_vhd") {
        QString errorMsg, mountPath;
        if (RemoteMountManager::mountVhd(selectedPath, errorMsg, mountPath)) {
            QMessageBox::information(this, "Mount VHD", QString("VHD mounted successfully at:\n%1").arg(mountPath));
            QWidget* parentW = parentWidget();
            while (parentW && !parentW->inherits("MainWindow")) {
                parentW = parentW->parentWidget();
            }
            if (MainWindow* mw = qobject_cast<MainWindow*>(parentW)) {
                mw->updateDrivesList();
            }
            if (!mountPath.isEmpty()) {
                setPath(mountPath);
            } else {
                refresh();
            }
        } else {
            QMessageBox::critical(this, "Mount VHD Error", QString("Failed to mount VHD:\n%1").arg(errorMsg));
        }
    } else if (command == "app.unmount_vhd") {
        QString errorMsg;
        if (RemoteMountManager::unmountVhd(selectedPath, errorMsg)) {
            QMessageBox::information(this, "Unmount VHD", "VHD unmounted successfully.");
            QWidget* parentW = parentWidget();
            while (parentW && !parentW->inherits("MainWindow")) {
                parentW = parentW->parentWidget();
            }
            if (MainWindow* mw = qobject_cast<MainWindow*>(parentW)) {
                mw->updateDrivesList();
            }
            refresh();
        } else {
            QMessageBox::critical(this, "Unmount VHD Error", QString("Failed to unmount VHD:\n%1").arg(errorMsg));
        }
    } else if (command == "app.create_archive") {
        ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeCreate, curSelected, m_currentPath, false, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QDialog::accepted, this, [this]() {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        });
        dlg->show();
    } else if (command == "app.create_secure_archive") {
        ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeCreate, curSelected, m_currentPath, true, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QDialog::accepted, this, [this]() {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        });
        dlg->show();
    } else if (command == "app.extract_archive") {
        ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeExtract, curSelected.first(), m_currentPath, false, true, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QDialog::accepted, this, [this]() {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        });
        dlg->show();
    } else if (command == "app.extract_here") {
        ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeExtract, curSelected.first(), m_currentPath, true, false, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QDialog::accepted, this, [this]() {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        });
        dlg->show();
    } else if (command == "app.extract_subfolder") {
        ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeExtract, curSelected.first(), m_currentPath, true, true, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QDialog::accepted, this, [this]() {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        });
        dlg->show();
    } else if (command == "app.calculate_checksum") {
        ChecksumDialog dlg(selectedPath, this);
        dlg.exec();
    } else if (command == "app.secure_shred") {
        ShredDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.disk_space_analyzer") {
        DiskSpaceAnalyzerDialog dlg(m_currentPath, this);
        connect(&dlg, &DiskSpaceAnalyzerDialog::locateFileRequested, this, &FilePanel::selectFilePath);
        dlg.exec();
    } else if (command == "app.image_convert") {
        QStringList imageExts = { "jpg", "jpeg", "png", "webp", "bmp" };
        QStringList selectedImages;
        for (const QString& sPath : curSelected) {
            if (imageExts.contains(QFileInfo(sPath).suffix().toLower())) {
                selectedImages.append(sPath);
            }
        }
        ImageConverterDialog dlg(selectedImages, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (command == "app.color_none") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "");
        refresh();
    } else if (command == "app.color_red") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "red");
        refresh();
    } else if (command == "app.color_orange") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "orange");
        refresh();
    } else if (command == "app.color_yellow") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "yellow");
        refresh();
    } else if (command == "app.color_green") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "green");
        refresh();
    } else if (command == "app.color_blue") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "blue");
        refresh();
    } else if (command == "app.color_purple") {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "purple");
        refresh();
    } else if (command == "app.color_custom_overlay") {
        IconPickerDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            QString iconName = dlg.selectedIconName();
            if (!iconName.isEmpty()) {
                for (const QString& path : curSelected) {
                    TagManager::instance().setFileOverlayIcon(path, iconName);
                }
                refresh();
            }
        }
    } else if (command == "app.color_clear_overlay") {
        for (const QString& path : curSelected) {
            TagManager::instance().setFileOverlayIcon(path, "");
        }
        refresh();
    } else if (command == "app.toggle_executable") {
        toggleSelectedExecutable();
    } else if (command == "app.change_permissions") {
        changeSelectedPermissions();
    } else if (command == "app.lock_folder_recursive") {
        lockSelectedFolderRecursive(true);
    } else if (command == "app.unlock_folder_recursive") {
        lockSelectedFolderRecursive(false);
    } else if (command == "app.create_symlink_sibling") {
        onCreateSymlinkInSiblingPane();
    } else if (command == "app.remove_green_screen") {
        removeSelectedGreenScreen();
    } else if (command == "app.properties") {
        onShowProperties();
    }
}

void FilePanel::showAudioShowcaseContextMenu(const QPoint& pos) {
    if (m_viewStack->currentWidget() == m_theaterListView) {
        QModelIndex index = m_theaterListView->indexAt(pos);
        if (index.isValid()) {
            if (!m_theaterListView->selectionModel()->isSelected(index)) {
                m_theaterListView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                m_theaterListView->setCurrentIndex(index);
            }
        }
    }

    QMenu menu(this);
    QStyle* style = QApplication::style();

    QAction* actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open");
    menu.addSeparator();

    // Add a Menu that says "Audio tools" as a test
    QMenu* audioToolsMenu = menu.addMenu("Audio tools");
    QAction* actTagEditor = audioToolsMenu->addAction("Batch Tag Editor...");
    connect(actTagEditor, &QAction::triggered, this, [this]() {
        QStringList curSelected = selectedPaths();
        if (!curSelected.isEmpty()) {
            TagEditorDialog dlg(curSelected, this);
            if (dlg.exec() == QDialog::Accepted) {
                refresh();
            }
        }
    });

    menu.addSeparator();

    QStringList curSelected = selectedPaths();
    QString selectedPath;
    bool isFolder = false;
    bool isFavorite = false;
    if (!curSelected.isEmpty()) {
        selectedPath = curSelected.first();
        QFileInfo info(selectedPath);
        isFolder = info.isDir();
        isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
    }

    QAction* actFav = nullptr;
    if (isFolder) {
        if (isFavorite) {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove from Favorites");
        } else {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add to Favorites");
        }
    } else {
        bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
        if (isCurrentFavorite) {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove Current from Favorites");
        } else {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add Current to Favorites");
        }
    }

    menu.addSeparator();

    QSettings settings("Amifiles", "Amifiles");
    bool zenActive = settings.value("preferences/zen_mode", false).toBool();
    bool builtinDoubleclick = settings.value("preferences/builtin_player_doubleclick", false).toBool();
    bool doubleclickQueue = settings.value("preferences/doubleclick_adds_to_queue", false).toBool();
    bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool();
    bool showInfoPanel = settings.value("audio_showcase/show_info_panel", true).toBool();
    bool hideAuxiliary = settings.value("audio_showcase/hide_active", true).toBool();

    QAction* actToggleInfoPanel = menu.addAction("Show Media Information Panel");
    actToggleInfoPanel->setCheckable(true);
    actToggleInfoPanel->setChecked(showInfoPanel);

    QAction* actToggleZen = menu.addAction("Clean Interface Mode (Zen Mode)");
    actToggleZen->setCheckable(true);
    actToggleZen->setChecked(zenActive);

    QAction* actToggleDoubleclick = menu.addAction("Double-click Plays Media in Built-in Fullscreen Player");
    actToggleDoubleclick->setCheckable(true);
    actToggleDoubleclick->setChecked(builtinDoubleclick);

    QAction* actToggleDoubleclickQueue = menu.addAction("Double-click Adds Media to Playlist Queue");
    actToggleDoubleclickQueue->setCheckable(true);
    actToggleDoubleclickQueue->setChecked(doubleclickQueue);

    QAction* actGroupMultiDisc = menu.addAction("Group Multi-Disc Albums");
    actGroupMultiDisc->setCheckable(true);
    actGroupMultiDisc->setChecked(groupMultiDisc);

    QAction* actHideAuxiliaryFiles = menu.addAction("Hide Auxiliary / Artwork Files");
    actHideAuxiliaryFiles->setCheckable(true);
    actHideAuxiliaryFiles->setChecked(hideAuxiliary);

    QAction* actHideExtensions = menu.addAction("Hide File Extensions...");

    QString casingType = settings.value("music_showcase/casing_type", "cd").toString();
    QMenu* casingMenu = menu.addMenu("Casing Style");
    QAction* actCasingClear = casingMenu->addAction("Clear CD Jewel Case");
    QAction* actCasingBlack = casingMenu->addAction("Black CD Jewel Case");
    QAction* actCasingBlackPremium = casingMenu->addAction("Black CD Jewel Case (Premium)");
    QAction* actCasingVinyl = casingMenu->addAction("Vinyl LP Record Sleeve");

    actCasingClear->setCheckable(true);
    actCasingBlack->setCheckable(true);
    actCasingBlackPremium->setCheckable(true);
    actCasingVinyl->setCheckable(true);

    if (casingType == "cd_black") actCasingBlack->setChecked(true);
    else if (casingType == "cd_black_premium") actCasingBlackPremium->setChecked(true);
    else if (casingType == "vinyl") actCasingVinyl->setChecked(true);
    else actCasingClear->setChecked(true);

    QAction* actConfigureFolderLayouts = menu.addAction("Configure Folder-Specific Layouts...");

    QAction* selected = menu.exec(QCursor::pos());
    if (!selected) return;

    if (selected == actOpen) {
        if (!selectedPath.isEmpty()) {
            onDoubleClickedPath(selectedPath);
        }
    } else if (selected == actFav) {
        if (isFolder) {
            if (isFavorite) {
                FavoritesManager::instance().removeFavorite(selectedPath);
            } else {
                FavoritesManager::instance().addFavorite(selectedPath);
            }
        } else {
            bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
            if (isCurrentFavorite) {
                FavoritesManager::instance().removeFavorite(m_currentPath);
            } else {
                FavoritesManager::instance().addFavorite(m_currentPath);
            }
        }
    } else if (selected == actToggleInfoPanel) {
        settings.setValue("audio_showcase/show_info_panel", !showInfoPanel);
        onSelectionChanged();
    } else if (selected == actToggleZen) {
        emit zenModeToggled(!zenActive);
    } else if (selected == actToggleDoubleclick) {
        settings.setValue("preferences/builtin_player_doubleclick", !builtinDoubleclick);
        emit mediaPlaybackSettingsChanged();
    } else if (selected == actToggleDoubleclickQueue) {
        settings.setValue("preferences/doubleclick_adds_to_queue", !doubleclickQueue);
        emit mediaPlaybackSettingsChanged();
    } else if (selected == actGroupMultiDisc) {
        settings.setValue("theater/group_multi_disc", !groupMultiDisc);
        m_proxyModel->setGroupMultiDiscActive(!groupMultiDisc);
        refresh();
        if (m_groupProxy && m_groupProxy->isGroupingActive() && m_viewStack->currentWidget() == m_theaterContainer) {
            queueRebuildTheaterGroups();
        }
    } else if (selected == actHideAuxiliaryFiles) {
        settings.setValue("audio_showcase/hide_active", !hideAuxiliary);
        updateHideSettings();
        refresh();
    } else if (selected == actHideExtensions) {
        promptHideExtensions();
    } else if (selected == actCasingClear || selected == actCasingBlack || selected == actCasingBlackPremium || selected == actCasingVinyl) {
        QString newCasing = "cd";
        if (selected == actCasingBlack) newCasing = "cd_black";
        else if (selected == actCasingBlackPremium) newCasing = "cd_black_premium";
        else if (selected == actCasingVinyl) newCasing = "vinyl";
        settings.setValue("music_showcase/casing_type", newCasing);
        if (m_proxyModel) {
            m_proxyModel->clearCasingCache();
        }
        refresh();
    } else if (selected == actConfigureFolderLayouts) {
        emit configureFolderLayoutsRequested();
    }
}

void FilePanel::showMusicShowcaseContextMenu(const QPoint& pos) {
    QModelIndex index;
    int vMode = viewModeIndex();
    if (vMode == 11 && m_coverFlowView) {
        index = m_coverFlowView->indexAt(pos);
        if (index.isValid()) {
            m_coverFlowView->setSelectedIndex(index.row());
        }
    } else if (m_viewStack->currentWidget() == m_theaterListView) {
        index = m_theaterListView->indexAt(pos);
        if (index.isValid()) {
            if (!m_theaterListView->selectionModel()->isSelected(index)) {
                m_theaterListView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                m_theaterListView->setCurrentIndex(index);
            }
        }
    }

    QMenu menu(this);
    QStyle* style = QApplication::style();

    QStringList curSelected = selectedPaths();
    QString selectedPath;
    bool isFolder = false;
    bool isFavorite = false;
    if (!curSelected.isEmpty()) {
        selectedPath = curSelected.first();
        QFileInfo info(selectedPath);
        isFolder = info.isDir();
        isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
    }

    QAction* actPlay = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), isFolder ? "Play Album" : "Play Track");
    QAction* actQueue = menu.addAction(style->standardIcon(QStyle::SP_MediaVolume), isFolder ? "Queue Album to Playlist" : "Queue Track to Playlist");
    QAction* actOpenFullscreen = nullptr;
    if (isFolder) {
        actOpenFullscreen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open in Full Screen View");
    }
    QAction* actApplyDvdOverlay = nullptr;
    if (!selectedPath.isEmpty() && !isFolder) {
        actApplyDvdOverlay = menu.addAction("💿 Apply DVD Case Overlay (Auto-Rename folder.jpg)");
    }
    menu.addSeparator();

    QAction* actFav = nullptr;
    if (isFolder) {
        if (isFavorite) {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove from Favorites");
        } else {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add to Favorites");
        }
    } else {
        bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
        if (isCurrentFavorite) {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove Current from Favorites");
        } else {
            actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add Current to Favorites");
        }
    }

    menu.addSeparator();

    QSettings settings("Amifiles", "Amifiles");
    bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool();
    bool hideAuxiliary = settings.value("audio_showcase/hide_active", true).toBool();

    QAction* actGroupMultiDisc = menu.addAction("Group Multi-Disc Albums");
    actGroupMultiDisc->setCheckable(true);
    actGroupMultiDisc->setChecked(groupMultiDisc);

    QAction* actHideAuxiliaryFiles = menu.addAction("Hide Auxiliary / Artwork Files");
    actHideAuxiliaryFiles->setCheckable(true);
    actHideAuxiliaryFiles->setChecked(hideAuxiliary);

    QAction* actHideExtensions = menu.addAction("Hide File Extensions...");

    QString casingType = settings.value("music_showcase/casing_type", "cd").toString();
    QMenu* casingMenu = menu.addMenu("Casing Style");
    QAction* actCasingClear = casingMenu->addAction("Clear CD Jewel Case");
    QAction* actCasingBlack = casingMenu->addAction("Black CD Jewel Case");
    QAction* actCasingBlackPremium = casingMenu->addAction("Black CD Jewel Case (Premium)");
    QAction* actCasingVinyl = casingMenu->addAction("Vinyl LP Record Sleeve");

    actCasingClear->setCheckable(true);
    actCasingBlack->setCheckable(true);
    actCasingBlackPremium->setCheckable(true);
    actCasingVinyl->setCheckable(true);

    if (casingType == "cd_black") actCasingBlack->setChecked(true);
    else if (casingType == "cd_black_premium") actCasingBlackPremium->setChecked(true);
    else if (casingType == "vinyl") actCasingVinyl->setChecked(true);
    else actCasingClear->setChecked(true);

    QAction* actConfigureFolderLayouts = menu.addAction("Configure Folder-Specific Layouts...");

    QAction* selected = menu.exec(QCursor::pos());
    if (!selected) return;

    if (selected == actPlay) {
        if (!selectedPath.isEmpty()) {
            QStringList playlistPaths;
            if (isFolder) {
                scanMediaFilesRecursively(selectedPath, playlistPaths, 1);
            } else {
                playlistPaths.append(selectedPath);
            }
            if (!playlistPaths.isEmpty()) {
                emit playMediaFullscreenRequested(playlistPaths);
            }
        }
    } else if (selected == actOpenFullscreen) {
        if (!selectedPath.isEmpty()) {
            int targetViewMode = 10; // Default to Music Full Screen (Showcase v2)
            bool containsVideo = false;
            QStringList videoExts = { "mp4", "mkv", "avi", "mov", "webm", "mpeg", "mpg" };
            QDir dir(selectedPath);
            QFileInfoList fileList = dir.entryInfoList(QDir::Files);
            for (const QFileInfo& fi : fileList) {
                if (videoExts.contains(fi.suffix().toLower())) {
                    containsVideo = true;
                    break;
                }
            }
            if (containsVideo) {
                bool isTv = false;
                for (const QString& subDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    if (subDir.toLower().contains("season") || subDir.toLower().contains("series")) {
                        isTv = true;
                        break;
                    }
                }
                targetViewMode = isTv ? 9 : 8;
            }
            m_comboViewMode->setCurrentIndex(targetViewMode);
            navigateTo(selectedPath, true);
        }
    } else if (selected == actQueue) {
        if (!selectedPath.isEmpty()) {
            QStringList playlistPaths;
            if (isFolder) {
                scanMediaFilesRecursively(selectedPath, playlistPaths, 1);
            } else {
                playlistPaths.append(selectedPath);
            }
            if (!playlistPaths.isEmpty()) {
                emit queueMediaBuiltinRequested(playlistPaths);
            }
        }
    } else if (selected == actApplyDvdOverlay) {
        QFileInfo info(selectedPath);
        QString dirPath = info.absolutePath();
        QString baseName = info.completeBaseName();
        QStringList genericImages = { "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png" };
        QString foundGeneric;
        for (const QString& name : genericImages) {
            QFileInfo fi(QDir(dirPath).filePath(name));
            if (fi.exists()) {
                foundGeneric = fi.absoluteFilePath();
                break;
            }
        }
        if (!foundGeneric.isEmpty()) {
            QString ext = QFileInfo(foundGeneric).suffix();
            QString destPath = QDir(dirPath).filePath(baseName + "_cover." + ext);
            if (QFile::rename(foundGeneric, destPath)) {
                m_proxyModel->clearCasingCache();
                refresh();
                QMessageBox::information(this, "Overlay Applied", QString("Successfully renamed '%1' to '%2' to activate the casing overlay.").arg(QFileInfo(foundGeneric).fileName()).arg(QFileInfo(destPath).fileName()));
            } else {
                QMessageBox::warning(this, "Rename Failed", "Could not rename the generic artwork file.");
            }
        } else {
            QMessageBox::warning(this, "No Artwork Found", "No generic artwork (folder.jpg, poster.jpg, or cover.jpg) was found in this folder to rename.");
        }
    } else if (selected == actFav) {
        if (isFolder) {
            if (isFavorite) {
                FavoritesManager::instance().removeFavorite(selectedPath);
            } else {
                FavoritesManager::instance().addFavorite(selectedPath);
            }
        } else {
            bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
            if (isCurrentFavorite) {
                FavoritesManager::instance().removeFavorite(m_currentPath);
            } else {
                FavoritesManager::instance().addFavorite(m_currentPath);
            }
        }
    } else if (selected == actGroupMultiDisc) {
        settings.setValue("theater/group_multi_disc", !groupMultiDisc);
        m_proxyModel->setGroupMultiDiscActive(!groupMultiDisc);
        refresh();
        if (m_groupProxy && m_groupProxy->isGroupingActive() && m_viewStack->currentWidget() == m_theaterContainer) {
            queueRebuildTheaterGroups();
        }
    } else if (selected == actHideAuxiliaryFiles) {
        settings.setValue("audio_showcase/hide_active", !hideAuxiliary);
        updateHideSettings();
        refresh();
    } else if (selected == actHideExtensions) {
        promptHideExtensions();
    } else if (selected == actCasingClear || selected == actCasingBlack || selected == actCasingBlackPremium || selected == actCasingVinyl) {
        QString newCasing = "cd";
        if (selected == actCasingBlack) newCasing = "cd_black";
        else if (selected == actCasingBlackPremium) newCasing = "cd_black_premium";
        else if (selected == actCasingVinyl) newCasing = "vinyl";
        settings.setValue("music_showcase/casing_type", newCasing);
        if (m_proxyModel) {
            m_proxyModel->clearCasingCache();
        }
        refresh();
    } else if (selected == actConfigureFolderLayouts) {
        emit configureFolderLayoutsRequested();
    }
}

void FilePanel::showVideoShowcaseContextMenu(const QPoint& pos) {
    if (m_viewStack->currentWidget() == m_theaterListView) {
        QModelIndex index = m_theaterListView->indexAt(pos);
        if (index.isValid()) {
            if (!m_theaterListView->selectionModel()->isSelected(index)) {
                m_theaterListView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                m_theaterListView->setCurrentIndex(index);
            }
        }
    }

    QStringList curSelected = selectedPaths();
    QString selectedPath;
    bool isFolder = false;
    bool isFavorite = false;
    bool isTvShowFolder = false;
    bool isSeasonFolder = false;

    if (!curSelected.isEmpty()) {
        selectedPath = curSelected.first();
        QFileInfo info(selectedPath);
        isFolder = info.isDir();
        isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
        
        if (isFolder) {
            // Check if this folder contains subdirs starting with "Season" or "Series"
            QDir dir(selectedPath);
            QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& sub : subDirs) {
                QString name = sub.fileName().toLower();
                if (name.contains("season") || name.contains("series")) {
                    isTvShowFolder = true;
                    break;
                }
            }
            // If it doesn't contain season subfolders, but its own name contains "season" or "series"
            if (!isTvShowFolder) {
                QString name = info.fileName().toLower();
                if (name.contains("season") || name.contains("series")) {
                    isSeasonFolder = true;
                }
            }
        }
    }

    QMenu menu(this);
    QStyle* style = QApplication::style();

    QAction* actPlay = nullptr;
    QAction* actQueue = nullptr;
    QAction* actOpen = nullptr;
    QAction* actPlayQueue = nullptr;

    if (!selectedPath.isEmpty()) {
        if (isFolder) {
            if (isTvShowFolder) {
                actPlay = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Entire TV Show in Fullscreen");
                actQueue = menu.addAction("➕ Queue Entire TV Show to Playlist");
                actPlayQueue = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Playlist Queue in Fullscreen");
                actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "📁 Enter TV Show");
            } else if (isSeasonFolder) {
                actPlay = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Season in Fullscreen");
                actQueue = menu.addAction("➕ Queue Season to Playlist");
                actPlayQueue = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Playlist Queue in Fullscreen");
                actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "📁 Enter Season");
            } else {
                actPlay = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Folder in Fullscreen");
                actQueue = menu.addAction("➕ Queue Folder to Playlist");
                actPlayQueue = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Playlist Queue in Fullscreen");
                actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "📁 Enter Folder");
            }
        } else {
            actPlay = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play in Fullscreen");
            actQueue = menu.addAction("➕ Queue to Playlist");
            actPlayQueue = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "▶ Play Playlist Queue in Fullscreen");
            actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open");
        }
        menu.addSeparator();
    }

    QAction* actMediaInfoSheet = nullptr;
    QAction* actScrapeVideoMeta = nullptr;
    QAction* actApplyDvdOverlay = nullptr;
    QAction* actMarkWatched = nullptr;
    QAction* actMarkUnwatched = nullptr;

    if (!selectedPath.isEmpty()) {
        actMediaInfoSheet = menu.addAction("🎬 Media Info Sheet... (ℹ)");
        actScrapeVideoMeta = menu.addAction("🔍 Scrape Video Metadata...");
        if (!isFolder) {
            actApplyDvdOverlay = menu.addAction("💿 Apply DVD Case Overlay (Auto-Rename folder.jpg)");
        }
        
        QSettings settings("Amifiles", "Amifiles");
        bool isWatched = settings.value("watched/" + selectedPath, false).toBool();
        if (isWatched) {
            actMarkUnwatched = menu.addAction("Mark as Unwatched");
        } else {
            actMarkWatched = menu.addAction("Mark as Watched");
        }
        menu.addSeparator();
    }

    QAction* actFav = nullptr;
    if (!selectedPath.isEmpty()) {
        if (isFolder) {
            if (isFavorite) {
                actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove from Favorites");
            } else {
                actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add to Favorites");
            }
        } else {
            bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
            if (isCurrentFavorite) {
                actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove Current from Favorites");
            } else {
                actFav = menu.addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add Current to Favorites");
            }
        }
        menu.addSeparator();
    }

    QSettings settings("Amifiles", "Amifiles");
    bool zenActive = settings.value("preferences/zen_mode", false).toBool();
    bool builtinDoubleclick = settings.value("preferences/builtin_player_doubleclick", false).toBool();
    bool doubleclickQueue = settings.value("preferences/doubleclick_adds_to_queue", false).toBool();
    bool showInfoPanel = settings.value("video_showcase/show_info_panel", true).toBool();
    bool hideAuxiliary = settings.value("video_showcase/hide_active", true).toBool();

    QAction* actToggleInfoPanel = menu.addAction("Show Media Information Panel");
    actToggleInfoPanel->setCheckable(true);
    actToggleInfoPanel->setChecked(showInfoPanel);

    QAction* actToggleZen = menu.addAction("Clean Interface Mode (Zen Mode)");
    actToggleZen->setCheckable(true);
    actToggleZen->setChecked(zenActive);

    QAction* actToggleDoubleclick = menu.addAction("Double-click Plays Media in Built-in Fullscreen Player");
    actToggleDoubleclick->setCheckable(true);
    actToggleDoubleclick->setChecked(builtinDoubleclick);

    QAction* actToggleDoubleclickQueue = menu.addAction("Double-click Adds Media to Playlist Queue");
    actToggleDoubleclickQueue->setCheckable(true);
    actToggleDoubleclickQueue->setChecked(doubleclickQueue);

    QAction* actHideAuxiliaryFiles = menu.addAction("Hide Auxiliary / Artwork Files");
    actHideAuxiliaryFiles->setCheckable(true);
    actHideAuxiliaryFiles->setChecked(hideAuxiliary);

    QAction* actHideExtensions = menu.addAction("Hide File Extensions...");

    QAction* actConfigureFolderLayouts = menu.addAction("Configure Folder-Specific Layouts...");

    QAction* selected = menu.exec(QCursor::pos());
    if (!selected) return;

    if (selected == actOpen) {
        if (!selectedPath.isEmpty()) {
            onDoubleClickedPath(selectedPath);
        }
    } else if (selected == actPlay) {
        if (!selectedPath.isEmpty()) {
            QStringList playlistPaths;
            if (isFolder) {
                scanMediaFilesRecursively(selectedPath, playlistPaths, 2);
            } else {
                playlistPaths.append(selectedPath);
            }
            if (!playlistPaths.isEmpty()) {
                if (viewModeIndex() == 8 || viewModeIndex() == 9 || viewModeIndex() == 10) {
                    emit playMediaFullscreenRequested(playlistPaths);
                } else {
                    emit playMediaBuiltinRequested(playlistPaths);
                }
            }
        }
    } else if (selected == actQueue) {
        if (!selectedPath.isEmpty()) {
            QStringList playlistPaths;
            if (isFolder) {
                scanMediaFilesRecursively(selectedPath, playlistPaths, 2);
            } else {
                playlistPaths.append(selectedPath);
            }
            if (!playlistPaths.isEmpty()) {
                emit queueMediaBuiltinRequested(playlistPaths);
            }
        }
    } else if (selected == actPlayQueue) {
        emit playQueueFullscreenRequested();
    } else if (selected == actMediaInfoSheet) {
        showInfoSheet(selectedPath);
    } else if (selected == actScrapeVideoMeta) {
        VideoScraperDialog scraperDlg(curSelected, this);
        if (scraperDlg.exec() == QDialog::Accepted) {
            refresh();
            onSelectionChanged();
        }
    } else if (selected == actApplyDvdOverlay) {
        QFileInfo info(selectedPath);
        QString dirPath = info.absolutePath();
        QString baseName = info.completeBaseName();
        QStringList genericImages = { "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png" };
        QString foundGeneric;
        for (const QString& name : genericImages) {
            QFileInfo fi(QDir(dirPath).filePath(name));
            if (fi.exists()) {
                foundGeneric = fi.absoluteFilePath();
                break;
            }
        }
        if (!foundGeneric.isEmpty()) {
            QString ext = QFileInfo(foundGeneric).suffix();
            QString destPath = QDir(dirPath).filePath(baseName + "_cover." + ext);
            if (QFile::rename(foundGeneric, destPath)) {
                m_proxyModel->clearCasingCache();
                refresh();
                QMessageBox::information(this, "Overlay Applied", QString("Successfully renamed '%1' to '%2' to activate the DVD case overlay.").arg(QFileInfo(foundGeneric).fileName()).arg(QFileInfo(destPath).fileName()));
            } else {
                QMessageBox::warning(this, "Rename Failed", "Could not rename the generic artwork file.");
            }
        } else {
            QMessageBox::warning(this, "No Artwork Found", "No generic artwork (folder.jpg, poster.jpg, or cover.jpg) was found in this folder to rename.");
        }
    } else if (selected == actMarkWatched) {
        settings.setValue("watched/" + selectedPath, true);
        refresh();
    } else if (selected == actMarkUnwatched) {
        settings.setValue("watched/" + selectedPath, false);
        refresh();
    } else if (selected == actFav) {
        if (isFolder) {
            if (isFavorite) {
                FavoritesManager::instance().removeFavorite(selectedPath);
            } else {
                FavoritesManager::instance().addFavorite(selectedPath);
            }
        } else {
            bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
            if (isCurrentFavorite) {
                FavoritesManager::instance().removeFavorite(m_currentPath);
            } else {
                FavoritesManager::instance().addFavorite(m_currentPath);
            }
        }
    } else if (selected == actToggleInfoPanel) {
        settings.setValue("video_showcase/show_info_panel", !showInfoPanel);
        onSelectionChanged();
    } else if (selected == actToggleZen) {
        emit zenModeToggled(!zenActive);
    } else if (selected == actToggleDoubleclick) {
        settings.setValue("preferences/builtin_player_doubleclick", !builtinDoubleclick);
        emit mediaPlaybackSettingsChanged();
    } else if (selected == actToggleDoubleclickQueue) {
        settings.setValue("preferences/doubleclick_adds_to_queue", !doubleclickQueue);
        emit mediaPlaybackSettingsChanged();
    } else if (selected == actHideAuxiliaryFiles) {
        settings.setValue("video_showcase/hide_active", !hideAuxiliary);
        updateHideSettings();
        refresh();
    } else if (selected == actHideExtensions) {
        promptHideExtensions();
    } else if (selected == actConfigureFolderLayouts) {
        emit configureFolderLayoutsRequested();
    }
}

void FilePanel::showInfoSheet(const QString& path) {
    if (path.isEmpty()) return;
    ShowcaseInfoDialog infoDlg(path, this);
    connect(&infoDlg, &ShowcaseInfoDialog::playRequested, this, [this](const QString& p) {
        if (viewModeIndex() == 8 || viewModeIndex() == 9 || viewModeIndex() == 10) {
            emit playMediaFullscreenRequested({p});
        } else {
            emit playMediaBuiltinRequested({p});
        }
    });
    connect(&infoDlg, &ShowcaseInfoDialog::openFolderRequested, this, [this](const QString& p) {
        navigateTo(p, true);
    });
    connect(&infoDlg, &ShowcaseInfoDialog::watchStatusChanged, this, [this](const QString& p, bool) {
        notifyPathDataChanged(p);
    });
    infoDlg.exec();
}

void FilePanel::toggleWatchStatus(const QString& path) {
    if (path.isEmpty()) return;
    QSettings settings("Amifiles", "Amifiles");
    bool isWatched = settings.value("watched/" + path, false).toBool();
    settings.setValue("watched/" + path, !isWatched);
    notifyPathDataChanged(path);
}

void FilePanel::scrapeVideoMetadata() {
    QStringList curSelected = selectedPaths();
    if (curSelected.isEmpty()) return;
    VideoScraperDialog dlg(curSelected, this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
        onSelectionChanged();
    }
}

void FilePanel::editAudioTags(bool autoFetch) {
    QStringList curSelected = selectedPaths();
    if (curSelected.isEmpty()) return;
    TagEditorDialog dlg(curSelected, this, autoFetch);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
    }
}

void FilePanel::applyDvdCasing() {
    QStringList curSelected = selectedPaths();
    QString selectedPath = curSelected.isEmpty() ? "" : curSelected.first();
    if (selectedPath.isEmpty()) return;
    QFileInfo info(selectedPath);
    if (!info.isDir()) {
        QString dirPath = info.absolutePath();
        QString baseName = info.completeBaseName();
        QStringList genericImages = { 
            baseName + "_cover.jpg", baseName + "_cover.jpeg", baseName + "_cover.png",
            baseName + "_bluray_cover.jpg", baseName + "_bluray_cover.jpeg", baseName + "_bluray_cover.png",
            "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png" 
        };
        QString foundGeneric;
        for (const QString& name : genericImages) {
            QFileInfo fi(QDir(dirPath).filePath(name));
            if (fi.exists()) {
                foundGeneric = fi.absoluteFilePath();
                break;
            }
        }
        if (!foundGeneric.isEmpty()) {
            QString ext = QFileInfo(foundGeneric).suffix();
            QString destPath = QDir(dirPath).filePath(baseName + "_dvd_cover." + ext);
            if (foundGeneric != destPath) {
                QFile::rename(foundGeneric, destPath);
            }
            if (m_proxyModel) m_proxyModel->clearCasingCache();
            refresh();
        }
    } else {
        QString dirPath = selectedPath;
        QStringList genericImages = { "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png", "bluray_cover.jpg", "bluray_cover.jpeg", "bluray_cover.png" };
        QString foundGeneric;
        for (const QString& name : genericImages) {
            QFileInfo fi(QDir(dirPath).filePath(name));
            if (fi.exists()) {
                foundGeneric = fi.absoluteFilePath();
                break;
            }
        }
        if (!foundGeneric.isEmpty()) {
            QString ext = QFileInfo(foundGeneric).suffix();
            QString destPath = QDir(dirPath).filePath("dvd_cover." + ext);
            if (foundGeneric != destPath) {
                QFile::rename(foundGeneric, destPath);
            }
            if (m_proxyModel) m_proxyModel->clearCasingCache();
            refresh();
        }
    }
}

void FilePanel::applyBluRayCasing() {
    QStringList curSelected = selectedPaths();
    QString selectedPath = curSelected.isEmpty() ? "" : curSelected.first();
    if (selectedPath.isEmpty()) return;
    QFileInfo info(selectedPath);
    if (!info.isDir()) {
        QString dirPath = info.absolutePath();
        QString baseName = info.completeBaseName();
        QStringList genericImages = { 
            baseName + "_cover.jpg", baseName + "_cover.jpeg", baseName + "_cover.png",
            baseName + "_dvd_cover.jpg", baseName + "_dvd_cover.jpeg", baseName + "_dvd_cover.png",
            "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png" 
        };
        QString foundGeneric;
        for (const QString& name : genericImages) {
            QFileInfo fi(QDir(dirPath).filePath(name));
            if (fi.exists()) {
                foundGeneric = fi.absoluteFilePath();
                break;
            }
        }
        if (!foundGeneric.isEmpty()) {
            QString ext = QFileInfo(foundGeneric).suffix();
            QString destPath = QDir(dirPath).filePath(baseName + "_bluray_cover." + ext);
            if (foundGeneric != destPath) {
                QFile::rename(foundGeneric, destPath);
            }
            if (m_proxyModel) m_proxyModel->clearCasingCache();
            refresh();
        }
    } else {
        QString dirPath = selectedPath;
        QStringList genericImages = { "folder.jpg", "folder.jpeg", "folder.png", "poster.jpg", "poster.jpeg", "poster.png", "cover.jpg", "cover.jpeg", "cover.png", "dvd_cover.jpg", "dvd_cover.jpeg", "dvd_cover.png" };
        QString foundGeneric;
        for (const QString& name : genericImages) {
            QFileInfo fi(QDir(dirPath).filePath(name));
            if (fi.exists()) {
                foundGeneric = fi.absoluteFilePath();
                break;
            }
        }
        if (!foundGeneric.isEmpty()) {
            QString ext = QFileInfo(foundGeneric).suffix();
            QString destPath = QDir(dirPath).filePath("bluray_cover." + ext);
            if (foundGeneric != destPath) {
                QFile::rename(foundGeneric, destPath);
            }
            if (m_proxyModel) m_proxyModel->clearCasingCache();
            refresh();
        }
    }
}

void FilePanel::playCollection() {
    QStringList curSelected = selectedPaths();
    QString selectedPath = curSelected.isEmpty() ? m_currentPath : curSelected.first();
    if (selectedPath.isEmpty()) return;
    QFileInfo info(selectedPath);
    QStringList playlistPaths;
    int filter = 0;
    int vm = viewModeIndex();
    if (vm == 6 || vm == 10) filter = 1;
    else if (vm == 7 || vm == 8 || vm == 9) filter = 2;
    
    if (info.isDir()) {
        scanMediaFilesRecursively(selectedPath, playlistPaths, filter);
    } else {
        playlistPaths.append(selectedPath);
    }
    
    if (!playlistPaths.isEmpty()) {
        if (vm == 8 || vm == 9 || vm == 10) {
            emit playMediaFullscreenRequested(playlistPaths);
        } else {
            emit playMediaBuiltinRequested(playlistPaths);
        }
    }
}

void FilePanel::createArchive(bool secure) {
    QStringList curSelected = selectedPaths();
    if (curSelected.isEmpty()) return;
    ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeCreate, curSelected, m_currentPath, secure, this);
    connect(dlg, &QDialog::accepted, this, &FilePanel::refresh);
    dlg->show();
}

void FilePanel::extractArchive() {
    QStringList curSelected = selectedPaths();
    if (curSelected.isEmpty()) return;
    ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeExtract, curSelected.first(), m_currentPath, this);
    connect(dlg, &QDialog::accepted, this, &FilePanel::refresh);
    dlg->show();
}


void FilePanel::setCategoryButtonsVisible(bool visible) {
    m_categoryButtonsVisible = visible;
    if (!m_flatViewEnabled && m_categoryWidget) {
        m_categoryWidget->setVisible(visible);
    }
}

void FilePanel::promptHideExtensions() {
    int viewMode = viewModeIndex();
    QSettings settings("Amifiles", "Amifiles");
    QString settingsKey;
    if (viewMode == 6) settingsKey = "audio_showcase/hidden_extensions";
    else if (viewMode == 7) settingsKey = "video_showcase/hidden_extensions";
    else if (viewMode == 8) settingsKey = "movie_showcase/hidden_extensions";
    else if (viewMode == 9) settingsKey = "tv_showcase/hidden_extensions";
    else if (viewMode == 10) settingsKey = "music_showcase/hidden_extensions";
    else settingsKey = "theater/hidden_extensions";

    QString currentExts = settings.value(settingsKey, "").toString();

    QDialog dlg(this);
    dlg.setWindowTitle("Hide File Extensions");
    dlg.setMinimumWidth(360);
    dlg.setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; font-size: 13px; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 6px; padding: 6px; selection-background-color: #89b4fa; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; color: #89b4fa; }"
    );

    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel* infoLabel = new QLabel("Enter file extensions to hide in this view\n(separated by commas, e.g. zip, srt, txt):", &dlg);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    QLineEdit* edit = new QLineEdit(&dlg);
    edit->setText(currentExts);
    edit->setPlaceholderText("zip, srt, txt");
    layout->addWidget(edit);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    QPushButton* btnCancel = new QPushButton("Cancel", &dlg);
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    QPushButton* btnSave = new QPushButton("Save", &dlg);
    btnSave->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; border: none; } QPushButton:hover { background-color: #b4befe; }");
    connect(btnSave, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(btnSave);

    layout->addLayout(btnLayout);

    if (dlg.exec() == QDialog::Accepted) {
        QString text = edit->text().trimmed();
        settings.setValue(settingsKey, text);
        updateHideSettings();
        refresh();
    }
}

void FilePanel::updateHideSettings() {
    if (!m_proxyModel) return;

    QSettings settings("Amifiles", "Amifiles");
    int vMode = viewModeIndex();
    
    bool hideActive = false;
    QStringList hiddenExts;
    QString patternsStr;
    QString defaultHide = "folder.jpg, folder.jpeg, folder.png, cover.jpg, cover.jpeg, cover.png, fanart.jpg, fanart.jpeg, fanart.png, backdrop.jpg, backdrop.jpeg, backdrop.png, poster.jpg, poster.jpeg, poster.png, *.nfo, *.xml, *.txt, *.srt, *.sub, *.vtt, *.ini, *.db";

    if (vMode == 6) { // Audio Showcase
        hideActive = settings.value("audio_showcase/hide_active", true).toBool();
        QString extsStr = settings.value("audio_showcase/hidden_extensions", "").toString();
        hiddenExts = extsStr.split(',', Qt::SkipEmptyParts);
        patternsStr = settings.value("audio_showcase/hide_patterns", defaultHide).toString();
    } else if (vMode == 7) { // Video Showcase
        hideActive = settings.value("video_showcase/hide_active", true).toBool();
        QString extsStr = settings.value("video_showcase/hidden_extensions", "").toString();
        hiddenExts = extsStr.split(',', Qt::SkipEmptyParts);
        patternsStr = settings.value("video_showcase/hide_patterns", defaultHide).toString();
    } else if (vMode == 8) { // Movie Showcase (v2)
        hideActive = settings.value("movie_showcase/hide_active", true).toBool();
        QString extsStr = settings.value("movie_showcase/hidden_extensions", "").toString();
        hiddenExts = extsStr.split(',', Qt::SkipEmptyParts);
        patternsStr = settings.value("movie_showcase/hide_patterns", defaultHide).toString();
    } else if (vMode == 9) { // TV Show Showcase (v2)
        hideActive = settings.value("tv_showcase/hide_active", true).toBool();
        QString extsStr = settings.value("tv_showcase/hidden_extensions", "").toString();
        hiddenExts = extsStr.split(',', Qt::SkipEmptyParts);
        patternsStr = settings.value("tv_showcase/hide_patterns", defaultHide).toString();
    } else if (vMode == 10) { // Music Showcase (v2)
        hideActive = settings.value("music_showcase/hide_active", true).toBool();
        QString extsStr = settings.value("music_showcase/hidden_extensions", "").toString();
        hiddenExts = extsStr.split(',', Qt::SkipEmptyParts);
        patternsStr = settings.value("music_showcase/hide_patterns", defaultHide).toString();
    } else {
        hideActive = false; 
    }

    m_proxyModel->setHideAuxiliaryFilesActive(hideActive);
    if (m_flatProxyModel) m_flatProxyModel->setHideAuxiliaryFilesActive(hideActive);
    
    QStringList patternsList = patternsStr.split(',', Qt::SkipEmptyParts);
    for (auto& p : patternsList) p = p.trimmed();
    m_proxyModel->setHidePatterns(patternsList);
    if (m_flatProxyModel) m_flatProxyModel->setHidePatterns(patternsList);
    
    for (auto& ext : hiddenExts) {
        ext = ext.trimmed().toLower();
        if (ext.startsWith("*.")) {
            ext = ext.mid(2);
        } else if (ext.startsWith('*')) {
            ext = ext.mid(1);
        } else if (ext.startsWith('.')) {
            ext = ext.mid(1);
        }
    }
    m_proxyModel->setHiddenExtensions(hiddenExts);
    if (m_flatProxyModel) m_flatProxyModel->setHiddenExtensions(hiddenExts);
}

void FilePanel::updateFileSystemFilters() {
    if (!m_fileModel) return;

    QSettings settings("Amifiles", "Amifiles");
    bool showHidden = settings.value("preferences/show_hidden_files", false).toBool();

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::Drives | QDir::NoDotAndDotDot;
    if (showHidden) {
        filters |= QDir::Hidden | QDir::System;
    }

    m_fileModel->setFilter(filters);
}

void FilePanel::setFilterTextBarVisible(bool visible) {
    m_filterTextBarVisible = visible;
    if (m_filterTextWidget) {
        m_filterTextWidget->setVisible(visible);
    }
}

bool FilePanel::isCategoryButtonsVisible() const {
    return m_categoryButtonsVisible;
}

bool FilePanel::isFilterTextBarVisible() const {
    return m_filterTextBarVisible;
}

QString FilePanel::filterText() const {
    return m_filterEdit ? m_filterEdit->text() : "";
}

void FilePanel::syncFilterText(const QString& text) {
    if (m_filterEdit) {
        m_filterEdit->blockSignals(true);
        m_filterEdit->setText(text);
        m_filterEdit->blockSignals(false);
    }
}

void FilePanel::syncFilterType(FileFilterProxyModel::FilterType type) {
    QSet<FileFilterProxyModel::FilterType> types = { type };
    syncFilterTypes(types);
}

void FilePanel::syncFilterTypes(const QSet<FileFilterProxyModel::FilterType>& types) {
    if (m_btnFilterAll && m_btnFilterAudio && m_btnFilterVideos && m_btnFilterPictures && m_btnFilterDocs && m_btnFilterArchive && m_btnFilterThreeD && m_btnFilterFiles && m_btnFilterFolders) {
        m_btnFilterAll->blockSignals(true);
        m_btnFilterAudio->blockSignals(true);
        m_btnFilterVideos->blockSignals(true);
        m_btnFilterPictures->blockSignals(true);
        m_btnFilterDocs->blockSignals(true);
        m_btnFilterArchive->blockSignals(true);
        m_btnFilterThreeD->blockSignals(true);
        m_btnFilterFiles->blockSignals(true);
        m_btnFilterFolders->blockSignals(true);

        m_btnFilterAll->setChecked(types.contains(FileFilterProxyModel::FilterAll));
        m_btnFilterAudio->setChecked(types.contains(FileFilterProxyModel::FilterAudio));
        m_btnFilterVideos->setChecked(types.contains(FileFilterProxyModel::FilterVideos));
        m_btnFilterPictures->setChecked(types.contains(FileFilterProxyModel::FilterPictures));
        m_btnFilterDocs->setChecked(types.contains(FileFilterProxyModel::FilterDocs));
        m_btnFilterArchive->setChecked(types.contains(FileFilterProxyModel::FilterArchive));
        m_btnFilterThreeD->setChecked(types.contains(FileFilterProxyModel::FilterThreeD));
        m_btnFilterFiles->setChecked(types.contains(FileFilterProxyModel::FilterFiles));
        m_btnFilterFolders->setChecked(types.contains(FileFilterProxyModel::FilterFolders));

        m_btnFilterAll->blockSignals(false);
        m_btnFilterAudio->blockSignals(false);
        m_btnFilterVideos->blockSignals(false);
        m_btnFilterPictures->blockSignals(false);
        m_btnFilterDocs->blockSignals(false);
        m_btnFilterArchive->blockSignals(false);
        m_btnFilterThreeD->blockSignals(false);
        m_btnFilterFiles->blockSignals(false);
        m_btnFilterFolders->blockSignals(false);
    }
}

void FilePanel::setFlatViewEnabled(bool enabled) {
    if (m_flatViewEnabled == enabled) return;
    m_flatViewEnabled = enabled;

    if (m_btnFlatView) {
        QStyle* style = QApplication::style();
        if (enabled) {
            m_btnFlatView->setIcon(style->standardIcon(QStyle::SP_FileDialogListView));
            m_btnFlatView->setToolTip("Disable Flat View (Return to folder tree view)");
        } else {
            m_btnFlatView->setIcon(style->standardIcon(QStyle::SP_DirIcon));
            m_btnFlatView->setToolTip("Enable Flat View (Recurse all subfolders)");
        }
    }

    if (m_btnFlatView && m_btnFlatView->isChecked() != enabled) {
        m_btnFlatView->blockSignals(true);
        m_btnFlatView->setChecked(enabled);
        m_btnFlatView->blockSignals(false);
    }

    if (enabled) {
        updateActiveViewModel();
        if (m_flatProxyModel) {
            m_flatProxyModel->setFilterTypes(m_proxyModel->filterTypes());
            m_flatProxyModel->setFilterText(m_proxyModel->filterText());
        }
        m_flatModel->setRootPath(m_currentPath);
        if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);

        connect(m_flatModel, &FlatFileSystemModel::scanStarted, this, [this]() {
            m_statusLabel->setText("Scanning directory recursively...");
        });
        connect(m_flatModel, &FlatFileSystemModel::scanFinished, this, &FilePanel::updateStatusText);
    } else {
        updateActiveViewModel();
        if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);

        QModelIndex srcIndex = m_fileModel->index(m_currentPath);
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            m_groupProxy->rebuildGroups();
            m_treeView->setRootIndex(QModelIndex());
            m_listView->setRootIndex(QModelIndex());
            m_theaterListView->setRootIndex(QModelIndex());
            if (m_coverFlowView) m_coverFlowView->setRootIndex(QModelIndex());
        } else {
            m_treeView->setRootIndex(proxyIndex);
            m_listView->setRootIndex(proxyIndex);
            m_theaterListView->setRootIndex(proxyIndex);
            if (m_coverFlowView) m_coverFlowView->setRootIndex(proxyIndex);
        }
    }

    // Enable interactive resizing for main columns
    for (int i = 0; i < 4; ++i) {
        m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
    }
    if (enabled) {
        m_treeView->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    }

    updateStatusText();
}

void FilePanel::onFavoriteButtonContextMenu(const QPoint& pos) {
    QMenu menu(this);
    QStyle* style = QApplication::style();
    QStringList favs = FavoritesManager::instance().getFavorites();

    QMenu* menuRemove = menu.addMenu(style->standardIcon(QStyle::SP_TrashIcon), "Remove from Favorites...");
    if (favs.isEmpty()) {
        QAction* actNone = menuRemove->addAction("(No Favorites Configured)");
        actNone->setEnabled(false);
    } else {
        for (const QString& path : favs) {
            QAction* actRemove = menuRemove->addAction(QDir::toNativeSeparators(path));
            connect(actRemove, &QAction::triggered, this, [path]() {
                FavoritesManager::instance().removeFavorite(path);
            });
        }
    }

    menu.exec(m_btnFavorite->mapToGlobal(pos));
}

void FilePanel::onToggleViewMode() {
    int current = m_comboViewMode->currentIndex();
    int next = (current + 1) % 6;
    m_comboViewMode->setCurrentIndex(next);
}

void FilePanel::zoomIn() {
    if (m_zoomLevel < 6) {
        m_zoomSlider->setValue(m_zoomLevel + 1);
    }
}

void FilePanel::zoomOut() {
    if (m_zoomLevel > 0) {
        m_zoomSlider->setValue(m_zoomLevel - 1);
    }
}

void FilePanel::onZoomChanged(int value) {
    if (m_zoomLevel == value) return;
    m_zoomLevel = value;
    
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("file_panel/zoom_level", value);

    int sizes[] = { 16, 24, 32, 48, 64, 96, 128 };
    int size = sizes[qBound(0, value, 6)];
    
    m_treeView->setIconSize(QSize(size, size));
    m_listView->setIconSize(QSize(size, size));
    if (m_proxyModel) {
        m_proxyModel->setZoomIconSize(size);
    }
    
    updateStyles();

    if (m_listView->viewMode() == QListView::IconMode) {
        m_listView->setGridSize(QSize(size + 60, size + 40));
    }

    updateTheaterGridSize();

    emit zoomChanged(value);
}

void FilePanel::onGroupingChanged(int index) {
    if (index == 0) {
        m_groupProxy->setGrouping(false, "");
        updateActiveViewModel();
        navigateTo(m_currentPath, false);
        return;
    }

    QString groupType;
    QString customKey;

    switch (index) {
        case 1: groupType = "Artist"; break;
        case 2: groupType = "Album"; break;
        case 3: groupType = "Genre"; break;
        case 4: groupType = "Type"; break;
        case 5: groupType = "Rating"; break;
        case 6: groupType = "Year"; break;
        case 7: groupType = "Decade"; break;
        case 8: {
            bool ok = false;
            QString text = QInputDialog::getText(this, "Group by Custom Text",
                                                 "Enter custom metadata/annotation attribute key:",
                                                 QLineEdit::Normal, "", &ok);
            if (ok && !text.trimmed().isEmpty()) {
                groupType = "CustomText";
                customKey = text.trimmed();
            } else {
                m_comboGrouping->blockSignals(true);
                m_comboGrouping->setCurrentIndex(0);
                m_comboGrouping->blockSignals(false);
                return;
            }
            break;
        }
        default: return;
    }

    m_groupProxy->setGrouping(true, groupType, customKey);
    updateActiveViewModel();
    m_treeView->expandAll();
    navigateTo(m_currentPath, false);
}

void FilePanel::syncZoom(int value) {
    if (m_zoomLevel == value) return;
    m_zoomLevel = value;

    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(value);
    m_zoomSlider->blockSignals(false);
    
    int sizes[] = { 16, 24, 32, 48, 64, 96, 128 };
    int size = sizes[qBound(0, value, 6)];
    
    m_treeView->setIconSize(QSize(size, size));
    m_listView->setIconSize(QSize(size, size));
    if (m_proxyModel) {
        m_proxyModel->setZoomIconSize(size);
    }
    
    updateStyles();

    if (m_listView->viewMode() == QListView::IconMode) {
        m_listView->setGridSize(QSize(size + 60, size + 40));
    }
    updateTheaterGridSize();
}

void FilePanel::updateTheaterGridSize() {
    int index = viewModeIndex();
    if (index < 8 || index > 10) return; // 8: Movies, 9: TV, 10: Music

    int sizes[] = { 16, 24, 32, 48, 64, 96, 128 };
    int size = sizes[qBound(0, m_zoomLevel, 6)];
    double factor = (double)size / 32.0;

    int gw, gh;
    if (index == 10) {
        QSettings settings("Amifiles", "Amifiles");
        QString casingType = settings.value("music_showcase/casing_type", "cd").toString();
        if (casingType == "vinyl") {
            gw = qRound(190.0 * factor);
            gh = qRound(160.0 * factor) + 24;
        } else {
            gw = qRound(160.0 * factor);
            gh = gw + 24;
        }
    } else {
        // 2:3 widescreen posters for Movie/TV
        gw = qRound(155.0 * factor);
        gh = qRound(245.0 * factor);
    }

    if (m_theaterListView) {
        m_theaterListView->setGridSize(QSize(gw, gh));
    }

    for (QListView* grid : m_theaterGrids) {
        if (grid) {
            grid->setGridSize(QSize(gw, gh));
            int childCount = grid->model() ? grid->model()->rowCount(grid->rootIndex()) : 0;
            int cols = qMax(1, (m_theaterScrollWidget ? m_theaterScrollWidget->width() : 800) / gw);
            int rows = (childCount + cols - 1) / cols;
            grid->setFixedHeight(rows * gh + 10);
        }
    }
}

void FilePanel::updateStyles() {
    QSettings settings("Amifiles", "Amifiles");
    QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();
    int baseFontSize = settings.value("theme/font_size", 13).toInt();

    // Scale font size based on zoom level (m_zoomLevel is 0 to 6, default zoom level is 2)
    int fontSize = baseFontSize + (m_zoomLevel - 2) * 2;
    if (fontSize < 8) fontSize = 8;

    QString bgStyle;
    QString stackBgStyle;
    bool hasBgImage = (!m_customBgImage.isEmpty() && QFile::exists(m_customBgImage));

    if (preset == "System Theme") {
        if (hasBgImage) {
            stackBgStyle = QString("border-image: url(\"%1\") 0 0 0 0 stretch stretch;").arg(m_customBgImage);
            int dimAlpha = static_cast<int>((1.0 - m_customBgOpacity) * 255.0);
            bgStyle = QString("background-color: rgba(255, 255, 255, %1);").arg(dimAlpha);
        } else {
            stackBgStyle = m_customBgColor.isEmpty() ? "" : QString("background-color: %1;").arg(m_customBgColor);
            bgStyle = m_customBgColor.isEmpty() ? "" : QString("background-color: %1;").arg(m_customBgColor);
        }

        m_viewStack->setStyleSheet(stackBgStyle.isEmpty() ? "" : QString("QStackedWidget { %1 }").arg(stackBgStyle));
        m_treeView->setStyleSheet(QString("QTreeView { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_listView->setStyleSheet(QString("QListView { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_theaterListView->setStyleSheet(QString("QListView { border: none; %1 font-size: %2px; }"
                                                 "QListView::item { border: none; }"
                                                 "QListView::item:hover { background: transparent; }"
                                                 "QListView::item:selected { background: transparent; }").arg(bgStyle).arg(fontSize));
        m_millerView->setStyleSheet(QString("MillerColumnsView { border: none; %1 }").arg(bgStyle));
        m_timelineView->setStyleSheet(QString("QTreeWidget { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_filmstripView->setStyleSheet(QString("FilmstripView { border: none; %1 }").arg(bgStyle));
        if (m_searchResultsView) {
            m_searchResultsView->setStyleSheet(QString("QListView { %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        }
    } else {
        Theme::ThemeColors colors = Theme::getThemeColors();
        QString bg = m_customBgColor.isEmpty() ? colors.bg : m_customBgColor;
        QString border = colors.border;
        QString text = colors.text;
        QString accent = colors.accent;
        
        int modeIndex = viewModeIndex();
        bool isFullscreenShowcase = (modeIndex >= 8 && modeIndex <= 10);
        QString borderColor = m_isActive ? accent : border;
        QString borderStyle = isFullscreenShowcase ? "border: none;" : QString("border: 2px solid %1; border-radius: 4px;").arg(borderColor);

        if (hasBgImage) {
            stackBgStyle = QString("border-image: url(\"%1\") 0 0 0 0 stretch stretch;").arg(m_customBgImage);
            int dimAlpha = static_cast<int>((1.0 - m_customBgOpacity) * 255.0);
            QColor c(bg);
            bgStyle = QString("background-color: rgba(%1, %2, %3, %4);").arg(c.red()).arg(c.green()).arg(c.blue()).arg(dimAlpha);
        } else {
            stackBgStyle = QString("background-color: %1;").arg(bg);
            bgStyle = QString("background-color: %1;").arg(bg);
        }

        m_viewStack->setStyleSheet(QString("QStackedWidget { %1 %2 }").arg(borderStyle).arg(stackBgStyle));

        m_treeView->setStyleSheet(QString("QTreeView { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_listView->setStyleSheet(QString("QListView { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_theaterListView->setStyleSheet(QString("QListView { border: none; %1 color: %2; font-size: %3px; }"
                                                 "QListView::item { border: none; }"
                                                 "QListView::item:hover { background: transparent; }"
                                                 "QListView::item:selected { background: transparent; }").arg(bgStyle).arg(text).arg(fontSize));
        m_millerView->setStyleSheet(QString("MillerColumnsView { border: none; %1 }").arg(bgStyle));
        m_timelineView->setStyleSheet(QString("QTreeWidget { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_filmstripView->setStyleSheet(QString("FilmstripView { border: none; %1 }").arg(bgStyle));

        if (m_searchResultsView) {
            m_searchResultsView->setStyleSheet(QString("QListView { border: 2px solid %1; %2 color: %3; font-size: %4px; }").arg(borderColor).arg(bgStyle).arg(text).arg(fontSize));
        }
    }
}

void FilePanel::onHeaderContextMenu(const QPoint& pos) {
    QMenu menu(this);
    QHeaderView* header = m_treeView->header();
    
    QStringList builtInNames = {"Name", "Size", "Type", "Date Modified"};
    for (int i = 0; i < 4; ++i) {
        QAction* act = menu.addAction(builtInNames[i]);
        act->setCheckable(true);
        act->setChecked(!header->isSectionHidden(i));
        connect(act, &QAction::toggled, this, [header, i](bool checked) {
            header->setSectionHidden(i, !checked);
            QSettings settings("Amifiles", "Amifiles");
            settings.setValue(QString("columns/hidden_%1").arg(i), !checked);
        });
    }

    menu.addSeparator();

    QList<CustomColumn> activeCustom = m_fileModel->activeColumns();
    for (int i = 0; i < activeCustom.size(); ++i) {
        int colIdx = i + 4;
        QAction* act = menu.addAction(activeCustom[i].name);
        act->setCheckable(true);
        act->setChecked(!header->isSectionHidden(colIdx));
        connect(act, &QAction::toggled, this, [header, colIdx](bool checked) {
            header->setSectionHidden(colIdx, !checked);
            QSettings settings("Amifiles", "Amifiles");
            settings.setValue(QString("columns/hidden_%1").arg(colIdx), !checked);
        });
    }

    menu.addSeparator();

    int logicalIndex = header->logicalIndexAt(pos);
    QAction* actAutoSizeThis = nullptr;
    if (logicalIndex >= 0) {
        QString colName = header->model()->headerData(logicalIndex, Qt::Horizontal).toString();
        actAutoSizeThis = menu.addAction(QString("Auto-Size '%1'").arg(colName));
    }
    QAction* actAutoSizeAll = menu.addAction("Auto-Size All Columns");

    menu.addSeparator();
    QAction* actCustomize = menu.addAction("Customize Columns...");

    QAction* selected = menu.exec(m_treeView->header()->mapToGlobal(pos));
    if (selected && selected == actAutoSizeThis && logicalIndex >= 0) {
        m_treeView->resizeColumnToContents(logicalIndex);
        saveColumnWidth(logicalIndex, m_treeView->columnWidth(logicalIndex));
    } else if (selected && selected == actAutoSizeAll) {
        autoSizeAllColumns();
    } else if (selected && selected == actCustomize) {
        ColumnsCustomizerDialog dlg(activeCustom, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_fileModel->setActiveColumns(dlg.getSelectedColumns());
            for (int i = 0; i < m_fileModel->columnCount(); ++i) {
                header->setSectionHidden(i, false);
            }
        }
    }
}

void FilePanel::setSearchQuery(const QString& query) {
    if (m_globalSearchEdit) {
        if (!m_isSearchModeActive) {
            onToggleSearchFilterMode();
        }
        m_globalSearchEdit->setText(query);
    }
}

QString FilePanel::searchQuery() const {
    return m_globalSearchEdit ? m_globalSearchEdit->text() : "";
}

void FilePanel::onGlobalSearchChanged(const QString& text) {
    m_searchDebounceTimer->stop();
    if (text.isEmpty()) {
        if (m_searchWorker) {
            m_searchWorker->cancel();
        }
        m_searchResultModel->setStringList(QStringList());
        m_searchResultsView->setVisible(false);
        m_viewStack->setVisible(true);
        updateStatusText();
    } else {
        m_viewStack->setVisible(false);
        m_searchResultsView->setVisible(true);
        m_searchDebounceTimer->start();
        m_statusLabel->setText("Typing...");
    }
}

void FilePanel::startSearch() {
    QString text = m_globalSearchEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    if (m_searchWorker) {
        m_searchWorker->cancel();
    }

    m_searchResultModel->setStringList(QStringList());
    m_statusLabel->setText("Searching...");

    emit sigStartSearch(text, m_currentPath);
}

void FilePanel::onSearchResultsReady(const QStringList& results) {
    m_bufferedSearchResults.append(results);
    if (!m_searchUpdateTimer->isActive()) {
        m_searchUpdateTimer->start();
    }
}

void FilePanel::onSearchFinished() {
    m_searchUpdateTimer->stop();
    onSearchUpdateTimeout(); // Flush final remaining results
    
    int count = m_searchResultModel->stringList().size();
    m_statusLabel->setText(QString("Search finished. Found %1 items").arg(count));
}

void FilePanel::onSearchUpdateTimeout() {
    if (m_bufferedSearchResults.isEmpty()) return;

    QStringList currentList = m_searchResultModel->stringList();
    currentList.append(m_bufferedSearchResults);
    m_searchResultModel->setStringList(currentList);
    m_bufferedSearchResults.clear();

    m_statusLabel->setText(QString("Found %1 items").arg(currentList.size()));
}

void FilePanel::updateDrawerVisibility() {
    if (!m_theaterSideContainer || !m_btnToggleSidePane) return;
    int index = viewModeIndex();
    bool isShowcase = (index >= 7 && index <= 10);
    bool hasTracks = (m_trackListWidget && m_trackListWidget->count() > 0);
    bool shouldShow = isShowcase && m_btnToggleSidePane->isVisible() && m_btnToggleSidePane->isChecked() && hasTracks;
    m_theaterSideContainer->setVisible(shouldShow);
}

void FilePanel::onToggleSidePane() {
    if (!m_blockCollapseTimerStop) {
        if (m_playlistCollapseTimer) {
            m_playlistCollapseTimer->stop();
        }
    }
    updateDrawerVisibility();
}

void FilePanel::onToggleSearchFilterMode() {
    m_isSearchModeActive = !m_isSearchModeActive;
    
    if (m_isSearchModeActive) {
        m_btnToggleSearchMode->setIcon(createFilterIcon(QColor("#a6e3a1"))); // active green filter icon
        m_btnToggleSearchMode->setToolTip("Switch to Filter Mode");
        
        m_filterEdit->setVisible(false);
        m_globalSearchEdit->setVisible(true);
        m_globalSearchEdit->setFocus();
        
        if (!m_globalSearchEdit->text().isEmpty()) {
            startSearch();
        }
    } else {
        m_btnToggleSearchMode->setIcon(createSearchIcon(QColor("#cdd6f4"))); // default search icon
        m_btnToggleSearchMode->setToolTip("Switch to Search Mode");
        
        m_filterEdit->setVisible(true);
        m_globalSearchEdit->setVisible(false);
        m_filterEdit->setFocus();
        
        m_searchResultsView->setVisible(false);
        m_viewStack->setVisible(true);
        
        onFilterChanged(m_filterEdit->text());
    }
}

void FilePanel::onSearchContextMenu(const QPoint& pos) {
    QModelIndexList selectedIndexes = m_searchResultsView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    QStringList selectedFiles;
    for (const QModelIndex& idx : selectedIndexes) {
        QString f = idx.data().toString();
        if (QFile::exists(f)) {
            selectedFiles.append(f);
        }
    }
    if (selectedFiles.isEmpty()) return;

    QString firstFile = selectedFiles.first();
    QFileInfo info(firstFile);

    QMenu menu(this);
    QStyle* style = QApplication::style();

    QAction* actOpen = nullptr;
    QAction* actOpenFolder = nullptr;
    QAction* actOpenFolderSibling = nullptr;
    QAction* actCopyPath = nullptr;
    QAction* actCopy = nullptr;
    QAction* actCut = nullptr;
    QAction* actCopyToSibling = nullptr;
    QAction* actMoveToSibling = nullptr;
    QAction* actDelete = nullptr;
    QAction* actRename = nullptr;
    QAction* actEditTags = nullptr;
    QAction* actScrapeVideo = nullptr;
    QAction* actCalculateChecksum = nullptr;
    QAction* actSecureShred = nullptr;
    QAction* actProp = nullptr;

    if (selectedFiles.size() == 1) {
        actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open File");
        actOpenFolder = menu.addAction(style->standardIcon(QStyle::SP_DirOpenIcon), "Open Containing Folder");
        actOpenFolderSibling = menu.addAction(style->standardIcon(QStyle::SP_DirOpenIcon), "Open Containing Folder in Sibling Panel");
        actCopyPath = menu.addAction("Copy Absolute Path");
        
        menu.addSeparator();

        actCopy = menu.addAction(style->standardIcon(QStyle::SP_DialogSaveButton), "Copy");
        actCopy->setShortcut(QKeySequence::Copy);
        
        actCut = menu.addAction("Cut");
        actCut->setShortcut(QKeySequence::Cut);

        actCopyToSibling = menu.addAction("Copy to Sibling Panel");
        actMoveToSibling = menu.addAction("Move to Sibling Panel");
        
        actDelete = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), "Delete");
        actDelete->setShortcut(QKeySequence::Delete);

        actRename = menu.addAction("Rename...");
        
        menu.addSeparator();

        QString ext = info.suffix().toLower();
        if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg" || ext == "m4a") {
            actEditTags = menu.addAction("Edit Audio Tags...");
        } else if (ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "mov" || ext == "webm") {
            actScrapeVideo = menu.addAction("Scrape Video Metadata...");
        }

        actCalculateChecksum = menu.addAction("Calculate Checksum Hash...");
        actSecureShred = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), "Secure Shred (Delete Permanently)...");
        
        menu.addSeparator();
        actProp = menu.addAction(style->standardIcon(QStyle::SP_MessageBoxInformation), "Properties");
    } else {
        actCopyPath = menu.addAction("Copy Absolute Paths");
        menu.addSeparator();
        actCopy = menu.addAction(style->standardIcon(QStyle::SP_DialogSaveButton), QString("Copy %1 Files").arg(selectedFiles.size()));
        actCopyToSibling = menu.addAction(QString("Copy %1 Files to Sibling Panel").arg(selectedFiles.size()));
        actDelete = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), QString("Delete %1 Files").arg(selectedFiles.size()));
        actSecureShred = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), QString("Secure Shred %1 Files (Delete Permanently)...").arg(selectedFiles.size()));
    }

    QAction* selected = menu.exec(m_searchResultsView->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actOpen) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(firstFile));
    } else if (selected == actOpenFolder) {
        QString parentDir = info.absolutePath();
        navigateTo(parentDir, true);
        QModelIndex srcIndex = m_fileModel->index(firstFile);
        if (srcIndex.isValid()) {
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
            if (proxyIndex.isValid()) {
                m_treeView->setCurrentIndex(proxyIndex);
                m_treeView->scrollTo(proxyIndex);
                m_listView->setCurrentIndex(proxyIndex);
                m_listView->scrollTo(proxyIndex);
            }
        }
    } else if (selected == actOpenFolderSibling) {
        if (m_siblingPanel) {
            QString parentDir = info.absolutePath();
            m_siblingPanel->navigateTo(parentDir, true);
            QModelIndex srcIndex = m_siblingPanel->m_fileModel->index(firstFile);
            if (srcIndex.isValid()) {
                QModelIndex proxyIndex = m_siblingPanel->m_proxyModel->mapFromSource(srcIndex);
                if (proxyIndex.isValid()) {
                    m_siblingPanel->m_treeView->setCurrentIndex(proxyIndex);
                    m_siblingPanel->m_treeView->scrollTo(proxyIndex);
                    m_siblingPanel->m_listView->setCurrentIndex(proxyIndex);
                    m_siblingPanel->m_listView->scrollTo(proxyIndex);
                }
            }
            m_siblingPanel->setFocus();
        }
    } else if (selected == actCopyPath) {
        QStringList nativePaths;
        for (const QString& f : selectedFiles) {
            nativePaths.append(QDir::toNativeSeparators(f));
        }
        QApplication::clipboard()->setText(nativePaths.join("\n"));
    } else if (selected == actCopy) {
        QMimeData* mimeData = new QMimeData();
        QList<QUrl> urls;
        for (const QString& f : selectedFiles) {
            urls.append(QUrl::fromLocalFile(f));
        }
        mimeData->setUrls(urls);
        QApplication::clipboard()->setMimeData(mimeData);
    } else if (selected == actCut) {
        QMimeData* mimeData = new QMimeData();
        QList<QUrl> urls = { QUrl::fromLocalFile(firstFile) };
        mimeData->setUrls(urls);
        QByteArray cutEffect;
        cutEffect.append((char)1);
        mimeData->setData("application/x-kde-cutselection", cutEffect);
        mimeData->setData("Preferred Drop Effect", "Cut");
        QApplication::clipboard()->setMimeData(mimeData);
    } else if (selected == actCopyToSibling) {
        if (m_siblingPanel) {
            QString siblingPath = m_siblingPanel->currentPath();
            bool isRemote = false;
            if (siblingPath.startsWith("/run/user/") && siblingPath.contains("/gvfs/")) {
                isRemote = true;
            } else if (siblingPath.contains("CloudMounts") || siblingPath.startsWith(QDir::homePath() + "/CloudMounts")) {
                isRemote = true;
            } else if (siblingPath.startsWith("ftp://") || siblingPath.startsWith("sftp://") || siblingPath.startsWith("smb://")) {
                isRemote = true;
            }

            if (!siblingPath.isEmpty() && (isRemote || QDir(siblingPath).exists())) {
                for (const QString& f : selectedFiles) {
                    QFileInfo fi(f);
                    QString destPath = QDir(siblingPath).filePath(fi.fileName());
                    QFile::copy(f, destPath);
                }
                m_siblingPanel->refresh();
            }
        }
    } else if (selected == actMoveToSibling) {
        if (m_siblingPanel) {
            QString siblingPath = m_siblingPanel->currentPath();
            bool isRemote = false;
            if (siblingPath.startsWith("/run/user/") && siblingPath.contains("/gvfs/")) {
                isRemote = true;
            } else if (siblingPath.contains("CloudMounts") || siblingPath.startsWith(QDir::homePath() + "/CloudMounts")) {
                isRemote = true;
            } else if (siblingPath.startsWith("ftp://") || siblingPath.startsWith("sftp://") || siblingPath.startsWith("smb://")) {
                isRemote = true;
            }

            if (!siblingPath.isEmpty() && (isRemote || QDir(siblingPath).exists())) {
                for (const QString& f : selectedFiles) {
                    QFileInfo fi(f);
                    QString destPath = QDir(siblingPath).filePath(fi.fileName());
                    if (QFile::rename(f, destPath)) {
                        QFile(destPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                    }
                }
                refresh();
                m_siblingPanel->refresh();
            }
        }
    } else if (selected == actDelete) {
        QString confirmMsg = (selectedFiles.size() == 1)
            ? QString("Are you sure you want to delete '%1'?").arg(info.fileName())
            : QString("Are you sure you want to delete these %1 selected files?").arg(selectedFiles.size());
            
        if (QMessageBox::question(this, "Delete Files", confirmMsg) == QMessageBox::Yes) {
            QStringList currentList = m_searchResultModel->stringList();
            for (const QString& f : selectedFiles) {
                if (QFile::remove(f)) {
                    currentList.removeAll(f);
                }
            }
            m_searchResultModel->setStringList(currentList);
            m_statusLabel->setText(QString("Found %1 items").arg(currentList.size()));
        }
    } else if (selected == actRename) {
        bool ok;
        QString oldName = info.fileName();
        QString oldExt = info.suffix();
        QString newName = QInputDialog::getText(this, "Rename File", "New name:", QLineEdit::Normal, oldName, &ok);
        if (ok && !newName.isEmpty() && newName != oldName) {
            QSettings settings("Amifiles", "Amifiles");
            bool keepExt = settings.value("behavior/keep_extension_on_rename", true).toBool();
            if (keepExt && !oldExt.isEmpty() && !info.isDir()) {
                QString dotExt = "." + oldExt;
                if (!newName.endsWith(dotExt, Qt::CaseInsensitive)) {
                    newName += dotExt;
                }
            }
            QString newPath = info.absoluteDir().filePath(newName);
            if (QFile::rename(firstFile, newPath)) {
                QFile(newPath).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                QStringList currentList = m_searchResultModel->stringList();
                int idx = currentList.indexOf(firstFile);
                if (idx != -1) {
                    currentList[idx] = newPath;
                    m_searchResultModel->setStringList(currentList);
                }
            } else {
                QMessageBox::warning(this, "Rename File", "Failed to rename file.");
            }
        }
    } else if (selected == actEditTags) {
        TagEditorDialog dlg({firstFile}, this);
        dlg.exec();
    } else if (selected == actScrapeVideo) {
        VideoScraperDialog dlg({firstFile}, this);
        dlg.exec();
    } else if (selected == actCalculateChecksum) {
        ChecksumDialog dlg(firstFile, this);
        dlg.exec();
    } else if (selected == actSecureShred) {
        ShredDialog dlg(selectedFiles, this);
        if (dlg.exec() == QDialog::Accepted) {
            QStringList currentList = m_searchResultModel->stringList();
            for (const QString& f : selectedFiles) {
                currentList.removeAll(f);
            }
            m_searchResultModel->setStringList(currentList);
            m_statusLabel->setText(QString("Found %1 items").arg(currentList.size()));
        }
    } else if (selected == actProp) {
        QDialog propDlg(this);
        propDlg.setWindowTitle("Properties - " + info.fileName());
        QVBoxLayout* layout = new QVBoxLayout(&propDlg);
        
        QLabel* lblName = new QLabel("<b>Name:</b> " + info.fileName(), &propDlg);
        QLabel* lblPath = new QLabel("<b>Path:</b> " + firstFile, &propDlg);
        QLabel* lblSize = new QLabel("<b>Size:</b> " + QString::number(info.size()) + " bytes", &propDlg);
        QLabel* lblModified = new QLabel("<b>Modified:</b> " + info.lastModified().toString(), &propDlg);
        
        layout->addWidget(lblName);
        layout->addWidget(lblPath);
        layout->addWidget(lblSize);
        layout->addWidget(lblModified);
        
        QPushButton* btnClose = new QPushButton("Close", &propDlg);
        connect(btnClose, &QPushButton::clicked, &propDlg, &QDialog::accept);
        layout->addWidget(btnClose);
        
        propDlg.exec();
    }
}

void FilePanel::onSearchEditContextMenu(const QPoint& pos) {
    QMenu menu(this);
    QStyle* style = QApplication::style();
    
    QAction* actSavePreset = menu.addAction("Save Current Search as Preset...");
    QMenu* menuPresets = menu.addMenu("Load Search Preset");
    
    QSettings settings("Amifiles", "Amifiles");
    QVariantList presets = settings.value("search/presets").toList();
    if (presets.isEmpty()) {
        QAction* emptyAct = menuPresets->addAction("No presets saved");
        emptyAct->setEnabled(false);
    } else {
        for (const QVariant& pVar : presets) {
            QVariantMap pMap = pVar.toMap();
            QString name = pMap["name"].toString();
            QString query = pMap["query"].toString();
            QAction* pAct = menuPresets->addAction(QString("%1 (%2)").arg(name, query));
            pAct->setData(query);
            connect(pAct, &QAction::triggered, this, [this, pAct]() {
                setSearchQuery(pAct->data().toString());
            });
        }
    }
    
    QAction* actClear = menu.addAction("Clear Search");
    
    menu.addSeparator();
    
    QAction* actUndo = menu.addAction("Undo");
    actUndo->setEnabled(m_globalSearchEdit->isUndoAvailable());
    connect(actUndo, &QAction::triggered, m_globalSearchEdit, &QLineEdit::undo);
    
    QAction* actRedo = menu.addAction("Redo");
    actRedo->setEnabled(m_globalSearchEdit->isRedoAvailable());
    connect(actRedo, &QAction::triggered, m_globalSearchEdit, &QLineEdit::redo);
    
    menu.addSeparator();
    
    QAction* actCut = menu.addAction("Cut");
    actCut->setEnabled(m_globalSearchEdit->hasSelectedText());
    connect(actCut, &QAction::triggered, m_globalSearchEdit, &QLineEdit::cut);
    
    QAction* actCopy = menu.addAction("Copy");
    actCopy->setEnabled(m_globalSearchEdit->hasSelectedText());
    connect(actCopy, &QAction::triggered, m_globalSearchEdit, &QLineEdit::copy);
    
    QAction* actPaste = menu.addAction("Paste");
    connect(actPaste, &QAction::triggered, m_globalSearchEdit, &QLineEdit::paste);
    
    QAction* actSelectAll = menu.addAction("Select All");
    connect(actSelectAll, &QAction::triggered, m_globalSearchEdit, &QLineEdit::selectAll);

    actSavePreset->setEnabled(!m_globalSearchEdit->text().trimmed().isEmpty());

    QAction* selected = menu.exec(m_globalSearchEdit->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actSavePreset) {
        QString query = m_globalSearchEdit->text().trimmed();
        bool ok;
        QString name = QInputDialog::getText(this, "Save Search Preset", 
                                             "Enter a name for this search preset:", 
                                             QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            QVariantMap newPreset;
            newPreset["name"] = name.trimmed();
            newPreset["query"] = query;
            presets.append(newPreset);
            settings.setValue("search/presets", presets);
        }
    } else if (selected == actClear) {
        m_globalSearchEdit->clear();
    }
}

void FilePanel::onSearchResultSelected(const QModelIndex& index) {
    QString filePath = index.data().toString();
    emit fileSelected(filePath);
}

void FilePanel::onSearchResultDoubleClicked(const QModelIndex& index) {
    QString filePath = index.data().toString();
    QFileInfo info(filePath);
    if (!info.exists()) {
        return;
    }

    if (info.isDir()) {
        navigateTo(filePath, true);
    } else {
        QString parentDir = info.absolutePath();
        navigateTo(parentDir, true);

        QModelIndex srcIndex = m_fileModel->index(filePath);
        if (srcIndex.isValid()) {
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
            if (proxyIndex.isValid()) {
                m_treeView->setCurrentIndex(proxyIndex);
                m_treeView->scrollTo(proxyIndex);
                m_listView->setCurrentIndex(proxyIndex);
                m_listView->scrollTo(proxyIndex);
            }
        }
    }

    m_globalSearchEdit->clear();
}

ColumnSelectorDialog::ColumnSelectorDialog(const QStringList& columnNames, const QList<bool>& visibilities, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Configure Columns");
    resize(420, 260);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; font-weight: bold; }"
                  "QCheckBox { color: #cdd6f4; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; }"
                  "QPushButton:hover { background-color: #45475a; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    mainLayout->addWidget(new QLabel("Select columns to display in tree view details:", this));

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(6);

    int rows = (columnNames.size() + 1) / 2;
    for (int i = 0; i < columnNames.size(); ++i) {
        QCheckBox* cb = new QCheckBox(columnNames[i], this);
        cb->setChecked(visibilities[i]);
        if (i == 0) {
            cb->setEnabled(false); // Name column is always visible
        }
        m_checkboxes.append(cb);
        grid->addWidget(cb, i % rows, i / rows);
    }
    mainLayout->addLayout(grid);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    
    QPushButton* btnOk = new QPushButton("OK", this);
    btnOk->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; }");
    QObject::connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    
    QPushButton* btnCancel = new QPushButton("Cancel", this);
    QObject::connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);
}

QList<bool> ColumnSelectorDialog::selectedVisibilities() const {
    QList<bool> vis;
    for (QCheckBox* cb : m_checkboxes) {
        vis.append(cb->isChecked());
    }
    return vis;
}

void FilePanel::onViewModeChanged(int index) {
    int internalIdx = comboIndexToInternal(index);

    if (internalIdx == 0) { // Details Table
        m_listView->setItemDelegate(m_defaultDelegate);
        m_viewStack->setCurrentWidget(m_treeView);
    } else if (internalIdx == 1) { // Grid / Icons
        m_listView->setItemDelegate(m_defaultDelegate);
        m_listView->setGridSize(QSize());
        m_viewStack->setCurrentWidget(m_listView);
    } else if (internalIdx == 2) { // Card / Tiles
        m_listView->setItemDelegate(m_cardDelegate);
        m_listView->setGridSize(QSize(195, 75));
        m_viewStack->setCurrentWidget(m_listView);
    } else if (internalIdx == 3) { // Miller Columns
        m_millerView->setRootPath(m_currentPath);
        m_viewStack->setCurrentWidget(m_millerView);
    } else if (internalIdx == 4) { // Chronological Timeline
        m_timelineView->setRootPath(m_currentPath);
        m_viewStack->setCurrentWidget(m_timelineView);
    } else if (internalIdx == 5) { // Filmstrip View
        m_filmstripView->setRootPath(m_currentPath);
        m_viewStack->setCurrentWidget(m_filmstripView);
    } else if (internalIdx >= 8 && internalIdx <= 10) { // 8: Movies Full Screen, 9: TV Shows Full Screen, 10: Music Full Screen
        if (m_theaterDelegate) {
            m_theaterDelegate->setCinemaMode(internalIdx == 8 || internalIdx == 9);
            m_theaterDelegate->setShowcaseViewMode(internalIdx);
        }
        updateTheaterGridSize();
        m_viewStack->setCurrentWidget(m_theaterContainer);
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            m_theaterListView->setVisible(false);
            m_theaterScrollArea->setVisible(true);
            queueRebuildTheaterGroups();
        } else {
            m_theaterListView->setVisible(true);
            m_theaterScrollArea->setVisible(false);
        }
    } else if (internalIdx == 11) { // Cover Flow Carousel
        if (m_coverFlowView) {
            m_coverFlowView->setRootIndex(m_listView->rootIndex());
            m_coverFlowView->setSelectedIndex(0);
            m_viewStack->setCurrentWidget(m_coverFlowView);
        }
    }
    
    // Save view mode index choice in preferences
    QSettings settings("Amifiles", "Amifiles");
    bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool() && (internalIdx == 10);
    if (m_proxyModel) {
        if (internalIdx == 8) {
            m_proxyModel->setShowcaseMode(3); // Movie Showcase (v2)
        } else if (internalIdx == 9) {
            m_proxyModel->setShowcaseMode(4); // TV Show Showcase (v2)
        } else if (internalIdx == 10) {
            m_proxyModel->setShowcaseMode(5); // Music Showcase (v2)
        } else {
            m_proxyModel->setShowcaseMode(0); // Standard View
        }

        m_proxyModel->setGroupMultiDiscActive(groupMultiDisc);
        updateHideSettings();
    }

    // Do not save view mode change if it is triggered programmatically by a folder profile
    QWidget* w = this;
    MainWindow* mainWin = nullptr;
    while (w) {
        if (MainWindow* mw = qobject_cast<MainWindow*>(w)) {
            mainWin = mw;
            break;
        }
        w = w->parentWidget();
    }
    if (!mainWin || !mainWin->isApplyingFolderProfile()) {
        settings.setValue("file_panel/view_mode_index", internalIdx);
    }

    onSelectionChanged(); // Trigger layout update for bottom info panel

    if (m_btnToggleSidePane) {
        bool groupingActive = m_groupProxy && m_groupProxy->isGroupingActive();
        m_btnToggleSidePane->setVisible((internalIdx >= 8 && internalIdx <= 10) || groupingActive);
        if (m_theaterSideContainer) {
            updateDrawerVisibility();
        }
        if (m_trackListWidget) {
            m_trackListWidget->setVisible(internalIdx >= 8 && internalIdx <= 10 && !groupingActive);
        }
        if (m_drawerBtnContainer) {
            m_drawerBtnContainer->setVisible(internalIdx >= 8 && internalIdx <= 10 && !groupingActive);
        }
    }
    emit viewModeChanged();
    updateThemeMusic();
}

void FilePanel::onPlaybackStateChanged(int state) {
    bool playing = (state == QMediaPlayer::PlayingState);
    if (m_visualizerWidget) {
        m_visualizerWidget->setPlaying(playing);
    }
    if (m_btnPlayPause) {
        m_btnPlayPause->setText(playing ? "⏸" : "▶");
    }
}

void FilePanel::onDoubleClickedPath(const QString& path) {
    QFileInfo info(path);
    bool builtinPlayerDoubleclick = false;
    {
        QWidget* p = parentWidget();
        while (p && !p->inherits("MainWindow")) {
            p = p->parentWidget();
        }
        if (p) {
            QMetaObject::invokeMethod(p, "isBuiltinPlayerDoubleclickActive", Q_RETURN_ARG(bool, builtinPlayerDoubleclick));
        }
    }

    QSettings settings("Amifiles", "Amifiles");
    bool doubleclickAddsToQueue = settings.value("preferences/doubleclick_adds_to_queue", false).toBool();
    bool zenActive = settings.value("preferences/zen_mode", false).toBool();
    bool isTheater = (m_viewStack->currentWidget() == m_theaterContainer || m_viewStack->currentWidget() == m_theaterListView);

    bool shouldPlayOnDoubleclick = builtinPlayerDoubleclick;

    if (info.isDir()) {
        // In standard views (Details, List, Icons, Cards, Miller, etc.), ALWAYS navigate into the directory
        // For TV Shows / Movies / Video Showcase views, also navigate/drill-down into the directory!
        if (!isTheater || viewModeIndex() == 7 || viewModeIndex() == 8 || viewModeIndex() == 9) {
            navigateTo(path, true);
            return;
        }

        // In Theater / Showcase view: Only trigger media playback for actual playable album/movie folders
        bool isPlayableAlbum = isPlayableAlbumFolder(path);
        bool isMusicShowcaseV2 = (viewModeIndex() == 10);
        if (isPlayableAlbum && (isMusicShowcaseV2 || shouldPlayOnDoubleclick || doubleclickAddsToQueue)) {
            bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool() && isTheater;

            QStringList scanPaths;
            if (groupMultiDisc) {
                QString folderName = info.fileName();
                QString parentDir = info.absolutePath();
                QDir dir(parentDir);
                QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                QString currentCleaned = FileFilterProxyModel::cleanAlbumFolderName(folderName);
                for (const QString& subDirName : subDirs) {
                    if (FileFilterProxyModel::cleanAlbumFolderName(subDirName) == currentCleaned) {
                        scanPaths.append(dir.filePath(subDirName));
                    }
                }
            } else {
                scanPaths.append(path);
            }

            int filter = 0;
            int vm = viewModeIndex();
            if (vm == 6 || vm == 10) filter = 1;
            else if (vm == 7 || vm == 8 || vm == 9) filter = 2;

            QStringList playlistPaths;
            for (const QString& scanPath : scanPaths) {
                scanMediaFilesRecursively(scanPath, playlistPaths, filter);
            }

            if (!playlistPaths.isEmpty()) {
                if (doubleclickAddsToQueue) {
                    emit queueMediaBuiltinRequested(playlistPaths);
                } else {
                    if (viewModeIndex() == 8 || viewModeIndex() == 9) {
                        emit playMediaFullscreenRequested(playlistPaths);
                    } else if (shouldPlayOnDoubleclick || viewModeIndex() == 10) {
                        emit playMediaBuiltinRequested(playlistPaths);
                    }
                }
                return;
            }
        }

        // Otherwise navigate into the directory
        navigateTo(path, true);
        return;
    } else {
        MainWindow::addToRecentFiles(path);
        QString ext = info.suffix().toLower();
        static const QStringList mediaExts = {
            "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "mod", "sid", "s3m", "xm", "it",
            "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg"
        };
        if (mediaExts.contains(ext)) {
            if (doubleclickAddsToQueue) {
                emit queueMediaBuiltinRequested({path});
                return;
            } else if (shouldPlayOnDoubleclick || isTheater) {
                bool isVideo = (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" || ext == "webm" || ext == "wmv" || ext == "m4v" || ext == "mpg" || ext == "mpeg");
                bool isShowcaseVideo = (viewModeIndex() == 7 || viewModeIndex() == 8 || viewModeIndex() == 9);
                if ((viewModeIndex() == 8 || viewModeIndex() == 9) || isShowcaseVideo || (isVideo && shouldPlayOnDoubleclick)) {
                    emit playMediaFullscreenRequested({path});
                } else {
                    emit playMediaBuiltinRequested({path});
                }
                return;
            }
        }

        bool archiveNavEnabled = settings.value("preferences/archive_nav", true).toBool();
        QStringList archiveExts = { "zip", "tar", "gz", "xz", "bz2", "tgz", "rar", "7z", "adf", "adz", "d64", "d71", "d81", "g64", "iso", "img" };

        if (archiveNavEnabled && archiveExts.contains(ext)) {
            m_statusLabel->setText("Loading archive...");
            QApplication::processEvents();
            bool ok = m_archiveModel->loadArchive(path);
            updateStatusText();
            if (ok) {
                m_archiveViewActive = true;
                m_currentPath = path + "//";
                updateActiveViewModel();

                m_pathEdit->setText(QDir::toNativeSeparators(path) + "//");
                
                for (int i = 0; i < 4; ++i) {
                    m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
                }

                if (m_categoryWidget) m_categoryWidget->hide();

                emit pathChanged(m_currentPath);
            } else {
                QMessageBox::warning(this, "Load Archive", "Failed to parse archive file listing.");
            }
        } else if (ext == "cbz" || ext == "cbr") {
            ComicBookViewerDialog dlg(path, this);
            dlg.exec();
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    }
}

int FilePanel::viewModeIndex() const {
    return m_comboViewMode ? comboIndexToInternal(m_comboViewMode->currentIndex()) : 0;
}

int FilePanel::getTrackListCurrentIndex() const {
    return m_trackListWidget ? m_trackListWidget->currentRow() : -1;
}

int FilePanel::getTrackListCount() const {
    return m_trackListWidget ? m_trackListWidget->count() : 0;
}

QString FilePanel::getTrackListPathAt(int index) const {
    if (!m_trackListWidget || index < 0 || index >= m_trackListWidget->count()) return QString();
    QListWidgetItem* item = m_trackListWidget->item(index);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QStringList FilePanel::getTrackListPaths() const {
    QStringList plist;
    if (!m_trackListWidget) return plist;
    for (int i = 0; i < m_trackListWidget->count(); ++i) {
        QString p = m_trackListWidget->item(i)->data(Qt::UserRole).toString();
        if (!p.isEmpty()) plist.append(p);
    }
    return plist;
}

void FilePanel::setShuffleState(bool enabled) {
    if (m_btnShuffle) {
        m_btnShuffle->blockSignals(true);
        m_btnShuffle->setChecked(enabled);
        m_btnShuffle->blockSignals(false);
    }
}

void FilePanel::setRepeatState(int mode) {
    if (m_btnRepeat) {
        m_btnRepeat->blockSignals(true);
        m_btnRepeat->setChecked(mode > 0);
        if (mode == 0) m_btnRepeat->setToolTip("Repeat: Off");
        else if (mode == 1) m_btnRepeat->setToolTip("Repeat: One");
        else if (mode == 2) m_btnRepeat->setToolTip("Repeat: All");
        m_btnRepeat->blockSignals(false);
    }
}

void FilePanel::setViewModeIndex(int index) {
    if (m_comboViewMode) {
        m_comboViewMode->setCurrentIndex(internalToComboIndex(index));
    }
}

void FilePanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateCloneButtonIcon();
}

void FilePanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_groupProxy && m_groupProxy->isGroupingActive() && m_viewStack->currentWidget() == m_theaterContainer) {
        queueRebuildTheaterGroups();
    }
}

void FilePanel::updateCloneButtonIcon() {
    QStyle* style = QApplication::style();
    bool isLeft = true;
    QWidget* temp = parentWidget();
    while (temp) {
        if (temp->objectName() == "leftTabWidget") {
            isLeft = true;
            break;
        } else if (temp->objectName() == "rightTabWidget") {
            isLeft = false;
            break;
        }
        temp = temp->parentWidget();
    }

    if (isLeft) {
        m_btnClonePath->setIcon(style->standardIcon(QStyle::SP_ArrowRight));
        m_btnClonePath->setToolTip("Clone current path to the right panel");
    } else {
        m_btnClonePath->setIcon(style->standardIcon(QStyle::SP_ArrowLeft));
        m_btnClonePath->setToolTip("Clone current path to the left panel");
    }
}


static QString findFirstCaseInsensitiveFile(const QString& dirPath, const QStringList& candidates) {
    QDir dir(dirPath);
    QStringList files = dir.entryList(QDir::Files);
    for (const QString& candidate : candidates) {
        for (const QString& file : files) {
            if (file.compare(candidate, Qt::CaseInsensitive) == 0) {
                return dir.filePath(file);
            }
        }
    }
    return "";
}

static QString generateSeasonPlaceholder(const QString& showTitle, int seasonNum) {
    QString cacheDir = QDir::temp().filePath("amifiles_cache/season_placeholders");
    QDir().mkpath(cacheDir);
    
    QString safeTitle = showTitle;
    safeTitle.remove(QRegularExpression(R"([\\\/\:\*\?\"\<\>\|])"));
    QString destPath = QDir(cacheDir).filePath(QString("%1_season_%2.jpg").arg(safeTitle).arg(seasonNum));
    
    if (QFile::exists(destPath)) {
        return destPath;
    }
    
    QImage img(600, 900, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QLinearGradient grad(0, 0, 0, 900);
    grad.setColorAt(0.0, QColor("#1e1e2e"));
    grad.setColorAt(0.5, QColor("#181825"));
    grad.setColorAt(1.0, QColor("#11111b"));
    painter.fillRect(img.rect(), grad);
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor("#89b4fa")));
    painter.drawRect(0, 0, 600, 15);
    
    QRadialGradient ringGrad(300, 450, 200, 300, 450);
    ringGrad.setColorAt(0.0, QColor(137, 180, 250, 40));
    ringGrad.setColorAt(0.8, QColor(203, 166, 247, 10));
    ringGrad.setColorAt(1.0, Qt::transparent);
    painter.setBrush(QBrush(ringGrad));
    painter.drawEllipse(100, 250, 400, 400);
    
    painter.setPen(QPen(QColor("#cba6f7"), 4));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(150, 300, 300, 300);
    
    painter.setPen(QPen(QColor("#a6e3a1"), 2));
    QFont symbolFont("Outfit", 96, QFont::Bold);
    painter.setFont(symbolFont);
    painter.drawText(QRect(150, 300, 300, 300), Qt::AlignCenter, "📺");
    
    painter.setPen(QPen(QColor("#cdd6f4")));
    QFont titleFont("Outfit", 36, QFont::Bold);
    painter.setFont(titleFont);
    painter.drawText(QRect(40, 60, 520, 150), Qt::AlignCenter | Qt::TextWordWrap, showTitle);
    
    painter.setPen(QPen(QColor("#f9e2af")));
    QFont seasonFont("Outfit", 40, QFont::Bold);
    painter.setFont(seasonFont);
    QString seasonText = (seasonNum == 0) ? "SPECIALS" : QString("SEASON %1").arg(seasonNum);
    painter.drawText(QRect(40, 720, 520, 100), Qt::AlignCenter, seasonText);
    
    painter.end();
    
    if (img.save(destPath, "JPG", 90)) {
        return destPath;
    }
    return QString();
}

CasingRunnable::CasingRunnable(QPointer<FileFilterProxyModel> model, const QString& path)
    : m_model(model), m_path(path) {
    setAutoDelete(true);
}

void CasingRunnable::run() {
    if (!m_model) return;
    QString artPath;
    int casingInt = 0; // CasingCD = 0, CasingDVD = 1, CasingBluRay = 2
    
    QFileInfo fileInfo(m_path);
    bool isDir = fileInfo.isDir();
    QString dirPath = isDir ? m_path : fileInfo.absolutePath();
    QString baseName = fileInfo.baseName();
    
    if (!isDir) {
        // 1. Check file-specific cover art first
        QStringList bluraySpecific = {
            baseName + "_bluray_cover.jpg", baseName + "_bluray_cover.jpeg", baseName + "_bluray_cover.png",
            baseName + "_bluray.jpg", baseName + "_bluray.jpeg", baseName + "_bluray.png"
        };
        artPath = findFirstCaseInsensitiveFile(dirPath, bluraySpecific);
        if (!artPath.isEmpty()) {
            casingInt = 2; // CasingBluRay
        } else {
            QStringList dvdSpecific = {
                baseName + "_dvd_cover.jpg", baseName + "_dvd_cover.jpeg", baseName + "_dvd_cover.png",
                baseName + "_dvd.jpg", baseName + "_dvd.jpeg", baseName + "_dvd.png"
            };
            artPath = findFirstCaseInsensitiveFile(dirPath, dvdSpecific);
            if (!artPath.isEmpty()) {
                casingInt = 1; // CasingDVD
            } else {
                QStringList fileSpecificChecks = {
                    baseName + "_cover.jpg", baseName + "_cover.jpeg", baseName + "_cover.png",
                    baseName + ".jpg", baseName + ".jpeg", baseName + ".png"
                };
                artPath = findFirstCaseInsensitiveFile(dirPath, fileSpecificChecks);
                
                if (!artPath.isEmpty()) {
                    QString ext = fileInfo.suffix().toLower();
                    QStringList videoExts = { "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v" };
                    if (videoExts.contains(ext)) {
                        QString lowerName = fileInfo.fileName().toLower();
                        if (lowerName.contains("bluray") || lowerName.contains("blu-ray")) {
                            casingInt = 2; // CasingBluRay
                        } else {
                            casingInt = 1; // CasingDVD
                        }
                    } else {
                        casingInt = 0; // CasingCD
                    }
                }
            }
        }
    }

    // 2. Check Blu-ray covers next
    if (artPath.isEmpty()) {
        bool checkBluRay = isDir;
        if (!isDir) {
            QString lowerName = fileInfo.fileName().toLower();
            if (lowerName.contains("bluray") || lowerName.contains("blu-ray")) {
                checkBluRay = true;
            }
        }
        if (checkBluRay) {
            QStringList blurayChecks = { "bluray_cover.jpg", "bluray_cover.jpeg", "bluray_cover.png", "bluray.jpg", "bluray.jpeg", "bluray.png", "blu-ray_cover.jpg", "blu-ray_cover.jpeg", "blu-ray_cover.png", "blu-ray.jpg", "blu-ray.jpeg", "blu-ray.png" };
            artPath = findFirstCaseInsensitiveFile(dirPath, blurayChecks);
            if (!artPath.isEmpty()) {
                casingInt = 2; // CasingBluRay
            }
        }
    }

    // 3. Check DVD covers next
    if (artPath.isEmpty()) {
        bool checkDVD = isDir;
        if (!isDir) {
            QString ext = fileInfo.suffix().toLower();
            QStringList videoExts = { "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v" };
            if (videoExts.contains(ext)) {
                checkDVD = true;
            }
        }
        if (checkDVD) {
            QStringList dvdChecks = { "dvd_cover.jpg", "dvd_cover.jpeg", "dvd_cover.png", "dvd.jpg", "dvd.jpeg", "dvd.png", "movie.jpg", "movie.jpeg", "movie.png", "poster.jpg", "poster.jpeg", "poster.png" };
            artPath = findFirstCaseInsensitiveFile(dirPath, dvdChecks);
            if (!artPath.isEmpty()) {
                casingInt = 1; // CasingDVD
            }
        }
    }

    // 4. Fall back to CD covers and common album art filenames
    if (artPath.isEmpty()) {
        QStringList cdChecks = {
            "folder.jpg", "folder.jpeg", "folder.png",
            "cover.jpg", "cover.jpeg", "cover.png",
            "front.jpg", "front.jpeg", "front.png",
            "album.jpg", "album.jpeg", "album.png",
            "art.jpg", "art.jpeg", "art.png",
            "jacket.jpg", "jacket.jpeg", "jacket.png",
            "disc.jpg", "disc.jpeg", "disc.png",
            "cd.jpg", "cd.jpeg", "cd.png",
            "image.jpg", "image.jpeg", "image.png",
            "thumb.jpg", "thumb.jpeg", "thumb.png",
            "poster.jpg", "poster.jpeg", "poster.png"
        };
        artPath = findFirstCaseInsensitiveFile(dirPath, cdChecks);
        if (!artPath.isEmpty()) {
            casingInt = 0; // CasingCD
        }
    }

    // 4.5 Check parent directory for seasonXX.jpg or generate placeholders if this is a Season folder!
    if (artPath.isEmpty() && isDir) {
        QString folderName = fileInfo.fileName().toLower().trimmed();
        QRegularExpression reSeason(R"(^season\s*(\d+))");
        auto match = reSeason.match(folderName);
        if (match.hasMatch()) {
            int seasonNum = match.captured(1).toInt();
            QString parentPath = fileInfo.absolutePath(); // Parent directory (TV Show root)
            QStringList seasonChecks = {
                QString("season%1.jpg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season%1.png").arg(seasonNum, 2, 10, QChar('0')),
                QString("season%1.jpeg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season-%1.jpg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season-%1.png").arg(seasonNum, 2, 10, QChar('0')),
                QString("season-%1.jpeg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season%1.jpg").arg(seasonNum),
                QString("season%1.png").arg(seasonNum),
                QString("season%1.jpeg").arg(seasonNum),
                QString("season-%1.jpg").arg(seasonNum),
                QString("season-%1.png").arg(seasonNum),
                QString("season-%1.jpeg").arg(seasonNum)
            };
            artPath = findFirstCaseInsensitiveFile(parentPath, seasonChecks);
            if (!artPath.isEmpty()) {
                casingInt = 1; // CasingDVD for TV season folders
            } else {
                QString showTitle = QDir(parentPath).dirName();
                artPath = generateSeasonPlaceholder(showTitle, seasonNum);
                if (!artPath.isEmpty()) {
                    casingInt = 1;
                }
            }
        } else if (folderName.contains("special") || folderName.contains("specials")) {
            QString parentPath = fileInfo.absolutePath();
            QStringList specialsChecks = {
                "season-specials.jpg", "season-specials.png", "season-specials.jpeg",
                "season00.jpg", "season00.png", "season00.jpeg",
                "season0.jpg", "season0.png", "season0.jpeg"
            };
            artPath = findFirstCaseInsensitiveFile(parentPath, specialsChecks);
            if (!artPath.isEmpty()) {
                casingInt = 1;
            } else {
                QString showTitle = QDir(parentPath).dirName();
                artPath = generateSeasonPlaceholder(showTitle, 0);
                if (!artPath.isEmpty()) {
                    casingInt = 1;
                }
            }
        }
    }

    // 4.6 If it is a file (episode) and we still have no artwork, check parent directories for Season/Show cover fallbacks!
    if (!isDir && artPath.isEmpty()) {
        QDir containingDir(dirPath);
        QString containingFolderName = containingDir.dirName().toLower().trimmed();
        QRegularExpression reSeason(R"(^season\s*(\d+))");
        auto match = reSeason.match(containingFolderName);
        if (match.hasMatch()) {
            int seasonNum = match.captured(1).toInt();
            QString parentPath = QFileInfo(dirPath).absolutePath(); // TV Show root directory
            QStringList seasonChecks = {
                QString("season%1.jpg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season%1.png").arg(seasonNum, 2, 10, QChar('0')),
                QString("season%1.jpeg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season-%1.jpg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season-%1.png").arg(seasonNum, 2, 10, QChar('0')),
                QString("season-%1.jpeg").arg(seasonNum, 2, 10, QChar('0')),
                QString("season%1.jpg").arg(seasonNum),
                QString("season%1.png").arg(seasonNum),
                QString("season%1.jpeg").arg(seasonNum)
            };
            artPath = findFirstCaseInsensitiveFile(parentPath, seasonChecks);
            if (!artPath.isEmpty()) {
                casingInt = 1; // CasingDVD for episode
            } else {
                // Fall back to main show folder.jpg in the parent directory!
                QStringList mainShowChecks = { "folder.jpg", "folder.png", "poster.jpg", "poster.png", "cover.jpg", "cover.png" };
                artPath = findFirstCaseInsensitiveFile(parentPath, mainShowChecks);
                if (!artPath.isEmpty()) {
                    casingInt = 1;
                }
            }
        } else if (containingFolderName.contains("special") || containingFolderName.contains("specials")) {
            QString parentPath = QFileInfo(dirPath).absolutePath();
            QStringList specialsChecks = {
                "season-specials.jpg", "season-specials.png", "season-specials.jpeg",
                "season00.jpg", "season00.png", "season00.jpeg"
            };
            artPath = findFirstCaseInsensitiveFile(parentPath, specialsChecks);
            if (!artPath.isEmpty()) {
                casingInt = 1;
            } else {
                QStringList mainShowChecks = { "folder.jpg", "folder.png", "poster.jpg", "poster.png", "cover.jpg", "cover.png" };
                artPath = findFirstCaseInsensitiveFile(parentPath, mainShowChecks);
                if (!artPath.isEmpty()) {
                    casingInt = 1;
                }
            }
        } else {
            // General video fallback: check if parent directory has folder.jpg
            QString parentPath = QFileInfo(dirPath).absolutePath();
            QStringList mainShowChecks = { "folder.jpg", "folder.png", "poster.jpg", "poster.png", "cover.jpg", "cover.png" };
            artPath = findFirstCaseInsensitiveFile(parentPath, mainShowChecks);
            if (!artPath.isEmpty()) {
                casingInt = 1;
            }
        }
    }

    // 5. If still empty, scan directory for ANY image file (*.jpg, *.jpeg, *.png, *.webp, *.bmp)
    if (artPath.isEmpty() && isDir) {
        QDir d(dirPath);
        QFileInfoList imgFiles = d.entryInfoList({"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp"}, QDir::Files, QDir::Name);
        if (!imgFiles.isEmpty()) {
            artPath = imgFiles.first().absoluteFilePath();
            casingInt = 0; // CasingCD
        }
    }

    // 6. If still empty, check sibling disc folders if groupMultiDisc is active
    QSettings settings("Amifiles", "Amifiles");
    bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool();
    if (artPath.isEmpty() && isDir && groupMultiDisc) {
        QString parentDir = fileInfo.absolutePath();
        QDir dir(parentDir);
        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QString currentCleaned = FileFilterProxyModel::cleanAlbumFolderName(fileInfo.fileName());
        for (const QString& subDirName : subDirs) {
            if (subDirName != fileInfo.fileName() && FileFilterProxyModel::cleanAlbumFolderName(subDirName) == currentCleaned) {
                QString siblingPath = dir.filePath(subDirName);
                
                // 1. Blu-ray
                QStringList blurayChecks = { "bluray_cover.jpg", "bluray_cover.jpeg", "bluray_cover.png", "bluray.jpg", "bluray.jpeg", "bluray.png" };
                artPath = findFirstCaseInsensitiveFile(siblingPath, blurayChecks);
                if (!artPath.isEmpty()) { casingInt = 2; break; }
                
                // 2. DVD
                QStringList dvdChecks = { "dvd_cover.jpg", "dvd_cover.jpeg", "dvd_cover.png", "dvd.jpg", "dvd.jpeg", "dvd.png", "movie.jpg", "movie.jpeg", "movie.png", "poster.jpg", "poster.jpeg", "poster.png" };
                artPath = findFirstCaseInsensitiveFile(siblingPath, dvdChecks);
                if (!artPath.isEmpty()) { casingInt = 1; break; }
                
                // 3. CD
                QStringList cdChecks = {
                    "folder.jpg", "folder.jpeg", "folder.png",
                    "cover.jpg", "cover.jpeg", "cover.png",
                    "front.jpg", "front.jpeg", "front.png",
                    "album.jpg", "album.jpeg", "album.png",
                    "art.jpg", "art.jpeg", "art.png",
                    "jacket.jpg", "jacket.jpeg", "jacket.png",
                    "disc.jpg", "disc.jpeg", "disc.png",
                    "cd.jpg", "cd.jpeg", "cd.png",
                    "image.jpg", "image.jpeg", "image.png"
                };
                artPath = findFirstCaseInsensitiveFile(siblingPath, cdChecks);
                if (!artPath.isEmpty()) { casingInt = 0; break; }

                // 4. Wildcard scan in sibling folder
                QDir sibDir(siblingPath);
                QFileInfoList imgFiles = sibDir.entryInfoList({"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp"}, QDir::Files, QDir::Name);
                if (!imgFiles.isEmpty()) {
                    artPath = imgFiles.first().absoluteFilePath();
                    casingInt = 0;
                    break;
                }
            }
        }
    }

    if (artPath.isEmpty()) {
        QPointer<FileFilterProxyModel> model = m_model;
        QString path = m_path;
        QMetaObject::invokeMethod(model.data(), [model, path]() {
            if (model) {
                model->onCasingRendered(path, "", 0, QImage());
            }
        }, Qt::QueuedConnection);
        return;
    }

    QPointer<FileFilterProxyModel> model = m_model;
    if (!model) return;
    int zoomSize = model->m_zoomIconSize;
    double scaleFactor = (double)zoomSize / 32.0;
    if (scaleFactor < 0.25) scaleFactor = 0.25;

    QImage cover;
    if (cover.load(artPath)) {
        int targetW = qRound(220.0 * scaleFactor);
        int targetH = qRound(220.0 * scaleFactor);
        if (casingInt == 1) { // DVD
            targetW = qRound(154.0 * scaleFactor);
            targetH = qRound(240.0 * scaleFactor);
        } else if (casingInt == 2) { // BluRay
            targetW = qRound(164.0 * scaleFactor);
            targetH = qRound(226.0 * scaleFactor);
        }
        cover = cover.scaled(QSize(targetW, targetH), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    if (cover.isNull()) {
        QString path = m_path;
        QMetaObject::invokeMethod(model.data(), [model, path]() {
            if (model) {
                model->onCasingRendered(path, "", 0, QImage());
            }
        }, Qt::QueuedConnection);
        return;
    }

    QString casingType = settings.value("music_showcase/casing_type", "cd").toString();

    int caseW = qRound(256.0 * scaleFactor);
    int caseH = qRound(256.0 * scaleFactor);
    if (casingInt == 1) { // DVD
        caseW = qRound(170.0 * scaleFactor);
    } else if (casingInt == 2) { // BluRay
        caseW = qRound(180.0 * scaleFactor);
    } else if (casingInt == 0 && casingType == "vinyl") {
        caseW = qRound(300.0 * scaleFactor);
    }

    double s = (256.0 / 48.0) * scaleFactor;

    QImage caseImage(caseW, caseH, QImage::Format_ARGB32_Premultiplied);
    caseImage.fill(Qt::transparent);
    QImage hoverImage;

    QPainter painter(&caseImage);
    painter.setRenderHint(QPainter::Antialiasing);

    if (casingInt == 0) { // CasingCD
        QSettings settings("Amifiles", "Amifiles");
        QString casingType = settings.value("music_showcase/casing_type", "cd").toString();

        if (casingType == "vinyl") {
            painter.end();

            auto renderVinyl = [&](bool hovered) {
                QImage img(caseW, caseH, QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::transparent);
                QPainter p(&img);
                p.setRenderHint(QPainter::Antialiasing);

                // 1. Paint the black vinyl record disk partially sticking out from the right
                int diskX = caseW - qRound((hovered ? 25.0 : 60.0) * scaleFactor);
                int diskY = caseH / 2;
                int diskR = qRound(95.0 * scaleFactor);

                p.save();
                p.translate(diskX, diskY);
                if (hovered) {
                    p.rotate(25.0); // Spin 25 degrees on hover!
                }

                p.setBrush(QColor("#0d0d0d")); // Charcoal black vinyl body
                p.setPen(Qt::NoPen);
                p.drawEllipse(-diskR, -diskR, diskR * 2, diskR * 2);

                // Groove lines
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(255, 255, 255, 12), 1));
                int startR = qRound(35.0 * scaleFactor);
                int stepR = qRound(10.0 * scaleFactor);
                for (int r = startR; r < diskR - 5; r += stepR) {
                    p.drawEllipse(-r, -r, r * 2, r * 2);
                }

                // Sticker label in the middle of vinyl disk
                int labelR = qRound(32.0 * scaleFactor);
                p.setBrush(QColor("#b4befe")); // Beautiful pastel purple label background
                p.setPen(Qt::NoPen);
                p.drawEllipse(-labelR, -labelR, labelR * 2, labelR * 2);

                // Draw micro sticker text
                p.setPen(QColor("#11111b"));
                QFont microFont = p.font();
                microFont.setPointSize(qMax(6, qRound(8.0 * scaleFactor)));
                microFont.setBold(true);
                p.setFont(microFont);
                p.drawText(QRect(-labelR, -labelR, labelR * 2, labelR * 2), Qt::AlignCenter, "LP");

                // Vinyl center spindle hole
                p.setBrush(QColor("#1e1e2e"));
                p.setPen(Qt::NoPen);
                int holeR = qRound(5.0 * scaleFactor);
                p.drawEllipse(-holeR, -holeR, holeR * 2, holeR * 2);

                p.restore();

                // 2. Paint the cardboard outer sleeve on the left covering the disk
                int sleeveW = caseW - qRound(99.0 * scaleFactor); // 300 - 99 = 201 wide cardboard sleeve
                int sleeveH = caseH;
                p.setBrush(QColor("#181825"));
                p.setPen(QPen(QColor("#313244"), qMax(1, qRound(1.5 * scaleFactor))));
                p.drawRoundedRect(0, 0, sleeveW, sleeveH, qRound(3.0 * scaleFactor), qRound(3.0 * scaleFactor));

                int coverX = qRound(4 * scaleFactor);
                int coverY = qRound(4 * scaleFactor);
                int coverW = sleeveW - qRound(8 * scaleFactor);
                int coverH = caseH - qRound(8 * scaleFactor);
                p.drawImage(QRect(coverX, coverY, coverW, coverH), cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

                // Draw inner shadow/edge highlight on the cardboard sleeve
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(255, 255, 255, 45), qMax(1, qRound(1.0 * scaleFactor))));
                p.drawRoundedRect(qRound(2 * scaleFactor), qRound(2 * scaleFactor), sleeveW - qRound(4 * scaleFactor), caseH - qRound(4 * scaleFactor), qRound(2 * scaleFactor), qRound(2 * scaleFactor));

                p.end();
                return img;
            };

            caseImage = renderVinyl(false);
            hoverImage = renderVinyl(true);

            QString path = m_path;
            QMetaObject::invokeMethod(model.data(), [model, path, artPath, casingInt, caseImage, hoverImage]() {
                if (model) {
                    model->onCasingRendered(path, artPath, casingInt, caseImage, hoverImage);
                }
            }, Qt::QueuedConnection);
            return;
        } else {
            // High-fidelity 3D CD Jewel Case using template PNG with procedural fallback
            QSettings settings("Amifiles", "Amifiles");
            QString casingType = settings.value("music_showcase/casing_type", "cd").toString();
            QString templatePath = "/home/dave/.gemini/antigravity/cd_case_overlay.png";
            if (casingType == "cd_black") {
                templatePath = "/home/dave/.gemini/antigravity/cd_case_overlay_black.png";
            } else if (casingType == "cd_black_premium") {
                templatePath = "/home/dave/.gemini/antigravity/cd_case_overlay_black_premium.png";
            }
            QImage templateImage(templatePath);
            if (templateImage.isNull() && (casingType == "cd_black" || casingType == "cd_black_premium")) {
                templateImage.load("/home/dave/.gemini/antigravity/cd_case_overlay.png");
            }
            if (!templateImage.isNull()) {
                // Perform pixel-perfect blending overlay using user's template PNG
                QImage scaledTemplate = templateImage.scaled(caseW, caseH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                
                double scale = caseW / 1024.0;
                int left, top, right, bottom;
                if (casingType == "cd_black") {
                    left = qRound(140.0 * scale);
                    top = qRound(100.0 * scale);
                    right = qRound(975.0 * scale);
                    bottom = qRound(915.0 * scale);
                } else {
                    // cd and cd_black_premium both use the clear case layout
                    left = qRound(181.0 * scale);
                    top = qRound(148.0 * scale);
                    right = qRound(925.0 * scale);
                    bottom = qRound(880.0 * scale);
                }
                
                int coverW = right - left;
                int coverH = bottom - top;

                QImage caseImage = scaledTemplate;
                QImage scaledCover = cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

                if (caseImage.format() != QImage::Format_ARGB32_Premultiplied && caseImage.format() != QImage::Format_ARGB32) {
                    caseImage = caseImage.convertToFormat(QImage::Format_ARGB32);
                }
                if (scaledCover.format() != QImage::Format_ARGB32_Premultiplied && scaledCover.format() != QImage::Format_ARGB32) {
                    scaledCover = scaledCover.convertToFormat(QImage::Format_ARGB32);
                }

                double neutral = (casingType == "cd_black") ? 246.0 : 219.0;

                for (int y = top; y < bottom; ++y) {
                    if (y < 0 || y >= caseImage.height()) continue;
                    QRgb* caseLine = reinterpret_cast<QRgb*>(caseImage.scanLine(y));
                    int cy = y - top;
                    if (cy < 0 || cy >= scaledCover.height()) continue;
                    const QRgb* coverLine = reinterpret_cast<const QRgb*>(scaledCover.constScanLine(cy));

                    for (int x = left; x < right; ++x) {
                        if (x < 0 || x >= caseImage.width()) continue;
                        int cx = x - left;
                        if (cx < 0 || cx >= scaledCover.width()) continue;

                        QRgb tc = caseLine[x];
                        QRgb cc = coverLine[cx];

                        int ta = qAlpha(tc);
                        int tr = qRed(tc);
                        int tg = qGreen(tc);
                        int tb = qBlue(tc);

                        int cr = qRed(cc);
                        int cg = qGreen(cc);
                        int cb = qBlue(cc);

                        int r = qMin(255, qMax(0, int(cr * (tr / neutral))));
                        int g = qMin(255, qMax(0, int(cg * (tg / neutral))));
                        int b = qMin(255, qMax(0, int(cb * (tb / neutral))));

                        caseLine[x] = qRgba(r, g, b, ta);
                    }
                }
                painter.drawImage(0, 0, caseImage);
            } else {
                // Procedural Fallback
                // 1. Drop Shadow (shifted right and down for realistic floating depth)
                QPolygonF shadowQuad;
                double shadowOff = 4.0 * s;
                shadowQuad << QPointF(1.5 * s + shadowOff, 3.5 * s + shadowOff)
                           << QPointF(43.5 * s + shadowOff, 7.5 * s + shadowOff)
                           << QPointF(43.5 * s + shadowOff, 40.5 * s + shadowOff)
                           << QPointF(1.5 * s + shadowOff, 44.5 * s + shadowOff);
                painter.setBrush(QColor(0, 0, 0, 95));
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(shadowQuad);

                // 2. Left side thickness edge (for 3D plastic depth)
                QPolygonF leftEdge;
                leftEdge << QPointF(0.5 * s, 4.5 * s)
                         << QPointF(1.5 * s, 3.5 * s)
                         << QPointF(1.5 * s, 44.5 * s)
                         << QPointF(0.5 * s, 45.5 * s);
                QLinearGradient leftEdgeGrad(0.5 * s, 0, 1.5 * s, 0);
                leftEdgeGrad.setColorAt(0.0, QColor(255, 255, 255, 75));
                leftEdgeGrad.setColorAt(0.4, QColor(255, 255, 255, 20));
                leftEdgeGrad.setColorAt(1.0, QColor(255, 255, 255, 85));
                painter.setBrush(leftEdgeGrad);
                painter.setPen(QPen(QColor(255, 255, 255, 95), 1.0));
                painter.drawPolygon(leftEdge);

                // 3. Bottom edge thickness (for 3D base depth)
                QPolygonF bottomEdge;
                bottomEdge << QPointF(1.5 * s, 44.5 * s)
                           << QPointF(43.5 * s, 40.5 * s)
                           << QPointF(42.5 * s, 41.5 * s)
                           << QPointF(0.5 * s, 45.5 * s);
                QLinearGradient bottomEdgeGrad(0, 40.5 * s, 0, 45.5 * s);
                bottomEdgeGrad.setColorAt(0.0, QColor(255, 255, 255, 15));
                bottomEdgeGrad.setColorAt(1.0, QColor(0, 0, 0, 80));
                painter.setBrush(bottomEdgeGrad);
                painter.setPen(QPen(QColor(255, 255, 255, 75), 1.0));
                painter.drawPolygon(bottomEdge);

                // 4. Ribbed Spine Hinge (Classic dark textured plastic with perspective ribs)
                QPolygonF spineQuad;
                spineQuad << QPointF(3.0 * s, 4.5 * s)
                          << QPointF(10.0 * s, 5.5 * s)
                          << QPointF(10.0 * s, 42.5 * s)
                          << QPointF(3.0 * s, 43.5 * s);
                QLinearGradient spineBgGrad(3.0 * s, 0, 10.0 * s, 0);
                spineBgGrad.setColorAt(0.0, QColor("#111112"));
                spineBgGrad.setColorAt(0.5, QColor("#2d2d31"));
                spineBgGrad.setColorAt(1.0, QColor("#0d0d0e"));
                painter.setBrush(spineBgGrad);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(spineQuad);

                int numRibs = 15;
                for (int i = 0; i <= numRibs; ++i) {
                    double t = (double)i / numRibs;
                    double rx_top = 3.0 + t * 7.0;
                    double ry_top = 4.5 + t * 1.0;
                    double rx_bot = 3.0 + t * 7.0;
                    double ry_bot = 43.5 - t * 1.0;

                    painter.setPen(QPen(QColor(0, 0, 0, 150), 1.0));
                    painter.drawLine(QPointF(rx_top * s, ry_top * s), QPointF(rx_bot * s, ry_bot * s));
                    painter.setPen(QPen(QColor(255, 255, 255, 35), 1.0));
                    painter.drawLine(QPointF((rx_top + 0.15) * s, ry_top * s), QPointF((rx_bot + 0.15) * s, ry_bot * s));
                }

                // 5. Warp and draw the album art insert booklet using perspective transform
                QPolygonF srcQuad;
                srcQuad << QPointF(0, 0)
                        << QPointF(cover.width(), 0)
                        << QPointF(cover.width(), cover.height())
                        << QPointF(0, cover.height());

                QPolygonF dstQuad;
                dstQuad << QPointF(10.0 * s, 5.5 * s)
                        << QPointF(41.0 * s, 8.5 * s)
                        << QPointF(41.0 * s, 39.5 * s)
                        << QPointF(10.0 * s, 42.5 * s);

                QTransform transform;
                if (QTransform::quadToQuad(srcQuad, dstQuad, transform)) {
                    painter.save();
                    painter.setTransform(transform, true);
                    painter.drawImage(0, 0, cover);
                    painter.restore();
                }

                // Cover booklet border highlight
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
                painter.drawPolygon(dstQuad);

                // 6. Draw outer transparent acrylic cover highlights
                QPolygonF frontCover;
                frontCover << QPointF(1.5 * s, 3.5 * s)
                           << QPointF(43.5 * s, 7.5 * s)
                           << QPointF(43.5 * s, 40.5 * s)
                           << QPointF(1.5 * s, 44.5 * s);
                painter.setPen(QPen(QColor(255, 255, 255, 90), 1.5));
                painter.drawPolygon(frontCover);

                // Double-layer inner plastic highlight
                QPolygonF frontCoverInner;
                frontCoverInner << QPointF(2.5 * s, 4.3 * s)
                                << QPointF(42.5 * s, 8.1 * s)
                                << QPointF(42.5 * s, 39.7 * s)
                                << QPointF(2.5 * s, 43.7 * s);
                painter.setPen(QPen(QColor(255, 255, 255, 35), 1.0));
                painter.drawPolygon(frontCoverInner);

                // 7. Glassy reflections and sheens
                // Wide glossy band
                QPolygonF gloss1;
                gloss1 << QPointF(10.0 * s, 5.5 * s)
                       << QPointF(25.0 * s, 6.9 * s)
                       << QPointF(10.0 * s, 21.0 * s);
                QLinearGradient gloss1Grad(10.0 * s, 5.5 * s, 20.0 * s, 15.0 * s);
                gloss1Grad.setColorAt(0.0, QColor(255, 255, 255, 80));
                gloss1Grad.setColorAt(0.5, QColor(255, 255, 255, 30));
                gloss1Grad.setColorAt(1.0, QColor(255, 255, 255, 0));
                painter.setBrush(gloss1Grad);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(gloss1);

                // Sharp diagonal glare stripe
                QPolygonF gloss2;
                gloss2 << QPointF(28.0 * s, 7.2 * s)
                       << QPointF(34.0 * s, 7.8 * s)
                       << QPointF(10.0 * s, 31.8 * s)
                       << QPointF(10.0 * s, 25.8 * s);
                QLinearGradient gloss2Grad(28.0 * s, 7.2 * s, 15.0 * s, 20.0 * s);
                gloss2Grad.setColorAt(0.0, QColor(255, 255, 255, 90));
                gloss2Grad.setColorAt(0.2, QColor(255, 255, 255, 100));
                gloss2Grad.setColorAt(0.35, QColor(255, 255, 255, 0));
                gloss2Grad.setColorAt(0.75, QColor(255, 255, 255, 0));
                gloss2Grad.setColorAt(0.85, QColor(255, 255, 255, 20));
                gloss2Grad.setColorAt(1.0, QColor(255, 255, 255, 0));
                painter.setBrush(gloss2Grad);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(gloss2);

                // Soft glare top right
                QPolygonF gloss3;
                gloss3 << QPointF(38.0 * s, 8.2 * s)
                       << QPointF(43.5 * s, 7.5 * s)
                       << QPointF(43.5 * s, 25.0 * s)
                       << QPointF(38.0 * s, 25.0 * s);
                QLinearGradient gloss3Grad(43.5 * s, 7.5 * s, 38.0 * s, 20.0 * s);
                gloss3Grad.setColorAt(0.0, QColor(255, 255, 255, 45));
                gloss3Grad.setColorAt(1.0, QColor(255, 255, 255, 0));
                painter.setBrush(gloss3Grad);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(gloss3);
            }
        }
    }
    else if (casingInt == 1) { // CasingDVD
        // 1. Draw outer case body with realistic rounded corners
        QLinearGradient caseGrad(0, 0, 0, caseH);
        caseGrad.setColorAt(0.0, QColor("#2b2b2b"));
        caseGrad.setColorAt(0.5, QColor("#1e1e1e"));
        caseGrad.setColorAt(1.0, QColor("#121212"));
        painter.setBrush(caseGrad);
        
        QLinearGradient borderGrad(0, 0, 0, caseH);
        borderGrad.setColorAt(0.0, QColor("#444444"));
        borderGrad.setColorAt(1.0, QColor("#111111"));
        painter.setPen(QPen(borderGrad, qMax(1.0, 1.0 * scaleFactor)));
        painter.drawRoundedRect(QRectF(0.5 * scaleFactor, 0.5 * scaleFactor, caseW - 1.0 * scaleFactor, caseH - 1.0 * scaleFactor), 10.0 * scaleFactor, 10.0 * scaleFactor);

        // 2. Spine hinge lines on the left side
        painter.setPen(QPen(QColor(255, 255, 255, 25), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(QPointF(5.5 * scaleFactor, 6.0 * scaleFactor), QPointF(5.5 * scaleFactor, caseH - 7.0 * scaleFactor));
        painter.setPen(QPen(QColor(0, 0, 0, 90), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(QPointF(6.5 * scaleFactor, 6.0 * scaleFactor), QPointF(6.5 * scaleFactor, caseH - 7.0 * scaleFactor));

        // 3. Sleeve pocket layout
        int coverX = qRound(10.0 * scaleFactor);
        int coverY = qRound(6.0 * scaleFactor);
        int coverW = caseW - qRound(16.0 * scaleFactor);
        int coverH = caseH - qRound(12.0 * scaleFactor);
        int headerH = qRound(24.0 * scaleFactor);

        // Clip everything inside the sleeve pocket
        painter.save();
        QPainterPath sleevePath;
        sleevePath.addRoundedRect(QRectF(coverX, coverY, coverW, coverH), 3.0 * scaleFactor, 3.0 * scaleFactor);
        painter.setClipPath(sleevePath);

        // A. Draw the Opaque Header (Opaque Charcoal)
        QLinearGradient headerGrad(coverX, coverY, coverX, coverY + headerH);
        headerGrad.setColorAt(0.0, QColor("#222223"));
        headerGrad.setColorAt(1.0, QColor("#141415"));
        painter.setBrush(headerGrad);
        painter.setPen(Qt::NoPen);
        painter.drawRect(coverX, coverY, coverW, headerH);

        // B. Draw the DVD Video logo inside the header
        int logoW = qRound(27.0 * scaleFactor);
        int logoH = qRound(12.5 * scaleFactor);
        int logoX = coverX + (coverW - logoW) / 2;
        int logoY = coverY + (headerH - logoH) / 2;
        
        static QImage dvdLogoImg;
        if (dvdLogoImg.isNull()) {
            QByteArray base64Data = QByteArray::fromBase64(
                "iVBORw0KGgoAAAANSUhEUgAAAHgAAAA4CAYAAAA2PDy+AAASNklEQVR4nNVdebAeRRH/zffeywEJAQlH"
                "goABhHAGEi4LUBSVUg4VQcDSKhEUCxH9QzygwNKSqwTBW5GoUFCIiliCSCGXhSCXIASMyCVHEiAJJJCE"
                "hJf3tdWbX7/qN293dvb7vkDsqq192W+np6d7prunu2cT0BBEpBVCaIvIsQA+D2AlgD6sezAEYByAS0MI"
                "Pze6O0UmIn0hhCER+QiAU4h/bY1bAAwCeBXASwAWAHgMwJMA/hVCWOLpAtAOIWibURAa9ywSFJmIXAHg"
                "41j34QEAe3YpXJvUbwdwO4DN8ObAMgDzAdwP4BoAN4cQFvsJ2JWAnXD7ybiddPYAaGHdhTaAA0IId3Wy"
                "inXMHN94ADcA2A/A6jdwzMKrVdLnEwAuA/CDEMLLKpcQwurOe1ozWL1vJyJLVdIiojN7XYVB3s8i3f0d"
                "jLlQwyLyowjnmwHKazUTq3kZzBWRD3kZGTSdhfb+rgAm0A4VeDu82sShs65jFZpB7wdEZLzO7pgBKeCK"
                "UIZ+EsBJpLVqkrS74EMVT/TubatpE510he3le9MB/F5EvkwN22fjbCpgY84+rqPQxWU4+vl3u8eCbpFB"
                "M3j5MSSB6lwnxEwA368xRfZbN7wo44nxNxa0H1+/49l3ROQ02uJWx04WgN0AbM6Oq3DUCWsAwMYAtgKw"
                "M4CZvHuG9QKGyKjzQwinVjkjsXDJ0PUA3MnxVnnN9vyHAH7HNlX4hSsuVAhM8UwEMAXADgD2IF/Wz+CL"
                "OA1zTAjhKjMv6wSIyAYioqr0z7QrQz2y70O8PywiE3SC1qlpZ3cvrbG7ZgevW4t8mSEiF4jIa84Op8aq"
                "vz8nIpsUTnEHHvQmAA5PrE7h6rwPwINO9ZaB9R+896e2BMB57p3GmiaCNmf3wSGEW1PeptvvfgnAhVxx"
                "ZXbXVtPj6qUD2Isr7/WSVSbEsRDAHxMrGH7MEU/eoXt6AG+vWclG7+khhLOzuGMD5/0TmSvnON8uA3+g"
                "c2D9nBqtkm5gkPfzibuUOfZcRA4QkVVcEaYBPLRJ10plvIiMFZHlGXTcmOq/gict8/5FZLqIvOi86dQq"
                "fkBExnUi4AuJZJVz1+0a5H2FiGxvRGZ3MlLQOrDbHNHdQJv3x9SbLqOL/WnfU0XkPzX92oT5ItseKCKv"
                "Rzzwl/Hqq56XDfkywPsXaia+jfUVEdmniSNjanaPyFX3lz1/GsAz6AAs5MaAxMU2vppmqhZTG/xAHNsC"
                "eKcLXiAStt5/SjU47IlWqMBfhRC+x9WozuGAU8V9Fdc/GjPE9cu+LgfwAvGV8UXH0KazNq3VYMugs0I9"
                "52kO0ahXeX8IwKripYoYaX2XBdPVjq9IDMaePQfgjuhZDObxf7iEJrW7ypTTARxGIVZ5zP0MFX7RRcZ2"
                "RzUIcb3MyJM9awRKs/alESsA9/Jxyg9S2Cx3BZswt6MjYaGzKsQP2Ia7wRhGdrhGCMsYbM+BX0e0xmD0"
                "HiQiG9GRKswB97uHAjgjIdw2cWvs91MhhFf0Ie3cTlEfcTuF/zBpUDRDB+Bs9/waPPZ8oKmAd6Eqqtrn"
                "mcc8J2q3tkFX1d8B/Dsxs1t8rup3H2oIi1RtDeBnHJsFLGIwz/ULIYQ5ahO5ejdhJKlOq2kWaCUnVEcC"
                "bgBGx+JcAZtAZ0YIPNiq1hU3l8+6jUrpBn+jxO/GqKUAHgFwY6bqOpJMVodkLIBfApiasLummr8bQrjS"
                "UnT8TZ3Jt7gVHoM9+2f078bAjFZw2ayUtlKan88SMNVtoINVhdgG/Kw6Wdw3dypgwz+NQq5inuF/gnvG"
                "P7jIkpS8b+N9n4i8hREt3W+/u8bu6vOb1EZHwgX3vwpVq9JoMQF3xBPnCA44jVFlJvXdRZo/bjVArDN8"
                "+4xZ+CAZ1wv1/C4jo+a9e3j/G4CHE4GEQIFpeFQjRO9TZykhXHOQ1OadoCpWn0Uqds8EXfbeItKVM5Yq"
                "sAjcTtwNmCCr+vyvTvycFWzvzKDrXYXY4D4jqOEA1lC3ZhCqisYAOLQGVz+F85fipRC0CuI3/K1d0cZw"
                "fQPAbCfEuA/L7ijO40MIqpUKb5vaaYh7aoufp5j9cAihcBa7sL9FJBHAhzjuVLxb4RbVajkCNsJ3cysg"
                "9V43ez0Fc0I+SOZVheUsPaf9adTG3rmO3ndfBf6W0w5bJkKhltQ/M4Rwg6UOIxzbcleBGhPyzyYRrKrk"
                "h8brAXwioqFsfEr71amXyojcI0UDcc1jkMOedbp6x3LLktpmmSaZbekxtp9DlR0Sk9HGldozq627JoRw"
                "Lu2ux2XCVFs4KeEjGGj1C7rIkNl++0vcqqacQaXjVp1UtVFEV8ExUUQeTITvLGx2kwtpdhKitHDcBTXh"
                "OH3eZiWD0lbYJ9f352pCjSmwNop7Y4sHR7RaP99OZJssZKgx6r26CFFaHHp/ZpRSWTb77eCs/lzwfTrj"
                "y1WIbYAXeaIaDMJWn/59ciTEMjDBHxkx3Oh9q4i8HDE6ByyI/6qI7O1xltCsgr/OMbaM2QqPi8ikpiqa"
                "+E24u4rIMzXjMRlcld2XY9wREWOrGP4p3y5jAMOlJToYEfmWw1c1EA3qK/yqZHIMrzat+pSRA88Be/ez"
                "RlMZ3bxPqGH6iFxxQ54Mvysi7xWReZn8/68GbciTRgI+O9FB2zFnFgkcMEJLrlGdM+V2M/GkVJAJ4A4W"
                "CIxK3rtZ/zGHLwcM90/dhBtlZtwEmlWD23h1Nsc9piFPNGF/HlOSqXHYYtD3DsqdTPFgboyI9mAda6w1"
                "GzTQoJWAInKlY27OylWPeWqdGlK1KCK6tclR0zauu0Vk/VTVh5tAn8lgvMJ7GvBEJ9W+tO2mHXL60HTk"
                "0WXCrbSVFonSwDzjtwopx2mJiLzfFbp5sLriyQDexi3Xrgw4GKRqnrRfdcBuBnBsCGFhqraKWZ6lIqLV"
                "EyfTy62a1bYNexHAcSGE5Zn101bEV7aNs7217qFVZb6X9Mc4dbeg9vmtDGDMoJdc5Kydt1yG38KnS0n3"
                "NTn1ZmXqeV/nYPW6BtpqfKt+8/bzx7q6TICZtL+L+HNU6RG+bQVe89Z1pf01at9LGKzgtVWSGPxdRHbp"
                "yEN3qui4BgNZ3eBKlcL4vnQ/d7ijKyu8yms9Ebk/Qb9NoHOMSantXeSlz8u08UMNedLOmOwvicgZOr46"
                "4fY3CHDkBC6a7vOs0NsfzTAcjwL4iWZ6NPdq0ZycBIblokMIqnmurQjSmIrT4yhn1B3iMtS8b8MIVk55"
                "b6tDnljwxCplWsxFa2nuRSGEIjVap5b7M1JTuzgh5On3vEHEAgVDjLdwEL9Xe5gziApo866x6a8BGBP9"
                "ZqVFn2bCv6hayaAb9B+sxrnXtccWF285Wh8kT64OITzqVm27ji/9qRJZFre/o8LQ9wIWMevxCE/t3aRB"
                "fUdH1iCqQNZM0LnEfVAU4ltF52RBBxPoIOLxk6ZX8Brr2fS46F10LLVCZpXjieTSW7WCLd22IfOg3aT/"
                "2iyK09X4MmuD51GwWsIyL4Sgg0KvBFsMgKcgw5rV+VsKxXufX6+rkU6sYM15X19RA52LRz3slfSCF5EX"
                "T7O+bIEdCx1u0FCw6xS4DX/tqYOGeFu861ZlkXNgrshxqt5MsHpoH6nrBGobdpriyuyzyLl2U6OUMfgW"
                "c7dqi49iZuf91CYKo5y2OnrYZ1ibfOnmwHoV0jcMojrkFB2pUtlGyXNZ8+mFy3naf24DGqvoq6PZ6rvX"
                "doHdGyvgCsb4Gd8zO0Lt0s9rwBWY23FUuPuWzN9e76JK5gkP8Rq0s7ldnZQfTWMrqhKxv7G2J0FXAnaq"
                "yq52rmrhJl0Pka/PazyrEzdiaZCG8DbgNZHvTOB7GuIbx2sMBeZLXlsV41vJ9jGjTdgq4NV0nlZG1wpu"
                "417l9QovdZKW0FF6zb23LISwLJMX/mxxQU+vhN7odGFEhB0vKXtXBaACnMpLY61bsIZ4KmPSXogTXfz1"
                "/xled8JfyvsSFu29SE/5Of5br6XxDqLE9wnd+Ct152SHV0OZWmWN0DQmDd7GpMTW/HsKD3jnOmkWvbFS"
                "nFQRW52TYzgkw857vKm/63D4BVAHwgmwgHte3R49xaMtxd+aUEmo+3ausKvynUXVYPR8EgW5GwvgZ7Do"
                "TFXqpJrBGCE+OhMLcm07fG2X5VlbYJGtEPU7GEWoUmO1eMHTLLW9n9czIQTVAsNgMeiUTxOqBMvVuz2L"
                "wg9geE6dlKJuKvG5htguGwwywKHPNqX99DS8RJU2QDUeStTfAj7fPBFFWuACEIH3SQ0+g/A825tAbHL2"
                "k+6673S0GQhZRLOzFX0Hwz9C5UYLwGLPZfAEI353MJyr9eeDXp1X+j9RiYh+IukUns1dUZMxyvnMgqUZ"
                "l/Ng9Tj9ao3D1Wb91ExWPZzifvPZmrvZdlIiQ6S49uR7fRyPBk82E5FjROSWCKdvJxzv7r6948tW/DSC"
                "fz9u/0eeFfZHUzXIclqDGrH4U0llmbdBZtm0xGmW62v05HM1Rtsy5/pChMwONXfzzQxLdV3CvsawMsPg"
                "Uj4fR8LFDcruxfFQlgPd62iLmbOLK1Q7VkQOE5EtXVur1RqqqICc7tofyXo0rTw5UUQWR+97POc7Xk4R"
                "ET2HvKvj87uZ5us0r24pw3jMy0Tkas3be3nGQj4iEmy3Ao3BBrXMBi0ix/M3JXg/Pju65PME9vedXI3j"
                "ReQ+1zbuZw++d5F7roIpPruoFSoi8mSE2wt4N7a/OGNcXruYxvisiMx3v/9WRDblb7F26gbiHLHWZJ0e"
                "L1yNc6p91RW0KZ0EO6Ve5ww0gWKPTDuoHzAFK+/n89jnnWTQiYlzRRYQqE3p8b3l7PM17q/PEpHJPEBt"
                "pxDLcNke9FZ+D/Jy8uemkjZm8y5nOHQHflNrigugaGmvrv4Wx7wocTiuCViQxydQtJbr0+734uE+dAJ0"
                "M5//0Y7OCFJijhIRPYap38GYTYdEQYvTDqypn2raX8t9WkEdLbWHi12fZVA4mXpMFIBeBVD1z4kcIrN5"
                "z7rc+Rj3mQf7Yt1M5tdf5T54csYZr1wwx2wV73ok5xdGpxKhhdIq9R3dR7p6uXo9Iau5mk4A8BUA57rU"
                "3slupfcC2i78aOd1FrMvDaxUgalarVI8xIUxtyjZ2pnnbPjMywf7W81FoznnQOFv6PjRy3FqcEkjZ4Wf"
                "MwLoYF0f6fgyg94rW6ypuy1c/7No/8rs/rANdo5YygYX38uIbLDCd1z68LaovbfB5iPk2GBrf6VzHmdH"
                "72gxelEyJCIf7NFXg2L7q/CQq4senjxFvjGE8ISIHMLjmrqS9meo0c8SH6zodPbZgTCNcOn5IT25p3g/"
                "5z4BWLXPtFMLdf3be/eyXEf31reHEC7j4FV77JfYD1v72+gzrGJ/Y3nicZJbyXYYXL1tLTFSh+pE2vjd"
                "aW/1+VN0tM7pwPbGe2XjgWkl/eDNbPoBRe3aqP1wSVW97klPr9kL2wq3VZfrcdv781lkPoVngVIHqhRu"
                "d/Td5eiI392mlEtS9HOmOyng+7O/1cv3tdoxjrIDeNZWx3ASzzXH7fYTkXtK2qb2wFUaVH//h/oxeoDd"
                "DuxVFTD4SJbZ3uFMBhtrXFn3WAcwRKl7yk0Tsy1e7dZPnOjXf2tlxQb8dFHqxLo+X8gKSMV5cIWjov++"
                "lgH+fj7rY+RrR0d3VV+6Iv7EVR87egMsFPArOKZR4d88vjqfGkBDunvTFlulpF+VxpsqzbWUTqE6eHfw"
                "KwaPacUoMqDqeEahAuKcKJ9r/Hkbeow7k3HTOPCUF27C9/82Ifiar5imXjp7Qz10IKUiXFsWyvVbmSp4"
                "nU6SxqAfZVhyDv+fhsdL6tbGc+FZXdfznJT6XexnrHAyJ5s0vPoSR0U2ZBZpW9712oorZnOmCXWldgJe"
                "K/iEQVWVxVAXmR4pORgex4mbZI1iWEG7/AKveRTgs8wmPRknFIaJGBk6FU6kSxjDn0CtNZFFjN+0StHk"
                "Od44sBAL3NQ5/xeQJazf9USNoWDX5/ZoCoW+GQU/mc8n8x1L4o91d6vUyIVua8j6Mt4ZovO1MrprcGUx"
                "ma73F7myXuB9oSsGqFSx0acWLQ88PHEpPD1wpupan+sC+yjlYt8KW5Oz75IZnqBY+EMNa6bGcwZOcJdV"
                "eozjvy0r5Ks67N7Hv8eQhoGSFedtn+2RrWxHhWSCU3WpKtGEp4KzSg4VzIjKDVfpsTx3zE6II+jKae/+"
                "Fxj1i6ytpm11vAtDCEVYV3H9D4GT69GR43ZsAAAAAElFTkSuQmCC"
            );
            dvdLogoImg.loadFromData(base64Data, "PNG");
        }
        if (!dvdLogoImg.isNull()) {
            painter.drawImage(QRect(logoX, logoY, logoW, logoH), dvdLogoImg);
        }

        // Header bottom divider line
        painter.setPen(QPen(QColor(255, 255, 255, 30), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(coverX, coverY + headerH, coverX + coverW, coverY + headerH);

        // C. Draw the cover image resized to fit in the remaining space below the header
        int artY = coverY + headerH;
        int artH = coverH - headerH;
        painter.drawImage(QRect(coverX, artY, coverW, artH), cover.scaled(coverW, artH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        // D. Draw inner pocket border reflection & shadow
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 35), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawRoundedRect(QRectF(coverX, coverY, coverW, coverH), 3.0 * scaleFactor, 3.0 * scaleFactor);

        painter.setPen(QPen(QColor(0, 0, 0, 100), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(coverX, coverY, coverX + coverW, coverY);
        painter.drawLine(coverX, coverY, coverX, coverY + coverH);

        painter.restore();

        // 10. Draw diagonal glossy overlay on top of the entire case (clipped to outer case shape)
        painter.save();
        QPainterPath outerPath;
        outerPath.addRoundedRect(QRectF(0.5 * scaleFactor, 0.5 * scaleFactor, caseW - 1.0 * scaleFactor, caseH - 1.0 * scaleFactor), 10.0 * scaleFactor, 10.0 * scaleFactor);
        painter.setClipPath(outerPath);

        QLinearGradient gloss(caseW - qRound(5.0 * scaleFactor), qRound(5.0 * scaleFactor), qRound(25.0 * scaleFactor), caseH - qRound(25.0 * scaleFactor));
        gloss.setColorAt(0.0, QColor(255, 255, 255, 110)); // Bright reflection
        gloss.setColorAt(0.20, QColor(255, 255, 255, 120));
        gloss.setColorAt(0.26, QColor(255, 255, 255, 0));  // Sharp highlight cutoff
        gloss.setColorAt(0.60, QColor(255, 255, 255, 0));
        gloss.setColorAt(0.70, QColor(255, 255, 255, 15));  // Soft secondary reflection
        gloss.setColorAt(0.80, QColor(255, 255, 255, 0));
        
        painter.setBrush(gloss);
        painter.setPen(Qt::NoPen);
        painter.drawRect(0, 0, caseW, caseH);
        painter.restore();
    }
    else if (casingInt == 2) { // CasingBluRay
        // 1. Draw outer case body with realistic rounded corners and blue plastic look
        QLinearGradient caseGrad(0, 0, 0, caseH);
        caseGrad.setColorAt(0.0, QColor("#1e3a8a"));
        caseGrad.setColorAt(0.5, QColor("#172554"));
        caseGrad.setColorAt(1.0, QColor("#0f172a"));
        painter.setBrush(caseGrad);
        
        QLinearGradient borderGrad(0, 0, 0, caseH);
        borderGrad.setColorAt(0.0, QColor("#3b82f6"));
        borderGrad.setColorAt(1.0, QColor("#1e293b"));
        painter.setPen(QPen(borderGrad, qMax(1.0, 1.0 * scaleFactor)));
        painter.drawRoundedRect(QRectF(0.5 * scaleFactor, 0.5 * scaleFactor, caseW - 1.0 * scaleFactor, caseH - 1.0 * scaleFactor), 10.0 * scaleFactor, 10.0 * scaleFactor);

        // 2. Spine hinge lines on the left side
        painter.setPen(QPen(QColor(96, 165, 250, 90), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(QPointF(5.5 * scaleFactor, 6.0 * scaleFactor), QPointF(5.5 * scaleFactor, caseH - 7.0 * scaleFactor));
        painter.setPen(QPen(QColor(0, 0, 0, 90), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(QPointF(6.5 * scaleFactor, 6.0 * scaleFactor), QPointF(6.5 * scaleFactor, caseH - 7.0 * scaleFactor));

        // 3. Sleeve pocket layout
        int coverX = qRound(10.0 * scaleFactor);
        int coverY = qRound(6.0 * scaleFactor);
        int coverW = caseW - qRound(16.0 * scaleFactor);
        int coverH = caseH - qRound(12.0 * scaleFactor);
        int headerH = qRound(24.0 * scaleFactor);

        // Clip everything inside the sleeve pocket
        painter.save();
        QPainterPath sleevePath;
        sleevePath.addRoundedRect(QRectF(coverX, coverY, coverW, coverH), 3.0 * scaleFactor, 3.0 * scaleFactor);
        painter.setClipPath(sleevePath);

        // A. Draw the Opaque Header (Opaque Blue)
        QLinearGradient headerGrad(coverX, coverY, coverX, coverY + headerH);
        headerGrad.setColorAt(0.0, QColor("#0284c7"));
        headerGrad.setColorAt(1.0, QColor("#075985"));
        painter.setBrush(headerGrad);
        painter.setPen(Qt::NoPen);
        painter.drawRect(coverX, coverY, coverW, headerH);

        // B. Draw the Blu-ray logo inside the header
        int centerX = coverX + coverW / 2;
        
        // Blu-ray Text
        QFont brFont("Arial");
        brFont.setPixelSize(qMax(6, qRound(9.0 * scaleFactor)));
        brFont.setBold(true);
        brFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(1.0 * scaleFactor));
        painter.setFont(brFont);
        painter.setPen(QColor("#ffffff"));
        painter.drawText(QRect(coverX, coverY + qRound(3.0 * scaleFactor), coverW, qRound(9.0 * scaleFactor)), Qt::AlignCenter, "BLU-RAY");

        // VIDEO Text
        QFont videoFont("Arial");
        videoFont.setPixelSize(qMax(4, qRound(5.0 * scaleFactor)));
        videoFont.setBold(true);
        videoFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(2.0 * scaleFactor));
        painter.setFont(videoFont);
        painter.setPen(QColor("#bae6fd"));
        painter.drawText(QRect(coverX, coverY + qRound(13.0 * scaleFactor), coverW, qRound(8.0 * scaleFactor)), Qt::AlignCenter, "VIDEO");

        // Header bottom divider line
        painter.setPen(QPen(QColor(255, 255, 255, 40), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(coverX, coverY + headerH, coverX + coverW, coverY + headerH);

        // C. Draw the cover image resized to fit in the remaining space below the header
        int artY = coverY + headerH;
        int artH = coverH - headerH;
        painter.drawImage(QRect(coverX, artY, coverW, artH), cover.scaled(coverW, artH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        // D. Draw inner pocket border reflection & shadow
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 45), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawRoundedRect(QRectF(coverX, coverY, coverW, coverH), 3.0 * scaleFactor, 3.0 * scaleFactor);

        painter.setPen(QPen(QColor(0, 0, 0, 100), qMax(1.0, 1.0 * scaleFactor)));
        painter.drawLine(coverX, coverY, coverX + coverW, coverY);
        painter.drawLine(coverX, coverY, coverX, coverY + coverH);

        painter.restore();

        // 10. Draw diagonal glossy overlay on top of the entire case (clipped to outer case shape)
        painter.save();
        QPainterPath outerPath;
        outerPath.addRoundedRect(QRectF(0.5 * scaleFactor, 0.5 * scaleFactor, caseW - 1.0 * scaleFactor, caseH - 1.0 * scaleFactor), 10.0 * scaleFactor, 10.0 * scaleFactor);
        painter.setClipPath(outerPath);

        QLinearGradient gloss(caseW - qRound(5.0 * scaleFactor), qRound(5.0 * scaleFactor), qRound(25.0 * scaleFactor), caseH - qRound(25.0 * scaleFactor));
        gloss.setColorAt(0.0, QColor(255, 255, 255, 110)); // Bright reflection
        gloss.setColorAt(0.20, QColor(255, 255, 255, 120));
        gloss.setColorAt(0.26, QColor(255, 255, 255, 0));  // Sharp highlight cutoff
        gloss.setColorAt(0.60, QColor(255, 255, 255, 0));
        gloss.setColorAt(0.70, QColor(255, 255, 255, 15));  // Soft secondary reflection
        gloss.setColorAt(0.80, QColor(255, 255, 255, 0));
        
        painter.setBrush(gloss);
        painter.setPen(Qt::NoPen);
        painter.drawRect(0, 0, caseW, caseH);
        painter.restore();
    }
    
    painter.end();

    QString path = m_path;
    QMetaObject::invokeMethod(model.data(), [model, path, artPath, casingInt, caseImage]() {
        if (model) {
            model->onCasingRendered(path, artPath, casingInt, caseImage);
        }
    }, Qt::QueuedConnection);
}

void FileFilterProxyModel::onCasingRendered(const QString& path, const QString& artPath, int casingType, const QImage& image, const QImage& hoverImage) {
    m_pendingCasingChecks.remove(path);
    m_casingCache.insert(path, qMakePair(artPath, casingType));
    
    if (!artPath.isEmpty() && !image.isNull()) {
        QString cacheKey = artPath + "_" + QString::number(casingType);
        m_iconCache.insert(cacheKey, QIcon(QPixmap::fromImage(image)));
        if (!hoverImage.isNull()) {
            m_iconCache.insert(cacheKey + "_hover", QIcon(QPixmap::fromImage(hoverImage)));
        }
    }
    
    QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (fileModel) {
        QModelIndex srcIndex = fileModel->index(path);
        if (srcIndex.isValid()) {
            QModelIndex proxyIndex = mapFromSource(srcIndex);
            if (proxyIndex.isValid()) {
                emit dataChanged(proxyIndex, proxyIndex, {Qt::DecorationRole});
            }
        }
    }
}

void FilePanel::loadColumnWidths() {
    QSettings settings("Amifiles", "Amifiles");
    int count = m_treeView->model()->columnCount();
    for (int i = 0; i < count; ++i) {
        int defaultWidth = 100;
        if (i == 0) defaultWidth = 250;      // Name
        else if (i == 1) defaultWidth = 80;   // Size
        else if (i == 2) defaultWidth = 100;  // Type
        else if (i == 3) defaultWidth = 140;  // Date Modified

        int width = settings.value(QString("columns/width_%1").arg(i), defaultWidth).toInt();
        m_treeView->setColumnWidth(i, width);
    }
}

void FilePanel::saveColumnWidth(int logicalIndex, int width) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue(QString("columns/width_%1").arg(logicalIndex), width);
}

void FilePanel::autoSizeAllColumns() {
    for (int i = 0; i < m_treeView->model()->columnCount(); ++i) {
        m_treeView->resizeColumnToContents(i);
        saveColumnWidth(i, m_treeView->columnWidth(i));
    }
}

void FilePanel::loadSortSettings() {
    QSettings settings("Amifiles", "Amifiles");
    m_sortColumn = settings.value("file_panel/sort_column", 0).toInt();
    m_sortOrder = (Qt::SortOrder)settings.value("file_panel/sort_order", (int)Qt::AscendingOrder).toInt();
}

void FilePanel::setNavigationAndFilterVisible(bool visible) {
    if (m_navContainer) m_navContainer->setVisible(visible);
    if (m_categoryWidget) m_categoryWidget->setVisible(visible && m_categoryButtonsVisible);
    if (m_filterTextWidget) {
        m_filterTextWidget->setVisible(visible && m_filterTextBarVisible);
        if (m_filterEdit) m_filterEdit->setVisible(!m_isSearchModeActive);
        if (m_globalSearchEdit) m_globalSearchEdit->setVisible(m_isSearchModeActive);
    }
    if (m_statusWidget) m_statusWidget->setVisible(visible);
}

void FilePanel::queueRebuildTheaterGroups() {
    if (m_rebuildGroupsTimer) {
        m_rebuildGroupsTimer->start(50); // 50ms coalescing window
    }
}

void FilePanel::rebuildTheaterGroups() {
    // Dynamic Ambient Background Glow in Showcase Mode
    if (m_theaterScrollWidget && (viewModeIndex() >= 6 && viewModeIndex() <= 10)) {
        if (m_cachedBgPath != m_currentPath) {
            m_cachedBgPath = m_currentPath;
            QStringList bgCandidates = { "poster.jpg", "cover.jpg", "folder.jpg", "fanart.jpg" };
            QString bgArtPath;
            for (const QString& cand : bgCandidates) {
                QString fp = QDir(m_currentPath).filePath(cand);
                if (QFile::exists(fp)) { bgArtPath = fp; break; }
            }
            if (!bgArtPath.isEmpty()) {
                QPixmap pix(bgArtPath);
                if (!pix.isNull()) {
                    QImage img = pix.toImage().scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    QColor avgCol = img.pixelColor(8, 8);
                    QColor darkCol = QColor::fromRgb(qMin(avgCol.red(), 40), qMin(avgCol.green(), 45), qMin(avgCol.blue(), 60));
                    m_cachedBgStyle = QString("QWidget#theaterScrollWidget { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 #11111b); }")
                        .arg(darkCol.name());
                } else {
                    m_cachedBgStyle = "QWidget#theaterScrollWidget { background: transparent; }";
                }
            } else {
                m_cachedBgStyle = "QWidget#theaterScrollWidget { background: transparent; }";
            }
        }
        m_theaterScrollWidget->setStyleSheet(m_cachedBgStyle);
    }

    if (m_theaterScrollWidget) {
        m_theaterScrollWidget->setUpdatesEnabled(false);
    }

    // Clear old headers and grids
    QLayoutItem* item;
    while ((item = m_theaterScrollLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_theaterHeaders.clear();
    m_theaterGrids.clear();

    if (!m_groupProxy || !m_groupProxy->isGroupingActive()) {
        m_theaterScrollLayout->addStretch(1);
        if (m_theaterScrollWidget) {
            m_theaterScrollWidget->setUpdatesEnabled(true);
        }
        return;
    }

    // Get group count from proxy model
    int numGroups = m_groupProxy->rowCount(QModelIndex());
    for (int g = 0; g < numGroups; ++g) {
        QModelIndex groupIndex = m_groupProxy->index(g, 0, QModelIndex());
        if (!groupIndex.isValid()) continue;

        QString groupName = groupIndex.data().toString();
        int childCount = m_groupProxy->rowCount(groupIndex);

        // Header button
        QPushButton* btnHeader = new QPushButton(QString("▼  %1 (%2 items)").arg(groupName).arg(childCount), m_theaterScrollWidget);
        btnHeader->setStyleSheet("QPushButton { text-align: left; font-weight: bold; font-size: 13px; color: #89b4fa; background-color: #313244; border: 1px solid #45475a; border-radius: 6px; padding: 6px 12px; } QPushButton:hover { background-color: #45475a; }");
        m_theaterScrollLayout->addWidget(btnHeader);
        m_theaterHeaders.append(btnHeader);

        // Grid view for this group
        QListView* grid = new QListView(m_theaterScrollWidget);
        grid->setViewMode(QListView::IconMode);
        grid->setResizeMode(QListView::Adjust);
        grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
        grid->installEventFilter(this);
        if (grid->viewport()) grid->viewport()->installEventFilter(this);
        grid->setDragEnabled(false);
        grid->setAcceptDrops(false);
        grid->setDragDropMode(QAbstractItemView::NoDragDrop);
        grid->setFrameShape(QFrame::NoFrame);
        grid->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        grid->setItemDelegate(m_theaterDelegate);
        grid->setModel(m_groupProxy);
        grid->setRootIndex(groupIndex);

        // Adjust heights and styles
        int gw = 135, gh = 185;
        if (m_zoomLevel >= 0) {
            gw = 100 + m_zoomLevel * 35;
            gh = 140 + m_zoomLevel * 45;
        }
        grid->setGridSize(QSize(gw, gh));
        grid->setStyleSheet("QListView { background: transparent; border: none; }");

        // Simple height adjustment based on children
        int cols = qMax(1, m_theaterScrollWidget->width() / gw);
        int rows = (childCount + cols - 1) / cols;
        grid->setFixedHeight(rows * gh + 10);

        m_theaterScrollLayout->addWidget(grid);
        m_theaterGrids.append(grid);

        // Connect collapse/expand
        connect(btnHeader, &QPushButton::clicked, this, [btnHeader, grid, groupName, childCount]() {
            bool visible = grid->isVisible();
            grid->setVisible(!visible);
            btnHeader->setText(QString("%1  %2 (%3 items)").arg(!visible ? "▼" : "▶").arg(groupName).arg(childCount));
        });

        // Set up context menu and double click forwarding
        grid->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(grid, &QListView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
        connect(grid, &QListView::doubleClicked, this, &FilePanel::onDoubleClicked);
        connect(grid, &QListView::activated, this, &FilePanel::onDoubleClicked);

        // Connect selection model sync so clicking cards updates selection
        connect(grid->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this, grid](const QItemSelection& selected, const QItemSelection& deselected) {
            Q_UNUSED(selected);
            Q_UNUSED(deselected);
            disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
            m_treeView->selectionModel()->clearSelection();
            QModelIndexList selList = grid->selectionModel()->selectedIndexes();
            for (const QModelIndex& idx : selList) {
                m_treeView->selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
            connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
            onSelectionChanged();
        });
    }

    m_theaterScrollLayout->addStretch(1);
    if (m_theaterScrollWidget) {
        m_theaterScrollWidget->setUpdatesEnabled(true);
    }
    focusFirstItemInActiveView();
}

void FilePanel::focusFirstItemInActiveView() {
    QWidget* activeView = m_viewStack->currentWidget();
    if (activeView == m_theaterContainer) {
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            if (!m_theaterGrids.isEmpty()) {
                bool anySelected = false;
                for (QListView* g : m_theaterGrids) {
                    if (g->selectionModel() && !g->selectionModel()->selectedIndexes().isEmpty()) {
                        anySelected = true;
                        break;
                    }
                }
                if (!anySelected) {
                    QListView* firstGrid = m_theaterGrids.first();
                    QModelIndex firstIdx = firstGrid->model()->index(0, 0, firstGrid->rootIndex());
                    if (firstIdx.isValid()) {
                        firstGrid->setCurrentIndex(firstIdx);
                        firstGrid->selectionModel()->select(firstIdx, QItemSelectionModel::ClearAndSelect);
                        firstGrid->setFocus();
                        
                        disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                        m_treeView->selectionModel()->clearSelection();
                        m_treeView->selectionModel()->select(firstIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                        connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                        onSelectionChanged();
                    }
                }
            }
        } else {
            if (m_theaterListView->selectionModel() && m_theaterListView->selectionModel()->selectedIndexes().isEmpty()) {
                QModelIndex firstIdx = m_theaterListView->model()->index(0, 0, m_theaterListView->rootIndex());
                if (firstIdx.isValid()) {
                    m_theaterListView->setCurrentIndex(firstIdx);
                    m_theaterListView->selectionModel()->select(firstIdx, QItemSelectionModel::ClearAndSelect);
                    m_theaterListView->setFocus();
                    
                    disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                    m_treeView->selectionModel()->clearSelection();
                    m_treeView->selectionModel()->select(firstIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                    onSelectionChanged();
                }
            } else {
                m_theaterListView->setFocus();
            }
        }
    } else if (activeView == m_listView) {
        m_listView->setFocus();
    } else if (activeView == m_treeView) {
        m_treeView->setFocus();
    } else if (activeView == m_coverFlowView) {
        m_coverFlowView->setFocus();
    } else if (activeView == m_millerView) {
        m_millerView->setFocus();
    } else if (activeView == m_timelineView) {
        m_timelineView->setFocus();
    } else if (activeView == m_filmstripView) {
        m_filmstripView->setFocus();
    }
}

void FilePanel::updateThemeMusic() {
    QSettings settings("Amifiles", "Amifiles");
    bool autoPlayTheme = settings.value("theater/auto_play_theme_music", true).toBool();

    int vMode = viewModeIndex();
    if (!autoPlayTheme || (vMode != 6 && vMode != 7 && vMode != 8 && vMode != 9 && vMode != 10)) { // 6 = Audio, 7 = Video, 8 = Movie FS, 9 = TV FS, 10 = Music FS
        stopThemeMusic();
        return;
    }

    QDir dir(m_currentPath);
    QString foundPath;

    // Check current directory for case-insensitive theme.* file
    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& fi : fileList) {
        QString base = fi.baseName().toLower();
        QString ext = fi.suffix().toLower();
        if (base == "theme" && (ext == "mp3" || ext == "ogg" || ext == "flac" || ext == "wav" || ext == "m4a" || ext == "aac" || ext == "wma")) {
            foundPath = fi.absoluteFilePath();
            break;
        }
    }

    // If inside a season subfolder (e.g. Season 01), check parent directory for theme file
    if (foundPath.isEmpty()) {
        QFileInfo dirFi(m_currentPath);
        if (dirFi.fileName().contains("season", Qt::CaseInsensitive)) {
            QDir parentDir(dirFi.absolutePath());
            QFileInfoList parentFiles = parentDir.entryInfoList(QDir::Files);
            for (const QFileInfo& fi : parentFiles) {
                QString base = fi.baseName().toLower();
                QString ext = fi.suffix().toLower();
                if (base == "theme" && (ext == "mp3" || ext == "ogg" || ext == "flac" || ext == "wav" || ext == "m4a" || ext == "aac" || ext == "wma")) {
                    foundPath = fi.absoluteFilePath();
                    break;
                }
            }
        }
    }

    if (foundPath.isEmpty()) {
        stopThemeMusic();
        return;
    }

    if (m_currentThemePath == foundPath && m_themePlayer && m_themePlayer->playbackState() == QMediaPlayer::PlayingState) {
        return; // Already playing this theme
    }

    m_currentThemePath = foundPath;

    if (!m_themePlayer) {
        m_themePlayer = new QMediaPlayer(this);
        m_themeAudio = new QAudioOutput(this);
        m_themePlayer->setAudioOutput(m_themeAudio);
        m_themeAudio->setVolume(0.85f);
    }

    m_themePlayer->stop();
    m_themePlayer->setSource(QUrl::fromLocalFile(foundPath));
    m_themePlayer->play(); // Plays through exactly once to the end, then stops automatically
}

void FilePanel::stopThemeMusic() {
    m_currentThemePath.clear();
    if (m_themePlayer) {
        m_themePlayer->stop();
    }
}

void FilePanel::syncPlaylist(const QStringList& playlistPaths, int currentIndex) {
    if (!m_trackListWidget) return;
    m_trackListWidget->blockSignals(true);
    m_trackListWidget->clear();
    m_trackListWidget->setIconSize(QSize(40, 40));
    for (const QString& path : playlistPaths) {
        QString filename = QFileInfo(path).fileName();
        QString folderName = QFileInfo(QFileInfo(path).absolutePath()).fileName();
        QString displayName = filename;
        if (!folderName.isEmpty() && folderName.toLower() != "music" && folderName.toLower() != "audio" && folderName.toLower() != "download" && folderName.toLower() != "downloads") {
            displayName = QString("%1 (%2)").arg(filename).arg(folderName);
        }
        
        QListWidgetItem* item = new QListWidgetItem(displayName, m_trackListWidget);
        item->setData(Qt::UserRole, path);
        item->setIcon(getTrackArtworkIcon(path));
    }
    if (currentIndex >= 0 && currentIndex < m_trackListWidget->count()) {
        m_trackListWidget->setCurrentRow(currentIndex);
        if (viewModeIndex() == 10 && m_bottomTitle) {
            m_bottomTitle->setText(m_trackListWidget->item(currentIndex)->text());
        }
    }
    m_trackListWidget->blockSignals(false);
    updateDrawerVisibility();

    if (playlistPaths.size() > m_lastPlaylistSize) {
        int vm = viewModeIndex();
        if (vm >= 8 && vm <= 10) {
            m_blockCollapseTimerStop = true;
            m_btnToggleSidePane->setChecked(true);
            m_blockCollapseTimerStop = false;
            
            if (m_playlistCollapseTimer) {
                m_playlistCollapseTimer->start(10000); // 10 seconds
            }
        }
    }
    m_lastPlaylistSize = playlistPaths.size();
}

void FilePanel::updatePlaybackProgress(qint64 position, qint64 duration) {
    if (m_musicProgressLabel) {
        m_musicProgressLabel->setText(QString("%1 / %2")
            .arg(formatDuration(position))
            .arg(formatDuration(duration)));
    }
}

QString FilePanel::formatDuration(qint64 ms) const {
    qint64 totalSec = ms / 1000;
    qint64 min = totalSec / 60;
    qint64 sec = totalSec % 60;
    return QString("%1:%2")
        .arg(min, 2, 10, QChar('0'))
        .arg(sec, 2, 10, QChar('0'));
}

void FilePanel::notifyPathDataChanged(const QString& path) {
    if (!m_fileModel) return;
    QModelIndex srcIdx = m_fileModel->index(path);
    if (!srcIdx.isValid()) return;

    emit m_fileModel->dataChanged(srcIdx, srcIdx);

    if (m_proxyModel) {
        QModelIndex filterIdx = m_proxyModel->mapFromSource(srcIdx);
        if (filterIdx.isValid()) {
            emit m_proxyModel->dataChanged(filterIdx, filterIdx);
            if (m_groupProxy) {
                QModelIndex grpIdx = m_groupProxy->mapFromSource(filterIdx);
                if (grpIdx.isValid()) {
                    emit m_groupProxy->dataChanged(grpIdx, grpIdx);
                }
            }
        }
    }
}

QIcon FilePanel::getTrackArtworkIcon(const QString& trackPath) {
    static QHash<QString, QIcon> trackArtCache;
    if (trackArtCache.contains(trackPath)) {
        return trackArtCache[trackPath];
    }

    QString dirPath = QFileInfo(trackPath).absolutePath();
    static QHash<QString, QIcon> folderArtCache;
    if (folderArtCache.contains(dirPath)) {
        QIcon icon = folderArtCache[dirPath];
        trackArtCache[trackPath] = icon;
        return icon;
    }

    // Try finding local folder cover art
    QDir dir(dirPath);
    QStringList artNames = { "folder", "cover", "album", "poster", "front" };
    QStringList artExts = { "jpg", "jpeg", "png", "webp" };
    for (const QString& name : artNames) {
        for (const QString& ext : artExts) {
            QString path = dir.filePath(name + "." + ext);
            if (QFile::exists(path)) {
                QPixmap p(path);
                if (!p.isNull()) {
                    QIcon icon(p.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    folderArtCache[dirPath] = icon;
                    trackArtCache[trackPath] = icon;
                    return icon;
                }
            }
        }
    }

    // Check parent directory (in case of CD 1 / CD 2 folders)
    QDir parentDir = dir;
    if (parentDir.cdUp()) {
        for (const QString& name : artNames) {
            for (const QString& ext : artExts) {
                QString path = parentDir.filePath(name + "." + ext);
                if (QFile::exists(path)) {
                    QPixmap p(path);
                    if (!p.isNull()) {
                        QIcon icon(p.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        folderArtCache[dirPath] = icon;
                        trackArtCache[trackPath] = icon;
                        return icon;
                    }
                }
            }
        }
    }

    // Try embedded artwork via exiftool as fallback
    QProcess proc;
    proc.start("exiftool", {"-Picture", "-b", trackPath});
    if (proc.waitForFinished(800)) {
        QByteArray imgData = proc.readAllStandardOutput();
        if (!imgData.isEmpty()) {
            QPixmap p;
            if (p.loadFromData(imgData)) {
                QIcon icon(p.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                trackArtCache[trackPath] = icon;
                return icon;
            }
        }
    }

    // Default fallback icon
    QIcon fallbackIcon = QIcon::fromTheme("audio-x-generic");
    trackArtCache[trackPath] = fallbackIcon;
    return fallbackIcon;
}

// ==========================================
// AudioVisualizerWidget Implementation
// ==========================================

#include <complex>
#include <vector>
#include <cmath>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QMediaPlayer>
#include "mainwindow.h"
#include "previewpanel.h"

static void fft_iterative_panel(std::vector<std::complex<double>>& a) {
    int n = a.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * 3.14159265358979323846 / len;
        std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (int j = 0; j < len / 2; j++) {
                std::complex<double> u = a[i + j];
                std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

AudioVisualizerWidget::AudioVisualizerWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(130, 60);
    
    QSettings settings("Amifiles", "Amifiles");
    m_style = static_cast<Style>(settings.value("preview/visualizer_style", VerticalBars).toInt());
    
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AudioVisualizerWidget::onAnimate);
    
    for (int i = 0; i < 15; ++i) m_heights[i] = 4.0;
    m_timer->start(30); // 30ms for smooth rendering
}

AudioVisualizerWidget::~AudioVisualizerWidget() {
    QString tempPath = QDir::temp().filePath(QString("amifiles_analysis_panel_%1.wav").arg(qApp->applicationPid()));
    QFile::remove(tempPath);
}

void AudioVisualizerWidget::setPlaying(bool playing) {
    m_playing = playing;
    if (!m_playing) {
        for (int i = 0; i < 15; ++i) m_heights[i] = 4.0;
    }
    update();
}

void AudioVisualizerWidget::setStyle(Style style) {
    m_style = style;
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preview/visualizer_style", static_cast<int>(m_style));
    update();
}

void AudioVisualizerWidget::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #313244; } QMenu::item:selected { background-color: #313244; color: #f5c2e7; }");
    
    QAction* actBars = menu.addAction("Classic Spectrum Bars");
    QAction* actCrt = menu.addAction("CRT Oscilloscope");
    QAction* actLed = menu.addAction("Retro LED Matrix");
    
    actBars->setCheckable(true);
    actCrt->setCheckable(true);
    actLed->setCheckable(true);
    
    if (m_style == VerticalBars) actBars->setChecked(true);
    else if (m_style == CrtOscilloscope) actCrt->setChecked(true);
    else if (m_style == LedMatrix) actLed->setChecked(true);
    
    QAction* selected = menu.exec(event->globalPos());
    if (selected == actBars) setStyle(VerticalBars);
    else if (selected == actCrt) setStyle(CrtOscilloscope);
    else if (selected == actLed) setStyle(LedMatrix);
}

void AudioVisualizerWidget::updateAudioPath() {
    QWidget* p = parentWidget();
    while (p && !p->inherits("MainWindow")) {
        p = p->parentWidget();
    }
    if (!p) return;
    
    MainWindow* mw = qobject_cast<MainWindow*>(p);
    if (!mw || !mw->previewPanel()) return;
    
    QMediaPlayer* player = mw->previewPanel()->player();
    if (!player) return;
    
    QString activePath = player->source().toLocalFile();
    
    QString suffix = QFileInfo(activePath).suffix().toLower();
    if (suffix == "mod" || suffix == "xm" || suffix == "s3m" || suffix == "it" || suffix == "sid") {
        QString tempWav = QDir::temp().filePath(QString("amifiles_retro_%1.wav").arg(qApp->applicationPid()));
        if (QFile::exists(tempWav)) {
            activePath = tempWav;
        }
    }
    
    if (m_loadedAudioPath != activePath) {
        m_loadedAudioPath = activePath;
        m_audioData.clear();
        m_samples = nullptr;
        m_numSamples = 0;
        
        if (!activePath.isEmpty()) {
            QFileInfo info(activePath);
            QString ext = info.suffix().toLower();
            if (ext == "wav") {
                loadWavData(activePath);
            } else {
                QString tempPath = QDir::temp().filePath(QString("amifiles_analysis_panel_%1.wav").arg(qApp->applicationPid()));
                QProcess* proc = new QProcess(this);
                connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, tempPath, proc](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        loadWavData(tempPath);
                    }
                    proc->deleteLater();
                });
                proc->start("ffmpeg", QStringList() << "-y" << "-i" << activePath 
                                                    << "-map_metadata" << "-1" 
                                                    << "-ac" << "1" 
                                                    << "-ar" << "22050" 
                                                    << "-acodec" << "pcm_s16le" 
                                                    << tempPath);
            }
        }
    }
}

void AudioVisualizerWidget::loadWavData(const QString& wavPath) {
    QFile file(wavPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    m_audioData = file.readAll();
    file.close();

    if (m_audioData.size() < 44) return;
    if (m_audioData.mid(0, 4) != "RIFF" || m_audioData.mid(8, 4) != "WAVE") return;

    int pos = 12;
    int dataOffset = -1;
    int dataSize = 0;
    int sampleRate = 22050;
    int numChannels = 1;

    while (pos + 8 <= m_audioData.size()) {
        QByteArray chunkId = m_audioData.mid(pos, 4);
        int chunkSize = *reinterpret_cast<const int*>(m_audioData.constData() + pos + 4);
        if (chunkId == "fmt ") {
            if (chunkSize >= 16) {
                numChannels = *reinterpret_cast<const int16_t*>(m_audioData.constData() + pos + 8 + 2);
                sampleRate = *reinterpret_cast<const int*>(m_audioData.constData() + pos + 8 + 4);
            }
        } else if (chunkId == "data") {
            dataOffset = pos + 8;
            dataSize = chunkSize;
            break;
        }
        pos += 8 + chunkSize;
    }

    if (dataOffset != -1 && dataOffset + dataSize <= m_audioData.size()) {
        m_samples = reinterpret_cast<const int16_t*>(m_audioData.constData() + dataOffset);
        m_numSamples = dataSize / 2;
        m_sampleRate = sampleRate;
        m_numChannels = numChannels;
    }
}

void AudioVisualizerWidget::onAnimate() {
    updateAudioPath();
    
    QWidget* p = parentWidget();
    while (p && !p->inherits("MainWindow")) {
        p = p->parentWidget();
    }
    QMediaPlayer* player = nullptr;
    if (p) {
        MainWindow* mw = qobject_cast<MainWindow*>(p);
        if (mw && mw->previewPanel()) {
            player = mw->previewPanel()->player();
        }
    }
    
    m_playing = player && (player->playbackState() == QMediaPlayer::PlayingState);
    qint64 positionMs = player ? player->position() : 0;
    int sampleIndex = static_cast<int>((positionMs / 1000.0) * m_sampleRate);
    
    if (m_style == VerticalBars || m_style == LedMatrix) {
        int fftSize = 512;
        std::vector<std::complex<double>> fftInput(fftSize, 0.0);

        if (m_playing && m_samples && m_numSamples > 0) {
            int start = sampleIndex - fftSize / 2;
            for (int i = 0; i < fftSize; ++i) {
                int idx = (start + i) * m_numChannels;
                if (idx >= 0 && idx < m_numSamples) {
                    double window = 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * i / (fftSize - 1)));
                    double sampleVal = m_samples[idx] / 32768.0;
                    fftInput[i] = sampleVal * window;
                } else {
                    fftInput[i] = 0.0;
                }
            }
        }

        fft_iterative_panel(fftInput);

        std::vector<double> magnitudes(fftSize / 2, 0.0);
        for (int i = 0; i < fftSize / 2; ++i) {
            magnitudes[i] = std::abs(fftInput[i]);
        }

        int numBars = 15;
        double startFreq = 20.0;
        double endFreq = 8000.0;

        for (int b = 0; b < numBars; ++b) {
            double target = 0.0;
            if (m_playing) {
                double fStart = startFreq * std::pow(endFreq / startFreq, static_cast<double>(b) / numBars);
                double fEnd = startFreq * std::pow(endFreq / startFreq, static_cast<double>(b + 1) / numBars);

                int binStart = qBound(0, static_cast<int>(fStart * fftSize / m_sampleRate), fftSize / 2 - 1);
                int binEnd = qBound(binStart + 1, static_cast<int>(fEnd * fftSize / m_sampleRate), fftSize / 2);

                double sum = 0.0;
                for (int i = binStart; i < binEnd; ++i) {
                    sum += magnitudes[i];
                }
                double avg = sum / (binEnd - binStart);

                // Adjust gain factor for visual appeal and screen height
                double heightVal = avg * (height() - 8.0) * 3.0;
                if (heightVal > height() - 8.0) heightVal = height() - 8.0;
                if (heightVal < 2.0) heightVal = 2.0;
                target = heightVal;
            } else {
                target = 2.0;
            }
            m_heights[b] = m_heights[b] * 0.6 + target * 0.4;
        }
    } else {
        m_phase += 0.25;
    }
    
    update();
}

void AudioVisualizerWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    int w = width();
    int h = height();
    
    if (m_style == VerticalBars) {
        int numBars = 15;
        double gap = 3.0;
        double barW = (w - (numBars - 1) * gap) / numBars;
        
        QLinearGradient grad(0, h, 0, 0);
        grad.setColorAt(0.0, QColor("#a6e3a1"));
        grad.setColorAt(0.6, QColor("#89b4fa"));
        grad.setColorAt(1.0, QColor("#f5c2e7"));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        
        for (int i = 0; i < numBars; ++i) {
            double barH = m_heights[i];
            QRectF barRect(i * (barW + gap), h - barH, barW, barH);
            p.drawRoundedRect(barRect, 2, 2);
        }
    } 
    else if (m_style == CrtOscilloscope) {
        int cy = h / 2;
        p.setBrush(Qt::NoBrush);
        
        QPainterPath path;
        path.moveTo(0, cy);
        
        int numPoints = w / 2;
        if (numPoints < 10) numPoints = 10;
        
        QWidget* parent = parentWidget();
        while (parent && !parent->inherits("MainWindow")) {
            parent = parent->parentWidget();
        }
        QMediaPlayer* player = nullptr;
        if (parent) {
            MainWindow* mw = qobject_cast<MainWindow*>(parent);
            if (mw && mw->previewPanel()) {
                player = mw->previewPanel()->player();
            }
        }
        
        qint64 positionMs = player ? player->position() : 0;
        int sampleIndex = static_cast<int>((positionMs / 1000.0) * m_sampleRate);
        
        if (m_playing && m_samples && m_numSamples > 0) {
            int spacing = 5;
            for (int i = 0; i < numPoints; ++i) {
                int x = i * 2;
                int idx = (sampleIndex + i * spacing) * m_numChannels;
                double y = cy;
                if (idx >= 0 && idx < m_numSamples) {
                    y += (m_samples[idx] / 32768.0) * (h * 0.4);
                }
                path.lineTo(x, y);
            }
        } else {
            for (int x = 0; x < w; x += 2) {
                double t = (double)x / w;
                double y = cy + qSin(t * 12.0 + m_phase) * 2.0;
                path.lineTo(x, y);
            }
        }
        
        QPen penGlow(QColor(166, 227, 161, 70));
        penGlow.setWidth(6);
        p.setPen(penGlow);
        p.drawPath(path);
        
        QPen penCore(QColor("#a6e3a1"));
        penCore.setWidth(2);
        p.setPen(penCore);
        p.drawPath(path);
    }
    else if (m_style == LedMatrix) {
        int numBars = 12;
        double gap = 4.0;
        double barW = (w - (numBars - 1) * gap) / numBars;
        int segmentH = 4;
        int segmentGap = 2;
        int maxSegments = h / (segmentH + segmentGap);
        if (maxSegments < 2) maxSegments = 2;
        
        p.setPen(Qt::NoPen);
        
        for (int i = 0; i < numBars; ++i) {
            double barH = m_heights[i % 15];
            int activeSegments = (barH / h) * maxSegments;
            if (activeSegments < 1 && m_playing) activeSegments = 1;
            
            for (int s = 0; s < maxSegments; ++s) {
                double t = (double)s / maxSegments;
                QColor col;
                if (t < 0.6) col = QColor("#a6e3a1");      // Green bottom
                else if (t < 0.85) col = QColor("#f9e2af"); // Yellow middle
                else col = QColor("#f38ba8");               // Red peak
                
                if (s >= activeSegments) {
                    col.setAlpha(35); // Dim background LED grid cells
                }
                p.setBrush(col);
                
                double y = h - (s + 1) * (segmentH + segmentGap);
                QRectF segRect(i * (barW + gap), y, barW, segmentH);
                p.drawRect(segRect);
            }
        }
    }
}

void FilePanel::playCurrentOrSelectedFolder() {
    QString targetFolder = m_currentPath;
    
    QModelIndexList selected = m_treeView->selectionModel()->selectedRows();
    if (!selected.isEmpty()) {
        QModelIndex srcIndex = m_proxyModel->mapToSource(selected.first());
        QFileInfo info = m_fileModel->fileInfo(srcIndex);
        if (info.isDir()) {
            targetFolder = info.absoluteFilePath();
        }
    }
    
    QStringList playlistPaths;
    scanMediaFilesRecursively(targetFolder, playlistPaths, 0);
    if (!playlistPaths.isEmpty()) {
        emit playMediaBuiltinRequested(playlistPaths);
    }
}

void FilePanel::queueCurrentOrSelectedFolder() {
    QString targetFolder = m_currentPath;
    
    QModelIndexList selected = m_treeView->selectionModel()->selectedRows();
    if (!selected.isEmpty()) {
        QModelIndex srcIndex = m_proxyModel->mapToSource(selected.first());
        QFileInfo info = m_fileModel->fileInfo(srcIndex);
        if (info.isDir()) {
            targetFolder = info.absoluteFilePath();
        }
    }
    
    QStringList playlistPaths;
    scanMediaFilesRecursively(targetFolder, playlistPaths, 0);
    if (!playlistPaths.isEmpty()) {
        emit queueMediaBuiltinRequested(playlistPaths);
    }
}

void FilePanel::playPlaylistQueue() {
    emit playQueueFullscreenRequested();
}

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QInputDialog>
#include <QMessageBox>

class FilePermissionsDialog : public QDialog {
public:
    FilePermissionsDialog(const QString& filePath, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Change File Permissions");
        resize(350, 250);
        setStyleSheet(
            "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
            "QLabel { color: #cdd6f4; }"
            "QCheckBox { color: #cdd6f4; }"
            "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; }"
            "QPushButton:hover { background-color: #45475a; }"
        );
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(10);
        
        QLabel* titleLabel = new QLabel(QString("Permissions for: %1").arg(QFileInfo(filePath).fileName()), this);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #89b4fa;");
        mainLayout->addWidget(titleLabel);
        
        QGridLayout* grid = new QGridLayout();
        grid->setSpacing(8);
        
        grid->addWidget(new QLabel("Read", this), 0, 1);
        grid->addWidget(new QLabel("Write", this), 0, 2);
        grid->addWidget(new QLabel("Execute", this), 0, 3);
        
        grid->addWidget(new QLabel("Owner:", this), 1, 0);
        m_ownerR = new QCheckBox(this); grid->addWidget(m_ownerR, 1, 1);
        m_ownerW = new QCheckBox(this); grid->addWidget(m_ownerW, 1, 2);
        m_ownerX = new QCheckBox(this); grid->addWidget(m_ownerX, 1, 3);
        
        grid->addWidget(new QLabel("Group:", this), 2, 0);
        m_groupR = new QCheckBox(this); grid->addWidget(m_groupR, 2, 1);
        m_groupW = new QCheckBox(this); grid->addWidget(m_groupW, 2, 2);
        m_groupX = new QCheckBox(this); grid->addWidget(m_groupX, 2, 3);
        
        grid->addWidget(new QLabel("Others:", this), 3, 0);
        m_otherR = new QCheckBox(this); grid->addWidget(m_otherR, 3, 1);
        m_otherW = new QCheckBox(this); grid->addWidget(m_otherW, 3, 2);
        m_otherX = new QCheckBox(this); grid->addWidget(m_otherX, 3, 3);
        
        mainLayout->addLayout(grid);
        
        // Load current permissions
        QFile file(filePath);
        QFile::Permissions p = file.permissions();
        m_ownerR->setChecked(p & QFile::ReadOwner);
        m_ownerW->setChecked(p & QFile::WriteOwner);
        m_ownerX->setChecked(p & QFile::ExeOwner);
        
        m_groupR->setChecked(p & QFile::ReadGroup);
        m_groupW->setChecked(p & QFile::WriteGroup);
        m_groupX->setChecked(p & QFile::ExeGroup);
        
        m_otherR->setChecked(p & QFile::ReadOther);
        m_otherW->setChecked(p & QFile::WriteOther);
        m_otherX->setChecked(p & QFile::ExeOther);
        
        QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttons);
    }
    
    QFile::Permissions permissions() const {
        QFile::Permissions p = {};
        if (m_ownerR->isChecked()) p |= QFile::ReadOwner;
        if (m_ownerW->isChecked()) p |= QFile::WriteOwner;
        if (m_ownerX->isChecked()) p |= QFile::ExeOwner;
        
        if (m_groupR->isChecked()) p |= QFile::ReadGroup;
        if (m_groupW->isChecked()) p |= QFile::WriteGroup;
        if (m_groupX->isChecked()) p |= QFile::ExeGroup;
        
        if (m_otherR->isChecked()) p |= QFile::ReadOther;
        if (m_otherW->isChecked()) p |= QFile::WriteOther;
        if (m_otherX->isChecked()) p |= QFile::ExeOther;
        return p;
    }
    
private:
    QCheckBox* m_ownerR;
    QCheckBox* m_ownerW;
    QCheckBox* m_ownerX;
    QCheckBox* m_groupR;
    QCheckBox* m_groupW;
    QCheckBox* m_groupX;
    QCheckBox* m_otherR;
    QCheckBox* m_otherW;
    QCheckBox* m_otherX;
};

void FilePanel::createNewFileTemplate(const QString& ext) {
    QString defaultName = "New Document." + ext;
    if (ext == "png") defaultName = "New Image.png";
    else if (ext == "py") defaultName = "script.py";
    
    bool ok;
    QString name = QInputDialog::getText(this, "Create New File", "File Name:", QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.isEmpty()) return;
    
    QString filePath = QDir(m_currentPath).filePath(name);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        if (ext == "png") {
            QImage img(1, 1, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            img.save(&file, "PNG");
        } else if (ext == "html") {
            file.write("<!DOCTYPE html>\n<html>\n<head>\n<title></title>\n</head>\n<body>\n</body>\n</html>");
        } else if (ext == "md") {
            file.write("# New Document\n");
        } else if (ext == "py") {
            file.write("#!/usr/bin/env python3\n\nprint(\"Hello, World!\")\n");
        } else {
            file.write("");
        }
        file.close();
        refresh();
    } else {
        QMessageBox::critical(this, "Error", "Could not create file: " + file.errorString());
    }
}

void FilePanel::toggleSelectedExecutable() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;
    
    for (const QString& path : paths) {
        QFile file(path);
        QFile::Permissions perms = file.permissions();
        if (perms & QFile::ExeOwner) {
            perms &= ~(QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
        } else {
            perms |= (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
        }
        file.setPermissions(perms);
    }
    refresh();
}

void FilePanel::changeSelectedPermissions() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;
    QString path = paths.first();
    
    FilePermissionsDialog dlg(path, this);
    if (dlg.exec() == QDialog::Accepted) {
        QFile file(path);
        file.setPermissions(dlg.permissions());
        refresh();
    }
}

void FilePanel::lockSelectedFolderRecursive(bool lock) {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    QString actionStr = lock ? "Lock" : "Unlock";
    QString question = QString("Are you sure you want to %1 the selected file(s)/folder(s) recursively?\n"
                               "This will modify write permissions for all nested files and folders.")
                       .arg(lock ? "lock" : "unlock");
    if (QMessageBox::question(this, actionStr + " Confirmation", question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    int filesCount = 0;
    int foldersCount = 0;

    for (const QString& rootPath : paths) {
        QFileInfo rootInfo(rootPath);
        if (!rootInfo.exists()) continue;

        QStringList pathsToProcess;
        pathsToProcess.append(rootPath);

        if (rootInfo.isDir()) {
            QDirIterator it(rootPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                pathsToProcess.append(it.next());
            }
        }

        for (const QString& itemPath : pathsToProcess) {
            QFileInfo info(itemPath);
            QFile::Permissions p = QFile::permissions(itemPath);
            if (lock) {
                p &= ~(QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther);
            } else {
                p |= (QFile::WriteOwner | QFile::WriteUser);
                if (info.isDir()) {
                    p |= (QFile::ExeOwner | QFile::ExeUser);
                }
            }
            
            // Apply OS permission change (backup, works locally)
            QFile::setPermissions(itemPath, p);

            // Apply persistent application-level database lock
            TagManager::instance().setFileLocked(itemPath, lock);

            if (info.isDir()) foldersCount++;
            else filesCount++;
        }
    }

    QMessageBox::information(this, actionStr + " Completed",
                             QString("Successfully %1ed:\n- %2 files\n- %3 folders")
                             .arg(actionStr)
                             .arg(filesCount)
                             .arg(foldersCount));
    refresh();
}

void FilePanel::onCreateSymlinkInSiblingPane() {
    FilePanel* targetPanel = m_siblingPanel;
    if (!targetPanel) {
        QMessageBox::warning(this, "Create Symlink", "Dual-pane split view is not active.");
        return;
    }
    QString destDir = targetPanel->currentPath();
    if (destDir.isEmpty() || destDir.startsWith("smart://")) {
        QMessageBox::warning(this, "Create Symlink", "Invalid target folder in sibling pane.");
        return;
    }

    QStringList targets = selectedPaths();
    if (targets.isEmpty() && !m_currentPath.isEmpty() && !m_currentPath.startsWith("smart://")) {
        targets.append(m_currentPath);
    }
    if (targets.isEmpty()) return;

    int createdCount = 0;
    for (const QString& target : targets) {
        QFileInfo fi(target);
        QString linkName = QDir(destDir).filePath(fi.fileName());
        if (QFile::exists(linkName)) {
            QMessageBox::warning(this, "Create Symlink", QString("Destination path already exists:\n%1").arg(linkName));
            continue;
        }
        if (QFile::link(target, linkName)) {
            createdCount++;
        } else {
            if (QProcess::execute("ln", { "-s", target, linkName }) == 0 && QFile::exists(linkName)) {
                createdCount++;
            } else {
                QMessageBox::warning(this, "Create Symlink", QString("Failed to create symlink for:\n%1").arg(target));
            }
        }
    }
    if (createdCount > 0) {
        targetPanel->refresh();
    }
}

void FilePanel::keyPressEvent(QKeyEvent* event) {
    if (m_filterEdit && m_filterEdit->hasFocus()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        if (!m_typeAheadString.isEmpty()) {
            m_typeAheadString.clear();
            if (m_typeAheadBadge) m_typeAheadBadge->hide();
            if (m_filterEdit) m_filterEdit->clear();
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Backspace) {
        if (!m_typeAheadString.isEmpty()) {
            m_typeAheadString.chop(1);
            if (m_typeAheadString.isEmpty()) {
                if (m_typeAheadBadge) m_typeAheadBadge->hide();
                if (m_filterEdit) m_filterEdit->clear();
            } else {
                if (m_filterEdit) m_filterEdit->setText(m_typeAheadString);
            }
            event->accept();
            return;
        }
    } else if (!event->text().isEmpty() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        QChar ch = event->text().at(0);
        if (ch.isPrint() && ch != '\r' && ch != '\n' && ch != '\t') {
            m_typeAheadString.append(ch);

            if (!m_typeAheadBadge) {
                m_typeAheadBadge = new QLabel(this);
                m_typeAheadBadge->setStyleSheet(
                    "background-color: #11111b; color: #89b4fa; border: 2px solid #89b4fa; "
                    "border-radius: 6px; padding: 4px 10px; font-weight: bold; font-size: 13px;"
                );
            }
            m_typeAheadBadge->setText(QString("🔍 Jump: \"%1\"").arg(m_typeAheadString));
            m_typeAheadBadge->adjustSize();
            m_typeAheadBadge->move(width() - m_typeAheadBadge->width() - 20, 45);
            m_typeAheadBadge->raise();
            m_typeAheadBadge->show();

            if (!m_typeAheadTimer) {
                m_typeAheadTimer = new QTimer(this);
                m_typeAheadTimer->setSingleShot(true);
                connect(m_typeAheadTimer, &QTimer::timeout, this, [this]() {
                    m_typeAheadString.clear();
                    if (m_typeAheadBadge) m_typeAheadBadge->hide();
                });
            }
            m_typeAheadTimer->start(1800);

            if (m_filterEdit) {
                m_filterEdit->setText(m_typeAheadString);
            }
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void FilePanel::removeSelectedGreenScreen() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;
    
    QStringList savedFiles;
    for (const QString& path : paths) {
        QImage img(path);
        if (img.isNull()) continue;
        
        img = img.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y) {
            QRgb* row = (QRgb*)img.scanLine(y);
            for (int x = 0; x < img.width(); ++x) {
                QRgb pixel = row[x];
                int r = qRed(pixel);
                int g = qGreen(pixel);
                int b = qBlue(pixel);
                double sum = r + g + b;
                if (sum > 0) {
                    double g_norm = (double)g / sum;
                    if (g_norm > 0.40 && g > 60 && g > r * 1.15 && g > b * 1.15) {
                        row[x] = qRgba(0, 0, 0, 0);
                    }
                }
            }
        }
        
        QFileInfo fi(path);
        QString outPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_chromakey.png";
        if (img.save(outPath, "PNG")) {
            savedFiles.append(fi.completeBaseName() + "_chromakey.png");
        }
    }
    
    if (!savedFiles.isEmpty()) {
        QMessageBox::information(this, "Green Screen Removed", 
            QString("Successfully removed green screen from selected image(s).\n\nCreated:\n%1").arg(savedFiles.join("\n")));
    } else {
        QMessageBox::warning(this, "Green Screen Removal Failed", "No images were processed successfully. Ensure the selected files are valid images with green backgrounds.");
    }
    refresh();
}

QIcon FilePanel::getIconForPathOrTheme(const QString& pathOrTheme) {
    if (pathOrTheme.isEmpty()) return QIcon();
    if (QFileInfo(pathOrTheme).exists()) return QIcon(pathOrTheme);
    return QIcon::fromTheme(pathOrTheme);
}

QJsonArray FilePanel::getDefaultContextMenuJson() const {
    QJsonArray arr;
    
    auto makeAction = [](const QString& title, const QString& command, const QString& icon = "") {
        QJsonObject obj;
        obj["type"] = "action";
        obj["title"] = title;
        obj["command"] = command;
        obj["icon"] = icon;
        obj["color"] = "";
        obj["mode"] = "Normal";
        return obj;
    };
    
    auto makeSeparator = []() {
        QJsonObject obj;
        obj["type"] = "separator";
        return obj;
    };

    // 1. Top-level essential commands
    arr.append(makeAction("Open", "app.open", "document-open"));
    arr.append(makeSeparator());
    arr.append(makeAction("Cut", "app.cut", "edit-cut"));
    arr.append(makeAction("Copy", "app.copy", "edit-copy"));
    arr.append(makeAction("Paste", "app.paste", "edit-paste"));
    arr.append(makeAction("Delete", "app.delete", "user-trash"));
    arr.append(makeAction("Rename", "app.rename", "edit-rename"));
    arr.append(makeAction("Bulk Rename...", "app.bulk_rename", ""));
    arr.append(makeSeparator());

    // 2. New Folder / Files Submenu
    {
        QJsonObject newMenu;
        newMenu["type"] = "menu";
        newMenu["title"] = "New";
        newMenu["icon"] = "folder-new";
        
        QJsonArray newKids;
        newKids.append(makeAction("New Folder", "app.new_folder", "folder-new"));
        newKids.append(makeAction("Advanced New Folder...", "app.advanced_new_folder", ""));
        newKids.append(makeSeparator());
        
        QJsonObject newFileMenu;
        newFileMenu["type"] = "menu";
        newFileMenu["title"] = "New File";
        newFileMenu["icon"] = "document-new";
        
        QJsonArray fileKids;
        fileKids.append(makeAction("Text Document (.txt)", "app.new_file_txt", ""));
        fileKids.append(makeAction("Markdown Document (.md)", "app.new_file_md", ""));
        fileKids.append(makeAction("HTML Document (.html)", "app.new_file_html", ""));
        fileKids.append(makeAction("Python Script (.py)", "app.new_file_py", ""));
        fileKids.append(makeAction("Blank PNG Image (.png)", "app.new_file_png", ""));
        
        newFileMenu["children"] = fileKids;
        newKids.append(newFileMenu);
        
        newMenu["children"] = newKids;
        arr.append(newMenu);
    }
    arr.append(makeSeparator());

    // 3. Clipboard Actions Submenu
    {
        QJsonObject clipMenu;
        clipMenu["type"] = "menu";
        clipMenu["title"] = "Clipboard Actions";
        clipMenu["icon"] = "edit-copy";
        
        QJsonArray clipKids;
        clipKids.append(makeAction("Copy File Name(s)", "app.copy_filename", ""));
        clipKids.append(makeAction("Copy Full Path(s)", "app.copy_path", ""));
        clipKids.append(makeAction("Copy Folder Contents (Paths List)", "app.copy_folder_contents", ""));
        clipKids.append(makeSeparator());
        clipKids.append(makeAction("Copy to Sibling Panel", "app.copy_sibling", "go-next"));
        clipKids.append(makeAction("Move to Sibling Panel", "app.move_sibling", "go-jump"));
        
        clipMenu["children"] = clipKids;
        arr.append(clipMenu);
    }

    // 4. Color Label & Overlay Submenu
    {
        QJsonObject colMenu;
        colMenu["type"] = "menu";
        colMenu["title"] = "Color Label & Overlay";
        colMenu["icon"] = "preferences-desktop-color";
        
        QJsonArray colKids;
        colKids.append(makeAction("None", "app.color_none", ""));
        colKids.append(makeAction("Red", "app.color_red", ""));
        colKids.append(makeAction("Orange", "app.color_orange", ""));
        colKids.append(makeAction("Yellow", "app.color_yellow", ""));
        colKids.append(makeAction("Green", "app.color_green", ""));
        colKids.append(makeAction("Blue", "app.color_blue", ""));
        colKids.append(makeAction("Purple", "app.color_purple", ""));
        colKids.append(makeSeparator());
        colKids.append(makeAction("Custom Icon Overlay...", "app.color_custom_overlay", ""));
        colKids.append(makeAction("Clear Icon Overlay", "app.color_clear_overlay", ""));
        
        colMenu["children"] = colKids;
        arr.append(colMenu);
    }
    arr.append(makeSeparator());

    // 5. Media & Tagging Tools Submenu
    {
        QJsonObject mediaMenu;
        mediaMenu["type"] = "menu";
        mediaMenu["title"] = "Media & Tagging Tools";
        mediaMenu["icon"] = "applications-multimedia";
        
        QJsonArray mediaKids;
        mediaKids.append(makeAction("Edit Audio Tags...", "app.edit_tags", ""));
        mediaKids.append(makeAction("Batch Tag/Filename Casing Wizard...", "app.metadata_casing_wizard", ""));
        mediaKids.append(makeAction("Advanced Tag Editor...", "app.advanced_tag_editor", ""));
        mediaKids.append(makeAction("Fetch MusicBrainz Album Info...", "app.fetch_musicbrainz", ""));
        mediaKids.append(makeAction("Scrape Video Metadata...", "app.scrape_video", ""));
        mediaKids.append(makeAction("Fetch Cover Art & Wallpaper...", "app.fetch_folder_art", ""));
        mediaKids.append(makeSeparator());
        mediaKids.append(makeAction("File Tags...", "app.file_tags", ""));
        mediaKids.append(makeAction("🎬 Media Info Sheet... (ℹ)", "app.media_info_sheet", "dialog-information"));
        mediaKids.append(makeAction("✔ Toggle Watch Status (Watched/Unwatched)", "app.toggle_watch", ""));
        mediaKids.append(makeSeparator());
        mediaKids.append(makeAction("Play Folder/Album in Preview", "app.play_preview", "media-playback-start"));
        mediaKids.append(makeAction("Play Folder/Album in Fullscreen", "app.play_fullscreen_playlist", "media-playback-start"));
        
        mediaMenu["children"] = mediaKids;
        arr.append(mediaMenu);
    }

    // 6. System & Advanced Tools Submenu
    {
        QJsonObject sysMenu;
        sysMenu["type"] = "menu";
        sysMenu["title"] = "System & Advanced Tools";
        sysMenu["icon"] = "applications-system";
        
        QJsonArray sysKids;
        sysKids.append(makeAction("Toggle Executable Status", "app.toggle_executable", ""));
        sysKids.append(makeAction("Change File Permissions (chmod)...", "app.change_permissions", ""));
        sysKids.append(makeAction("Lock Folder/File (Prevent Deletion)", "app.lock_folder_recursive", ""));
        sysKids.append(makeAction("Unlock Folder/File", "app.unlock_folder_recursive", ""));
        sysKids.append(makeAction("🔗 Create Symlink in Sibling Pane", "app.create_symlink_sibling", ""));
        sysKids.append(makeAction("Remove Green Screen 🟢", "app.remove_green_screen", ""));
        sysKids.append(makeSeparator());
        sysKids.append(makeAction("Compare Selected Files", "app.compare_selected", ""));
        sysKids.append(makeAction("Compare with Sibling Pane File", "app.compare_sibling", ""));
        sysKids.append(makeSeparator());
        sysKids.append(makeAction("Calculate Checksum Hash...", "app.calculate_checksum", ""));
        sysKids.append(makeAction("Secure Shred (Delete Permanently)...", "app.secure_shred", "user-trash"));
        sysKids.append(makeAction("Batch Convert/Resize Images...", "app.image_convert", ""));
        sysKids.append(makeAction("Explore Directory Disk Space (TreeMap)...", "app.disk_space_analyzer", ""));
        
        sysMenu["children"] = sysKids;
        arr.append(sysMenu);
    }

    // 7. Virtual Drives & Archives Submenu
    {
        QJsonObject archiveMenu;
        archiveMenu["type"] = "menu";
        archiveMenu["title"] = "Virtual Drives & Archives";
        archiveMenu["icon"] = "package-x-generic";
        
        QJsonArray archiveKids;
        archiveKids.append(makeAction("Vault Encryption/Decryption", "app.vault_toggle", ""));
        archiveKids.append(makeAction("ISO Virtual Drive", "app.iso_toggle", ""));
        archiveKids.append(makeAction("VHD Virtual Drive", "app.vhd_toggle", ""));
        archiveKids.append(makeSeparator());
        archiveKids.append(makeAction("Create Archive...", "app.create_archive", ""));
        archiveKids.append(makeAction("Create Secure Archive (AES-256)...", "app.create_secure_archive", ""));
        archiveKids.append(makeAction("Extract Archive...", "app.extract_archive", ""));
        archiveKids.append(makeAction("Extract Here", "app.extract_here", ""));
        archiveKids.append(makeAction("Extract to Subfolder", "app.extract_subfolder", ""));
        
        archiveMenu["children"] = archiveKids;
        arr.append(archiveMenu);
    }

    // 8. Favorites & Pins Submenu
    {
        QJsonObject favMenu;
        favMenu["type"] = "menu";
        favMenu["title"] = "Favorites & Pins";
        favMenu["icon"] = "emblem-favorite";
        
        QJsonArray favKids;
        favKids.append(makeAction("Add/Remove Favorites", "app.favorites", ""));
        favKids.append(makeAction("Pin/Unpin Home Screen", "app.pin_home", ""));
        
        favMenu["children"] = favKids;
        arr.append(favMenu);
    }
    arr.append(makeSeparator());

    // 9. Folder Layout Profiles Submenu
    {
        QJsonObject profSub;
        profSub["type"] = "menu";
        profSub["title"] = "Folder Layout Profiles";
        profSub["icon"] = "preferences-other";
        
        QJsonArray profKids;
        
        QJsonObject applySub;
        applySub["type"] = "menu";
        applySub["title"] = "Apply Profile Layout to Current Folder";
        applySub["command"] = "app.apply_profile_submenu";
        profKids.append(applySub);
        
        profKids.append(makeSeparator());
        profKids.append(makeAction("Save Current Layout as Folder Profile...", "app.save_folder_profile", ""));
        profKids.append(makeAction("Save Current Layout as Default Profile", "app.save_default_profile", ""));
        profKids.append(makeAction("Load Default Profile", "app.load_default_profile", ""));
        
        profSub["children"] = profKids;
        arr.append(profSub);
    }

    return arr;
}

QAction* FilePanel::createContextMenuAction(QMenu* parentMenu, const QJsonObject& obj, const QStringList& selected, const QModelIndex& index, QMap<QAction*, QString>& actionCommands) {
    QString type = obj["type"].toString();
    if (type == "separator") {
        parentMenu->addSeparator();
        return nullptr;
    }
    
    QStyle* style = QApplication::style();
    
    if (type == "menu") {
        QString title = obj["title"].toString();
        QJsonArray children = obj["children"].toArray();
        QString command = obj["command"].toString();
        
        QMenu* sub = nullptr;
        if (command == "app.apply_profile_submenu") {
            sub = parentMenu->addMenu("Apply Profile Layout to Current Folder");
            QWidget* parentW = parentWidget();
            while (parentW && !parentW->inherits("MainWindow")) {
                parentW = parentW->parentWidget();
            }
            MainWindow* mw = qobject_cast<MainWindow*>(parentW);
            if (mw) {
                for (const auto& r : mw->folderRules()) {
                    if (!r.name.isEmpty()) {
                        QString profileName = r.name;
                        QAction* act = sub->addAction(profileName);
                        actionCommands[act] = "app.apply_profile:" + profileName;
                    }
                }
            }
        } else {
            sub = parentMenu->addMenu(title);
            QString iconPath = obj["icon"].toString();
            if (!iconPath.isEmpty()) {
                QIcon icon = getIconForPathOrTheme(iconPath);
                if (!icon.isNull()) sub->setIcon(icon);
            }
            for (int i = 0; i < children.size(); ++i) {
                createContextMenuAction(sub, children[i].toObject(), selected, index, actionCommands);
            }
        }
        return nullptr;
    }
    
    QString title = obj["title"].toString();
    QString command = obj["command"].toString();
    QString iconPath = obj["icon"].toString();
    
    QIcon icon;
    if (!iconPath.isEmpty()) {
        icon = getIconForPathOrTheme(iconPath);
    }
    
    QAction* act = nullptr;
    
    bool isFolder = false;
    QString selectedPath;
    if (!selected.isEmpty()) {
        selectedPath = selected.first();
        isFolder = QFileInfo(selectedPath).isDir();
    }
    
    bool isTheater = (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer);
    
    if (isTheater) {
        if (command == "app.copy" || command == "app.cut" || command == "app.paste" ||
            command == "app.copy_sibling" || command == "app.move_sibling" ||
            command == "app.delete" || command == "app.rename" || command == "app.bulk_rename" ||
            command == "app.new_folder" || command == "app.advanced_new_folder" ||
            command == "app.favorites" || command == "app.pin_home" ||
            command == "app.compare_selected" || command == "app.compare_sibling" ||
            command == "app.encrypt_vault" || command == "app.decrypt_vault" || command == "app.vault_toggle" ||
            command == "app.iso_toggle" || command == "app.vhd_toggle" ||
            command == "app.create_archive" || command == "app.create_secure_archive" || command == "app.extract_archive" ||
            command == "app.calculate_checksum" || command == "app.secure_shred" || command == "app.image_convert" ||
            command == "app.disk_space_analyzer" ||
            command == "app.folder_layouts" || command == "app.save_folder_profile" ||
            command == "app.save_default_profile" || command == "app.load_default_profile" ||
            command == "app.toggle_executable" || command == "app.change_permissions" ||
            command == "app.lock_folder_recursive" || command == "app.unlock_folder_recursive" ||
            command == "app.remove_green_screen") {
            return nullptr;
        }
    }
    
    if (command == "app.open") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open");
    } else if (command == "app.open_fullscreen") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_MediaPlay), "Open in Full Screen View");
    } else if (command == "app.copy") {
        act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("edit-copy") : icon, "Copy");
        act->setShortcut(QKeySequence::Copy);
    } else if (command == "app.cut") {
        act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("edit-cut") : icon, "Cut");
        act->setShortcut(QKeySequence::Cut);
    } else if (command == "app.paste") {
        act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("edit-paste") : icon, "Paste");
        act->setShortcut(QKeySequence::Paste);
    } else if (command == "app.copy_filename") {
        act = parentMenu->addAction(icon, "Copy File Name(s)");
    } else if (command == "app.copy_path") {
        act = parentMenu->addAction(icon, "Copy Full Path(s)");
    } else if (command == "app.copy_folder_contents") {
        act = parentMenu->addAction(icon, "Copy Folder Contents (Paths List)");
    } else if (command == "app.copy_sibling") {
        act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("go-next") : icon, "Copy to Sibling Panel");
        act->setShortcut(QKeySequence(Qt::Key_F5));
    } else if (command == "app.move_sibling") {
        act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("go-jump") : icon, "Move to Sibling Panel");
        act->setShortcut(QKeySequence(Qt::Key_F6));
    } else if (command == "app.delete") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_TrashIcon), "Delete");
        act->setShortcut(QKeySequence::Delete);
    } else if (command == "app.rename") {
        act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("edit-rename") : icon, "Rename");
    } else if (command == "app.bulk_rename") {
        act = parentMenu->addAction(icon, "Bulk Rename...");
    } else if (command == "app.new_folder") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_FileDialogNewFolder), "New Folder");
    } else if (command == "app.advanced_new_folder") {
        act = parentMenu->addAction(icon, "Advanced New Folder...");
        if (m_archiveViewActive && act) act->setEnabled(false);
    } else if (command == "app.favorites") {
        if (isFolder) {
            bool isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
            if (isFavorite) {
                act = parentMenu->addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove from Favorites");
            } else {
                act = parentMenu->addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add to Favorites");
            }
        } else {
            bool isCurrentFavorite = FavoritesManager::instance().isFavorite(m_currentPath);
            if (isCurrentFavorite) {
                act = parentMenu->addAction(style->standardIcon(QStyle::SP_DialogCancelButton), "Remove Current from Favorites");
            } else {
                act = parentMenu->addAction(style->standardIcon(QStyle::SP_DialogYesButton), "Add Current to Favorites");
            }
        }
    } else if (command == "app.pin_home") {
        if (isFolder) {
            QSettings settings("Amifiles", "Amifiles");
            QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
            bool isPinned = false;
            for (const QString& item : pinned) {
                if (item.startsWith(selectedPath + ";")) {
                    isPinned = true;
                    break;
                }
            }
            if (isPinned) {
                act = parentMenu->addAction(icon, "📌 Unpin from Home Screen");
            } else {
                act = parentMenu->addAction(icon, "📌 Pin to Home Screen");
            }
        }
    } else if (command == "app.play_preview" || command == "app.play_fullscreen_playlist") {
        QString folderToCheck = isFolder ? selectedPath : m_currentPath;
        QStringList playlistPaths;
        if (!folderToCheck.isEmpty()) {
            if (isFolder) {
                QSettings settings("Amifiles", "Amifiles");
                bool groupMultiDisc = settings.value("theater/group_multi_disc", true).toBool() && (m_viewStack->currentWidget() == m_theaterContainer);
                if (groupMultiDisc) {
                    QFileInfo folderInfo(folderToCheck);
                    QString parentDir = folderInfo.absolutePath();
                    QDir dir(parentDir);
                    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                    QString currentCleaned = FileFilterProxyModel::cleanAlbumFolderName(folderInfo.fileName());
                    for (const QString& subDirName : subDirs) {
                        if (FileFilterProxyModel::cleanAlbumFolderName(subDirName) == currentCleaned) {
                            scanMediaFilesRecursively(dir.filePath(subDirName), playlistPaths, 1);
                        }
                    }
                } else {
                    scanMediaFilesRecursively(folderToCheck, playlistPaths, 1);
                }
            } else {
                QDir dir(folderToCheck);
                QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg", "mod", "sid", "s3m", "xm", "it" };
                QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
                for (const QFileInfo& fInfo : files) {
                    if (mediaExts.contains(fInfo.suffix().toLower())) {
                        playlistPaths.append(fInfo.absoluteFilePath());
                    }
                }
            }
        }
        if (!playlistPaths.isEmpty()) {
            if (command == "app.play_preview") {
                act = parentMenu->addAction(style->standardIcon(QStyle::SP_MediaPlay), "Play Folder/Album in Preview");
            } else {
                act = parentMenu->addAction(style->standardIcon(QStyle::SP_MediaPlay), "Play Folder/Album in Fullscreen");
            }
        } else {
            return nullptr;
        }
    } else if (command == "app.compare_selected") {
        act = parentMenu->addAction(icon, "Compare Selected Files");
    } else if (command == "app.compare_sibling") {
        act = parentMenu->addAction(icon, "Compare with Sibling Pane File");
    } else if (command == "app.media_info_sheet") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_MessageBoxInformation), "🎬 Media Info Sheet... (ℹ)");
    } else if (command == "app.toggle_watch") {
        act = parentMenu->addAction(icon, "✔ Toggle Watch Status (Watched/Unwatched)");
    } else if (command == "app.metadata_casing_wizard") {
        act = parentMenu->addAction(icon, "Batch Tag/Filename Casing Wizard...");
    } else if (command == "app.edit_tags") {
        act = parentMenu->addAction(icon, "Edit Audio Tags...");
    } else if (command == "app.advanced_tag_editor") {
        act = parentMenu->addAction(icon, "Advanced Tag Editor...");
    } else if (command == "app.fetch_musicbrainz") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Fetch MusicBrainz Album Info...");
    } else if (command == "app.scrape_video") {
        if (viewModeIndex() == 10) {
            act = parentMenu->addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Fetch Cover Art & Wallpaper...");
            actionCommands[act] = "app.fetch_folder_art";
        } else {
            act = parentMenu->addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Scrape Video Metadata...");
        }
    } else if (command == "app.fetch_folder_art") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Fetch Cover Art & Wallpaper...");
    } else if (command == "app.color_none") {
        act = parentMenu->addAction(icon, "None");
    } else if (command == "app.color_red") {
        act = parentMenu->addAction(icon, "Red");
    } else if (command == "app.color_orange") {
        act = parentMenu->addAction(icon, "Orange");
    } else if (command == "app.color_yellow") {
        act = parentMenu->addAction(icon, "Yellow");
    } else if (command == "app.color_green") {
        act = parentMenu->addAction(icon, "Green");
    } else if (command == "app.color_blue") {
        act = parentMenu->addAction(icon, "Blue");
    } else if (command == "app.color_purple") {
        act = parentMenu->addAction(icon, "Purple");
    } else if (command == "app.color_custom_overlay") {
        act = parentMenu->addAction(icon, "Custom Icon Overlay...");
    } else if (command == "app.color_clear_overlay") {
        act = parentMenu->addAction(icon, "Clear Icon Overlay");
    } else if (command == "app.file_tags") {
        act = parentMenu->addAction(icon, "File Tags...");
    } else if (command == "app.vault_toggle") {
        if (index.isValid()) {
            QString selectedExt = QFileInfo(selectedPath).suffix().toLower();
            if (selectedExt == "vault") {
                act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("object-unlocked") : icon, "Decrypt Secure Vault...");
                actionCommands[act] = "app.decrypt_vault";
            } else {
                act = parentMenu->addAction(icon.isNull() ? QIcon::fromTheme("object-locked") : icon, "Encrypt into Secure Vault...");
                actionCommands[act] = "app.encrypt_vault";
            }
        }
    } else if (command == "app.iso_toggle") {
        if (index.isValid()) {
            QString selectedExt = QFileInfo(selectedPath).suffix().toLower();
            if (selectedExt == "iso") {
                QString dummy;
                if (RemoteMountManager::isIsoMounted(selectedPath, dummy)) {
                    act = parentMenu->addAction(style->standardIcon(QStyle::SP_DriveCDIcon), "Unmount ISO Virtual Drive");
                    actionCommands[act] = "app.unmount_iso";
                } else {
                    act = parentMenu->addAction(style->standardIcon(QStyle::SP_DriveCDIcon), "Mount ISO as Virtual Drive");
                    actionCommands[act] = "app.mount_iso";
                }
            }
        }
    } else if (command == "app.vhd_toggle") {
        if (index.isValid()) {
            QString selectedExt = QFileInfo(selectedPath).suffix().toLower();
            if (selectedExt == "vhd" || selectedExt == "vhdx") {
                QString dummy;
                if (RemoteMountManager::isVhdMounted(selectedPath, dummy)) {
                    act = parentMenu->addAction(style->standardIcon(QStyle::SP_DriveHDIcon), "Unmount VHD Virtual Drive");
                    actionCommands[act] = "app.unmount_vhd";
                } else {
                    act = parentMenu->addAction(style->standardIcon(QStyle::SP_DriveHDIcon), "Mount VHD as Virtual Drive");
                    actionCommands[act] = "app.mount_vhd";
                }
            }
        }
    } else if (command == "app.create_archive") {
        act = parentMenu->addAction(icon, "Create Archive...");
    } else if (command == "app.create_secure_archive") {
        act = parentMenu->addAction(icon, "Create Secure Archive (AES-256)...");
    } else if (command == "app.extract_archive") {
        act = parentMenu->addAction(icon, "Extract Archive...");
    } else if (command == "app.calculate_checksum") {
        act = parentMenu->addAction(icon, "Calculate Checksum Hash...");
    } else if (command == "app.secure_shred") {
        act = parentMenu->addAction(style->standardIcon(QStyle::SP_TrashIcon), "Secure Shred (Delete Permanently)...");
    } else if (command == "app.disk_space_analyzer") {
        act = parentMenu->addAction(icon, "Explore Directory Disk Space (TreeMap)...");
    } else if (command == "app.image_convert") {
        QStringList imageExts = { "jpg", "jpeg", "png", "webp", "bmp" };
        QStringList selectedImages;
        for (const QString& sPath : selected) {
            if (imageExts.contains(QFileInfo(sPath).suffix().toLower())) {
                selectedImages.append(sPath);
            }
        }
        if (!selectedImages.isEmpty()) {
            act = parentMenu->addAction(icon, "Batch Convert/Resize Images...");
        }
    } else if (command == "app.folder_layouts") {
        act = parentMenu->addAction(icon, "Folder Profiles & Layouts...");
    } else if (command == "app.save_folder_profile") {
        act = parentMenu->addAction(icon, "Save Current Layout as Folder Profile...");
    } else if (command == "app.save_default_profile") {
        act = parentMenu->addAction(icon, "Save Current Layout as Default Profile");
    } else if (command == "app.load_default_profile") {
        act = parentMenu->addAction(icon, "Load Default Profile");
    } else if (command == "app.new_file_txt") {
        act = parentMenu->addAction(icon, "Text Document (.txt)");
    } else if (command == "app.new_file_md") {
        act = parentMenu->addAction(icon, "Markdown Document (.md)");
    } else if (command == "app.new_file_html") {
        act = parentMenu->addAction(icon, "HTML Document (.html)");
    } else if (command == "app.new_file_py") {
        act = parentMenu->addAction(icon, "Python Script (.py)");
    } else if (command == "app.new_file_png") {
        act = parentMenu->addAction(icon, "Blank PNG Image (.png)");
    } else if (command == "app.toggle_executable") {
        if (!selected.isEmpty() && !isFolder) {
            act = parentMenu->addAction(icon, "Toggle Executable Status");
        }
    } else if (command == "app.change_permissions") {
        if (!selected.isEmpty()) {
            act = parentMenu->addAction(icon, "Change File Permissions (chmod)...");
        }
    } else if (command == "app.lock_folder_recursive") {
        if (!selected.isEmpty()) {
            act = parentMenu->addAction(icon, "Lock Folder/File (Prevent Deletion)");
        }
    } else if (command == "app.unlock_folder_recursive") {
        if (!selected.isEmpty()) {
            act = parentMenu->addAction(icon, "Unlock Folder/File");
        }
    } else if (command == "app.create_symlink_sibling") {
        if (m_siblingPanel) {
            act = parentMenu->addAction(icon, "🔗 Create Symlink in Sibling Pane");
        }
    } else if (command == "app.remove_green_screen") {
        QStringList imageExts = { "jpg", "jpeg", "png", "webp", "bmp" };
        bool hasImage = false;
        for (const QString& sPath : selected) {
            if (imageExts.contains(QFileInfo(sPath).suffix().toLower())) {
                hasImage = true;
                break;
            }
        }
        if (hasImage) {
            act = parentMenu->addAction(icon, "Remove Green Screen 🟢");
        }
    } else {
        act = parentMenu->addAction(icon, title);
    }
    
    if (act) {
        if (!actionCommands.contains(act)) {
            actionCommands[act] = command;
        }
    }
    
    return act;
}

void FilePanel::populateFilterTagsCombo() {
    if (!m_comboFilterTag) return;
    m_comboFilterTag->blockSignals(true);
    QString currentSelected = m_comboFilterTag->currentData().toString();
    
    m_comboFilterTag->clear();
    m_comboFilterTag->addItem("All Tags", "");
    
    QStringList allTags = TagManager::instance().getAllTags();
    allTags.sort(Qt::CaseInsensitive);
    for (const QString& tag : allTags) {
        if (!tag.trimmed().isEmpty()) {
            m_comboFilterTag->addItem(tag, tag);
        }
    }
    
    int index = m_comboFilterTag->findData(currentSelected);
    if (index != -1) {
        m_comboFilterTag->setCurrentIndex(index);
    } else {
        m_comboFilterTag->setCurrentIndex(0);
    }
    m_comboFilterTag->blockSignals(false);
}

void FilePanel::onRatingFilterClicked() {
    QToolButton* clicked = qobject_cast<QToolButton*>(sender());
    if (!clicked) return;

    bool isChecked = clicked->isChecked();

    if (clicked == m_btnRateAll) {
        if (!isChecked) {
            // Force "All" to remain checked if the user tries to uncheck it directly
            m_btnRateAll->setChecked(true);
        } else {
            // Uncheck all star buttons
            for (QToolButton* btn : m_btnStars) {
                btn->blockSignals(true);
                btn->setChecked(false);
                btn->blockSignals(false);
            }
        }
    } else {
        if (isChecked) {
            // Uncheck "All" and all other star buttons
            m_btnRateAll->blockSignals(true);
            m_btnRateAll->setChecked(false);
            m_btnRateAll->blockSignals(false);

            for (QToolButton* btn : m_btnStars) {
                if (btn != clicked) {
                    btn->blockSignals(true);
                    btn->setChecked(false);
                    btn->blockSignals(false);
                }
            }
        } else {
            // Checked star button was toggled off, so revert to "All"
            m_btnRateAll->blockSignals(true);
            m_btnRateAll->setChecked(true);
            m_btnRateAll->blockSignals(false);
        }
    }

    int rating = -1;
    for (QToolButton* btn : m_btnStars) {
        if (btn->isChecked()) {
            rating = btn->property("ratingValue").toInt();
            break;
        }
    }

    if (m_proxyModel) m_proxyModel->setRatingFilter(rating);
    if (m_flatProxyModel) m_flatProxyModel->setRatingFilter(rating);

    updateStatusText();
}

void FilePanel::onTagFilterComboChanged(int index) {
    if (!m_comboFilterTag) return;
    QString tag = m_comboFilterTag->itemData(index).toString();
    
    if (m_proxyModel) m_proxyModel->setTagFilter(tag);
    if (m_flatProxyModel) m_flatProxyModel->setTagFilter(tag);
    
    updateStatusText();
}

void FilePanel::onCommentFilterChanged(const QString& text) {
    if (m_proxyModel) m_proxyModel->setCommentFilter(text);
    if (m_flatProxyModel) m_flatProxyModel->setCommentFilter(text);
    updateStatusText();
}

void FilePanel::onCloseTagsRatingsFilterBar() {
    if (m_btnToggleTRFilter) {
        m_btnToggleTRFilter->setChecked(false); // Triggers onToggleTagsRatingsFilterBar()
    }
}

void FilePanel::onToggleTagsRatingsFilterBar() {
    bool checked = m_btnToggleTRFilter ? m_btnToggleTRFilter->isChecked() : false;
    
    if (m_tagsRatingsFilterWidget) {
        m_tagsRatingsFilterWidget->setVisible(checked);
    }
    
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/show_tags_ratings_filter_bar", checked);
    
    if (checked) {
        populateFilterTagsCombo();
    } else {
        // Clear filters when closing the bar
        if (m_btnRateAll) m_btnRateAll->setChecked(true);
        if (m_comboFilterTag) m_comboFilterTag->setCurrentIndex(0);
        if (m_editFilterComment) m_editFilterComment->clear();
        
        if (m_proxyModel) {
            m_proxyModel->setRatingFilter(-1);
            m_proxyModel->setTagFilter("");
            m_proxyModel->setCommentFilter("");
        }
        if (m_flatProxyModel) {
            m_flatProxyModel->setRatingFilter(-1);
            m_flatProxyModel->setTagFilter("");
            m_flatProxyModel->setCommentFilter("");
        }
        updateStatusText();
    }
}

