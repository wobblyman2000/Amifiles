#include "folderlayoutdialog.h"
#include "toolbareditordialog.h"
#include "custommenueditordialog.h"
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>
#include <QColorDialog>
#include <QStackedWidget>
#include <QScrollArea>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class ButtonChooserDialog : public QDialog {
public:
    ButtonChooserDialog(const QList<CustomButton>& available, const QStringList& selected, QWidget* parent) : QDialog(parent) {
        setWindowTitle("Select Custom Toolbar Buttons");
        QVBoxLayout* layout = new QVBoxLayout(this);
        
        QLabel* label = new QLabel("Select which custom script buttons to display in this folder's toolbar:", this);
        layout->addWidget(label);
        
        m_listWidget = new QListWidget(this);
        for (const auto& btn : available) {
            QListWidgetItem* item = new QListWidgetItem(btn.name, m_listWidget);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected.contains(btn.name) ? Qt::Checked : Qt::Unchecked);
        }
        layout->addWidget(m_listWidget);
        
        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("OK", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);
        
        setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                      "QLabel { color: #cdd6f4; }"
                      "QListWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
                      "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; }"
                      "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    }
    
    QStringList selectedButtons() const {
        QStringList res;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem* item = m_listWidget->item(i);
            if (item->checkState() == Qt::Checked) {
                res.append(item->text());
            }
        }
        return res;
    }
private:
    QListWidget* m_listWidget;
};

class MultiSelectDialog : public QDialog {
public:
    MultiSelectDialog(const QString& title, const QString& labelText, const QList<QPair<QString, QString>>& items, const QStringList& initiallySelected, QWidget* parent) : QDialog(parent) {
        setWindowTitle(title);
        QVBoxLayout* layout = new QVBoxLayout(this);
        
        QLabel* label = new QLabel(labelText, this);
        layout->addWidget(label);
        
        m_listWidget = new QListWidget(this);
        for (const auto& pair : items) {
            QListWidgetItem* item = new QListWidgetItem(pair.first, m_listWidget);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setData(Qt::UserRole, pair.second);
            item->setCheckState(initiallySelected.contains(pair.second) ? Qt::Checked : Qt::Unchecked);
        }
        layout->addWidget(m_listWidget);
        
        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("OK", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);
        
        setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                      "QLabel { color: #cdd6f4; }"
                      "QListWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
                      "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; }"
                      "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    }
    
    QStringList selectedIds() const {
        QStringList res;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem* item = m_listWidget->item(i);
            if (item->checkState() == Qt::Checked) {
                res.append(item->data(Qt::UserRole).toString());
            }
        }
        return res;
    }
private:
    QListWidget* m_listWidget;
};


FolderLayoutDialog::FolderLayoutDialog(const QList<FolderLayoutRule>& existingRules, const QList<CustomButton>& availableButtons, QWidget* parent)
    : QDialog(parent), m_rules(existingRules), m_availableButtons(availableButtons) {
    setWindowTitle("Folder Profiles & Layouts");
    resize(920, 600);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; font-size: 12px; }"
                  "QGroupBox { font-weight: bold; border: 1px solid #313244; border-radius: 6px; margin-top: 12px; padding-top: 16px; color: #f5c2e7; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; }"
                  "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 5px; }"
                  "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; min-width: 120px; }"
                  "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QCheckBox { color: #cdd6f4; }");

    setupUI();
    populateList();
    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    } else {
        m_editorWidget->setEnabled(false);
    }
}

void FolderLayoutDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // ================= LEFT MASTER COLUMN =================
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(8);

    QLabel* listLabel = new QLabel("Profiles & Rules:", this);
    listLabel->setStyleSheet("font-weight: bold; color: #89b4fa;");
    leftLayout->addWidget(listLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setFixedWidth(220);
    leftLayout->addWidget(m_listWidget);

    QHBoxLayout* listButtons = new QHBoxLayout();
    m_btnAdd = new QPushButton("Add", this);
    m_btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; }"
                            "QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    connect(m_btnAdd, &QPushButton::clicked, this, &FolderLayoutDialog::onAddProfile);
    
    m_btnDelete = new QPushButton("Delete", this);
    m_btnDelete->setStyleSheet("QPushButton { background-color: #313244; color: #f38ba8; border: 1px solid #45475a; }"
                               "QPushButton:hover { background-color: #f38ba8; color: #11111b; }");
    connect(m_btnDelete, &QPushButton::clicked, this, &FolderLayoutDialog::onDeleteProfile);

    listButtons->addWidget(m_btnAdd);
    listButtons->addWidget(m_btnDelete);
    leftLayout->addLayout(listButtons);

    QHBoxLayout* orderButtons = new QHBoxLayout();
    m_btnMoveUp = new QPushButton("▲ Move Up", this);
    connect(m_btnMoveUp, &QPushButton::clicked, this, &FolderLayoutDialog::onMoveUpProfile);

    m_btnMoveDown = new QPushButton("▼ Move Down", this);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &FolderLayoutDialog::onMoveDownProfile);

    orderButtons->addWidget(m_btnMoveUp);
    orderButtons->addWidget(m_btnMoveDown);
    leftLayout->addLayout(orderButtons);

    QHBoxLayout* backupRestoreButtons = new QHBoxLayout();
    m_btnBackup = new QPushButton("Backup", this);
    m_btnBackup->setStyleSheet("QPushButton { background-color: #313244; color: #89b4fa; border: 1px solid #45475a; }"
                               "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    connect(m_btnBackup, &QPushButton::clicked, this, &FolderLayoutDialog::onBackupProfiles);

    m_btnRestore = new QPushButton("Restore", this);
    m_btnRestore->setStyleSheet("QPushButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; }"
                                "QPushButton:hover { background-color: #f9e2af; color: #11111b; }");
    connect(m_btnRestore, &QPushButton::clicked, this, &FolderLayoutDialog::onRestoreProfiles);

    backupRestoreButtons->addWidget(m_btnBackup);
    backupRestoreButtons->addWidget(m_btnRestore);
    leftLayout->addLayout(backupRestoreButtons);

    QHBoxLayout* shortcutButtons = new QHBoxLayout();
    QPushButton* btnEditToolbars = new QPushButton("Edit Toolbars...", this);
    btnEditToolbars->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; }"
                                   "QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    connect(btnEditToolbars, &QPushButton::clicked, this, &FolderLayoutDialog::onEditToolbarsShortcut);

    QPushButton* btnEditMenus = new QPushButton("Edit Menus...", this);
    btnEditMenus->setStyleSheet("QPushButton { background-color: #313244; color: #cba6f7; border: 1px solid #45475a; }"
                                "QPushButton:hover { background-color: #cba6f7; color: #11111b; }");
    connect(btnEditMenus, &QPushButton::clicked, this, &FolderLayoutDialog::onEditMenusShortcut);

    shortcutButtons->addWidget(btnEditToolbars);
    shortcutButtons->addWidget(btnEditMenus);
    leftLayout->addLayout(shortcutButtons);

    mainLayout->addLayout(leftLayout);

    // ================= RIGHT DETAIL COLUMN =================
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    m_editorWidget = new QWidget(this);
    QVBoxLayout* editorLayout = new QVBoxLayout(m_editorWidget);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    // Scroll Area for details editor
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget* scrollContent = new QWidget(this);
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(12);
    scrollLayout->setContentsMargins(0, 0, 10, 0);

    // 1. General Profile Name & Trigger
    QGroupBox* triggerGroup = new QGroupBox("1. Name & Activation Triggers", this);
    QGridLayout* triggerGrid = new QGridLayout(triggerGroup);
    triggerGrid->setSpacing(8);

    triggerGrid->addWidget(new QLabel("Profile Name:", this), 0, 0);
    m_editName = new QLineEdit(this);
    triggerGrid->addWidget(m_editName, 0, 1, 1, 2);

    m_checkAutoApply = new QCheckBox("Auto-apply when browsing matching folders", this);
    triggerGrid->addWidget(m_checkAutoApply, 1, 0, 1, 3);

    triggerGrid->addWidget(new QLabel("Match Condition:", this), 2, 0);
    m_comboRuleType = new QComboBox(this);
    m_comboRuleType->addItems({"Path", "Category"});
    triggerGrid->addWidget(m_comboRuleType, 2, 1, 1, 2);
    connect(m_comboRuleType, &QComboBox::currentTextChanged, this, &FolderLayoutDialog::onRuleTypeChanged);

    triggerGrid->addWidget(new QLabel("Match Value:", this), 3, 0);
    m_editValue = new QLineEdit(this);
    triggerGrid->addWidget(m_editValue, 3, 1);

    m_btnBrowse = new QPushButton("Browse...", this);
    connect(m_btnBrowse, &QPushButton::clicked, this, &FolderLayoutDialog::onBrowseFolder);
    triggerGrid->addWidget(m_btnBrowse, 3, 2);

    m_btnUseActivePath = new QPushButton("Use Active Path", this);
    connect(m_btnUseActivePath, &QPushButton::clicked, this, &FolderLayoutDialog::onUseActivePath);
    triggerGrid->addWidget(m_btnUseActivePath, 4, 1, 1, 2);

    scrollLayout->addWidget(triggerGroup);

    // 2. View Settings
    QGroupBox* viewGroup = new QGroupBox("2. View Mode & Toolbars", this);
    QGridLayout* viewGrid = new QGridLayout(viewGroup);
    viewGrid->setSpacing(8);

    viewGrid->addWidget(new QLabel("View Mode:", this), 0, 0);
    m_comboViewMode = new QComboBox(this);
    m_comboViewMode->addItems({"No Change", "List", "Grid", "Card", "Miller", "Timeline", "Filmstrip", "Theater"});
    viewGrid->addWidget(m_comboViewMode, 0, 1);

    viewGrid->addWidget(new QLabel("Custom Toolbar:", this), 1, 0);
    m_btnChooseButtons = new QPushButton("All Buttons (Default)", this);
    connect(m_btnChooseButtons, &QPushButton::clicked, this, &FolderLayoutDialog::onChooseButtons);
    viewGrid->addWidget(m_btnChooseButtons, 1, 1);

    scrollLayout->addWidget(viewGroup);

    // 3. Visibility Overrides
    QGroupBox* visGroup = new QGroupBox("3. Layout & Docks Visibility Overrides", this);
    QGridLayout* visGrid = new QGridLayout(visGroup);
    visGrid->setSpacing(8);

    visGrid->addWidget(new QLabel("Layout Component", this), 0, 0);
    QLabel* hdrOverride = new QLabel("Override Visibility?", this);
    hdrOverride->setStyleSheet("font-weight: bold; color: #89b4fa;");
    visGrid->addWidget(hdrOverride, 0, 1);
    QLabel* hdrState = new QLabel("Visible/Enabled State", this);
    hdrState->setStyleSheet("font-weight: bold; color: #a6e3a1;");
    visGrid->addWidget(hdrState, 0, 2);

    // Drives
    m_overrideDrives = new QCheckBox(this);
    m_stateDrives = new QCheckBox("Drives Toolbar", this);
    visGrid->addWidget(new QLabel("Drives Toolbar", this), 1, 0);
    visGrid->addWidget(m_overrideDrives, 1, 1);
    visGrid->addWidget(m_stateDrives, 1, 2);
    connect(m_overrideDrives, &QCheckBox::toggled, m_stateDrives, &QCheckBox::setEnabled);

    // Center Ops
    m_overrideCenterOps = new QCheckBox(this);
    m_stateCenterOps = new QCheckBox("Operations Bar", this);
    visGrid->addWidget(new QLabel("Operations Bar", this), 2, 0);
    visGrid->addWidget(m_overrideCenterOps, 2, 1);
    visGrid->addWidget(m_stateCenterOps, 2, 2);
    connect(m_overrideCenterOps, &QCheckBox::toggled, m_stateCenterOps, &QCheckBox::setEnabled);

    // Console
    m_overrideConsole = new QCheckBox(this);
    m_stateConsole = new QCheckBox("Console / Bash Shell Panel", this);
    visGrid->addWidget(new QLabel("Console Panel", this), 3, 0);
    visGrid->addWidget(m_overrideConsole, 3, 1);
    visGrid->addWidget(m_stateConsole, 3, 2);
    connect(m_overrideConsole, &QCheckBox::toggled, m_stateConsole, &QCheckBox::setEnabled);

    // Preview Pane
    m_overridePreview = new QCheckBox(this);
    m_statePreview = new QCheckBox("Preview Pane", this);
    visGrid->addWidget(new QLabel("Preview Pane", this), 4, 0);
    visGrid->addWidget(m_overridePreview, 4, 1);
    visGrid->addWidget(m_statePreview, 4, 2);
    connect(m_overridePreview, &QCheckBox::toggled, m_statePreview, &QCheckBox::setEnabled);

    // Favorites
    m_overrideFavorites = new QCheckBox(this);
    m_stateFavorites = new QCheckBox("Favorites Sidebar", this);
    visGrid->addWidget(new QLabel("Favorites Sidebar", this), 5, 0);
    visGrid->addWidget(m_overrideFavorites, 5, 1);
    visGrid->addWidget(m_stateFavorites, 5, 2);
    connect(m_overrideFavorites, &QCheckBox::toggled, m_stateFavorites, &QCheckBox::setEnabled);

    // Zen
    m_overrideZen = new QCheckBox(this);
    m_stateZen = new QCheckBox("Enable Zen Mode", this);
    visGrid->addWidget(new QLabel("Zen Mode State", this), 6, 0);
    visGrid->addWidget(m_overrideZen, 6, 1);
    visGrid->addWidget(m_stateZen, 6, 2);
    connect(m_overrideZen, &QCheckBox::toggled, m_stateZen, &QCheckBox::setEnabled);

    // Built-in Fullscreen Playback
    m_overrideBuiltinPlayerDoubleclick = new QCheckBox(this);
    m_stateBuiltinPlayerDoubleclick = new QCheckBox("Double-click plays media in built-in player", this);
    visGrid->addWidget(new QLabel("Built-in Fullscreen", this), 7, 0);
    visGrid->addWidget(m_overrideBuiltinPlayerDoubleclick, 7, 1);
    visGrid->addWidget(m_stateBuiltinPlayerDoubleclick, 7, 2);
    connect(m_overrideBuiltinPlayerDoubleclick, &QCheckBox::toggled, m_stateBuiltinPlayerDoubleclick, &QCheckBox::setEnabled);

    // Custom Toolbars Override
    m_overrideToolbars = new QCheckBox(this);
    m_btnSelectToolbars = new QPushButton("Select Active Toolbars...", this);
    m_btnSelectToolbars->setEnabled(false);
    connect(m_btnSelectToolbars, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectToolbars);
    connect(m_overrideToolbars, &QCheckBox::toggled, m_btnSelectToolbars, &QPushButton::setEnabled);
    visGrid->addWidget(new QLabel("Custom Toolbars", this), 8, 0);
    visGrid->addWidget(m_overrideToolbars, 8, 1);
    visGrid->addWidget(m_btnSelectToolbars, 8, 2);

    // Custom Menus Override
    m_overrideMenus = new QCheckBox(this);
    m_btnSelectMenus = new QPushButton("Select Custom Menus...", this);
    m_btnSelectMenus->setEnabled(false);
    connect(m_btnSelectMenus, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectMenus);
    connect(m_overrideMenus, &QCheckBox::toggled, m_btnSelectMenus, &QPushButton::setEnabled);
    visGrid->addWidget(new QLabel("Custom Menus", this), 9, 0);
    visGrid->addWidget(m_overrideMenus, 9, 1);
    visGrid->addWidget(m_btnSelectMenus, 9, 2);

    scrollLayout->addWidget(visGroup);

    // 4. Styling (Custom Background Color)
    QGroupBox* styleGroup = new QGroupBox("4. Look & Feel Custom Background", this);
    QHBoxLayout* styleLayout = new QHBoxLayout(styleGroup);
    m_useBgColor = new QCheckBox("Use Custom Background Color", this);
    m_btnSelectBgColor = new QPushButton("Select Color...", this);
    connect(m_btnSelectBgColor, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectBgColor);
    connect(m_useBgColor, &QCheckBox::toggled, m_btnSelectBgColor, &QPushButton::setEnabled);
    
    styleLayout->addWidget(m_useBgColor);
    styleLayout->addWidget(m_btnSelectBgColor);
    styleLayout->addStretch();
    scrollLayout->addWidget(styleGroup);

    // 5. Session Tab Snapshots
    QGroupBox* tabsGroup = new QGroupBox("5. Tab Snapshots (For Manual Session Restoring)", this);
    QVBoxLayout* tabsLayout = new QVBoxLayout(tabsGroup);
    tabsLayout->setSpacing(6);

    m_hasTabsSnapshot = new QCheckBox("Include Open Tabs Snapshot", this);
    tabsLayout->addWidget(m_hasTabsSnapshot);

    QHBoxLayout* tabsButtons = new QHBoxLayout();
    m_btnCaptureTabs = new QPushButton("Capture Current Open Tabs", this);
    connect(m_btnCaptureTabs, &QPushButton::clicked, this, &FolderLayoutDialog::onCaptureTabs);
    
    m_btnClearTabs = new QPushButton("Clear Snapshot", this);
    connect(m_btnClearTabs, &QPushButton::clicked, this, &FolderLayoutDialog::onClearTabs);

    tabsButtons->addWidget(m_btnCaptureTabs);
    tabsButtons->addWidget(m_btnClearTabs);
    tabsButtons->addStretch();
    tabsLayout->addLayout(tabsButtons);

    m_labelTabsInfo = new QLabel("No snapshot tabs saved in this profile.", this);
    m_labelTabsInfo->setStyleSheet("color: #a6adc8; font-style: italic;");
    tabsLayout->addWidget(m_labelTabsInfo);

    scrollLayout->addWidget(tabsGroup);

    scrollArea->setWidget(scrollContent);
    editorLayout->addWidget(scrollArea);
    
    rightLayout->addWidget(m_editorWidget, 1);

    // Bottom Action Row inside details pane
    QHBoxLayout* activeActionsRow = new QHBoxLayout();
    m_btnCaptureUI = new QPushButton("Capture Current UI State", this);
    m_btnCaptureUI->setStyleSheet("QPushButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; }");
    connect(m_btnCaptureUI, &QPushButton::clicked, this, &FolderLayoutDialog::onCaptureUI);

    m_btnApplyNow = new QPushButton("Apply Profile to Current View Now", this);
    m_btnApplyNow->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
    connect(m_btnApplyNow, &QPushButton::clicked, this, &FolderLayoutDialog::onApplyNow);

    activeActionsRow->addWidget(m_btnCaptureUI);
    activeActionsRow->addWidget(m_btnApplyNow);
    activeActionsRow->addStretch();
    rightLayout->addLayout(activeActionsRow);

    // Dialog buttons (Save & Cancel)
    QHBoxLayout* dialogButtons = new QHBoxLayout();
    dialogButtons->addStretch();
    QPushButton* btnSave = new QPushButton("Save & Apply Profiles", this);
    btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
    connect(btnSave, &QPushButton::clicked, this, &FolderLayoutDialog::onSave);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    dialogButtons->addWidget(btnSave);
    dialogButtons->addWidget(btnCancel);
    rightLayout->addLayout(dialogButtons);

    mainLayout->addLayout(rightLayout, 1);

    connect(m_listWidget, &QListWidget::currentRowChanged, this, &FolderLayoutDialog::onProfileSelected);
}

