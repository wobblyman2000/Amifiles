#include "preferencesdialog.h"
#include "mainwindow.h"
#include "agestylingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QScrollArea>
#include <QMessageBox>
#include <QSpinBox>
#include <QSlider>
#include <QColorDialog>
#include <QInputDialog>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGroupBox>
#include "theme.h"

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Preferences & System Settings");
    resize(640, 420);
    setupUI();
    loadPreferences();
}

void PreferencesDialog::setupUI() {
    // Apply Catppuccin Styling System
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QListWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 6px; padding: 5px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 4px; color: #a6adc8; }"
        "QListWidget::item:hover { background-color: #313244; color: #f5c2e7; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
        "QCheckBox { color: #cdd6f4; font-size: 13px; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid #45475a; background: #11111b; }"
        "QCheckBox::indicator:checked { background: #89b4fa; border-color: #89b4fa; image: url(:/icons/check.png); }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QLineEdit { background-color: #181825; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 6px 10px; }"
        "QPushButton { border: none; background-color: #313244; color: #cdd6f4; padding: 8px 16px; border-radius: 4px; font-weight: bold; min-width: 80px; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton:pressed { background-color: #585b70; }"
        "QFrame#line { border: 1px solid #313244; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // Sidebar Category Panel
    m_listCategory = new QListWidget(this);
    m_listCategory->setFixedWidth(160);
    m_listCategory->addItems({
        "General",
        "View & Colors",
        "Theme Studio",
        "Archives & VFS",
        "Media Preview",
        "Services",
        "Keyboard Shortcuts"
    });
    mainLayout->addWidget(m_listCategory);

    // Settings Stack Sheet
    m_stackPages = new QStackedWidget(this);

    // ----------------------------------------------------
    // Page 1: General Options
    // ----------------------------------------------------
    QWidget* pageGeneral = new QWidget(this);
    QVBoxLayout* layGen = new QVBoxLayout(pageGeneral);
    layGen->setContentsMargins(10, 10, 10, 10);
    layGen->setSpacing(15);

    QLabel* lblGenTitle = new QLabel("General & Workspace Settings", this);
    lblGenTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layGen->addWidget(lblGenTitle);

    m_chkAutoSaveLayout = new QCheckBox("Auto-save current layout on close", this);
    m_chkAutoSaveLayout->setToolTip("Persists panel split sizes, views, and active tabs dynamically on exit.");
    layGen->addWidget(m_chkAutoSaveLayout);

    m_chkHorizontalSplit = new QCheckBox("Stack panels vertically (Horizontal Split)", this);
    m_chkHorizontalSplit->setToolTip("Lays out panels vertically (Right pane on Top, Left pane on Bottom).");
    layGen->addWidget(m_chkHorizontalSplit);

    m_chkDrivesToolbar = new QCheckBox("Show Drives Navigation Toolbar", this);
    m_chkDrivesToolbar->setToolTip("Toggles horizontal bar listing mounted drive shortcuts above panels.");
    layGen->addWidget(m_chkDrivesToolbar);

    m_chkDrivesMenu = new QCheckBox("Populate Drives in Main Menu bar", this);
    m_chkDrivesMenu->setToolTip("Integrates drop-down shortcuts of all partitions under the Drives menu.");
    layGen->addWidget(m_chkDrivesMenu);

    m_chkStickyFilters = new QCheckBox("Remember active category filters during folder navigation (Sticky Filters)", this);
    m_chkStickyFilters->setToolTip("Keeps your checked file type filters active when navigating through directories.");
    layGen->addWidget(m_chkStickyFilters);

    m_chkAlwaysCenterSplitter = new QCheckBox("Always keep dual panels centered (50/50 balance)", this);
    m_chkAlwaysCenterSplitter->setToolTip("Locks dual file pane layout to always occupy exactly 50% width/height each when resizing or toggling side panels.");
    layGen->addWidget(m_chkAlwaysCenterSplitter);

    m_chkAlwaysCenterPreview = new QCheckBox("Always keep preview panel at 50/50 split", this);
    m_chkAlwaysCenterPreview->setToolTip("Forces the file preview panel dock to occupy exactly 50% width/height of the window area.");
    layGen->addWidget(m_chkAlwaysCenterPreview);

    m_chkLockLayoutInShowcase = new QCheckBox("Lock layout in fullscreen showcase views", this);
    m_chkLockLayoutInShowcase->setToolTip("Disables and hides the standard dual pane and preview dock when viewing directories in Showcase modes (Movie, TV, Music, etc.).");
    layGen->addWidget(m_chkLockLayoutInShowcase);

    m_chkDetailsFullRowSelect = new QCheckBox("Select full row in Details Table view", this);
    m_chkDetailsFullRowSelect->setToolTip("If unchecked, selecting items only highlights the first column and clicking empty columns deselects selection, allowing you to paste items directly into the current folder.");
    layGen->addWidget(m_chkDetailsFullRowSelect);

    m_chkEnableSmartHome = new QCheckBox("Enable Smart Home Dashboard (smart://home)", this);
    m_chkEnableSmartHome->setToolTip("If enabled, Amifiles defaults to the Smart Home Dashboard. If disabled, clicking Home or entering smart://home loads your local physical home directory.");
    layGen->addWidget(m_chkEnableSmartHome);

    m_chkShowHiddenFiles = new QCheckBox("Show Hidden Files & Folders", this);
    m_chkShowHiddenFiles->setToolTip("Show files and folders that have the hidden attribute or start with a dot (Ctrl+H).");
    layGen->addWidget(m_chkShowHiddenFiles);

    layGen->addStretch(1);
    m_stackPages->addWidget(pageGeneral);

    // ----------------------------------------------------
    // Page 2: View & Colors Badges
    // ----------------------------------------------------
    QWidget* pageView = new QWidget(this);
    QVBoxLayout* layView = new QVBoxLayout(pageView);
    layView->setContentsMargins(10, 10, 10, 10);
    layView->setSpacing(15);

    QLabel* lblViewTitle = new QLabel("View Styles & Badge Rules", this);
    lblViewTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layView->addWidget(lblViewTitle);

    m_chkAgeColoring = new QCheckBox("Enable File Age Styling & Emojis", this);
    m_chkAgeColoring->setToolTip("Toggles text highlights and emoji icons on list views based on file date.");
    layView->addWidget(m_chkAgeColoring);

    m_btnConfigureAgeRules = new QPushButton("Configure Age Threshold Badges...", this);
    m_btnConfigureAgeRules->setToolTip("Open custom age badge styler for day filters and emojis.");
    m_btnConfigureAgeRules->setStyleSheet("QPushButton { background-color: #fab387; color: #11111b; } QPushButton:hover { background-color: #f9e2af; }");
    layView->addWidget(m_btnConfigureAgeRules);
    connect(m_btnConfigureAgeRules, &QPushButton::clicked, this, &PreferencesDialog::onConfigureAgeRules);

    layView->addStretch(1);
    m_stackPages->addWidget(pageView);

    // ----------------------------------------------------
    // Page 3: Theme Studio
    // ----------------------------------------------------
    QWidget* pageTheme = new QWidget(this);
    QVBoxLayout* layThemeMain = new QVBoxLayout(pageTheme);
    layThemeMain->setContentsMargins(10, 10, 10, 10);
    layThemeMain->setSpacing(8);

    QLabel* lblThemeTitle = new QLabel("Application Theme Customizer", this);
    lblThemeTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layThemeMain->addWidget(lblThemeTitle);

    QScrollArea* themeScroll = new QScrollArea(this);
    themeScroll->setWidgetResizable(true);
    themeScroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    QWidget* themeScrollContent = new QWidget(this);
    QVBoxLayout* layTheme = new QVBoxLayout(themeScrollContent);
    layTheme->setContentsMargins(0, 0, 0, 0);
    layTheme->setSpacing(12);

    // Group 1: Presets & Layout Metrics
    QGroupBox* presetGroup = new QGroupBox("Presets & Layout Metrics", this);
    presetGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #b4befe; }");
    QGridLayout* presetLayout = new QGridLayout(presetGroup);
    presetLayout->setSpacing(8);

    presetLayout->addWidget(new QLabel("Theme Preset:", this), 0, 0);
    m_comboThemePreset = new QComboBox(this);
    presetLayout->addWidget(m_comboThemePreset, 0, 1);

    m_btnSaveThemePreset = new QPushButton("Save Custom...", this);
    m_btnSaveThemePreset->setToolTip("Save the current color configuration as a new custom theme template");
    m_btnSaveThemePreset->setStyleSheet("QPushButton { background-color: #fab387; color: #11111b; }");
    presetLayout->addWidget(m_btnSaveThemePreset, 0, 2);

    m_btnDeleteThemePreset = new QPushButton("Delete Preset", this);
    m_btnDeleteThemePreset->setToolTip("Delete the currently highlighted custom theme template file");
    m_btnDeleteThemePreset->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; }");
    presetLayout->addWidget(m_btnDeleteThemePreset, 0, 3);

    presetLayout->addWidget(new QLabel("Font Size (px):", this), 1, 0);
    m_spinThemeFontSize = new QSpinBox(this);
    m_spinThemeFontSize->setRange(8, 20);
    presetLayout->addWidget(m_spinThemeFontSize, 1, 1);

    presetLayout->addWidget(new QLabel("Border Radius (px):", this), 2, 0);
    m_spinThemeBorderRadius = new QSpinBox(this);
    m_spinThemeBorderRadius->setRange(0, 16);
    presetLayout->addWidget(m_spinThemeBorderRadius, 2, 1);

    presetLayout->addWidget(new QLabel("Sidebar Opacity:", this), 3, 0);
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    m_sliderThemeOpacity = new QSlider(Qt::Horizontal, this);
    m_sliderThemeOpacity->setRange(10, 100);
    m_lblThemeOpacityVal = new QLabel("100%", this);
    opacityLayout->addWidget(m_sliderThemeOpacity);
    opacityLayout->addWidget(m_lblThemeOpacityVal);
    presetLayout->addLayout(opacityLayout, 3, 1);

    layTheme->addWidget(presetGroup);

    // Group 2: Color Palette
    QGroupBox* colorGroup = new QGroupBox("Custom Palette Color Config (Only for 'Custom' / Preset overrides)", this);
    colorGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #fab387; }");
    QGridLayout* colorLayout = new QGridLayout(colorGroup);
    colorLayout->setSpacing(8);

    colorLayout->addWidget(new QLabel("Background Color:", this), 0, 0);
    m_btnThemeBg = createThemeColorButton("#1e1e2e", "theme/custom_bg", pageTheme);
    colorLayout->addWidget(m_btnThemeBg, 0, 1);

    colorLayout->addWidget(new QLabel("Sidebar Color:", this), 0, 2);
    m_btnThemeSidebar = createThemeColorButton("#181825", "theme/custom_sidebar", pageTheme);
    colorLayout->addWidget(m_btnThemeSidebar, 0, 3);

    colorLayout->addWidget(new QLabel("Border Color:", this), 1, 0);
    m_btnThemeBorder = createThemeColorButton("#313244", "theme/custom_border", pageTheme);
    colorLayout->addWidget(m_btnThemeBorder, 1, 1);

    colorLayout->addWidget(new QLabel("Accent Color:", this), 1, 2);
    m_btnThemeAccent = createThemeColorButton("#89b4fa", "theme/custom_accent", pageTheme);
    colorLayout->addWidget(m_btnThemeAccent, 1, 3);

    colorLayout->addWidget(new QLabel("Success/Green Color:", this), 2, 0);
    m_btnThemeGreen = createThemeColorButton("#a6e3a1", "theme/custom_green", pageTheme);
    colorLayout->addWidget(m_btnThemeGreen, 2, 1);

    colorLayout->addWidget(new QLabel("Text Color:", this), 2, 2);
    m_btnThemeText = createThemeColorButton("#cdd6f4", "theme/custom_text", pageTheme);
    colorLayout->addWidget(m_btnThemeText, 2, 3);

    colorLayout->addWidget(new QLabel("Secondary Text:", this), 3, 0);
    m_btnThemeSecText = createThemeColorButton("#a6adc8", "theme/custom_sec_text", pageTheme);
    colorLayout->addWidget(m_btnThemeSecText, 3, 1);

    colorLayout->addWidget(new QLabel("Hover Color:", this), 3, 2);
    m_btnThemeHover = createThemeColorButton("#45475a", "theme/custom_hover", pageTheme);
    colorLayout->addWidget(m_btnThemeHover, 3, 3);

    layTheme->addWidget(colorGroup);
    layTheme->addStretch(1);

    themeScroll->setWidget(themeScrollContent);
    layThemeMain->addWidget(themeScroll);
    m_stackPages->addWidget(pageTheme);

    // Connections
    connect(m_comboThemePreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PreferencesDialog::onThemePresetChanged);
    connect(m_sliderThemeOpacity, &QSlider::valueChanged, this, [this](int val) {
        m_lblThemeOpacityVal->setText(QString("%1%").arg(val));
    });
    connect(m_btnSaveThemePreset, &QPushButton::clicked, this, &PreferencesDialog::onSaveThemePresetClicked);
    connect(m_btnDeleteThemePreset, &QPushButton::clicked, this, &PreferencesDialog::onDeleteThemePresetClicked);

    // ----------------------------------------------------
    // Page 3: Archives & Disk Images
    // ----------------------------------------------------
    QWidget* pageArchives = new QWidget(this);
    QVBoxLayout* layArch = new QVBoxLayout(pageArchives);
    layArch->setContentsMargins(10, 10, 10, 10);
    layArch->setSpacing(15);

    QLabel* lblArchTitle = new QLabel("Archive VFS & Disk Image Navigation", this);
    lblArchTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layArch->addWidget(lblArchTitle);

    m_chkArchiveNav = new QCheckBox("Enable Virtual Archive Navigation (VFS)", this);
    m_chkArchiveNav->setToolTip("Allows double-clicking archives/disk images to browse contents like directories.");
    layArch->addWidget(m_chkArchiveNav);

    m_chkArchiveWrite = new QCheckBox("Allow modifications to Archives / Disk Images (Write privileges)", this);
    m_chkArchiveWrite->setToolTip("Permits drag-and-drop file additions and deletions inside ZIP, Tar, ADF, and D64 files.");
    layArch->addWidget(m_chkArchiveWrite);

    layArch->addStretch(1);
    m_stackPages->addWidget(pageArchives);

    // ----------------------------------------------------
    // Page 4: Media & Preview Panel Options
    // ----------------------------------------------------
    QWidget* pageMedia = new QWidget(this);
    QVBoxLayout* layMedia = new QVBoxLayout(pageMedia);
    layMedia->setContentsMargins(10, 10, 10, 10);
    layMedia->setSpacing(15);

    QLabel* lblMediaTitle = new QLabel("Media Playback & Preview Options", this);
    lblMediaTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layMedia->addWidget(lblMediaTitle);

    m_chkCasingOverlays = new QCheckBox("Enable DVD/CD Case Overlays on Cover Art", this);
    m_chkCasingOverlays->setToolTip("Appends beautiful glassmorphic physical cover overlays on album directories.");
    layMedia->addWidget(m_chkCasingOverlays);

    m_chkShowMetadataHoverCard = new QCheckBox("Display popup Metadata Hover Cards on file hover", this);
    m_chkShowMetadataHoverCard->setToolTip("Paints interactive floating card details listing metadata summaries on mouse hovers.");
    layMedia->addWidget(m_chkShowMetadataHoverCard);

    m_chkAudioCoverArt = new QCheckBox("Display Audio Cover Art in Preview dock", this);
    m_chkAudioCoverArt->setToolTip("Searches and renders metadata track covers when playing music.");
    layMedia->addWidget(m_chkAudioCoverArt);

    m_chkSpectrumVisualizer = new QCheckBox("Render real-time Audio Spectrum visualizer", this);
    m_chkSpectrumVisualizer->setToolTip("Paints interactive bar charts mapping frequency ranges.");
    layMedia->addWidget(m_chkSpectrumVisualizer);

    m_chkMutePreview = new QCheckBox("Mute audio preview playback by default", this);
    m_chkMutePreview->setToolTip("Mutes audio tracks when double-clicking files.");
    layMedia->addWidget(m_chkMutePreview);

    m_chkBuiltinPlayerDoubleclick = new QCheckBox("Double-click plays media in built-in player", this);
    m_chkBuiltinPlayerDoubleclick->setToolTip("Built-in player starts playback in borderless fullscreen window upon double-clicks.");
    layMedia->addWidget(m_chkBuiltinPlayerDoubleclick);

    m_chkDoubleclickAddsToQueue = new QCheckBox("Double-click adds media to playlist queue", this);
    m_chkDoubleclickAddsToQueue->setToolTip("Double-clicking files or folders adds them to the play queue instead of opening them fullscreen.");
    layMedia->addWidget(m_chkDoubleclickAddsToQueue);

    connect(m_chkBuiltinPlayerDoubleclick, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_chkDoubleclickAddsToQueue->setChecked(false);
    });
    connect(m_chkDoubleclickAddsToQueue, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_chkBuiltinPlayerDoubleclick->setChecked(false);
    });

    m_chkAutoFullscreen = new QCheckBox("Auto-open full screen media player on playback", this);
    m_chkAutoFullscreen->setToolTip("Opens the full screen borderless media player HUD automatically when starting audio/video playback.");
    layMedia->addWidget(m_chkAutoFullscreen);

    m_chkAutoPlayThemeMusic = new QCheckBox("Auto-play TV series background theme music (theme.mp3) in Video Showcase", this);
    m_chkAutoPlayThemeMusic->setToolTip("Automatically plays theme.mp3 background audio when entering TV show folders in Video Showcase mode.");
    layMedia->addWidget(m_chkAutoPlayThemeMusic);

    m_chkRememberVideoProgress = new QCheckBox("Remember and resume video playback progress (TV shows & movies)", this);
    m_chkRememberVideoProgress->setToolTip("Saves progress periodically and prompts to resume when reopening TV shows or movie files.");
    layMedia->addWidget(m_chkRememberVideoProgress);

    m_chkKeyboardRemoteMode = new QCheckBox("Enable Remote Control Navigation Mode (Flirc USB / Keyboard)", this);
    m_chkKeyboardRemoteMode->setToolTip("Allows navigating the main UI, Showcase, and full screen players using remote controls/keyboards.");
    layMedia->addWidget(m_chkKeyboardRemoteMode);

    m_chkAutoQueueSiblings = new QCheckBox("Auto-Queue Sibling Files inside Folders", this);
    m_chkAutoQueueSiblings->setToolTip("Automatically populates the playlist queue with all other audio/video files in the same directory upon playback start.");
    layMedia->addWidget(m_chkAutoQueueSiblings);

    m_chkShowFolderLabel = new QCheckBox("Display 'Folder' text label for directories in Showcase views", this);
    m_chkShowFolderLabel->setToolTip("Displays the word 'Folder' in small text below directory titles in Theater/Showcase views.");
    layMedia->addWidget(m_chkShowFolderLabel);

    QFrame* lineMedia = new QFrame(this);
    lineMedia->setFrameShape(QFrame::HLine);
    lineMedia->setFrameShadow(QFrame::Sunken);
    lineMedia->setStyleSheet("border: 1px solid #313244;");
    layMedia->addWidget(lineMedia);

    QFormLayout* formMedia = new QFormLayout();
    formMedia->setSpacing(10);
    m_editHidePatterns = new QLineEdit(this);
    m_editHidePatterns->setToolTip("Enter comma-separated wildcard patterns of auxiliary files (e.g. folder.jpg, *.nfo, *.txt) to hide in Theater View.");
    formMedia->addRow("Hide files matching in Theater View:", m_editHidePatterns);
    layMedia->addLayout(formMedia);

    layMedia->addStretch(1);
    m_stackPages->addWidget(pageMedia);

    // ----------------------------------------------------
    // Page 5: Services & API keys
    // ----------------------------------------------------
    QWidget* pageServices = new QWidget(this);
    QVBoxLayout* layServ = new QVBoxLayout(pageServices);
    layServ->setContentsMargins(10, 10, 10, 10);
    layServ->setSpacing(15);

    QLabel* lblServTitle = new QLabel("Online Services & Integration API Keys", this);
    lblServTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layServ->addWidget(lblServTitle);

    QFormLayout* formServ = new QFormLayout();
    formServ->setSpacing(12);

    m_editTmdbApiKey = new QLineEdit(this);
    m_editTmdbApiKey->setPlaceholderText("Enter your free TMDb API Key...");
    m_editTmdbApiKey->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    formServ->addRow("TMDb API Key:", m_editTmdbApiKey);

    layServ->addLayout(formServ);

    QLabel* lblServNote = new QLabel(
        "Note: TVmaze is used by default and does not require an API Key. "
        "A TMDb API Key is optional but required for movie metadata. "
        "Get a free key by signing up at: <a href=\"https://www.themoviedb.org/\" style=\"color: #89b4fa;\">themoviedb.org</a>",
        this
    );
    lblServNote->setOpenExternalLinks(true);
    lblServNote->setWordWrap(true);
    lblServNote->setStyleSheet("color: #a6adc8; font-size: 12px; line-height: 1.4;");
    layServ->addWidget(lblServNote);

    layServ->addStretch(1);
    m_stackPages->addWidget(pageServices);

    // ----------------------------------------------------
    // Page 6: Keyboard & Remote Shortcuts Mapping
    // ----------------------------------------------------
    QWidget* pageShortcuts = new QWidget(this);
    QVBoxLayout* layShortcuts = new QVBoxLayout(pageShortcuts);
    layShortcuts->setContentsMargins(10, 10, 10, 10);
    layShortcuts->setSpacing(12);

    QLabel* lblShortcutsTitle = new QLabel("Keyboard & Remote Control Shortcuts", this);
    lblShortcutsTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #89b4fa;");
    layShortcuts->addWidget(lblShortcutsTitle);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QScrollArea::NoFrame);
    scrollArea->setStyleSheet("background-color: transparent;");
    
    QWidget* scrollContent = new QWidget(this);
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(15);

    // Section 1: Showcase / Theater View Shortcuts
    QLabel* lblSec1 = new QLabel("Showcase / Theater View Navigation", this);
    lblSec1->setStyleSheet("font-weight: bold; color: #f5c2e7; font-size: 14px;");
    scrollLayout->addWidget(lblSec1);

    QFormLayout* formSec1 = new QFormLayout();
    formSec1->setSpacing(8);

    m_keyPlayCollection = new QKeySequenceEdit(this);
    m_keyPlayCollection->setToolTip("Shortcut to play the entire TV show / album / folder recursively.");
    formSec1->addRow("Play Collection:", m_keyPlayCollection);

    m_keyInfoSheet = new QKeySequenceEdit(this);
    m_keyInfoSheet->setToolTip("Shortcut to open the media info sheet.");
    formSec1->addRow("Open Info Sheet:", m_keyInfoSheet);

    m_keyScrapeMeta = new QKeySequenceEdit(this);
    m_keyScrapeMeta->setToolTip("Shortcut to scrape video metadata online.");
    formSec1->addRow("Scrape Video Metadata:", m_keyScrapeMeta);

    m_keyApplyCasing = new QKeySequenceEdit(this);
    m_keyApplyCasing->setToolTip("Shortcut to automatically rename folder.jpg and apply DVD overlay.");
    formSec1->addRow("Apply DVD Case Overlay:", m_keyApplyCasing);

    m_keyToggleDrawer = new QKeySequenceEdit(this);
    m_keyToggleDrawer->setToolTip("Shortcut to toggle the playlist track drawer.");
    formSec1->addRow("Toggle Playlist Drawer:", m_keyToggleDrawer);

    m_keyNavigateUp = new QKeySequenceEdit(this);
    m_keyNavigateUp->setToolTip("Shortcut to navigate to the parent folder.");
    formSec1->addRow("Navigate Up (Parent):", m_keyNavigateUp);

    m_keyNavigateBack = new QKeySequenceEdit(this);
    m_keyNavigateBack->setToolTip("Shortcut to go back in history.");
    formSec1->addRow("Navigate Back (History):", m_keyNavigateBack);

    scrollLayout->addLayout(formSec1);

    // Separator line
    QFrame* lineShortcuts = new QFrame(this);
    lineShortcuts->setFrameShape(QFrame::HLine);
    lineShortcuts->setFrameShadow(QFrame::Sunken);
    lineShortcuts->setStyleSheet("border: 1px solid #313244;");
    scrollLayout->addWidget(lineShortcuts);

    // Section 2: Fullscreen Media Player Control Shortcuts
    QLabel* lblSec2 = new QLabel("Fullscreen Media Player Controls", this);
    lblSec2->setStyleSheet("font-weight: bold; color: #a6e3a1; font-size: 14px;");
    scrollLayout->addWidget(lblSec2);

    QFormLayout* formSec2 = new QFormLayout();
    formSec2->setSpacing(8);

    m_keyPlayerPlayPause = new QKeySequenceEdit(this);
    m_keyPlayerPlayPause->setToolTip("Shortcut to toggle play/pause.");
    formSec2->addRow("Play / Pause:", m_keyPlayerPlayPause);

    m_keyPlayerPrev = new QKeySequenceEdit(this);
    m_keyPlayerPrev->setToolTip("Shortcut to play the previous track/episode.");
    formSec2->addRow("Previous Track:", m_keyPlayerPrev);

    m_keyPlayerNext = new QKeySequenceEdit(this);
    m_keyPlayerNext->setToolTip("Shortcut to play the next track/episode.");
    formSec2->addRow("Next Track:", m_keyPlayerNext);

    m_keyPlayerMute = new QKeySequenceEdit(this);
    m_keyPlayerMute->setToolTip("Shortcut to toggle mute state.");
    formSec2->addRow("Mute / Unmute:", m_keyPlayerMute);

    m_keyPlayerMenu = new QKeySequenceEdit(this);
    m_keyPlayerMenu->setToolTip("Shortcut to display the player context menu (chapters list, exit, controls).");
    formSec2->addRow("Open Player Menu:", m_keyPlayerMenu);

    m_keyPlayerPlaylist = new QKeySequenceEdit(this);
    m_keyPlayerPlaylist->setToolTip("Shortcut to toggle the playlist view in fullscreen mode.");
    formSec2->addRow("Toggle Fullscreen Playlist:", m_keyPlayerPlaylist);

    m_keyPlayFolder = new QKeySequenceEdit(this);
    m_keyPlayFolder->setToolTip("Shortcut to play all media files in the current/selected folder.");
    formSec2->addRow("Play Entire Folder:", m_keyPlayFolder);

    m_keyQueueFolder = new QKeySequenceEdit(this);
    m_keyQueueFolder->setToolTip("Shortcut to queue all media files in the folder to the playlist.");
    formSec2->addRow("Queue Folder to Playlist:", m_keyQueueFolder);

    m_keyPlayQueue = new QKeySequenceEdit(this);
    m_keyPlayQueue->setToolTip("Shortcut to start playback of the playlist queue.");
    formSec2->addRow("Play Playlist Queue:", m_keyPlayQueue);

    scrollLayout->addLayout(formSec2);
    scrollLayout->addStretch(1);

    scrollArea->setWidget(scrollContent);
    layShortcuts->addWidget(scrollArea);
    m_stackPages->addWidget(pageShortcuts);

    // Right Layout
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(m_stackPages);

    // Horizontal Split Line separator
    QFrame* line = new QFrame(this);
    line->setObjectName("line");
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    rightLayout->addWidget(line);

    // Bottom Actions Panel
    QHBoxLayout* layBtns = new QHBoxLayout();
    
    m_btnResetDefaults = new QPushButton("🔄 Reset All to Defaults", this);
    m_btnResetDefaults->setToolTip("Restores all preferences, window geometry, toolbar settings, and layout rules to factory defaults.");
    m_btnResetDefaults->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #eba0ac; }");
    connect(m_btnResetDefaults, &QPushButton::clicked, this, &PreferencesDialog::onResetDefaults);
    layBtns->addWidget(m_btnResetDefaults);

    layBtns->addStretch(1);

    m_btnOk = new QPushButton("OK", this);
    m_btnOk->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnOk, &QPushButton::clicked, this, &PreferencesDialog::onOkClicked);

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnApply = new QPushButton("Apply", this);
    connect(m_btnApply, &QPushButton::clicked, this, &PreferencesDialog::onApplyClicked);

    layBtns->addWidget(m_btnOk);
    layBtns->addWidget(m_btnCancel);
    layBtns->addWidget(m_btnApply);
    rightLayout->addLayout(layBtns);

    mainLayout->addLayout(rightLayout, 1);

    // Default Page
    m_listCategory->setCurrentRow(0);
    connect(m_listCategory, &QListWidget::currentRowChanged, this, &PreferencesDialog::onCategoryChanged);
}

