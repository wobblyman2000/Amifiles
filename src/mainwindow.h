#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QList>
#include <QToolBar>
#include <QTabWidget>
#include <QDockWidget>
#include <QFrame>
#include <QScrollBar>
#include <QMap>
#include <QKeySequence>
#include "filepanel.h"
#include "previewpanel.h"
#include <QWidget>
#include <QIcon>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QMouseEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

class CustomMenuActionWidget : public QWidget {
    Q_OBJECT
public:
    CustomMenuActionWidget(const QIcon& icon, const QString& text, const QString& colorStr, const QString& displayMode, QWidget* parent = nullptr)
        : QWidget(parent) {
        QHBoxLayout* l = new QHBoxLayout(this);
        l->setContentsMargins(12, 6, 12, 6);
        l->setSpacing(8);

        if (displayMode != "TextOnly" && !icon.isNull()) {
            QLabel* iconLabel = new QLabel(this);
            iconLabel->setPixmap(icon.pixmap(16, 16));
            l->addWidget(iconLabel);
        }

        if (displayMode != "IconOnly") {
            QLabel* textLabel = new QLabel(text, this);
            if (!colorStr.isEmpty()) {
                textLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(colorStr));
            } else {
                textLabel->setStyleSheet("color: #cdd6f4;");
            }
            l->addWidget(textLabel);
        }
        l->addStretch();

        setAutoFillBackground(true);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(0, 0, 0, 0));
        setPalette(pal);
    }

signals:
    void clicked();

protected:
    void enterEvent(QEnterEvent* event) override {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor("#313244"));
        setPalette(pal);
        update();
        QWidget::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(0, 0, 0, 0));
        setPalette(pal);
        update();
        QWidget::leaveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (rect().contains(event->pos())) {
            emit clicked();
        }
        QWidget::mouseReleaseEvent(event);
    }
};

struct CustomButton {
    QString name;
    QString script;
    QString icon;
};

struct FolderLayoutRule {
    QString name;
    QString ruleType; // "Path" or "Category"
    QString value;    // Exact path or Category name (Music, Video, Documents, Images)
    QString viewMode; // "No Change", "List", "Grid", etc.
    QStringList customButtons; // Button names subset
    bool autoApply = true;

    // Visibility Overrides
    bool overrideDrivesToolbar = false;
    bool drivesToolbarVisible = false;
    bool overrideCenterOps = false;
    bool centerOpsVisible = false;
    bool overrideConsole = false;
    bool consoleVisible = false;
    bool overridePreview = false;
    bool previewVisible = false;
    bool overrideFavoritesSidebar = false;
    bool favoritesSidebarVisible = false;
    bool overrideZenMode = false;
    bool zenModeActive = false;
    bool overrideBuiltinPlayerDoubleclick = false;
    bool builtinPlayerDoubleclick = false;
    bool overrideFullScreenPlayer = false;
    bool fullScreenPlayerActive = false;
    bool overrideVisualizer = false;
    bool visualizerActive = false;
    bool overrideDualPane = false;
    bool dualPaneActive = false;
    bool overrideHorizontalSplit = false;
    bool horizontalSplitActive = false;
    bool overrideCasingOverlays = false;
    bool casingOverlaysActive = false;
    bool overrideSmartHome = false;
    bool smartHomeEnabled = true;

    // Toolbar & Menu Overrides
    bool overrideToolbars = false;
    QStringList selectedToolbars;
    bool overrideMenus = false;
    QStringList selectedMenus;

    // Appearance
    bool useBgColor = false;
    QString bgColor; // Hex value
    bool useBgImage = false;
    QString bgImage; // Path to image file
    double bgOpacity = 1.0; // 0.0 to 1.0 (opacity overlay dimming)

    // Tab Snapshot
    bool hasTabsSnapshot = false;
    QStringList leftPaths;
    int leftActiveIndex = 0;
    QStringList rightPaths;
    int rightActiveIndex = 0;
    QByteArray windowState;
    QString linkedProfile;
    QString entryCommand;
    int subfolderDepth = 0; // Number of nested subfolder levels to inherit layout (0 = exact folder only, 3 = show->season->episodes, 999 = unlimited)
};

class MiniMediaControls;
class ConsolePanel;
class QListWidget;
class QListWidgetItem;

