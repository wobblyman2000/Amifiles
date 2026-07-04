#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QList>
#include <QToolBar>
#include "filepanel.h"
#include "previewpanel.h"

struct CustomButton {
    QString name;
    QString script;
    QString icon;
};

class MiniMediaControls;
class ConsolePanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

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
    
    // Command Routing to Active File Panel
    void onCopyAction();
    void onCutAction();
    void onPasteAction();
    void onDeleteAction();
    void onRenameAction();
    void onNewFolderAction();
    void onShowPropertiesAction();
    void onRefreshAction();
    void onBulkRenameAction();

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

    // Individual pane filter toggle slots
    void onToggleLeftFilterText(bool checked);
    void onToggleLeftCategoryButtons(bool checked);
    void onToggleRightFilterText(bool checked);
    void onToggleRightCategoryButtons(bool checked);
    void onToggleConsole(bool checked);
    void onToggleFlatView(bool checked);
    void onCompareSyncAction();
    void onDuplicateFinderAction();

private:
    void setupActions();
    void setupMenus();
    void setupToolbars();
    void setupCentralWidget();

    // Custom Buttons Persistence
    void loadCustomButtons();
    void saveCustomButtons();
    void rebuildCustomToolBar();

    // State Tracking
    FilePanel* m_activePanel = nullptr;
    bool m_isDualPane = true;
    bool m_showPreview = true;
    bool m_ageColoringEnabled = true;

    // Custom Buttons List
    QList<CustomButton> m_customButtons;

    // View Splitter and Panels
    QSplitter* m_splitter = nullptr;
    FilePanel* m_leftPanel = nullptr;
    FilePanel* m_rightPanel = nullptr;
    PreviewPanel* m_previewPanel = nullptr;
    MiniMediaControls* m_miniMediaControls = nullptr;
    ConsolePanel* m_consolePanel = nullptr;

    // Menus
    QMenu* m_menuFile = nullptr;
    QMenu* m_menuEdit = nullptr;
    QMenu* m_menuView = nullptr;
    QMenu* m_menuFavorites = nullptr;
    QMenu* m_menuDrives = nullptr;
    QMenu* m_menuTools = nullptr;
    QMenu* m_menuHelp = nullptr;

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
    QAction* m_actToggleAgeColoring = nullptr;
    QAction* m_actToggleDrivesMenu = nullptr;
    QAction* m_actToggleDrivesToolbar = nullptr;
    QAction* m_actToggleConsole = nullptr;
    QAction* m_actToggleFlatView = nullptr;

    QAction* m_actLeftShowFilterText = nullptr;
    QAction* m_actLeftShowCategoryButtons = nullptr;
    QAction* m_actRightShowFilterText = nullptr;
    QAction* m_actRightShowCategoryButtons = nullptr;

    // Dynamic Toolbars
    QToolBar* m_tbFile = nullptr;
    QToolBar* m_tbView = nullptr;
    QToolBar* m_customToolBar = nullptr;
    QToolBar* m_tbDrives = nullptr;
};

#endif // MAINWINDOW_H
