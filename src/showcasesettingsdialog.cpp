#include "showcasesettingsdialog.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSettings>
#include <QMessageBox>

ShowcaseSettingsDialog::ShowcaseSettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Showcase & Full Screen Media Settings");
    resize(580, 520);
    setStyleSheet(Theme::Stylesheet);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Title / Description Header
    QLabel* headerLabel = new QLabel("Customize layout options, card dimensions, and media behaviors across all Classic and Full Screen (v2) Showcase views.", this);
    headerLabel->setWordWrap(true);
    headerLabel->setStyleSheet("color: #a6adc8; font-size: 12px; margin-bottom: 4px;");
    mainLayout->addWidget(headerLabel);

    // 1. Grid & Card Display Settings
    QGroupBox* displayGroup = new QGroupBox("1. Card Sizing & Grid Layout", this);
    QGridLayout* displayGrid = new QGridLayout(displayGroup);
    displayGrid->setSpacing(10);

    displayGrid->addWidget(new QLabel("Showcase Card Zoom / Scale:", this), 0, 0);
    m_cardSizeSlider = new QSlider(Qt::Horizontal, this);
    m_cardSizeSlider->setRange(0, 6);
    m_cardSizeSlider->setToolTip("Adjust poster and album cover card size (0 = Smallest, 6 = Largest)");
    displayGrid->addWidget(m_cardSizeSlider, 0, 1);

    displayGrid->addWidget(new QLabel("Card Grid Spacing (px):", this), 1, 0);
    m_gridSpacingSpin = new QSpinBox(this);
    m_gridSpacingSpin->setRange(4, 40);
    m_gridSpacingSpin->setValue(12);
    m_gridSpacingSpin->setToolTip("Spacing gap between adjacent media cards in the grid layout");
    displayGrid->addWidget(m_gridSpacingSpin, 1, 1);

    displayGrid->addWidget(new QLabel("Ambient Blur / Glow Opacity (%):", this), 2, 0);
    m_ambientBlurSlider = new QSlider(Qt::Horizontal, this);
    m_ambientBlurSlider->setRange(0, 100);
    m_ambientBlurSlider->setToolTip("Opacity level for dynamic blurred album/poster background glow");
    displayGrid->addWidget(m_ambientBlurSlider, 2, 1);

    mainLayout->addWidget(displayGroup);

    // 2. Playback & Media Behaviors
    QGroupBox* mediaGroup = new QGroupBox("2. Playback & Media Integration", this);
    QGridLayout* mediaGrid = new QGridLayout(mediaGroup);
    mediaGrid->setSpacing(10);

    m_checkAutoPlayTheme = new QCheckBox("Auto-play TV Series Theme Music (theme.mp3) on folder enter", this);
    m_checkAutoPlayTheme->setToolTip("Automatically plays TV show background theme songs when navigating inside series folders");
    mediaGrid->addWidget(m_checkAutoPlayTheme, 0, 0, 1, 2);

    mediaGrid->addWidget(new QLabel("TV Theme Song Volume:", this), 1, 0);
    m_themeVolumeSlider = new QSlider(Qt::Horizontal, this);
    m_themeVolumeSlider->setRange(0, 100);
    m_themeVolumeSlider->setToolTip("Volume level for background TV show theme song playback");
    mediaGrid->addWidget(m_themeVolumeSlider, 1, 1);

    m_checkGroupMultiDisc = new QCheckBox("Merge Multi-Disc Music Albums (CD1, CD2, Disc 3) into single collection", this);
    m_checkGroupMultiDisc->setToolTip("Combines subdirectories like CD1, CD2 into a unified album entry inside Audio & Music Showcase views");
    mediaGrid->addWidget(m_checkGroupMultiDisc, 2, 0, 1, 2);

    m_checkShowBottomPanel = new QCheckBox("Show Glassmorphic Bottom Media Information Drawer", this);
    m_checkShowBottomPanel->setToolTip("Displays tracklists, metadata, star ratings, and play controls at bottom of Showcase views");
    mediaGrid->addWidget(m_checkShowBottomPanel, 3, 0, 1, 2);

    mainLayout->addWidget(mediaGroup);

    // 3. Extension Hiding Rules
    QGroupBox* hideGroup = new QGroupBox("3. Hide Companion / Auxiliary Files per View", this);
    QGridLayout* hideGrid = new QGridLayout(hideGroup);
    hideGrid->setSpacing(8);

    hideGrid->addWidget(new QLabel("Movies Full Screen Hide Exts:", this), 0, 0);
    m_editMovieHideExts = new QLineEdit(this);
    m_editMovieHideExts->setPlaceholderText("*.nfo, *.srt, *.jpg, *.png, *.xml");
    hideGrid->addWidget(m_editMovieHideExts, 0, 1);

    hideGrid->addWidget(new QLabel("TV Shows Full Screen Hide Exts:", this), 1, 0);
    m_editTvHideExts = new QLineEdit(this);
    m_editTvHideExts->setPlaceholderText("*.nfo, *.srt, *.jpg, *.png, *.xml, theme.mp3");
    hideGrid->addWidget(m_editTvHideExts, 1, 1);

    hideGrid->addWidget(new QLabel("Music Full Screen Hide Exts:", this), 2, 0);
    m_editMusicHideExts = new QLineEdit(this);
    m_editMusicHideExts->setPlaceholderText("*.jpg, *.png, *.nfo, *.txt, *.sfv");
    hideGrid->addWidget(m_editMusicHideExts, 2, 1);

    mainLayout->addWidget(hideGroup);

    // Dialog Action Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();

    QPushButton* btnReset = new QPushButton("🔄 Reset to Defaults", this);
    btnReset->setStyleSheet("QPushButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; padding: 6px 12px; } QPushButton:hover { background-color: #45475a; }");
    connect(btnReset, &QPushButton::clicked, this, &ShowcaseSettingsDialog::onResetDefaults);

    QPushButton* btnSave = new QPushButton("💾 Save Settings", this);
    btnSave->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; padding: 6px 16px; } QPushButton:hover { background-color: #b4befe; }");
    connect(btnSave, &QPushButton::clicked, this, &ShowcaseSettingsDialog::onSave);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(btnReset);
    btnLayout->addStretch(1);
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(btnLayout);

    loadSettings();
}