void PreferencesDialog::onCategoryChanged(int index) {
    m_stackPages->setCurrentIndex(index);
}

void PreferencesDialog::setCurrentPage(int pageIndex) {
    if (m_listCategory) {
        m_listCategory->setCurrentRow(pageIndex);
    }
}

void PreferencesDialog::loadPreferences() {
    QSettings settings("Amifiles", "Amifiles");

    m_chkAutoSaveLayout->setChecked(settings.value("layout/auto_save_on_close", true).toBool());
    m_chkHorizontalSplit->setChecked(settings.value("preferences/horizontal_split", false).toBool());
    m_chkDrivesToolbar->setChecked(settings.value("layout/drives_toolbar_visible", true).toBool());
    m_chkDrivesMenu->setChecked(settings.value("layout/drives_menu_visible", true).toBool());
    m_chkStickyFilters->setChecked(settings.value("preferences/sticky_filters", false).toBool());
    m_chkAlwaysCenterSplitter->setChecked(settings.value("preferences/always_center_splitter", true).toBool());
    m_chkAlwaysCenterPreview->setChecked(settings.value("preferences/always_center_preview", true).toBool());
    m_chkLockLayoutInShowcase->setChecked(settings.value("preferences/lock_layout_in_showcase", true).toBool());
    m_chkDetailsFullRowSelect->setChecked(settings.value("preferences/details_full_row_select", true).toBool());
    m_chkEnableSmartHome->setChecked(settings.value("preferences/enable_smart_home", true).toBool());
    m_chkShowHiddenFiles->setChecked(settings.value("preferences/show_hidden_files", false).toBool());

    m_chkAgeColoring->setChecked(settings.value("preferences/age_coloring_enabled", true).toBool());

    m_chkArchiveNav->setChecked(settings.value("preferences/archive_nav", true).toBool());
    m_chkArchiveWrite->setChecked(settings.value("preferences/archive_write", false).toBool());

    m_chkCasingOverlays->setChecked(settings.value("preferences/casing_overlays", true).toBool());
    m_chkShowMetadataHoverCard->setChecked(settings.value("preview/show_metadata_hover_card", true).toBool());
    m_chkAudioCoverArt->setChecked(settings.value("preview/show_audio_cover_art", true).toBool());
    m_chkSpectrumVisualizer->setChecked(settings.value("preview/show_spectrum_visualizer", true).toBool());
    m_chkMutePreview->setChecked(settings.value("preview/muted", false).toBool());
    m_chkBuiltinPlayerDoubleclick->setChecked(settings.value("preferences/builtin_player_doubleclick", false).toBool());
    m_chkDoubleclickAddsToQueue->setChecked(settings.value("preferences/doubleclick_adds_to_queue", false).toBool());
    m_chkAutoFullscreen->setChecked(settings.value("preview/auto_fullscreen", true).toBool());
    m_chkAutoPlayThemeMusic->setChecked(settings.value("theater/auto_play_theme_music", true).toBool());
    m_chkRememberVideoProgress->setChecked(settings.value("preview/resume_progress", false).toBool());
    m_chkKeyboardRemoteMode->setChecked(settings.value("preferences/keyboard_remote_mode", false).toBool());
    m_chkAutoQueueSiblings->setChecked(settings.value("preview/auto_queue_sibling_files", true).toBool());
    m_chkShowFolderLabel->setChecked(settings.value("theater/show_folder_label", true).toBool());

    QString defaultHide = "folder.jpg, folder.jpeg, folder.png, cover.jpg, cover.jpeg, cover.png, fanart.jpg, fanart.jpeg, fanart.png, backdrop.jpg, backdrop.jpeg, backdrop.png, poster.jpg, poster.jpeg, poster.png, *.nfo, *.xml, *.txt, *.srt, *.sub, *.vtt, *.ini, *.db";
    m_editHidePatterns->setText(settings.value("theater/hide_patterns", defaultHide).toString());

    m_editTmdbApiKey->setText(settings.value("services/tmdb_api_key", "").toString());

    m_keyPlayCollection->setKeySequence(QKeySequence(settings.value("shortcuts/play_collection", "Ctrl+Space").toString()));
    m_keyInfoSheet->setKeySequence(QKeySequence(settings.value("shortcuts/info_sheet", "I").toString()));
    m_keyScrapeMeta->setKeySequence(QKeySequence(settings.value("shortcuts/scrape_meta", "M").toString()));
    m_keyApplyCasing->setKeySequence(QKeySequence(settings.value("shortcuts/apply_casing", "D").toString()));
    m_keyToggleDrawer->setKeySequence(QKeySequence(settings.value("shortcuts/toggle_drawer", "P").toString()));
    m_keyNavigateUp->setKeySequence(QKeySequence(settings.value("shortcuts/navigate_up", "Backspace").toString()));
    m_keyNavigateBack->setKeySequence(QKeySequence(settings.value("shortcuts/navigate_back", "Alt+Left").toString()));

    m_keyPlayerPlayPause->setKeySequence(QKeySequence(settings.value("shortcuts/player_play_pause", "Space").toString()));
    m_keyPlayerPrev->setKeySequence(QKeySequence(settings.value("shortcuts/player_prev", "P").toString()));
    m_keyPlayerNext->setKeySequence(QKeySequence(settings.value("shortcuts/player_next", "N").toString()));
    m_keyPlayerMute->setKeySequence(QKeySequence(settings.value("shortcuts/player_mute", "M").toString()));
    m_keyPlayerMenu->setKeySequence(QKeySequence(settings.value("shortcuts/player_menu", "C").toString()));
    m_keyPlayerPlaylist->setKeySequence(QKeySequence(settings.value("shortcuts/player_playlist", "L").toString()));
    m_keyPlayFolder->setKeySequence(QKeySequence(settings.value("shortcuts/play_folder", "Ctrl+Alt+F").toString()));
    m_keyQueueFolder->setKeySequence(QKeySequence(settings.value("shortcuts/queue_folder", "Ctrl+Alt+Q").toString()));
    m_keyPlayQueue->setKeySequence(QKeySequence(settings.value("shortcuts/play_queue", "Ctrl+Alt+Space").toString()));

    // Load Theme Studio
    populateThemePresets();
    m_comboThemePreset->setCurrentText(settings.value("theme/preset", "Catppuccin Mocha").toString());
    m_spinThemeFontSize->setValue(settings.value("theme/font_size", 13).toInt());
    m_spinThemeBorderRadius->setValue(settings.value("theme/border_radius", 4).toInt());
    m_sliderThemeOpacity->setValue(qRound(settings.value("theme/sidebar_opacity", 1.0).toDouble() * 100.0));
    m_lblThemeOpacityVal->setText(QString("%1%").arg(m_sliderThemeOpacity->value()));
    updateThemeControlsState();
}