void FolderLayoutDialog::populateList() {
    m_listWidget->clear();
    for (const auto& r : m_rules) {
        m_listWidget->addItem(r.name.isEmpty() ? "(Unnamed Profile)" : r.name);
    }
}

void FolderLayoutDialog::populateFields(const FolderLayoutRule& r) {
    m_editName->setText(r.name);
    m_checkAutoApply->setChecked(r.autoApply);
    m_comboRuleType->setCurrentText(r.ruleType);
    m_editValue->setText(r.value);

    // View settings
    m_comboViewMode->setCurrentText(r.viewMode.isEmpty() ? "No Change" : r.viewMode);
    
    // Choose buttons button text
    m_btnChooseButtons->setProperty("selectedButtons", r.customButtons);
    m_btnChooseButtons->setText(r.customButtons.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(r.customButtons.size()));

    // Visibility checkboxes
    m_overrideDrives->setChecked(r.overrideDrivesToolbar);
    m_stateDrives->setChecked(r.drivesToolbarVisible);
    m_stateDrives->setEnabled(r.overrideDrivesToolbar);

    m_overrideCenterOps->setChecked(r.overrideCenterOps);
    m_stateCenterOps->setChecked(r.centerOpsVisible);
    m_stateCenterOps->setEnabled(r.overrideCenterOps);

    m_overrideConsole->setChecked(r.overrideConsole);
    m_stateConsole->setChecked(r.consoleVisible);
    m_stateConsole->setEnabled(r.overrideConsole);

    m_overridePreview->setChecked(r.overridePreview);
    m_statePreview->setChecked(r.previewVisible);
    m_statePreview->setEnabled(r.overridePreview);

    m_overrideFavorites->setChecked(r.overrideFavoritesSidebar);
    m_stateFavorites->setChecked(r.favoritesSidebarVisible);
    m_stateFavorites->setEnabled(r.overrideFavoritesSidebar);

    m_overrideZen->setChecked(r.overrideZenMode);
    m_stateZen->setChecked(r.zenModeActive);
    m_stateZen->setEnabled(r.overrideZenMode);

    m_overrideBuiltinPlayerDoubleclick->setChecked(r.overrideBuiltinPlayerDoubleclick);
    m_stateBuiltinPlayerDoubleclick->setChecked(r.builtinPlayerDoubleclick);
    m_stateBuiltinPlayerDoubleclick->setEnabled(r.overrideBuiltinPlayerDoubleclick);

    // Toolbar & Menu Overrides
    m_overrideToolbars->setChecked(r.overrideToolbars);
    m_btnSelectToolbars->setEnabled(r.overrideToolbars);
    m_selectedToolbars = r.selectedToolbars;
    m_btnSelectToolbars->setText(r.selectedToolbars.isEmpty() ? "Select Toolbars..." : QString("%1 Selected").arg(r.selectedToolbars.size()));

    m_overrideMenus->setChecked(r.overrideMenus);
    m_btnSelectMenus->setEnabled(r.overrideMenus);
    m_selectedMenus = r.selectedMenus;
    m_btnSelectMenus->setText(r.selectedMenus.isEmpty() ? "Select Custom Menus..." : QString("%1 Selected").arg(r.selectedMenus.size()));

    // Appearance styling
    m_useBgColor->setChecked(r.useBgColor);
    m_selectedBgColor = r.bgColor;
    m_btnSelectBgColor->setEnabled(r.useBgColor);
    if (r.useBgColor && !m_selectedBgColor.isEmpty()) {
        m_btnSelectBgColor->setText(m_selectedBgColor);
        m_btnSelectBgColor->setStyleSheet(QString("background-color: %1; color: #11111b;").arg(m_selectedBgColor));
    } else {
        m_btnSelectBgColor->setText("Select Color...");
        m_btnSelectBgColor->setStyleSheet("");
    }

    // Tabs Snapshot
    m_hasTabsSnapshot->setChecked(r.hasTabsSnapshot);
    updateTabsLabel(r);
    m_capturedWindowState = r.windowState;
}

