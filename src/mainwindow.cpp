#include "mainwindow.h"
#include "copyqueue.h"
#include "theme.h"
#include "favoritesmanager.h"
#include "syncscheduler.h"
#include "bulkrename.h"
#include "consolepanel.h"
#include "foldersync.h"
#include "dupfinder.h"
#include "helpdialog.h"
#include "spaceanalyzer.h"
#include "terminalpanel.h"
#include "keybindingseditor.h"
#include "checksumdialog.h"
#include "shreddialog.h"
#include "remotemountdialog.h"
#include "cloudmountdialog.h"
#include "imageconverterdialog.h"
#include "agestylingdialog.h"
#include "autoorganizerdialog.h"
#include "folderlayoutdialog.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "remotemountmanager.h"
#include "processmanagerdialog.h"
#include "vaultdialog.h"
#include "preferencesdialog.h"
#include "iconpickerdialog.h"
#include "version.h"
#include "texteditordialog.h"
#include "themestudiodialog.h"
#include "smartcollectiondialog.h"
#include <QMenuBar>
#include <QStorageInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QToolBar>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <QToolTip>
#include <QMessageBox>
#include <QMouseEvent>
#include <QAudioOutput>
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QRandomGenerator>
#include <QTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QListWidget>
#include <QStatusBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QResizeEvent>

#include "custombuttondialog.h"

// Mini player widget designed to sit in status bar when preview pane is toggled off
class MiniMediaControls : public QWidget {
    Q_OBJECT
public:
    MiniMediaControls(QMediaPlayer* player, QWidget* parent = nullptr) 
        : QWidget(parent), m_player(player) {
        
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(4);

        QStyle* style = QApplication::style();

        m_btnPlayPause = new QToolButton(this);
        m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
        m_btnPlayPause->setToolTip("Play/Pause");
        m_btnPlayPause->setStyleSheet("QToolButton { background-color: transparent; border: none; }");
        connect(m_btnPlayPause, &QToolButton::clicked, this, &MiniMediaControls::onPlayPause);

        m_btnStop = new QToolButton(this);
        m_btnStop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
        m_btnStop->setToolTip("Stop");
        m_btnStop->setStyleSheet("QToolButton { background-color: transparent; border: none; }");
        connect(m_btnStop, &QToolButton::clicked, this, &MiniMediaControls::onStop);

        m_lblInfo = new QLabel(this);
        m_lblInfo->setStyleSheet("color: #a6e3a1; font-size: 11px; font-weight: bold;");

        layout->addWidget(m_btnPlayPause);
        layout->addWidget(m_btnStop);
        layout->addWidget(m_lblInfo);

        // Connect player signals to keep controls in sync
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, &MiniMediaControls::onStateChanged);
        connect(m_player, &QMediaPlayer::positionChanged, this, &MiniMediaControls::onPositionChanged);
    }

    void updateTrackInfo(const QString& name, qint64 duration, bool isVideo) {
        m_trackName = name;
        m_duration = duration;
        m_isVideo = isVideo;
        updateLabel(m_player->position());
    }

private slots:
    void onPlayPause() {
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_player->pause();
        } else {
            m_player->play();
        }
    }

    void onStop() {
        m_player->stop();
    }

    void onStateChanged(QMediaPlayer::PlaybackState state) {
        QStyle* style = QApplication::style();
        if (state == QMediaPlayer::PlayingState) {
            m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPause));
        } else {
            m_btnPlayPause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
        }
    }

    void onPositionChanged(qint64 position) {
        updateLabel(position);
    }

private:
    void updateLabel(qint64 position) {
        QString prefix = m_isVideo ? "🎥" : "🎵";
        m_lblInfo->setText(QString("%1 %2 (%3 / %4)")
                           .arg(prefix)
                           .arg(m_trackName)
                           .arg(formatDuration(position))
                           .arg(formatDuration(m_duration)));
    }

    QString formatDuration(qint64 ms) const {
        qint64 totalSec = ms / 1000;
        qint64 min = totalSec / 60;
        qint64 sec = totalSec % 60;
        return QString("%1:%2")
            .arg(min, 2, 10, QChar('0'))
            .arg(sec, 2, 10, QChar('0'));
    }

    QMediaPlayer* m_player = nullptr;
    QToolButton* m_btnPlayPause = nullptr;
    QToolButton* m_btnStop = nullptr;
    QLabel* m_lblInfo = nullptr;
    
    QString m_trackName;
    qint64 m_duration = 0;
    bool m_isVideo = false;
};

bool MainWindow::isBuiltinPlayerDoubleclickActive() const {
    if (m_hasActiveFolderRule && m_activeFolderRule.overrideBuiltinPlayerDoubleclick) {
        return m_activeFolderRule.builtinPlayerDoubleclick;
    }
    QSettings settings("Amifiles", "Amifiles");
    return settings.value("preferences/builtin_player_doubleclick", false).toBool();
}

void MainWindow::setBuiltinPlayerDoubleclickActive(bool active) {
    if (m_hasActiveFolderRule && m_activeFolderRule.overrideBuiltinPlayerDoubleclick) {
        m_activeFolderRule.builtinPlayerDoubleclick = active;
        saveFolderRules();
    }
    
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/builtin_player_doubleclick", active);
    
    emit builtinPlayerDoubleclickChanged(active);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_isInitializing = true;
    setWindowTitle("Amifiles - File Manager");
    resize(1200, 800);

    // Apply global modern theme
    qApp->setStyleSheet(Theme::getStylesheet());

    // Initialize layout components
    setupActions();
    setupCentralWidget();
    loadKeybindings();
    applyKeybindings();
    setupMenus();
    setupToolbars();

    // Connect favorites changes to keep menu and sidebar updated
    connect(&FavoritesManager::instance(), &FavoritesManager::favoritesChanged,
            this, &MainWindow::updateFavoritesMenu);
    connect(&FavoritesManager::instance(), &FavoritesManager::favoritesChanged,
            this, &MainWindow::refreshFavoritesSidebar);
    updateFavoritesMenu();

    // Start background sync scheduler and link notifications
    connect(&SyncScheduler::instance(), &SyncScheduler::syncStarted, this, [this](const QString& jobName) {
        statusBar()->showMessage(QString("Backup Sync '%1' started...").arg(jobName), 4000);
    });
    connect(&SyncScheduler::instance(), &SyncScheduler::syncFinished, this, [this](const QString& jobName, bool success) {
        statusBar()->showMessage(QString("Backup Sync '%1' completed: %2")
                                 .arg(jobName)
                                 .arg(success ? "SUCCESS" : "FAILED"), 5000);
    });
    SyncScheduler::instance().start();

    // Start background Auto-Organizer Rules scheduler (runs every 30 seconds)
    QTimer* organizerTimer = new QTimer(this);
    connect(organizerTimer, &QTimer::timeout, this, []() {
        AutoOrganizerDialog::executeRules();
    });
    organizerTimer->start(30000); // 30 seconds

    // Load custom buttons & build custom toolbar
    loadCustomButtons();
    rebuildCustomToolBar();

    // Load drives menu & toolbar visibility from settings
    QSettings settings("Amifiles", "Amifiles");
    bool dualPaneVisible = settings.value("preferences/dual_pane", true).toBool();
    m_actToggleDualPane->setChecked(dualPaneVisible);
    m_isDualPane = dualPaneVisible;
    if (m_rightTabWidget) m_rightTabWidget->setVisible(dualPaneVisible);

    bool drivesMenuVisible = settings.value("drives/menu_visible", true).toBool();
    bool drivesToolbarVisible = settings.value("drives/toolbar_visible", true).toBool();

    m_actToggleDrivesMenu->setChecked(drivesMenuVisible);
    m_menuDrives->menuAction()->setVisible(drivesMenuVisible);

    m_actToggleDrivesToolbar->setChecked(drivesToolbarVisible);
    if (m_tbDrives) m_tbDrives->setVisible(drivesToolbarVisible);

    bool centerOpsVisible = settings.value("layout/center_ops_visible", true).toBool();
    m_actToggleCenterOps->setChecked(centerOpsVisible);
    if (m_tbCenterOps) m_tbCenterOps->setVisible(centerOpsVisible && m_isDualPane);

    bool favoritesSidebarVisible = settings.value("favorites/sidebar_visible", false).toBool();
    m_actToggleFavoritesSidebar->setChecked(favoritesSidebarVisible);
    m_sidebarTabWidget->setVisible(favoritesSidebarVisible);

    bool previewMuted = settings.value("preview/muted", false).toBool();
    m_actMutePreview->setChecked(previewMuted);
    if (m_previewPanel) {
        m_previewPanel->setMuted(previewMuted);
    }

    bool archiveNav = settings.value("preferences/archive_nav", true).toBool();
    m_actToggleArchiveNav->setChecked(archiveNav);

    bool archiveWrite = settings.value("preferences/archive_write", false).toBool();
    m_actToggleArchiveWrite->setChecked(archiveWrite);

    bool horizontalSplit = settings.value("preferences/horizontal_split", false).toBool();
    m_actToggleHorizontalSplit->setChecked(horizontalSplit);
    onToggleHorizontalSplit(horizontalSplit);

    bool casingOverlays = settings.value("preferences/casing_overlays", true).toBool();
    m_actToggleCasingOverlays->setChecked(casingOverlays);

    bool syncScroll = settings.value("preferences/sync_scroll", false).toBool();
    m_actToggleSyncScroll->setChecked(syncScroll);

    bool showAudioCover = settings.value("preview/show_audio_cover_art", true).toBool();
    m_actShowAudioCoverArt->setChecked(showAudioCover);

    bool showSpectrum = settings.value("preview/show_spectrum_visualizer", true).toBool();
    m_actToggleSpectrum->setChecked(showSpectrum);
    if (m_previewPanel) {
        m_previewPanel->setSpectrumVisualizerVisible(showSpectrum);
    }

    bool consoleVisible = settings.value("console/visible", true).toBool();
    m_actToggleConsole->setChecked(consoleVisible);
    if (m_bottomTabWidget) {
        m_bottomTabWidget->setVisible(consoleVisible);
    }

    // Load individual filter elements visibility from settings
    bool leftFilterTextVisible = settings.value("left_panel/filter_text_visible", true).toBool();
    bool leftCategoryVisible = settings.value("left_panel/category_buttons_visible", true).toBool();
    bool rightFilterTextVisible = settings.value("right_panel/filter_text_visible", true).toBool();
    bool rightCategoryVisible = settings.value("right_panel/category_buttons_visible", true).toBool();

    m_actLeftShowFilterText->setChecked(leftFilterTextVisible);
    if (leftPanel()) leftPanel()->setFilterTextBarVisible(leftFilterTextVisible);

    m_actLeftShowCategoryButtons->setChecked(leftCategoryVisible);
    if (leftPanel()) leftPanel()->setCategoryButtonsVisible(leftCategoryVisible);

    m_actRightShowFilterText->setChecked(rightFilterTextVisible);
    if (rightPanel()) rightPanel()->setFilterTextBarVisible(rightFilterTextVisible);

    m_actRightShowCategoryButtons->setChecked(rightCategoryVisible);
    if (rightPanel()) rightPanel()->setCategoryButtonsVisible(rightCategoryVisible);

    // Initial populate of drives and custom menus
    updateDrivesList();
    rebuildCustomMenus();

    // Default active panel is Left
    onPanelActivated(leftPanel());

    // Load folder rules first to determine if we should restore custom folder layout state
    loadFolderRules();

    bool bypassRules = settings.value("preferences/bypass_folder_rules", false).toBool();
    if (m_actBypassFolderProfiles) {
        m_actBypassFolderProfiles->setChecked(bypassRules);
    }

    FolderLayoutRule matchedRule;
    bool foundMatch = false;
    if (m_activePanel && !bypassRules) {
        QString startingPath = m_activePanel->currentPath();
        for (const auto& r : m_folderRules) {
            if (!r.autoApply) continue;
            if (r.ruleType == "Path" && r.value == startingPath) {
                matchedRule = r;
                foundMatch = true;
                break;
            }
        }
        if (!foundMatch) {
            QString category = detectFolderCategory(startingPath);
            if (!category.isEmpty()) {
                for (const auto& r : m_folderRules) {
                    if (!r.autoApply) continue;
                    if (r.ruleType == "Category" && r.value == category) {
                        matchedRule = r;
                        foundMatch = true;
                        break;
                    }
                }
            }
        }
        if (!foundMatch) {
            for (const auto& r : m_folderRules) {
                if (r.name.toLower() == "default") {
                    matchedRule = r;
                    foundMatch = true;
                    break;
                }
            }
        }
    }

    // Determine state to restore
    bool autoSaveLayout = settings.value("layout/auto_save", true).toBool();
    if (m_actAutoSaveLayout) {
        m_actAutoSaveLayout->setChecked(autoSaveLayout);
    }

    QByteArray stateToRestore;
    if (foundMatch && !matchedRule.windowState.isEmpty()) {
        stateToRestore = matchedRule.windowState;
    } else {
        stateToRestore = settings.value("window/state").toByteArray();
    }

    if (autoSaveLayout || settings.contains("window/geometry")) {
        QByteArray geom = settings.value("window/geometry").toByteArray();
        if (!geom.isEmpty()) {
            restoreGeometry(geom);
        }
        if (!stateToRestore.isEmpty()) {
            restoreState(stateToRestore);
        }
    }

    bool zenActive = settings.value("preferences/zen_mode", false).toBool();
    setZenMode(zenActive);

    // Apply remaining folder profile options (skipping redundant restoreState)
    if (m_activePanel) {
        applyFolderRules(m_activePanel->currentPath());
    }

    m_isInitializing = false;

    updateWidgetStylesheets();
    updateTooltips();
}

void MainWindow::setupCentralWidget() {
    QSplitter* mainVSplitter = new QSplitter(Qt::Vertical, this);
    mainVSplitter->setHandleWidth(4);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(4);

    m_sidebarTabWidget = new QTabWidget(this);
    m_sidebarTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: #1e1e2e; border-radius: 6px; }"
        "QTabBar::tab { background-color: #181825; color: #a6adc8; padding: 6px 10px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #313244; color: #cdd6f4; }"
    );

    m_favoritesSidebar = new QListWidget(m_sidebarTabWidget);
    m_favoritesSidebar->setStyleSheet(
        "QListWidget { background-color: #181825; color: #cdd6f4; border: none; padding: 4px; }"
        "QListWidget::item { padding: 6px 8px; border-radius: 4px; color: #cdd6f4; }"
        "QListWidget::item:hover { background-color: #313244; color: #f5c2e7; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
    );
    connect(m_favoritesSidebar, &QListWidget::itemDoubleClicked, this, &MainWindow::onFavoritesSidebarDoubleClicked);

    m_filtersSidebar = new QListWidget(m_sidebarTabWidget);
    m_filtersSidebar->setStyleSheet(m_favoritesSidebar->styleSheet());

    // Add preset quick filters
    m_filtersSidebar->addItem("Show All Files");
    m_filtersSidebar->addItem("Large Files (> 100 MB)");
    m_filtersSidebar->addItem("Medium Files (1 - 100 MB)");
    m_filtersSidebar->addItem("Small Files (< 1 MB)");
    m_filtersSidebar->addItem("Modified Today");
    m_filtersSidebar->addItem("Modified This Week");
    m_filtersSidebar->addItem("Modified This Month");
    connect(m_filtersSidebar, &QListWidget::itemClicked, this, &MainWindow::onQuickFilterSidebarClicked);

    m_tagsSidebar = new QListWidget(m_sidebarTabWidget);
    m_tagsSidebar->setStyleSheet(m_favoritesSidebar->styleSheet());
    connect(m_tagsSidebar, &QListWidget::itemClicked, this, &MainWindow::onTagsSidebarClicked);

    m_sidebarTabWidget->addTab(m_favoritesSidebar, "Bookmarks");
    m_sidebarTabWidget->addTab(m_filtersSidebar, "Filters");
    m_sidebarTabWidget->addTab(m_tagsSidebar, "Tags");

    refreshTagsSidebar();

    m_leftTabWidget = new QTabWidget(this);
    m_leftTabWidget->setObjectName("leftTabWidget");
    m_leftTabWidget->setTabsClosable(true);
    m_leftTabWidget->setMovable(true);
    m_leftTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: #1e1e2e; }"
        "QTabBar::tab { background-color: #181825; color: #a6adc8; border: 1px solid #313244; border-bottom: none; padding: 6px 12px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #1e1e2e; color: #cdd6f4; border-color: #313244; }"
        "QTabBar::tab:hover { background-color: #313244; color: #cdd6f4; }"
    );

    m_rightTabWidget = new QTabWidget(this);
    m_rightTabWidget->setObjectName("rightTabWidget");
    m_rightTabWidget->setTabsClosable(true);
    m_rightTabWidget->setMovable(true);
    m_rightTabWidget->setStyleSheet(m_leftTabWidget->styleSheet());

    m_previewPanel = new PreviewPanel(this);
    m_previewPanel->setZenMode(m_zenMode);
    connect(m_previewPanel, &PreviewPanel::spectrumVisualizerToggled, this, &MainWindow::onToggleSpectrum);
    connect(m_previewPanel, &PreviewPanel::tagsChanged, this, [this](const QString&) {
        // Refresh active panel to show modified tags
        if (m_activePanel) m_activePanel->refresh();
    });
    connect(m_previewPanel, &PreviewPanel::builtinPlayerDoubleclickToggled, this, &MainWindow::setBuiltinPlayerDoubleclickActive);
    connect(this, &MainWindow::builtinPlayerDoubleclickChanged, m_previewPanel, &PreviewPanel::setBuiltinPlayerDoubleclickActive);
    connect(m_previewPanel, &PreviewPanel::fullscreenExited, this, [this]() {
        if (m_actTogglePreview && !m_actTogglePreview->isChecked()) {
            m_previewPanel->clearPreview();
        }
    });

    m_previewDock = new QDockWidget("File Preview Panel", this);
    m_previewDock->setObjectName("previewDockWidget");
    m_previewDock->setWidget(m_previewPanel);
    m_previewDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    m_previewDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_previewDock);

    m_fullscreenQueueDock = new QDockWidget("Playback Queue", this);
    m_fullscreenQueueDock->setObjectName("fullscreenQueueDockWidget");
    m_fullscreenQueueDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_fullscreenQueueDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    
    QWidget* queueContent = new QWidget(m_fullscreenQueueDock);
    QVBoxLayout* queueLayout = new QVBoxLayout(queueContent);
    queueLayout->setContentsMargins(12, 12, 12, 12);
    queueLayout->setSpacing(8);
    
    QLabel* lblHeader = new QLabel("📋 CURRENT PLAYLIST", queueContent);
    lblHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #89b4fa; letter-spacing: 1px;");
    queueLayout->addWidget(lblHeader);
    
    m_fullscreenQueueList = new QListWidget(queueContent);
    m_fullscreenQueueList->setStyleSheet(
        "QListWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 8px; padding: 4px; font-family: 'Outfit'; font-size: 13px; } "
        "QListWidget::item { padding: 8px; border-bottom: 1px solid rgba(255, 255, 255, 0.05); border-radius: 4px; } "
        "QListWidget::item:hover { background-color: rgba(137, 180, 250, 0.15); color: #ffffff; } "
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
    );
    queueLayout->addWidget(m_fullscreenQueueList);
    
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    
    QPushButton* btnPlay = new QPushButton("▶ Play", queueContent);
    QPushButton* btnRemove = new QPushButton("✖ Remove", queueContent);
    QPushButton* btnClear = new QPushButton("🗑 Clear", queueContent);
    
    QString btnStyle = 
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 6px 12px; font-weight: bold; font-family: 'Outfit'; font-size: 12px; } "
        "QPushButton:hover { background-color: #45475a; color: #ffffff; } "
        "QPushButton:pressed { background-color: #585b70; }";
        
    btnPlay->setStyleSheet(btnStyle);
    btnRemove->setStyleSheet(btnStyle);
    btnClear->setStyleSheet(btnStyle);
    
    btnRow->addWidget(btnPlay);
    btnRow->addWidget(btnRemove);
    btnRow->addWidget(btnClear);
    queueLayout->addLayout(btnRow);
    
    m_fullscreenQueueDock->setWidget(queueContent);
    addDockWidget(Qt::RightDockWidgetArea, m_fullscreenQueueDock);
    m_fullscreenQueueDock->setVisible(false);

    connect(m_fullscreenQueueList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        int idx = m_fullscreenQueueList->row(item);
        m_previewPanel->playPlaylistIndex(idx);
        if (!m_previewPanel->isFullscreen()) {
            m_previewPanel->toggleFullscreen();
        }
    });
    
    connect(btnPlay, &QPushButton::clicked, this, [this]() {
        int idx = m_fullscreenQueueList->currentRow();
        if (idx >= 0) {
            m_previewPanel->playPlaylistIndex(idx);
            if (!m_previewPanel->isFullscreen()) {
                m_previewPanel->toggleFullscreen();
            }
        }
    });
    
    connect(btnRemove, &QPushButton::clicked, this, [this]() {
        int idx = m_fullscreenQueueList->currentRow();
        if (idx >= 0) {
            m_previewPanel->removeFromPlaylist(idx);
        }
    });
    
    connect(btnClear, &QPushButton::clicked, this, [this]() {
        m_previewPanel->clearPlaylist();
    });

    connect(m_previewPanel, &PreviewPanel::playlistChanged, this, &MainWindow::syncFullscreenQueue);

    m_previewDock->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_previewDock, &QDockWidget::customContextMenuRequested, this, &MainWindow::onPreviewDockContextMenu);

    connect(m_previewDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_showPreview = visible;
        m_actTogglePreview->setChecked(visible);
        if (!visible && m_previewPanel && m_previewPanel->player()) {
            m_previewPanel->player()->pause();
        }
        if (visible && m_previewPanel) {
            m_previewPanel->update();
            m_previewPanel->repaint();
            QMetaObject::invokeMethod(m_previewPanel, "updateGeometry");
        }
        updateMiniPlayer();
    });

    // Add first tabs
    createTab(m_leftTabWidget, QDir::homePath());
    createTab(m_rightTabWidget, QDir::homePath());

    m_tbCenterOps = new QFrame(this);
    m_tbCenterOps->setObjectName("centerOpsBarWidget");
    m_tbCenterOps->setFrameShape(QFrame::NoFrame);
    m_tbCenterOps->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 4px;");
    m_tbCenterOps->setFixedWidth(38);
    
    QBoxLayout* centerLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_tbCenterOps);
    centerLayout->setContentsMargins(4, 8, 4, 8);
    centerLayout->setSpacing(8);
    centerLayout->setAlignment(Qt::AlignTop);

    auto createBarButton = [this](QAction* act) -> QToolButton* {
        QToolButton* btn = new QToolButton(m_tbCenterOps);
        btn->setDefaultAction(act);
        if (act) {
            btn->setIcon(act->icon());
            btn->setToolTip(act->text() + (act->shortcut().isEmpty() ? "" : " (" + act->shortcut().toString() + ")"));
        }
        btn->setFixedSize(30, 30);
        btn->setIconSize(QSize(20, 20));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setStyleSheet(
            "QToolButton { background-color: #1e1e2e; border: 1px solid #45475a; border-radius: 4px; padding: 2px; color: #89b4fa; }"
            "QToolButton:hover { background-color: #313244; color: #a6e3a1; }"
        );
        return btn;
    };

    centerLayout->addWidget(createBarButton(m_actCopy));
    centerLayout->addWidget(createBarButton(m_actMoveToSibling));
    centerLayout->addWidget(createBarButton(m_actCut));
    centerLayout->addWidget(createBarButton(m_actPaste));

    m_tbCenterOpsSeparator = new QFrame(m_tbCenterOps);
    m_tbCenterOpsSeparator->setFrameShape(QFrame::HLine);
    m_tbCenterOpsSeparator->setFrameShadow(QFrame::Sunken);
    m_tbCenterOpsSeparator->setStyleSheet("background-color: #45475a; max-height: 1px;");
    centerLayout->addWidget(m_tbCenterOpsSeparator);

    centerLayout->addWidget(createBarButton(m_actDelete));
    centerLayout->addWidget(createBarButton(m_actRename));
    centerLayout->addWidget(createBarButton(m_actRefresh));

    m_dualSplitter = new QSplitter(Qt::Horizontal, this);
    m_dualSplitter->setHandleWidth(4);
    m_dualSplitter->addWidget(m_leftTabWidget);
    m_dualSplitter->addWidget(m_tbCenterOps);
    m_tbCenterOps->setVisible(m_actToggleCenterOps->isChecked() && m_isDualPane);
    m_dualSplitter->addWidget(m_rightTabWidget);

    m_splitter->addWidget(m_sidebarTabWidget);
    m_splitter->addWidget(m_dualSplitter);

    m_splitter->setSizes({160, 1040});

    m_bottomTabWidget = new QTabWidget(this);
    m_bottomTabWidget->setTabPosition(QTabWidget::South);
    m_bottomTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: #11111b; }"
        "QTabBar::tab { background-color: #181825; color: #a6adc8; border: 1px solid #313244; padding: 4px 10px; }"
        "QTabBar::tab:selected { background-color: #11111b; color: #cdd6f4; }"
    );

    m_consolePanel = new ConsolePanel(this);
    m_bottomTabWidget->addTab(m_consolePanel, "Command Output Console");

    m_terminalPanel = new TerminalPanel(this);
    m_terminalPanel->startShell(QDir::homePath());
    m_bottomTabWidget->addTab(m_terminalPanel, "Interactive Bash Shell");

    mainVSplitter->addWidget(m_splitter);
    mainVSplitter->addWidget(m_bottomTabWidget);
    mainVSplitter->setSizes({600, 150});

    setCentralWidget(mainVSplitter);

    refreshFavoritesSidebar();

    // Connect tab change and close signals
    connect(m_leftTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onLeftTabChanged);
    connect(m_rightTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onRightTabChanged);
    connect(m_leftTabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_rightTabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    m_leftTabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_rightTabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_leftTabWidget, &QTabWidget::customContextMenuRequested, this, &MainWindow::onTabContextMenuRequested);
    connect(m_rightTabWidget, &QTabWidget::customContextMenuRequested, this, &MainWindow::onTabContextMenuRequested);

    updateSiblingLinks();

    // Initialize mini media controller as a floating overlay HUD
    m_miniMediaControls = new MiniMediaControls(m_previewPanel->player(), this);
    m_miniMediaControls->hide();

    // Wire player notifications to keep mini status bar controller synchronized
    connect(m_previewPanel->player(), &QMediaPlayer::sourceChanged, this, &MainWindow::updateMiniPlayer);
    connect(m_previewPanel->player(), &QMediaPlayer::durationChanged, this, &MainWindow::updateMiniPlayer);
    connect(m_previewPanel->player(), &QMediaPlayer::playbackStateChanged, this, &MainWindow::onMainPlayerStateChanged);
}

