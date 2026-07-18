#include "themestudiodialog.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSettings>
#include <QApplication>
#include <QMessageBox>

ThemeStudioDialog::ThemeStudioDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Catppuccin Theme Studio");
    resize(480, 520);
    setStyleSheet(Theme::getStylesheet());

    setupUI();
    loadSettings();
    updateCustomControlsState();

    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ThemeStudioDialog::onPresetChanged);
    connect(m_sliderOpacity, &QSlider::valueChanged, this, [this](int value) {
        m_lblOpacityVal->setText(QString("%1%").arg(value));
    });
}

void ThemeStudioDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // Group 1: Presets & Layout Metrics
    QGroupBox* presetGroup = new QGroupBox("Presets & Layout Metrics", this);
    QGridLayout* presetLayout = new QGridLayout(presetGroup);
    presetLayout->setSpacing(8);

    presetLayout->addWidget(new QLabel("Theme Preset:", this), 0, 0);
    m_comboPreset = new QComboBox(this);
    m_comboPreset->addItems({"Catppuccin Mocha", "Catppuccin Macchiato", "Catppuccin Frappé", "Catppuccin Latte", "Amiga Workbench Classic", "Custom"});
    presetLayout->addWidget(m_comboPreset, 0, 1);

    presetLayout->addWidget(new QLabel("Font Size (px):", this), 1, 0);
    m_spinFontSize = new QSpinBox(this);
    m_spinFontSize->setRange(8, 20);
    presetLayout->addWidget(m_spinFontSize, 1, 1);

    presetLayout->addWidget(new QLabel("Border Radius (px):", this), 2, 0);
    m_spinBorderRadius = new QSpinBox(this);
    m_spinBorderRadius->setRange(0, 16);
    presetLayout->addWidget(m_spinBorderRadius, 2, 1);

    presetLayout->addWidget(new QLabel("Sidebar Opacity:", this), 3, 0);
    QHBoxLayout* opacityLayout = new QHBoxLayout();
    m_sliderOpacity = new QSlider(Qt::Horizontal, this);
    m_sliderOpacity->setRange(10, 100);
    m_lblOpacityVal = new QLabel("100%", this);
    opacityLayout->addWidget(m_sliderOpacity);
    opacityLayout->addWidget(m_lblOpacityVal);
    presetLayout->addLayout(opacityLayout, 3, 1);

    mainLayout->addWidget(presetGroup);

    // Group 2: Custom Theme Colors
    QGroupBox* colorGroup = new QGroupBox("Custom Theme Colors (Only for 'Custom' Preset)", this);
    QGridLayout* colorLayout = new QGridLayout(colorGroup);
    colorLayout->setSpacing(8);

    colorLayout->addWidget(new QLabel("Background Color:", this), 0, 0);
    m_btnBg = createColorButton("#1e1e2e", "theme/custom_bg");
    colorLayout->addWidget(m_btnBg, 0, 1);

    colorLayout->addWidget(new QLabel("Sidebar Color:", this), 1, 0);
    m_btnSidebar = createColorButton("#181825", "theme/custom_sidebar");
    colorLayout->addWidget(m_btnSidebar, 1, 1);

    colorLayout->addWidget(new QLabel("Border Color:", this), 2, 0);
    m_btnBorder = createColorButton("#313244", "theme/custom_border");
    colorLayout->addWidget(m_btnBorder, 2, 1);

    colorLayout->addWidget(new QLabel("Accent Color:", this), 3, 0);
    m_btnAccent = createColorButton("#89b4fa", "theme/custom_accent");
    colorLayout->addWidget(m_btnAccent, 3, 1);

    colorLayout->addWidget(new QLabel("Text Color:", this), 4, 0);
    m_btnText = createColorButton("#cdd6f4", "theme/custom_text");
    colorLayout->addWidget(m_btnText, 4, 1);

    colorLayout->addWidget(new QLabel("Secondary Text:", this), 5, 0);
    m_btnSecText = createColorButton("#a6adc8", "theme/custom_sec_text");
    colorLayout->addWidget(m_btnSecText, 5, 1);

    mainLayout->addWidget(colorGroup);

    // Group 3: Bottom Action Buttons
    QHBoxLayout* actionsLayout = new QHBoxLayout();
    m_btnApply = new QPushButton("Apply", this);
    connect(m_btnApply, &QPushButton::clicked, this, &ThemeStudioDialog::applyTheme);

    m_btnSave = new QPushButton("Save & Close", this);
    m_btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }");
    connect(m_btnSave, &QPushButton::clicked, this, &ThemeStudioDialog::saveAndClose);

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
        // Revert temporary settings if apply was clicked
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("theme/preset", m_origPreset);
        settings.setValue("theme/font_size", m_origFontSize);
        settings.setValue("theme/border_radius", m_origBorderRadius);
        settings.setValue("theme/sidebar_opacity", m_origOpacity);
        settings.setValue("theme/custom_bg", m_origBg);
        settings.setValue("theme/custom_sidebar", m_origSidebar);
        settings.setValue("theme/custom_border", m_origBorder);
        settings.setValue("theme/custom_accent", m_origAccent);
        settings.setValue("theme/custom_text", m_origText);
        settings.setValue("theme/custom_sec_text", m_origSecText);
        
        // Reapply original global styles
        qApp->setStyleSheet(Theme::getStylesheet());
        reject();
    });

    actionsLayout->addStretch(1);
    actionsLayout->addWidget(m_btnApply);
    actionsLayout->addWidget(m_btnSave);
    actionsLayout->addWidget(m_btnCancel);

    mainLayout->addLayout(actionsLayout);
}

