#include "filepanel.h"
#include "mainwindow.h"
#include <QDebug>
#include "theme.h"
#include "favoritesmanager.h"
#include "archivemodel.h"
#include "smartfoldermodel.h"
#include "diskdashboardwidget.h"
#include "millercolumnsview.h"
#include "timelineview.h"
#include "filmstripview.h"
#include "cardviewdelegate.h"
#include "groupproxymodel.h"
#include "columnscustomizerdialog.h"
#include <QComboBox>
#include "searchworker.h"
#include "metadataextractor.h"
#include "bulkrename.h"
#include "diffdialog.h"
#include "tageditordialog.h"
#include "filetagsdialog.h"
#include "iconpickerdialog.h"
#include "theaterviewdelegate.h"
#include "theaterlistview.h"
#include "videoscraperdialog.h"
#include "folderartscraperdialog.h"
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
#include <QStyle>
#include <QApplication>
#include <QDir>
#include <QSet>

static void scanMediaFilesRecursively(const QString& folderPath, QStringList& playlistPaths, int depth = 0);
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

#include <QPainter>
#include <QPen>

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

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("Enter folder path...");
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &FilePanel::onPathEntered);

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
    m_btnHome->setToolTip("Go Home (Right-click to set current folder as Home)");
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
        "Theater View"
    });
    m_comboViewMode->setToolTip("Switch active file listing visual layout view mode");
    m_comboViewMode->setStyleSheet("QComboBox { background-color: #313244; color: #89b4fa; border: 1px solid #45475a; border-radius: 4px; padding: 2px 6px; font-weight: bold; }");
    connect(m_comboViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilePanel::onViewModeChanged);

    navLayout->addWidget(m_btnBack);
    navLayout->addWidget(m_btnForward);
    navLayout->addWidget(m_btnUp);
    navLayout->addWidget(m_btnHome);
    navLayout->addWidget(m_btnFavorite);
    navLayout->addWidget(m_pathEdit, 1);
    navLayout->addWidget(m_btnGo);
    navLayout->addWidget(m_btnClonePath);
    navLayout->addWidget(m_btnFlatView);
    navLayout->addWidget(m_comboViewMode);

    // Central Tree View
    m_treeView = new QTreeView(this);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSortingEnabled(true);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu); // Enable context menu
    m_treeView->installEventFilter(this); // Install event filter to capture focus events
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(false);
    m_treeView->setDropIndicatorShown(false);
    m_treeView->setDragDropMode(QAbstractItemView::DragOnly);

    // Icon Grid List View
    m_listView = new QListView(this);
    m_defaultDelegate = m_listView->itemDelegate();
    m_cardDelegate = new CardViewDelegate(m_listView);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setWordWrap(true);
    m_listView->setUniformItemSizes(true);
    m_listView->setSpacing(10);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->installEventFilter(this);
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(false);
    m_listView->setDropIndicatorShown(false);
    m_listView->setDragDropMode(QAbstractItemView::DragOnly);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->addWidget(m_listView);

    // Global Search UI Setup

    m_searchResultsView = new QListView(this);
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
    m_fileModel->setRootPath("");

    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileModel);

    m_groupProxy = new GroupProxyModel(this);
    m_groupProxy->setSourceModel(m_proxyModel);

    m_flatModel = new FlatFileSystemModel(this);
    m_flatProxyModel = new QSortFilterProxyModel(this);
    m_flatProxyModel->setSourceModel(m_flatModel);
    m_flatProxyModel->setFilterKeyColumn(0);
    m_flatProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_smartModel = new SmartFolderModel(this);
    m_archiveModel = new ArchiveModel(this);
    m_dashboardWidget = new DiskDashboardWidget(this);
    m_theaterListView = new TheaterListView(this);
    m_theaterListView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_theaterDelegate = new TheaterViewDelegate(m_theaterListView);
    m_theaterListView->setItemDelegate(m_theaterDelegate);
    m_theaterListView->installEventFilter(this);
    m_theaterListView->setDragEnabled(true);
    m_theaterListView->setAcceptDrops(false);
    m_theaterListView->setDropIndicatorShown(false);
    m_theaterListView->setDragDropMode(QAbstractItemView::DragOnly);

    // Slide-out tracks drawer container
    m_theaterContainer = new QWidget(this);
    QHBoxLayout* theaterLayout = new QHBoxLayout(m_theaterContainer);
    theaterLayout->setContentsMargins(0, 0, 0, 0);
    theaterLayout->setSpacing(0);
    
    m_theaterScrollArea = new QScrollArea(m_theaterContainer);
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

    theaterLayout->addWidget(m_theaterListView, 1);
    theaterLayout->addWidget(m_theaterScrollArea, 1);

    m_theaterDrawer = new QWidget(m_theaterContainer);
    m_theaterDrawer->setFixedWidth(280);
    m_theaterDrawer->setStyleSheet("QWidget { background-color: #181825; border-left: 1px solid #313244; }");
    m_theaterDrawer->setVisible(false);

    QVBoxLayout* drawerLayout = new QVBoxLayout(m_theaterDrawer);
    drawerLayout->setContentsMargins(8, 8, 8, 8);
    drawerLayout->setSpacing(8);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    m_drawerTitle = new QLabel("Album Tracks", m_theaterDrawer);
    m_drawerTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #89b4fa;");
    m_drawerCloseBtn = new QPushButton("✕", m_theaterDrawer);
    m_drawerCloseBtn->setFixedSize(24, 24);
    m_drawerCloseBtn->setStyleSheet("QPushButton { background-color: transparent; color: #f38ba8; font-weight: bold; border: none; } QPushButton:hover { color: #f38ba8; background-color: #313244; border-radius: 4px; }");
    headerLayout->addWidget(m_drawerTitle, 1);
    headerLayout->addWidget(m_drawerCloseBtn);
    drawerLayout->addLayout(headerLayout);

    m_drawerList = new QListWidget(m_theaterDrawer);
    m_drawerList->setStyleSheet("QListWidget { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; } QListWidget::item { padding: 6px; } QListWidget::item:hover { background-color: #313244; } QListWidget::item:selected { background-color: #89b4fa; color: #11111b; }");
    drawerLayout->addWidget(m_drawerList, 1);

    m_drawerPlayBtn = new QPushButton("Play Album", m_theaterDrawer);
    m_drawerPlayBtn->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; padding: 8px; border-radius: 4px; } QPushButton:hover { background-color: #94e2d5; }");
    drawerLayout->addWidget(m_drawerPlayBtn);

    theaterLayout->addWidget(m_theaterDrawer);

    // Connections for drawer
    connect(m_drawerCloseBtn, &QPushButton::clicked, this, [this]() {
        m_theaterDrawer->setVisible(false);
    });

    connect(m_drawerPlayBtn, &QPushButton::clicked, this, [this]() {
        QStringList playlistPaths;
        for (int i = 0; i < m_drawerList->count(); ++i) {
            QListWidgetItem* item = m_drawerList->item(i);
            QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) playlistPaths.append(path);
        }
        if (!playlistPaths.isEmpty()) {
            emit playMediaBuiltinRequested(playlistPaths);
        }
    });

    connect(m_drawerList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            emit playMediaBuiltinRequested({path});
        }
    });

    m_theaterContainer->installEventFilter(this);
    m_theaterDrawer->installEventFilter(this);
    m_drawerList->installEventFilter(this);

    m_millerView = new MillerColumnsView(m_fileModel, this);
    m_millerView->installEventFilter(this);

    m_timelineView = new TimelineView(this);
    m_timelineView->installEventFilter(this);

    m_filmstripView = new FilmstripView(m_fileModel, this);
    m_filmstripView->installEventFilter(this);

    m_viewStack->addWidget(m_theaterContainer);
    m_viewStack->addWidget(m_millerView);
    m_viewStack->addWidget(m_timelineView);
    m_viewStack->addWidget(m_filmstripView);
    m_viewStack->addWidget(m_dashboardWidget);

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
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_listView, &QListView::doubleClicked, this, &FilePanel::onDoubleClicked);
    connect(m_listView, &QListView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_theaterListView, &QListView::doubleClicked, this, &FilePanel::onDoubleClicked);
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

    // Button group to make selection mutually exclusive
    QButtonGroup* filterGroup = new QButtonGroup(this);
    filterGroup->setExclusive(true);
    filterGroup->addButton(m_btnFilterAll);
    filterGroup->addButton(m_btnFilterAudio);
    filterGroup->addButton(m_btnFilterVideos);
    filterGroup->addButton(m_btnFilterPictures);
    filterGroup->addButton(m_btnFilterDocs);
    filterGroup->addButton(m_btnFilterArchive);

    connect(m_btnFilterAll, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterAudio, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterVideos, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterPictures, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterDocs, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);
    connect(m_btnFilterArchive, &QToolButton::clicked, this, &FilePanel::onFilterTypeChanged);

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

    m_btnToggleSearchMode = new QToolButton(this);
    m_btnToggleSearchMode->setIcon(createSearchIcon(QColor("#cdd6f4")));
    m_btnToggleSearchMode->setToolTip("Switch to Search Mode");
    m_btnToggleSearchMode->setStyleSheet("QToolButton { background-color: transparent; border: none; }");
    connect(m_btnToggleSearchMode, &QToolButton::clicked, this, &FilePanel::onToggleSearchFilterMode);
    categoryLayout->addWidget(m_btnToggleSearchMode);

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
        "Group by Custom Text..."
    });
    m_comboGrouping->setFixedWidth(160);
    m_comboGrouping->setToolTip("Group Files in List View");
    connect(m_comboGrouping, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilePanel::onGroupingChanged);
    statusLayout->addWidget(m_comboGrouping);

    QVBoxLayout* bottomLayout = new QVBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);
    bottomLayout->addWidget(m_categoryWidget);
    bottomLayout->addWidget(m_filterTextWidget);
    bottomLayout->addWidget(m_statusWidget);

    mainLayout->addWidget(m_navContainer);
    mainLayout->addWidget(m_searchResultsView, 1);
    mainLayout->addWidget(m_viewStack, 1);
    mainLayout->addLayout(bottomLayout);

    int initialZoom = settings.value("file_panel/zoom_level", 1).toInt();
    m_zoomSlider->setValue(initialZoom);
    onZoomChanged(initialZoom);

    setActive(false);
    updateNavigationButtons();
}

