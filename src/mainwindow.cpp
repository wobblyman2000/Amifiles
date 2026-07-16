#include "mainwindow.h"
#include "copyqueue.h"
#include "theme.h"
#include "favoritesmanager.h"
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
#include "processmanagerdialog.h"
#include "vaultdialog.h"
#include <QMenuBar>
#include <QStorageInfo>
#include <QToolBar>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
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

// Built-in dialog to create/edit custom commands (script buttons)
class CustomButtonDialog : public QDialog {
public:
    CustomButtonDialog(const QString& name, const QString& script, const QString& iconPath, QWidget* parent = nullptr) 
        : QDialog(parent), m_iconPath(iconPath) {
        setWindowTitle("Custom Command Button Editor");
        resize(500, 500);

        QVBoxLayout* layout = new QVBoxLayout(this);

        layout->addWidget(new QLabel("Button Label:"));
        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setText(name);
        m_nameEdit->setPlaceholderText("Enter button text...");
        layout->addWidget(m_nameEdit);

        // System Icon Theme Selection
        layout->addWidget(new QLabel("Select System Icon (Links to active KDE/Breeze Theme):"));
        m_comboSysIcon = new QComboBox(this);
        m_comboSysIcon->addItems({
            "(No System Icon)",
            "📁 Folder (folder)",
            "💽 Hard Drive (drive-harddisk)",
            "🖥️ Terminal (utilities-terminal)",
            "⚡ Run / Execute (system-run)",
            "🔄 Refresh / Sync (view-refresh)",
            "ℹ️ Info / Help (help-about)",
            "📄 Document (document-properties)",
            "📋 Copy (edit-copy)",
            "✂️ Cut (edit-cut)",
            "📥 Paste (edit-paste)",
            "🗑️ Delete (edit-delete)",
            "⚙️ Settings (preferences-system)",
            "🔍 Search (edit-find)",
            "➕ Add (list-add)"
        });

        static const QStringList themeKeys = {
            "",
            "folder",
            "drive-harddisk",
            "utilities-terminal",
            "system-run",
            "view-refresh",
            "help-about",
            "document-properties",
            "edit-copy",
            "edit-cut",
            "edit-paste",
            "edit-delete",
            "preferences-system",
            "edit-find",
            "list-add"
        };

        if (m_iconPath.startsWith("theme:")) {
            QString currentThemeKey = m_iconPath.mid(6);
            int idx = themeKeys.indexOf(currentThemeKey);
            if (idx >= 0) {
                m_comboSysIcon->setCurrentIndex(idx);
            }
        }

        connect(m_comboSysIcon, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (index > 0) {
                m_iconPath = "theme:" + themeKeys[index];
                m_lblIconPath->setText("System Theme: " + themeKeys[index]);
            } else {
                if (m_iconPath.startsWith("theme:")) {
                    m_iconPath.clear();
                    m_lblIconPath->setText("(No Icon Selected)");
                }
            }
        });
        layout->addWidget(m_comboSysIcon);

        layout->addWidget(new QLabel("Or Browse for Custom Image File:"));
        QHBoxLayout* iconLayout = new QHBoxLayout();
        m_lblIconPath = new QLabel(m_iconPath.isEmpty() ? "(No Icon Selected)" : m_iconPath, this);
        m_lblIconPath->setStyleSheet("color: #a6adc8; font-size: 11px;");
        m_lblIconPath->setWordWrap(true);
        
        QPushButton* btnSelectIcon = new QPushButton("Browse...", this);
        btnSelectIcon->setMaximumWidth(100);
        connect(btnSelectIcon, &QPushButton::clicked, this, [this]() {
            QString file = QFileDialog::getOpenFileName(this, "Select Icon Image", 
                                                        QDir::homePath(), 
                                                        "Images (*.png *.jpg *.jpeg *.svg *.xpm *.gif);;All Files (*)");
            if (!file.isEmpty()) {
                m_iconPath = file;
                m_lblIconPath->setText(m_iconPath);
                m_comboSysIcon->setCurrentIndex(0); // Reset system theme selection
            }
        });

        QPushButton* btnClearIcon = new QPushButton("Clear", this);
        btnClearIcon->setMaximumWidth(80);
        connect(btnClearIcon, &QPushButton::clicked, this, [this]() {
            m_iconPath.clear();
            m_lblIconPath->setText("(No Icon Selected)");
            m_comboSysIcon->setCurrentIndex(0); // Reset
        });

