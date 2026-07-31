#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QSettings>

class QListWidget;
class QStackedWidget;
class QCheckBox;
class QPushButton;
class QLineEdit;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    void setCurrentPage(int pageIndex);
    ~PreferencesDialog() override = default;

signals:
    void preferencesChanged();

private slots:
    void onCategoryChanged(int index);
    void onApplyClicked();
    void onOkClicked();
    void onConfigureAgeRules();
    void onResetDefaults();
    void onThemePresetChanged(int index);
    void onSaveThemePresetClicked();
    void onDeleteThemePresetClicked();
    void chooseThemeColor(QPushButton* button, const QString& settingsKey);

private:
    void setupUI();
    void loadPreferences();
    void savePreferences();
    QPushButton* createThemeColorButton(const QString& colorHex, const QString& settingsKey, QWidget* parentPage);
    void updateThemeControlsState();
    void populateThemePresets();

    QListWidget* m_listCategory = nullptr;
    QStackedWidget* m_stackPages = nullptr;

    // General Preferences
    QCheckBox* m_chkAutoSaveLayout = nullptr;
    QCheckBox* m_chkHorizontalSplit = nullptr;
    QCheckBox* m_chkDrivesToolbar = nullptr;
    QCheckBox* m_chkDrivesMenu = nullptr;
    QCheckBox* m_chkStickyFilters = nullptr;
    QCheckBox* m_chkAlwaysCenterSplitter = nullptr;
    QCheckBox* m_chkAlwaysCenterPreview = nullptr;
    QCheckBox* m_chkLockLayoutInShowcase = nullptr;
    QCheckBox* m_chkDetailsFullRowSelect = nullptr;

    // View & Style Colors
    QCheckBox* m_chkAgeColoring = nullptr;
    QPushButton* m_btnConfigureAgeRules = nullptr;

    // Archives & Disk Images
    QCheckBox* m_chkArchiveNav = nullptr;
    QCheckBox* m_chkArchiveWrite = nullptr;

    // Media & Previews
    QCheckBox* m_chkCasingOverlays = nullptr;
    QCheckBox* m_chkAudioCoverArt = nullptr;
    QCheckBox* m_chkSpectrumVisualizer = nullptr;
    QCheckBox* m_chkMutePreview = nullptr;
    QCheckBox* m_chkBuiltinPlayerDoubleclick = nullptr;
    QCheckBox* m_chkDoubleclickAddsToQueue = nullptr;
    QCheckBox* m_chkAutoFullscreen = nullptr;
    QCheckBox* m_chkAutoPlayThemeMusic = nullptr;
    QCheckBox* m_chkRememberVideoProgress = nullptr;
    QCheckBox* m_chkKeyboardRemoteMode = nullptr;
    QCheckBox* m_chkAutoQueueSiblings = nullptr;
    
    // Shortcuts Mapping
    class QKeySequenceEdit* m_keyPlayCollection = nullptr;
    class QKeySequenceEdit* m_keyInfoSheet = nullptr;
    class QKeySequenceEdit* m_keyScrapeMeta = nullptr;
    class QKeySequenceEdit* m_keyApplyCasing = nullptr;
    class QKeySequenceEdit* m_keyToggleDrawer = nullptr;
    class QKeySequenceEdit* m_keyNavigateUp = nullptr;
    class QKeySequenceEdit* m_keyNavigateBack = nullptr;

    class QKeySequenceEdit* m_keyPlayerPlayPause = nullptr;
    class QKeySequenceEdit* m_keyPlayerPrev = nullptr;
    class QKeySequenceEdit* m_keyPlayerNext = nullptr;
    class QKeySequenceEdit* m_keyPlayerMute = nullptr;
    class QKeySequenceEdit* m_keyPlayerMenu = nullptr;
    class QKeySequenceEdit* m_keyPlayFolder = nullptr;
    class QKeySequenceEdit* m_keyQueueFolder = nullptr;
    class QKeySequenceEdit* m_keyPlayQueue = nullptr;

    // Services & API keys
    QLineEdit* m_editTmdbApiKey = nullptr;
    QLineEdit* m_editHidePatterns = nullptr;

    // Theme Studio Tab Controls
    class QComboBox* m_comboThemePreset = nullptr;
    class QSpinBox* m_spinThemeFontSize = nullptr;
    class QSpinBox* m_spinThemeBorderRadius = nullptr;
    class QSlider* m_sliderThemeOpacity = nullptr;
    class QLabel* m_lblThemeOpacityVal = nullptr;

    QPushButton* m_btnThemeBg = nullptr;
    QPushButton* m_btnThemeSidebar = nullptr;
    QPushButton* m_btnThemeBorder = nullptr;
    QPushButton* m_btnThemeAccent = nullptr;
    QPushButton* m_btnThemeGreen = nullptr;
    QPushButton* m_btnThemeText = nullptr;
    QPushButton* m_btnThemeSecText = nullptr;
    QPushButton* m_btnThemeHover = nullptr;

    QPushButton* m_btnSaveThemePreset = nullptr;
    QPushButton* m_btnDeleteThemePreset = nullptr;

    QPushButton* m_btnResetDefaults = nullptr;
    QPushButton* m_btnOk = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnApply = nullptr;
};

#endif // PREFERENCESDIALOG_H