bool FilePanel::eventFilter(QObject* watched, QEvent* event) {
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
    if (watched == m_treeView || watched == m_listView || watched == m_millerView || watched == m_timelineView || watched == m_filmstripView || watched == m_theaterListView || watched == m_theaterContainer || watched == m_theaterDrawer || watched == m_drawerList) {
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
                    QString destDir = m_currentPath;
                    
                    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(watched);
                    if (view) {
                        QModelIndex index = view->indexAt(dropEvent->position().toPoint());
                        if (index.isValid()) {
                            QModelIndex srcIndex = index;
                            if (watched == m_treeView) {
                                srcIndex = m_proxyModel->mapToSource(index);
                            } else if (watched == m_listView && m_flatViewEnabled) {
                                srcIndex = m_flatProxyModel->mapToSource(index);
                            } else if (watched == m_listView) {
                                srcIndex = m_proxyModel->mapToSource(index);
                            }
                            
                            if (m_fileModel && m_fileModel->isDir(srcIndex)) {
                                destDir = m_fileModel->filePath(srcIndex);
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
                        CopyQueueManager::instance().queueCopy(srcPaths, destDir, true, this);
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

void FilePanel::setCustomBgColor(const QString& hexColor) {
    if (m_customBgColor != hexColor) {
        m_customBgColor = hexColor;
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
    } else {
        m_treeView->setModel(base);
        m_listView->setModel(base);
        m_theaterListView->setModel(base);
        m_filmstripView->setModel(base);
    }
    m_listView->setSelectionModel(m_treeView->selectionModel());
    m_theaterListView->setSelectionModel(m_treeView->selectionModel());
    
    if (m_treeView->selectionModel()) {
        connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
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

void FilePanel::navigateTo(const QString& path, bool addHistory) {
    if (m_isPathLocked && path != m_lockedPath) {
        emit openNewTabRequested(path);
        return;
    }

    if (m_isPathLockedWithSubdirs && !path.startsWith(m_lockedPath, Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Locked Path", "This tab is locked to the current folder hierarchy.");
        return;
    }

    if (m_isSearchModeActive) {
        onToggleSearchFilterMode();
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
    } else if (m_archiveViewActive || m_smartViewActive || m_dashboardActive) {
        m_archiveViewActive = false;
        m_smartViewActive = false;
        m_dashboardActive = false;

        // Restore active view stack widget
        onViewModeChanged(viewModeIndex());

        updateActiveViewModel();

        if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);

        // Restore interactive resizing for all columns
        for (int i = 0; i < 4; ++i) {
            m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
        }
    }

    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    m_currentPath = dir.absolutePath();
    m_pathEdit->setText(QDir::toNativeSeparators(m_currentPath));

    // Reset filter to All if target directory has no matching files for the current category
    FileFilterProxyModel::FilterType currentFilter = m_proxyModel->filterType();
    if (currentFilter != FileFilterProxyModel::FilterAll) {
        QStringList filters;
        if (currentFilter == FileFilterProxyModel::FilterAudio) {
            filters = { "*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.wma", "*.aac", "*.mid", "*.midi" };
        } else if (currentFilter == FileFilterProxyModel::FilterVideos) {
            filters = { "*.mp4", "*.avi", "*.mkv", "*.mov", "*.webm", "*.flv", "*.wmv", "*.m4v", "*.mpg", "*.mpeg" };
        } else if (currentFilter == FileFilterProxyModel::FilterPictures) {
            filters = { "*.png", "*.jpg", "*.jpeg", "*.gif", "*.bmp", "*.webp", "*.svg", "*.tiff", "*.ico" };
        } else if (currentFilter == FileFilterProxyModel::FilterDocs) {
            filters = { "*.txt", "*.log", "*.pdf", "*.doc", "*.docx", "*.xls", "*.xlsx", "*.ppt", "*.pptx", "*.odt", "*.ods", "*.odp", "*.md", "*.csv", "*.rtf", "*.html", "*.xml", "*.json" };
        } else if (currentFilter == FileFilterProxyModel::FilterArchive) {
            filters = { "*.zip", "*.tar", "*.gz", "*.bz2", "*.xz", "*.rar", "*.7z", "*.tgz" };
        }

        QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
        if (list.isEmpty()) {
            m_btnFilterAll->setChecked(true);
            m_proxyModel->setFilterType(FileFilterProxyModel::FilterAll);
        }
    }

    // Update tree view root
    if (m_flatViewEnabled) {
        m_flatModel->setRootPath(m_currentPath);
        m_groupProxy->setSourceRoot(QModelIndex());
        m_treeView->setRootIndex(QModelIndex());
        m_listView->setRootIndex(QModelIndex());
        m_theaterListView->setRootIndex(QModelIndex());
    } else {
        m_proxyModel->setCurrentPath(m_currentPath);
        QModelIndex srcIndex = m_fileModel->setRootPath(m_currentPath);
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            m_groupProxy->setSourceRoot(proxyIndex);
            QModelIndex groupRootIndex = m_groupProxy->mapFromSource(proxyIndex);
            m_treeView->setRootIndex(groupRootIndex);
            m_listView->setRootIndex(groupRootIndex);
            m_theaterListView->setRootIndex(groupRootIndex);
            
            if (m_viewStack->currentWidget() == m_theaterContainer) {
                m_theaterListView->setVisible(false);
                m_theaterScrollArea->setVisible(true);
                rebuildTheaterGroups();
            }
        } else {
            m_treeView->setRootIndex(proxyIndex);
            m_listView->setRootIndex(proxyIndex);
            m_theaterListView->setRootIndex(proxyIndex);
            
            m_theaterListView->setVisible(true);
            m_theaterScrollArea->setVisible(false);
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
    updateStatusText();

    emit pathChanged(m_currentPath);

    // Since we navigated, trigger selection check
    onSelectionChanged();
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
    QString homePath = settings.value("preferences/home_path", QDir::homePath()).toString();
    navigateTo(homePath);
}

void FilePanel::onHomeContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
    );
    QAction* actSetHome = menu.addAction("Set Current Folder as Home");
    QAction* selected = menu.exec(m_btnHome->mapToGlobal(pos));
    if (selected == actSetHome) {
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
            m_siblingPanel->m_flatProxyModel->setFilterFixedString(filterText);
        } else {
            m_siblingPanel->proxyModel()->setFilterText(filterText);
        }
        m_siblingPanel->updateStatusText();
        m_siblingPanel->syncFilterText(filterText);
    } else {
        if (m_flatViewEnabled) {
            m_flatProxyModel->setFilterFixedString(filterText);
        } else {
            m_proxyModel->setFilterText(filterText);
        }
        updateStatusText();
    }
}

void FilePanel::onFilterTypeChanged() {
    FileFilterProxyModel::FilterType targetType = FileFilterProxyModel::FilterAll;
    
    if (m_btnFilterAudio->isChecked()) {
        targetType = FileFilterProxyModel::FilterAudio;
    } else if (m_btnFilterVideos->isChecked()) {
        targetType = FileFilterProxyModel::FilterVideos;
    } else if (m_btnFilterPictures->isChecked()) {
        targetType = FileFilterProxyModel::FilterPictures;
    } else if (m_btnFilterDocs->isChecked()) {
        targetType = FileFilterProxyModel::FilterDocs;
    } else if (m_btnFilterArchive->isChecked()) {
        targetType = FileFilterProxyModel::FilterArchive;
    }

    FilePanel* targetPanel = this;
    if (m_siblingPanel && !m_isActive && !m_siblingPanel->isCategoryButtonsVisible()) {
        targetPanel = m_siblingPanel;
    }

    if (targetType != FileFilterProxyModel::FilterAll) {
        QStringList filters;
        if (targetType == FileFilterProxyModel::FilterAudio) {
            filters = { "*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.wma", "*.aac", "*.mid", "*.midi" };
        } else if (targetType == FileFilterProxyModel::FilterVideos) {
            filters = { "*.mp4", "*.avi", "*.mkv", "*.mov", "*.webm", "*.flv", "*.wmv", "*.m4v", "*.mpg", "*.mpeg" };
        } else if (targetType == FileFilterProxyModel::FilterPictures) {
            filters = { "*.png", "*.jpg", "*.jpeg", "*.gif", "*.bmp", "*.webp", "*.svg", "*.tiff", "*.ico" };
        } else if (targetType == FileFilterProxyModel::FilterDocs) {
            filters = { "*.txt", "*.log", "*.pdf", "*.doc", "*.docx", "*.xls", "*.xlsx", "*.ppt", "*.pptx", "*.odt", "*.ods", "*.odp", "*.md", "*.csv", "*.rtf", "*.html", "*.xml", "*.json" };
        } else if (targetType == FileFilterProxyModel::FilterArchive) {
            filters = { "*.zip", "*.tar", "*.gz", "*.bz2", "*.xz", "*.rar", "*.7z", "*.tgz" };
        }

        QDir dir(targetPanel->currentPath());
        QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
        if (list.isEmpty()) {
            m_btnFilterAll->setChecked(true);
            targetPanel->proxyModel()->setFilterType(FileFilterProxyModel::FilterAll);
            m_statusLabel->setText("No matching files found. Showing all.");
            return;
        }
    }

    targetPanel->proxyModel()->setFilterType(targetType);
    targetPanel->updateStatusText();

    if (targetPanel != this) {
        targetPanel->syncFilterType(targetType);
    }
}

static bool containsMediaFilesDirectly(const QString& folderPath) {
    QDir dir(folderPath);
    QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg" };
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
            mappedIndex = m_groupProxy->mapToSource(index);
        }
        QModelIndex srcIndex = m_proxyModel->mapToSource(mappedIndex);
        path = m_fileModel->filePath(srcIndex);
    }

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

    bool shouldPlayOnDoubleclick = builtinPlayerDoubleclick;

    if (info.isDir()) {
        if (shouldPlayOnDoubleclick && isPlayableAlbumFolder(path)) {
            QStringList playlistPaths;
            scanMediaFilesRecursively(path, playlistPaths);
            if (!playlistPaths.isEmpty()) {
                emit playMediaBuiltinRequested(playlistPaths);
                return;
            }
        }
        if (m_viewStack->currentWidget() == m_treeView) {
            m_treeView->collapse(index);
        }
        navigateTo(path, true);
    } else {
        QString ext = info.suffix().toLower();
        QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm" };
        if (builtinPlayerDoubleclick && mediaExts.contains(ext)) {
            emit playMediaBuiltinRequested({path});
            return;
        }

        QSettings settings("Amifiles", "Amifiles");
        bool archiveNavEnabled = settings.value("preferences/archive_nav", true).toBool();
        QStringList archiveExts = { "zip", "tar", "gz", "xz", "bz2", "tgz", "rar", "7z", "adf", "d64", "iso", "img" };

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
                
                // Use interactive resizing for archive view columns
                for (int i = 0; i < 4; ++i) {
                    m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
                }

                if (m_categoryWidget) m_categoryWidget->hide();

                emit pathChanged(m_currentPath);
                return;
            }
        }

        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
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

        // Update slide-out tracks list drawer
        if (!paths.isEmpty() && QFileInfo(paths.first()).isDir()) {
            QString path = paths.first();
            m_drawerFolderPath = path;
            m_drawerTitle->setText(QFileInfo(path).fileName());
            m_drawerList->clear();

            QDir dir(path);
            QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "wma", "aac", "mp4", "mkv", "avi", "mov", "webm" };
            QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
            for (const QFileInfo& fInfo : files) {
                if (mediaExts.contains(fInfo.suffix().toLower())) {
                    QListWidgetItem* item = new QListWidgetItem(fInfo.fileName(), m_drawerList);
                    item->setData(Qt::UserRole, fInfo.absoluteFilePath());
                }
            }

            if (m_drawerList->count() > 0) {
                m_theaterDrawer->setVisible(true);
            } else {
                m_theaterDrawer->setVisible(false);
            }
        } else {
            m_theaterDrawer->setVisible(false);
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
    if (active == m_theaterListView || active == m_theaterContainer) {
        selModel = m_theaterListView->selectionModel();
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
                paths.append(m_archiveModel->entryPath(mappedIndex));
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
    if (m_fileModel) {
        m_fileModel->clearCache();
    }
    if (m_proxyModel) {
        m_proxyModel->clearCasingCache();
    }
    if (m_flatViewEnabled) {
        m_flatModel->setRootPath(m_currentPath);
    } else {
        QModelIndex srcIndex = m_fileModel->index(m_currentPath);
        m_fileModel->setRootPath("");
        m_fileModel->setRootPath(m_currentPath);
    }
    checkFolderArt();
    updateStatusText();
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
    if (!mimeData || !mimeData->hasUrls()) return;

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

    // Queue copy/move operations
    CopyQueueManager::instance().queueCopy(srcPaths, m_currentPath, isCut, this);

    if (isCut) {
        clipboard->clear();
    }
}

void FilePanel::onDelete() {
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

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

        for (const QString& path : paths) {
            QFileInfo info(path);
            if (info.isDir()) {
                QDir(path).removeRecursively();
            } else {
                QFile::remove(path);
            }
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

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename File", 
                                            "Enter new name:", QLineEdit::Normal,
                                            oldName, &ok);
    if (ok && !newName.isEmpty() && newName != oldName) {
        QString newPath = info.dir().filePath(newName);
        if (QFile::rename(oldPath, newPath)) {
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
        QDir dir(m_currentPath);
        if (dir.mkdir(folderName)) {
            refresh();
        } else {
            QMessageBox::warning(this, "Error", "Could not create folder.");
        }
    }
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

static void scanMediaFilesRecursively(const QString& folderPath, QStringList& playlistPaths, int depth) {
    if (depth > 3) return;
    QFileInfo fi(folderPath);
    if (fi.isSymLink()) return;

    QDir dir(folderPath);
    QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg" };
    
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fInfo : files) {
        if (fInfo.isSymLink()) continue;
        if (mediaExts.contains(fInfo.suffix().toLower())) {
            playlistPaths.append(fInfo.absoluteFilePath());
        }
    }
    
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& sub : subdirs) {
        scanMediaFilesRecursively(sub.absoluteFilePath(), playlistPaths, depth + 1);
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
        if (ext == "mp3" || ext == "flac") return true;
    }

    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& sub : subdirs) {
        if (hasAudioFilesRecursively(sub.absoluteFilePath(), depth + 1)) return true;
    }
    return false;
}

void FilePanel::onCustomContextMenu(const QPoint& pos) {
    QModelIndex index;
    if (m_viewStack->currentWidget() == m_listView) {
        index = m_listView->indexAt(pos);
    } else if (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer) {
        index = m_theaterListView->indexAt(pos);
    } else {
        index = m_treeView->indexAt(pos);
    }
    
    if (index.isValid()) {
        QWidget* cur = m_viewStack->currentWidget();
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
    
    QMenu menu(this);
    QStyle* style = QApplication::style();
    bool isTheater = (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer);

    QAction* actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open");
    menu.addSeparator();
    
    QAction* actCopy = nullptr;
    QAction* actCut = nullptr;
    QAction* actPaste = nullptr;
    QAction* actCopyToSibling = nullptr;
    QAction* actMoveToSibling = nullptr;
    QAction* actDelete = nullptr;
    QAction* actRename = nullptr;
    QAction* actBulkRename = nullptr;
    QAction* actNewFolder = nullptr;

    if (!isTheater) {
        actCopy = menu.addAction(style->standardIcon(QStyle::SP_DialogSaveButton), "Copy");
        actCopy->setShortcut(QKeySequence::Copy);
        
        actCut = menu.addAction("Cut");
        actCut->setShortcut(QKeySequence::Cut);
        
        actPaste = menu.addAction("Paste");
        actPaste->setShortcut(QKeySequence::Paste);

        actCopyToSibling = menu.addAction("Copy to Sibling Panel");
        actCopyToSibling->setShortcut(QKeySequence(Qt::Key_F5));

        actMoveToSibling = menu.addAction("Move to Sibling Panel");
        actMoveToSibling->setShortcut(QKeySequence(Qt::Key_F6));
        
        actDelete = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), "Delete");
        actDelete->setShortcut(QKeySequence::Delete);
        
        actRename = menu.addAction("Rename");
        actBulkRename = menu.addAction("Bulk Rename...");
        menu.addSeparator();
        
        actNewFolder = menu.addAction(style->standardIcon(QStyle::SP_FileDialogNewFolder), "New Folder");
        menu.addSeparator();
    }
    menu.addSeparator();
    
    QStringList curSelected = selectedPaths();
    QString selectedPath;
    bool isFolder = false;
    bool isFavorite = false;

    QWidget* activeViewWidget = m_viewStack->currentWidget();
    bool isNewView = (activeViewWidget == m_millerView || activeViewWidget == m_timelineView || activeViewWidget == m_filmstripView || activeViewWidget == m_theaterListView || activeViewWidget == m_theaterContainer);

    if (isNewView) {
        if (!curSelected.isEmpty()) {
            selectedPath = curSelected.first();
            QFileInfo info(selectedPath);
            isFolder = info.isDir();
            isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
        }
    } else {
        if (index.isValid()) {
            if (m_archiveViewActive) {
                selectedPath = m_archiveModel->entryPath(index);
                isFolder = m_archiveModel->isDir(index);
            } else if (m_flatViewEnabled) {
                QModelIndex srcIndex = m_flatProxyModel->mapToSource(index);
                selectedPath = m_flatModel->filePath(srcIndex);
            } else {
                QModelIndex srcIndex = m_proxyModel->mapToSource(index);
                selectedPath = m_fileModel->filePath(srcIndex);
            }
            QFileInfo info(selectedPath);
            if (info.isDir()) {
                isFolder = true;
                isFavorite = FavoritesManager::instance().isFavorite(selectedPath);
            }
        }
    }

    QAction* actFav = nullptr;
    if (!isTheater) {
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
    }
    // Scan selected folder recursively, or current folder non-recursively, for playable audio & video files
    QString folderToCheck = isFolder ? selectedPath : m_currentPath;
    QStringList playlistPaths;
    if (!folderToCheck.isEmpty()) {
        if (isFolder) {
            scanMediaFilesRecursively(folderToCheck, playlistPaths, 0);
        } else {
            QDir dir(folderToCheck);
            QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg" };
            QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
            for (const QFileInfo& fInfo : files) {
                if (mediaExts.contains(fInfo.suffix().toLower())) {
                    playlistPaths.append(fInfo.absoluteFilePath());
                }
            }
        }
    }

    QAction* actPlayPlaylist = nullptr;
    QAction* actPlayFullscreen = nullptr;
    if (!playlistPaths.isEmpty()) {
        actPlayPlaylist = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "Play Folder/Album in Preview");
        actPlayFullscreen = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "Play Folder/Album in Fullscreen");
    }
    QAction* actCompareSelected = nullptr;
    QAction* actCompareSibling = nullptr;
    if (!isTheater) {
        menu.addSeparator();
        actCompareSelected = menu.addAction("Compare Selected Files");
        actCompareSibling = menu.addAction("Compare with Sibling Pane File");
    }

    menu.addSeparator();
    QAction* actEditTags = menu.addAction("Edit Audio Tags...");
    QAction* actFetchMusicBrainz = menu.addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Fetch MusicBrainz Album Info...");
    QAction* actScrapeVideo = menu.addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Scrape Video Metadata...");
    QAction* actFetchFolderArt = menu.addAction(style->standardIcon(QStyle::SP_ComputerIcon), "Fetch Cover Art & Wallpaper...");
    
    QMenu* menuColorLabel = nullptr;
    QAction* actNone = nullptr;
    QAction* actRed = nullptr;
    QAction* actOrange = nullptr;
    QAction* actYellow = nullptr;
    QAction* actGreen = nullptr;
    QAction* actBlue = nullptr;
    QAction* actPurple = nullptr;
    QAction* actCustomOverlay = nullptr;
    QAction* actClearOverlay = nullptr;

    if (!isTheater) {
        menu.addSeparator();
        menuColorLabel = menu.addMenu("Color Label");
        actNone = menuColorLabel->addAction("None");
        actRed = menuColorLabel->addAction("Red");
        actOrange = menuColorLabel->addAction("Orange");
        actYellow = menuColorLabel->addAction("Yellow");
        actGreen = menuColorLabel->addAction("Green");
        actBlue = menuColorLabel->addAction("Blue");
        actPurple = menuColorLabel->addAction("Purple");
        menuColorLabel->addSeparator();
        actCustomOverlay = menuColorLabel->addAction("Custom Icon Overlay...");
        actClearOverlay = menuColorLabel->addAction("Clear Icon Overlay");
    }

    menu.addSeparator();
    QAction* actFileTags = menu.addAction("File Tags...");

    QAction* actEncryptVault = nullptr;
    QAction* actDecryptVault = nullptr;
    QAction* actCreateArchive = nullptr;
    QAction* actCreateSecureArchive = nullptr;
    QAction* actExtractArchive = nullptr;
    QAction* actCalculateChecksum = nullptr;
    QAction* actSecureShred = nullptr;
    QAction* actImageConvert = nullptr;

    if (!isTheater) {
        menu.addSeparator();
        if (index.isValid()) {
            if (QFileInfo(selectedPath).suffix().toLower() == "vault") {
                actDecryptVault = menu.addAction("Decrypt Secure Vault...");
            } else {
                actEncryptVault = menu.addAction("Encrypt into Secure Vault...");
            }
        }

        actCreateArchive = menu.addAction("Create Archive...");
        actCreateSecureArchive = menu.addAction("Create Secure Archive (AES-256)...");
        actExtractArchive = menu.addAction("Extract Archive...");
        menu.addSeparator();
        actCalculateChecksum = menu.addAction("Calculate Checksum Hash...");
        actSecureShred = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), "Secure Shred (Delete Permanently)...");
        
        QStringList imageExts = { "jpg", "jpeg", "png", "webp", "bmp" };
        QStringList selectedImages;
        for (const QString& sPath : curSelected) {
            if (imageExts.contains(QFileInfo(sPath).suffix().toLower())) {
                selectedImages.append(sPath);
            }
        }
        if (!selectedImages.isEmpty()) {
            actImageConvert = menu.addAction("Batch Convert/Resize Images...");
        }
    }

    menu.addSeparator();
    QAction* actConfigureFolderLayouts = menu.addAction("Folder Profiles & Layouts...");
    QAction* actSaveFolderProfile = menu.addAction("Save Current Layout as Folder Profile...");
    QAction* actSaveDefaultProfile = menu.addAction("Save Current Layout as Default Profile");
    QAction* actLoadDefaultProfile = menu.addAction("Load Default Profile");

    QWidget* parentW = parentWidget();
    while (parentW && !parentW->inherits("MainWindow")) {
        parentW = parentW->parentWidget();
    }
    MainWindow* mw = qobject_cast<MainWindow*>(parentW);
    if (mw) {
        QMenu* menuApplyProfile = menu.addMenu("Apply Profile Layout to Current Folder");
        for (const auto& r : mw->folderRules()) {
            if (!r.name.isEmpty()) {
                QString profileName = r.name;
                QAction* act = menuApplyProfile->addAction(profileName);
                connect(act, &QAction::triggered, this, [mw, profileName]() {
                    mw->onApplyProfileToCurrentFolder(profileName);
                });
            }
        }
    }

    menu.addSeparator();

    QAction* actProp = menu.addAction(style->standardIcon(QStyle::SP_MessageBoxInformation), "Properties");

    bool hasSelection = index.isValid();
    actOpen->setEnabled(hasSelection);
    if (actFileTags) actFileTags->setEnabled(hasSelection);
    if (!isTheater) {
        actCopy->setEnabled(hasSelection);
        actCut->setEnabled(hasSelection);
        actDelete->setEnabled(hasSelection);
        actCalculateChecksum->setEnabled(hasSelection && QFileInfo(selectedPath).isFile());
        actSecureShred->setEnabled(hasSelection);
        actRename->setEnabled(hasSelection);
        actBulkRename->setEnabled(hasSelection);
        menuColorLabel->setEnabled(hasSelection);

        bool canCompareSelected = (curSelected.size() == 2 && QFileInfo(curSelected[0]).isFile() && QFileInfo(curSelected[1]).isFile());
        actCompareSelected->setEnabled(canCompareSelected);

        bool canCompareSibling = false;
        QString sibSelectedPath;
        if (m_siblingPanel && m_siblingPanel->isVisible() && curSelected.size() == 1 && QFileInfo(curSelected[0]).isFile()) {
            QStringList sibSelected = m_siblingPanel->selectedPaths();
            if (sibSelected.size() == 1 && QFileInfo(sibSelected[0]).isFile()) {
                canCompareSibling = true;
                sibSelectedPath = sibSelected[0];
            }
        }
        actCompareSibling->setEnabled(canCompareSibling);
    }
    bool hasAudioFiles = false;
    for (const QString& sPath : curSelected) {
        QFileInfo info(sPath);
        if (info.isDir()) {
            if (hasAudioFilesRecursively(sPath, 0)) {
                hasAudioFiles = true;
            }
        } else if (info.isFile()) {
            QString ext = info.suffix().toLower();
            if (ext == "mp3" || ext == "flac") {
                hasAudioFiles = true;
            }
        }
        if (hasAudioFiles) break;
    }
    actEditTags->setEnabled(hasSelection && hasAudioFiles);
    actFetchMusicBrainz->setEnabled(hasSelection && hasAudioFiles);

    bool hasVideoFilesOrDirs = false;
    QStringList videoExts = { "mp4", "mkv", "avi", "mov", "webm", "mpeg", "mpg" };
    for (const QString& sPath : curSelected) {
        QFileInfo info(sPath);
        if (info.isDir()) {
            hasVideoFilesOrDirs = true;
            break;
        } else if (info.isFile()) {
            QString ext = info.suffix().toLower();
            if (videoExts.contains(ext)) {
                hasVideoFilesOrDirs = true;
                break;
            }
        }
    }
    actScrapeVideo->setEnabled(hasSelection && hasVideoFilesOrDirs);
    actFetchFolderArt->setEnabled(isFolder || !m_currentPath.isEmpty());

    if (!isTheater) {
        bool isArchive = false;
        if (curSelected.size() == 1) {
            QString ext = QFileInfo(curSelected.first()).suffix().toLower();
            isArchive = (ext == "zip" || ext == "tar" || ext == "gz" || ext == "xz" || ext == "bz2" || ext == "tgz" || ext == "7z" || ext == "rar" || ext == "adf" || ext == "d64" || ext == "iso" || ext == "img");
        }
        actCreateArchive->setEnabled(!curSelected.isEmpty());
        actExtractArchive->setEnabled(isArchive);

        bool hasSibling = m_siblingPanel && m_siblingPanel->isVisible();
        actCopyToSibling->setEnabled(hasSelection && hasSibling);
        actMoveToSibling->setEnabled(hasSelection && hasSibling);

        QClipboard* clipboard = QApplication::clipboard();
        const QMimeData* mimeData = clipboard->mimeData();
        actPaste->setEnabled(mimeData && mimeData->hasUrls());
    }

    QAction* actToggleZen = nullptr;
    QAction* actToggleDoubleclick = nullptr;

    if (m_viewStack->currentWidget() == m_theaterListView || m_viewStack->currentWidget() == m_theaterContainer) {
        menu.addSeparator();

        QSettings settings("Amifiles", "Amifiles");
        bool zenActive = settings.value("preferences/zen_mode", false).toBool();
        bool builtinDoubleclick = settings.value("preferences/builtin_player_doubleclick", false).toBool();

        actToggleZen = menu.addAction("Clean Interface Mode (Zen Mode)");
        actToggleZen->setCheckable(true);
        actToggleZen->setChecked(zenActive);

        actToggleDoubleclick = menu.addAction("Double-click Plays Media in Built-in Fullscreen Player");
        actToggleDoubleclick->setCheckable(true);
        actToggleDoubleclick->setChecked(builtinDoubleclick);
    }

    // Execute menu on the active view layout widget
    QPoint globalPos = QCursor::pos();
    QAction* selected = menu.exec(globalPos);
    if (!selected) return;

    if (selected == actConfigureFolderLayouts) {
        emit configureFolderLayoutsRequested();
        return;
    } else if (selected == actSaveFolderProfile) {
        emit saveFolderProfileRequested();
        return;
    } else if (selected == actSaveDefaultProfile) {
        emit saveDefaultProfileRequested();
        return;
    } else if (selected == actLoadDefaultProfile) {
        emit loadDefaultProfileRequested();
        return;
    }

    if (selected == actOpen) {
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
    } else if (selected == actCopy) {
        onCopy();
    } else if (selected == actCut) {
        onCut();
    } else if (selected == actPaste) {
        onPaste();
    } else if (selected == actCopyToSibling) {
        QWidget* p = parentWidget();
        while (p && !p->inherits("MainWindow")) {
            p = p->parentWidget();
        }
        if (p) {
            QMetaObject::invokeMethod(p, "onCopyToSiblingAction");
        }
    } else if (selected == actMoveToSibling) {
        QWidget* p = parentWidget();
        while (p && !p->inherits("MainWindow")) {
            p = p->parentWidget();
        }
        if (p) {
            QMetaObject::invokeMethod(p, "onMoveToSiblingAction");
        }
    } else if (selected == actDelete) {
        onDelete();
    } else if (selected == actRename) {
        QStringList paths = selectedPaths();
        if (paths.size() > 1) {
            BulkRenameDialog dlg(paths, this);
            if (dlg.exec() == QDialog::Accepted) {
                refresh();
            }
        } else {
            onRename();
        }
    } else if (selected == actBulkRename) {
        QStringList paths = selectedPaths();
        if (!paths.isEmpty()) {
            BulkRenameDialog dlg(paths, this);
            if (dlg.exec() == QDialog::Accepted) {
                refresh();
            }
        }
    } else if (selected == actNewFolder) {
        onNewFolder();
    } else if (selected == actPlayPlaylist) {
        emit playlistPlayRequested(playlistPaths);
    } else if (selected == actPlayFullscreen) {
        emit playMediaBuiltinRequested(playlistPaths);
    } else if (selected == actFav) {
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
    } else if (selected == actCompareSelected) {
        VisualDiffDialog dlg(curSelected[0], curSelected[1], this);
        dlg.exec();
    } else if (selected == actCompareSibling) {
        QString sibSelectedPath;
        if (m_siblingPanel) {
            QStringList sibSelected = m_siblingPanel->selectedPaths();
            if (!sibSelected.isEmpty()) {
                sibSelectedPath = sibSelected.first();
            }
        }
        VisualDiffDialog dlg(curSelected[0], sibSelectedPath, this);
        dlg.exec();
    } else if (selected == actEditTags) {
        TagEditorDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actFetchMusicBrainz) {
        TagEditorDialog dlg(curSelected, this, true);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actScrapeVideo) {
        VideoScraperDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actFetchFolderArt) {
        QString folderPath = isFolder ? selectedPath : m_currentPath;
        if (!folderPath.isEmpty()) {
            FolderArtScraperDialog dlg(folderPath, this);
            dlg.exec();
            refresh();
        }
    } else if (selected == actCreateArchive) {
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
    } else if (selected == actCreateSecureArchive) {
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
    } else if (selected == actExtractArchive) {
        ArchiveDialog* dlg = new ArchiveDialog(ArchiveDialog::ModeExtract, curSelected.first(), m_currentPath, this);
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
    } else if (selected == actEncryptVault) {
        VaultDialog dlg(true, selectedPath, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        }
    } else if (selected == actDecryptVault) {
        VaultDialog dlg(false, selectedPath, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
            if (m_siblingPanel) m_siblingPanel->refresh();
            QTimer::singleShot(500, this, [this]() {
                refresh();
                if (m_siblingPanel) m_siblingPanel->refresh();
            });
        }
    } else if (selected == actCalculateChecksum) {
        ChecksumDialog dlg(selectedPath, this);
        dlg.exec();
    } else if (selected == actSecureShred) {
        ShredDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actImageConvert) {
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
    } else if (selected == actNone) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "");
        refresh();
    } else if (selected == actRed) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "red");
        refresh();
    } else if (selected == actOrange) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "orange");
        refresh();
    } else if (selected == actYellow) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "yellow");
        refresh();
    } else if (selected == actGreen) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "green");
        refresh();
    } else if (selected == actBlue) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "blue");
        refresh();
    } else if (selected == actPurple) {
        for (const QString& path : curSelected) TagManager::instance().setFileColor(path, "purple");
        refresh();
    } else if (selected == actCustomOverlay) {
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
    } else if (selected == actClearOverlay) {
        for (const QString& path : curSelected) {
            TagManager::instance().setFileOverlayIcon(path, "");
        }
        refresh();
    } else if (selected == actFileTags) {
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
    } else if (selected == actProp) {
        onShowProperties();
    } else if (selected && selected == actToggleZen) {
        QSettings settings("Amifiles", "Amifiles");
        bool current = settings.value("preferences/zen_mode", false).toBool();
        settings.setValue("preferences/zen_mode", !current);
        emit zenModeToggled(!current);
    } else if (selected && selected == actToggleDoubleclick) {
        QWidget* p = parentWidget();
        while (p && !p->inherits("MainWindow")) {
            p = p->parentWidget();
        }
        if (p) {
            bool current = false;
            QMetaObject::invokeMethod(p, "isBuiltinPlayerDoubleclickActive", Q_RETURN_ARG(bool, current));
            QMetaObject::invokeMethod(p, "setBuiltinPlayerDoubleclickActive", Q_ARG(bool, !current));
        }
    }
}