        iconLayout->addWidget(m_lblIconPath, 1);
        iconLayout->addWidget(btnSelectIcon);
        iconLayout->addWidget(btnClearIcon);
        layout->addLayout(iconLayout);

        layout->addWidget(new QLabel("Bash Script / Shell Commands (Working dir is active folder):"));
        m_scriptEdit = new QPlainTextEdit(this);
        m_scriptEdit->setPlainText(script);
        m_scriptEdit->setPlaceholderText("e.g. tar -czf myfiles.tar.gz $AMIFILES_SELECTED");
        m_scriptEdit->setStyleSheet("font-family: monospace; font-size: 12px; background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px;");
        layout->addWidget(m_scriptEdit, 1);

        // Env variables description
        QLabel* lblHelp = new QLabel(
            "<b>Available Environment Variables & Macros:</b><br/>"
            "  <font color='#f38ba8'>$AMIFILES_CURRENT_DIR</font> or <font color='#f38ba8'>{dir}</font> - Active folder directory<br/>"
            "  <font color='#a6e3a1'>$AMIFILES_SELECTED_FIRST</font> or <font color='#a6e3a1'>{filepath}</font> - First selected path<br/>"
            "  <font color='#89b4fa'>$AMIFILES_SELECTED</font> - Newline list of all selected paths<br/>"
            "  <font color='#fab387'>{dest}</font> - Path of the opposite panel folder<br/>"
            "<b>Internal Commands:</b> Start command with <font color='#89b4fa'>@internal:</font> followed by:<br/>"
            "  Copy, Move, Cut, Paste, Delete, Rename, NewFolder, Refresh, CompareSync,<br/>"
            "  DuplicateFinder, SpaceAnalyzer, ToggleDualPane, TogglePreview, ToggleFlatView, Go &lt;path&gt;", this);
        lblHelp->setStyleSheet("color: #a6adc8; font-size: 10px;");
        layout->addWidget(lblHelp);

        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* btnSave = new QPushButton("Save", this);
        QPushButton* btnCancel = new QPushButton("Cancel", this);
        
        btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
        connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        
        btnLayout->addStretch(1);
        btnLayout->addWidget(btnSave);
        btnLayout->addWidget(btnCancel);
        layout->addLayout(btnLayout);
    }

    QString buttonName() const { return m_nameEdit->text().trimmed(); }
    QString script() const { return m_scriptEdit->toPlainText(); }
    QString iconPath() const { return m_iconPath; }