QPushButton* ThemeStudioDialog::createColorButton(const QString& colorHex, const QString& settingsKey) {
    QPushButton* btn = new QPushButton(this);
    btn->setFixedSize(60, 24);
    btn->setStyleSheet(QString("background-color: %1; border: 1px solid #45475a; border-radius: 4px;").arg(colorHex));
    connect(btn, &QPushButton::clicked, this, [this, btn, settingsKey]() {
        chooseColor(btn, settingsKey);
    });
    return btn;
}

void ThemeStudioDialog::loadSettings() {
    QSettings settings("Amifiles", "Amifiles");
    m_origPreset = settings.value("theme/preset", "Catppuccin Mocha").toString();
    m_origFontSize = settings.value("theme/font_size", 13).toInt();
    m_origBorderRadius = settings.value("theme/border_radius", 4).toInt();
    m_origOpacity = settings.value("theme/sidebar_opacity", 1.0).toDouble();

    m_origBg = settings.value("theme/custom_bg", "#1e1e2e").toString();
    m_origSidebar = settings.value("theme/custom_sidebar", "#181825").toString();
    m_origBorder = settings.value("theme/custom_border", "#313244").toString();
    m_origAccent = settings.value("theme/custom_accent", "#89b4fa").toString();
    m_origText = settings.value("theme/custom_text", "#cdd6f4").toString();
    m_origSecText = settings.value("theme/custom_sec_text", "#a6adc8").toString();

    m_comboPreset->setCurrentText(m_origPreset);
    m_spinFontSize->setValue(m_origFontSize);
    m_spinBorderRadius->setValue(m_origBorderRadius);
    m_sliderOpacity->setValue(static_cast<int>(m_origOpacity * 100));
}

void ThemeStudioDialog::updateCustomControlsState() {
    bool isCustom = (m_comboPreset->currentText() == "Custom");
    m_btnBg->setEnabled(isCustom);
    m_btnSidebar->setEnabled(isCustom);
    m_btnBorder->setEnabled(isCustom);
    m_btnAccent->setEnabled(isCustom);
    m_btnText->setEnabled(isCustom);
    m_btnSecText->setEnabled(isCustom);
}

void ThemeStudioDialog::onPresetChanged(int index) {
    Q_UNUSED(index);
    updateCustomControlsState();
}

void ThemeStudioDialog::chooseColor(QPushButton* button, const QString& settingsKey) {
    QSettings settings("Amifiles", "Amifiles");
    QColor curr(settings.value(settingsKey, button->palette().color(QPalette::Button).name()).toString());
    QColor chosen = QColorDialog::getColor(curr, this, "Choose Color");
    if (chosen.isValid()) {
        QString hex = chosen.name();
        button->setStyleSheet(QString("background-color: %1; border: 1px solid #45475a; border-radius: 4px;").arg(hex));
        settings.setValue(settingsKey, hex);
        applyTheme();
    }
}

void ThemeStudioDialog::applyTheme() {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("theme/preset", m_comboPreset->currentText());
    settings.setValue("theme/font_size", m_spinFontSize->value());
    settings.setValue("theme/border_radius", m_spinBorderRadius->value());
    settings.setValue("theme/sidebar_opacity", m_sliderOpacity->value() / 100.0);

    // Apply global modern theme dynamically on all open dialogs / widgets
    qApp->setStyleSheet(Theme::getStylesheet());
}

void ThemeStudioDialog::saveAndClose() {
    applyTheme();
    accept();
}
