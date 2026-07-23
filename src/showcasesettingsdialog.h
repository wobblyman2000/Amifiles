#pragma once

#include <QDialog>

class QSlider;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;

class ShowcaseSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ShowcaseSettingsDialog(QWidget* parent = nullptr);
    ~ShowcaseSettingsDialog() override = default;

private slots:
    void onSave();
    void onResetDefaults();

private:
    void loadSettings();

    // Sizing controls
    QSlider* m_cardSizeSlider = nullptr;
    QSpinBox* m_gridSpacingSpin = nullptr;
    
    // Feature toggles
    QCheckBox* m_checkAutoPlayTheme = nullptr;
    QSlider* m_themeVolumeSlider = nullptr;
    QCheckBox* m_checkGroupMultiDisc = nullptr;
    QCheckBox* m_checkShowBottomPanel = nullptr;
    QSlider* m_ambientBlurSlider = nullptr;

    // Extension Hiding
    QLineEdit* m_editMovieHideExts = nullptr;
    QLineEdit* m_editTvHideExts = nullptr;
    QLineEdit* m_editMusicHideExts = nullptr;
};