void FolderLayoutDialog::harvestCurrentProfile(int index) {
    if (index < 0 || index >= m_rules.size()) return;

    FolderLayoutRule& r = m_rules[index];
    r.name = m_editName->text().trimmed();
    if (r.name.isEmpty()) r.name = "Unnamed Profile";
    r.autoApply = m_checkAutoApply->isChecked();
    r.ruleType = m_comboRuleType->currentText();
    r.value = m_editValue->text().trimmed();

    r.viewMode = m_comboViewMode->currentText();
    r.customButtons = m_btnChooseButtons->property("selectedButtons").toStringList();

    r.overrideDrivesToolbar = m_overrideDrives->isChecked();
    r.drivesToolbarVisible = m_stateDrives->isChecked();

    r.overrideCenterOps = m_overrideCenterOps->isChecked();
    r.centerOpsVisible = m_stateCenterOps->isChecked();

    r.overrideConsole = m_overrideConsole->isChecked();
    r.consoleVisible = m_stateConsole->isChecked();

    r.overridePreview = m_overridePreview->isChecked();
    r.previewVisible = m_statePreview->isChecked();

    r.overrideFavoritesSidebar = m_overrideFavorites->isChecked();
    r.favoritesSidebarVisible = m_stateFavorites->isChecked();

    r.overrideZenMode = m_overrideZen->isChecked();
    r.zenModeActive = m_stateZen->isChecked();

    r.overrideBuiltinPlayerDoubleclick = m_overrideBuiltinPlayerDoubleclick->isChecked();
    r.builtinPlayerDoubleclick = m_stateBuiltinPlayerDoubleclick->isChecked();

    r.overrideToolbars = m_overrideToolbars->isChecked();
    r.selectedToolbars = m_selectedToolbars;

    r.overrideMenus = m_overrideMenus->isChecked();
    r.selectedMenus = m_selectedMenus;

    r.useBgColor = m_useBgColor->isChecked();
    r.bgColor = m_selectedBgColor;

    r.hasTabsSnapshot = m_hasTabsSnapshot->isChecked();
    r.windowState = m_capturedWindowState;

    // Update list widget item text dynamically
    QListWidgetItem* item = m_listWidget->item(index);
    if (item) {
        item->setText(r.name);
    }
}