private:
    QLineEdit* m_nameEdit = nullptr;
    QPlainTextEdit* m_scriptEdit = nullptr;
    QComboBox* m_comboSysIcon = nullptr;
    QLabel* m_lblIconPath = nullptr;
    QString m_iconPath;
};

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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Amifiles - File Manager");
    resize(1200, 800);

    // Apply global modern theme
    setStyleSheet(Theme::Stylesheet);

    // Initialize layout components
    setupCentralWidget();
    setupActions();
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

    // Load custom buttons & build custom toolbar
    loadCustomButtons();
    rebuildCustomToolBar();

    // Load drives menu & toolbar visibility from settings
    QSettings settings("Amifiles", "Amifiles");
    bool drivesMenuVisible = settings.value("drives/menu_visible", true).toBool();
    bool drivesToolbarVisible = settings.value("drives/toolbar_visible", true).toBool();

    m_actToggleDrivesMenu->setChecked(drivesMenuVisible);
    m_menuDrives->menuAction()->setVisible(drivesMenuVisible);

    m_actToggleDrivesToolbar->setChecked(drivesToolbarVisible);
    m_tbDrives->setVisible(drivesToolbarVisible);

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

    bool casingOverlays = settings.value("preferences/casing_overlays", true).toBool();
    m_actToggleCasingOverlays->setChecked(casingOverlays);

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

    // Initial populate of drives
    updateDrivesList();

    // Default active panel is Left
    onPanelActivated(leftPanel());

    // Restore window layout geometry & state
    bool autoSaveLayout = settings.value("layout/auto_save", true).toBool();
    if (m_actAutoSaveLayout) {
        m_actAutoSaveLayout->setChecked(autoSaveLayout);
    }
    if (autoSaveLayout || settings.contains("window/geometry")) {
        QByteArray geom = settings.value("window/geometry").toByteArray();
        QByteArray state = settings.value("window/state").toByteArray();
        if (!geom.isEmpty()) {
            restoreGeometry(geom);
        }
        if (!state.isEmpty()) {
            restoreState(state);
        }
    }

    // Load and apply folder layout rules
    loadFolderRules();
    if (m_activePanel) {
        applyFolderRules(m_activePanel->currentPath());
    }
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
    m_favoritesSidebar->setFocusPolicy(Qt::NoFocus);
    connect(m_favoritesSidebar, &QListWidget::itemDoubleClicked, this, &MainWindow::onFavoritesSidebarDoubleClicked);

    m_filtersSidebar = new QListWidget(m_sidebarTabWidget);
    m_filtersSidebar->setStyleSheet(m_favoritesSidebar->styleSheet());
    m_filtersSidebar->setFocusPolicy(Qt::NoFocus);

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
    m_tagsSidebar->setFocusPolicy(Qt::NoFocus);
    connect(m_tagsSidebar, &QListWidget::itemClicked, this, &MainWindow::onTagsSidebarClicked);

    m_sidebarTabWidget->addTab(m_favoritesSidebar, "Bookmarks");
    m_sidebarTabWidget->addTab(m_filtersSidebar, "Filters");
    m_sidebarTabWidget->addTab(m_tagsSidebar, "Tags");

    refreshTagsSidebar();

    m_leftTabWidget = new QTabWidget(this);
    m_leftTabWidget->setTabsClosable(true);
    m_leftTabWidget->setMovable(true);
    m_leftTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: #1e1e2e; }"
        "QTabBar::tab { background-color: #181825; color: #a6adc8; border: 1px solid #313244; border-bottom: none; padding: 6px 12px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #1e1e2e; color: #cdd6f4; border-color: #313244; }"
        "QTabBar::tab:hover { background-color: #313244; color: #cdd6f4; }"
    );

    m_rightTabWidget = new QTabWidget(this);
    m_rightTabWidget->setTabsClosable(true);
    m_rightTabWidget->setMovable(true);
    m_rightTabWidget->setStyleSheet(m_leftTabWidget->styleSheet());

    m_previewPanel = new PreviewPanel(this);
    connect(m_previewPanel, &PreviewPanel::spectrumVisualizerToggled, this, &MainWindow::onToggleSpectrum);

    // Add first tabs
    createTab(m_leftTabWidget, QDir::homePath());
    createTab(m_rightTabWidget, QDir::homePath());

    m_splitter->addWidget(m_sidebarTabWidget);
    m_splitter->addWidget(m_leftTabWidget);
    m_splitter->addWidget(m_rightTabWidget);
    m_splitter->addWidget(m_previewPanel);

    m_splitter->setSizes({160, 400, 400, 240});

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

    updateSiblingLinks();

    // Initialize mini media controller in the status bar
    m_miniMediaControls = new MiniMediaControls(m_previewPanel->player(), this);
    statusBar()->addPermanentWidget(m_miniMediaControls);
    m_miniMediaControls->hide();

    // Wire player notifications to keep mini status bar controller synchronized
    connect(m_previewPanel->player(), &QMediaPlayer::sourceChanged, this, &MainWindow::updateMiniPlayer);
    connect(m_previewPanel->player(), &QMediaPlayer::durationChanged, this, &MainWindow::updateMiniPlayer);
    connect(m_previewPanel->player(), &QMediaPlayer::playbackStateChanged, this, &MainWindow::updateMiniPlayer);
}