void FilePanel::setCategoryButtonsVisible(bool visible) {
    m_categoryButtonsVisible = visible;
    if (!m_flatViewEnabled && m_categoryWidget) {
        m_categoryWidget->setVisible(visible);
    }
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
    if (m_btnFilterAll && m_btnFilterAudio && m_btnFilterVideos && m_btnFilterPictures && m_btnFilterDocs && m_btnFilterArchive) {
        m_btnFilterAll->blockSignals(true);
        m_btnFilterAudio->blockSignals(true);
        m_btnFilterVideos->blockSignals(true);
        m_btnFilterPictures->blockSignals(true);
        m_btnFilterDocs->blockSignals(true);
        m_btnFilterArchive->blockSignals(true);

        m_btnFilterAll->setChecked(type == FileFilterProxyModel::FilterAll);
        m_btnFilterAudio->setChecked(type == FileFilterProxyModel::FilterAudio);
        m_btnFilterVideos->setChecked(type == FileFilterProxyModel::FilterVideos);
        m_btnFilterPictures->setChecked(type == FileFilterProxyModel::FilterPictures);
        m_btnFilterDocs->setChecked(type == FileFilterProxyModel::FilterDocs);
        m_btnFilterArchive->setChecked(type == FileFilterProxyModel::FilterArchive);

        m_btnFilterAll->blockSignals(false);
        m_btnFilterAudio->blockSignals(false);
        m_btnFilterVideos->blockSignals(false);
        m_btnFilterPictures->blockSignals(false);
        m_btnFilterDocs->blockSignals(false);
        m_btnFilterArchive->blockSignals(false);
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
        m_flatModel->setRootPath(m_currentPath);
        if (m_categoryWidget) m_categoryWidget->hide();

        connect(m_flatModel, &FlatFileSystemModel::scanStarted, this, [this]() {
            m_statusLabel->setText("Scanning directory recursively...");
        });
        connect(m_flatModel, &FlatFileSystemModel::scanFinished, this, &FilePanel::updateStatusText);
    } else {
        updateActiveViewModel();
        if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);
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

    emit zoomChanged(value);
}

