#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include <QAbstractButton>
#include "mainwindow.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr) : QAbstractButton(parent) {
        setCheckable(true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
    }
    
    QSize sizeHint() const override {
        return QSize(44, 22);
    }
    
protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        bool chk = isChecked();
        
        // Draw track
        QRect trackRect(0, 0, width(), height());
        QColor trackColor = chk ? QColor("#a6e3a1") : QColor("#313244");
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(trackRect, height() / 2, height() / 2);
        
        // Draw thumb (handle)
        int thumbSize = height() - 4;
        int thumbX = chk ? (width() - thumbSize - 2) : 2;
        QRect thumbRect(thumbX, 2, thumbSize, thumbSize);
        p.setBrush(QColor("#11111b"));
        p.drawEllipse(thumbRect);
    }
};

class ProfileListItemWidget : public QWidget {
    Q_OBJECT
public:
    ProfileListItemWidget(const QString& name, bool checked, QWidget* parent = nullptr) : QWidget(parent) {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 4, 6, 4);
        
        m_label = new QLabel(name, this);
        m_label->setStyleSheet("font-weight: bold; color: #cdd6f4;");
        layout->addWidget(m_label);
        
        layout->addStretch();
        
        m_switch = new ToggleSwitch(this);
        m_switch->setChecked(checked);
        m_switch->setToolTip(checked ? "Profile Active (Green): This profile rule is enabled and auto-applies when browsing matching folders. Click to disable."
                                     : "Profile Inactive (Gray): This profile rule is disabled and will be ignored when browsing. Click to enable.");
        layout->addWidget(m_switch);
        
        connect(m_switch, &ToggleSwitch::toggled, this, [this](bool chk) {
            m_switch->setToolTip(chk ? "Profile Active (Green): This profile rule is enabled and auto-applies when browsing matching folders. Click to disable."
                                     : "Profile Inactive (Gray): This profile rule is disabled and will be ignored when browsing. Click to enable.");
            emit toggled(chk);
        });
    }
    
    void setName(const QString& name) { m_label->setText(name); }
    bool isChecked() const { return m_switch->isChecked(); }
    void setChecked(bool checked) { m_switch->setChecked(checked); }
    ToggleSwitch* toggle() const { return m_switch; }
    
signals:
    void toggled(bool checked);
    
private:
    QLabel* m_label;
    ToggleSwitch* m_switch;
};

class FolderLayoutDialog : public QDialog {
    Q_OBJECT

public:
    FolderLayoutDialog(const QList<FolderLayoutRule>& existingRules, const QList<CustomButton>& availableButtons, QWidget* parent = nullptr);
    QList<FolderLayoutRule> rules() const { return m_rules; }

private slots:
    void onProfileSelected(int row);
    void onAddProfile();
    void onAddTemplate();
    void onDeleteProfile();
    void onMoveUpProfile();
    void onMoveDownProfile();
    void onRuleTypeChanged(const QString& type);
    void onBrowseFolder();
    void onUseActivePath();
    void onChooseButtons();
    void onSelectToolbars();
    void onSelectMenus();
    void onSelectBgColor();
    void onCaptureUI();
    void onApplyNow();
    void onCaptureTabs();
    void onClearTabs();
    void onBackupProfiles();
    void onRestoreProfiles();
    void onEditToolbarsShortcut();
    void onEditMenusShortcut();
    void onSave();

private:
    void setupUI();
    void populateList();
    void populateFields(const FolderLayoutRule& r);
    void harvestCurrentProfile(int index);
    void updateTabsLabel(const FolderLayoutRule& r);
    void updateLinkedProfileCombo();
    void onLinkedProfileChanged(int index);
    void onApplyToCurrentFolder();

    // Left Pane (Master List)
    QListWidget* m_listWidget = nullptr;
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnAddTemplate = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QPushButton* m_btnMoveUp = nullptr;
    QPushButton* m_btnMoveDown = nullptr;
    QPushButton* m_btnBackup = nullptr;
    QPushButton* m_btnRestore = nullptr;
    QPushButton* m_btnApplyCurrentFolder = nullptr;

    // Right Pane (Detail Editor)
    QWidget* m_editorWidget = nullptr;
    QLineEdit* m_editName = nullptr;
    QAbstractButton* m_checkAutoApply = nullptr;
    QComboBox* m_comboRuleType = nullptr;
    QLineEdit* m_editValue = nullptr;
    QPushButton* m_btnBrowse = nullptr;
    QPushButton* m_btnUseActivePath = nullptr;
    QComboBox* m_comboViewMode = nullptr;
    QPushButton* m_btnChooseButtons = nullptr;
    QComboBox* m_comboLinkedProfile = nullptr;
    QLabel* m_labelInheritedInfo = nullptr;
    class QGroupBox* m_viewGroup = nullptr;
    class QGroupBox* m_visGroup = nullptr;
    class QGroupBox* m_styleGroup = nullptr;
    class QGroupBox* m_tabsGroup = nullptr;

    // Visibility Overrides & States
    QAbstractButton* m_overrideDrives = nullptr;
    QAbstractButton* m_stateDrives = nullptr;
    QAbstractButton* m_overrideCenterOps = nullptr;
    QAbstractButton* m_stateCenterOps = nullptr;
    QAbstractButton* m_overrideConsole = nullptr;
    QAbstractButton* m_stateConsole = nullptr;
    QAbstractButton* m_overridePreview = nullptr;
    QAbstractButton* m_statePreview = nullptr;
    QAbstractButton* m_overrideFavorites = nullptr;
    QAbstractButton* m_stateFavorites = nullptr;
    QAbstractButton* m_overrideZen = nullptr;
    QAbstractButton* m_stateZen = nullptr;
    QAbstractButton* m_overrideBuiltinPlayerDoubleclick = nullptr;
    QAbstractButton* m_stateBuiltinPlayerDoubleclick = nullptr;
    QAbstractButton* m_overrideFullScreenPlayer = nullptr;
    QAbstractButton* m_stateFullScreenPlayer = nullptr;
    QAbstractButton* m_overrideVisualizer = nullptr;
    QAbstractButton* m_stateVisualizer = nullptr;

    // Toolbar & Menu overrides
    QAbstractButton* m_overrideToolbars = nullptr;
    QPushButton* m_btnSelectToolbars = nullptr;
    QStringList m_selectedToolbars;
    QAbstractButton* m_overrideMenus = nullptr;
    QPushButton* m_btnSelectMenus = nullptr;
    QStringList m_selectedMenus;

    // Appearance Color Styling
    QAbstractButton* m_useBgColor = nullptr;
    QPushButton* m_btnSelectBgColor = nullptr;
    QString m_selectedBgColor;

    // Tab Snapshots
    QAbstractButton* m_hasTabsSnapshot = nullptr;
    QPushButton* m_btnCaptureTabs = nullptr;
    QPushButton* m_btnClearTabs = nullptr;
    QLabel* m_labelTabsInfo = nullptr;

    // Utility Actions
    QPushButton* m_btnCaptureUI = nullptr;
    QPushButton* m_btnApplyNow = nullptr;

    // Data lists
    QList<FolderLayoutRule> m_rules;
    QList<CustomButton> m_availableButtons;
    QByteArray m_capturedWindowState;
    int m_currentIndex = -1;
};
