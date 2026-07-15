#include "filepanel.h"
#include "favoritesmanager.h"
#include "archivemodel.h"
#include "searchworker.h"
#include "metadataextractor.h"
#include "bulkrename.h"
#include "diffdialog.h"
#include "tageditordialog.h"
#include "copyqueue.h"
#include "archivedialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include "checksumdialog.h"
#include "shreddialog.h"
#include <QMessageBox>
#include <QCheckBox>
#include <QInputDialog>
#include <QClipboard>
#include <QMimeData>
#include <QButtonGroup>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

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
    QHBoxLayout* navLayout = new QHBoxLayout();
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
    m_btnFavorite->setToolTip("Add/Remove Favorite (Right-click to manage all favorites)");
    m_btnFavorite->setStyleSheet("QToolButton { font-size: 16px; font-weight: bold; color: #f9e2af; }");
    m_btnFavorite->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_btnFavorite, &QToolButton::clicked, this, &FilePanel::onFavoriteClicked);
    connect(m_btnFavorite, &QToolButton::customContextMenuRequested, this, &FilePanel::onFavoriteButtonContextMenu);

    m_btnFlatView = new QToolButton(this);
    m_btnFlatView->setText("Flat View");
    m_btnFlatView->setCheckable(true);
    m_btnFlatView->setToolTip("Toggle Flat View (Recurse all subfolders)");
    m_btnFlatView->setStyleSheet("QToolButton { font-weight: bold; color: #a6e3a1; }");
    connect(m_btnFlatView, &QToolButton::toggled, this, &FilePanel::setFlatViewEnabled);

    m_btnViewMode = new QToolButton(this);
    m_btnViewMode->setText("List/Grid");
    m_btnViewMode->setToolTip("Toggle Details List View vs. Icon Grid View");
    m_btnViewMode->setStyleSheet("QToolButton { font-weight: bold; color: #89b4fa; }");
    connect(m_btnViewMode, &QToolButton::clicked, this, &FilePanel::onToggleViewMode);

    navLayout->addWidget(m_btnBack);
    navLayout->addWidget(m_btnForward);
    navLayout->addWidget(m_btnUp);
    navLayout->addWidget(m_pathEdit, 1);
    navLayout->addWidget(m_btnGo);
    navLayout->addWidget(m_btnFavorite);
    navLayout->addWidget(m_btnFlatView);
    navLayout->addWidget(m_btnViewMode);

    // Central Tree View
    m_treeView = new QTreeView(this);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSortingEnabled(true);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu); // Enable context menu
    m_treeView->installEventFilter(this); // Install event filter to capture focus events
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragDrop);

    // Icon Grid List View
    m_listView = new QListView(this);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setWordWrap(true);
    m_listView->setUniformItemSizes(true);
    m_listView->setSpacing(10);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->installEventFilter(this);
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    m_listView->setDragDropMode(QAbstractItemView::DragDrop);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->addWidget(m_listView);

    // Global Search UI Setup
    m_globalSearchEdit = new QLineEdit(this);
    m_globalSearchEdit->setPlaceholderText("🔍 Search files & subfolders... (Esc to clear)");
    m_globalSearchEdit->setClearButtonEnabled(true);
    m_globalSearchEdit->installEventFilter(this);
    connect(m_globalSearchEdit, &QLineEdit::textChanged, this, &FilePanel::onGlobalSearchChanged);

    m_searchResultsView = new QListView(this);
    m_searchResultsView->setVisible(false);
    m_searchResultsView->installEventFilter(this);
    m_searchResultModel = new QStringListModel(this);
    m_searchResultsView->setModel(m_searchResultModel);
    connect(m_searchResultsView, &QListView::clicked, this, &FilePanel::onSearchResultSelected);
    connect(m_searchResultsView, &QListView::doubleClicked, this, &FilePanel::onSearchResultDoubleClicked);

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

    m_flatModel = new FlatFileSystemModel(this);
    m_flatProxyModel = new QSortFilterProxyModel(this);
    m_flatProxyModel->setSourceModel(m_flatModel);
    m_flatProxyModel->setFilterKeyColumn(0);
    m_flatProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_archiveModel = new ArchiveModel(this);

    m_treeView->setModel(m_proxyModel);
    m_listView->setModel(m_proxyModel);
    m_listView->setSelectionModel(m_treeView->selectionModel()); // sync selections

    // Header formatting
    QHeaderView* header = m_treeView->header();
    header->setSectionsMovable(true);
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);

    connect(m_treeView, &QTreeView::doubleClicked, this, &FilePanel::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_listView, &QListView::doubleClicked, this, &FilePanel::onDoubleClicked);
    connect(m_listView, &QListView::customContextMenuRequested, this, &FilePanel::onCustomContextMenu);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);

    // Restore view mode choice from settings
    QSettings settings("Amifiles", "Amifiles");
    bool viewModeGrid = settings.value("file_panel/view_mode_grid", false).toBool();
    if (viewModeGrid) {
        m_viewStack->setCurrentWidget(m_listView);
    } else {
        m_viewStack->setCurrentWidget(m_treeView);
    }

    m_treeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView->header(), &QHeaderView::customContextMenuRequested, this, &FilePanel::onHeaderContextMenu);
    for (int i = 0; i < 16; ++i) {
        bool hidden = settings.value(QString("columns/hidden_%1").arg(i), i >= 4).toBool();
        m_treeView->header()->setSectionHidden(i, hidden);
    }


    // Bottom Filter and Status bar

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter files & folders (contains)...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FilePanel::onFilterChanged);

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
    categoryLayout->addStretch(1); // Push buttons to the left

    // Wrap Text filter row in a container widget to make it toggleable
    m_filterTextWidget = new QWidget(this);
    QHBoxLayout* filterTextLayout = new QHBoxLayout(m_filterTextWidget);
    filterTextLayout->setSpacing(6);
    filterTextLayout->setContentsMargins(0, 0, 0, 0);
    filterTextLayout->addWidget(m_filterEdit, 1);

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

    QVBoxLayout* bottomLayout = new QVBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);
    bottomLayout->addWidget(m_categoryWidget);
    bottomLayout->addWidget(m_filterTextWidget);
    bottomLayout->addWidget(m_statusWidget);

    mainLayout->addWidget(m_globalSearchEdit);
    mainLayout->addLayout(navLayout);
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
    if (watched == m_treeView || watched == m_listView) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            setActive(true);
            emit panelActivated(this);
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
                        CopyQueueManager::instance().queueCopy(srcPaths, destDir, false);
                        CopyQueueManager::instance().showQueueDialog(this);
                    } else if (chosen == actMove) {
                        CopyQueueManager::instance().queueCopy(srcPaths, destDir, true);
                        CopyQueueManager::instance().showQueueDialog(this);
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

void FilePanel::setPath(const QString& path) {
    navigateTo(path, true);
}

void FilePanel::navigateTo(const QString& path, bool addHistory) {
    if (path.contains("//")) {
        int sepIdx = path.indexOf("//");
        QString archiveFile = path.left(sepIdx);
        QString virtualSubpath = path.mid(sepIdx + 2);

        QFileInfo archInfo(archiveFile);
        if (archInfo.exists() && archInfo.isFile()) {
            if (!m_archiveViewActive) {
                if (m_treeView->selectionModel()) {
                    disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                }
                
                m_archiveViewActive = true;
                m_treeView->setModel(m_archiveModel);
                m_listView->setModel(m_archiveModel);
                m_listView->setSelectionModel(m_treeView->selectionModel());

                if (m_treeView->selectionModel()) {
                    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                }

                if (m_categoryWidget) m_categoryWidget->hide();

                // Enable interactive resizing for all columns
                for (int i = 0; i < 4; ++i) {
                    m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
                }
            }

            m_archiveModel->loadArchive(archiveFile);
            m_archiveModel->navigateToVirtualPath(virtualSubpath);
            m_pathEdit->setText(QDir::toNativeSeparators(archiveFile) + "//" + m_archiveModel->currentVirtualPath());
            emit pathChanged(m_currentPath);
            updateStatusText();
            return;
        }
    } else if (m_archiveViewActive) {
        if (m_treeView->selectionModel()) {
            disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
        }
        m_archiveViewActive = false;
        m_treeView->setModel(m_proxyModel);
        m_listView->setModel(m_proxyModel);
        m_listView->setSelectionModel(m_treeView->selectionModel());
        if (m_treeView->selectionModel()) {
            connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
        }

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
        m_treeView->setRootIndex(QModelIndex());
        m_listView->setRootIndex(QModelIndex());
    } else {
        m_proxyModel->setCurrentPath(m_currentPath);
        QModelIndex srcIndex = m_fileModel->index(m_currentPath);
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(srcIndex);
        m_treeView->setRootIndex(proxyIndex);
        m_listView->setRootIndex(proxyIndex);
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

    // Set column widths nicely for initial load
    m_treeView->setColumnWidth(0, 250); // Name
    m_treeView->setColumnWidth(1, 80);  // Size
    m_treeView->setColumnWidth(2, 100); // Type
    m_treeView->setColumnWidth(3, 140); // Date Modified

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
            if (m_treeView->selectionModel()) {
                disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
            }

            m_archiveViewActive = false;
            m_treeView->setModel(m_proxyModel);

            if (m_treeView->selectionModel()) {
                connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
            }

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
    } else {
        QModelIndex srcIndex = m_proxyModel->mapToSource(index);
        path = m_fileModel->filePath(srcIndex);
    }

    QFileInfo info(path);
    if (info.isDir()) {
        navigateTo(path, true);
    } else {
        QString ext = info.suffix().toLower();
        QSettings settings("Amifiles", "Amifiles");
        bool archiveNavEnabled = settings.value("preferences/archive_nav", true).toBool();
        QStringList archiveExts = { "zip", "tar", "gz", "xz", "bz2", "tgz", "rar" };

        if (archiveNavEnabled && archiveExts.contains(ext)) {
            m_statusLabel->setText("Loading archive...");
            QApplication::processEvents();
            bool ok = m_archiveModel->loadArchive(path);
            updateStatusText();

            if (ok) {
                if (m_treeView->selectionModel()) {
                    disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                }
                
                m_archiveViewActive = true;
                m_treeView->setModel(m_archiveModel);

                if (m_treeView->selectionModel()) {
                    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
                }

                m_pathEdit->setText(QDir::toNativeSeparators(path) + "//");
                
                // Use interactive resizing for archive view columns
                for (int i = 0; i < 4; ++i) {
                    m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
                }

                if (m_categoryWidget) m_categoryWidget->hide();

                emit pathChanged(path);
                return;
            }
        }

        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void FilePanel::onSelectionChanged() {
    updateStatusText();
    QStringList paths = selectedPaths();

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
    QStringList paths;
    if (!m_treeView->selectionModel()) return paths;
    QModelIndexList selectedRows = m_treeView->selectionModel()->selectedRows();
    for (const QModelIndex& index : selectedRows) {
        if (m_archiveViewActive) {
            paths.append(m_archiveModel->entryPath(index));
        } else if (m_flatViewEnabled) {
            QSortFilterProxyModel* proxy = qobject_cast<QSortFilterProxyModel*>(m_treeView->model());
            if (proxy) {
                QModelIndex srcIndex = proxy->mapToSource(index);
                paths.append(m_flatModel->filePath(srcIndex));
            }
        } else {
            QModelIndex srcIndex = m_proxyModel->mapToSource(index);
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
    CopyQueueManager::instance().queueCopy(srcPaths, m_currentPath, isCut);

    if (isCut) {
        clipboard->clear();
    }

    // Launch the centralized modeless monitor dialog
    CopyQueueManager::instance().showQueueDialog(this);
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

void FilePanel::onCustomContextMenu(const QPoint& pos) {
    QModelIndex index;
    if (m_viewStack->currentWidget() == m_listView) {
        index = m_listView->indexAt(pos);
    } else {
        index = m_treeView->indexAt(pos);
    }
    
    QMenu menu(this);
    QStyle* style = QApplication::style();

    QAction* actOpen = menu.addAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Open");
    menu.addSeparator();
    
    QAction* actCopy = menu.addAction(style->standardIcon(QStyle::SP_DialogSaveButton), "Copy");
    actCopy->setShortcut(QKeySequence::Copy);
    
    QAction* actCut = menu.addAction("Cut");
    actCut->setShortcut(QKeySequence::Cut);
    
    QAction* actPaste = menu.addAction("Paste");
    actPaste->setShortcut(QKeySequence::Paste);

    QAction* actCopyToSibling = menu.addAction("Copy to Sibling Panel");
    actCopyToSibling->setShortcut(QKeySequence(Qt::Key_F5));

    QAction* actMoveToSibling = menu.addAction("Move to Sibling Panel");
    actMoveToSibling->setShortcut(QKeySequence(Qt::Key_F6));
    
    QAction* actDelete = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), "Delete");
    actDelete->setShortcut(QKeySequence::Delete);
    
    QAction* actRename = menu.addAction("Rename");
    QAction* actBulkRename = menu.addAction("Bulk Rename...");
    menu.addSeparator();
    
    QAction* actNewFolder = menu.addAction(style->standardIcon(QStyle::SP_FileDialogNewFolder), "New Folder");
    menu.addSeparator();
    
    QString selectedPath;
    bool isFolder = false;
    bool isFavorite = false;
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
    // Scan selected folder or current folder for playable audio & video files
    QString folderToCheck = isFolder ? selectedPath : m_currentPath;
    QStringList playlistPaths;
    QStringList mediaExts = { "mp3", "wav", "flac", "ogg", "m4a", "mp4", "avi", "mkv", "mov", "webm", "mpeg", "mpg" };
    if (!folderToCheck.isEmpty()) {
        QDir dir(folderToCheck);
        QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo& fInfo : files) {
            if (mediaExts.contains(fInfo.suffix().toLower())) {
                playlistPaths.append(fInfo.absoluteFilePath());
            }
        }
    }

    QAction* actPlayPlaylist = nullptr;
    if (!playlistPaths.isEmpty()) {
        actPlayPlaylist = menu.addAction(style->standardIcon(QStyle::SP_MediaPlay), "Play Folder/Album in Preview");
    }

    menu.addSeparator();
    QAction* actCompareSelected = menu.addAction("Compare Selected Files");
    QAction* actCompareSibling = menu.addAction("Compare with Sibling Pane File");
    QAction* actEditTags = menu.addAction("Edit Tags...");
    menu.addSeparator();
    QAction* actCreateArchive = menu.addAction("Create Archive...");
    QAction* actExtractArchive = menu.addAction("Extract Archive...");
    menu.addSeparator();
    QAction* actCalculateChecksum = menu.addAction("Calculate Checksum Hash...");
    QAction* actSecureShred = menu.addAction(style->standardIcon(QStyle::SP_TrashIcon), "Secure Shred (Delete Permanently)...");
    menu.addSeparator();
    QAction* actProp = menu.addAction(style->standardIcon(QStyle::SP_MessageBoxInformation), "Properties");

    bool hasSelection = index.isValid();
    actOpen->setEnabled(hasSelection);
    actCopy->setEnabled(hasSelection);
    actCut->setEnabled(hasSelection);
    actDelete->setEnabled(hasSelection);
    actCalculateChecksum->setEnabled(hasSelection && QFileInfo(selectedPath).isFile());
    actSecureShred->setEnabled(hasSelection);
    actRename->setEnabled(hasSelection);
    actBulkRename->setEnabled(hasSelection);

    QStringList curSelected = selectedPaths();
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
    actEditTags->setEnabled(hasSelection);

    bool isArchive = false;
    if (curSelected.size() == 1) {
        QString ext = QFileInfo(curSelected.first()).suffix().toLower();
        isArchive = (ext == "zip" || ext == "tar" || ext == "gz" || ext == "xz" || ext == "bz2" || ext == "tgz");
    }
    actCreateArchive->setEnabled(!curSelected.isEmpty());
    actExtractArchive->setEnabled(isArchive);

    bool hasSibling = m_siblingPanel && m_siblingPanel->isVisible();
    actCopyToSibling->setEnabled(hasSelection && hasSibling);
    actMoveToSibling->setEnabled(hasSelection && hasSibling);

    QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    actPaste->setEnabled(mimeData && mimeData->hasUrls());

    // Execute menu on the active view layout widget
    QWidget* activeViewWidget = (m_viewStack->currentWidget() == m_listView) ? static_cast<QWidget*>(m_listView) : static_cast<QWidget*>(m_treeView);
    QAction* selected = menu.exec(activeViewWidget->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actOpen) {
        onDoubleClicked(index);
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
        VisualDiffDialog dlg(curSelected[0], sibSelectedPath, this);
        dlg.exec();
    } else if (selected == actEditTags) {
        TagEditorDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actCreateArchive) {
        ArchiveDialog dlg(ArchiveDialog::ModeCreate, curSelected, m_currentPath, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actExtractArchive) {
        ArchiveDialog dlg(ArchiveDialog::ModeExtract, curSelected.first(), m_currentPath, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actCalculateChecksum) {
        ChecksumDialog dlg(selectedPath, this);
        dlg.exec();
    } else if (selected == actSecureShred) {
        ShredDialog dlg(curSelected, this);
        if (dlg.exec() == QDialog::Accepted) {
            refresh();
        }
    } else if (selected == actProp) {
        onShowProperties();
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

    if (m_btnFlatView && m_btnFlatView->isChecked() != enabled) {
        m_btnFlatView->blockSignals(true);
        m_btnFlatView->setChecked(enabled);
        m_btnFlatView->blockSignals(false);
    }

    if (m_treeView->selectionModel()) {
        disconnect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
    }

    if (enabled) {
        m_treeView->setModel(m_flatProxyModel);
        m_listView->setModel(m_flatProxyModel);
        m_listView->setSelectionModel(m_treeView->selectionModel());
        m_flatModel->setRootPath(m_currentPath);
        if (m_categoryWidget) m_categoryWidget->hide();

        connect(m_flatModel, &FlatFileSystemModel::scanStarted, this, [this]() {
            m_statusLabel->setText("Scanning directory recursively...");
        });
        connect(m_flatModel, &FlatFileSystemModel::scanFinished, this, &FilePanel::updateStatusText);
    } else {
        m_treeView->setModel(m_proxyModel);
        m_listView->setModel(m_proxyModel);
        m_listView->setSelectionModel(m_treeView->selectionModel());
        if (m_categoryWidget) m_categoryWidget->setVisible(m_categoryButtonsVisible);
    }

    if (m_treeView->selectionModel()) {
        connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilePanel::onSelectionChanged);
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
    if (m_viewStack->currentWidget() == m_treeView) {
        m_viewStack->setCurrentWidget(m_listView);
        
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("file_panel/view_mode_grid", true);

        onZoomChanged(m_zoomLevel);
    } else {
        m_viewStack->setCurrentWidget(m_treeView);

        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("file_panel/view_mode_grid", false);
    }
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
    
    updateStyles();

    if (m_listView->viewMode() == QListView::IconMode) {
        m_listView->setGridSize(QSize(size + 60, size + 40));
    }

    emit zoomChanged(value);
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
    
    updateStyles();

    if (m_listView->viewMode() == QListView::IconMode) {
        m_listView->setGridSize(QSize(size + 60, size + 40));
    }
}

void FilePanel::updateStyles() {
    int fonts[] = { 9, 10, 12, 14, 16, 18, 20 };
    int fontSize = fonts[qBound(0, m_zoomLevel, 6)];
    QString borderColor = m_isActive ? "#89b4fa" : "#313244";

    m_treeView->setStyleSheet(QString("QTreeView { border: 2px solid %1; font-size: %2px; }").arg(borderColor).arg(fontSize));
    m_listView->setStyleSheet(QString("QListView { border: 2px solid %1; font-size: %2px; }").arg(borderColor).arg(fontSize));
    if (m_searchResultsView) {
        m_searchResultsView->setStyleSheet(QString("QListView { border: 2px solid %1; background-color: #1e1e2e; color: #cdd6f4; font-size: %2px; }").arg(borderColor).arg(fontSize));
    }
}

void FilePanel::onHeaderContextMenu(const QPoint& pos) {
    QMenu menu(this);
    QHeaderView* header = m_treeView->header();
    
    QStringList colNames = {
        "Name", "Size", "Type", "Date Modified", 
        "Title", "Artist", "Album", "Bitrate", "Resolution", 
        "Date Taken", "Camera Model", "Genre", "Year", "Track", "Duration", "Codec"
    };

    // Standard context menu lists first 9 columns for quick toggle
    for (int i = 0; i < qMin(9, colNames.size()); ++i) {
        QAction* act = menu.addAction(colNames[i]);
        act->setCheckable(true);
        act->setChecked(!header->isSectionHidden(i));
        
        connect(act, &QAction::toggled, this, [header, i](bool checked) {
            header->setSectionHidden(i, !checked);
            QSettings settings("Amifiles", "Amifiles");
            settings.setValue(QString("columns/hidden_%1").arg(i), !checked);
        });
    }

    menu.addSeparator();
    QAction* actMore = menu.addAction("More...");

    QAction* selected = menu.exec(m_treeView->header()->mapToGlobal(pos));
    if (selected == actMore) {
        QList<bool> visibilities;
        for (int i = 0; i < colNames.size(); ++i) {
            visibilities.append(!header->isSectionHidden(i));
        }

        ColumnSelectorDialog dlg(colNames, visibilities, this);
        if (dlg.exec() == QDialog::Accepted) {
            QList<bool> newVis = dlg.selectedVisibilities();
            QSettings settings("Amifiles", "Amifiles");
            for (int i = 0; i < colNames.size(); ++i) {
                bool visible = newVis[i];
                header->setSectionHidden(i, !visible);
                settings.setValue(QString("columns/hidden_%1").arg(i), !visible);
            }
        }
    }
}

void FilePanel::setSearchQuery(const QString& query) {
    if (m_globalSearchEdit) {
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
    QStringList currentList = m_searchResultModel->stringList();
    currentList.append(results);
    m_searchResultModel->setStringList(currentList);

    m_statusLabel->setText(QString("Found %1 items").arg(currentList.size()));
}

void FilePanel::onSearchFinished() {
    int count = m_searchResultModel->stringList().size();
    m_statusLabel->setText(QString("Search finished. Found %1 items").arg(count));
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
