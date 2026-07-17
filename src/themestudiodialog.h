#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>

class ThemeStudioDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeStudioDialog(QWidget* parent = nullptr);
    ~ThemeStudioDialog() override = default;

private slots:
    void onPresetChanged(int index);
    void applyTheme();
    void saveAndClose();
    void chooseColor(QPushButton* button, const QString& settingsKey);

private:
    void setupUI();
    void loadSettings();
    void updateCustomControlsState();
    QPushButton* createColorButton(const QString& colorHex, const QString& settingsKey);

    QComboBox* m_comboPreset = nullptr;
    QSpinBox* m_spinFontSize = nullptr;
    QSpinBox* m_spinBorderRadius = nullptr;
    QSlider* m_sliderOpacity = nullptr;
    QLabel* m_lblOpacityVal = nullptr;

    // Custom color buttons
    QPushButton* m_btnBg = nullptr;
    QPushButton* m_btnSidebar = nullptr;
    QPushButton* m_btnBorder = nullptr;
    QPushButton* m_btnAccent = nullptr;
    QPushButton* m_btnText = nullptr;
    QPushButton* m_btnSecText = nullptr;

    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnSave = nullptr;
    QPushButton* m_btnCancel = nullptr;

    // Temporary states to allow reverting on Cancel
    QString m_origPreset;
    int m_origFontSize = 13;
    int m_origBorderRadius = 4;
    double m_origOpacity = 1.0;
    QString m_origBg, m_origSidebar, m_origBorder, m_origAccent, m_origText, m_origSecText;
};