void MainWindow::setupActions() {
    QStyle* style = QApplication::style();

    // File Actions
    m_actNewFolder = new QAction(style->standardIcon(QStyle::SP_FileDialogNewFolder), "New Folder", this);
    m_actNewFolder->setShortcut(QKeySequence::New);
    m_actNewFolder->setToolTip("Create a new folder in active directory");
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
    m_actDelete->setShortcut(QKeySequence::Delete);
    m_actDelete->setToolTip("Permanently delete selected items");
    connect(m_actDelete, &QAction::triggered, this, &MainWindow::onDeleteAction);

    m_actRename = new QAction("Rename", this);
    m_actRename->setShortcut(QKeySequence(Qt::Key_F2));
    m_actRename->setToolTip("Rename selected item");
    connect(m_actRename, &QAction::triggered, this, &MainWindow::onRenameAction);

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

    m_actCompareSync = new QAction(style->standardIcon(QStyle::SP_DialogYesButton), "Compare & Sync Folders...", this);
    m_actCompareSync->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    m_actCompareSync->setToolTip("Compare left and right folder contents and synchronize them");
    connect(m_actCompareSync, &QAction::triggered, this, &MainWindow::onCompareSyncAction);

    m_actDuplicateFinder = new QAction(style->standardIcon(QStyle::SP_MessageBoxQuestion), "Find Duplicate Files...", this);
    m_actDuplicateFinder->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    m_actDuplicateFinder->setToolTip("Find duplicate files by size or hash in directories");
    connect(m_actDuplicateFinder, &QAction::triggered, this, &MainWindow::onDuplicateFinderAction);

    // Layout Toggle Actions
    m_actToggleDualPane = new QAction("Dual Pane View", this);
    m_actToggleDualPane->setCheckable(true);
    m_actToggleDualPane->setChecked(true);
    m_actToggleDualPane->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_actToggleDualPane->setToolTip("Toggle between single and dual pane file display");
    connect(m_actToggleDualPane, &QAction::toggled, this, &MainWindow::onToggleDualPane);

    m_actTogglePreview = new QAction("Preview Pane", this);
    m_actTogglePreview->setCheckable(true);
    m_actTogglePreview->setChecked(true);
    m_actTogglePreview->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    m_actTogglePreview->setToolTip("Toggle the file preview panel on/off");
    connect(m_actTogglePreview, &QAction::toggled, this, &MainWindow::onTogglePreview);

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

    m_actSaveLayoutNow = new QAction("Save Current Layout Now", this);
    m_actSaveLayoutNow->setToolTip("Manually save toolbar positions and window dimensions");
    connect(m_actSaveLayoutNow, &QAction::triggered, this, &MainWindow::onSaveLayoutNow);

    m_actResetLayout = new QAction("Reset Layout to Defaults", this);
    m_actResetLayout->setToolTip("Restore all toolbars and panels to their factory positions");
    connect(m_actResetLayout, &QAction::triggered, this, &MainWindow::onResetLayout);

    m_actConfigureFolderLayouts = new QAction("Configure Folder-Specific Layouts...", this);
    m_actConfigureFolderLayouts->setToolTip("Define custom view modes and toolbars for specific folders or media categories");
    connect(m_actConfigureFolderLayouts, &QAction::triggered, this, &MainWindow::onConfigureFolderLayouts);

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
    connect(m_actKeybindings, &QAction::triggered, this, &MainWindow::onKeybindingsEditorAction);
    addAction(m_actKeybindings);

    m_actCalculateChecksum = new QAction("Calculate Checksum Hash...", this);
    m_actCalculateChecksum->setToolTip("Calculate MD5/SHA-1/SHA-256 hash for a file");
    connect(m_actCalculateChecksum, &QAction::triggered, this, &MainWindow::onCalculateChecksum);
    addAction(m_actCalculateChecksum);

    m_actSecureShred = new QAction("Secure Shred files...", this);
    m_actSecureShred->setToolTip("Securely shred and delete files permanently");
    connect(m_actSecureShred, &QAction::triggered, this, &MainWindow::onSecureShred);
    addAction(m_actSecureShred);

    m_actRemoteMount = new QAction("Mount Remote Share...", this);
    m_actRemoteMount->setToolTip("Mount SFTP/FTP/Samba directory locally");
    connect(m_actRemoteMount, &QAction::triggered, this, &MainWindow::onRemoteMount);
    addAction(m_actRemoteMount);

    m_actCloudMount = new QAction("Rclone Cloud Mounts...", this);
    m_actCloudMount->setToolTip("Configure and mount Google Drive, OneDrive, or Dropbox");
    connect(m_actCloudMount, &QAction::triggered, this, &MainWindow::onCloudMount);
    addAction(m_actCloudMount);

    m_actImageConvert = new QAction("Batch Image Converter...", this);
    m_actImageConvert->setToolTip("Bulk format conversion and resizing for images");
    connect(m_actImageConvert, &QAction::triggered, this, &MainWindow::onImageConvert);
    addAction(m_actImageConvert);

    m_actProcessManager = new QAction("System Monitor...", this);
    m_actProcessManager->setToolTip("Monitor CPU/Memory and manage running processes");
    connect(m_actProcessManager, &QAction::triggered, this, &MainWindow::onProcessManagerAction);
    addAction(m_actProcessManager);

    m_actEncryptVault = new QAction("Encrypt into Vault...", this);
    m_actEncryptVault->setToolTip("Secure password-lock directories/files using AES-256");
    connect(m_actEncryptVault, &QAction::triggered, this, &MainWindow::onEncryptVault);
    addAction(m_actEncryptVault);

    m_actDecryptVault = new QAction("Decrypt Vault...", this);
    m_actDecryptVault->setToolTip("Unlock password-protected secure vaults");
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
    m_menuEdit->addSeparator();
    m_menuEdit->addAction(m_actDelete);
    m_menuEdit->addAction(m_actRename);
    m_menuEdit->addAction(m_actBulkRename);
    m_menuEdit->addSeparator();
    m_menuEdit->addAction(m_actKeybindings);

    m_menuView = menuBar()->addMenu("View");
    m_menuView->addAction(m_actToggleDualPane);
    m_menuView->addAction(m_actTogglePreview);
    m_menuView->addAction(m_actMutePreview);
    m_menuView->addAction(m_actToggleAgeColoring);
    m_menuView->addAction(m_actConfigureAgeStyling);
    m_menuView->addAction(m_actToggleDrivesMenu);
    m_menuView->addAction(m_actToggleDrivesToolbar);
    m_menuView->addAction(m_actToggleFavoritesSidebar);
    m_menuView->addAction(m_actToggleConsole);
    m_menuView->addAction(m_actToggleFlatView);
    m_menuView->addSeparator();
    m_menuView->addAction(m_actToggleArchiveNav);
    m_menuView->addAction(m_actToggleCasingOverlays);
    m_menuView->addAction(m_actShowAudioCoverArt);
    m_menuView->addAction(m_actToggleSpectrum);
    
    m_menuView->addSeparator();
    QMenu* menuSaveLayout = m_menuView->addMenu("Save Layout");
    menuSaveLayout->addAction(m_actAutoSaveLayout);
    menuSaveLayout->addAction(m_actSaveLayoutNow);
    menuSaveLayout->addAction(m_actResetLayout);
    
    QMenu* menuFilterToggles = m_menuView->addMenu("Filter Bars");
    menuFilterToggles->addAction(m_actLeftShowFilterText);
    menuFilterToggles->addAction(m_actLeftShowCategoryButtons);
    menuFilterToggles->addAction(m_actRightShowFilterText);
    menuFilterToggles->addAction(m_actRightShowCategoryButtons);

    m_menuView->addSeparator();
    m_menuView->addAction(m_actRefresh);

    m_menuFavorites = menuBar()->addMenu("Favorites");
    m_menuDrives = menuBar()->addMenu("Drives");

    m_menuTools = menuBar()->addMenu("Tools");
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
    m_menuTools->addAction(m_actConfigureFolderLayouts);
    m_menuTools->addSeparator();
    m_menuTools->addAction(m_actEncryptVault);
    m_menuTools->addAction(m_actDecryptVault);

    m_menuSearch = menuBar()->addMenu("Search");
    m_menuSearch->addAction("Save Current Search as Preset...", this, &MainWindow::onSaveSearchPreset);
    m_menuSearchPresets = m_menuSearch->addMenu("Saved Presets");
    connect(m_menuSearchPresets, &QMenu::aboutToShow, this, &MainWindow::updateSearchPresetsMenu);

    m_menuHelp = menuBar()->addMenu("Help");
    m_menuHelp->addAction(m_actShowHelp);
    m_menuHelp->addSeparator();
    m_menuHelp->addAction("About Amifiles", this, [this]() {
        QMessageBox::about(this, "About Amifiles", 
                           "<h3>Amifiles v1.0</h3>"
                           "<p>A modern Directory Opus clone for Linux.</p>"
                           "<p>Supports dual-pane browsing, real-time filtering, "
                           "dynamic toolbars, text quick edits, folder artwork detection, "
                           "and media file metadata previews.</p>");
    });
}