void FolderLayoutDialog::onProfileSelected(int row) {
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }
    m_currentIndex = row;
    if (row >= 0 && row < m_rules.size()) {
        m_editorWidget->setEnabled(true);
        populateFields(m_rules[row]);
    } else {
        m_editorWidget->setEnabled(false);
    }
}

void FolderLayoutDialog::onAddProfile() {
    FolderLayoutRule r;
    r.name = QString("Profile %1").arg(m_rules.size() + 1);
    r.ruleType = "Path";
    r.viewMode = "No Change";
    m_rules.append(r);
    
    populateList();
    m_listWidget->setCurrentRow(m_rules.size() - 1);
}

void FolderLayoutDialog::onDeleteProfile() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_rules.size()) return;

    if (QMessageBox::question(this, "Delete Profile", QString("Are you sure you want to delete profile '%1'?").arg(m_rules[row].name)) == QMessageBox::Yes) {
        m_rules.removeAt(row);
        m_currentIndex = -1;
        populateList();
        if (m_listWidget->count() > 0) {
            m_listWidget->setCurrentRow(qMin(row, m_listWidget->count() - 1));
        } else {
            m_editorWidget->setEnabled(false);
        }
    }
}

void FolderLayoutDialog::onMoveUpProfile() {
    int row = m_listWidget->currentRow();
    if (row <= 0 || row >= m_rules.size()) return;

    harvestCurrentProfile(row);
    m_rules.swapItemsAt(row, row - 1);
    populateList();
    m_listWidget->setCurrentRow(row - 1);
}