void PreferencesDialog::savePreferences() {
    QSettings settings("Amifiles", "Amifiles");

    settings.setValue("layout/auto_save_on_close", m_chkAutoSaveLayout->isChecked());
    settings.setValue("preferences/horizontal_split", m_chkHorizontalSplit->isChecked());
    settings.setValue("layout/drives_toolbar_visible", m_chkDrivesToolbar->isChecked());
    settings.setValue("layout/drives_menu_visible", m_chkDrivesMenu->isChecked());
    settings.setValue("preferences/sticky_filters", m_chkStickyFilters->isChecked());
    settings.setValue("preferences/always_center_splitter", m_chkAlwaysCenterSplitter->isChecked());
    settings.setValue("preferences/always_center_preview", m_chkAlwaysCenterPreview->isChecked());
    settings.setValue("preferences/lock_layout_in_showcase", m_chkLockLayoutInShowcase->isChecked());
    settings.setValue("preferences/details_full_row_select", m_chkDetailsFullRowSelect->isChecked());
    settings.setValue("preferences/enable_smart_home", m_chkEnableSmartHome->isChecked());
    settings.setValue("preferences/show_hidden_files", m_chkShowHiddenFiles->isChecked());

    settings.setValue("preferences/age_coloring_enabled", m_chkAgeColoring->isChecked());

    settings.setValue("preferences/archive_nav", m_chkArchiveNav->isChecked());
    settings.setValue("preferences/archive_write", m_chkArchiveWrite->isChecked());

    settings.setValue("preferences/casing_overlays", m_chkCasingOverlays->isChecked());
    settings.setValue("preview/show_metadata_hover_card", m_chkShowMetadataHoverCard->isChecked());
    settings.setValue("preview/show_audio_cover_art", m_chkAudioCoverArt->isChecked());
    settings.setValue("preview/show_spectrum_visualizer", m_chkSpectrumVisualizer->isChecked());
    settings.setValue("preview/muted", m_chkMutePreview->isChecked());
    settings.setValue("preferences/builtin_player_doubleclick", m_chkBuiltinPlayerDoubleclick->isChecked());
    settings.setValue("preferences/doubleclick_adds_to_queue", m_chkDoubleclickAddsToQueue->isChecked());
    settings.setValue("preview/auto_fullscreen", m_chkAutoFullscreen->isChecked());
    settings.setValue("theater/auto_play_theme_music", m_chkAutoPlayThemeMusic->isChecked());
    settings.setValue("preview/resume_progress", m_chkRememberVideoProgress->isChecked());
    settings.setValue("preferences/keyboard_remote_mode", m_chkKeyboardRemoteMode->isChecked());
    settings.setValue("preview/auto_queue_sibling_files", m_chkAutoQueueSiblings->isChecked());
    settings.setValue("theater/show_folder_label", m_chkShowFolderLabel->isChecked());
    settings.setValue("theater/hide_patterns", m_editHidePatterns->text().trimmed());

    settings.setValue("services/tmdb_api_key", m_editTmdbApiKey->text().trimmed());

    settings.setValue("shortcuts/play_collection", m_keyPlayCollection->keySequence().toString());
    settings.setValue("shortcuts/info_sheet", m_keyInfoSheet->keySequence().toString());
    settings.setValue("shortcuts/scrape_meta", m_keyScrapeMeta->keySequence().toString());
    settings.setValue("shortcuts/apply_casing", m_keyApplyCasing->keySequence().toString());
    settings.setValue("shortcuts/toggle_drawer", m_keyToggleDrawer->keySequence().toString());
    settings.setValue("shortcuts/navigate_up", m_keyNavigateUp->keySequence().toString());
    settings.setValue("shortcuts/navigate_back", m_keyNavigateBack->keySequence().toString());

    settings.setValue("shortcuts/player_play_pause", m_keyPlayerPlayPause->keySequence().toString());
    settings.setValue("shortcuts/player_prev", m_keyPlayerPrev->keySequence().toString());
    settings.setValue("shortcuts/player_next", m_keyPlayerNext->keySequence().toString());
    settings.setValue("shortcuts/player_mute", m_keyPlayerMute->keySequence().toString());
    settings.setValue("shortcuts/player_menu", m_keyPlayerMenu->keySequence().toString());
    settings.setValue("shortcuts/player_playlist", m_keyPlayerPlaylist->keySequence().toString());
    settings.setValue("shortcuts/play_folder", m_keyPlayFolder->keySequence().toString());
    settings.setValue("shortcuts/queue_folder", m_keyQueueFolder->keySequence().toString());
    settings.setValue("shortcuts/play_queue", m_keyPlayQueue->keySequence().toString());

    // Save Theme Studio Settings
    settings.setValue("theme/preset", m_comboThemePreset->currentText());
    settings.setValue("theme/font_size", m_spinThemeFontSize->value());
    settings.setValue("theme/border_radius", m_spinThemeBorderRadius->value());
    settings.setValue("theme/sidebar_opacity", m_sliderThemeOpacity->value() / 100.0);

    emit preferencesChanged();

    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (mw) {
        mw->saveFolderRules();
    }
    settings.sync();
}