void MainWindow::setupToolbars() {
    // Toolbar 1: File Operations
    m_tbFile = addToolBar("File Operations");
    m_tbFile->setObjectName("fileToolBar");
    m_tbFile->setMovable(true);
    m_tbFile->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_tbFile->addAction(m_actNewFolder);
    m_tbFile->addAction(m_actCopy);
    m_tbFile->addAction(m_actCut);
    m_tbFile->addAction(m_actPaste);
    m_tbFile->addAction(m_actCopyToSibling);
    m_tbFile->addAction(m_actMoveToSibling);
    m_tbFile->addAction(m_actDelete);
    m_tbFile->addAction(m_actRefresh);
    m_tbFile->addAction(m_actBulkRename);
    m_tbFile->addAction(m_actProperties);

    // Toolbar 2: Views/Layouts
    m_tbView = addToolBar("Layout Controls");
    m_tbView->setObjectName("viewToolBar");
    m_tbView->setMovable(true);
    m_tbView->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_tbView->addAction(m_actToggleDualPane);
    m_tbView->addAction(m_actTogglePreview);
    m_tbView->addAction(m_actToggleAgeColoring);

    // Toolbar 3: Custom script command buttons
    m_customToolBar = addToolBar("Custom Commands");
    m_customToolBar->setObjectName("customToolBar");
    m_customToolBar->setMovable(true);
    m_customToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_customToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_customToolBar, &QToolBar::customContextMenuRequested, this, &MainWindow::onCustomToolBarContextMenu);

    // Toolbar 4: Drives Toolbar
    m_tbDrives = addToolBar("Drives");
    m_tbDrives->setObjectName("drivesToolBar");
    m_tbDrives->setMovable(true);
    m_tbDrives->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
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
    }
}