void FolderLayoutDialog::onMoveDownProfile() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_rules.size() - 1) return;

    harvestCurrentProfile(row);
    m_rules.swapItemsAt(row, row + 1);
    populateList();
    m_listWidget->setCurrentRow(row + 1);
}

void FolderLayoutDialog::onRuleTypeChanged(const QString& type) {
    if (type == "Category") {
        m_btnBrowse->setEnabled(false);
        m_btnUseActivePath->setEnabled(false);
        m_editValue->setPlaceholderText("Enter category: Music, Videos, Pictures, Documents");
    } else {
        m_btnBrowse->setEnabled(true);
        m_btnUseActivePath->setEnabled(true);
        m_editValue->setPlaceholderText("Enter exact folder path...");
    }
}

void FolderLayoutDialog::onBrowseFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Target Folder", m_editValue->text());
    if (!dir.isEmpty()) {
        m_editValue->setText(dir);
    }
}

void FolderLayoutDialog::onUseActivePath() {
    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (mw && mw->m_activePanel) {
        m_editValue->setText(mw->m_activePanel->currentPath());
    }
}

void FolderLayoutDialog::onChooseButtons() {
    QStringList current = m_btnChooseButtons->property("selectedButtons").toStringList();
    ButtonChooserDialog dlg(m_availableButtons, current, this);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList next = dlg.selectedButtons();
        m_btnChooseButtons->setProperty("selectedButtons", next);
        m_btnChooseButtons->setText(next.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(next.size()));
    }
}

