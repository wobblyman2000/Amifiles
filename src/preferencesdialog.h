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

private:
    void setupUI();
    void loadPreferences();
    void savePreferences();

    QListWidget* m_listCategory = nullptr;
    QStackedWidget* m_stackPages = nullptr;

    // General Preferences
    QCheckBox* m_chkAutoSaveLayout = nullptr;
    QCheckBox* m_chkHorizontalSplit = nullptr;
    QCheckBox* m_chkDrivesToolbar = nullptr;
    QCheckBox* m_chkDrivesMenu = nullptr;

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

    // Services & API keys
    QLineEdit* m_editTmdbApiKey = nullptr;
    QLineEdit* m_editHidePatterns = nullptr;

    QPushButton* m_btnOk = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnApply = nullptr;
};

#endif // PREFERENCESDIALOG_H