void MainWindow::setupActions() {
    QStyle* style = QApplication::style();

    // File Actions
    m_actNewFolder = new QAction(style->standardIcon(QStyle::SP_FileDialogNewFolder), "New Folder", this);
    m_actNewFolder->setShortcuts({ QKeySequence::New, QKeySequence(Qt::Key_F7) });
    m_actNewFolder->setToolTip("Create a new folder in active directory (F7)");
    connect(m_actNewFolder, &QAction::triggered, this, &MainWindow::onNewFolderAction);

    m_actProperties = new QAction(style->standardIcon(QStyle::SP_MessageBoxInformation), "Properties", this);
    m_actProperties->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    m_actProperties->setToolTip("Show properties for selected item");
    connect(m_actProperties, &QAction::triggered, this, &MainWindow::onShowPropertiesAction);

    // Edit Actions
    m_actCut = new QAction(style->standardIcon(QStyle::SP_DialogDiscardButton), "Cut", this);
    m_actCut->setShortcut(QKeySequence::Cut);
    m_actCut->setToolTip("Cut selected items to clipboard");
    connect(m_actCut, &QAction::triggered, this, &MainWindow::onCutAction);

    m_actCopy = new QAction(style->standardIcon(QStyle::SP_DialogSaveButton), "Copy", this);
    m_actCopy->setShortcut(QKeySequence::Copy);
    m_actCopy->setToolTip("Copy selected items to clipboard");
    connect(m_actCopy, &QAction::triggered, this, &MainWindow::onCopyAction);

    m_actPaste = new QAction(style->standardIcon(QStyle::SP_DialogOpenButton), "Paste", this);
    m_actPaste->setShortcut(QKeySequence::Paste);
    m_actPaste->setToolTip("Paste items from clipboard");
    connect(m_actPaste, &QAction::triggered, this, &MainWindow::onPasteAction);

    m_actDelete = new QAction(style->standardIcon(QStyle::SP_TrashIcon), "Delete", this);
    m_actDelete->setShortcuts({ QKeySequence::Delete, QKeySequence(Qt::Key_F8) });
    m_actDelete->setToolTip("Permanently delete selected items (F8)");
    connect(m_actDelete, &QAction::triggered, this, &MainWindow::onDeleteAction);

    m_actRename = new QAction(style->standardIcon(QStyle::SP_FileDialogInfoView), "Rename", this);
    m_actRename->setShortcut(QKeySequence(Qt::Key_F2));
    m_actRename->setToolTip("Rename selected item");
    connect(m_actRename, &QAction::triggered, this, &MainWindow::onRenameAction);

    m_actEdit = new QAction(style->standardIcon(QStyle::SP_FileIcon), "Edit Selected File", this);
    m_actEdit->setShortcut(QKeySequence(Qt::Key_F4));
    m_actEdit->setToolTip("Edit selected file in inline/built-in editor (F4)");
    connect(m_actEdit, &QAction::triggered, this, &MainWindow::onEditAction);

    m_actRefresh = new QAction(style->standardIcon(QStyle::SP_BrowserReload), "Refresh", this);
    m_actRefresh->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_actRefresh->setToolTip("Refresh current directory listing");
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::onRefreshAction);

    m_actBulkRename = new QAction(style->standardIcon(QStyle::SP_DialogResetButton), "Bulk Rename...", this);
    m_actBulkRename->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    m_actBulkRename->setToolTip("Open the bulk rename tool for selected files");
    connect(m_actBulkRename, &QAction::triggered, this, &MainWindow::onBulkRenameAction);

    m_actCopyToSibling = new QAction(style->standardIcon(QStyle::SP_DialogSaveButton), "Copy to Sibling", this);
    m_actCopyToSibling->setShortcut(QKeySequence(Qt::Key_F5));
    m_actCopyToSibling->setToolTip("Copy selected items to the opposite panel (F5)");
    connect(m_actCopyToSibling, &QAction::triggered, this, &MainWindow::onCopyToSiblingAction);

    m_actMoveToSibling = new QAction(style->standardIcon(QStyle::SP_DialogDiscardButton), "Move to Sibling", this);
    m_actMoveToSibling->setShortcut(QKeySequence(Qt::Key_F6));
    m_actMoveToSibling->setToolTip("Move selected items to the opposite panel (F6)");
    connect(m_actMoveToSibling, &QAction::triggered, this, &MainWindow::onMoveToSiblingAction);

    m_actClonePathToSibling = new QAction(style->standardIcon(QStyle::SP_FileDialogContentsView), "Clone Path to Sibling", this);
    m_actClonePathToSibling->setShortcut(QKeySequence(Qt::Key_F9));
    m_actClonePathToSibling->setToolTip("Navigate the opposite panel to the active panel's directory (F9)");
    connect(m_actClonePathToSibling, &QAction::triggered, this, [this]() {
        if (m_activePanel) {
            onClonePathRequested(m_activePanel->currentPath());
        }
    });

    m_actCompareSync = new QAction(style->standardIcon(QStyle::SP_DialogYesButton), "Compare & Sync Folders...", this);
    m_actCompareSync->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    m_actCompareSync->setToolTip("Compare left and right folder contents and synchronize them");
    m_actCompareSync->setStatusTip("Compare left and right folder contents and synchronize them");
    connect(m_actCompareSync, &QAction::triggered, this, &MainWindow::onCompareSyncAction);

    m_actDuplicateFinder = new QAction(style->standardIcon(QStyle::SP_MessageBoxQuestion), "Find Duplicate Files...", this);
    m_actDuplicateFinder->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    m_actDuplicateFinder->setToolTip("Find duplicate files by size or hash in directories");
    m_actDuplicateFinder->setStatusTip("Find duplicate files by size or hash in directories");
    connect(m_actDuplicateFinder, &QAction::triggered, this, &MainWindow::onDuplicateFinderAction);

    // Layout Toggle Actions
    m_actToggleDualPane = new QAction("Dual Pane View", this);
    m_actToggleDualPane->setCheckable(true);
    m_actToggleDualPane->setChecked(true);
    m_actToggleDualPane->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_actToggleDualPane->setToolTip("Toggle between single and dual pane file display");
    m_actToggleDualPane->setStatusTip("Toggle between single and dual pane file display");
    connect(m_actToggleDualPane, &QAction::toggled, this, &MainWindow::onToggleDualPane);

    m_actTogglePreview = new QAction("Preview Pane", this);
    m_actTogglePreview->setCheckable(true);
    m_actTogglePreview->setChecked(true);
    m_actTogglePreview->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_P), QKeySequence(Qt::Key_F3) });
    m_actTogglePreview->setToolTip("Toggle the file preview panel on/off (F3 / Ctrl+P)");
    m_actTogglePreview->setStatusTip("Toggle the file preview panel on/off (F3 / Ctrl+P)");
    connect(m_actTogglePreview, &QAction::toggled, this, &MainWindow::onTogglePreview);

    m_actCommandPalette = new QAction("Command Palette...", this);
    m_actCommandPalette->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    m_actCommandPalette->setToolTip("Open spotlight command palette dialog");
    m_actCommandPalette->setStatusTip("Open spotlight command palette dialog");
    connect(m_actCommandPalette, &QAction::triggered, this, &MainWindow::onCommandPaletteAction);

    m_actMutePreview = new QAction("Mute Preview Audio", this);
    m_actMutePreview->setCheckable(true);
    m_actMutePreview->setChecked(false);
    m_actMutePreview->setToolTip("Mute the file preview panel audio output");
    connect(m_actMutePreview, &QAction::toggled, this, &MainWindow::onMutePreview);

    // Age Coloring Highlights Toggle Action
    m_actToggleAgeColoring = new QAction("Enable File Age Overlay", this);
    m_actToggleAgeColoring->setCheckable(true);
    m_actToggleAgeColoring->setChecked(true);
    m_actToggleAgeColoring->setToolTip("Highlight files by customized age color & emoji rules");
    connect(m_actToggleAgeColoring, &QAction::toggled, this, &MainWindow::onToggleAgeColoring);

    m_actConfigureAgeStyling = new QAction("Configure File Age Styles...", this);
    m_actConfigureAgeStyling->setToolTip("Set custom time thresholds, text colors, and emoji badges for file age");
    connect(m_actConfigureAgeStyling, &QAction::triggered, this, &MainWindow::onConfigureAgeStyling);

    m_actToggleArchiveNav = new QAction("Enable Archive Navigation", this);
    m_actToggleArchiveNav->setCheckable(true);
    m_actToggleArchiveNav->setChecked(true);
    m_actToggleArchiveNav->setToolTip("Allows browsing archives (.zip, .tar.gz, etc.) like directories");
    connect(m_actToggleArchiveNav, &QAction::toggled, this, &MainWindow::onToggleArchiveNav);

    m_actToggleArchiveWrite = new QAction("Allow Modifications to Archives / Disk Images", this);
    m_actToggleArchiveWrite->setCheckable(true);
    m_actToggleArchiveWrite->setChecked(false);
    m_actToggleArchiveWrite->setToolTip("Allows drag-and-drop file additions and deletions inside archives and disk images");
    connect(m_actToggleArchiveWrite, &QAction::toggled, this, &MainWindow::onToggleArchiveWrite);

    m_actToggleHorizontalSplit = new QAction("Split Panels Horizontally (Top/Bottom)", this);
    m_actToggleHorizontalSplit->setCheckable(true);
    m_actToggleHorizontalSplit->setChecked(false);
    m_actToggleHorizontalSplit->setToolTip("Stacks file panels vertically: Right panel on Top, Left panel on Bottom");
    connect(m_actToggleHorizontalSplit, &QAction::toggled, this, &MainWindow::onToggleHorizontalSplit);

    m_actToggleCasingOverlays = new QAction("Enable Media Casing Overlays", this);
    m_actToggleCasingOverlays->setCheckable(true);
    m_actToggleCasingOverlays->setChecked(true);
    m_actToggleCasingOverlays->setToolTip("Renders DVD/CD case overlays over folders containing cover art");
    connect(m_actToggleCasingOverlays, &QAction::toggled, this, &MainWindow::onToggleCasingOverlays);

    // Toggle Drives Menu Action
    m_actToggleDrivesMenu = new QAction("Attached Drives Menu", this);
    m_actToggleDrivesMenu->setCheckable(true);
    m_actToggleDrivesMenu->setChecked(true);
    m_actToggleDrivesMenu->setToolTip("Show/hide the Drives menu in the menu bar");
    connect(m_actToggleDrivesMenu, &QAction::toggled, this, &MainWindow::onToggleDrivesMenu);

    // Toggle Drives Toolbar Action
    m_actToggleDrivesToolbar = new QAction("Drives Toolbar", this);
    m_actToggleDrivesToolbar->setCheckable(true);
    m_actToggleDrivesToolbar->setChecked(true);
    m_actToggleDrivesToolbar->setToolTip("Show/hide the attached drives toolbar");
    connect(m_actToggleDrivesToolbar, &QAction::toggled, this, &MainWindow::onToggleDrivesToolbar);

    m_actToggleCenterOps = new QAction("Central Operations Button Bar", this);
    m_actToggleCenterOps->setCheckable(true);
    m_actToggleCenterOps->setChecked(true);
    m_actToggleCenterOps->setToolTip("Show/hide a vertical file operations buttons bar situated between the left and right panels");
    connect(m_actToggleCenterOps, &QAction::toggled, this, &MainWindow::onToggleCenterOps);

    m_actToggleSyncScroll = new QAction("Synchronize Scrolling", this);
    m_actToggleSyncScroll->setCheckable(true);
    m_actToggleSyncScroll->setChecked(false);
    m_actToggleSyncScroll->setToolTip("Scroll left and right folder panels in parallel (Ctrl+Shift+S)");
    m_actToggleSyncScroll->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_actToggleSyncScroll, &QAction::toggled, this, &MainWindow::onToggleSyncScroll);

    // Toggle Favorites Sidebar Action
    m_actToggleFavoritesSidebar = new QAction("Favorites Sidebar", this);
    m_actToggleFavoritesSidebar->setCheckable(true);
    m_actToggleFavoritesSidebar->setChecked(true);
    m_actToggleFavoritesSidebar->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    m_actToggleFavoritesSidebar->setToolTip("Show/hide the Favorites sidebar");
    connect(m_actToggleFavoritesSidebar, &QAction::toggled, this, &MainWindow::onToggleFavoritesSidebar);

    // Toggle Console Action
    m_actToggleConsole = new QAction("Command Output Console", this);
    m_actToggleConsole->setCheckable(true);
    m_actToggleConsole->setChecked(true);
    m_actToggleConsole->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    m_actToggleConsole->setToolTip("Toggle command script output console window");
    connect(m_actToggleConsole, &QAction::toggled, this, &MainWindow::onToggleConsole);

    m_actShowAudioCoverArt = new QAction("Show Audio Cover Art in Preview", this);
    m_actShowAudioCoverArt->setCheckable(true);
    m_actShowAudioCoverArt->setChecked(true);
    m_actShowAudioCoverArt->setToolTip("Toggle displaying audio album art background inside the preview panel");
    connect(m_actShowAudioCoverArt, &QAction::toggled, this, &MainWindow::onToggleAudioCoverArt);

    m_actToggleSpectrum = new QAction("Show Spectrum Visualizer in Preview", this);
    m_actToggleSpectrum->setCheckable(true);
    m_actToggleSpectrum->setChecked(true);
    m_actToggleSpectrum->setToolTip("Toggle displaying the bouncy retro spectrum visualizer on music playback");
    connect(m_actToggleSpectrum, &QAction::toggled, this, &MainWindow::onToggleSpectrum);

    m_actAutoSaveLayout = new QAction("Auto-save Layout on Close", this);
    m_actAutoSaveLayout->setCheckable(true);
    m_actAutoSaveLayout->setChecked(true);
    m_actAutoSaveLayout->setToolTip("Automatically persist toolbar positions and window dimensions when closing");
    connect(m_actAutoSaveLayout, &QAction::toggled, this, [](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("layout/auto_save", checked);
    });

    m_actPreferences = new QAction("Preferences...", this);
    m_actPreferences->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    m_actPreferences->setToolTip("Open unified preferences control center dialog");
    m_actPreferences->setStatusTip("Open unified preferences control center dialog");
    connect(m_actPreferences, &QAction::triggered, this, &MainWindow::onPreferencesAction);

    m_actMediaPreferences = new QAction("Media Player Preferences...", this);
    m_actMediaPreferences->setToolTip("Open preferences on the Media Player tab");
    m_actMediaPreferences->setStatusTip("Open preferences on the Media Player tab");
    connect(m_actMediaPreferences, &QAction::triggered, this, &MainWindow::onMediaPreferences);

    m_actBypassFolderProfiles = new QAction("Bypass Folder Profiles", this);
    m_actBypassFolderProfiles->setCheckable(true);
    m_actBypassFolderProfiles->setToolTip("Temporarily ignore all folder profiles/rules for debugging layout issues");
    m_actBypassFolderProfiles->setStatusTip("Temporarily ignore all folder profiles/rules for debugging layout issues");
    connect(m_actBypassFolderProfiles, &QAction::triggered, this, [this](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preferences/bypass_folder_rules", checked);
        if (m_activePanel) {
            applyFolderRules(m_activePanel->currentPath());
        }
    });

    m_actThemeStudio = new QAction("Catppuccin Theme Studio...", this);
    m_actThemeStudio->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    m_actThemeStudio->setToolTip("Open Catppuccin theme customizer (Ctrl+Shift+T)");
    m_actThemeStudio->setStatusTip("Open Catppuccin theme customizer (Ctrl+Shift+T)");
    connect(m_actThemeStudio, &QAction::triggered, this, &MainWindow::onThemeStudioAction);

    m_actConfigureCustomMenus = new QAction(style->standardIcon(QStyle::SP_CustomBase), "Configure Custom Menus...", this);
    m_actConfigureCustomMenus->setToolTip("Create custom top-level menus, submenus, separators, and app launchers");
    m_actConfigureCustomMenus->setStatusTip("Create custom top-level menus, submenus, separators, and app launchers");
    connect(m_actConfigureCustomMenus, &QAction::triggered, this, &MainWindow::onConfigureCustomMenus);

    m_actConfigureToolbars = new QAction(style->standardIcon(QStyle::SP_CustomBase), "Configure Toolbars...", this);
    m_actConfigureToolbars->setToolTip("Add, remove, or customize multiple floating/dockable toolbars");
    m_actConfigureToolbars->setStatusTip("Add, remove, or customize multiple floating/dockable toolbars");
    connect(m_actConfigureToolbars, &QAction::triggered, this, &MainWindow::onConfigureToolbars);



    m_actSaveLayoutNow = new QAction("Save Current Layout Now", this);
    m_actSaveLayoutNow->setToolTip("Manually save toolbar positions and window dimensions");
    connect(m_actSaveLayoutNow, &QAction::triggered, this, &MainWindow::onSaveLayoutNow);

    m_actResetLayout = new QAction("Reset Layout to Defaults", this);
    m_actResetLayout->setToolTip("Restore all toolbars and panels to their factory positions");
    connect(m_actResetLayout, &QAction::triggered, this, &MainWindow::onResetLayout);

    m_actBackupSettings = new QAction("Save Settings to Backup File...", this);
    m_actBackupSettings->setToolTip("Export all settings and layouts to a backup file");
    connect(m_actBackupSettings, &QAction::triggered, this, &MainWindow::onBackupSettings);

    m_actRestoreSettings = new QAction("Load Settings from Backup File...", this);
    m_actRestoreSettings->setToolTip("Import settings and layouts from a backup file");
    connect(m_actRestoreSettings, &QAction::triggered, this, &MainWindow::onRestoreSettings);

    m_actConfigureFolderLayouts = new QAction("Folder Profiles & Layouts...", this);
    m_actConfigureFolderLayouts->setToolTip("Create, manage, or restore UI layout and theme profiles for specific folders");
    m_actConfigureFolderLayouts->setStatusTip("Create, manage, or restore UI layout and theme profiles for specific folders");
    connect(m_actConfigureFolderLayouts, &QAction::triggered, this, &MainWindow::onConfigureFolderLayouts);

    m_actSaveFolderProfileForCurrentDir = new QAction("Save Current Layout as Folder Profile...", this);
    m_actSaveFolderProfileForCurrentDir->setToolTip("Save the current panel view mode, toolbars, and menus configuration as a profile for this folder path");
    m_actSaveFolderProfileForCurrentDir->setStatusTip("Save the current panel view mode, toolbars, and menus configuration as a profile for this folder path");
    connect(m_actSaveFolderProfileForCurrentDir, &QAction::triggered, this, &MainWindow::onSaveFolderProfileForCurrentDir);

    m_actSaveDefaultProfile = new QAction("Save Current Layout as Default Profile", this);
    m_actSaveDefaultProfile->setToolTip("Save the current layout settings (views, toolbars, sidebars) as the global Default Profile");
    m_actSaveDefaultProfile->setStatusTip("Save the current layout settings (views, toolbars, sidebars) as the global Default Profile");
    connect(m_actSaveDefaultProfile, &QAction::triggered, this, &MainWindow::onSaveDefaultProfile);

    m_actLoadDefaultProfile = new QAction("Load Default Profile", this);
    m_actLoadDefaultProfile->setToolTip("Reset the layout configuration to the global Default Profile");
    m_actLoadDefaultProfile->setStatusTip("Reset the layout configuration to the global Default Profile");
    connect(m_actLoadDefaultProfile, &QAction::triggered, this, &MainWindow::onLoadDefaultProfile);

    m_actAutoSizeColumns = new QAction("Auto-Size All Columns", this);
    m_actAutoSizeColumns->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    m_actAutoSizeColumns->setToolTip("Auto-fit all column widths to their contents (Ctrl+Shift+A)");
    m_actAutoSizeColumns->setStatusTip("Auto-fit all column widths to their contents (Ctrl+Shift+A)");
    connect(m_actAutoSizeColumns, &QAction::triggered, this, &MainWindow::onAutoSizeColumns);

    m_actToggleZenMode = new QAction("Toggle Clean Zen Interface", this);
    m_actToggleZenMode->setShortcut(QKeySequence(Qt::Key_F11));
    m_actToggleZenMode->setToolTip("Toggle distraction-free Zen Mode (F11)");
    m_actToggleZenMode->setStatusTip("Toggle distraction-free Zen Mode (F11)");
    connect(m_actToggleZenMode, &QAction::triggered, this, [this]() {
        setZenMode(!m_zenMode);
    });

    m_actConfigureBackupSchedule = new QAction("Configure Backup Schedule...", this);
    m_actConfigureBackupSchedule->setToolTip("Define automated copy and sync backup schedules for directories");
    m_actConfigureBackupSchedule->setStatusTip("Define automated copy and sync backup schedules for directories");
    connect(m_actConfigureBackupSchedule, &QAction::triggered, this, &MainWindow::onConfigureBackupSchedule);

    m_actCreateSmartCollection = new QAction("Create Smart Collection...", this);
    m_actCreateSmartCollection->setToolTip("Create dynamic folder collections based on custom search criteria");
    m_actCreateSmartCollection->setStatusTip("Create dynamic folder collections based on custom search criteria");
    connect(m_actCreateSmartCollection, &QAction::triggered, this, &MainWindow::onCreateSmartCollectionAction);

    m_actConfigureAutoTags = new QAction("Configure Auto-Tagging Rules...", this);
    m_actConfigureAutoTags->setToolTip("Define dynamic rules to automatically assign tags and colors to files");
    m_actConfigureAutoTags->setStatusTip("Define dynamic rules to automatically assign tags and colors to files");
    connect(m_actConfigureAutoTags, &QAction::triggered, this, &MainWindow::onConfigureAutoTags);

    m_actConfigureAutoOrganizer = new QAction("Configure Smart Auto-Organizer...", this);
    m_actConfigureAutoOrganizer->setToolTip("Define background rules to automatically sort files into subfolders");
    m_actConfigureAutoOrganizer->setStatusTip("Define background rules to automatically sort files into subfolders");
    connect(m_actConfigureAutoOrganizer, &QAction::triggered, this, &MainWindow::onConfigureAutoOrganizer);

    m_actRemoteMountsManager = new QAction("Remote Mounts Manager...", this);
    m_actRemoteMountsManager->setToolTip("Configure, mount, or unmount remote FTP, SFTP, Samba, and Cloud folders");
    m_actRemoteMountsManager->setStatusTip("Configure, mount, or unmount remote FTP, SFTP, Samba, and Cloud folders");
    connect(m_actRemoteMountsManager, &QAction::triggered, this, &MainWindow::onRemoteMountsManager);

    // Toggle Flat View Action
    m_actToggleFlatView = new QAction("Flat View (Recursion Mode)", this);
    m_actToggleFlatView->setCheckable(true);
    m_actToggleFlatView->setChecked(false);
    m_actToggleFlatView->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    m_actToggleFlatView->setToolTip("Toggle flat list representation recursively showing subdirectories");
    connect(m_actToggleFlatView, &QAction::toggled, this, &MainWindow::onToggleFlatView);

    // Individual panel filter toggle actions
    m_actLeftShowFilterText = new QAction("Left Panel Text Filter Bar", this);
    m_actLeftShowFilterText->setCheckable(true);
    m_actLeftShowFilterText->setChecked(true);
    connect(m_actLeftShowFilterText, &QAction::toggled, this, &MainWindow::onToggleLeftFilterText);

    m_actLeftShowCategoryButtons = new QAction("Left Panel Category Buttons", this);
    m_actLeftShowCategoryButtons->setCheckable(true);
    m_actLeftShowCategoryButtons->setChecked(true);
    connect(m_actLeftShowCategoryButtons, &QAction::toggled, this, &MainWindow::onToggleLeftCategoryButtons);

    m_actRightShowFilterText = new QAction("Right Panel Text Filter Bar", this);
    m_actRightShowFilterText->setCheckable(true);
    m_actRightShowFilterText->setChecked(true);
    connect(m_actRightShowFilterText, &QAction::toggled, this, &MainWindow::onToggleRightFilterText);

    m_actRightShowCategoryButtons = new QAction("Right Panel Category Buttons", this);
    m_actRightShowCategoryButtons->setCheckable(true);
    m_actRightShowCategoryButtons->setChecked(true);
    connect(m_actRightShowCategoryButtons, &QAction::toggled, this, &MainWindow::onToggleRightCategoryButtons);
    m_actNewTab = new QAction("New Tab", this);
    m_actNewTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_actNewTab->setToolTip("Open a new folder tab (Ctrl+T)");
    connect(m_actNewTab, &QAction::triggered, this, &MainWindow::onNewTabAction);

    m_actCloseTab = new QAction("Close Tab", this);
    m_actCloseTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actCloseTab->setToolTip("Close the current tab (Ctrl+W)");
    connect(m_actCloseTab, &QAction::triggered, this, &MainWindow::onCloseTabAction);

    m_actShowHelp = new QAction(style->standardIcon(QStyle::SP_DialogHelpButton), "User Guide & Documentation", this);
    m_actShowHelp->setShortcut(QKeySequence(Qt::Key_F1));
    m_actShowHelp->setToolTip("Open user manual (F1)");
    connect(m_actShowHelp, &QAction::triggered, this, &MainWindow::onShowHelpAction);

    m_actSpaceAnalyzer = new QAction(style->standardIcon(QStyle::SP_DriveHDIcon), "Folder Space Analyzer...", this);
    m_actSpaceAnalyzer->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    m_actSpaceAnalyzer->setToolTip("View visual directory size analysis (Ctrl+L)");
    connect(m_actSpaceAnalyzer, &QAction::triggered, this, &MainWindow::onSpaceAnalyzerAction);

    // Bind actions to window to ensure keyboard shortcuts work globally
    addAction(m_actNewFolder);
    addAction(m_actProperties);
    addAction(m_actCut);
    addAction(m_actCopy);
    addAction(m_actPaste);
    addAction(m_actDelete);
    addAction(m_actRename);
    addAction(m_actRefresh);
    addAction(m_actBulkRename);
    addAction(m_actToggleDualPane);
    addAction(m_actTogglePreview);
    addAction(m_actToggleAgeColoring);
    addAction(m_actToggleDrivesMenu);
    addAction(m_actToggleDrivesToolbar);
    addAction(m_actToggleFavoritesSidebar);
    addAction(m_actLeftShowFilterText);
    addAction(m_actLeftShowCategoryButtons);
    addAction(m_actRightShowFilterText);
    addAction(m_actRightShowCategoryButtons);
    addAction(m_actToggleConsole);
    addAction(m_actToggleFlatView);
    addAction(m_actCompareSync);
    addAction(m_actDuplicateFinder);
    addAction(m_actNewTab);
    addAction(m_actCloseTab);
    addAction(m_actShowHelp);
    addAction(m_actSpaceAnalyzer);

    m_actKeybindings = new QAction("Keyboard Shortcuts...", this);
    m_actKeybindings->setToolTip("Configure custom keyboard shortcuts");
    m_actKeybindings->setStatusTip("Configure custom keyboard shortcuts");
    connect(m_actKeybindings, &QAction::triggered, this, &MainWindow::onKeybindingsEditorAction);
    addAction(m_actKeybindings);

    m_actCalculateChecksum = new QAction("Calculate Checksum Hash...", this);
    m_actCalculateChecksum->setToolTip("Calculate MD5/SHA-1/SHA-256 hash for a file");
    m_actCalculateChecksum->setStatusTip("Calculate MD5/SHA-1/SHA-256 hash for a file");
    connect(m_actCalculateChecksum, &QAction::triggered, this, &MainWindow::onCalculateChecksum);
    addAction(m_actCalculateChecksum);

    m_actSecureShred = new QAction("Secure Shred files...", this);
    m_actSecureShred->setToolTip("Securely shred and delete files permanently");
    m_actSecureShred->setStatusTip("Securely shred and delete files permanently");
    connect(m_actSecureShred, &QAction::triggered, this, &MainWindow::onSecureShred);
    addAction(m_actSecureShred);

    m_actRemoteMount = new QAction("Mount Remote Share...", this);
    m_actRemoteMount->setToolTip("Mount SFTP/FTP/Samba directory locally");
    m_actRemoteMount->setStatusTip("Mount SFTP/FTP/Samba directory locally");
    connect(m_actRemoteMount, &QAction::triggered, this, &MainWindow::onRemoteMount);
    addAction(m_actRemoteMount);

    m_actCloudMount = new QAction("Rclone Cloud Mounts...", this);
    m_actCloudMount->setToolTip("Configure and mount Google Drive, OneDrive, or Dropbox");
    m_actCloudMount->setStatusTip("Configure and mount Google Drive, OneDrive, or Dropbox");
    connect(m_actCloudMount, &QAction::triggered, this, &MainWindow::onCloudMount);
    addAction(m_actCloudMount);

    m_actImageConvert = new QAction("Batch Image Converter...", this);
    m_actImageConvert->setToolTip("Bulk format conversion and resizing for images");
    m_actImageConvert->setStatusTip("Bulk format conversion and resizing for images");
    connect(m_actImageConvert, &QAction::triggered, this, &MainWindow::onImageConvert);
    addAction(m_actImageConvert);

    m_actProcessManager = new QAction("System Monitor...", this);
    m_actProcessManager->setToolTip("Monitor CPU/Memory and manage running processes");
    m_actProcessManager->setStatusTip("Monitor CPU/Memory and manage running processes");
    connect(m_actProcessManager, &QAction::triggered, this, &MainWindow::onProcessManagerAction);
    addAction(m_actProcessManager);

    m_actEncryptVault = new QAction("Encrypt into Vault...", this);
    m_actEncryptVault->setToolTip("Secure password-lock directories/files using AES-256");
    m_actEncryptVault->setStatusTip("Secure password-lock directories/files using AES-256");
    connect(m_actEncryptVault, &QAction::triggered, this, &MainWindow::onEncryptVault);
    addAction(m_actEncryptVault);

    m_actDecryptVault = new QAction("Decrypt Vault...", this);
    m_actDecryptVault->setToolTip("Unlock password-protected secure vaults");
    m_actDecryptVault->setStatusTip("Unlock password-protected secure vaults");
    connect(m_actDecryptVault, &QAction::triggered, this, &MainWindow::onDecryptVault);
    addAction(m_actDecryptVault);

    // Register action mapping for custom keybindings
    registerKeybindableAction("copy", m_actCopy);
    registerKeybindableAction("cut", m_actCut);
    registerKeybindableAction("paste", m_actPaste);
    registerKeybindableAction("delete", m_actDelete);
    registerKeybindableAction("rename", m_actRename);
    registerKeybindableAction("new_folder", m_actNewFolder);
    registerKeybindableAction("properties", m_actProperties);
    registerKeybindableAction("refresh", m_actRefresh);
    registerKeybindableAction("toggle_dual_pane", m_actToggleDualPane);
    registerKeybindableAction("toggle_preview", m_actTogglePreview);
    registerKeybindableAction("mute_preview", m_actMutePreview);
    registerKeybindableAction("toggle_flat_view", m_actToggleFlatView);
    registerKeybindableAction("compare_sync", m_actCompareSync);
    registerKeybindableAction("space_analyzer", m_actSpaceAnalyzer);
    registerKeybindableAction("dup_finder", m_actDuplicateFinder);
    registerKeybindableAction("help", m_actShowHelp);
    registerKeybindableAction("remote_mount", m_actRemoteMount);
    registerKeybindableAction("cloud_mount", m_actCloudMount);
    registerKeybindableAction("batch_images", m_actImageConvert);
    registerKeybindableAction("process_manager", m_actProcessManager);
    registerKeybindableAction("vault_encrypt", m_actEncryptVault);
    registerKeybindableAction("vault_decrypt", m_actDecryptVault);
    registerKeybindableAction("shred", m_actSecureShred);
    registerKeybindableAction("checksum", m_actCalculateChecksum);
    registerKeybindableAction("configure_layouts", m_actConfigureFolderLayouts);
    registerKeybindableAction("save_folder_profile", m_actSaveFolderProfileForCurrentDir);
    registerKeybindableAction("configure_age_styles", m_actConfigureAgeStyling);
    registerKeybindableAction("configure_autotags", m_actConfigureAutoTags);
}