struct DecryptedVault {
    QString decryptedPath;
    QString vaultPath;
    QString password;
    QDateTime lastActivity;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    Q_PROPERTY(bool casingOverlaysEnabled READ isCasingOverlaysEnabled)
    friend class FolderLayoutDialog;
public:
    explicit MainWindow(QWidget* parent = nullptr);
    Q_INVOKABLE bool isBuiltinPlayerDoubleclickActive() const;
    Q_INVOKABLE void setBuiltinPlayerDoubleclickActive(bool active);
    bool isApplyingFolderProfile() const { return m_isApplyingFolderProfile; }
    bool hasActiveFolderRule() const { return m_hasActiveFolderRule; }
    FolderLayoutRule activeFolderRule() const { return m_activeFolderRule; }
    class PreviewPanel* previewPanel() const { return m_previewPanel; }
    bool isCasingOverlaysEnabled() const { return m_actToggleCasingOverlays ? m_actToggleCasingOverlays->isChecked() : true; }
    static QJsonObject ruleToJson(const FolderLayoutRule& r);
    static FolderLayoutRule jsonToRule(const QJsonObject& obj);
    ~MainWindow() override;
    void navigateToPathAndSelect(const QString& filePath);
    void updateDrivesList();

signals:
    void builtinPlayerDoubleclickChanged(bool active);

protected:
    void closeEvent(class QCloseEvent* event) override;
    QMenu* createPopupMenu() override;
    void resizeEvent(class QResizeEvent* event) override;
    bool event(class QEvent* event) override;

private slots:
    // Event Routing Slots
    void onPanelActivated(FilePanel* panel);
    void onFileSelected(const QString& filePath);
    void onPreviewFileSaved(const QString& tempPath);
    void onFolderArtDetected(const QString& artPath);
    void onPathChanged(const QString& path);

    // Layout Actions
    void onToggleDualPane(bool checked);
    void onTogglePreview(bool checked);
    void onToggleAgeColoring(bool checked);
    void onToggleCenterOps(bool checked);
    void onPreviewDockContextMenu(const QPoint& pos);
    void onClonePathRequested(const QString& path);
    void onTabPressed();
    void onToggleSyncScroll(bool checked);
    void onAutoSizeColumns();
    void setZenMode(bool enabled);
    void onPlayMediaBuiltin(const QStringList& filePaths);
    void onPlayMediaFullscreen(const QStringList& filePaths);
    void onPlayQueueFullscreen();
    void onQueueMediaBuiltin(const QStringList& filePaths);
    void onActivePanelViewModeChanged();
    void syncFullscreenQueue();
    void onShuffleStateChanged(bool enabled);
    void onRepeatStateChanged(int mode);
    void updateScrollSyncConnections();
    void onTabContextMenuRequested(const QPoint& pos);
    
    // Command Routing to Active File Panel
    void onCopyAction();
    void onCutAction();
    void onPasteAction();
    void onDeleteAction();
    void onRenameAction();
    void onEditAction();
    void onNewFolderAction();
    void onShowPropertiesAction();
    void onRefreshAction();
    void onBulkRenameAction();
    void onQuickRenameAction(const QString& caseType);
    void onCopyToSiblingAction();
    void onMoveToSiblingAction();

    // Favorites List Management
    void onFavoriteTriggered();
    void updateFavoritesMenu();

    // Custom Buttons Slots
    void onAddCustomButton();
    void onCustomButtonClicked();
    void onCustomToolBarContextMenu(const QPoint& pos);
    void onAdvancedSearch();
    
    // Mini status bar player slot
    void updateMiniPlayer();

    // Drive navigation slots
    void onToggleDrivesMenu(bool checked);
    void onToggleDrivesToolbar(bool checked);
    void onDrivesToolbarContextMenu(const QPoint& pos);

    // Individual pane filter toggle slots
    void onToggleLeftFilterText(bool checked);
    void onToggleLeftCategoryButtons(bool checked);
    void onToggleRightFilterText(bool checked);
    void onToggleRightCategoryButtons(bool checked);
    void onToggleConsole(bool checked);
    void onToggleFlatView(bool checked);
    void onToggleShowHiddenFiles(bool checked);
    void onCompareSyncAction();
    void onDuplicateFinderAction();
    void onToggleFavoritesSidebar(bool checked);
    void onFavoritesSidebarClicked(class QListWidgetItem* item);
    void refreshFavoritesSidebar();
    void refreshRecentsSidebar();
    void addToRecentFolders(const QString& path);
    void onMutePreview(bool checked);
    void onToggleArchiveNav(bool checked);
    void onToggleArchiveWrite(bool checked);
    void onToggleHorizontalSplit(bool checked);
    void onToggleCasingOverlays(bool checked);
    void onToggleAudioCoverArt(bool checked);
    void onSaveSearchPreset();
    void onScreenGrabAction();
    void onAdvancedTagEditorAction();
    void onPreferencesAction();
    void onMediaPreferences();
    void onThemeStudioAction();
    void onCreateSmartCollectionAction();
    void updateSearchPresetsMenu();
    void onSearchPresetTriggered();

