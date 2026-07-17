#include "preferencesdialog.h"
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
        "Archives & VFS",
        "Media Preview"
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

    m_chkAudioCoverArt = new QCheckBox("Display Audio Cover Art in Preview dock", this);
    m_chkAudioCoverArt->setToolTip("Searches and renders metadata track covers when playing music.");
    layMedia->addWidget(m_chkAudioCoverArt);

    m_chkSpectrumVisualizer = new QCheckBox("Render real-time Audio Spectrum visualizer", this);
    m_chkSpectrumVisualizer->setToolTip("Paints interactive bar charts mapping frequency ranges.");
    layMedia->addWidget(m_chkSpectrumVisualizer);

    m_chkMutePreview = new QCheckBox("Mute audio preview playback by default", this);
    m_chkMutePreview->setToolTip("Mutes audio tracks when double-clicking files.");
    layMedia->addWidget(m_chkMutePreview);

    layMedia->addStretch(1);
    m_stackPages->addWidget(pageMedia);

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

void PreferencesDialog::loadPreferences() {
    QSettings settings("Amifiles", "Amifiles");

    m_chkAutoSaveLayout->setChecked(settings.value("layout/auto_save_on_close", true).toBool());
    m_chkHorizontalSplit->setChecked(settings.value("preferences/horizontal_split", false).toBool());
    m_chkDrivesToolbar->setChecked(settings.value("layout/drives_toolbar_visible", true).toBool());
    m_chkDrivesMenu->setChecked(settings.value("layout/drives_menu_visible", true).toBool());

    m_chkAgeColoring->setChecked(settings.value("preferences/age_coloring_enabled", true).toBool());

    m_chkArchiveNav->setChecked(settings.value("preferences/archive_nav", true).toBool());
    m_chkArchiveWrite->setChecked(settings.value("preferences/archive_write", false).toBool());

    m_chkCasingOverlays->setChecked(settings.value("preferences/casing_overlays", true).toBool());
    m_chkAudioCoverArt->setChecked(settings.value("preview/show_audio_cover_art", true).toBool());
    m_chkSpectrumVisualizer->setChecked(settings.value("preview/show_spectrum_visualizer", true).toBool());
    m_chkMutePreview->setChecked(settings.value("preview/muted", false).toBool());
}

void PreferencesDialog::savePreferences() {
    QSettings settings("Amifiles", "Amifiles");

    settings.setValue("layout/auto_save_on_close", m_chkAutoSaveLayout->isChecked());
    settings.setValue("preferences/horizontal_split", m_chkHorizontalSplit->isChecked());
    settings.setValue("layout/drives_toolbar_visible", m_chkDrivesToolbar->isChecked());
    settings.setValue("layout/drives_menu_visible", m_chkDrivesMenu->isChecked());

    settings.setValue("preferences/age_coloring_enabled", m_chkAgeColoring->isChecked());

    settings.setValue("preferences/archive_nav", m_chkArchiveNav->isChecked());
    settings.setValue("preferences/archive_write", m_chkArchiveWrite->isChecked());

    settings.setValue("preferences/casing_overlays", m_chkCasingOverlays->isChecked());
    settings.setValue("preview/show_audio_cover_art", m_chkAudioCoverArt->isChecked());
    settings.setValue("preview/show_spectrum_visualizer", m_chkSpectrumVisualizer->isChecked());
    settings.setValue("preview/muted", m_chkMutePreview->isChecked());

    emit preferencesChanged();
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