void MainWindow::onFileSelected(const QString& filePath) {
    if (sender() != m_activePanel) return;

    if (filePath.isEmpty()) {
        m_previewPanel->clearPreview();
    } else {
        m_previewPanel->previewFile(filePath);
    }
    updateMiniPlayer();
}

void MainWindow::onFolderArtDetected(const QString& artPath) {
    if (sender() != m_activePanel) return;

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
    
    if (!checked && m_activePanel && m_rightTabWidget && m_rightTabWidget->indexOf(m_activePanel) != -1) {
        onPanelActivated(leftPanel());
    }

    if (m_splitter) {
        int totalWidth = m_splitter->width();
        if (checked) {
            if (m_showPreview) {
                m_splitter->setSizes({totalWidth / 3, totalWidth / 3, totalWidth / 3});
            } else {
                m_splitter->setSizes({totalWidth / 2, totalWidth / 2, 0});
            }
        } else {
            if (m_showPreview) {
                m_splitter->setSizes({totalWidth / 2, 0, totalWidth / 2});
            } else {
                m_splitter->setSizes({totalWidth, 0, 0});
            }
        }
    }
}

void MainWindow::onTogglePreview(bool checked) {
    m_showPreview = checked;
    m_previewPanel->setVisible(checked);
    if (!checked && m_previewPanel && m_previewPanel->player()) {
        m_previewPanel->player()->pause();
    }

    if (m_splitter) {
        int totalWidth = m_splitter->width();
        if (checked) {
            if (m_isDualPane) {
                m_splitter->setSizes({totalWidth / 3, totalWidth / 3, totalWidth / 3});
            } else {
                m_splitter->setSizes({totalWidth / 2, 0, totalWidth / 2});
            }
        } else {
            if (m_isDualPane) {
                m_splitter->setSizes({totalWidth / 2, totalWidth / 2, 0});
            } else {
                m_splitter->setSizes({totalWidth, 0, 0});
            }
        }
    }

    updateMiniPlayer();
}