    // Tab slots
    void onLeftTabChanged(int index);
    void onRightTabChanged(int index);
    void onTabCloseRequested(int index);
    void onNewTabAction();
    void onCloseTabAction();
    void onShowHelpAction();
    void onSpaceAnalyzerAction();
    void onKeybindingsEditorAction();
    void onQuickFilterSidebarClicked(class QListWidgetItem* item);
    void onCalculateChecksum();
    void onSecureShred();
    void onRemoteMount();
    void onCloudMount();
    void onCreateVhd();
    void onImageConvert();
    void onConfigureDynamicBookmarks();
    void onImportCustomButtons();
    void onExportCustomButtons();
    void onProcessManagerAction();
    void onTagsSidebarClicked(class QListWidgetItem* item);
    void onEncryptVault();
    void onDecryptVault();
    void onToggleSpectrum(bool checked);
    void onConfigureAgeStyling();
    void onSaveLayoutNow();
    void onResetLayout();
    void onBackupSettings();
    void onRestoreSettings();
    void onConfigureBackupSchedule();
    void onConfigureAutoTags();
    void onConfigureAutoOrganizer();
    void onRemoteMountsManager();
    void onCommandPaletteAction();
    void onCommandPaletteTriggered(const QString& action);
    Q_INVOKABLE void refreshTagsSidebar();

public:
    void onConfigureFolderLayouts();
    void onSaveFolderProfileForCurrentDir();
    void onSaveDefaultProfile();
    void onLoadDefaultProfile();
    void onApplyProfileToCurrentFolder(const QString& profileName);
    void applyFolderRules(const QString& path, class FilePanel* callingPanel = nullptr);
    QTabWidget* leftTabWidget() const { return m_leftTabWidget; }
    QTabWidget* rightTabWidget() const { return m_rightTabWidget; }
    FilePanel* leftPanel() const;
    FilePanel* rightPanel() const;
    FilePanel* activePanel() const { return m_activePanel; }
    const QList<FolderLayoutRule>& folderRules() const { return m_folderRules; }
    void setFolderRules(const QList<FolderLayoutRule>& rules) { m_folderRules = rules; }
    void saveFolderRules();
    void refreshAllDashboards();
    FilePanel* createTab(QTabWidget* tabWidget, const QString& path);
    void updateSiblingLinks();
    void executeCustomCommand(const QString& commandOrPath);
    QJsonArray getDefaultCustomMenus();

private:
    void setupActions();
    void setupMenus();
    void setupToolbars();
    void setupCentralWidget();

    // Custom Buttons Persistence
    void loadCustomButtons();
    void syncCustomButtonsFromJson();
    void saveCustomButtons();
    void rebuildCustomToolBar();
    void loadFolderRules();
    void applyProfile(const FolderLayoutRule& r, FilePanel* targetPanel = nullptr);
    QString detectFolderCategory(const QString& path);
    void adjustSplitterSizes();
    void apply5050Layouts();
    void updateLayoutLockState();
    void queueAdjustSplitterSizes();
    FolderLayoutRule getDefaultRule();

    // State Tracking
    FilePanel* m_activePanel = nullptr;
    bool m_isDualPane = true;
    bool m_showPreview = true;
    bool m_previewDockAutoShownForPlayback = false;
    bool m_ageColoringEnabled = true;
    bool m_wasDualPaneBeforeLock = false;
    bool m_wasPreviewBeforeLock = false;

    // Custom Buttons List
    QList<CustomButton> m_customButtons;
    QList<FolderLayoutRule> m_folderRules;
    QStringList m_activeToolbarFilter;