void ShowcaseSettingsDialog::loadSettings() {
    QSettings settings("Amifiles", "Amifiles");
    int zoom = settings.value("file_panel/zoom_level", 1).toInt();
    m_cardSizeSlider->setValue(qBound(0, zoom, 6));

    m_gridSpacingSpin->setValue(settings.value("showcase/grid_spacing", 12).toInt());
    m_ambientBlurSlider->setValue(settings.value("showcase/ambient_blur_opacity", 75).toInt());

    m_checkAutoPlayTheme->setChecked(settings.value("theater/auto_play_theme_music", true).toBool());
    m_themeVolumeSlider->setValue(settings.value("theater/theme_music_volume", 60).toInt());
    m_checkGroupMultiDisc->setChecked(settings.value("theater/group_multi_disc", true).toBool());
    m_checkShowBottomPanel->setChecked(settings.value("showcase/show_info_panel", true).toBool());

    m_editMovieHideExts->setText(settings.value("movie_showcase/hidden_extensions", "*.nfo, *.srt, *.jpg, *.png, *.xml").toString());
    m_editTvHideExts->setText(settings.value("tv_showcase/hidden_extensions", "*.nfo, *.srt, *.jpg, *.png, *.xml, theme.mp3").toString());
    m_editMusicHideExts->setText(settings.value("music_showcase/hidden_extensions", "*.jpg, *.png, *.nfo, *.txt, *.sfv").toString());
}

void ShowcaseSettingsDialog::onSave() {
    QSettings settings("Amifiles", "Amifiles");

    settings.setValue("file_panel/zoom_level", m_cardSizeSlider->value());
    settings.setValue("showcase/grid_spacing", m_gridSpacingSpin->value());
    settings.setValue("showcase/ambient_blur_opacity", m_ambientBlurSlider->value());

    settings.setValue("theater/auto_play_theme_music", m_checkAutoPlayTheme->isChecked());
    settings.setValue("theater/theme_music_volume", m_themeVolumeSlider->value());
    settings.setValue("theater/group_multi_disc", m_checkGroupMultiDisc->isChecked());
    settings.setValue("showcase/show_info_panel", m_checkShowBottomPanel->isChecked());

    settings.setValue("movie_showcase/hidden_extensions", m_editMovieHideExts->text().trimmed());
    settings.setValue("tv_showcase/hidden_extensions", m_editTvHideExts->text().trimmed());
    settings.setValue("music_showcase/hidden_extensions", m_editMusicHideExts->text().trimmed());

    accept();
}

void ShowcaseSettingsDialog::onResetDefaults() {
    m_cardSizeSlider->setValue(1);
    m_gridSpacingSpin->setValue(12);
    m_ambientBlurSlider->setValue(75);
    m_checkAutoPlayTheme->setChecked(true);
    m_themeVolumeSlider->setValue(60);
    m_checkGroupMultiDisc->setChecked(true);
    m_checkShowBottomPanel->setChecked(true);

    m_editMovieHideExts->setText("*.nfo, *.srt, *.jpg, *.png, *.xml");
    m_editTvHideExts->setText("*.nfo, *.srt, *.jpg, *.png, *.xml, theme.mp3");
    m_editMusicHideExts->setText("*.jpg, *.png, *.nfo, *.txt, *.sfv");
}