void PreferencesDialog::onApplyClicked() {
    savePreferences();
}

void PreferencesDialog::onOkClicked() {
    savePreferences();
    accept();
}

void PreferencesDialog::onConfigureAgeRules() {
    AgeStylingDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        emit preferencesChanged();
    }
}

void PreferencesDialog::onResetDefaults() {
    auto res = QMessageBox::question(
        this,
        "Reset All Settings to Factory Defaults",
        "Are you sure you want to completely reset all preferences, layout rules, toolbars, and options to factory defaults?\n\nThis will restore all default settings without needing to manually remove config files.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        QSettings settings("Amifiles", "Amifiles");
        settings.clear();
        settings.sync();

        loadPreferences();
        emit preferencesChanged();

        QMessageBox::information(
            this,
            "Reset Complete",
            "All settings, layout rules, and preferences have been successfully reset to factory defaults!"
        );
    }
}

void PreferencesDialog::populateThemePresets() {
    m_comboThemePreset->blockSignals(true);
    m_comboThemePreset->clear();
    m_comboThemePreset->addItems({
        "Catppuccin Mocha", 
        "Catppuccin Macchiato", 
        "Catppuccin Frappé", 
        "Catppuccin Latte", 
        "Midnight High Contrast", 
        "Cyber Obsidian", 
        "Nordic Frost", 
        "Amiga Workbench Classic", 
        "System Theme", 
        "Custom"
    });

    QDir themesDir(QDir::homePath() + "/.config/Amifiles/themes");
    if (themesDir.exists()) {
        QStringList customThemes = themesDir.entryList({"*.json"}, QDir::Files);
        for (const QString& file : customThemes) {
            QString name = file.left(file.length() - 5);
            if (m_comboThemePreset->findText(name) == -1) {
                m_comboThemePreset->addItem(name);
            }
        }
    }
    m_comboThemePreset->blockSignals(false);
}