    // View Splitter and Panels
    QSplitter* m_splitter = nullptr;
    QSplitter* m_dualSplitter = nullptr;
    QTabWidget* m_sidebarTabWidget = nullptr;
    QListWidget* m_favoritesSidebar = nullptr;
    QListWidget* m_recentsSidebar = nullptr;
    QListWidget* m_filtersSidebar = nullptr;
    QListWidget* m_tagsSidebar = nullptr;
    QTabWidget* m_leftTabWidget = nullptr;
    QTabWidget* m_rightTabWidget = nullptr;
    PreviewPanel* m_previewPanel = nullptr;
    QDockWidget* m_previewDock = nullptr;
    QDockWidget* m_fullscreenQueueDock = nullptr;
    class QListWidget* m_fullscreenQueueList = nullptr;
    QFrame* m_tbCenterOps = nullptr;
    QFrame* m_tbCenterOpsSeparator = nullptr;
    QAction* m_actToggleCenterOps = nullptr;
    class QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    class QMediaPlayer* m_themePlayer = nullptr;
    class QAudioOutput* m_themeAudioOutput = nullptr;
    void updateThemeMusic();
    void onMainPlayerStateChanged(QMediaPlayer::PlaybackState state);
    bool m_wasPreviewPriorToCollapse = false;
    bool m_wasDualPanePriorToCollapse = false;
    QString m_lastEntryCommandPath;
    QList<DecryptedVault> m_activeVaults;
    class QTimer* m_vaultIdleTimer = nullptr;
    QDateTime m_lastUserActivity;
    void registerDecryptedVault(const QString& decryptedPath, const QString& vaultPath, const QString& password);
    void resetVaultIdleTime();
    void checkVaultIdleTimeout();
    void lockVault(int index);
    ConsolePanel* m_consolePanel = nullptr;
    class TerminalPanel* m_terminalPanel = nullptr;
    QTabWidget* m_bottomTabWidget = nullptr;
    QAction* m_actToggleFavoritesSidebar = nullptr;

    // Menus
    QMenu* m_menuFile = nullptr;
    QMenu* m_menuEdit = nullptr;
    QMenu* m_menuSettings = nullptr;
    QMenu* m_menuView = nullptr;
    QMenu* m_menuFavorites = nullptr;
    QMenu* m_menuDrives = nullptr;
    QMenu* m_menuTools = nullptr;
    QMenu* m_menuHelp = nullptr;
    QMenu* m_menuSearch = nullptr;
    QMenu* m_menuSearchPresets = nullptr;
    QMenu* m_menuPlayback = nullptr;
    QAction* m_actPlaybackNowPlaying = nullptr;
    QAction* m_actPlaybackPlayPause = nullptr;
    QAction* m_actPlaybackStop = nullptr;
    QAction* m_actPlaybackNext = nullptr;
    QAction* m_actPlaybackPrev = nullptr;
    QAction* m_actPlaybackShuffle = nullptr;
    QAction* m_actPlaybackRepeat = nullptr;
    QAction* m_actPlayFolder = nullptr;
    QAction* m_actQueueFolder = nullptr;
    QAction* m_actPlayQueue = nullptr;
    QAction* m_actPlayCollection = nullptr;

    // Actions (wired to toolbar & menu items)
    QAction* m_actCopy = nullptr;
    QAction* m_actCut = nullptr;
    QAction* m_actPaste = nullptr;
    QAction* m_actDelete = nullptr;
    QAction* m_actRename = nullptr;
    QAction* m_actNewFolder = nullptr;
    QAction* m_actProperties = nullptr;
    QAction* m_actRefresh = nullptr;
    QAction* m_actBulkRename = nullptr;
    QAction* m_actCompareSync = nullptr;
    QAction* m_actDuplicateFinder = nullptr;

    QAction* m_actToggleDualPane = nullptr;
    QAction* m_actTogglePreview = nullptr;
    QAction* m_actScreenGrab = nullptr;
    QAction* m_actAdvancedTagEditor = nullptr;
    QAction* m_actCommandPalette = nullptr;
    QAction* m_actMutePreview = nullptr;
    QAction* m_actToggleAgeColoring = nullptr;
    QAction* m_actConfigureAgeStyling = nullptr;
    QAction* m_actToggleArchiveNav = nullptr;
    QAction* m_actToggleArchiveWrite = nullptr;
    QAction* m_actToggleHorizontalSplit = nullptr;
    QAction* m_actToggleCasingOverlays = nullptr;
    QAction* m_actToggleDrivesMenu = nullptr;
    QAction* m_actToggleDrivesToolbar = nullptr;
    QAction* m_actToggleConsole = nullptr;
    QAction* m_actToggleFlatView = nullptr;
    QAction* m_actShowHiddenFiles = nullptr;
    QAction* m_actShowAudioCoverArt = nullptr;
    QAction* m_actToggleSpectrum = nullptr;
    QAction* m_actAutoSaveLayout = nullptr;
    QAction* m_actPreferences = nullptr;
    QAction* m_actMediaPreferences = nullptr;
    QAction* m_actBypassFolderProfiles = nullptr;
    QAction* m_actThemeStudio = nullptr;
    QAction* m_actSaveLayoutNow = nullptr;
    QAction* m_actResetLayout = nullptr;
    QAction* m_actBackupSettings = nullptr;
    QAction* m_actRestoreSettings = nullptr;
    QAction* m_actConfigureFolderLayouts = nullptr;
    QAction* m_actSaveFolderProfileForCurrentDir = nullptr;
    QAction* m_actSaveDefaultProfile = nullptr;
    QAction* m_actLoadDefaultProfile = nullptr;
    QAction* m_actConfigureBackupSchedule = nullptr;
    QAction* m_actCreateSmartCollection = nullptr;
    QAction* m_actCopyToSibling = nullptr;
    QAction* m_actMoveToSibling = nullptr;
    QAction* m_actClonePathToSibling = nullptr;
    QAction* m_actEdit = nullptr;
    QAction* m_actToggleSyncScroll = nullptr;
    bool m_syncScrollEnabled = false;
    bool m_zenMode = false;
    bool m_toolbarEditMode = false;
    QPoint m_dragStartPos;
    FolderLayoutRule m_activeFolderRule;
    bool m_hasActiveFolderRule = false;
    bool m_isInitializing = false;
    bool m_isApplyingFolderProfile = false;
    bool isToolbarDefaultVisible(const QString& toolbarId);
    QScrollBar* m_leftScrollConnected = nullptr;
    QScrollBar* m_rightScrollConnected = nullptr;
    QAction* m_actLeftShowFilterText = nullptr;
    QAction* m_actLeftShowCategoryButtons = nullptr;
    QAction* m_actRightShowFilterText = nullptr;
    QAction* m_actRightShowCategoryButtons = nullptr;
    QAction* m_actAutoSizeColumns = nullptr;
    QAction* m_actToggleZenMode = nullptr;
    QAction* m_actToggleToolbarEditMode = nullptr;