void MainWindow::setupMenus() {
    m_menuFile = menuBar()->addMenu("File");
    m_menuFile->addAction(m_actNewTab);
    m_menuFile->addAction(m_actCloseTab);
    m_menuFile->addSeparator();
    m_menuFile->addAction(m_actNewFolder);
    m_menuFile->addAction(m_actProperties);
    m_menuFile->addSeparator();
    m_menuFile->addAction("Exit", this, &QWidget::close);

    m_menuEdit = menuBar()->addMenu("Edit");
    m_menuEdit->addAction(m_actCut);
    m_menuEdit->addAction(m_actCopy);
    m_menuEdit->addAction(m_actPaste);
    m_menuEdit->addAction(m_actCopyToSibling);
    m_menuEdit->addAction(m_actMoveToSibling);
    m_menuEdit->addAction(m_actClonePathToSibling);
    m_menuEdit->addSeparator();
    m_menuEdit->addAction(m_actDelete);
    m_menuEdit->addAction(m_actRename);
    m_menuEdit->addAction(m_actEdit);
    m_menuEdit->addAction(m_actBulkRename);
    m_menuEdit->addSeparator();

    m_menuSettings = menuBar()->addMenu("Settings");
    m_menuSettings->addAction(m_actPreferences);
    m_menuSettings->addAction(m_actMediaPreferences);
    m_menuSettings->addAction(m_actBypassFolderProfiles);
    m_menuSettings->addSeparator();

    // Settings -> Folder Profiles & Layouts
    QMenu* menuLayoutSettings = m_menuSettings->addMenu("Folder Profiles & Layouts");
    menuLayoutSettings->addAction(m_actConfigureFolderLayouts);
    menuLayoutSettings->addAction(m_actSaveFolderProfileForCurrentDir);

    QMenu* menuApplyProfileCurrent = menuLayoutSettings->addMenu("Apply Profile Layout to Current Folder");
    connect(menuApplyProfileCurrent, &QMenu::aboutToShow, this, [this, menuApplyProfileCurrent]() {
        menuApplyProfileCurrent->clear();
        for (const auto& r : m_folderRules) {
            if (!r.name.isEmpty()) {
                QString profileName = r.name;
                QAction* act = menuApplyProfileCurrent->addAction(profileName);
                connect(act, &QAction::triggered, this, [this, profileName]() {
                    onApplyProfileToCurrentFolder(profileName);
                });
            }
        }
    });

    menuLayoutSettings->addAction(m_actSaveDefaultProfile);
    menuLayoutSettings->addAction(m_actLoadDefaultProfile);
    menuLayoutSettings->addSeparator();
    menuLayoutSettings->addAction(m_actAutoSaveLayout);

    // Settings -> Themes & Styling
    QMenu* menuThemeSettings = m_menuSettings->addMenu("Themes & Styling");
    menuThemeSettings->addAction(m_actThemeStudio);
    menuThemeSettings->addSeparator();
    menuThemeSettings->addAction(m_actConfigureAgeStyling);
    menuThemeSettings->addAction(m_actToggleAgeColoring);

    // Settings -> Toolbars & Hotkeys
    QMenu* menuControlSettings = m_menuSettings->addMenu("Toolbars & Hotkeys");
    menuControlSettings->addAction(m_actConfigureToolbars);
    menuControlSettings->addAction(m_actConfigureCustomMenus);
    menuControlSettings->addAction(m_actKeybindings);

    // Settings -> Automations & Mounts
    QMenu* menuAutomations = m_menuSettings->addMenu("Automations & Mounts");
    menuAutomations->addAction(m_actConfigureBackupSchedule);
    menuAutomations->addAction(m_actConfigureAutoTags);
    menuAutomations->addAction(m_actConfigureAutoOrganizer);
    menuAutomations->addAction(m_actCreateSmartCollection);
    menuAutomations->addSeparator();
    menuAutomations->addAction(m_actRemoteMountsManager);

    // Settings -> Save/Load Config
    m_menuSettings->addSeparator();
    m_menuSettings->addAction(m_actBackupSettings);
    m_menuSettings->addAction(m_actRestoreSettings);

    m_menuView = menuBar()->addMenu("View");
    m_menuView->addAction(m_actToggleDualPane);
    m_menuView->addAction(m_actToggleHorizontalSplit);
    m_menuView->addAction(m_actTogglePreview);
    m_menuView->addAction(m_actToggleDrivesToolbar);
    m_menuView->addAction(m_actToggleCenterOps);
    m_menuView->addAction(m_actToggleFavoritesSidebar);
    m_menuView->addAction(m_actToggleConsole);
    m_menuView->addAction(m_actToggleFlatView);
    m_menuView->addAction(m_actToggleSyncScroll);
    m_menuView->addAction(m_actToggleCasingOverlays);
    
    m_menuView->addSeparator();
    
    QMenu* menuFilterToggles = m_menuView->addMenu("Filter Bars");
    menuFilterToggles->addAction(m_actLeftShowFilterText);
    menuFilterToggles->addAction(m_actLeftShowCategoryButtons);
    menuFilterToggles->addAction(m_actRightShowFilterText);
    menuFilterToggles->addAction(m_actRightShowCategoryButtons);

    m_menuView->addAction(m_actAutoSizeColumns);
    m_menuView->addAction(m_actToggleZenMode);
    m_menuView->addSeparator();
    m_menuView->addAction(m_actRefresh);

    m_menuFavorites = menuBar()->addMenu("Favorites");
    m_menuFavorites->installEventFilter(this);
    m_menuDrives = menuBar()->addMenu("Drives");

    m_menuTools = menuBar()->addMenu("Tools");
    m_menuTools->addAction(m_actCommandPalette);
    m_menuTools->addSeparator();
    m_menuTools->addAction(m_actCompareSync);
    m_menuTools->addAction(m_actDuplicateFinder);
    m_menuTools->addAction(m_actSpaceAnalyzer);
    m_menuTools->addSeparator();
    m_menuTools->addAction(m_actCalculateChecksum);
    m_menuTools->addAction(m_actSecureShred);
    m_menuTools->addAction(m_actRemoteMount);
    m_menuTools->addAction(m_actCloudMount);
    m_menuTools->addAction(m_actImageConvert);
    m_menuTools->addAction(m_actProcessManager);
    m_menuTools->addSeparator();
    m_menuTools->addAction(m_actEncryptVault);
    m_menuTools->addAction(m_actDecryptVault);


    m_menuSearch = menuBar()->addMenu("Search");
    m_menuSearch->addAction("Save Current Search as Preset...", this, &MainWindow::onSaveSearchPreset);
    m_menuSearchPresets = m_menuSearch->addMenu("Saved Presets");
    connect(m_menuSearchPresets, &QMenu::aboutToShow, this, &MainWindow::updateSearchPresetsMenu);

    m_menuHelp = menuBar()->addMenu("Help");
    m_menuHelp->addAction(m_actShowHelp);
    
    QAction* actDetailedTooltips = m_menuHelp->addAction("Enable Detailed Hover Tooltips");
    actDetailedTooltips->setCheckable(true);
    QSettings settings("Amifiles", "Amifiles");
    actDetailedTooltips->setChecked(settings.value("help/detailed_tooltips", true).toBool());
    connect(actDetailedTooltips, &QAction::toggled, this, &MainWindow::onToggleDetailedTooltips);

    m_menuHelp->addSeparator();
    m_menuHelp->addAction("About Amifiles", this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("About Amifiles");
        dlg.setFixedSize(460, 260);
        dlg.setStyleSheet(
            "QDialog { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 8px; }"
            "QLabel { color: #cdd6f4; background: transparent; }"
            "QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border: none; border-radius: 6px; padding: 8px 24px; font-size: 12px; }"
            "QPushButton:hover { background-color: #b4befe; }"
        );

        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(12);

        QLabel* titleLabel = new QLabel("<h2>Amifiles v" AMIFILES_VERSION_STRING "</h2>", &dlg);
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);

        QLabel* descLabel = new QLabel(
            "<p style='text-align: center; color: #a6adc8; font-size: 13px; margin: 0;'>A modern file manager and media showcase hub for Linux.</p>"
            "<p style='text-align: center; font-weight: bold; color: #89b4fa; font-size: 14px; margin-top: 8px;'>Created by Gemini AI & Dave. Warry</p>"
            "<p style='text-align: center; color: #6c7086; font-size: 11px; margin: 0;'>Dual-pane navigation • Real-time filtering • Theater View • Media metadata & playlists</p>", &dlg);
        descLabel->setWordWrap(true);
        descLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(descLabel);

        layout->addStretch();

        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        QPushButton* okBtn = new QPushButton("OK", &dlg);
        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        btnLayout->addWidget(okBtn);
        btnLayout->addStretch();

        layout->addLayout(btnLayout);
        dlg.exec();
    });
}

void MainWindow::setupToolbars() {
    rebuildToolBars();
}

void MainWindow::onPanelActivated(FilePanel* panel) {
    if (m_activePanel != panel) {
        m_activePanel = panel;
        if (m_terminalPanel && m_activePanel) {
            m_terminalPanel->syncDirectory(m_activePanel->currentPath());
        }
        
        FilePanel* lp = leftPanel();
        FilePanel* rp = rightPanel();
        if (lp) lp->setActive(m_activePanel == lp);
        if (rp) rp->setActive(m_activePanel == rp);

        // Sync visible filter controls to active panel's state
        if (lp && rp) {
            if (m_activePanel == lp) {
                if (!lp->isFilterTextBarVisible() && rp->isFilterTextBarVisible()) {
                    rp->syncFilterText(lp->filterText());
                }
                if (!lp->isCategoryButtonsVisible() && rp->isCategoryButtonsVisible()) {
                    rp->syncFilterType(lp->proxyModel()->filterType());
                }
            } else { // right is active
                if (!rp->isFilterTextBarVisible() && lp->isFilterTextBarVisible()) {
                    lp->syncFilterText(rp->filterText());
                }
                if (!rp->isCategoryButtonsVisible() && lp->isCategoryButtonsVisible()) {
                    lp->syncFilterType(rp->proxyModel()->filterType());
                }
            }
        }

        QString selectedFile = m_activePanel->activeFilePath();
        if (m_activePanel->selectedPaths().isEmpty()) {
            QString artPath = m_activePanel->folderArtPath();
            if (!artPath.isEmpty()) {
                onFolderArtDetected(artPath);
            } else {
                onFileSelected(""); 
            }
        } else {
            onFileSelected(selectedFile);
        }
        // Sync the flat view menu checkbox state
        if (m_actToggleFlatView) {
            m_actToggleFlatView->blockSignals(true);
            m_actToggleFlatView->setChecked(m_activePanel->isFlatViewEnabled());
            m_actToggleFlatView->blockSignals(false);
        }
        if (m_activePanel) {
            applyFolderRules(m_activePanel->currentPath());
        }
        onActivePanelViewModeChanged();
    }
}

void MainWindow::onFileSelected(const QString& filePath) {
    if (sender() != m_activePanel) return;

    if (m_activePanel) {
        int vm = m_activePanel->viewModeIndex();
        if (vm == 8 || vm == 9 || vm == 10) {
            return;
        }
    }

    // Do not interrupt active media playback (playing or paused)
    if (m_previewPanel->player() && m_previewPanel->player()->playbackState() != QMediaPlayer::StoppedState) {
        return;
    }

    if (filePath.isEmpty()) {
        m_previewPanel->clearPreview();
    } else {
        m_previewPanel->previewFile(filePath, m_activePanel->selectedPaths());
    }
    updateMiniPlayer();
}

void MainWindow::onFolderArtDetected(const QString& artPath) {
    if (sender() != m_activePanel) return;

    if (m_activePanel) {
        int vm = m_activePanel->viewModeIndex();
        if (vm == 8 || vm == 9 || vm == 10) {
            return;
        }
    }

    // Do not interrupt active media playback (playing or paused)
    if (m_previewPanel->player() && m_previewPanel->player()->playbackState() != QMediaPlayer::StoppedState) {
        return;
    }

    if (artPath.isEmpty()) {
        m_previewPanel->clearPreview();
    } else {
        m_previewPanel->previewFolderArt(artPath, m_activePanel->currentPath());
    }
    updateMiniPlayer();
}

void MainWindow::onPathChanged(const QString& path) {
    if (m_terminalPanel) {
        m_terminalPanel->syncDirectory(path);
    }
    applyFolderRules(path);
}

void MainWindow::onToggleDualPane(bool checked) {
    m_isDualPane = checked;
    if (m_rightTabWidget) m_rightTabWidget->setVisible(checked);
    if (m_tbCenterOps) {
        m_tbCenterOps->setVisible(checked && m_actToggleCenterOps->isChecked());
    }
    
    if (!checked && m_activePanel && m_rightTabWidget && m_rightTabWidget->indexOf(m_activePanel) != -1) {
        onPanelActivated(leftPanel());
    }

    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/dual_pane", checked);

    adjustSplitterSizes();
}

void MainWindow::onTogglePreview(bool checked) {
    if (m_previewDock) {
        m_previewDock->setVisible(checked);
    }
}

void MainWindow::updateMiniPlayer() {
    if (!m_previewPanel || !m_miniMediaControls) return;

    QMediaPlayer* player = m_previewPanel->player();
    QUrl source = player->source();
    QString path = source.toLocalFile();
    QFileInfo info(path);
    QString ext = info.suffix().toLower();
    static const QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a", "wma", "aac" };
    static const QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v" };

    bool isAudio = audioExts.contains(ext);
    bool isVideo = videoExts.contains(ext);
    bool isMedia = isAudio || isVideo;

    if (!m_showPreview && isMedia) {
        m_miniMediaControls->updateTrackInfo(info.fileName(), player->duration(), isVideo);
        m_miniMediaControls->show();
        m_miniMediaControls->raise();

        // Dynamically position the overlay in the bottom-right corner
        int statusHeight = (statusBar() && statusBar()->isVisible()) ? statusBar()->height() : 0;
        int x = width() - m_miniMediaControls->width() - 20;
        int y = height() - m_miniMediaControls->height() - statusHeight - 20;
        m_miniMediaControls->move(x, y);
    } else {
        m_miniMediaControls->hide();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (m_miniMediaControls && m_miniMediaControls->isVisible()) {
        int statusHeight = (statusBar() && statusBar()->isVisible()) ? statusBar()->height() : 0;
        int x = width() - m_miniMediaControls->width() - 20;
        int y = height() - m_miniMediaControls->height() - statusHeight - 20;
        m_miniMediaControls->move(x, y);
    }

    // Adaptive Responsive Panels Collapsing
    int w = width();
    if (w < 850) {
        // Collapse preview dock if visible
        if (m_previewDock && m_previewDock->isVisible()) {
            m_wasPreviewPriorToCollapse = true;
            m_actTogglePreview->setChecked(false);
            onTogglePreview(false);
        }
        // Collapse dual pane if active
        if (m_isDualPane) {
            m_wasDualPanePriorToCollapse = true;
            m_actToggleDualPane->setChecked(false);
            onToggleDualPane(false);
        }
    } else if (w >= 1000) {
        // Restore preview dock if it was collapsed automatically
        if (m_wasPreviewPriorToCollapse && m_previewDock && !m_previewDock->isVisible()) {
            m_wasPreviewPriorToCollapse = false;
            m_actTogglePreview->setChecked(true);
            onTogglePreview(true);
        }
        // Restore dual pane if it was collapsed automatically
        if (m_wasDualPanePriorToCollapse && !m_isDualPane) {
            m_wasDualPanePriorToCollapse = false;
            m_actToggleDualPane->setChecked(true);
            onToggleDualPane(true);
        }
    }
}

void MainWindow::onToggleAgeColoring(bool checked) {
    m_ageColoringEnabled = checked;
    for (int i = 0; i < m_leftTabWidget->count(); ++i) {
        FilePanel* panel = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
        if (panel) {
            panel->proxyModel()->setAgeColoringEnabled(checked);
            panel->refresh();
        }
    }
    for (int i = 0; i < m_rightTabWidget->count(); ++i) {
        FilePanel* panel = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
        if (panel) {
            panel->proxyModel()->setAgeColoringEnabled(checked);
            panel->refresh();
        }
    }
}

void MainWindow::onConfigureAgeStyling() {
    AgeStylingDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        for (int i = 0; i < m_leftTabWidget->count(); ++i) {
            FilePanel* panel = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
            if (panel) {
                panel->proxyModel()->loadAgeRules();
                panel->refresh();
            }
        }
        for (int i = 0; i < m_rightTabWidget->count(); ++i) {
            FilePanel* panel = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
            if (panel) {
                panel->proxyModel()->loadAgeRules();
                panel->refresh();
            }
        }
    }
}

// Router Slots
void MainWindow::onCopyAction() {
    if (m_activePanel) m_activePanel->onCopy();
}

void MainWindow::onCutAction() {
    if (m_activePanel) m_activePanel->onCut();
}

void MainWindow::onPasteAction() {
    if (m_activePanel) m_activePanel->onPaste();
}

void MainWindow::onDeleteAction() {
    if (m_activePanel) m_activePanel->onDelete();
}

void MainWindow::onRenameAction() {
    if (m_activePanel) m_activePanel->onRename();
}

void MainWindow::onEditAction() {
    if (!m_activePanel) return;
    QString filePath = m_activePanel->activeFilePath();
    if (filePath.isEmpty() || QFileInfo(filePath).isDir()) return;

    TextEditorDialog dlg(filePath, this);
    dlg.exec();
    m_activePanel->refresh();
}

void MainWindow::onNewFolderAction() {
    if (m_activePanel) m_activePanel->onNewFolder();
}

void MainWindow::onShowPropertiesAction() {
    if (m_activePanel) m_activePanel->onShowProperties();
}

void MainWindow::onRefreshAction() {
    if (m_activePanel) m_activePanel->refresh();
}

void MainWindow::onBulkRenameAction() {
    if (!m_activePanel) return;
    QStringList selected = m_activePanel->selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "Bulk Rename", "Please select one or more files in the active display pane first.");
        return;
    }
    
    BulkRenameDialog dlg(selected, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (leftPanel()) leftPanel()->refresh();
        if (rightPanel()) rightPanel()->refresh();
    }
}

void MainWindow::onCopyToSiblingAction() {
    if (!m_activePanel) return;
    FilePanel* destPanel = (m_activePanel == leftPanel()) ? rightPanel() : leftPanel();
    if (!destPanel) return;

    QStringList paths = m_activePanel->selectedPaths();
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Copy to Sibling", "Please select one or more items in the active display pane first.");
        return;
    }

    QString destDir = destPanel->currentPath();
    if (destDir.isEmpty()) return;

    auto button = QMessageBox::question(this, "Confirm Copy",
                                         QString("Copy the %1 selected item(s) to %2?")
                                         .arg(paths.size()).arg(destDir),
                                         QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
        if (destPanel->isArchiveViewActive()) {
            QMessageBox::warning(this, "Archive Write Unsupported", "Directly copying to a closed archive VFS from opposite pane is not supported. Please paste inside the active VFS instead.");
            return;
        }
        
        CopyQueueManager::instance().queueCopy(paths, destDir, false, this);
    }
}

void MainWindow::onMoveToSiblingAction() {
    if (!m_activePanel) return;
    FilePanel* destPanel = (m_activePanel == leftPanel()) ? rightPanel() : leftPanel();
    if (!destPanel) return;

    QStringList paths = m_activePanel->selectedPaths();
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Move to Sibling", "Please select one or more items in the active display pane first.");
        return;
    }

    QString destDir = destPanel->currentPath();
    if (destDir.isEmpty()) return;

    auto button = QMessageBox::question(this, "Confirm Move",
                                         QString("Move the %1 selected item(s) to %2?")
                                         .arg(paths.size()).arg(destDir),
                                         QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
        if (destPanel->isArchiveViewActive()) {
            QMessageBox::warning(this, "Archive Write Unsupported", "Directly moving to a closed archive VFS from opposite pane is not supported. Please paste inside the active VFS instead.");
            return;
        }
        
        CopyQueueManager::instance().queueCopy(paths, destDir, true, this);
    }
}

void MainWindow::onFavoriteTriggered() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (act && m_activePanel) {
        QString path = act->data().toString();
        m_activePanel->setPath(path);
    }
}

void MainWindow::updateFavoritesMenu() {
    m_menuFavorites->clear();
    
    QAction* actAdd = m_menuFavorites->addAction("Add Current Directory to Favorites");
    connect(actAdd, &QAction::triggered, this, [this]() {
        if (m_activePanel) {
            FavoritesManager::instance().addFavorite(m_activePanel->currentPath());
        }
    });

    m_menuFavorites->addAction("Configure Dynamic Bookmarks...", this, &MainWindow::onConfigureDynamicBookmarks);

    QStringList favs = FavoritesManager::instance().getFavorites();

    m_menuFavorites->addSeparator();

    if (favs.isEmpty()) {
        QAction* actNone = m_menuFavorites->addAction("(No Favorites Configured)");
        actNone->setEnabled(false);
    } else {
        for (const QString& path : favs) {
            QAction* actFav = m_menuFavorites->addAction(QDir::toNativeSeparators(path));
            actFav->setData(path);
            connect(actFav, &QAction::triggered, this, &MainWindow::onFavoriteTriggered);
        }
    }

    m_menuFavorites->addSeparator();
    QAction* actHeader = m_menuFavorites->addAction("⭐ Dynamic Bookmarks:");
    actHeader->setEnabled(false);
    
    QStringList dynamicPaths = FavoritesManager::instance().getEvaluatedDynamicPaths();
    if (dynamicPaths.isEmpty()) {
        QAction* actNone = m_menuFavorites->addAction("(No Dynamic Matches)");
        actNone->setEnabled(false);
    } else {
        for (const QString& path : dynamicPaths) {
            QAction* actFav = m_menuFavorites->addAction(QDir::toNativeSeparators(path));
            actFav->setData(path);
            connect(actFav, &QAction::triggered, this, &MainWindow::onFavoriteTriggered);
        }
    }

    m_menuFavorites->addSeparator();
    m_menuFavorites->addAction("Folder Profiles & Layouts...", this, [this]() {
        FolderLayoutDialog dlg(m_folderRules, m_customButtons, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_folderRules = dlg.rules();
            saveFolderRules();
            if (m_activePanel) {
                applyFolderRules(m_activePanel->currentPath());
            }
        }
    });

    if (!m_folderRules.isEmpty()) {
        QMenu* menuProfiles = m_menuFavorites->addMenu("Load Layout Profile...");
        for (const FolderLayoutRule& r : m_folderRules) {
            QAction* actLoad = menuProfiles->addAction(r.name);
            connect(actLoad, &QAction::triggered, this, [this, r]() {
                applyProfile(r);
            });
        }
    }
}

void MainWindow::updateDrivesList() {
    if (!m_menuDrives || !m_tbDrives) return;

    m_menuDrives->clear();
    m_tbDrives->clear();

    QStyle* style = QApplication::style();

    // 1. Add static "Refresh Drives" action
    QAction* actRefresh = new QAction(style->standardIcon(QStyle::SP_BrowserReload), "Refresh Drives", this);
    connect(actRefresh, &QAction::triggered, this, &MainWindow::updateDrivesList);
    m_menuDrives->addAction(actRefresh);
    m_tbDrives->addAction(actRefresh);

    m_menuDrives->addSeparator();
    m_tbDrives->addSeparator();

    // 2. Fetch mounted volumes
    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    
    // Sort volumes by root path to be neat
    std::sort(volumes.begin(), volumes.end(), [](const QStorageInfo& a, const QStorageInfo& b) {
        return a.rootPath() < b.rootPath();
    });

    // Helper lambda to add a shortcut option
    auto addShortcutOption = [this, style](const QString& name, const QString& path, const QString& themeIconName, QStyle::StandardPixmap fallbackPixmap) {
        if (path.isEmpty()) return;
        QIcon icon = QIcon::fromTheme(themeIconName);
        if (icon.isNull()) {
            icon = style->standardIcon(fallbackPixmap);
        }

        // Menu action
        QAction* actMenu = m_menuDrives->addAction(icon, QString("%1 (%2)").arg(name).arg(QDir::toNativeSeparators(path)));
        connect(actMenu, &QAction::triggered, this, [this, path]() {
            if (m_activePanel) m_activePanel->setPath(path);
        });

        // Toolbar action
        QAction* actTb = m_tbDrives->addAction(icon, name);
        actTb->setToolTip(QString("Navigate to %1").arg(QDir::toNativeSeparators(path)));
        connect(actTb, &QAction::triggered, this, [this, path]() {
            if (m_activePanel) m_activePanel->setPath(path);
        });
    };

    // Helper lambda to add a drive option
    auto addDriveOption = [this, style](const QString& name, const QString& path, QStyle::StandardPixmap iconPixmap) {
        QIcon icon = style->standardIcon(iconPixmap);

        // Menu action
        QAction* actMenu = m_menuDrives->addAction(icon, QString("%1 (%2)").arg(name).arg(QDir::toNativeSeparators(path)));
        connect(actMenu, &QAction::triggered, this, [this, path]() {
            if (m_activePanel) m_activePanel->setPath(path);
        });

        // Toolbar action
        QAction* actTb = m_tbDrives->addAction(icon, name);
        actTb->setToolTip(QString("Navigate to %1").arg(QDir::toNativeSeparators(path)));
        connect(actTb, &QAction::triggered, this, [this, path]() {
            if (m_activePanel) m_activePanel->setPath(path);
        });
    };

    // Helper to get standard paths safely with home subdirectory fallback
    auto getSafeShortcutPath = [](QStandardPaths::StandardLocation location, const QString& fallbackFolderName) {
        QString path = QStandardPaths::writableLocation(location);
        if (path.isEmpty() || path == QDir::homePath()) {
            QString testPath = QDir(QDir::homePath()).filePath(fallbackFolderName);
            if (QDir(testPath).exists()) {
                return testPath;
            }
        }
        return path;
    };

    // Add common shortcuts
    addShortcutOption("Home", QDir::homePath(), "user-home", QStyle::SP_DirHomeIcon);
    addShortcutOption("Desktop", getSafeShortcutPath(QStandardPaths::DesktopLocation, "Desktop"), "user-desktop", QStyle::SP_DirIcon);
    addShortcutOption("Documents", getSafeShortcutPath(QStandardPaths::DocumentsLocation, "Documents"), "folder-documents", QStyle::SP_DirIcon);
    addShortcutOption("Downloads", getSafeShortcutPath(QStandardPaths::DownloadLocation, "Downloads"), "folder-download", QStyle::SP_DirIcon);
    addShortcutOption("Pictures", getSafeShortcutPath(QStandardPaths::PicturesLocation, "Pictures"), "folder-pictures", QStyle::SP_DirIcon);
    addShortcutOption("Music", getSafeShortcutPath(QStandardPaths::MusicLocation, "Music"), "folder-music", QStyle::SP_DirIcon);
    addShortcutOption("Videos", getSafeShortcutPath(QStandardPaths::MoviesLocation, "Videos"), "folder-videos", QStyle::SP_DirIcon);

    m_menuDrives->addSeparator();
    m_tbDrives->addSeparator();

    // List of added paths to avoid duplicates
    QStringList addedPaths;
    addedPaths.append(QDir::homePath());
    auto addPathSafe = [&addedPaths](const QString& path) {
        if (!path.isEmpty()) addedPaths.append(path);
    };
    addPathSafe(getSafeShortcutPath(QStandardPaths::DesktopLocation, "Desktop"));
    addPathSafe(getSafeShortcutPath(QStandardPaths::DocumentsLocation, "Documents"));
    addPathSafe(getSafeShortcutPath(QStandardPaths::DownloadLocation, "Downloads"));
    addPathSafe(getSafeShortcutPath(QStandardPaths::PicturesLocation, "Pictures"));
    addPathSafe(getSafeShortcutPath(QStandardPaths::MusicLocation, "Music"));
    addPathSafe(getSafeShortcutPath(QStandardPaths::MoviesLocation, "Videos"));

    for (const QStorageInfo& volume : volumes) {
        if (!volume.isValid() || !volume.isReady()) continue;

        QString path = volume.rootPath();
        
        // Filter out duplicates or internal system mounts to keep the list clean
        if (addedPaths.contains(path)) continue;

        bool isPhysicalDrive = path == "/" ||
                               path.startsWith("/media/") ||
                               path.startsWith("/mnt/") ||
                               path.startsWith("/run/media/");

        if (!isPhysicalDrive) continue;

        QString name = volume.displayName();
        if (name.isEmpty()) {
            name = volume.name();
        }
        if (name.isEmpty() || name == "/") {
            name = (path == "/") ? "Root (/) " : QFileInfo(path).fileName();
        }

        addDriveOption(name, path, QStyle::SP_DriveHDIcon);
        addedPaths.append(path);
    }

    // 3. Add remote mounts from settings
    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("RemoteMounts");
    QStringList keys = settings.allKeys();
    if (!keys.isEmpty()) {
        m_menuDrives->addSeparator();
        m_tbDrives->addSeparator();
        for (const QString& label : keys) {
            QString mountedPath = settings.value(label).toString();
            if (QDir(mountedPath).exists()) {
                addShortcutOption(label, mountedPath, "folder-remote", QStyle::SP_DirLinkIcon);
            }
        }
    }
    settings.endGroup();

    // 4. Add drag-and-drop user shortcuts from settings
    settings.beginGroup("DrivesShortcuts");
    QStringList shortcutKeys = settings.allKeys();
    if (!shortcutKeys.isEmpty()) {
        m_menuDrives->addSeparator();
        m_tbDrives->addSeparator();
        for (const QString& label : shortcutKeys) {
            QString path = settings.value(label).toString();
            if (QDir(path).exists()) {
                QIcon icon = getFolderIcon(label);

                // Menu action
                QAction* actMenu = m_menuDrives->addAction(icon, QString("%1 (%2)").arg(label).arg(QDir::toNativeSeparators(path)));
                connect(actMenu, &QAction::triggered, this, [this, path]() {
                    if (m_activePanel) m_activePanel->setPath(path);
                });

                // Toolbar action
                QAction* actTb = m_tbDrives->addAction(icon, label);
                actTb->setToolTip(QString("Navigate to %1").arg(QDir::toNativeSeparators(path)));
                actTb->setData(label); // Store label for deletion
                connect(actTb, &QAction::triggered, this, [this, path]() {
                    if (m_activePanel) m_activePanel->setPath(path);
                });
            }
        }
    }
    settings.endGroup();
}