void FolderLayoutDialog::onSelectToolbars() {
    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_toolbars_v1").toString();
    QList<QPair<QString, QString>> items;
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject tbObj = arr[i].toObject();
            QString name = tbObj["name"].toString();
            QString id = tbObj["id"].toString();
            items.append({name, id});
        }
    } else {
        items = {
            {"File Operations", "tb_file_ops"},
            {"View Options", "tb_view_ops"},
            {"Drives", "tb_drives"},
            {"Custom Commands", "customToolBar"}
        };
    }

    MultiSelectDialog dlg("Select Active Toolbars", "Select which toolbars should be visible under this profile:", items, m_selectedToolbars, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedToolbars = dlg.selectedIds();
        m_btnSelectToolbars->setText(m_selectedToolbars.isEmpty() ? "Select Toolbars..." : QString("%1 Selected").arg(m_selectedToolbars.size()));
    }
}

void FolderLayoutDialog::onSelectMenus() {
    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_menus_v2").toString();
    QList<QPair<QString, QString>> items;
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject mObj = arr[i].toObject();
            QString title = mObj["title"].toString();
            items.append({title, title});
        }
    } else {
        items = {
            {"Custom commands", "Custom commands"}
        };
    }

    MultiSelectDialog dlg("Select Active Custom Menus", "Select which custom menus should be visible in the menu bar under this profile:", items, m_selectedMenus, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedMenus = dlg.selectedIds();
        m_btnSelectMenus->setText(m_selectedMenus.isEmpty() ? "Select Custom Menus..." : QString("%1 Selected").arg(m_selectedMenus.size()));
    }
}

void FolderLayoutDialog::onSelectBgColor() {
    QColor initialCol = m_selectedBgColor.isEmpty() ? QColor("#1e1e2e") : QColor(m_selectedBgColor);
    QColor col = QColorDialog::getColor(initialCol, this, "Select Folder Panel Background Color");
    if (col.isValid()) {
        m_selectedBgColor = col.name();
        m_btnSelectBgColor->setText(m_selectedBgColor);
        m_btnSelectBgColor->setStyleSheet(QString("background-color: %1; color: #11111b;").arg(m_selectedBgColor));
    }
}

void FolderLayoutDialog::onCaptureUI() {
    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (!mw) return;

    // 1. Capture current View Mode
    if (mw->m_activePanel) {
        int idx = mw->m_activePanel->viewModeIndex();
        if (idx == 0) m_comboViewMode->setCurrentText("List");
        else if (idx == 1) m_comboViewMode->setCurrentText("Grid");
        else if (idx == 2) m_comboViewMode->setCurrentText("Card");
        else if (idx == 3) m_comboViewMode->setCurrentText("Miller");
        else if (idx == 4) m_comboViewMode->setCurrentText("Timeline");
        else if (idx == 5) m_comboViewMode->setCurrentText("Filmstrip");
        else if (idx == 6) m_comboViewMode->setCurrentText("Theater");
    }

    // 2. Capture custom buttons filter list
    m_btnChooseButtons->setProperty("selectedButtons", mw->m_activeToolbarFilter);
    m_btnChooseButtons->setText(mw->m_activeToolbarFilter.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(mw->m_activeToolbarFilter.size()));

    // 3. Visibilities
    m_overrideDrives->setChecked(true);
    m_stateDrives->setChecked(mw->m_actToggleDrivesToolbar && mw->m_actToggleDrivesToolbar->isChecked());

    m_overrideCenterOps->setChecked(true);
    m_stateCenterOps->setChecked(mw->m_actToggleCenterOps && mw->m_actToggleCenterOps->isChecked());

    m_overrideConsole->setChecked(true);
    m_stateConsole->setChecked(mw->m_actToggleConsole && mw->m_actToggleConsole->isChecked());

    m_overridePreview->setChecked(true);
    m_statePreview->setChecked(mw->m_actTogglePreview && mw->m_actTogglePreview->isChecked());

    m_overrideFavorites->setChecked(true);
    m_stateFavorites->setChecked(mw->m_actToggleFavoritesSidebar && mw->m_actToggleFavoritesSidebar->isChecked());

    m_overrideZen->setChecked(true);
    m_stateZen->setChecked(mw->m_zenMode);

    m_overrideBuiltinPlayerDoubleclick->setChecked(true);
    m_stateBuiltinPlayerDoubleclick->setChecked(mw->isBuiltinPlayerDoubleclickActive());

    m_capturedWindowState = mw->saveState();
}

void FolderLayoutDialog::onApplyNow() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_rules.size()) return;

    harvestCurrentProfile(row);
    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (mw) {
        mw->applyProfile(m_rules[row]);
        QMessageBox::information(this, "Apply Layout", QString("Layout profile '%1' applied to active panel successfully!").arg(m_rules[row].name));
    }
}