    QAction* m_actNewTab = nullptr;
    QAction* m_actCloseTab = nullptr;
    QAction* m_actShowHelp = nullptr;
    QAction* m_actSpaceAnalyzer = nullptr;
    QAction* m_actSmartHome = nullptr;
    QAction* m_actHome = nullptr;
    QAction* m_actKeybindings = nullptr;
    QAction* m_actCalculateChecksum = nullptr;
    QAction* m_actAdvancedSearch = nullptr;
    QAction* m_actSecureShred = nullptr;
    QAction* m_actRemoteMount = nullptr;
    QAction* m_actCloudMount = nullptr;
    QAction* m_actCreateVhd = nullptr;
    QAction* m_actImageConvert = nullptr;
    QAction* m_actProcessManager = nullptr;
    QAction* m_actEncryptVault = nullptr;
    QAction* m_actDecryptVault = nullptr;
    QAction* m_actConfigureAutoTags = nullptr;
    QAction* m_actConfigureAutoOrganizer = nullptr;
    QAction* m_actRemoteMountsManager = nullptr;

    // Dynamic Toolbars
    QToolBar* m_tbFile = nullptr;
    QToolBar* m_tbView = nullptr;
    QToolBar* m_customToolBar = nullptr;
    QToolBar* m_tbDrives = nullptr;

    // Keybindings mapping & management
    QMap<QString, QKeySequence> m_keybindings;
    QMap<QString, QAction*> m_keybindableActions;
    void loadKeybindings();
    void saveKeybindings();
    void applyKeybindings();
    void updateWidgetStylesheets();
    void updateTooltips();
    void onToggleDetailedTooltips(bool enabled);
    void registerKeybindableAction(const QString& id, QAction* action);
    QIcon getFolderIcon(const QString& folderName);

    // Custom Dynamic Menu System
    void rebuildCustomMenus();
    void onConfigureCustomMenus();
    void onConfigureContextMenu();
    void buildMenuTree(QMenu* menu, const QJsonArray& itemsArray);
    QList<QMenu*> m_customMenus;
    QAction* m_actConfigureCustomMenus = nullptr;
    QAction* m_actConfigureContextMenu = nullptr;

    // Custom Dynamic Toolbar System
    void rebuildToolBars();
    QAction* findInternalAction(const QString& actId) const;
    void onConfigureToolbars();
    QList<QToolBar*> m_dynamicToolBars;
    QAction* m_actConfigureToolbars = nullptr;
    void showToolbarItemEditMenu(QToolBar* tb, QAction* act, const QPoint& globalPos);
    void showToolbarContextMenu(QToolBar* tb, const QPoint& pos);
    void editToolbarItem(QToolBar* tb, QAction* act);
    void removeToolbarItem(QToolBar* tb, QAction* act);
    void handleToolbarDrop(const QString& sourceTbId, int sourceIdx, const QString& targetTbId, int targetIdx,
                           const QString& type, const QString& actId, const QString& name, const QString& icon,
                           const QString& command, bool isDynamicDrive);
    QJsonObject m_copiedToolbarItem;
    void updateActiveRuleLayoutSetting(const QString& field, bool value);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

#endif // MAINWINDOW_H