void MainWindow::updateMiniPlayer() {
    if (!m_previewPanel || !m_miniMediaControls) return;

    QMediaPlayer* player = m_previewPanel->player();
    QUrl source = player->source();
    QString path = source.toLocalFile();
    QFileInfo info(path);
    QString ext = info.suffix().toLower();
    static const QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a" };
    static const QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v" };

    bool isAudio = audioExts.contains(ext);
    bool isVideo = videoExts.contains(ext);
    bool isMedia = isAudio || isVideo;

    if (!m_showPreview && isMedia) {
        m_miniMediaControls->updateTrackInfo(info.fileName(), player->duration(), isVideo);
        m_miniMediaControls->show();
    } else {
        m_miniMediaControls->hide();
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
        
        CopyQueueManager::instance().queueCopy(paths, destDir, false);
        CopyQueueManager::instance().showQueueDialog(this);
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
        
        CopyQueueManager::instance().queueCopy(paths, destDir, true);
        CopyQueueManager::instance().showQueueDialog(this);
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

    QStringList favs = FavoritesManager::instance().getFavorites();
    QMenu* menuRemove = m_menuFavorites->addMenu("Remove from Favorites...");
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
    if (m_consolePanel) {
        m_consolePanel->setVisible(checked);
    }
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("console/visible", checked);
}

void MainWindow::onToggleFlatView(bool checked) {
    if (m_activePanel) {
        m_activePanel->setFlatViewEnabled(checked);
    }
}

void MainWindow::onCompareSyncAction() {
    FilePanel* lp = leftPanel();
    FilePanel* rp = rightPanel();
    if (!lp || !rp) return;

    QString leftPath = lp->currentPath();
    QString rightPath = rp->currentPath();

    FolderSyncDialog dlg(leftPath, rightPath, this);
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
        QAction* selected = menu.exec(m_customToolBar->mapToGlobal(pos));
        if (selected == actAdd) {
            onAddCustomButton();
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
    connect(panel, &FilePanel::folderArtDetected, this, &MainWindow::onFolderArtDetected);
    connect(panel, &FilePanel::pathChanged, this, &MainWindow::onPathChanged);
    connect(panel, &FilePanel::playlistPlayRequested, this, [this](const QStringList& paths) {
        if (m_previewPanel) m_previewPanel->playPlaylist(paths);
    });

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
    m_keybindings["space_analyzer"] = QKeySequence("Ctrl+L");
    m_keybindings["dup_finder"] = QKeySequence("Ctrl+U");
    m_keybindings["help"] = QKeySequence(Qt::Key_F1);

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
    
    addToolBar(Qt::TopToolBarArea, m_tbFile);
    addToolBar(Qt::TopToolBarArea, m_tbView);
    addToolBar(Qt::TopToolBarArea, m_customToolBar);
    addToolBar(Qt::TopToolBarArea, m_tbDrives);
    
    m_tbFile->setVisible(true);
    m_tbView->setVisible(true);
    m_customToolBar->setVisible(true);
    m_tbDrives->setVisible(true);
    
    QMessageBox::information(this, "Reset Layout", "Window layout reset to default. Please restart Amifiles to apply full default geometry.");
}

void MainWindow::loadFolderRules() {
    m_folderRules.clear();
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized = settings.value("folder_layout_rules").toStringList();
    for (const QString& s : serialized) {
        QStringList parts = s.split(';');
        if (parts.size() >= 4) {
            FolderLayoutRule r;
            r.ruleType = parts[0];
            r.value = parts[1];
            r.viewMode = parts[2];
            r.customButtons = parts[3].isEmpty() ? QStringList() : parts[3].split(',');
            m_folderRules.append(r);
        }
    }
}

void MainWindow::saveFolderRules() {
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized;
    for (const auto& r : m_folderRules) {
        QString buttonsJoined = r.customButtons.join(',');
        serialized.append(QString("%1;%2;%3;%4")
                          .arg(r.ruleType)
                          .arg(r.value)
                          .arg(r.viewMode)
                          .arg(buttonsJoined));
    }
    settings.setValue("folder_layout_rules", serialized);
}

void MainWindow::applyFolderRules(const QString& path) {
    if (!m_activePanel) return;

    FolderLayoutRule matchedRule;
    bool foundMatch = false;

    // 1. Exact Path matching
    for (const auto& r : m_folderRules) {
        if (r.ruleType == "Path" && r.value == path) {
            matchedRule = r;
            foundMatch = true;
            break;
        }
    }

    // 2. Category matching
    if (!foundMatch) {
        QString category = detectFolderCategory(path);
        if (!category.isEmpty()) {
            for (const auto& r : m_folderRules) {
                if (r.ruleType == "Category" && r.value == category) {
                    matchedRule = r;
                    foundMatch = true;
                    break;
                }
            }
        }
    }

    if (foundMatch) {
        if (matchedRule.viewMode == "List") {
            m_activePanel->setViewModeGrid(false);
        } else if (matchedRule.viewMode == "Grid") {
            m_activePanel->setViewModeGrid(true);
        }
        
        m_activeToolbarFilter = matchedRule.customButtons;
        rebuildCustomToolBar();
    } else {
        if (!m_activeToolbarFilter.isEmpty()) {
            m_activeToolbarFilter.clear();
            rebuildCustomToolBar();
        }
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

#include "folderlayoutdialog.h"

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

#include "mainwindow.moc"