void MainWindow::onToggleDrivesMenu(bool checked) {
    if (m_menuDrives) {
        m_menuDrives->menuAction()->setVisible(checked);
    }
    // Save to settings
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("drives/menu_visible", checked);
}

void MainWindow::onToggleDrivesToolbar(bool checked) {
    if (m_tbDrives) {
        m_tbDrives->setVisible(checked);
    }
    // Save to settings
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("drives/toolbar_visible", checked);
}

void MainWindow::onMutePreview(bool checked) {
    if (m_previewPanel) {
        m_previewPanel->setMuted(checked);
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preview/muted", checked);
}

void MainWindow::onToggleArchiveNav(bool checked) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/archive_nav", checked);
    for (int i = 0; i < m_leftTabWidget->count(); ++i) {
        FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
        if (p) p->refresh();
    }
    for (int i = 0; i < m_rightTabWidget->count(); ++i) {
        FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
        if (p) p->refresh();
    }
}

void MainWindow::onToggleArchiveWrite(bool checked) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/archive_write", checked);
    for (int i = 0; i < m_leftTabWidget->count(); ++i) {
        FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
        if (p) p->refresh();
    }
    for (int i = 0; i < m_rightTabWidget->count(); ++i) {
        FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
        if (p) p->refresh();
    }
}

void MainWindow::onToggleHorizontalSplit(bool checked) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/horizontal_split", checked);
    if (m_dualSplitter) {
        if (checked) {
            m_dualSplitter->setOrientation(Qt::Vertical);
            if (m_tbCenterOps) {
                m_tbCenterOps->setFixedWidth(QWIDGETSIZE_MAX);
                m_tbCenterOps->setFixedHeight(38);
                if (m_tbCenterOpsSeparator) {
                    m_tbCenterOpsSeparator->setFrameShape(QFrame::VLine);
                    m_tbCenterOpsSeparator->setStyleSheet("background-color: #45475a; max-width: 1px; max-height: 30px;");
                }
                if (QBoxLayout* l = qobject_cast<QBoxLayout*>(m_tbCenterOps->layout())) {
                    l->setDirection(QBoxLayout::LeftToRight);
                    l->setContentsMargins(8, 4, 8, 4);
                }
            }
            m_dualSplitter->insertWidget(0, m_leftTabWidget);
            m_dualSplitter->insertWidget(1, m_tbCenterOps);
            m_dualSplitter->insertWidget(2, m_rightTabWidget);
            if (m_tbCenterOps) {
                m_tbCenterOps->setVisible(m_actToggleCenterOps->isChecked() && m_isDualPane);
            }
        } else {
            m_dualSplitter->setOrientation(Qt::Horizontal);
            if (m_tbCenterOps) {
                m_tbCenterOps->setFixedWidth(38);
                m_tbCenterOps->setFixedHeight(QWIDGETSIZE_MAX);
                if (m_tbCenterOpsSeparator) {
                    m_tbCenterOpsSeparator->setFrameShape(QFrame::HLine);
                    m_tbCenterOpsSeparator->setStyleSheet("background-color: #45475a; max-height: 1px; max-width: 30px;");
                }
                if (QBoxLayout* l = qobject_cast<QBoxLayout*>(m_tbCenterOps->layout())) {
                    l->setDirection(QBoxLayout::TopToBottom);
                    l->setContentsMargins(4, 8, 4, 8);
                }
            }
            m_dualSplitter->insertWidget(0, m_leftTabWidget);
            m_dualSplitter->insertWidget(1, m_tbCenterOps);
            m_dualSplitter->insertWidget(2, m_rightTabWidget);
            if (m_tbCenterOps) {
                m_tbCenterOps->setVisible(m_actToggleCenterOps->isChecked() && m_isDualPane);
            }
        }
    }
}

void MainWindow::onToggleCasingOverlays(bool checked) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/casing_overlays", checked);
    for (int i = 0; i < m_leftTabWidget->count(); ++i) {
        FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
        if (p) p->refresh();
    }
    for (int i = 0; i < m_rightTabWidget->count(); ++i) {
        FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
        if (p) p->refresh();
    }
}

void MainWindow::onToggleAudioCoverArt(bool checked) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preview/show_audio_cover_art", checked);
    if (m_previewPanel) {
        m_previewPanel->setAudioCoverArtVisible(checked);
    }
}

void MainWindow::onToggleSpectrum(bool checked) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preview/show_spectrum_visualizer", checked);
    if (m_actToggleSpectrum && m_actToggleSpectrum->isChecked() != checked) {
        m_actToggleSpectrum->setChecked(checked);
    }
    if (m_previewPanel) {
        m_previewPanel->setSpectrumVisualizerVisible(checked);
    }
}

void MainWindow::onToggleLeftFilterText(bool checked) {
    for (int i = 0; i < m_leftTabWidget->count(); ++i) {
        FilePanel* panel = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
        if (panel) {
            panel->setFilterTextBarVisible(checked);
        }
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("left_panel/filter_text_visible", checked);
}

void MainWindow::onToggleLeftCategoryButtons(bool checked) {
    for (int i = 0; i < m_leftTabWidget->count(); ++i) {
        FilePanel* panel = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
        if (panel) {
            panel->setCategoryButtonsVisible(checked);
        }
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("left_panel/category_buttons_visible", checked);
}

void MainWindow::onToggleRightFilterText(bool checked) {
    for (int i = 0; i < m_rightTabWidget->count(); ++i) {
        FilePanel* panel = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
        if (panel) {
            panel->setFilterTextBarVisible(checked);
        }
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("right_panel/filter_text_visible", checked);
}

void MainWindow::onToggleRightCategoryButtons(bool checked) {
    for (int i = 0; i < m_rightTabWidget->count(); ++i) {
        FilePanel* panel = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
        if (panel) {
            panel->setCategoryButtonsVisible(checked);
        }
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("right_panel/category_buttons_visible", checked);
}

void MainWindow::onToggleConsole(bool checked) {
    if (m_bottomTabWidget) {
        m_bottomTabWidget->setVisible(checked);
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("console/visible", checked);
}

void MainWindow::onToggleFlatView(bool checked) {
    if (m_activePanel) {
        m_activePanel->setFlatViewEnabled(checked);
    }
}

#include "folderdiffdialog.h"

void MainWindow::onCompareSyncAction() {
    FilePanel* lp = leftPanel();
    FilePanel* rp = rightPanel();
    if (!lp || !rp) return;

    QString leftPath = lp->currentPath();
    QString rightPath = rp->currentPath();

    FolderDiffDialog dlg(leftPath, rightPath, this);
    dlg.exec();

    lp->refresh();
    rp->refresh();
}

void MainWindow::onDuplicateFinderAction() {
    if (!m_activePanel) return;

    QString scanPath = m_activePanel->currentPath();
    DuplicateFinderDialog dlg(scanPath, this);
    dlg.exec();

    m_activePanel->refresh();
}

// ================= Custom Script Buttons Implementation =================

void MainWindow::loadCustomButtons() {
    QSettings settings("Amifiles", "Amifiles");
    int count = settings.value("custom_buttons/count", -1).toInt();
    m_customButtons.clear();

    if (count == -1) {
        // First run: setup sample commands with standard Qt icons to demonstrate features
        m_customButtons.append(CustomButton{"Say Hello", "zenity --info --text=\"Hello from Amifiles!\\nActive Folder: $AMIFILES_CURRENT_DIR\"", "qt:SP_MessageBoxInformation"});
        m_customButtons.append(CustomButton{"Disk Space", "df -h | zenity --text-info --title=\"Disk Usage Summary\" --width=600 --height=400", "qt:SP_DriveHDIcon"});
        m_customButtons.append(CustomButton{"Count Files", "find . -maxdepth 1 -type f | wc -l | xargs -I {} zenity --info --text=\"There are {} files in the current folder.\"", "qt:SP_FileDialogContentsView"});
        saveCustomButtons();
    } else {
        for (int i = 0; i < count; ++i) {
            QString name = settings.value(QString("custom_buttons/btn_%1/name").arg(i)).toString();
            QString script = settings.value(QString("custom_buttons/btn_%1/script").arg(i)).toString();
            QString icon = settings.value(QString("custom_buttons/btn_%1/icon").arg(i)).toString();
            m_customButtons.append(CustomButton{name, script, icon});
        }
    }
}

void MainWindow::saveCustomButtons() {
    QSettings settings("Amifiles", "Amifiles");
    settings.remove("custom_buttons");
    settings.setValue("custom_buttons/count", m_customButtons.size());
    for (int i = 0; i < m_customButtons.size(); ++i) {
        settings.setValue(QString("custom_buttons/btn_%1/name").arg(i), m_customButtons[i].name);
        settings.setValue(QString("custom_buttons/btn_%1/script").arg(i), m_customButtons[i].script);
        settings.setValue(QString("custom_buttons/btn_%1/icon").arg(i), m_customButtons[i].icon);
    }
}

void MainWindow::rebuildCustomToolBar() {
    if (!m_customToolBar) return;
    m_customToolBar->clear();

    // Standard static action to add custom script buttons
    QAction* actAdd = m_customToolBar->addAction("➕ Add Button");
    actAdd->setToolTip("Create a new scriptable command button");
    connect(actAdd, &QAction::triggered, this, &MainWindow::onAddCustomButton);
    m_customToolBar->addSeparator();

    // Populate buttons loaded from settings
    for (int i = 0; i < m_customButtons.size(); ++i) {
        if (!m_activeToolbarFilter.isEmpty() && !m_activeToolbarFilter.contains(m_customButtons[i].name)) {
            continue;
        }
        QIcon qicon;
        QString iconPath = m_customButtons[i].icon;
        if (!iconPath.isEmpty()) {
            if (iconPath.startsWith("theme:")) {
                qicon = QIcon::fromTheme(iconPath.mid(6));
            } else if (iconPath.startsWith("qt:")) {
                QString qtKey = iconPath.mid(3);
                QStyle::StandardPixmap pixmap = QStyle::SP_CustomBase;
                if (qtKey == "SP_MessageBoxInformation") pixmap = QStyle::SP_MessageBoxInformation;
                else if (qtKey == "SP_DriveHDIcon") pixmap = QStyle::SP_DriveHDIcon;
                else if (qtKey == "SP_FileDialogContentsView") pixmap = QStyle::SP_FileDialogContentsView;
                
                if (pixmap != QStyle::SP_CustomBase) {
                    qicon = style()->standardIcon(pixmap);
                }
            } else {
                qicon = QIcon(iconPath);
            }
        }

        QAction* act = m_customToolBar->addAction(qicon, m_customButtons[i].name);
        act->setProperty("script", m_customButtons[i].script);
        act->setProperty("icon", m_customButtons[i].icon);
        act->setProperty("index", i);
        act->setProperty("isCustom", true);
        connect(act, &QAction::triggered, this, &MainWindow::onCustomButtonClicked);
    }
}

void MainWindow::onAddCustomButton() {
    CustomButtonDialog dlg("", "", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString name = dlg.buttonName();
        QString script = dlg.script();
        QString icon = dlg.iconPath();
        if (name.isEmpty() || script.isEmpty()) return;

        m_customButtons.append(CustomButton{name, script, icon});
        saveCustomButtons();
        rebuildCustomToolBar();
        statusBar()->showMessage(QString("Custom button '%1' added.").arg(name), 3000);
    }
}

void MainWindow::onCustomButtonClicked() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act || !m_activePanel) return;

    QString script = act->property("script").toString().trimmed();
    if (script.isEmpty()) return;

    if (script.startsWith("@internal:")) {
        QString cmd = script.mid(10).trimmed();
        if (cmd == "Copy") {
            onCopyToSiblingAction();
        } else if (cmd == "Move") {
            onMoveToSiblingAction();
        } else if (cmd == "Cut") {
            onCutAction();
        } else if (cmd == "Paste") {
            onPasteAction();
        } else if (cmd == "Delete") {
            onDeleteAction();
        } else if (cmd == "Rename") {
            onRenameAction();
        } else if (cmd == "NewFolder") {
            onNewFolderAction();
        } else if (cmd == "Refresh") {
            onRefreshAction();
        } else if (cmd == "ToggleDualPane") {
            m_actToggleDualPane->trigger();
        } else if (cmd == "TogglePreview") {
            m_actTogglePreview->trigger();
        } else if (cmd == "ToggleFlatView") {
            m_actToggleFlatView->trigger();
        } else if (cmd == "CompareSync") {
            onCompareSyncAction();
        } else if (cmd == "DuplicateFinder") {
            onDuplicateFinderAction();
        } else if (cmd == "SpaceAnalyzer") {
            onSpaceAnalyzerAction();
        } else if (cmd.startsWith("Go ") || cmd == "Go") {
            QString path = cmd.mid(3).trimmed();
            QString activeDir = m_activePanel ? m_activePanel->currentPath() : "";
            FilePanel* destPanel = (m_activePanel == leftPanel()) ? rightPanel() : leftPanel();
            QString destDir = destPanel ? destPanel->currentPath() : "";
            path.replace("{dest}", destDir);
            path.replace("{dir}", activeDir);
            m_activePanel->setPath(path);
        } else {
            statusBar()->showMessage(QString("Unknown internal command: %1").arg(cmd), 4000);
        }
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_activePanel->currentPath());

    // Inject active directories and file selection to script env
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("AMIFILES_CURRENT_DIR", m_activePanel->currentPath());
    
    QStringList selected = m_activePanel->selectedPaths();
    if (!selected.isEmpty()) {
        env.insert("AMIFILES_SELECTED_FIRST", selected.first());
        env.insert("AMIFILES_SELECTED", selected.join("\n"));
    }
    process->setProcessEnvironment(env);

    statusBar()->showMessage(QString("Executing '%1'...").arg(act->text()), 3000);

    if (m_consolePanel) {
        m_consolePanel->appendSystem(QString("=== Running Command: %1 ===").arg(act->text()));
        m_consolePanel->appendSystem(QString("Working Directory: %1").arg(m_activePanel->currentPath()));
    }

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (m_consolePanel) {
            m_consolePanel->appendOutput(QString::fromUtf8(process->readAllStandardOutput()));
        }
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        if (m_consolePanel) {
            m_consolePanel->appendError(QString::fromUtf8(process->readAllStandardError()));
        }
    });

    QString commandStr = script;
    QString activeDir = m_activePanel ? m_activePanel->currentPath() : "";
    FilePanel* destPanel = (m_activePanel == leftPanel()) ? rightPanel() : leftPanel();
    QString destDir = destPanel ? destPanel->currentPath() : "";
    QString firstSelected = selected.isEmpty() ? "" : selected.first();

    commandStr.replace("{filepath}", firstSelected);
    commandStr.replace("{dir}", activeDir);
    commandStr.replace("{dest}", destDir);

    // Execute in bash shell environment
    process->start("bash", {"-c", commandStr});
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process, act](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit) {
            statusBar()->showMessage(QString("Command '%1' crashed!").arg(act->text()), 4000);
            if (m_consolePanel) m_consolePanel->appendError("Command crashed!");
        } else if (exitCode != 0) {
            statusBar()->showMessage(QString("Command '%1' returned exit code %2").arg(act->text()).arg(exitCode), 4000);
            if (m_consolePanel) m_consolePanel->appendError(QString("Command finished with exit code: %1").arg(exitCode));
        } else {
            statusBar()->showMessage(QString("Command '%1' completed successfully.").arg(act->text()), 3000);
            if (m_consolePanel) m_consolePanel->appendSystem("Command finished successfully.");
            if (leftPanel()) leftPanel()->refresh();
            if (rightPanel()) rightPanel()->refresh();
        }
        process->deleteLater();
    });
}

void MainWindow::onCustomToolBarContextMenu(const QPoint& pos) {
    if (!m_customToolBar) return;
    QAction* action = m_customToolBar->actionAt(pos);
    QMenu menu(this);

    if (action && action->property("isCustom").toBool()) {
        int idx = action->property("index").toInt();
        
        QAction* actEdit = menu.addAction("Edit Button...");
        QAction* actDelete = menu.addAction("Delete Button");

        QAction* selected = menu.exec(m_customToolBar->mapToGlobal(pos));
        if (!selected) return;

        if (selected == actEdit) {
            CustomButtonDialog dlg(m_customButtons[idx].name, m_customButtons[idx].script, m_customButtons[idx].icon, this);
            if (dlg.exec() == QDialog::Accepted) {
                QString name = dlg.buttonName();
                QString script = dlg.script();
                QString icon = dlg.iconPath();
                if (!name.isEmpty() && !script.isEmpty()) {
                    m_customButtons[idx] = {name, script, icon};
                    saveCustomButtons();
                    rebuildCustomToolBar();
                    statusBar()->showMessage("Custom button updated.", 3000);
                }
            }
        } else if (selected == actDelete) {
            auto answer = QMessageBox::question(this, "Delete Button", 
                                                QString("Are you sure you want to delete the button '%1'?")
                                                .arg(m_customButtons[idx].name),
                                                QMessageBox::Yes | QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                m_customButtons.removeAt(idx);
                saveCustomButtons();
                rebuildCustomToolBar();
                statusBar()->showMessage("Custom button deleted.", 3000);
            }
        }
    } else {
        QAction* actAdd = menu.addAction("Add Custom Button...");
        menu.addSeparator();
        QAction* actImport = menu.addAction("Import Custom Buttons...");
        QAction* actExport = menu.addAction("Export Custom Buttons...");

        QAction* selected = menu.exec(m_customToolBar->mapToGlobal(pos));
        if (!selected) return;

        if (selected == actAdd) {
            onAddCustomButton();
        } else if (selected == actImport) {
            onImportCustomButtons();
        } else if (selected == actExport) {
            onExportCustomButtons();
        }
    }
}

void MainWindow::onSaveSearchPreset() {
    if (!m_activePanel) return;
    
    QString query = m_activePanel->searchQuery();
    if (query.isEmpty()) {
        QMessageBox::information(this, "Save Search Preset", "Please enter a search query in the search bar first.");
        return;
    }
    
    bool ok;
    QString presetName = QInputDialog::getText(this, "Save Search Preset", 
                                               "Enter a name for this search preset:", 
                                               QLineEdit::Normal, "", &ok);
    if (!ok || presetName.trimmed().isEmpty()) {
        return;
    }
    
    QSettings settings("Amifiles", "Amifiles");
    QVariantList presets = settings.value("search/presets").toList();
    
    QVariantMap newPreset;
    newPreset["name"] = presetName.trimmed();
    newPreset["query"] = query;
    presets.append(newPreset);
    
    settings.setValue("search/presets", presets);
    statusBar()->showMessage(QString("Search preset '%1' saved.").arg(presetName), 3000);
}

void MainWindow::updateSearchPresetsMenu() {
    m_menuSearchPresets->clear();
    
    QSettings settings("Amifiles", "Amifiles");
    QVariantList presets = settings.value("search/presets").toList();
    
    if (presets.isEmpty()) {
        QAction* emptyAct = m_menuSearchPresets->addAction("No presets saved");
        emptyAct->setEnabled(false);
        return;
    }
    
    for (const QVariant& pVar : presets) {
        QVariantMap pMap = pVar.toMap();
        QString name = pMap["name"].toString();
        QString query = pMap["query"].toString();
        
        QAction* act = m_menuSearchPresets->addAction(QString("%1 (%2)").arg(name, query));
        act->setData(query);
        connect(act, &QAction::triggered, this, &MainWindow::onSearchPresetTriggered);
    }
}

void MainWindow::onSearchPresetTriggered() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    
    QString query = act->data().toString();
    if (m_activePanel) {
        m_activePanel->setSearchQuery(query);
    }
}

MainWindow::~MainWindow() {
    if (m_previewPanel) {
        disconnect(m_previewPanel, nullptr, nullptr, nullptr);
        if (m_previewPanel->player()) {
            disconnect(m_previewPanel->player(), nullptr, nullptr, nullptr);
        }
    }
    if (m_themePlayer) {
        m_themePlayer->stop();
        delete m_themePlayer;
    }
    if (m_themeAudioOutput) {
        delete m_themeAudioOutput;
    }
}

FilePanel* MainWindow::leftPanel() const {
    if (!m_leftTabWidget) return nullptr;
    return qobject_cast<FilePanel*>(m_leftTabWidget->currentWidget());
}

FilePanel* MainWindow::rightPanel() const {
    if (!m_rightTabWidget) return nullptr;
    return qobject_cast<FilePanel*>(m_rightTabWidget->currentWidget());
}

FilePanel* MainWindow::createTab(QTabWidget* tabWidget, const QString& path) {
    FilePanel* panel = new FilePanel(path, this);

    connect(panel, &FilePanel::panelActivated, this, &MainWindow::onPanelActivated);
    connect(panel, &FilePanel::fileSelected, this, &MainWindow::onFileSelected);
    connect(panel, &FilePanel::zenModeToggled, this, &MainWindow::setZenMode);
    connect(panel, &FilePanel::playMediaBuiltinRequested, this, &MainWindow::onPlayMediaBuiltin);
    connect(panel, &FilePanel::playMediaFullscreenRequested, this, &MainWindow::onPlayMediaFullscreen);
    connect(panel, &FilePanel::queueMediaBuiltinRequested, this, &MainWindow::onQueueMediaBuiltin);
    connect(panel, &FilePanel::viewModeChanged, this, &MainWindow::onActivePanelViewModeChanged);
    connect(panel, &FilePanel::folderArtDetected, this, &MainWindow::onFolderArtDetected);
    connect(panel, &FilePanel::pathChanged, this, &MainWindow::onPathChanged);
    connect(panel, &FilePanel::clonePathRequested, this, &MainWindow::onClonePathRequested);
    connect(panel, &FilePanel::tabPressed, this, &MainWindow::onTabPressed);
    connect(panel, &FilePanel::viewModeChanged, this, &MainWindow::updateScrollSyncConnections);
    connect(panel, &FilePanel::openNewTabRequested, this, [this, tabWidget](const QString& targetPath) {
        createTab(tabWidget, targetPath);
    });
    connect(panel, &FilePanel::playlistPlayRequested, this, [this](const QStringList& paths) {
        if (m_previewPanel) m_previewPanel->playPlaylist(paths);
    });
    connect(panel, &FilePanel::saveDefaultProfileRequested, this, &MainWindow::onSaveDefaultProfile);
    connect(panel, &FilePanel::loadDefaultProfileRequested, this, &MainWindow::onLoadDefaultProfile);
    connect(panel, &FilePanel::saveFolderProfileRequested, this, &MainWindow::onSaveFolderProfileForCurrentDir);
    connect(panel, &FilePanel::configureFolderLayoutsRequested, this, &MainWindow::onConfigureFolderLayouts);

    if (m_previewPanel && m_previewPanel->player()) {
        panel->onPlaybackStateChanged(static_cast<int>(m_previewPanel->player()->playbackState()));
    }

    panel->proxyModel()->setAgeColoringEnabled(m_ageColoringEnabled);

    bool isLeft = (tabWidget == m_leftTabWidget);
    if (isLeft) {
        if (m_actLeftShowFilterText) panel->setFilterTextBarVisible(m_actLeftShowFilterText->isChecked());
        if (m_actLeftShowCategoryButtons) panel->setCategoryButtonsVisible(m_actLeftShowCategoryButtons->isChecked());
    } else {
        if (m_actRightShowFilterText) panel->setFilterTextBarVisible(m_actRightShowFilterText->isChecked());
        if (m_actRightShowCategoryButtons) panel->setCategoryButtonsVisible(m_actRightShowCategoryButtons->isChecked());
    }

    // Keep tab title sync'd with path changes
    connect(panel, &FilePanel::pathChanged, this, [tabWidget, panel](const QString& newPath) {
        int idx = tabWidget->indexOf(panel);
        if (idx != -1) {
            QString name = QFileInfo(newPath).fileName();
            if (name.isEmpty()) name = newPath;
            if (panel->isPinned()) name = "📌 " + name;
            if (panel->isPathLocked()) name += " 🔒";
            else if (panel->isPathLockedWithSubdirs()) name += " 📂🔒";
            tabWidget->setTabText(idx, name);
        }
    });

    QString name = QFileInfo(path).fileName();
    if (name.isEmpty()) name = path;

    int index = tabWidget->addTab(panel, name);
    tabWidget->setCurrentIndex(index);

    updateSiblingLinks();
    return panel;
}

void MainWindow::updateSiblingLinks() {
    FilePanel* left = leftPanel();
    FilePanel* right = rightPanel();
    if (left && right) {
        left->setSiblingPanel(right);
        right->setSiblingPanel(left);
    }
    updateScrollSyncConnections();
}

void MainWindow::onLeftTabChanged(int index) {
    Q_UNUSED(index);
    updateSiblingLinks();
    FilePanel* lp = leftPanel();
    if (lp) {
        onPanelActivated(lp);
        QStringList selected = lp->selectedPaths();
        if (!selected.isEmpty()) {
            onFileSelected(selected.first());
        }
    }
}

void MainWindow::onRightTabChanged(int index) {
    Q_UNUSED(index);
    updateSiblingLinks();
    FilePanel* rp = rightPanel();
    if (rp) {
        onPanelActivated(rp);
        QStringList selected = rp->selectedPaths();
        if (!selected.isEmpty()) {
            onFileSelected(selected.first());
        }
    }
}