void FilePanel::onGroupingChanged(int index) {
    if (index == 0) {
        m_groupProxy->setGrouping(false, "");
        m_treeView->expandAll();
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
        case 6: {
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
    if (m_theaterListView) {
        double factor = (double)size / 32.0;
        int gw = qRound(170.0 * factor);
        int gh = qRound(270.0 * factor);
        m_theaterListView->setGridSize(QSize(gw, gh));
    }
}

void FilePanel::updateStyles() {
    QSettings settings("Amifiles", "Amifiles");
    QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();
    int baseFontSize = settings.value("theme/font_size", 13).toInt();

    // Scale font size based on zoom level (m_zoomLevel is 0 to 6, default zoom level is 2)
    int fontSize = baseFontSize + (m_zoomLevel - 2) * 2;
    if (fontSize < 8) fontSize = 8;

    if (preset == "System Theme") {
        QString bgStyle = m_customBgColor.isEmpty() ? "" : QString("background-color: %1;").arg(m_customBgColor);
        m_viewStack->setStyleSheet(m_customBgColor.isEmpty() ? "" : QString("QStackedWidget { background-color: %1; }").arg(m_customBgColor));
        m_treeView->setStyleSheet(QString("QTreeView { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_listView->setStyleSheet(QString("QListView { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_theaterListView->setStyleSheet(QString("QListView { background: transparent; border: none; font-size: %1px; }"
                                                 "QListView::item { border: none; }"
                                                 "QListView::item:hover { background: transparent; }"
                                                 "QListView::item:selected { background: transparent; }").arg(fontSize));
        m_millerView->setStyleSheet(m_customBgColor.isEmpty() ? "" : QString("MillerColumnsView { %1 }").arg(bgStyle));
        m_timelineView->setStyleSheet(QString("QTreeWidget { border: none; %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        m_filmstripView->setStyleSheet(m_customBgColor.isEmpty() ? "" : QString("FilmstripView { %1 }").arg(bgStyle));
        if (m_searchResultsView) {
            m_searchResultsView->setStyleSheet(QString("QListView { %1 font-size: %2px; }").arg(bgStyle).arg(fontSize));
        }
    } else {
        Theme::ThemeColors colors = Theme::getThemeColors();
        QString bg = m_customBgColor.isEmpty() ? colors.bg : m_customBgColor;
        QString border = colors.border;
        QString text = colors.text;
        QString accent = colors.accent;
        
        QString borderColor = m_isActive ? accent : border;

        m_viewStack->setStyleSheet(QString("QStackedWidget { border: 2px solid %1; border-radius: 4px; background-color: %2; }").arg(borderColor).arg(bg));

        m_treeView->setStyleSheet(QString("QTreeView { border: none; background-color: %1; font-size: %2px; }").arg(bg).arg(fontSize));
        m_listView->setStyleSheet(QString("QListView { border: none; background-color: %1; font-size: %2px; }").arg(bg).arg(fontSize));
        m_theaterListView->setStyleSheet(QString("QListView { background: transparent; color: %1; border: none; font-size: %2px; }"
                                                 "QListView::item { border: none; }"
                                                 "QListView::item:hover { background: transparent; }"
                                                 "QListView::item:selected { background: transparent; }").arg(text).arg(fontSize));
        m_millerView->setStyleSheet(QString("MillerColumnsView { border: none; background-color: %1; }").arg(bg));
        m_timelineView->setStyleSheet(QString("QTreeWidget { border: none; background-color: %1; font-size: %2px; }").arg(bg).arg(fontSize));
        m_filmstripView->setStyleSheet(QString("FilmstripView { border: none; background-color: %1; }").arg(bg));

        if (m_searchResultsView) {
            m_searchResultsView->setStyleSheet(QString("QListView { border: 2px solid %1; background-color: %2; color: %3; font-size: %4px; }").arg(borderColor).arg(bg).arg(text).arg(fontSize));
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
            if (!siblingPath.isEmpty() && QDir(siblingPath).exists()) {
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
            if (!siblingPath.isEmpty() && QDir(siblingPath).exists()) {
                for (const QString& f : selectedFiles) {
                    QFileInfo fi(f);
                    QString destPath = QDir(siblingPath).filePath(fi.fileName());
                    QFile::rename(f, destPath);
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
        QString newName = QInputDialog::getText(this, "Rename File", "New name:", QLineEdit::Normal, oldName, &ok);
        if (ok && !newName.isEmpty() && newName != oldName) {
            QString newPath = info.absoluteDir().filePath(newName);
            if (QFile::rename(firstFile, newPath)) {
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
    if (index == 0) { // Details Table
        m_listView->setItemDelegate(m_defaultDelegate);
        m_viewStack->setCurrentWidget(m_treeView);
    } else if (index == 1) { // Grid / Icons
        m_listView->setItemDelegate(m_defaultDelegate);
        m_listView->setGridSize(QSize());
        m_viewStack->setCurrentWidget(m_listView);
    } else if (index == 2) { // Card / Tiles
        m_listView->setItemDelegate(m_cardDelegate);
        m_listView->setGridSize(QSize(195, 75));
        m_viewStack->setCurrentWidget(m_listView);
    } else if (index == 3) { // Miller Columns
        m_millerView->setRootPath(m_currentPath);
        m_viewStack->setCurrentWidget(m_millerView);
    } else if (index == 4) { // Chronological Timeline
        m_timelineView->setRootPath(m_currentPath);
        m_viewStack->setCurrentWidget(m_timelineView);
    } else if (index == 5) { // Filmstrip View
        m_filmstripView->setRootPath(m_currentPath);
        m_viewStack->setCurrentWidget(m_filmstripView);
    } else if (index == 6) { // Theater View
        m_viewStack->setCurrentWidget(m_theaterContainer);
        if (m_groupProxy && m_groupProxy->isGroupingActive()) {
            m_theaterListView->setVisible(false);
            m_theaterScrollArea->setVisible(true);
            rebuildTheaterGroups();
        } else {
            m_theaterListView->setVisible(true);
            m_theaterScrollArea->setVisible(false);
        }
    }
    
    // Save view mode index choice in preferences
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("file_panel/view_mode_index", index);
    emit viewModeChanged();
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

    bool shouldPlayOnDoubleclick = builtinPlayerDoubleclick;

    if (info.isDir()) {
        if (shouldPlayOnDoubleclick && isPlayableAlbumFolder(path)) {
            QStringList playlistPaths;
            scanMediaFilesRecursively(path, playlistPaths);
            if (!playlistPaths.isEmpty()) {
                emit playMediaBuiltinRequested(playlistPaths);
                return;
            }
        }
        navigateTo(path, true);
    } else {
        QString ext = info.suffix().toLower();
        QSettings settings("Amifiles", "Amifiles");
        bool archiveNavEnabled = settings.value("preferences/archive_nav", true).toBool();
        QStringList archiveExts = { "zip", "tar", "gz", "xz", "bz2", "tgz", "rar", "7z", "adf", "d64", "iso", "img" };

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
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    }
}

int FilePanel::viewModeIndex() const {
    return m_comboViewMode ? m_comboViewMode->currentIndex() : 0;
}

void FilePanel::setViewModeIndex(int index) {
    if (m_comboViewMode) {
        m_comboViewMode->setCurrentIndex(index);
    }
}

void FilePanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateCloneButtonIcon();
}

void FilePanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_groupProxy && m_groupProxy->isGroupingActive() && m_viewStack->currentWidget() == m_theaterContainer) {
        rebuildTheaterGroups();
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
            QStringList blurayChecks = { "bluray_cover.jpg", "bluray_cover.jpeg", "bluray_cover.png", "bluray.jpg", "bluray.jpeg", "bluray.png" };
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

    // 4. Fall back to CD covers
    if (artPath.isEmpty()) {
        QStringList cdChecks = { "folder.jpg", "folder.jpeg", "folder.png", "cover.jpg", "cover.jpeg", "cover.png" };
        artPath = findFirstCaseInsensitiveFile(dirPath, cdChecks);
        if (!artPath.isEmpty()) {
            casingInt = 0; // CasingCD
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

    QImage cover;
    QImageReader reader(artPath);
    if (reader.canRead()) {
        QSize origSize = reader.size();
        if (origSize.isValid()) {
            int targetW = 220;
            int targetH = 220;
            if (casingInt == 1) { // DVD
                targetW = 154;
                targetH = 240;
            } else if (casingInt == 2) { // BluRay
                targetW = 164;
                targetH = 226;
            }
            QSize scaledSize = origSize.scaled(QSize(targetW, targetH), Qt::KeepAspectRatioByExpanding);
            reader.setScaledSize(scaledSize);
        }
        cover = reader.read();
    }

    if (cover.isNull()) {
        QPointer<FileFilterProxyModel> model = m_model;
        QString path = m_path;
        QMetaObject::invokeMethod(model.data(), [model, path]() {
            if (model) {
                model->onCasingRendered(path, "", 0, QImage());
            }
        }, Qt::QueuedConnection);
        return;
    }

    int caseW = 256;
    int caseH = 256;
    if (casingInt == 1) { // DVD
        caseW = 170;
    } else if (casingInt == 2) { // BluRay
        caseW = 180;
    }

    double s = 256.0 / 48.0;

    QImage caseImage(caseW, caseH, QImage::Format_ARGB32_Premultiplied);
    caseImage.fill(Qt::transparent);

    QPainter painter(&caseImage);
    painter.setRenderHint(QPainter::Antialiasing);

    if (casingInt == 0) { // CasingCD
        painter.setBrush(QColor("#313244"));
        painter.setPen(QPen(QColor("#45475a"), qMax(1, qRound(1 * s))));
        painter.drawRoundedRect(qRound(2 * s), qRound(2 * s), caseW - qRound(4 * s), caseH - qRound(4 * s), qRound(3 * s), qRound(3 * s));

        painter.setBrush(QColor("#11111b"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(qRound(3 * s), qRound(3 * s), qRound(5 * s), caseH - qRound(6 * s));

        int coverX = qRound(10 * s);
        int coverY = qRound(4 * s);
        int coverW = caseW - qRound(14 * s);
        int coverH = caseH - qRound(8 * s);
        painter.drawImage(QRect(coverX, coverY, coverW, coverH), cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 60), qMax(1, qRound(1 * s))));
        painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), caseH - qRound(6 * s), qRound(2 * s), qRound(2 * s));

        QLinearGradient gradient(0, 0, caseW, caseH);
        gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
        gradient.setColorAt(0.3, QColor(255, 255, 255, 120));
        gradient.setColorAt(0.35, QColor(255, 255, 255, 0));
        gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        QPolygon gloss;
        gloss << QPoint(qRound(9 * s), qRound(4 * s))
              << QPoint(caseW - qRound(4 * s), qRound(4 * s))
              << QPoint(qRound(9 * s), caseH - qRound(4 * s));
        painter.drawPolygon(gloss);
    }
    else if (casingInt == 1) { // CasingDVD
        painter.setBrush(QColor("#1e1e2e"));
        painter.setPen(QPen(QColor("#313244"), qMax(1, qRound(1.5 * s))));
        painter.drawRoundedRect(qRound(2 * s), qRound(2 * s), caseW - qRound(4 * s), caseH - qRound(4 * s), qRound(4 * s), qRound(4 * s));

        int coverX = qRound(4 * s);
        int coverY = qRound(4 * s);
        int coverW = caseW - qRound(8 * s);
        int coverH = caseH - qRound(8 * s);
        painter.drawImage(QRect(coverX, coverY, coverW, coverH), cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 40), qMax(1, qRound(1 * s))));
        painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), caseH - qRound(6 * s), qRound(3 * s), qRound(3 * s));

        QLinearGradient gradient(0, 0, caseW, caseH);
        gradient.setColorAt(0.0, QColor(255, 255, 255, 60));
        gradient.setColorAt(0.25, QColor(255, 255, 255, 100));
        gradient.setColorAt(0.3, QColor(255, 255, 255, 0));
        gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        QPolygon gloss;
        gloss << QPoint(qRound(4 * s), qRound(4 * s))
              << QPoint(caseW - qRound(4 * s), qRound(4 * s))
              << QPoint(qRound(4 * s), caseH - qRound(4 * s));
        painter.drawPolygon(gloss);
    }
    else if (casingInt == 2) { // CasingBluRay
        painter.setBrush(QColor("#1e1e2e"));
        painter.setPen(QPen(QColor("#313244"), qMax(1, qRound(1.5 * s))));
        painter.drawRoundedRect(qRound(2 * s), qRound(2 * s), caseW - qRound(4 * s), caseH - qRound(4 * s), qRound(4 * s), qRound(4 * s));

        int coverX = qRound(4 * s);
        int coverY = qRound(9 * s);
        int coverW = caseW - qRound(8 * s);
        int coverH = caseH - qRound(13 * s);
        painter.drawImage(QRect(coverX, coverY, coverW, coverH), cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        QLinearGradient blueGrad(0, 0, 0, coverY);
        blueGrad.setColorAt(0.0, QColor(14, 165, 233, 220));
        blueGrad.setColorAt(1.0, QColor(3, 105, 161, 180));
        
        painter.setBrush(blueGrad);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), qRound(7 * s), qRound(2 * s), qRound(2 * s));

        painter.setPen(QPen(QColor(255, 255, 255, 140), 1));
        QFont logoFont = painter.font();
        logoFont.setPointSize(qRound(2.2 * s));
        logoFont.setBold(true);
        logoFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(0.4 * s));
        painter.setFont(logoFont);
        painter.drawText(QRect(0, qRound(3 * s), caseW, qRound(6 * s)), Qt::AlignCenter, "BLU-RAY");

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 45), qMax(1, qRound(1 * s))));
        painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), caseH - qRound(6 * s), qRound(3 * s), qRound(3 * s));

        QLinearGradient gradient(0, 0, caseW, caseH);
        gradient.setColorAt(0.0, QColor(255, 255, 255, 60));
        gradient.setColorAt(0.25, QColor(255, 255, 255, 90));
        gradient.setColorAt(0.3, QColor(255, 255, 255, 0));
        gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        QPolygon gloss;
        gloss << QPoint(qRound(4 * s), qRound(4 * s))
              << QPoint(caseW - qRound(4 * s), qRound(4 * s))
              << QPoint(qRound(4 * s), caseH - qRound(4 * s));
        painter.drawPolygon(gloss);
    }
    
    painter.end();

    QPointer<FileFilterProxyModel> model = m_model;
    QString path = m_path;
    QMetaObject::invokeMethod(model.data(), [model, path, artPath, casingInt, caseImage]() {
        if (model) {
            model->onCasingRendered(path, artPath, casingInt, caseImage);
        }
    }, Qt::QueuedConnection);
}

void FileFilterProxyModel::onCasingRendered(const QString& path, const QString& artPath, int casingType, const QImage& image) {
    m_pendingCasingChecks.remove(path);
    m_casingCache.insert(path, qMakePair(artPath, casingType));
    
    if (!artPath.isEmpty() && !image.isNull()) {
        QString cacheKey = artPath + "_" + QString::number(casingType);
        m_iconCache.insert(cacheKey, QIcon(QPixmap::fromImage(image)));
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

void FilePanel::rebuildTheaterGroups() {
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
        grid->setDragEnabled(false);
        grid->setAcceptDrops(false);
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
}