QPushButton* PreferencesDialog::createThemeColorButton(const QString& colorHex, const QString& settingsKey, QWidget* parentPage) {
    QPushButton* btn = new QPushButton(parentPage);
    btn->setFixedWidth(100);
    QSettings settings("Amifiles", "Amifiles");
    QString color = settings.value(settingsKey, colorHex).toString();
    btn->setProperty("colorHex", color);
    btn->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(color));
    
    connect(btn, &QPushButton::clicked, this, [this, btn, settingsKey]() {
        chooseThemeColor(btn, settingsKey);
    });
    return btn;
}

void PreferencesDialog::chooseThemeColor(QPushButton* button, const QString& settingsKey) {
    QString currentHex = button->property("colorHex").toString();
    QColor initial(currentHex);
    QColor selected = QColorDialog::getColor(initial, this, "Choose Palette Color");
    if (selected.isValid()) {
        QString newHex = selected.name();
        button->setProperty("colorHex", newHex);
        button->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(newHex));
        
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue(settingsKey, newHex);
        emit preferencesChanged();
    }
}

void PreferencesDialog::onThemePresetChanged(int index) {
    Q_UNUSED(index);
    QString preset = m_comboThemePreset->currentText();
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("theme/preset", preset);

    updateThemeControlsState();
    emit preferencesChanged();
}