void MainWindow::onTabCloseRequested(int index) {
    QTabWidget* tabWidget = qobject_cast<QTabWidget*>(sender());
    if (!tabWidget) {
        if (m_activePanel) {
            if (m_leftTabWidget->indexOf(m_activePanel) != -1) {
                tabWidget = m_leftTabWidget;
            } else if (m_rightTabWidget->indexOf(m_activePanel) != -1) {
                tabWidget = m_rightTabWidget;
            }
        }
    }
    if (!tabWidget) return;

    if (tabWidget->count() <= 1) {
        statusBar()->showMessage("Cannot close the last tab.", 3000);
        return;
    }

    QWidget* widget = tabWidget->widget(index);
    FilePanel* panel = qobject_cast<FilePanel*>(widget);
    if (panel && panel->isPinned()) {
        QMessageBox::warning(this, "Pinned Tab", "This tab is pinned and cannot be closed.");
        return;
    }
    tabWidget->removeTab(index);
    widget->deleteLater();

    updateSiblingLinks();
    
    FilePanel* active = qobject_cast<FilePanel*>(tabWidget->currentWidget());
    if (active) {
        onPanelActivated(active);
    }
}

void MainWindow::onNewTabAction() {
    QTabWidget* activeTabWidget = nullptr;
    FilePanel* active = m_activePanel;
    if (active) {
        if (m_leftTabWidget->indexOf(active) != -1) {
            activeTabWidget = m_leftTabWidget;
        } else if (m_rightTabWidget->indexOf(active) != -1) {
            activeTabWidget = m_rightTabWidget;
        }
    }

    if (!activeTabWidget) {
        activeTabWidget = m_leftTabWidget;
    }

    QString currentPath = QDir::homePath();
    FilePanel* currentPanel = qobject_cast<FilePanel*>(activeTabWidget->currentWidget());
    if (currentPanel) {
        currentPath = currentPanel->currentPath();
    }

    createTab(activeTabWidget, currentPath);
    statusBar()->showMessage("New tab opened.", 2000);
}

void MainWindow::onCloseTabAction() {
    QTabWidget* activeTabWidget = nullptr;
    FilePanel* active = m_activePanel;
    if (active) {
        if (m_leftTabWidget->indexOf(active) != -1) {
            activeTabWidget = m_leftTabWidget;
        } else if (m_rightTabWidget->indexOf(active) != -1) {
            activeTabWidget = m_rightTabWidget;
        }
    }

    if (!activeTabWidget) return;

    int idx = activeTabWidget->currentIndex();
    if (idx != -1) {
        onTabCloseRequested(idx);
    }
}

void MainWindow::onShowHelpAction() {
    HelpDialog dlg(this);
    dlg.exec();
}

void MainWindow::onSpaceAnalyzerAction() {
    if (!m_activePanel) return;

    QString startPath = m_activePanel->currentPath();
    SpaceAnalyzerDialog dlg(startPath, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.navigateRequested() && !dlg.selectedPath().isEmpty()) {
            m_activePanel->setPath(dlg.selectedPath());
        }
    }
}

void MainWindow::onToggleFavoritesSidebar(bool checked) {
    if (m_sidebarTabWidget) {
        m_sidebarTabWidget->setVisible(checked);
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("favorites/sidebar_visible", checked);
        adjustSplitterSizes();
    }
}

void MainWindow::onFavoritesSidebarDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    if (m_activePanel) {
        m_activePanel->setPath(path);
    }
}

void MainWindow::refreshFavoritesSidebar() {
    if (!m_favoritesSidebar) return;
    m_favoritesSidebar->clear();

    QStringList favorites = FavoritesManager::instance().getFavorites();
    for (const QString& path : favorites) {
        QFileInfo info(path);
        QString folderName = info.fileName();
        if (folderName.isEmpty()) {
            folderName = path;
        }

        QListWidgetItem* item = new QListWidgetItem(m_favoritesSidebar);
        item->setText(folderName);
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        item->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        m_favoritesSidebar->addItem(item);
    }

    QStringList dynamicPaths = FavoritesManager::instance().getEvaluatedDynamicPaths();
    if (!dynamicPaths.isEmpty()) {
        QListWidgetItem* separatorItem = new QListWidgetItem(m_favoritesSidebar);
        separatorItem->setText("── Dynamic Bookmarks ──");
        separatorItem->setFlags(Qt::NoItemFlags);
        separatorItem->setTextAlignment(Qt::AlignCenter);
        separatorItem->setForeground(QBrush(QColor("#f5c2e7")));
        m_favoritesSidebar->addItem(separatorItem);

        for (const QString& path : dynamicPaths) {
            QFileInfo info(path);
            QString folderName = info.fileName();
            if (folderName.isEmpty()) {
                folderName = path;
            }

            QListWidgetItem* item = new QListWidgetItem(m_favoritesSidebar);
            item->setText(folderName);
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
            item->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogListView));
            m_favoritesSidebar->addItem(item);
        }
    }

    QList<DynamicFavoriteRule> smartRules = FavoritesManager::instance().getDynamicRules();
    if (!smartRules.isEmpty()) {
        QListWidgetItem* separatorItem = new QListWidgetItem(m_favoritesSidebar);
        separatorItem->setText("── Smart Virtual Folders ──");
        separatorItem->setFlags(Qt::NoItemFlags);
        separatorItem->setTextAlignment(Qt::AlignCenter);
        separatorItem->setForeground(QBrush(QColor("#a6e3a1"))); // light green
        m_favoritesSidebar->addItem(separatorItem);

        for (const auto& rule : smartRules) {
            QListWidgetItem* item = new QListWidgetItem(m_favoritesSidebar);
            item->setText(rule.name);
            item->setToolTip(QString("Smart dynamic query: %1 (%2)").arg(rule.ruleType).arg(rule.value));
            item->setData(Qt::UserRole, "smart://" + rule.name);
            item->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView));
            m_favoritesSidebar->addItem(item);
        }
    }
}

void MainWindow::onQuickFilterSidebarClicked(QListWidgetItem* item) {
    if (!item) return;
    if (!m_activePanel || !m_activePanel->proxyModel()) return;

    QString text = item->text();
    FileFilterProxyModel* model = m_activePanel->proxyModel();

    if (text == "Show All Files") {
        model->clearAdvancedFilters();
    } else if (text == "Large Files (> 100 MB)") {
        model->setSizeFilter(100ULL * 1024 * 1024, -1);
    } else if (text == "Medium Files (1 - 100 MB)") {
        model->setSizeFilter(1024 * 1024, 100ULL * 1024 * 1024);
    } else if (text == "Small Files (< 1 MB)") {
        model->setSizeFilter(0, 1024 * 1024 - 1);
    } else if (text == "Modified Today") {
        QDateTime todayStart(QDate::currentDate(), QTime(0, 0, 0));
        model->setDateFilter(todayStart, QDateTime());
    } else if (text == "Modified This Week") {
        QDateTime weekStart(QDate::currentDate().addDays(-7), QTime(0, 0, 0));
        model->setDateFilter(weekStart, QDateTime());
    } else if (text == "Modified This Month") {
        QDateTime monthStart(QDate::currentDate().addMonths(-1), QTime(0, 0, 0));
        model->setDateFilter(monthStart, QDateTime());
    }
}

void MainWindow::registerKeybindableAction(const QString& id, QAction* action) {
    if (action) {
        m_keybindableActions[id] = action;
    }
}

void MainWindow::loadKeybindings() {
    // Initialize default keybindings mapping
    m_keybindings["copy"] = QKeySequence::Copy;
    m_keybindings["cut"] = QKeySequence::Cut;
    m_keybindings["paste"] = QKeySequence::Paste;
    m_keybindings["delete"] = QKeySequence::Delete;
    m_keybindings["rename"] = QKeySequence(Qt::Key_F2);
    m_keybindings["new_folder"] = QKeySequence("Ctrl+N");
    m_keybindings["properties"] = QKeySequence("Alt+Return");
    m_keybindings["refresh"] = QKeySequence(Qt::Key_F5);
    m_keybindings["toggle_dual_pane"] = QKeySequence("Ctrl+D");
    m_keybindings["toggle_preview"] = QKeySequence("Ctrl+P");
    m_keybindings["mute_preview"] = QKeySequence("Ctrl+M");
    m_keybindings["toggle_flat_view"] = QKeySequence("Ctrl+F");
    m_keybindings["compare_sync"] = QKeySequence("Ctrl+S");
    m_keybindings["space_analyzer"] = QKeySequence("Ctrl+K");
    m_keybindings["dup_finder"] = QKeySequence("Ctrl+J");
    m_keybindings["help"] = QKeySequence(Qt::Key_F1);
    m_keybindings["remote_mount"] = QKeySequence("Ctrl+R");
    m_keybindings["cloud_mount"] = QKeySequence("Ctrl+Shift+G");
    m_keybindings["batch_images"] = QKeySequence("Ctrl+I");
    m_keybindings["process_manager"] = QKeySequence("Ctrl+Alt+P");
    m_keybindings["vault_encrypt"] = QKeySequence("Ctrl+Shift+E");
    m_keybindings["vault_decrypt"] = QKeySequence("Ctrl+Shift+D");
    m_keybindings["shred"] = QKeySequence("Shift+Delete");
    m_keybindings["checksum"] = QKeySequence("Ctrl+H");
    m_keybindings["configure_layouts"] = QKeySequence("Ctrl+Shift+L");
    m_keybindings["configure_age_styles"] = QKeySequence("Ctrl+Shift+A");

    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("Keybindings");
    for (const QString& id : m_keybindings.keys()) {
        if (settings.contains(id)) {
            m_keybindings[id] = QKeySequence(settings.value(id).toString());
        }
    }
    settings.endGroup();
}

void MainWindow::saveKeybindings() {
    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("Keybindings");
    for (auto it = m_keybindings.begin(); it != m_keybindings.end(); ++it) {
        settings.setValue(it.key(), it.value().toString());
    }
    settings.endGroup();
}

void MainWindow::applyKeybindings() {
    for (auto it = m_keybindings.begin(); it != m_keybindings.end(); ++it) {
        QAction* act = m_keybindableActions.value(it.key());
        if (act) {
            act->setShortcut(it.value());
        }
    }
}

void MainWindow::onKeybindingsEditorAction() {
    KeybindingsEditorDialog dlg(m_keybindings, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_keybindings = dlg.getNewBindings();
        saveKeybindings();
        applyKeybindings();
        statusBar()->showMessage("Keyboard shortcuts updated.", 3000);
    }
}

void MainWindow::onCalculateChecksum() {
    QString activePath;
    if (m_activePanel) {
        QStringList paths = m_activePanel->selectedPaths();
        if (!paths.isEmpty() && QFileInfo(paths.first()).isFile()) {
            activePath = paths.first();
        }
    }
    ChecksumDialog dlg(activePath, this);
    dlg.exec();
}

void MainWindow::onSecureShred() {
    if (!m_activePanel) return;
    QStringList paths = m_activePanel->selectedPaths();
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Secure Shredder", "Please select one or more files/folders in the file view panel first.");
        return;
    }

    ShredDialog dlg(paths, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_activePanel->refresh();
    }
}

void MainWindow::onRemoteMount() {
    RemoteMountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        updateDrivesList();
    }
}

void MainWindow::onCloudMount() {
    CloudMountDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        updateDrivesList();
    }
}

void MainWindow::onImageConvert() {
    if (!m_activePanel) return;
    QStringList paths = m_activePanel->selectedPaths();
    QStringList imageExts = { "jpg", "jpeg", "png", "webp", "bmp" };
    QStringList selectedImages;
    for (const QString& path : paths) {
        if (imageExts.contains(QFileInfo(path).suffix().toLower())) {
            selectedImages.append(path);
        }
    }

    if (selectedImages.isEmpty()) {
        QMessageBox::information(this, "Batch Image Converter", "Please select one or more images (JPEG, PNG, WEBP, BMP) in the active panel first.");
        return;
    }

    ImageConverterDialog dlg(selectedImages, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_activePanel->refresh();
    }
}

void MainWindow::onTagsSidebarClicked(QListWidgetItem* item) {
    if (!item) return;
    if (!m_activePanel || !m_activePanel->proxyModel()) return;

    QString text = item->text();
    FileFilterProxyModel* model = m_activePanel->proxyModel();

    if (text == "Clear Tag Filter") {
        model->clearAdvancedFilters();
    } else {
        model->setTagFilter(text);
    }
}

void MainWindow::refreshTagsSidebar() {
    if (!m_tagsSidebar) return;
    m_tagsSidebar->clear();

    m_tagsSidebar->addItem("Clear Tag Filter");

    QStringList allTags = TagManager::instance().getAllTags();
    for (const QString& tag : allTags) {
        QListWidgetItem* item = new QListWidgetItem(m_tagsSidebar);
        item->setText(tag);
        item->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirLinkIcon));
        m_tagsSidebar->addItem(item);
    }
}

void MainWindow::onProcessManagerAction() {
    ProcessManagerDialog dlg(this);
    dlg.exec();
}

void MainWindow::onEncryptVault() {
    QString activePath;
    if (m_activePanel) {
        QStringList paths = m_activePanel->selectedPaths();
        if (!paths.isEmpty()) activePath = paths.first();
    }
    VaultDialog dlg(true, activePath, this);
    if (dlg.exec() == QDialog::Accepted && m_activePanel) {
        m_activePanel->refresh();
    }
}

void MainWindow::onDecryptVault() {
    QString activePath;
    if (m_activePanel) {
        QStringList paths = m_activePanel->selectedPaths();
        if (!paths.isEmpty() && QFileInfo(paths.first()).suffix().toLower() == "vault") {
            activePath = paths.first();
        }
    }
    VaultDialog dlg(false, activePath, this);
    if (dlg.exec() == QDialog::Accepted && m_activePanel) {
        m_activePanel->refresh();
        registerDecryptedVault(dlg.decryptedPath(), dlg.vaultPath(), dlg.password());
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings("Amifiles", "Amifiles");
    if (m_actAutoSaveLayout && m_actAutoSaveLayout->isChecked()) {
        settings.setValue("window/geometry", saveGeometry());
        settings.setValue("window/state", saveState());
    }
    QMainWindow::closeEvent(event);
}

QMenu* MainWindow::createPopupMenu() {
    QMenu* menu = QMainWindow::createPopupMenu();
    if (!menu) {
        menu = new QMenu(this);
    }
    
    menu->addSeparator();
    QAction* actConfigure = menu->addAction("Folder Profiles & Layouts...");
    QAction* actSaveFolder = menu->addAction("Save Current Layout as Folder Profile...");

    QMenu* menuApplyProfile = menu->addMenu("Apply Profile Layout to Current Folder");
    for (const auto& r : m_folderRules) {
        if (!r.name.isEmpty()) {
            QString profileName = r.name;
            QAction* act = menuApplyProfile->addAction(profileName);
            connect(act, &QAction::triggered, this, [this, profileName]() {
                onApplyProfileToCurrentFolder(profileName);
            });
        }
    }

    QAction* actSaveDefault = menu->addAction("Save Current Layout as Default Profile");
    QAction* actLoadDefault = menu->addAction("Load Default Profile");
    
    connect(actConfigure, &QAction::triggered, this, &MainWindow::onConfigureFolderLayouts);
    connect(actSaveFolder, &QAction::triggered, this, &MainWindow::onSaveFolderProfileForCurrentDir);
    connect(actSaveDefault, &QAction::triggered, this, &MainWindow::onSaveDefaultProfile);
    connect(actLoadDefault, &QAction::triggered, this, &MainWindow::onLoadDefaultProfile);
    
    return menu;
}

void MainWindow::onSaveLayoutNow() {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
    QMessageBox::information(this, "Save Layout", "Current layout (toolbar positions and window size) saved successfully!");
}

void MainWindow::onResetLayout() {
    QSettings settings("Amifiles", "Amifiles");
    settings.remove("window/geometry");
    settings.remove("window/state");
    settings.remove("custom_toolbars_v1");

    rebuildToolBars();

    QMessageBox::information(this, "Reset Layout", "Window geometry and toolbar layouts reset to default successfully!");
}

QJsonObject MainWindow::ruleToJson(const FolderLayoutRule& r) {
    QJsonObject obj;
    obj["name"] = r.name;
    obj["ruleType"] = r.ruleType;
    obj["value"] = r.value;
    obj["viewMode"] = r.viewMode;
    obj["customButtons"] = QJsonArray::fromStringList(r.customButtons);
    obj["autoApply"] = r.autoApply;
    
    obj["overrideDrivesToolbar"] = r.overrideDrivesToolbar;
    obj["drivesToolbarVisible"] = r.drivesToolbarVisible;
    obj["overrideCenterOps"] = r.overrideCenterOps;
    obj["centerOpsVisible"] = r.centerOpsVisible;
    obj["overrideConsole"] = r.overrideConsole;
    obj["consoleVisible"] = r.consoleVisible;
    obj["overridePreview"] = r.overridePreview;
    obj["previewVisible"] = r.previewVisible;
    obj["overrideFavoritesSidebar"] = r.overrideFavoritesSidebar;
    obj["favoritesSidebarVisible"] = r.favoritesSidebarVisible;
    obj["overrideZenMode"] = r.overrideZenMode;
    obj["zenModeActive"] = r.zenModeActive;
    obj["overrideBuiltinPlayerDoubleclick"] = r.overrideBuiltinPlayerDoubleclick;
    obj["builtinPlayerDoubleclick"] = r.builtinPlayerDoubleclick;
    obj["overrideFullScreenPlayer"] = r.overrideFullScreenPlayer;
    obj["fullScreenPlayerActive"] = r.fullScreenPlayerActive;
    obj["overrideVisualizer"] = r.overrideVisualizer;
    obj["visualizerActive"] = r.visualizerActive;

    obj["overrideToolbars"] = r.overrideToolbars;
    obj["selectedToolbars"] = QJsonArray::fromStringList(r.selectedToolbars);
    obj["overrideMenus"] = r.overrideMenus;
    obj["selectedMenus"] = QJsonArray::fromStringList(r.selectedMenus);
    
    obj["useBgColor"] = r.useBgColor;
    obj["bgColor"] = r.bgColor;
    obj["windowState"] = QString::fromLatin1(r.windowState.toBase64());
    
    obj["hasTabsSnapshot"] = r.hasTabsSnapshot;
    obj["leftPaths"] = QJsonArray::fromStringList(r.leftPaths);
    obj["leftActiveIndex"] = r.leftActiveIndex;
    obj["rightPaths"] = QJsonArray::fromStringList(r.rightPaths);
    obj["rightActiveIndex"] = r.rightActiveIndex;
    obj["linkedProfile"] = r.linkedProfile;
    obj["entryCommand"] = r.entryCommand;
    obj["subfolderDepth"] = r.subfolderDepth;
    return obj;
}

FolderLayoutRule MainWindow::jsonToRule(const QJsonObject& obj) {
    FolderLayoutRule r;
    r.name = obj["name"].toString();
    r.linkedProfile = obj["linkedProfile"].toString("");
    r.ruleType = obj["ruleType"].toString();
    r.value = obj["value"].toString();
    r.entryCommand = obj["entryCommand"].toString("");
    r.subfolderDepth = obj["subfolderDepth"].toInt(3);
    r.viewMode = obj["viewMode"].toString("No Change");
    
    QJsonArray btns = obj["customButtons"].toArray();
    for (auto b : btns) r.customButtons.append(b.toString());
    
    r.autoApply = obj["autoApply"].toBool(true);
    
    r.overrideDrivesToolbar = obj["overrideDrivesToolbar"].toBool(false);
    r.drivesToolbarVisible = obj["drivesToolbarVisible"].toBool(false);
    r.overrideCenterOps = obj["overrideCenterOps"].toBool(false);
    r.centerOpsVisible = obj["centerOpsVisible"].toBool(false);
    r.overrideConsole = obj["overrideConsole"].toBool(false);
    r.consoleVisible = obj["consoleVisible"].toBool(false);
    r.overridePreview = obj["overridePreview"].toBool(false);
    r.previewVisible = obj["previewVisible"].toBool(false);
    r.overrideFavoritesSidebar = obj["overrideFavoritesSidebar"].toBool(false);
    r.favoritesSidebarVisible = obj["favoritesSidebarVisible"].toBool(false);
    r.overrideZenMode = obj["overrideZenMode"].toBool(false);
    r.zenModeActive = obj["zenModeActive"].toBool(false);
    r.overrideBuiltinPlayerDoubleclick = obj["overrideBuiltinPlayerDoubleclick"].toBool(false);
    r.builtinPlayerDoubleclick = obj["builtinPlayerDoubleclick"].toBool(false);
    r.overrideFullScreenPlayer = obj["overrideFullScreenPlayer"].toBool(false);
    r.fullScreenPlayerActive = obj["fullScreenPlayerActive"].toBool(false);
    r.overrideVisualizer = obj["overrideVisualizer"].toBool(false);
    r.visualizerActive = obj["visualizerActive"].toBool(false);

    r.overrideToolbars = obj["overrideToolbars"].toBool(false);
    QJsonArray tbs = obj["selectedToolbars"].toArray();
    for (auto tb : tbs) r.selectedToolbars.append(tb.toString());
    
    r.overrideMenus = obj["overrideMenus"].toBool(false);
    QJsonArray mns = obj["selectedMenus"].toArray();
    for (auto mn : mns) r.selectedMenus.append(mn.toString());
    
    r.useBgColor = obj["useBgColor"].toBool(false);
    r.bgColor = obj["bgColor"].toString();
    r.windowState = QByteArray::fromBase64(obj["windowState"].toString().toLatin1());
    
    r.hasTabsSnapshot = obj["hasTabsSnapshot"].toBool(false);
    
    QJsonArray lp = obj["leftPaths"].toArray();
    for (auto p : lp) r.leftPaths.append(p.toString());
    r.leftActiveIndex = obj["leftActiveIndex"].toInt(0);
    
    QJsonArray rp = obj["rightPaths"].toArray();
    for (auto p : rp) r.rightPaths.append(p.toString());
    r.rightActiveIndex = obj["rightActiveIndex"].toInt(0);
    return r;
}

void MainWindow::loadFolderRules() {
    m_folderRules.clear();
    QSettings settings("Amifiles", "Amifiles");
    
    if (settings.contains("folder_profiles_v1")) {
        QString jsonStr = settings.value("folder_profiles_v1").toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            m_folderRules.append(jsonToRule(arr[i].toObject()));
        }
    } else {
        // Migrate old folder layout rules
        QStringList serialized = settings.value("folder_layout_rules").toStringList();
        for (const QString& s : serialized) {
            QStringList parts = s.split(';');
            if (parts.size() >= 4) {
                FolderLayoutRule r;
                r.name = QString("Migrated Rule: %1").arg(parts[1]);
                r.ruleType = parts[0];
                r.value = parts[1];
                r.viewMode = parts[2];
                r.customButtons = parts[3].isEmpty() ? QStringList() : parts[3].split(',');
                r.autoApply = true;
                m_folderRules.append(r);
            }
        }
        
        // Migrate old workspace profiles
        QStringList profiles = settings.value("workspace_profiles/profile_list").toStringList();
        for (const QString& name : profiles) {
            FolderLayoutRule r;
            r.name = name;
            r.ruleType = "Path";
            r.value = "";
            r.autoApply = false; // Manual apply only
            
            r.leftPaths = settings.value(QString("workspace_profiles/%1/left_paths").arg(name)).toStringList();
            r.leftActiveIndex = settings.value(QString("workspace_profiles/%1/left_active").arg(name), 0).toInt();
            int leftViewMode = settings.value(QString("workspace_profiles/%1/left_view_mode").arg(name), 0).toInt();
            if (leftViewMode == 0) r.viewMode = "List";
            else if (leftViewMode == 1) r.viewMode = "Grid";
            else if (leftViewMode == 2) r.viewMode = "Card";
            else if (leftViewMode == 3) r.viewMode = "Miller";
            else if (leftViewMode == 4) r.viewMode = "Timeline";
            else if (leftViewMode == 5) r.viewMode = "Filmstrip";
            else if (leftViewMode == 6) r.viewMode = "Theater";
            
            r.rightPaths = settings.value(QString("workspace_profiles/%1/right_paths").arg(name)).toStringList();
            r.rightActiveIndex = settings.value(QString("workspace_profiles/%1/right_active").arg(name), 0).toInt();
            
            r.overrideDrivesToolbar = true;
            r.drivesToolbarVisible = true;
            
            r.overrideCenterOps = true;
            r.centerOpsVisible = true;
            
            r.overrideConsole = true;
            r.consoleVisible = settings.value(QString("workspace_profiles/%1/console_visible").arg(name), true).toBool();
            
            r.overridePreview = true;
            r.previewVisible = settings.value(QString("workspace_profiles/%1/right_visible").arg(name), true).toBool();
            
            r.overrideFavoritesSidebar = true;
            r.favoritesSidebarVisible = settings.value(QString("workspace_profiles/%1/sidebar_visible").arg(name), true).toBool();
            
            r.hasTabsSnapshot = true;
            m_folderRules.append(r);
        }
        
        saveFolderRules();
    }

    // Ensure "Default" profile and helper preset profiles exist
    bool defaultExists = false;
    bool musicExists = false;
    bool moviesExists = false;
    bool tvShowsExists = false;
    bool picturesExists = false;

    for (const auto& r : m_folderRules) {
        QString name = r.name.toLower().trimmed();
        if (name == "default") defaultExists = true;
        else if (name == "music") musicExists = true;
        else if (name == "movies") moviesExists = true;
        else if (name == "tv shows") tvShowsExists = true;
        else if (name == "pictures") picturesExists = true;
    }

    bool addedPreset = false;

    if (!defaultExists) {
        FolderLayoutRule r;
        r.name = "Default";
        r.ruleType = "Path";
        r.value = "";
        r.autoApply = true;
        r.viewMode = "No Change";
        r.overrideConsole = true;
        r.consoleVisible = false;
        r.overrideDrivesToolbar = true;
        r.drivesToolbarVisible = true;
        r.overridePreview = true;
        r.previewVisible = false;
        r.overrideCenterOps = true;
        r.centerOpsVisible = true;
        r.overrideFavoritesSidebar = true;
        r.favoritesSidebarVisible = true;
        m_folderRules.prepend(r); // Keep Default at the very top
        addedPreset = true;
    }

    if (!musicExists) {
        FolderLayoutRule r;
        r.name = "Music";
        r.ruleType = "Category";
        r.value = "Music";
        r.autoApply = true;
        r.viewMode = "Music Showcase";
        r.overridePreview = true;
        r.previewVisible = true;
        r.overrideDrivesToolbar = true;
        r.drivesToolbarVisible = false;
        r.overrideConsole = true;
        r.consoleVisible = false;
        r.overrideFavoritesSidebar = true;
        r.favoritesSidebarVisible = false;
        m_folderRules.append(r);
        addedPreset = true;
    }

    if (!moviesExists) {
        FolderLayoutRule r;
        r.name = "Movies";
        r.ruleType = "Category";
        r.value = "Videos";
        r.autoApply = true;
        r.viewMode = "Cinema Showcase";
        r.overridePreview = true;
        r.previewVisible = true;
        r.overrideDrivesToolbar = true;
        r.drivesToolbarVisible = false;
        r.overrideConsole = true;
        r.consoleVisible = false;
        r.overrideFavoritesSidebar = true;
        r.favoritesSidebarVisible = false;
        m_folderRules.append(r);
        addedPreset = true;
    }

    if (!tvShowsExists) {
        FolderLayoutRule r;
        r.name = "TV Shows";
        r.ruleType = "Category";
        r.value = "Videos";
        r.autoApply = true;
        r.viewMode = "Cinema Showcase";
        r.overridePreview = true;
        r.previewVisible = true;
        r.overrideDrivesToolbar = true;
        r.drivesToolbarVisible = false;
        r.overrideConsole = true;
        r.consoleVisible = false;
        r.overrideFavoritesSidebar = true;
        r.favoritesSidebarVisible = false;
        m_folderRules.append(r);
        addedPreset = true;
    }

    if (!picturesExists) {
        FolderLayoutRule r;
        r.name = "Pictures";
        r.ruleType = "Category";
        r.value = "Images";
        r.autoApply = true;
        r.viewMode = "Grid";
        r.overridePreview = true;
        r.previewVisible = false;
        r.overrideDrivesToolbar = true;
        r.drivesToolbarVisible = true;
        r.overrideConsole = true;
        r.consoleVisible = false;
        r.overrideFavoritesSidebar = true;
        r.favoritesSidebarVisible = true;
        m_folderRules.append(r);
        addedPreset = true;
    }

    if (addedPreset) {
        saveFolderRules();
    }
}

