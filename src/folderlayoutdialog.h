#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include "mainwindow.h"

class FolderLayoutDialog : public QDialog {
    Q_OBJECT

public:
    FolderLayoutDialog(const QList<FolderLayoutRule>& existingRules, const QList<CustomButton>& availableButtons, QWidget* parent = nullptr);
    QList<FolderLayoutRule> rules() const { return m_rules; }

private slots:
    void onProfileSelected(int row);
    void onAddProfile();
    void onDeleteProfile();
    void onMoveUpProfile();
    void onMoveDownProfile();
    void onRuleTypeChanged(const QString& type);
    void onBrowseFolder();
    void onUseActivePath();
    void onChooseButtons();
    void onSelectBgColor();
    void onCaptureUI();
    void onApplyNow();
    void onCaptureTabs();
    void onClearTabs();
    void onSave();

private:
    void setupUI();
    void populateList();
    void populateFields(const FolderLayoutRule& r);
    void harvestCurrentProfile(int index);
    void updateTabsLabel(const FolderLayoutRule& r);

    // Left Pane (Master List)
    QListWidget* m_listWidget = nullptr;
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QPushButton* m_btnMoveUp = nullptr;
    QPushButton* m_btnMoveDown = nullptr;

    // Right Pane (Detail Editor)
    QWidget* m_editorWidget = nullptr;
    QLineEdit* m_editName = nullptr;
    QCheckBox* m_checkAutoApply = nullptr;
    QComboBox* m_comboRuleType = nullptr;
    QLineEdit* m_editValue = nullptr;
    QPushButton* m_btnBrowse = nullptr;
    QPushButton* m_btnUseActivePath = nullptr;
    QComboBox* m_comboViewMode = nullptr;
    QPushButton* m_btnChooseButtons = nullptr;

    // Visibility Overrides & States
    QCheckBox* m_overrideDrives = nullptr;
    QCheckBox* m_stateDrives = nullptr;
    QCheckBox* m_overrideCenterOps = nullptr;
    QCheckBox* m_stateCenterOps = nullptr;
    QCheckBox* m_overrideConsole = nullptr;
    QCheckBox* m_stateConsole = nullptr;
    QCheckBox* m_overridePreview = nullptr;
    QCheckBox* m_statePreview = nullptr;
    QCheckBox* m_overrideFavorites = nullptr;
    QCheckBox* m_stateFavorites = nullptr;
    QCheckBox* m_overrideZen = nullptr;
    QCheckBox* m_stateZen = nullptr;

    // Appearance Color Styling
    QCheckBox* m_useBgColor = nullptr;
    QPushButton* m_btnSelectBgColor = nullptr;
    QString m_selectedBgColor;

    // Tab Snapshots
    QCheckBox* m_hasTabsSnapshot = nullptr;
    QPushButton* m_btnCaptureTabs = nullptr;
    QPushButton* m_btnClearTabs = nullptr;
    QLabel* m_labelTabsInfo = nullptr;

    // Utility Actions
    QPushButton* m_btnCaptureUI = nullptr;
    QPushButton* m_btnApplyNow = nullptr;

    // Data lists
    QList<FolderLayoutRule> m_rules;
    QList<CustomButton> m_availableButtons;
    int m_currentIndex = -1;
};