void FolderLayoutDialog::onCaptureTabs() {
    MainWindow* mw = qobject_cast<MainWindow*>(parent());
    if (!mw) return;

    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_rules.size()) return;

    FolderLayoutRule& r = m_rules[row];
    r.leftPaths.clear();
    for (int i = 0; i < mw->m_leftTabWidget->count(); ++i) {
        FilePanel* fp = qobject_cast<FilePanel*>(mw->m_leftTabWidget->widget(i));
        if (fp) r.leftPaths.append(fp->currentPath());
    }
    r.leftActiveIndex = mw->m_leftTabWidget->currentIndex();

    r.rightPaths.clear();
    for (int i = 0; i < mw->m_rightTabWidget->count(); ++i) {
        FilePanel* fp = qobject_cast<FilePanel*>(mw->m_rightTabWidget->widget(i));
        if (fp) r.rightPaths.append(fp->currentPath());
    }
    r.rightActiveIndex = mw->m_rightTabWidget->currentIndex();

    r.hasTabsSnapshot = true;
    m_hasTabsSnapshot->setChecked(true);

    updateTabsLabel(r);
    QMessageBox::information(this, "Capture Tabs", "Captured open Left and Right tabs layouts into this profile.");
}

void FolderLayoutDialog::onClearTabs() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_rules.size()) return;

    FolderLayoutRule& r = m_rules[row];
    r.leftPaths.clear();
    r.rightPaths.clear();
    r.leftActiveIndex = 0;
    r.rightActiveIndex = 0;
    r.hasTabsSnapshot = false;
    m_hasTabsSnapshot->setChecked(false);

    updateTabsLabel(r);
}

void FolderLayoutDialog::updateTabsLabel(const FolderLayoutRule& r) {
    if (r.hasTabsSnapshot && !r.leftPaths.isEmpty()) {
        m_labelTabsInfo->setText(QString("Saved snapshot: Left (%1 tabs), Right (%2 tabs)")
                                 .arg(r.leftPaths.size())
                                 .arg(r.rightPaths.size()));
        m_labelTabsInfo->setStyleSheet("color: #a6e3a1; font-weight: bold;");
    } else {
        m_labelTabsInfo->setText("No snapshot tabs saved in this profile.");
        m_labelTabsInfo->setStyleSheet("color: #a6adc8; font-style: italic;");
    }
}

void FolderLayoutDialog::onSave() {
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }
    
    // Validate rules condition path/values are configured (excluding "Default")
    for (int i = 0; i < m_rules.size(); ++i) {
        const auto& r = m_rules[i];
        if (r.autoApply && r.value.isEmpty() && r.name.toLower() != "default") {
            QMessageBox::warning(this, "Validation Failed", 
                                 QString("Profile '%1' is set to auto-apply but has an empty match value. Please fill or disable auto-apply.").arg(r.name));
            m_listWidget->setCurrentRow(i);
            return;
        }
    }

    accept();
}

#include <QFile>

void FolderLayoutDialog::onBackupProfiles() {
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }
    if (m_rules.isEmpty()) {
        QMessageBox::information(this, "Backup Profiles", "No profiles to backup!");
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, "Backup Folder Profiles", QDir::homePath(), "JSON Files (*.json)");
    if (fileName.isEmpty()) return;

    QJsonArray arr;
    for (const auto& r : m_rules) {
        arr.append(MainWindow::ruleToJson(r));
    }
    QJsonDocument doc(arr);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Backup Failed", "Cannot open file for writing: " + file.errorString());
        return;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    QMessageBox::information(this, "Backup Successful", "All folder profiles backed up successfully to:\n" + fileName);
}

void FolderLayoutDialog::onRestoreProfiles() {
    QString fileName = QFileDialog::getOpenFileName(this, "Restore Folder Profiles", QDir::homePath(), "JSON Files (*.json)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Restore Failed", "Cannot open file for reading: " + file.errorString());
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        QMessageBox::critical(this, "Restore Failed", "Invalid JSON format in the backup file.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Restore", 
        "Restoring profiles will merge them with your current profiles. Would you like to proceed?",
        QMessageBox::Yes | QMessageBox::No
    );
    if (reply == QMessageBox::No) return;

    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }

    QJsonArray arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        FolderLayoutRule restoredRule = MainWindow::jsonToRule(arr[i].toObject());
        // Avoid adding duplicate names by removing existing rule if it has the same name
        for (int j = 0; j < m_rules.size(); ++j) {
            if (m_rules[j].name.toLower() == restoredRule.name.toLower()) {
                m_rules.removeAt(j);
                break;
            }
        }
        m_rules.append(restoredRule);
    }

    populateList();
    if (m_rules.size() > 0) {
        m_listWidget->setCurrentRow(0);
    }
    QMessageBox::information(this, "Restore Successful", "Folder profiles restored and merged successfully!");
}

void FolderLayoutDialog::onEditToolbarsShortcut() {
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin) {
        ToolbarEditorDialog dlg(mainWin);
        if (dlg.exec() == QDialog::Accepted) {
            mainWin->rebuildToolBars();
            // Re-harvest current profile layout if they wish
        }
    }
}

void FolderLayoutDialog::onEditMenusShortcut() {
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin) {
        CustomMenuEditorDialog dlg(mainWin);
        if (dlg.exec() == QDialog::Accepted) {
            mainWin->rebuildCustomMenus();
        }
    }
}