void MainWindow::saveFolderRules() {
    QSettings settings("Amifiles", "Amifiles");
    QJsonArray arr;
    for (const auto& r : m_folderRules) {
        arr.append(ruleToJson(r));
    }
    QJsonDocument doc(arr);
    settings.setValue("folder_profiles_v1", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

bool MainWindow::isToolbarDefaultVisible(const QString& toolbarId) {
    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_toolbars_v1").toString();
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject tbObj = arr[i].toObject();
            if (tbObj["id"].toString() == toolbarId) {
                return tbObj["visible"].toBool(true);
            }
        }
    }
    return true;
}

void MainWindow::applyProfile(const FolderLayoutRule& r, FilePanel* targetPanel) {
    if (!targetPanel) targetPanel = m_activePanel;
    if (!targetPanel) return;

    m_activeFolderRule = r;
    m_hasActiveFolderRule = true;

    if (!m_isInitializing) {
        if (!r.windowState.isEmpty()) {
            restoreState(r.windowState);
        } else {
            QSettings settings("Amifiles", "Amifiles");
            QByteArray defaultState = settings.value("window/state").toByteArray();
            if (!defaultState.isEmpty()) {
                restoreState(defaultState);
            }
        }
    }

    // 1. View Mode
    if (r.viewMode != "No Change" && !r.viewMode.isEmpty()) {
        if (r.viewMode == "List") targetPanel->setViewModeIndex(0);
        else if (r.viewMode == "Grid") targetPanel->setViewModeIndex(1);
        else if (r.viewMode == "Card") targetPanel->setViewModeIndex(2);
        else if (r.viewMode == "Miller") targetPanel->setViewModeIndex(3);
        else if (r.viewMode == "Timeline") targetPanel->setViewModeIndex(4);
        else if (r.viewMode == "Filmstrip") targetPanel->setViewModeIndex(5);
        else if (r.viewMode == "Theater" || r.viewMode == "Music Showcase" || r.viewMode == "Audio Showcase" || r.viewMode == "Audio Showcase (Classic)") targetPanel->setViewModeIndex(6);
        else if (r.viewMode == "Cinema Showcase" || r.viewMode == "Video Showcase" || r.viewMode == "Video Showcase (Classic)") targetPanel->setViewModeIndex(7);
        else if (r.viewMode == "Movies Full Screen" || r.viewMode == "Movie Showcase (v2)") targetPanel->setViewModeIndex(8);
        else if (r.viewMode == "TV Shows Full Screen" || r.viewMode == "TV Show Showcase (v2)") targetPanel->setViewModeIndex(9);
        else if (r.viewMode == "Music Full Screen" || r.viewMode == "Music Showcase (v2)") targetPanel->setViewModeIndex(10);
    }

    // 2. Toolbar filter
    m_activeToolbarFilter = r.customButtons;
    rebuildCustomToolBar();

    // 3. Visibility overrides
    if (r.overrideDrivesToolbar) {
        m_actToggleDrivesToolbar->setChecked(r.drivesToolbarVisible);
        if (m_tbDrives) m_tbDrives->setVisible(r.drivesToolbarVisible);
    }
    if (r.overrideCenterOps) {
        m_actToggleCenterOps->setChecked(r.centerOpsVisible);
        if (m_tbCenterOps) m_tbCenterOps->setVisible(r.centerOpsVisible && m_isDualPane);
    }
    if (r.overrideConsole) {
        m_actToggleConsole->setChecked(r.consoleVisible);
        if (m_bottomTabWidget) m_bottomTabWidget->setVisible(r.consoleVisible);
    }
    if (r.overridePreview) {
        m_actTogglePreview->setChecked(r.previewVisible);
        if (m_previewDock) m_previewDock->setVisible(r.previewVisible);
    }
    if (r.overrideFavoritesSidebar) {
        m_actToggleFavoritesSidebar->setChecked(r.favoritesSidebarVisible);
        if (m_sidebarTabWidget) m_sidebarTabWidget->setVisible(r.favoritesSidebarVisible);
    }
    if (r.overrideZenMode) {
        setZenMode(r.zenModeActive);
    }

    // 3b. Custom Toolbars visibility overrides
    if (r.overrideToolbars) {
        for (QToolBar* tb : m_dynamicToolBars) {
            tb->setVisible(r.selectedToolbars.contains(tb->objectName()));
        }
    } else {
        QSettings settings("Amifiles", "Amifiles");
        QString jsonStr = settings.value("custom_toolbars_v1").toString();
        if (!jsonStr.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            QJsonArray arr = doc.array();
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject tbObj = arr[i].toObject();
                QString id = tbObj["id"].toString();
                bool visible = tbObj["visible"].toBool(true);
                for (QToolBar* tb : m_dynamicToolBars) {
                    if (tb->objectName() == id) {
                        tb->setVisible(visible);
                        break;
                    }
                }
            }
        } else {
            for (QToolBar* tb : m_dynamicToolBars) {
                tb->setVisible(true);
            }
        }
    }

    // 3c. Custom Menus visibility overrides
    if (r.overrideMenus) {
        for (QMenu* menu : m_customMenus) {
            if (menu->menuAction()) {
                menu->menuAction()->setVisible(r.selectedMenus.contains(menu->title()));
            }
        }
    } else {
        for (QMenu* menu : m_customMenus) {
            if (menu->menuAction()) {
                menu->menuAction()->setVisible(true);
            }
        }
    }

    // 4. Background styling
    if (r.useBgColor) {
        targetPanel->setCustomBgColor(r.bgColor);
    } else {
        targetPanel->setCustomBgColor("");
    }

    // 4b. Visualizer override
    if (r.overrideVisualizer) {
        if (m_previewPanel) {
            m_previewPanel->setSpectrumVisualizerVisible(r.visualizerActive);
        }
    }

    // 5. Tabs Snapshot
    if (r.hasTabsSnapshot && !r.leftPaths.isEmpty()) {
        while (m_leftTabWidget->count() > 0) {
            QWidget* w = m_leftTabWidget->widget(0);
            m_leftTabWidget->removeTab(0);
            delete w;
        }
        for (const QString& p : r.leftPaths) {
            createTab(m_leftTabWidget, p);
        }
        if (m_leftTabWidget->count() > 0) {
            m_leftTabWidget->setCurrentIndex(qBound(0, r.leftActiveIndex, m_leftTabWidget->count() - 1));
        }

        while (m_rightTabWidget->count() > 0) {
            QWidget* w = m_rightTabWidget->widget(0);
            m_rightTabWidget->removeTab(0);
            delete w;
        }
        for (const QString& p : r.rightPaths) {
            createTab(m_rightTabWidget, p);
        }
        if (m_rightTabWidget->count() > 0) {
            m_rightTabWidget->setCurrentIndex(qBound(0, r.rightActiveIndex, m_rightTabWidget->count() - 1));
        }
        updateSiblingLinks();
    }

    // 6. Doubleclick playback preference override
    bool isDoubleclickActive = false;
    if (r.overrideBuiltinPlayerDoubleclick) {
        isDoubleclickActive = r.builtinPlayerDoubleclick;
    } else {
        QSettings settings("Amifiles", "Amifiles");
        isDoubleclickActive = settings.value("preferences/builtin_player_doubleclick", false).toBool();
    }
    emit builtinPlayerDoubleclickChanged(isDoubleclickActive);
    targetPanel->updateThemeMusic();
}

void MainWindow::applyFolderRules(const QString& path) {
    if (!m_activePanel) return;

    QSettings settings("Amifiles", "Amifiles");
    bool bypassRules = settings.value("preferences/bypass_folder_rules", false).toBool();
    if (bypassRules) {
        return;
    }

    FolderLayoutRule matchedRule;
    bool foundMatch = false;
        // 1. Path matching (with exact match preference and subfolder depth inheritance)
        int bestMatchDepth = 9999;
        for (const auto& r : m_folderRules) {
            if (!r.autoApply) continue;
            if (r.ruleType == "Path") {
                QString targetPath = QDir::cleanPath(r.value);
                QString currentCleanPath = QDir::cleanPath(path);

                if (currentCleanPath == targetPath) {
                    matchedRule = r;
                    foundMatch = true;
                    break;
                } else if (r.subfolderDepth > 0 && currentCleanPath.startsWith(targetPath + "/")) {
                    QString relPath = currentCleanPath.mid(targetPath.length() + 1);
                    int depth = relPath.split('/', Qt::SkipEmptyParts).size();
                    if (depth <= r.subfolderDepth || r.subfolderDepth >= 999) {
                        if (depth < bestMatchDepth) {
                            bestMatchDepth = depth;
                            matchedRule = r;
                            foundMatch = true;
                        }
                    }
                }
            }
        }

        // 2. Category matching
        if (!foundMatch) {
            QString category = detectFolderCategory(path);
            if (!category.isEmpty()) {
                for (const auto& r : m_folderRules) {
                    if (!r.autoApply) continue;
                    if (r.ruleType == "Category" && r.value == category) {
                        matchedRule = r;
                        foundMatch = true;
                        break;
                    }
                }
            }
        }

        // 3. Smart Rules extensions: Extensions, SizeGreater, Metadata matching
        if (!foundMatch) {
            for (const auto& r : m_folderRules) {
                if (!r.autoApply) continue;

                if (r.ruleType == "Extensions") {
                    QDir dir(path);
                    QStringList exts = r.value.split(',', Qt::SkipEmptyParts);
                    for (auto& ext : exts) ext = ext.trimmed().toLower();
                    
                    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
                    bool extMatch = false;
                    for (const QFileInfo& fi : fileList) {
                        if (exts.contains(fi.suffix().toLower())) {
                            extMatch = true;
                            break;
                        }
                    }
                    if (extMatch) {
                        matchedRule = r;
                        foundMatch = true;
                        break;
                    }
                }
                else if (r.ruleType == "SizeGreater") {
                    qint64 thresholdMB = r.value.toLongLong();
                    qint64 totalSize = 0;
                    QDir dir(path);
                    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
                    for (const QFileInfo& fi : fileList) {
                        totalSize += fi.size();
                    }
                    qint64 sizeMB = totalSize / (1024 * 1024);
                    if (sizeMB >= thresholdMB) {
                        matchedRule = r;
                        foundMatch = true;
                        break;
                    }
                }
                else if (r.ruleType == "Metadata") {
                    QStringList parts = r.value.split('=', Qt::SkipEmptyParts);
                    if (parts.size() == 2) {
                        QString key = parts[0].trimmed();
                        QString val = parts[1].trimmed();
                        QString tagVal = TagManager::instance().getCustomAttribute(path, key);
                        if (tagVal.compare(val, Qt::CaseInsensitive) == 0) {
                            matchedRule = r;
                            foundMatch = true;
                            break;
                        }
                    }
                }
            }
        }

    if (foundMatch) {
        if (!matchedRule.linkedProfile.isEmpty() && matchedRule.linkedProfile != matchedRule.name) {
            FolderLayoutRule inheritedRule = matchedRule;
            bool foundInherited = false;
            for (const auto& r : m_folderRules) {
                if (r.name == matchedRule.linkedProfile) {
                    inheritedRule = r;
                    inheritedRule.name = matchedRule.name;
                    inheritedRule.ruleType = matchedRule.ruleType;
                    inheritedRule.value = matchedRule.value;
                    inheritedRule.autoApply = matchedRule.autoApply;
                    foundInherited = true;
                    break;
                }
            }
            applyProfile(inheritedRule, m_activePanel);
        } else {
            applyProfile(matchedRule, m_activePanel);
        }
    } else {
        FolderLayoutRule defaultRule;
        bool foundDefault = false;
        for (const auto& r : m_folderRules) {
            if (r.name.toLower() == "default") {
                defaultRule = r;
                foundDefault = true;
                break;
            }
        }
        if (foundDefault) {
            if (!defaultRule.linkedProfile.isEmpty() && defaultRule.linkedProfile != defaultRule.name) {
                FolderLayoutRule inheritedRule = defaultRule;
                for (const auto& r : m_folderRules) {
                    if (r.name == defaultRule.linkedProfile) {
                        inheritedRule = r;
                        inheritedRule.name = defaultRule.name;
                        inheritedRule.ruleType = defaultRule.ruleType;
                        inheritedRule.value = defaultRule.value;
                        inheritedRule.autoApply = defaultRule.autoApply;
                        break;
                    }
                }
                applyProfile(inheritedRule, m_activePanel);
            } else {
                applyProfile(defaultRule, m_activePanel);
            }
        } else {
            m_hasActiveFolderRule = false;
            
            QSettings settings("Amifiles", "Amifiles");
            QByteArray defaultState = settings.value("window/state").toByteArray();
            if (!defaultState.isEmpty()) {
                restoreState(defaultState);
            }

            m_activePanel->setCustomBgColor("");
            if (!m_activeToolbarFilter.isEmpty()) {
                m_activeToolbarFilter.clear();
                rebuildCustomToolBar();
            }
            
            // Revert View Mode to default preferred view mode
            int defaultViewMode = settings.value("file_panel/view_mode_index", 0).toInt();
            m_activePanel->setViewModeIndex(defaultViewMode);

            // Revert Toolbars to default config visibility
            QString tbJsonStr = settings.value("custom_toolbars_v1").toString();
            if (!tbJsonStr.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(tbJsonStr.toUtf8());
                QJsonArray arr = doc.array();
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject tbObj = arr[i].toObject();
                    QString id = tbObj["id"].toString();
                    bool visible = tbObj["visible"].toBool(true);
                    for (QToolBar* tb : m_dynamicToolBars) {
                        if (tb->objectName() == id) {
                            tb->setVisible(visible);
                            break;
                        }
                    }
                }
            } else {
                for (QToolBar* tb : m_dynamicToolBars) {
                    tb->setVisible(true);
                }
            }

            // Revert Custom Menus to all visible
            for (QMenu* menu : m_customMenus) {
                if (menu->menuAction()) {
                    menu->menuAction()->setVisible(true);
                }
            }

            bool drivesToolbar = settings.value("drives/toolbar_visible", true).toBool();
            bool centerOps = settings.value("layout/center_ops_visible", true).toBool();
            bool console = settings.value("console/visible", true).toBool();
            bool favorites = settings.value("favorites/sidebar_visible", false).toBool();
            bool zen = settings.value("preferences/zen_mode", false).toBool();

            m_actToggleDrivesToolbar->setChecked(drivesToolbar);
            if (m_tbDrives) m_tbDrives->setVisible(drivesToolbar);
            
            m_actToggleCenterOps->setChecked(centerOps);
            if (m_tbCenterOps) m_tbCenterOps->setVisible(centerOps && m_isDualPane);
            
            m_actToggleConsole->setChecked(console);
            if (m_bottomTabWidget) m_bottomTabWidget->setVisible(console);
            
            m_actToggleFavoritesSidebar->setChecked(favorites);
            if (m_sidebarTabWidget) m_sidebarTabWidget->setVisible(favorites);
            
            setZenMode(zen);

            if (m_previewPanel) {
                bool prefVisualizer = settings.value("preview/show_spectrum_visualizer", true).toBool();
                m_previewPanel->setSpectrumVisualizerVisible(prefVisualizer);
            }
            emit builtinPlayerDoubleclickChanged(settings.value("preferences/builtin_player_doubleclick", false).toBool());
        }
    }

    // Execute Folder Entry script/macro if matched and active
    if (foundMatch && !matchedRule.entryCommand.isEmpty() && m_lastEntryCommandPath != path) {
        m_lastEntryCommandPath = path;

        QProcess* proc = new QProcess(this);
        proc->setWorkingDirectory(path);
        
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("AMIFILES_CURRENT_DIR", path);
        proc->setProcessEnvironment(env);
        
        proc->start("sh", {"-c", matchedRule.entryCommand});
        
        if (m_consolePanel) {
            m_consolePanel->appendSystem(QString("=== Auto Folder Entry Command Applied ==="));
            m_consolePanel->appendSystem(QString("Path: %1").arg(path));
            m_consolePanel->appendSystem(QString("Command: %1").arg(matchedRule.entryCommand));
        }
        
        connect(proc, &QProcess::finished, proc, &QObject::deleteLater);
    }
}

QString MainWindow::detectFolderCategory(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) return "";
    
    QFileInfoList list = dir.entryInfoList(QDir::Files);
    int musicCount = 0;
    int videoCount = 0;
    int imageCount = 0;
    int docCount = 0;
    
    QStringList musicExt = {"mp3", "wav", "flac", "ogg", "m4a"};
    QStringList videoExt = {"mp4", "avi", "mkv", "mov", "webm"};
    QStringList imageExt = {"jpg", "jpeg", "png", "webp", "bmp", "gif"};
    QStringList docExt   = {"txt", "pdf", "doc", "docx", "odt", "rtf", "html", "cpp", "h"};
    
    int limit = qMin(list.size(), 50);
    for (int i = 0; i < limit; ++i) {
        QString ext = list[i].suffix().toLower();
        if (musicExt.contains(ext)) musicCount++;
        else if (videoExt.contains(ext)) videoCount++;
        else if (imageExt.contains(ext)) imageCount++;
        else if (docExt.contains(ext)) docCount++;
    }
    
    int maxVal = qMax(qMax(musicCount, videoCount), qMax(imageCount, docCount));
    if (maxVal > 0) {
        if (maxVal == musicCount) return "Music";
        if (maxVal == videoCount) return "Videos";
        if (maxVal == imageCount) return "Images";
        if (maxVal == docCount) return "Documents";
    }
    return "";
}


void MainWindow::onConfigureFolderLayouts() {
    FolderLayoutDialog dlg(m_folderRules, m_customButtons, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_folderRules = dlg.rules();
        saveFolderRules();
        if (m_activePanel) {
            applyFolderRules(m_activePanel->currentPath());
        }
    }
}

void MainWindow::onApplyProfileToCurrentFolder(const QString& profileName) {
    if (!m_activePanel) return;
    QString currentPath = m_activePanel->currentPath();
    if (currentPath.isEmpty()) return;

    FolderLayoutRule* existingRule = nullptr;
    for (auto& r : m_folderRules) {
        if (r.ruleType == "Path" && QDir::cleanPath(r.value) == QDir::cleanPath(currentPath)) {
            existingRule = &r;
            break;
        }
    }

    if (existingRule) {
        existingRule->linkedProfile = profileName;
    } else {
        FolderLayoutRule r;
        r.name = QString("%1 (%2)").arg(QFileInfo(currentPath).fileName()).arg(profileName);
        if (r.name.isEmpty()) {
            r.name = QString("Path (%1)").arg(profileName);
        }
        r.ruleType = "Path";
        r.value = currentPath;
        r.autoApply = true;
        r.linkedProfile = profileName;
        m_folderRules.append(r);
    }

    saveFolderRules();
    applyFolderRules(currentPath);

    QMessageBox::information(this, "Profile Linked", 
        QString("Successfully linked folder '%1' to profile '%2'.\nIt will now inherit the '%2' layout settings.")
        .arg(QFileInfo(currentPath).fileName())
        .arg(profileName));
}

void MainWindow::onSaveFolderProfileForCurrentDir() {
    if (!m_activePanel) return;
    QString currentPath = m_activePanel->currentPath();
    if (currentPath.isEmpty()) {
        QMessageBox::warning(this, "Save Folder Profile", "Cannot save profile: Current directory path is empty!");
        return;
    }

    // Find if a rule for this path already exists
    int existingIndex = -1;
    for (int i = 0; i < m_folderRules.size(); ++i) {
        if (m_folderRules[i].ruleType == "Path" && m_folderRules[i].value == currentPath) {
            existingIndex = i;
            break;
        }
    }

    QString profileName;
    bool isUpdate = (existingIndex != -1);

    if (isUpdate) {
        profileName = m_folderRules[existingIndex].name;
        auto button = QMessageBox::question(this, "Save Folder Profile",
            QString("A profile named '%1' already exists for this directory:\n%2\n\nDo you want to update it with your current layout settings?").arg(profileName, currentPath),
            QMessageBox::Yes | QMessageBox::No);
        if (button != QMessageBox::Yes) {
            return;
        }
    } else {
        bool ok = false;
        QString defaultName = QFileInfo(currentPath).fileName();
        if (defaultName.isEmpty()) defaultName = "Root Folder";
        profileName = QInputDialog::getText(this, "Save Folder Profile",
            QString("Create new layout profile for directory:\n%1\n\nEnter profile name:").arg(currentPath),
            QLineEdit::Normal, defaultName, &ok).trimmed();
        if (!ok) return;
        if (profileName.isEmpty()) {
            profileName = defaultName;
        }
    }

    FolderLayoutRule r;
    r.name = profileName;
    r.ruleType = "Path";
    r.value = currentPath;
    r.autoApply = true;

    // 1. Capture View Mode
    int idx = m_activePanel->viewModeIndex();
    if (idx == 0) r.viewMode = "List";
    else if (idx == 1) r.viewMode = "Grid";
    else if (idx == 2) r.viewMode = "Card";
    else if (idx == 3) r.viewMode = "Miller";
    else if (idx == 4) r.viewMode = "Timeline";
    else if (idx == 5) r.viewMode = "Filmstrip";
    else if (idx == 6) r.viewMode = "Audio Showcase (Classic)";
    else if (idx == 7) r.viewMode = "Video Showcase (Classic)";
    else if (idx == 8) r.viewMode = "Movies Full Screen";
    else if (idx == 9) r.viewMode = "TV Shows Full Screen";
    else if (idx == 10) r.viewMode = "Music Full Screen";
    else r.viewMode = "No Change";

    // 2. Capture custom buttons filter list
    r.customButtons = m_activeToolbarFilter;

    // 3. Capture Visibility Overrides
    r.overrideDrivesToolbar = true;
    r.drivesToolbarVisible = (m_actToggleDrivesToolbar && m_actToggleDrivesToolbar->isChecked());

    r.overrideCenterOps = true;
    r.centerOpsVisible = (m_actToggleCenterOps && m_actToggleCenterOps->isChecked());

    r.overrideConsole = true;
    r.consoleVisible = (m_actToggleConsole && m_actToggleConsole->isChecked());

    r.overridePreview = true;
    r.previewVisible = (m_actTogglePreview && m_actTogglePreview->isChecked());

    r.overrideFavoritesSidebar = true;
    r.favoritesSidebarVisible = (m_actToggleFavoritesSidebar && m_actToggleFavoritesSidebar->isChecked());

    r.overrideZenMode = true;
    r.zenModeActive = m_zenMode;

    r.overrideBuiltinPlayerDoubleclick = true;
    r.builtinPlayerDoubleclick = isBuiltinPlayerDoubleclickActive();

    // 4. Capture Custom Background Color
    QString customBg = m_activePanel->customBgColor();
    if (!customBg.isEmpty()) {
        r.useBgColor = true;
        r.bgColor = customBg;
    } else {
        r.useBgColor = false;
        r.bgColor = "";
    }

    // 5. Capture Custom Toolbars visibility
    r.overrideToolbars = true;
    for (QToolBar* tb : m_dynamicToolBars) {
        if (tb && tb->isVisible()) {
            r.selectedToolbars.append(tb->objectName());
        }
    }

    // 6. Capture Custom Menus visibility
    r.overrideMenus = true;
    for (QMenu* menu : m_customMenus) {
        if (menu && menu->menuAction() && menu->menuAction()->isVisible()) {
            r.selectedMenus.append(menu->title());
        }
    }

    // 7. Capture Window/Toolbar states and positions
    r.windowState = saveState();

    // Save to list
    if (isUpdate) {
        m_folderRules[existingIndex] = r;
    } else {
        m_folderRules.append(r);
    }

    saveFolderRules();

    QMessageBox::information(this, "Save Folder Profile",
        QString("Folder layout profile '%1' has been saved successfully for folder:\n%2").arg(r.name, currentPath));
}

void MainWindow::onSaveDefaultProfile() {
    int defaultIndex = -1;
    for (int i = 0; i < m_folderRules.size(); ++i) {
        if (m_folderRules[i].name.toLower() == "default") {
            defaultIndex = i;
            break;
        }
    }

    auto button = QMessageBox::question(this, "Save as Default Layout Profile",
        "Do you want to save the current window layout (views, toolbars positions, sidebars, et cetera) as your default layout profile?\n\nThis will apply to all directories that do not have a custom profile.",
        QMessageBox::Yes | QMessageBox::No);
    if (button != QMessageBox::Yes) {
        return;
    }

    FolderLayoutRule r;
    r.name = "Default";
    r.ruleType = "Path";
    r.value = "";
    r.autoApply = true;

    // 1. Capture View Mode
    if (m_activePanel) {
        int idx = m_activePanel->viewModeIndex();
        if (idx == 0) r.viewMode = "List";
        else if (idx == 1) r.viewMode = "Grid";
        else if (idx == 2) r.viewMode = "Card";
        else if (idx == 3) r.viewMode = "Miller";
        else if (idx == 4) r.viewMode = "Timeline";
        else if (idx == 5) r.viewMode = "Filmstrip";
        else if (idx == 6) r.viewMode = "Audio Showcase (Classic)";
        else if (idx == 7) r.viewMode = "Video Showcase (Classic)";
        else if (idx == 8) r.viewMode = "Movies Full Screen";
        else if (idx == 9) r.viewMode = "TV Shows Full Screen";
        else if (idx == 10) r.viewMode = "Music Full Screen";
        else r.viewMode = "No Change";
        
        // 4. Capture Custom Background Color
        QString customBg = m_activePanel->customBgColor();
        if (!customBg.isEmpty()) {
            r.useBgColor = true;
            r.bgColor = customBg;
        } else {
            r.useBgColor = false;
            r.bgColor = "";
        }
    } else {
        r.viewMode = "List";
        r.useBgColor = false;
        r.bgColor = "";
    }

    // 2. Capture custom buttons filter list
    r.customButtons = m_activeToolbarFilter;

    // 3. Capture Visibility Overrides
    r.overrideDrivesToolbar = true;
    r.drivesToolbarVisible = (m_actToggleDrivesToolbar && m_actToggleDrivesToolbar->isChecked());

    r.overrideCenterOps = true;
    r.centerOpsVisible = (m_actToggleCenterOps && m_actToggleCenterOps->isChecked());

    r.overrideConsole = true;
    r.consoleVisible = (m_actToggleConsole && m_actToggleConsole->isChecked());

    r.overridePreview = true;
    r.previewVisible = (m_actTogglePreview && m_actTogglePreview->isChecked());

    r.overrideFavoritesSidebar = true;
    r.favoritesSidebarVisible = (m_actToggleFavoritesSidebar && m_actToggleFavoritesSidebar->isChecked());

    r.overrideZenMode = true;
    r.zenModeActive = m_zenMode;

    r.overrideBuiltinPlayerDoubleclick = true;
    r.builtinPlayerDoubleclick = isBuiltinPlayerDoubleclickActive();

    // 5. Capture Custom Toolbars visibility
    r.overrideToolbars = true;
    for (QToolBar* tb : m_dynamicToolBars) {
        if (tb && tb->isVisible()) {
            r.selectedToolbars.append(tb->objectName());
        }
    }

    // 6. Capture Custom Menus visibility
    r.overrideMenus = true;
    for (QMenu* menu : m_customMenus) {
        if (menu && menu->menuAction() && menu->menuAction()->isVisible()) {
            r.selectedMenus.append(menu->title());
        }
    }

    // 7. Capture Window/Toolbar states and positions
    r.windowState = saveState();

    // Save to list
    if (defaultIndex != -1) {
        m_folderRules[defaultIndex] = r;
    } else {
        m_folderRules.prepend(r);
    }

    saveFolderRules();

    QMessageBox::information(this, "Save Default Layout Profile",
        "The Default Layout Profile has been updated successfully with your current layout settings.");
}

void MainWindow::onLoadDefaultProfile() {
    FolderLayoutRule defaultRule;
    bool foundDefault = false;
    for (const auto& r : m_folderRules) {
        if (r.name.toLower() == "default") {
            defaultRule = r;
            foundDefault = true;
            break;
        }
    }

    if (!foundDefault) {
        QMessageBox::warning(this, "Load Default Layout Profile", "No Default layout profile found in the profiles list!");
        return;
    }

    applyProfile(defaultRule, m_activePanel);
    QMessageBox::information(this, "Load Default Layout Profile", "Loaded and applied your Default layout profile settings.");
}

void MainWindow::adjustSplitterSizes() {
    if (!m_splitter) return;

    int totalWidth = m_splitter->width();
    bool sidebarVisible = m_sidebarTabWidget && m_sidebarTabWidget->isVisible();

    int sidebarWidth = sidebarVisible ? 180 : 0;
    int mainWidth = qMax(100, totalWidth - sidebarWidth);

    m_splitter->setSizes({sidebarWidth, mainWidth});
}