void PreferencesDialog::updateThemeControlsState() {
    QString preset = m_comboThemePreset->currentText();
    bool isCustom = (preset == "Custom");

    m_btnThemeBg->setEnabled(isCustom);
    m_btnThemeSidebar->setEnabled(isCustom);
    m_btnThemeBorder->setEnabled(isCustom);
    m_btnThemeAccent->setEnabled(isCustom);
    m_btnThemeGreen->setEnabled(isCustom);
    m_btnThemeText->setEnabled(isCustom);
    m_btnThemeSecText->setEnabled(isCustom);
    m_btnThemeHover->setEnabled(isCustom);

    m_btnDeleteThemePreset->setEnabled(
        preset != "Catppuccin Mocha" &&
        preset != "Catppuccin Macchiato" &&
        preset != "Catppuccin Frappé" &&
        preset != "Catppuccin Latte" &&
        preset != "Midnight High Contrast" &&
        preset != "Cyber Obsidian" &&
        preset != "Nordic Frost" &&
        preset != "Amiga Workbench Classic" &&
        preset != "System Theme" &&
        preset != "Custom"
    );

    Theme::ThemeColors c = Theme::getThemeColors();

    m_btnThemeBg->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.bg));
    m_btnThemeSidebar->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.sidebar));
    m_btnThemeBorder->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.border));
    m_btnThemeAccent->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.accent));
    m_btnThemeGreen->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.green));
    m_btnThemeText->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.text));
    m_btnThemeSecText->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.secText));
    m_btnThemeHover->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #45475a; height: 20px; }").arg(c.hover));
}

