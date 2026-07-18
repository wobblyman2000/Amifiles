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
    QString ruleType; // "Path" or "Category"
    QString value;    // Exact path or Category name (Music, Video, Documents, Images)
    QString viewMode; // "Default", "List", "Grid"
    QStringList customButtons; // Button names subset
};

class MiniMediaControls;
class ConsolePanel;
class QListWidget;
class QListWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class WorkspaceProfileDialog;
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(class QCloseEvent* event) override;

private slots:
    // Event Routing Slots
    void onPanelActivated(FilePanel* panel);
    void onFileSelected(const QString& filePath);
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
    void onCopyToSiblingAction();
    void onMoveToSiblingAction();

    // Favorites List Management
    void onFavoriteTriggered();
    void updateFavoritesMenu();

    // Custom Buttons Slots
    void onAddCustomButton();
    void onCustomButtonClicked();
    void onCustomToolBarContextMenu(const QPoint& pos);
    
    // Mini status bar player slot
    void updateMiniPlayer();

    // Drive navigation slots
    void updateDrivesList();
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
    void onCompareSyncAction();
    void onDuplicateFinderAction();
    void onToggleFavoritesSidebar(bool checked);
    void onFavoritesSidebarDoubleClicked(class QListWidgetItem* item);
    void refreshFavoritesSidebar();
    void onMutePreview(bool checked);
    void onToggleArchiveNav(bool checked);
    void onToggleArchiveWrite(bool checked);
    void onToggleHorizontalSplit(bool checked);
    void onToggleCasingOverlays(bool checked);
    void onToggleAudioCoverArt(bool checked);
    void onSaveSearchPreset();
    void onPreferencesAction();
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
    void onConfigureFolderLayouts();
    void onConfigureBackupSchedule();
    void onConfigureAutoTags();
    void onConfigureAutoOrganizer();
    void onRemoteMountsManager();
    void onCommandPaletteAction();
    void onCommandPaletteTriggered(const QString& action);
    Q_INVOKABLE void refreshTagsSidebar();

public:
    QTabWidget* leftTabWidget() const { return m_leftTabWidget; }
    QTabWidget* rightTabWidget() const { return m_rightTabWidget; }
    FilePanel* leftPanel() const;
    FilePanel* rightPanel() const;
    FilePanel* createTab(QTabWidget* tabWidget, const QString& path);
    void updateSiblingLinks();

private:
    void setupActions();
    void setupMenus();
    void setupToolbars();
    void setupCentralWidget();

    // Custom Buttons Persistence
    void loadCustomButtons();
    void saveCustomButtons();
    void rebuildCustomToolBar();
    void loadFolderRules();
    void saveFolderRules();
    void applyFolderRules(const QString& path);
    QString detectFolderCategory(const QString& path);
    void adjustSplitterSizes();

    // State Tracking
    FilePanel* m_activePanel = nullptr;
    bool m_isDualPane = true;
    bool m_showPreview = true;
    bool m_ageColoringEnabled = true;

    // Custom Buttons List
    QList<CustomButton> m_customButtons;
    QList<FolderLayoutRule> m_folderRules;
    QStringList m_activeToolbarFilter;

    // View Splitter and Panels
    QSplitter* m_splitter = nullptr;
    QSplitter* m_dualSplitter = nullptr;
    QTabWidget* m_sidebarTabWidget = nullptr;
    QListWidget* m_favoritesSidebar = nullptr;
    QListWidget* m_filtersSidebar = nullptr;
    QListWidget* m_tagsSidebar = nullptr;
    QTabWidget* m_leftTabWidget = nullptr;
    QTabWidget* m_rightTabWidget = nullptr;
    PreviewPanel* m_previewPanel = nullptr;
    QDockWidget* m_previewDock = nullptr;
    QFrame* m_tbCenterOps = nullptr;
    QFrame* m_tbCenterOpsSeparator = nullptr;
    QAction* m_actToggleCenterOps = nullptr;
    MiniMediaControls* m_miniMediaControls = nullptr;
    ConsolePanel* m_consolePanel = nullptr;
    class TerminalPanel* m_terminalPanel = nullptr;
    QTabWidget* m_bottomTabWidget = nullptr;
    QAction* m_actToggleFavoritesSidebar = nullptr;

    // Menus
    QMenu* m_menuFile = nullptr;
    QMenu* m_menuEdit = nullptr;
    QMenu* m_menuView = nullptr;
    QMenu* m_menuFavorites = nullptr;
    QMenu* m_menuDrives = nullptr;
    QMenu* m_menuTools = nullptr;
    QMenu* m_menuHelp = nullptr;
    QMenu* m_menuSearch = nullptr;
    QMenu* m_menuSearchPresets = nullptr;

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
    QAction* m_actShowAudioCoverArt = nullptr;
    QAction* m_actToggleSpectrum = nullptr;
    QAction* m_actAutoSaveLayout = nullptr;
    QAction* m_actWorkspaceProfiles = nullptr;
    QAction* m_actPreferences = nullptr;
    QAction* m_actThemeStudio = nullptr;
    QAction* m_actSaveLayoutNow = nullptr;
    QAction* m_actResetLayout = nullptr;
    QAction* m_actConfigureFolderLayouts = nullptr;
    QAction* m_actConfigureBackupSchedule = nullptr;
    QAction* m_actCreateSmartCollection = nullptr;
    QAction* m_actCopyToSibling = nullptr;
    QAction* m_actMoveToSibling = nullptr;
    QAction* m_actClonePathToSibling = nullptr;
    QAction* m_actEdit = nullptr;
    QAction* m_actToggleSyncScroll = nullptr;
    bool m_syncScrollEnabled = false;
    QScrollBar* m_leftScrollConnected = nullptr;
    QScrollBar* m_rightScrollConnected = nullptr;
    QAction* m_actLeftShowFilterText = nullptr;
    QAction* m_actLeftShowCategoryButtons = nullptr;
    QAction* m_actRightShowFilterText = nullptr;
    QAction* m_actRightShowCategoryButtons = nullptr;

    QAction* m_actNewTab = nullptr;
    QAction* m_actCloseTab = nullptr;
    QAction* m_actShowHelp = nullptr;
    QAction* m_actSpaceAnalyzer = nullptr;
    QAction* m_actKeybindings = nullptr;
    QAction* m_actCalculateChecksum = nullptr;
    QAction* m_actSecureShred = nullptr;
    QAction* m_actRemoteMount = nullptr;
    QAction* m_actCloudMount = nullptr;
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
    void registerKeybindableAction(const QString& id, QAction* action);
    QIcon getFolderIcon(const QString& folderName);

    // Custom Dynamic Menu System
    void rebuildCustomMenus();
    void onConfigureCustomMenus();
    void executeCustomCommand(const QString& commandOrPath);
    void buildMenuTree(QMenu* menu, const QJsonArray& itemsArray);
    QJsonArray getDefaultCustomMenus();
    QList<QMenu*> m_customMenus;
    QAction* m_actConfigureCustomMenus = nullptr;

    // Custom Dynamic Toolbar System
    void rebuildToolBars();
    void onConfigureToolbars();
    QList<QToolBar*> m_dynamicToolBars;
    QAction* m_actConfigureToolbars = nullptr;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

#endif // MAINWINDOW_H