#include "dynamicfavoritesdialog.h"

void MainWindow::onConfigureDynamicBookmarks() {
    DynamicFavoritesDialog dlg(this);
    dlg.exec();
}

void MainWindow::onExportCustomButtons() {
    QString fileName = QFileDialog::getSaveFileName(this, "Export Custom Buttons", QString(), "JSON Files (*.json)");
    if (fileName.isEmpty()) return;

    QJsonArray arr;
    for (const auto& btn : m_customButtons) {
        QJsonObject obj;
        obj["name"] = btn.name;
        obj["script"] = btn.script;
        obj["icon"] = btn.icon;
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        statusBar()->showMessage("Custom script buttons exported successfully.", 3000);
    } else {
        QMessageBox::critical(this, "Error", "Could not save custom script buttons file.");
    }
}

void MainWindow::onImportCustomButtons() {
    QString fileName = QFileDialog::getOpenFileName(this, "Import Custom Buttons", QString(), "JSON Files (*.json)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "Could not open selected buttons file.");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        QMessageBox::critical(this, "Error", "Selected file has an invalid format.");
        return;
    }

    QJsonArray arr = doc.array();
    QList<CustomButton> importedList;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QString name = obj["name"].toString();
        QString script = obj["script"].toString();
        QString icon = obj["icon"].toString();
        if (!name.isEmpty() && !script.isEmpty()) {
            importedList.append(CustomButton{name, script, icon});
        }
    }

    if (importedList.isEmpty()) {
        QMessageBox::warning(this, "Empty Import", "No valid custom script buttons found in file.");
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Import Options");
    msgBox.setText(QString("Found %1 custom script buttons to import. Select how you want to apply them:").arg(importedList.size()));
    QPushButton* btnOverwrite = msgBox.addButton("Overwrite Existing", QMessageBox::ActionRole);
    QPushButton* btnMerge = msgBox.addButton("Merge / Append", QMessageBox::ActionRole);
    QPushButton* btnCancel = msgBox.addButton("Cancel", QMessageBox::RejectRole);
    
    msgBox.exec();

    if (msgBox.clickedButton() == btnCancel) return;

    if (msgBox.clickedButton() == btnOverwrite) {
        m_customButtons = importedList;
    } else if (msgBox.clickedButton() == btnMerge) {
        for (const auto& btn : importedList) {
            bool duplicate = false;
            for (const auto& existing : m_customButtons) {
                if (existing.name == btn.name) {
                    duplicate = true;
                    break;
                }
              }
              if (!duplicate) {
                  m_customButtons.append(btn);
              }
          }
      }

      saveCustomButtons();
      rebuildCustomToolBar();
      statusBar()->showMessage("Custom script buttons imported successfully.", 3000);
}

#include "schedulerdialog.h"
#include "autotagdialog.h"

void MainWindow::onConfigureBackupSchedule() {
    SchedulerDialog dlg(this);
    dlg.exec();
}

void MainWindow::onConfigureAutoTags() {
    AutoTagDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (leftPanel()) leftPanel()->refresh();
        if (rightPanel()) rightPanel()->refresh();
    }
}

#include "commandpalette.h"

void MainWindow::onCommandPaletteAction() {
    CommandPalette dlg(this);
    connect(&dlg, &CommandPalette::commandTriggered, this, &MainWindow::onCommandPaletteTriggered);
    dlg.move(x() + (width() - dlg.width()) / 2, y() + (height() - dlg.height()) / 3);
    dlg.exec();
}

void MainWindow::onCommandPaletteTriggered(const QString& action) {
    if (action == "view_details") {
        if (m_activePanel) m_activePanel->onViewModeChanged(0);
    } else if (action == "view_icons") {
        if (m_activePanel) m_activePanel->onViewModeChanged(1);
    } else if (action == "view_cards") {
        if (m_activePanel) m_activePanel->onViewModeChanged(2);
    } else if (action == "view_miller") {
        if (m_activePanel) m_activePanel->onViewModeChanged(3);
    } else if (action == "view_timeline") {
        if (m_activePanel) m_activePanel->onViewModeChanged(4);
    } else if (action == "view_filmstrip") {
        if (m_activePanel) m_activePanel->onViewModeChanged(5);
    } else if (action == "toggle_dual") {
        m_actToggleDualPane->toggle();
    } else if (action == "toggle_sidebar") {
        m_actToggleFavoritesSidebar->toggle();
    } else if (action == "toggle_age") {
        m_actToggleAgeColoring->toggle();
    } else if (action == "tool_image") {
        onImageConvert();
    } else if (action == "tool_diff") {
        onCompareSyncAction();
    } else if (action == "tool_duplicates") {
        onDuplicateFinderAction();
    } else if (action == "tool_autotags") {
        onConfigureAutoTags();
    } else if (action == "tool_autoorganizer") {
        onConfigureAutoOrganizer();
    } else if (action == "tool_remotemountmanager") {
        onRemoteMountsManager();
    } else if (action == "nav_up") {
        if (m_activePanel) m_activePanel->onNavigateUp();
    } else if (action == "nav_back") {
        if (m_activePanel) m_activePanel->onNavigateBack();
    } else if (action == "nav_forward") {
        if (m_activePanel) m_activePanel->onNavigateForward();
    } else if (action == "help_open") {
        onShowHelpAction();
    } else if (action == "tool_folder_diff") {
        onCompareSyncAction();
    } else if (action == "tool_encrypt") {
        onEncryptVault();
    } else if (action == "open_disk_dashboard") {
        if (m_activePanel) m_activePanel->setPath("smart://disk_dashboard");
    }
}

void MainWindow::onConfigureAutoOrganizer() {
    AutoOrganizerDialog dlg(this);
    dlg.exec();
}

void MainWindow::onRemoteMountsManager() {
    RemoteMountManager dlg(this);
    dlg.exec();
    updateDrivesList();
}

void MainWindow::onPreferencesAction() {
    PreferencesDialog dlg(this);
    connect(&dlg, &PreferencesDialog::preferencesChanged, this, [this]() {
        updateWidgetStylesheets();
        QSettings settings("Amifiles", "Amifiles");
        
        bool horizontalSplit = settings.value("preferences/horizontal_split", false).toBool();
        if (m_actToggleHorizontalSplit) m_actToggleHorizontalSplit->setChecked(horizontalSplit);
        onToggleHorizontalSplit(horizontalSplit);
        
        bool toolbarVisible = settings.value("layout/drives_toolbar_visible", true).toBool();
        if (m_actToggleDrivesToolbar) m_actToggleDrivesToolbar->setChecked(toolbarVisible);
        if (m_tbDrives) m_tbDrives->setVisible(toolbarVisible);
        
        bool menuDrivesVisible = settings.value("layout/drives_menu_visible", true).toBool();
        if (m_menuDrives && m_menuDrives->menuAction()) {
            m_menuDrives->menuAction()->setVisible(menuDrivesVisible);
        }
        
        bool previewMuted = settings.value("preview/muted", false).toBool();
        if (m_actMutePreview) m_actMutePreview->setChecked(previewMuted);
        if (m_previewPanel) {
            m_previewPanel->setMuted(previewMuted);
            m_previewPanel->setSpectrumVisualizerVisible(settings.value("preview/show_spectrum_visualizer", true).toBool());
        }
        setBuiltinPlayerDoubleclickActive(settings.value("preferences/builtin_player_doubleclick", false).toBool());
        
        for (int i = 0; i < m_leftTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
            if (p) p->refresh();
        }
        for (int i = 0; i < m_rightTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
            if (p) p->refresh();
        }
    });
    dlg.exec();
}

void MainWindow::onMediaPreferences() {
    PreferencesDialog dlg(this);
    dlg.setCurrentPage(3); // Media page index
    connect(&dlg, &PreferencesDialog::preferencesChanged, this, [this]() {
        updateWidgetStylesheets();
        QSettings settings("Amifiles", "Amifiles");
        
        bool horizontalSplit = settings.value("preferences/horizontal_split", false).toBool();
        if (m_actToggleHorizontalSplit) m_actToggleHorizontalSplit->setChecked(horizontalSplit);
        onToggleHorizontalSplit(horizontalSplit);
        
        bool toolbarVisible = settings.value("layout/drives_toolbar_visible", true).toBool();
        if (m_actToggleDrivesToolbar) m_actToggleDrivesToolbar->setChecked(toolbarVisible);
        if (m_tbDrives) m_tbDrives->setVisible(toolbarVisible);
        
        bool menuDrivesVisible = settings.value("layout/drives_menu_visible", true).toBool();
        if (m_menuDrives && m_menuDrives->menuAction()) {
            m_menuDrives->menuAction()->setVisible(menuDrivesVisible);
        }
        
        bool previewMuted = settings.value("preview/muted", false).toBool();
        if (m_actMutePreview) m_actMutePreview->setChecked(previewMuted);
        if (m_previewPanel) {
            m_previewPanel->setMuted(previewMuted);
            m_previewPanel->setSpectrumVisualizerVisible(settings.value("preview/show_spectrum_visualizer", true).toBool());
        }
        setBuiltinPlayerDoubleclickActive(settings.value("preferences/builtin_player_doubleclick", false).toBool());
        
        for (int i = 0; i < m_leftTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
            if (p) p->refresh();
        }
        for (int i = 0; i < m_rightTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
            if (p) p->refresh();
        }
    });
    dlg.exec();
}

void MainWindow::onThemeStudioAction() {
    ThemeStudioDialog dlg(this);
    dlg.exec();
    updateWidgetStylesheets();
}

void MainWindow::onCreateSmartCollectionAction() {
    SmartCollectionDialog dlg(this);
    dlg.exec();
}

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QMenu>

QIcon MainWindow::getFolderIcon(const QString& folderName) {
    QString nameLower = folderName.toLower();
    QString themeName;
    if (nameLower.contains("game")) {
        themeName = "applications-games";
    } else if (nameLower.contains("code") || nameLower.contains("prog") || nameLower.contains("dev") || nameLower.contains("src")) {
        themeName = "applications-development";
    } else if (nameLower.contains("music") || nameLower.contains("song") || nameLower.contains("audio")) {
        themeName = "folder-music";
    } else if (nameLower.contains("video") || nameLower.contains("movie") || nameLower.contains("film")) {
        themeName = "folder-videos";
    } else if (nameLower.contains("picture") || nameLower.contains("photo") || nameLower.contains("image")) {
        themeName = "folder-pictures";
    } else if (nameLower.contains("doc") || nameLower.contains("paper") || nameLower.contains("text")) {
        themeName = "folder-documents";
    } else if (nameLower.contains("download")) {
        themeName = "folder-download";
    }

    QIcon icon = QIcon::fromTheme(themeName);
    if (icon.isNull()) {
        QStyle* style = QApplication::style();
        if (themeName == "applications-games" || themeName == "applications-development") {
            icon = style->standardIcon(QStyle::SP_ComputerIcon);
        } else {
            icon = style->standardIcon(QStyle::SP_DirIcon);
        }
    }
    return icon;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (QToolBar* tb = qobject_cast<QToolBar*>(watched)) {
        if (event->type() == QEvent::Move) {
            if (QApplication::mouseButtons() & Qt::LeftButton) {
                QToolTip::showText(QCursor::pos(), QString("Toolbar: %1").arg(tb->windowTitle()), tb);
            }
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            QToolTip::hideText();
        }
    }

    if (watched == m_menuFavorites) {
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::RightButton) {
                QAction* action = m_menuFavorites->actionAt(mouseEvent->pos());
                if (action) {
                    QString path = action->data().toString();
                    if (!path.isEmpty()) {
                        QMessageBox::StandardButton reply = QMessageBox::question(
                            this, "Remove Favorite",
                            QString("Are you sure you want to remove '%1' from Favorites?").arg(QDir::toNativeSeparators(path)),
                            QMessageBox::Yes | QMessageBox::No
                        );
                        if (reply == QMessageBox::Yes) {
                            FavoritesManager::instance().removeFavorite(path);
                            m_menuFavorites->close();
                        }
                        return true;
                    }
                }
            }
        }
    }

    if (watched == m_tbDrives) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent* dragMoveEvent = static_cast<QDragMoveEvent*>(event);
            dragMoveEvent->acceptProposedAction();
            return true;
        } else if (event->type() == QEvent::Drop) {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            QList<QUrl> urls = dropEvent->mimeData()->urls();
            bool addedAny = false;
            for (const QUrl& url : urls) {
                QString localPath = url.toLocalFile();
                if (!localPath.isEmpty() && QFileInfo(localPath).isDir()) {
                    QString name = QFileInfo(localPath).fileName();
                    if (name.isEmpty()) name = localPath;
                    
                    QSettings settings("Amifiles", "Amifiles");
                    settings.setValue("DrivesShortcuts/" + name, localPath);
                    addedAny = true;
                }
            }
            if (addedAny) {
                updateDrivesList();
            }
            dropEvent->acceptProposedAction();
            return true;
        }
    } else if (watched == m_customToolBar) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent* dragMoveEvent = static_cast<QDragMoveEvent*>(event);
            dragMoveEvent->acceptProposedAction();
            return true;
        } else if (event->type() == QEvent::Drop) {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            QList<QUrl> urls = dropEvent->mimeData()->urls();
            bool addedAny = false;
            for (const QUrl& url : urls) {
                QString localPath = url.toLocalFile();
                if (!localPath.isEmpty()) {
                    QString name = QFileInfo(localPath).fileName();
                    if (name.isEmpty()) name = localPath;
                    
                    QString script;
                    QString iconStr;
                    if (QFileInfo(localPath).isDir()) {
                        script = "@internal:Go " + localPath;
                        QString folderLower = name.toLower();
                        QString themeName = "folder";
                        if (folderLower.contains("game")) themeName = "applications-games";
                        else if (folderLower.contains("code") || folderLower.contains("prog") || folderLower.contains("dev") || folderLower.contains("src")) themeName = "applications-development";
                        else if (folderLower.contains("music") || folderLower.contains("song") || folderLower.contains("audio")) themeName = "folder-music";
                        else if (folderLower.contains("video") || folderLower.contains("movie") || folderLower.contains("film")) themeName = "folder-videos";
                        else if (folderLower.contains("picture") || folderLower.contains("photo") || folderLower.contains("image")) themeName = "folder-pictures";
                        else if (folderLower.contains("doc") || folderLower.contains("paper") || folderLower.contains("text")) themeName = "folder-documents";
                        else if (folderLower.contains("download")) themeName = "folder-download";
                        
                        iconStr = "theme:" + themeName;
                    } else {
                        script = localPath;
                        iconStr = "theme:system-run";
                    }
                    
                    m_customButtons.append(CustomButton{name, script, iconStr});
                    addedAny = true;
                }
            }
            if (addedAny) {
                saveCustomButtons();
                rebuildCustomToolBar();
            }
            dropEvent->acceptProposedAction();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onDrivesToolbarContextMenu(const QPoint& pos) {
    QAction* act = m_tbDrives->actionAt(pos);
    if (!act) return;

    QString label = act->data().toString();
    if (label.isEmpty()) return;

    QMenu menu(this);
    QAction* actRemove = menu.addAction(QIcon::fromTheme("edit-delete"), QString("Remove Shortcut '%1'").arg(label));
    QAction* selected = menu.exec(m_tbDrives->mapToGlobal(pos));
    if (selected == actRemove) {
        QSettings settings("Amifiles", "Amifiles");
        settings.remove("DrivesShortcuts/" + label);
        updateDrivesList();
    }
}

#include "custommenueditordialog.h"
#include <QWidgetAction>

// Helper function to build custom menu sub-items recursively
void MainWindow::buildMenuTree(QMenu* menu, const QJsonArray& itemsArray) {
    for (int i = 0; i < itemsArray.size(); ++i) {
        QJsonObject obj = itemsArray[i].toObject();
        QString type = obj["type"].toString();
        
        if (type == "separator") {
            menu->addSeparator();
        } else if (type == "menu") {
            QString title = obj["title"].toString();
            QMenu* sub = menu->addMenu(title);
            
            QString iconPath = obj["icon"].toString();
            if (!iconPath.isEmpty()) {
                QIcon icon;
                if (QFileInfo(iconPath).exists()) icon = QIcon(iconPath);
                else icon = QIcon::fromTheme(iconPath);
                if (!icon.isNull()) sub->setIcon(icon);
            }
            
            buildMenuTree(sub, obj["children"].toArray());
        } else if (type == "action") {
            QString title = obj["title"].toString();
            QString command = obj["command"].toString();
            QString iconPath = obj["icon"].toString();
            QString colorStr = obj["color"].toString();
            QString mode = obj["mode"].toString("Normal");
            
            QIcon icon;
            if (!iconPath.isEmpty()) {
                if (QFileInfo(iconPath).exists()) icon = QIcon(iconPath);
                else icon = QIcon::fromTheme(iconPath);
            }
            
            QWidgetAction* act = new QWidgetAction(menu);
            CustomMenuActionWidget* w = new CustomMenuActionWidget(icon, title, colorStr, mode, menu);
            act->setDefaultWidget(w);
            
            QObject::connect(w, &CustomMenuActionWidget::clicked, this, [this, act, command, menu]() {
                // Close parent menus recursively
                QMenu* p = menu;
                while (p) {
                    p->close();
                    p = qobject_cast<QMenu*>(p->parentWidget());
                }
                executeCustomCommand(command);
            });
            menu->addAction(act);
        }
    }
}

void MainWindow::rebuildCustomMenus() {
    for (QMenu* menu : m_customMenus) {
        menuBar()->removeAction(menu->menuAction());
        delete menu;
    }
    m_customMenus.clear();

    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_menus_v2").toString();
    QJsonArray arr;
    
    if (jsonStr.isEmpty()) {
        arr = getDefaultCustomMenus();
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        arr = doc.array();
    }

    QAction* helpAction = nullptr;
    if (m_menuHelp) helpAction = m_menuHelp->menuAction();

    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QString type = obj["type"].toString();
        if (type != "menu") continue;
        
        QString title = obj["title"].toString();
        QMenu* customMenu = new QMenu(title, this);
        customMenu->setStyleSheet(
            "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; }"
            "QMenu::item:selected { background-color: #313244; color: #f5c2e7; }"
        );

        buildMenuTree(customMenu, obj["children"].toArray());

        if (helpAction) {
            menuBar()->insertMenu(helpAction, customMenu);
        } else {
            menuBar()->addMenu(customMenu);
        }
        m_customMenus.append(customMenu);
    }
}

void MainWindow::onConfigureCustomMenus() {
    CustomMenuEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        rebuildCustomMenus();
    }
}

void MainWindow::executeCustomCommand(const QString& commandOrPath) {
    if (commandOrPath.isEmpty()) return;

    QString script = commandOrPath.trimmed();

    QString resolvedPath = script;
    if (resolvedPath.startsWith("~")) {
        resolvedPath = QDir::home().filePath(resolvedPath.mid(1));
    }
    if (QFileInfo(resolvedPath).isDir()) {
        if (m_activePanel) {
            m_activePanel->setPath(resolvedPath);
        }
        return;
    }

    if (script.startsWith("@internal:")) {
        QString cmd = script.mid(10).trimmed();
        if (cmd == "Copy") {
            onCopyToSiblingAction();
        } else if (cmd == "Move") {
            onMoveToSiblingAction();
        } else if (cmd == "Cut") {
            onCutAction();
        } else if (cmd == "Paste") {
            onPasteAction();
        } else if (cmd == "Delete") {
            onDeleteAction();
        } else if (cmd == "Rename") {
            onRenameAction();
        } else if (cmd == "NewFolder") {
            onNewFolderAction();
        } else if (cmd == "Refresh") {
            onRefreshAction();
        } else if (cmd == "ToggleDualPane") {
            if (m_actToggleDualPane) m_actToggleDualPane->trigger();
        } else if (cmd == "TogglePreview") {
            if (m_actTogglePreview) m_actTogglePreview->trigger();
        } else if (cmd == "ToggleFlatView") {
            if (m_actToggleFlatView) m_actToggleFlatView->trigger();
        } else if (cmd == "CompareSync") {
            onCompareSyncAction();
        } else if (cmd == "DuplicateFinder") {
            onDuplicateFinderAction();
        } else if (cmd == "SpaceAnalyzer") {
            onSpaceAnalyzerAction();
        } else if (cmd.startsWith("Go ") || cmd == "Go") {
            QString path = cmd.mid(3).trimmed();
            QString activeDir = m_activePanel ? m_activePanel->currentPath() : "";
            FilePanel* destPanel = (m_activePanel == leftPanel()) ? rightPanel() : leftPanel();
            QString destDir = destPanel ? destPanel->currentPath() : "";
            path.replace("{dest}", destDir);
            path.replace("{dir}", activeDir);
            if (m_activePanel) m_activePanel->setPath(path);
        } else {
            statusBar()->showMessage(QString("Unknown internal command: %1").arg(cmd), 4000);
        }
        return;
    }

    QString activeDir = m_activePanel ? m_activePanel->currentPath() : QDir::homePath();
    FilePanel* destPanel = (m_activePanel == leftPanel()) ? rightPanel() : leftPanel();
    QString destDir = destPanel ? destPanel->currentPath() : QDir::homePath();
    
    script.replace("{dir}", activeDir);
    script.replace("{dest}", destDir);
    
    QProcess::startDetached("/bin/bash", QStringList() << "-c" << script);
}

QJsonArray MainWindow::getDefaultCustomMenus() {
    QJsonArray root;
    
    QJsonObject m1;
    m1["type"] = "menu";
    m1["title"] = "🚀 Quick Launch";
    
    QJsonArray c1;
    
    QJsonObject a1;
    a1["type"] = "action";
    a1["title"] = "Home Directory";
    a1["command"] = QDir::homePath();
    a1["icon"] = "user-home";
    a1["color"] = "#89b4fa";
    a1["mode"] = "Normal";
    c1.append(a1);
    
    QJsonObject s1;
    s1["type"] = "separator";
    c1.append(s1);
    
    QJsonObject a2;
    a2["type"] = "action";
    a2["title"] = "System Monitor";
    a2["command"] = "gnome-system-monitor";
    a2["icon"] = "utilities-system-monitor";
    a2["color"] = "#a6e3a1";
    a2["mode"] = "Normal";
    c1.append(a2);

    QJsonObject subM;
    subM["type"] = "menu";
    subM["title"] = "Project Workspaces";
    QJsonArray subC;
    
    QJsonObject a3;
    a3["type"] = "action";
    a3["title"] = "Amifiles Workspace";
    a3["command"] = "/home/dave/cpp_projects/Amifiles";
    a3["icon"] = "folder-development";
    a3["color"] = "#cba6f7";
    a3["mode"] = "Normal";
    subC.append(a3);
    
    subM["children"] = subC;
    c1.append(subM);
    
    m1["children"] = c1;
    root.append(m1);
    
    return root;
}

#include "toolbareditordialog.h"

void MainWindow::rebuildToolBars() {
    // 1. Delete all existing dynamic toolbars
    for (QToolBar* tb : m_dynamicToolBars) {
        removeToolBar(tb);
        delete tb;
    }
    m_dynamicToolBars.clear();

    m_tbDrives = nullptr;
    m_customToolBar = nullptr;

    // 2. Load JSON structure from settings
    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_toolbars_v1").toString();
    QJsonArray arr;

    if (jsonStr.isEmpty()) {
        arr = ToolbarEditorDialog::getDefaultToolbarsJson();
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        arr = doc.array();
    }

    // Helper lambda to find internal actions by ID
    auto findInternalAction = [this](const QString& actId) -> QAction* {
        if (actId == "Copy") return m_actCopy;
        if (actId == "Cut") return m_actCut;
        if (actId == "Paste") return m_actPaste;
        if (actId == "Delete") return m_actDelete;
        if (actId == "Rename") return m_actRename;
        if (actId == "NewFolder") return m_actNewFolder;
        if (actId == "Refresh") return m_actRefresh;
        if (actId == "ToggleDualPane") return m_actToggleDualPane;
        if (actId == "TogglePreview") return m_actTogglePreview;
        if (actId == "ToggleFlatView") return m_actToggleFlatView;
        if (actId == "CompareSync") return m_actCompareSync;
        if (actId == "DuplicateFinder") return m_actDuplicateFinder;
        if (actId == "SpaceAnalyzer") return m_actSpaceAnalyzer;
        return nullptr;
    };

    // 3. Rebuild toolbars
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject tbObj = arr[i].toObject();
        QString name = tbObj["name"].toString();
        QString id = tbObj["id"].toString();
        QString styleStr = tbObj["style"].toString("TextBesideIcon");
        bool visible = tbObj["visible"].toBool(true);

        QToolBar* tb = new QToolBar(name, this);
        tb->setObjectName(id);
        tb->setMovable(true);
        tb->setToolTip(QString("%1 (Drag to reposition)").arg(name));

        // Apply toolbar button style style
        if (styleStr == "IconOnly") tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
        else if (styleStr == "TextOnly") tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
        else if (styleStr == "TextBesideIcon") tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        else if (styleStr == "TextUnderIcon") tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        QJsonArray itemsArr = tbObj["items"].toArray();
        for (int j = 0; j < itemsArr.size(); ++j) {
            QJsonObject itemObj = itemsArr[j].toObject();
            QString type = itemObj["type"].toString();
            QString itemName = itemObj["name"].toString();
            QString itemIcon = itemObj["icon"].toString();

            if (type == "separator") {
                tb->addSeparator();
            } else if (type == "internal") {
                QString actId = itemObj["id"].toString();
                QAction* internalAct = findInternalAction(actId);
                if (internalAct) {
                    QAction* customAct = new QAction(tb);
                    customAct->setText(itemName.isEmpty() ? internalAct->text() : itemName);
                    
                    QIcon qicon;
                    if (!itemIcon.isEmpty()) {
                        if (itemIcon.startsWith("theme:")) qicon = QIcon::fromTheme(itemIcon.mid(6));
                        else if (QFileInfo(itemIcon).exists()) qicon = QIcon(itemIcon);
                        else qicon = QIcon::fromTheme(itemIcon);
                    } else {
                        qicon = internalAct->icon();
                    }
                    customAct->setIcon(qicon);
                    connect(customAct, &QAction::triggered, internalAct, &QAction::trigger);
                    tb->addAction(customAct);
                }
            } else if (type == "custom") {
                QString command = itemObj["command"].toString();
                QAction* customAct = new QAction(tb);
                customAct->setText(itemName);

                QIcon qicon;
                if (!itemIcon.isEmpty()) {
                    if (itemIcon.startsWith("theme:")) qicon = QIcon::fromTheme(itemIcon.mid(6));
                    else if (QFileInfo(itemIcon).exists()) qicon = QIcon(itemIcon);
                    else qicon = QIcon::fromTheme(itemIcon);
                } else {
                    qicon = QIcon::fromTheme("system-run");
                }
                customAct->setIcon(qicon);
                connect(customAct, &QAction::triggered, this, [this, command]() {
                    executeCustomCommand(command);
                });
                tb->addAction(customAct);
            }
        }

        // Install drag and drop hooks
        if (id == "tb_drives" || id == "drivesToolBar") {
            m_tbDrives = tb;
            tb->setAcceptDrops(true);
            tb->installEventFilter(this);
            tb->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(tb, &QToolBar::customContextMenuRequested, this, &MainWindow::onDrivesToolbarContextMenu);
        } else if (id == "tb_custom_ops" || id.startsWith("tb_custom") || id == "customToolBar") {
            // Keep a pointer to allow drops on custom script commands toolbar
            m_customToolBar = tb;
            tb->setAcceptDrops(true);
            tb->installEventFilter(this);
            tb->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(tb, &QToolBar::customContextMenuRequested, this, &MainWindow::onCustomToolBarContextMenu);
        }

        tb->setVisible(visible);
        addToolBar(Qt::TopToolBarArea, tb);
        tb->installEventFilter(this);
        m_dynamicToolBars.append(tb);
    }

    // 4. Restore geometry/dock settings
    restoreState(settings.value("window/state").toByteArray());

    // 5. Force JSON configured visibility (as restoreState can override it)
    for (QToolBar* tb : m_dynamicToolBars) {
        QString id = tb->objectName();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject tbObj = arr[i].toObject();
            if (tbObj["id"].toString() == id) {
                tb->setVisible(tbObj["visible"].toBool(true));
                break;
            }
        }
    }

    // Force fullscreen queue dock visibility to match the active panel's view mode
    if (m_fullscreenQueueDock) {
        bool isFullscreenMode = false;
        if (m_activePanel) {
            int vm = m_activePanel->viewModeIndex();
            isFullscreenMode = (vm == 8 || vm == 9 || vm == 10);
        }
        m_fullscreenQueueDock->setVisible(isFullscreenMode);
    }
}