void PreferencesDialog::onSaveThemePresetClicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "Save Custom Theme Preset", "Theme Preset Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed();
    QString filename = name;
    filename.replace(QRegularExpression("[^a-zA-Z0-9_\\- ]"), "");
    if (filename.isEmpty()) {
        QMessageBox::critical(this, "Error", "Invalid preset name.");
        return;
    }

    QDir themesDir(QDir::homePath() + "/.config/Amifiles/themes");
    if (!themesDir.exists()) {
        themesDir.mkpath(".");
    }

    QJsonObject obj;
    Theme::ThemeColors c = Theme::getThemeColors();

    obj["name"] = name;
    obj["bg"] = c.bg;
    obj["sidebar"] = c.sidebar;
    obj["border"] = c.border;
    obj["accent"] = c.accent;
    obj["green"] = c.green;
    obj["text"] = c.text;
    obj["secText"] = c.secText;
    obj["hover"] = c.hover;
    obj["fontSize"] = m_spinThemeFontSize->value();
    obj["borderRadius"] = m_spinThemeBorderRadius->value();
    obj["sidebarOpacity"] = m_sliderThemeOpacity->value() / 100.0;

    QString savePath = themesDir.filePath(filename + ".json");
    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();

        QMessageBox::information(this, "Theme Saved", QString("Theme preset '%1' successfully saved!").arg(name));
        populateThemePresets();
        m_comboThemePreset->setCurrentText(filename);
    } else {
        QMessageBox::critical(this, "Error", "Could not save preset file.");
    }
}

void PreferencesDialog::onDeleteThemePresetClicked() {
    QString preset = m_comboThemePreset->currentText();
    QString customThemePath = QDir::homePath() + "/.config/Amifiles/themes/" + preset + ".json";
    if (QFile::exists(customThemePath)) {
        auto res = QMessageBox::question(this, "Delete Preset", QString("Are you sure you want to delete the theme preset '%1'?").arg(preset));
        if (res == QMessageBox::Yes) {
            QFile::remove(customThemePath);
            populateThemePresets();
            m_comboThemePreset->setCurrentIndex(0);
        }
    }
}