void MainWindow::onConfigureToolbars() {
    ToolbarEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        rebuildToolBars();
    }
}

void MainWindow::onToggleCenterOps(bool checked) {
    if (m_tbCenterOps) {
        m_tbCenterOps->setVisible(checked && m_isDualPane);
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("layout/center_ops_visible", checked);
}

void MainWindow::onPreviewDockContextMenu(const QPoint& pos) {
    if (!m_previewDock) return;
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
    );

    QAction* actDockRight = menu.addAction("Dock to Right Side");
    QAction* actDockLeft = menu.addAction("Dock to Left Side");
    QAction* actFloat = menu.addAction("Float Window");
    QAction* actClose = menu.addAction("Hide Preview Panel");

    actFloat->setEnabled(!m_previewDock->isFloating());
    actDockRight->setEnabled(m_previewDock->isFloating() || dockWidgetArea(m_previewDock) != Qt::RightDockWidgetArea);
    actDockLeft->setEnabled(m_previewDock->isFloating() || dockWidgetArea(m_previewDock) != Qt::LeftDockWidgetArea);

    QAction* selected = menu.exec(m_previewDock->mapToGlobal(pos));
    if (selected == actDockRight) {
        m_previewDock->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, m_previewDock);
        m_previewDock->show();
    } else if (selected == actDockLeft) {
        m_previewDock->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, m_previewDock);
        m_previewDock->show();
    } else if (selected == actFloat) {
        m_previewDock->setFloating(true);
    } else if (selected == actClose) {
        m_previewDock->setVisible(false);
    }
}

void MainWindow::onClonePathRequested(const QString& path) {
    FilePanel* senderPanel = qobject_cast<FilePanel*>(sender());
    if (!senderPanel) return;

    FilePanel* targetPanel = nullptr;
    if (leftPanel() == senderPanel) {
        targetPanel = rightPanel();
    } else if (rightPanel() == senderPanel) {
        targetPanel = leftPanel();
    }

    if (targetPanel) {
        targetPanel->setPath(path);
    }
}

void MainWindow::onTabPressed() {
    FilePanel* targetPanel = nullptr;
    if (m_activePanel == leftPanel()) {
        targetPanel = rightPanel();
    } else {
        targetPanel = leftPanel();
    }

    if (targetPanel) {
        targetPanel->focusActiveView();
        onPanelActivated(targetPanel);
    }
}

void MainWindow::onAutoSizeColumns() {
    FilePanel* left = leftPanel();
    FilePanel* right = rightPanel();
    if (left) left->autoSizeAllColumns();
    if (right) right->autoSizeAllColumns();
}

void MainWindow::onToggleSyncScroll(bool checked) {
    m_syncScrollEnabled = checked;
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/sync_scroll", checked);
    updateScrollSyncConnections();
}

void MainWindow::updateScrollSyncConnections() {
    if (m_leftScrollConnected && m_rightScrollConnected) {
        disconnect(m_leftScrollConnected, &QScrollBar::valueChanged, m_rightScrollConnected, &QScrollBar::setValue);
        disconnect(m_rightScrollConnected, &QScrollBar::valueChanged, m_leftScrollConnected, &QScrollBar::setValue);
        m_leftScrollConnected = nullptr;
        m_rightScrollConnected = nullptr;
    }

    if (!m_syncScrollEnabled) return;

    FilePanel* left = leftPanel();
    FilePanel* right = rightPanel();
    if (!left || !right) return;

    QScrollBar* leftScroll = left->activeVerticalScrollBar();
    QScrollBar* rightScroll = right->activeVerticalScrollBar();

    if (leftScroll && rightScroll) {
        m_leftScrollConnected = leftScroll;
        m_rightScrollConnected = rightScroll;

        rightScroll->setValue(leftScroll->value());

        connect(leftScroll, &QScrollBar::valueChanged, rightScroll, &QScrollBar::setValue);
        connect(rightScroll, &QScrollBar::valueChanged, leftScroll, &QScrollBar::setValue);
    }
}

struct TabWidgetEx : public QTabWidget {
    using QTabWidget::tabBar;
};

void MainWindow::onTabContextMenuRequested(const QPoint& pos) {
    QTabWidget* tabWidget = qobject_cast<QTabWidget*>(sender());
    if (!tabWidget) return;

    int tabIndex = static_cast<TabWidgetEx*>(tabWidget)->tabBar()->tabAt(pos);

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
    );

    QAction* actPin = nullptr;
    QAction* actLockPath = nullptr;
    QAction* actLockSubdirs = nullptr;
    FilePanel* panel = nullptr;

    if (tabIndex != -1) {
        panel = qobject_cast<FilePanel*>(tabWidget->widget(tabIndex));
        if (panel) {
            actPin = menu.addAction("Pin Tab");
            actPin->setCheckable(true);
            actPin->setChecked(panel->isPinned());

            actLockPath = menu.addAction("Lock Path");
            actLockPath->setCheckable(true);
            actLockPath->setChecked(panel->isPathLocked());

            actLockSubdirs = menu.addAction("Lock Path with Subdirs");
            actLockSubdirs->setCheckable(true);
            actLockSubdirs->setChecked(panel->isPathLockedWithSubdirs());
            
            menu.addSeparator();
        }
    }

    QAction* actConfigure = menu.addAction("Folder Profiles & Layouts...");
    QAction* actSaveFolder = menu.addAction("Save Current Layout as Folder Profile...");

    QMenu* menuApplyProfile = menu.addMenu("Apply Profile Layout to Current Folder");
    for (const auto& r : m_folderRules) {
        if (!r.name.isEmpty()) {
            QString profileName = r.name;
            QAction* act = menuApplyProfile->addAction(profileName);
            connect(act, &QAction::triggered, this, [this, profileName]() {
                onApplyProfileToCurrentFolder(profileName);
            });
        }
    }

    QAction* actSaveDefault = menu.addAction("Save Current Layout as Default Profile");
    QAction* actLoadDefault = menu.addAction("Load Default Profile");

    QAction* selected = menu.exec(tabWidget->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actConfigure) {
        onConfigureFolderLayouts();
    } else if (selected == actSaveFolder) {
        onSaveFolderProfileForCurrentDir();
    } else if (selected == actSaveDefault) {
        onSaveDefaultProfile();
    } else if (selected == actLoadDefault) {
        onLoadDefaultProfile();
    } else if (panel) {
        if (selected == actPin) {
            panel->setPinned(actPin->isChecked());
            QString name = QFileInfo(panel->currentPath()).fileName();
            if (name.isEmpty()) name = panel->currentPath();
            if (panel->isPinned()) name = "📌 " + name;
            if (panel->isPathLocked()) name += " 🔒";
            else if (panel->isPathLockedWithSubdirs()) name += " 📂🔒";
            tabWidget->setTabText(tabIndex, name);
        } else if (selected == actLockPath) {
            bool lock = actLockPath->isChecked();
            panel->setPathLocked(lock);
            if (lock) {
                panel->setPathLockedWithSubdirs(false);
            }
            QString name = QFileInfo(panel->currentPath()).fileName();
            if (name.isEmpty()) name = panel->currentPath();
            if (panel->isPinned()) name = "📌 " + name;
            if (panel->isPathLocked()) name += " 🔒";
            else if (panel->isPathLockedWithSubdirs()) name += " 📂🔒";
            tabWidget->setTabText(tabIndex, name);
        } else if (selected == actLockSubdirs) {
            bool lock = actLockSubdirs->isChecked();
            panel->setPathLockedWithSubdirs(lock);
            if (lock) {
                panel->setPathLocked(false);
            }
            QString name = QFileInfo(panel->currentPath()).fileName();
            if (name.isEmpty()) name = panel->currentPath();
            if (panel->isPinned()) name = "📌 " + name;
            if (panel->isPathLocked()) name += " 🔒";
            else if (panel->isPathLockedWithSubdirs()) name += " 📂🔒";
            tabWidget->setTabText(tabIndex, name);
        }
    }
}

void MainWindow::updateWidgetStylesheets() {
    QSettings settings("Amifiles", "Amifiles");
    QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();

    if (preset == "System Theme") {
        qApp->setStyleSheet("");
        if (m_sidebarTabWidget) m_sidebarTabWidget->setStyleSheet("");
        if (m_favoritesSidebar) m_favoritesSidebar->setStyleSheet("");
        if (m_filtersSidebar) m_filtersSidebar->setStyleSheet("");
        if (m_tagsSidebar) m_tagsSidebar->setStyleSheet("");
        if (m_leftTabWidget) m_leftTabWidget->setStyleSheet("");
        if (m_rightTabWidget) m_rightTabWidget->setStyleSheet("");
        if (m_bottomTabWidget) m_bottomTabWidget->setStyleSheet("");
        if (m_tbCenterOps) m_tbCenterOps->setStyleSheet("");
        if (m_tbCenterOpsSeparator) m_tbCenterOpsSeparator->setStyleSheet("");
        if (m_tbDrives) m_tbDrives->setStyleSheet("");
    } else {
        qApp->setStyleSheet(Theme::getStylesheet());
        
        if (m_sidebarTabWidget) {
            m_sidebarTabWidget->setStyleSheet(
                "QTabWidget::pane { border: 1px solid #313244; background-color: #1e1e2e; border-radius: 6px; }"
                "QTabBar::tab { background-color: #181825; color: #a6adc8; padding: 6px 10px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
                "QTabBar::tab:selected { background-color: #313244; color: #cdd6f4; }"
            );
        }
        
        if (m_favoritesSidebar) {
            m_favoritesSidebar->setStyleSheet(
                "QListWidget { background-color: #181825; color: #cdd6f4; border: none; padding: 4px; }"
                "QListWidget::item { padding: 6px 8px; border-radius: 4px; color: #cdd6f4; }"
                "QListWidget::item:hover { background-color: #313244; color: #f5c2e7; }"
                "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
            );
        }
        if (m_filtersSidebar && m_favoritesSidebar) m_filtersSidebar->setStyleSheet(m_favoritesSidebar->styleSheet());
        if (m_tagsSidebar && m_favoritesSidebar) m_tagsSidebar->setStyleSheet(m_favoritesSidebar->styleSheet());
        
        if (m_leftTabWidget) {
            m_leftTabWidget->setStyleSheet(
                "QTabWidget::pane { border: 1px solid #313244; background-color: #1e1e2e; }"
                "QTabBar::tab { background-color: #181825; color: #a6adc8; border: 1px solid #313244; border-bottom: none; padding: 6px 12px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
                "QTabBar::tab:selected { background-color: #1e1e2e; color: #cdd6f4; border-color: #313244; }"
                "QTabBar::tab:hover { background-color: #313244; color: #cdd6f4; }"
            );
        }
        if (m_rightTabWidget && m_leftTabWidget) m_rightTabWidget->setStyleSheet(m_leftTabWidget->styleSheet());
        if (m_bottomTabWidget && m_leftTabWidget) m_bottomTabWidget->setStyleSheet(m_leftTabWidget->styleSheet());
        
        if (m_tbCenterOps) m_tbCenterOps->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 4px;");
        if (m_tbCenterOpsSeparator) m_tbCenterOpsSeparator->setStyleSheet("background-color: #45475a; max-height: 1px;");
        if (m_tbDrives) {
            m_tbDrives->setStyleSheet("QToolBar { background-color: #11111b; border-bottom: 1px solid #313244; }");
        }
    }

    // Update style on all active file panels
    if (m_leftTabWidget) {
        for (int i = 0; i < m_leftTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
            if (p) p->updateStyles();
        }
    }
    if (m_rightTabWidget) {
        for (int i = 0; i < m_rightTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
            if (p) p->updateStyles();
        }
    }
    updateThemeMusic();
}

void MainWindow::updateTooltips() {
    QSettings settings("Amifiles", "Amifiles");
    bool detailed = settings.value("help/detailed_tooltips", true).toBool();

    if (detailed) {
        m_actShowHelp->setToolTip("Open the interactive Help Guide (F1) containing standard/advanced guides, search tips, custom scripting parameters, and database sync manuals.");
        m_actToggleHorizontalSplit->setToolTip("Toggle layout split: checked splits panels horizontally; unchecked splits them vertically.");
        m_actToggleDualPane->setToolTip("Toggle single panel or dual-pane directory side-by-side mode. Dual pane enables fast comparison, drag & drop, and copy/move operations.");
        m_actToggleFavoritesSidebar->setToolTip("Toggle the bookmarks, filters, and tags sidebar widget. Useful for instant navigation, file category isolation, and custom tag matching.");
        m_actToggleConsole->setToolTip("Toggle the bottom panel containing the integrated interactive Terminal tab and custom command execution output shells.");
        m_actToggleCasingOverlays->setToolTip("Toggle 3D visual sleeves (CD jewel cases or DVD covers) on folders containing cover.jpg or poster.jpg artwork.");
        if (m_actCompareSync) m_actCompareSync->setToolTip("Compare active and destination panel folders by date/size and synchronize files between them bidirectionally or unidirectionally.");
        if (m_actDuplicateFinder) m_actDuplicateFinder->setToolTip("Find duplicate files recursively in any directory by matching file name, size, or cryptographic MD5/SHA256 checksum hashes.");
        if (m_actSpaceAnalyzer) m_actSpaceAnalyzer->setToolTip("Scan folder trees recursively and display disk usage statistics using a dynamic visual sunburst chart or hierarchical bar graphs.");
    } else {
        m_actShowHelp->setToolTip("Open user manual (F1)");
        m_actToggleHorizontalSplit->setToolTip("Toggle horizontal layout split");
        m_actToggleDualPane->setToolTip("Toggle Dual-Pane view");
        m_actToggleFavoritesSidebar->setToolTip("Toggle Bookmarks sidebar");
        m_actToggleConsole->setToolTip("Toggle Bottom console");
        m_actToggleCasingOverlays->setToolTip("Toggle CD/DVD cover casings");
        if (m_actCompareSync) m_actCompareSync->setToolTip("Folder Sync comparison");
        if (m_actDuplicateFinder) m_actDuplicateFinder->setToolTip("Duplicate Finder");
        if (m_actSpaceAnalyzer) m_actSpaceAnalyzer->setToolTip("Visual Folder Space Analyzer");
    }
}

void MainWindow::onToggleDetailedTooltips(bool enabled) {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("help/detailed_tooltips", enabled);
    updateTooltips();
}

void MainWindow::setZenMode(bool enabled) {
    m_zenMode = enabled;
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("preferences/zen_mode", enabled);

    // Hide/show main window menu bar, toolbars, center ops, bottom tabs, status bar
    if (menuBar()) menuBar()->setVisible(!enabled);
    if (m_tbFile) m_tbFile->setVisible(!enabled);
    if (m_tbView) m_tbView->setVisible(!enabled);
    if (m_customToolBar) m_customToolBar->setVisible(!enabled);
    if (m_tbDrives) m_tbDrives->setVisible(!enabled && m_actToggleDrivesToolbar->isChecked());
    if (m_tbCenterOps) m_tbCenterOps->setVisible(!enabled && m_actToggleCenterOps->isChecked() && m_isDualPane);
    if (m_bottomTabWidget) m_bottomTabWidget->setVisible(!enabled && m_actToggleConsole->isChecked());
    if (statusBar()) statusBar()->setVisible(!enabled);
    for (QToolBar* tb : m_dynamicToolBars) {
        if (tb) {
            bool shouldBeVisible = false;
            if (m_hasActiveFolderRule && m_activeFolderRule.overrideToolbars) {
                shouldBeVisible = m_activeFolderRule.selectedToolbars.contains(tb->objectName());
            } else {
                shouldBeVisible = isToolbarDefaultVisible(tb->objectName());
            }
            tb->setVisible(!enabled && shouldBeVisible);
        }
    }
    
    // Hide/show navigation and filter bars in all open tabs on both left and right sides
    if (m_leftTabWidget) {
        for (int i = 0; i < m_leftTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
            if (p) p->setNavigationAndFilterVisible(!enabled);
        }
    }
    if (m_rightTabWidget) {
        for (int i = 0; i < m_rightTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
            if (p) p->setNavigationAndFilterVisible(!enabled);
        }
    }
    if (m_previewPanel) {
        m_previewPanel->setZenMode(enabled);
    }
    updateThemeMusic();
}

void MainWindow::onPlayMediaBuiltin(const QStringList& filePaths) {
    if (!m_previewPanel || filePaths.isEmpty()) return;

    // Show preview panel if it is hidden
    if (m_actTogglePreview && !m_actTogglePreview->isChecked()) {
        m_actTogglePreview->setChecked(true);
    }

    if (filePaths.size() == 1) {
        m_previewPanel->previewFile(filePaths.first(), {});
    } else {
        m_previewPanel->playPlaylist(filePaths);
    }
    
    // Start playback
    if (m_previewPanel->player()) {
        m_previewPanel->player()->play();
    }
    
    // Go fullscreen if preferred
    bool autoFullscreen = true;
    if (m_hasActiveFolderRule && m_activeFolderRule.overrideFullScreenPlayer) {
        autoFullscreen = m_activeFolderRule.fullScreenPlayerActive;
    } else {
        QSettings settings("Amifiles", "Amifiles");
        autoFullscreen = settings.value("preview/auto_fullscreen", true).toBool();
        if (isBuiltinPlayerDoubleclickActive()) {
            autoFullscreen = true;
        }
    }

    if (m_previewPanel) {
        bool currentlyFullscreen = m_previewPanel->isFullscreen();
        if (autoFullscreen && !currentlyFullscreen) {
            m_previewPanel->toggleFullscreen();
        } else if (!autoFullscreen && currentlyFullscreen) {
            m_previewPanel->toggleFullscreen();
        }
    }
}

void MainWindow::onPlayMediaFullscreen(const QStringList& filePaths) {
    if (!m_previewPanel || filePaths.isEmpty()) return;
    m_previewPanel->clearPreview();
    m_previewPanel->playPlaylist(filePaths);
    if (m_previewPanel->player()) {
        m_previewPanel->player()->play();
    }
    if (!m_previewPanel->isFullscreen()) {
        m_previewPanel->toggleFullscreen();
    }
}

void MainWindow::onActivePanelViewModeChanged() {
    if (!m_activePanel) return;
    int vm = m_activePanel->viewModeIndex();
    bool isFullscreenMode = (vm == 8 || vm == 9 || vm == 10);
    
    if (isFullscreenMode) {
        if (m_actTogglePreview && m_actTogglePreview->isChecked()) {
            m_actTogglePreview->setChecked(false);
        }
        if (m_fullscreenQueueDock && !m_fullscreenQueueDock->isVisible()) {
            m_fullscreenQueueDock->setVisible(true);
            syncFullscreenQueue();
        }
    } else {
        if (m_fullscreenQueueDock && m_fullscreenQueueDock->isVisible()) {
            m_fullscreenQueueDock->setVisible(false);
        }
    }
}

void MainWindow::syncFullscreenQueue() {
    if (!m_fullscreenQueueList || !m_previewPanel) return;
    m_fullscreenQueueList->clear();
    QStringList playlist = m_previewPanel->playlist();
    int activeIdx = m_previewPanel->playlistIndex();
    
    for (int i = 0; i < playlist.size(); ++i) {
        QFileInfo fi(playlist.at(i));
        QString text = QString("%1. %2").arg(i + 1).arg(fi.fileName());
        QListWidgetItem* item = new QListWidgetItem(text, m_fullscreenQueueList);
        if (i == activeIdx) {
            item->setSelected(true);
            item->setText("▶ " + fi.fileName());
            item->setFont(QFont("Outfit", 13, QFont::Bold));
        }
    }
}

void MainWindow::onQueueMediaBuiltin(const QStringList& filePaths) {
    if (!m_previewPanel || filePaths.isEmpty()) return;

    bool isFullscreenMode = false;
    if (m_activePanel) {
        int vm = m_activePanel->viewModeIndex();
        isFullscreenMode = (vm == 8 || vm == 9 || vm == 10);
    }

    if (!isFullscreenMode) {
        if (m_actTogglePreview && !m_actTogglePreview->isChecked()) {
            m_actTogglePreview->setChecked(true);
            onTogglePreview(true);
        }
    }

    m_previewPanel->addToPlaylist(filePaths);
}

void MainWindow::onBackupSettings() {
    QSettings settings("Amifiles", "Amifiles");
    QString srcFile = settings.fileName();
    if (srcFile.isEmpty() || !QFile::exists(srcFile)) {
        QMessageBox::warning(this, "Backup Failed", "No settings file found to back up yet.");
        return;
    }
    
    QString destFile = QFileDialog::getSaveFileName(this, "Backup Settings", QDir::homePath() + "/Amifiles_backup.conf", "Configuration Files (*.conf);;All Files (*)");
    if (destFile.isEmpty()) return;
    
    if (QFile::exists(destFile)) {
        QFile::remove(destFile);
    }
    
    if (QFile::copy(srcFile, destFile)) {
        QMessageBox::information(this, "Backup Successful", QString("Settings successfully backed up to:\n%1").arg(destFile));
    } else {
        QMessageBox::critical(this, "Backup Failed", "Failed to copy settings file. Please check file permissions.");
    }
}

void MainWindow::onRestoreSettings() {
    QString srcFile = QFileDialog::getOpenFileName(this, "Restore Settings", QDir::homePath(), "Configuration Files (*.conf);;All Files (*)");
    if (srcFile.isEmpty()) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Restore", 
        "Restoring settings will overwrite your current configuration. The application will close to apply changes.\n\nAre you sure you want to proceed?",
        QMessageBox::Yes | QMessageBox::No
    );
    if (reply != QMessageBox::Yes) return;
    
    QSettings settings("Amifiles", "Amifiles");
    QString destFile = settings.fileName();
    
    settings.sync();
    
    if (QFile::exists(destFile)) {
        if (!QFile::remove(destFile)) {
            QMessageBox::critical(this, "Restore Failed", "Could not remove the existing configuration file. Please check file permissions.");
            return;
        }
    }
    
    if (QFile::copy(srcFile, destFile)) {
        QMessageBox::information(this, "Restore Successful", "Settings successfully restored. The application will now close. Please restart it to apply the changes.");
        QApplication::quit();
    } else {
        QMessageBox::critical(this, "Restore Failed", "Failed to restore settings file. Please check file permissions.");
    }
}

void MainWindow::registerDecryptedVault(const QString& decryptedPath, const QString& vaultPath, const QString& password) {
    for (const auto& v : m_activeVaults) {
        if (v.decryptedPath == decryptedPath) return;
    }

    DecryptedVault dv;
    dv.decryptedPath = decryptedPath;
    dv.vaultPath = vaultPath;
    dv.password = password;
    dv.lastActivity = QDateTime::currentDateTime();
    m_activeVaults.append(dv);

    if (!m_vaultIdleTimer) {
        m_vaultIdleTimer = new QTimer(this);
        connect(m_vaultIdleTimer, &QTimer::timeout, this, &MainWindow::checkVaultIdleTimeout);
        m_vaultIdleTimer->start(5000);
    }
    
    m_lastUserActivity = QDateTime::currentDateTime();
}

void MainWindow::resetVaultIdleTime() {
    m_lastUserActivity = QDateTime::currentDateTime();
    for (auto& v : m_activeVaults) {
        v.lastActivity = m_lastUserActivity;
    }
}

void MainWindow::checkVaultIdleTimeout() {
    if (m_activeVaults.isEmpty()) return;

    QSettings settings("Amifiles", "Amifiles");
    int timeoutSecs = settings.value("vault/auto_lock_timeout", 300).toInt();

    QDateTime now = QDateTime::currentDateTime();
    int elapsed = m_lastUserActivity.secsTo(now);

    if (elapsed >= timeoutSecs) {
        for (int i = m_activeVaults.size() - 1; i >= 0; --i) {
            lockVault(i);
        }
        m_activeVaults.clear();
        
        statusBar()->showMessage("Active secure vaults auto-locked due to inactivity.", 5000);
        if (m_activePanel) {
            m_activePanel->refresh();
        }
    }
}

void MainWindow::lockVault(int index) {
    if (index < 0 || index >= m_activeVaults.size()) return;
    DecryptedVault v = m_activeVaults[index];

    if (!QFile::exists(v.decryptedPath)) return;

    QFileInfo info(v.decryptedPath);
    QString parentDir = info.absolutePath();
    QString baseName = info.fileName();
    QString tempTar = parentDir + QString("/.vault_temp_%1.tar").arg(QRandomGenerator::global()->generate());

    QProcess tarProc;
    tarProc.start("tar", {"-cf", tempTar, "-C", parentDir, baseName});
    if (tarProc.waitForFinished(10000)) {
        QProcess encProc;
        encProc.start("openssl", {"enc", "-aes-256-cbc", "-salt", "-pbkdf2", "-in", tempTar, "-out", v.vaultPath, "-k", v.password});
        if (encProc.waitForFinished(10000)) {
            QProcess rmProc;
            rmProc.start("rm", {"-rf", v.decryptedPath});
            rmProc.waitForFinished(5000);
        }
    }
    
    if (QFile::exists(tempTar)) {
        QFile::remove(tempTar);
    }
}

bool MainWindow::event(QEvent* event) {
    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseMove) {
        resetVaultIdleTime();
    }
    return QMainWindow::event(event);
}

void MainWindow::updateThemeMusic() {
    QSettings settings("Amifiles", "Amifiles");
    bool zenActive = settings.value("preferences/zen_mode", false).toBool();
    bool shouldPlay = zenActive;

    // Check if the main player is currently playing anything. If so, suspend theme music
    if (m_previewPanel && m_previewPanel->player() && m_previewPanel->player()->playbackState() == QMediaPlayer::PlayingState) {
        shouldPlay = false;
    }

    if (!shouldPlay) {
        if (m_themePlayer && m_themePlayer->playbackState() == QMediaPlayer::PlayingState) {
            m_themePlayer->pause();
        }
        return;
    }

    // Zen Mode is active, and main player is not playing. Ensure theme music is playing/loaded
    QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();
    QStringList searchDirs = {
        QDir::homePath() + "/.config/Amifiles/themes",
        QDir::homePath() + "/.local/share/Amifiles/themes",
        QDir::homePath() + "/.config/Amifiles",
        QDir::homePath() + "/.local/share/Amifiles"
    };

    QStringList namePatterns;
    QString cleanedName = preset.toLower();
    namePatterns.append(cleanedName + ".mp3");
    namePatterns.append(cleanedName.replace(" ", "_") + ".mp3");
    namePatterns.append(cleanedName.replace(" ", "-") + ".mp3");
    namePatterns.append("theme.mp3");
    namePatterns.append("background.mp3");
    namePatterns.append("zen.mp3");

    QString foundMusicPath;
    for (const QString& dirPath : searchDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        for (const QString& pattern : namePatterns) {
            QString fullPath = dir.filePath(pattern);
            if (QFile::exists(fullPath)) {
                foundMusicPath = fullPath;
                break;
            }
        }
        if (!foundMusicPath.isEmpty()) break;
    }

    // If no matching file, stop player if running
    if (foundMusicPath.isEmpty()) {
        if (m_themePlayer) {
            m_themePlayer->stop();
        }
        return;
    }

    // Initialize player if not done already
    if (!m_themePlayer) {
        m_themePlayer = new QMediaPlayer(this);
        m_themeAudioOutput = new QAudioOutput(this);
        m_themePlayer->setAudioOutput(m_themeAudioOutput);
        m_themePlayer->setLoops(QMediaPlayer::Infinite); // Continuous looping
        m_themeAudioOutput->setVolume(0.5f); // Default theme background volume: 50%
    }

    // Only set source if it has changed or is empty
    QUrl musicUrl = QUrl::fromLocalFile(foundMusicPath);
    if (m_themePlayer->source() != musicUrl) {
        m_themePlayer->setSource(musicUrl);
    }

    if (m_themePlayer->playbackState() != QMediaPlayer::PlayingState) {
        m_themePlayer->play();
    }
}

void MainWindow::onMainPlayerStateChanged(QMediaPlayer::PlaybackState state) {
    updateMiniPlayer();
    updateThemeMusic();

    QList<FilePanel*> panels;
    if (m_leftTabWidget) {
        for (int i = 0; i < m_leftTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_leftTabWidget->widget(i));
            if (p) panels.append(p);
        }
    }
    if (m_rightTabWidget) {
        for (int i = 0; i < m_rightTabWidget->count(); ++i) {
            FilePanel* p = qobject_cast<FilePanel*>(m_rightTabWidget->widget(i));
            if (p) panels.append(p);
        }
    }
    for (FilePanel* panel : panels) {
        panel->onPlaybackStateChanged(static_cast<int>(state));
    }
}

#include "mainwindow.moc"
