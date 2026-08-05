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
        resize(450, 450);
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
        resize(450, 450);
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
    
    bool hasDefault = false;
    for (const auto& r : m_rules) {
        if (r.name.toLower() == "default") {
            hasDefault = true;
            break;
        }
    }
    if (!hasDefault) {
        FolderLayoutRule def;
        def.name = "Default";
        def.ruleType = "Path";
        def.value = "";
        def.autoApply = true;
        def.viewMode = "List";
        m_rules.prepend(def);
    }

    setWindowTitle("Folder Profiles & Layouts");
    resize(1020, 640);
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

    QLabel* listLabel = new QLabel("Profiles & Templates:", this);
    listLabel->setStyleSheet("font-weight: bold; color: #89b4fa;");
    listLabel->setToolTip("Select a folder profile assignment or layout template from this list to configure its properties.");
    leftLayout->addWidget(listLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setFixedWidth(290);
    m_listWidget->setToolTip("List of active folder profiles and layout templates. Green switch = Active (Enabled), Gray = Inactive (Disabled).");
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &FolderLayoutDialog::onListWidgetContextMenu);
    leftLayout->addWidget(m_listWidget);

    QHBoxLayout* addButtonsLayout = new QHBoxLayout();
    m_btnAdd = new QPushButton("+ Add Profile", this);
    m_btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; padding: 6px 8px; }"
                            "QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    m_btnAdd->setToolTip("Create a new Folder Profile mapping a specific path to a Layout Template.");
    connect(m_btnAdd, &QPushButton::clicked, this, &FolderLayoutDialog::onAddProfile);
    
    m_btnAddTemplate = new QPushButton("+ Add Template", this);
    m_btnAddTemplate->setStyleSheet("QPushButton { background-color: #313244; color: #89b4fa; border: 1px solid #45475a; padding: 6px 8px; }"
                                    "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    m_btnAddTemplate->setToolTip("Create a new standalone Layout Template preset (view mode, toolbars, visualizer settings).");
    connect(m_btnAddTemplate, &QPushButton::clicked, this, &FolderLayoutDialog::onAddTemplate);

    m_btnDelete = new QPushButton("🗑 Delete Profile / Template", this);
    m_btnDelete->setStyleSheet("QPushButton { background-color: #313244; color: #f38ba8; border: 1px solid #45475a; }"
                               "QPushButton:hover { background-color: #f38ba8; color: #11111b; }");
    m_btnDelete->setToolTip("Delete the selected profile or layout template. (Prompts for confirmation before deleting).");
    connect(m_btnDelete, &QPushButton::clicked, this, &FolderLayoutDialog::onDeleteProfile);

    addButtonsLayout->addWidget(m_btnAdd);
    addButtonsLayout->addWidget(m_btnAddTemplate);
    leftLayout->addLayout(addButtonsLayout);
    leftLayout->addWidget(m_btnDelete);

    QHBoxLayout* orderButtons = new QHBoxLayout();
    m_btnMoveUp = new QPushButton("▲ Move Up", this);
    m_btnMoveUp->setToolTip("Move selected profile higher in priority. Profiles near the top take precedence when matching folder paths.");
    connect(m_btnMoveUp, &QPushButton::clicked, this, &FolderLayoutDialog::onMoveUpProfile);

    m_btnMoveDown = new QPushButton("▼ Move Down", this);
    m_btnMoveDown->setToolTip("Move selected profile lower in priority.");
    connect(m_btnMoveDown, &QPushButton::clicked, this, &FolderLayoutDialog::onMoveDownProfile);

    orderButtons->addWidget(m_btnMoveUp);
    orderButtons->addWidget(m_btnMoveDown);
    leftLayout->addLayout(orderButtons);

    QHBoxLayout* backupRestoreButtons = new QHBoxLayout();
    m_btnBackup = new QPushButton("📦 Backup", this);
    m_btnBackup->setStyleSheet("QPushButton { background-color: #313244; color: #89b4fa; border: 1px solid #45475a; }"
                               "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    m_btnBackup->setToolTip("Export all folder profiles and layout templates to a JSON backup file.");
    connect(m_btnBackup, &QPushButton::clicked, this, &FolderLayoutDialog::onBackupProfiles);

    m_btnRestore = new QPushButton("📥 Restore", this);
    m_btnRestore->setStyleSheet("QPushButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; }"
                                "QPushButton:hover { background-color: #f9e2af; color: #11111b; }");
    m_btnRestore->setToolTip("Import folder profiles and layout templates from a JSON backup file.");
    connect(m_btnRestore, &QPushButton::clicked, this, &FolderLayoutDialog::onRestoreProfiles);

    backupRestoreButtons->addWidget(m_btnBackup);
    backupRestoreButtons->addWidget(m_btnRestore);
    leftLayout->addLayout(backupRestoreButtons);

    m_btnApplyCurrentFolder = new QPushButton("⚡ Profile Current Folder", this);
    m_btnApplyCurrentFolder->setStyleSheet("QPushButton { background-color: #313244; color: #b4befe; border: 1px solid #45475a; }"
                                           "QPushButton:hover { background-color: #b4befe; color: #11111b; }");
    m_btnApplyCurrentFolder->setToolTip("Instantly create a folder profile for the currently open directory path in the active file panel.");
    connect(m_btnApplyCurrentFolder, &QPushButton::clicked, this, &FolderLayoutDialog::onApplyToCurrentFolder);
    leftLayout->addWidget(m_btnApplyCurrentFolder);

    QHBoxLayout* shortcutRow1 = new QHBoxLayout();
    QPushButton* btnEditToolbars = new QPushButton("Edit Toolbars...", this);
    btnEditToolbars->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; padding: 6px 8px; }"
                                   "QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnEditToolbars->setToolTip("Configure custom toolbar button definitions, icons, and shell/action commands.");
    connect(btnEditToolbars, &QPushButton::clicked, this, &FolderLayoutDialog::onEditToolbarsShortcut);

    QPushButton* btnEditMenus = new QPushButton("Edit Menus...", this);
    btnEditMenus->setStyleSheet("QPushButton { background-color: #313244; color: #cba6f7; border: 1px solid #45475a; padding: 6px 8px; }"
                                "QPushButton:hover { background-color: #cba6f7; color: #11111b; }");
    btnEditMenus->setToolTip("Configure custom right-click context menu actions and commands.");
    connect(btnEditMenus, &QPushButton::clicked, this, &FolderLayoutDialog::onEditMenusShortcut);

    shortcutRow1->addWidget(btnEditToolbars);
    shortcutRow1->addWidget(btnEditMenus);
    leftLayout->addLayout(shortcutRow1);

    QPushButton* btnEditContextMenus = new QPushButton("Edit Context Menus...", this);
    btnEditContextMenus->setStyleSheet("QPushButton { background-color: #313244; color: #fab387; border: 1px solid #45475a; padding: 6px 8px; }"
                                       "QPushButton:hover { background-color: #fab387; color: #11111b; }");
    btnEditContextMenus->setToolTip("Configure custom right-click context menu items for files and folders.");
    connect(btnEditContextMenus, &QPushButton::clicked, this, &FolderLayoutDialog::onEditContextMenusShortcut);
    leftLayout->addWidget(btnEditContextMenus);

    mainLayout->addLayout(leftLayout);

    // ================= RIGHT DETAIL COLUMN =================
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    m_editorWidget = new QWidget(this);
    QVBoxLayout* editorLayout = new QVBoxLayout(m_editorWidget);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    QHBoxLayout* modeHeaderLayout = new QHBoxLayout();
    QLabel* editorTitle = new QLabel("Layout Configuration Settings", this);
    editorTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #89b4fa;");
    modeHeaderLayout->addWidget(editorTitle);
    
    modeHeaderLayout->addStretch();
    
    m_chkAdvancedMode = new QCheckBox("Advanced Settings Mode", this);
    m_chkAdvancedMode->setToolTip("Toggle to show/hide advanced visibility overrides, custom colors, and session tab snapshot configuration options.");
    modeHeaderLayout->addWidget(m_chkAdvancedMode);
    editorLayout->addLayout(modeHeaderLayout);

    // Scroll Area for details editor
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget* scrollContent = new QWidget(this);
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(12);
    scrollLayout->setContentsMargins(0, 0, 10, 0);

    // 1. General Profile Name & Trigger
    QGroupBox* triggerGroup = new QGroupBox("1. Folder Profile & Layout Template", this);
    QGridLayout* triggerGrid = new QGridLayout(triggerGroup);
    triggerGrid->setSpacing(8);
    triggerGrid->setColumnMinimumWidth(0, 180);
    triggerGrid->setColumnStretch(0, 0);
    triggerGrid->setColumnStretch(1, 1);
    triggerGrid->setColumnStretch(2, 0);

    triggerGrid->addWidget(new QLabel("Profile / Template Name:", this), 0, 0);
    m_editName = new QLineEdit(this);
    m_editName->setToolTip("Descriptive label for this profile or template (e.g. 'My Music Collection', 'Movies Showcase', 'TV Series').");
    m_editName->setFixedWidth(400);
    triggerGrid->addWidget(m_editName, 0, 1, 1, 2, Qt::AlignLeft);
    connect(m_editName, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
            m_rules[m_currentIndex].name = text.trimmed();
            for (int i = 0; i < m_listWidget->count(); ++i) {
                QListWidgetItem* item = m_listWidget->item(i);
                if (item && item->data(Qt::UserRole).toInt() == m_currentIndex) {
                    ProfileListItemWidget* widget = qobject_cast<ProfileListItemWidget*>(m_listWidget->itemWidget(item));
                    if (widget) {
                        QString displayName = text.trimmed();
                        if (displayName.isEmpty()) {
                            displayName = (m_rules[m_currentIndex].ruleType == "Path") ? "(Unnamed Profile)" : "(Unnamed Template)";
                        }
                        if (m_rules[m_currentIndex].ruleType == "Path" && !m_rules[m_currentIndex].linkedProfile.isEmpty()) {
                            displayName += QString(" [%1]").arg(m_rules[m_currentIndex].linkedProfile);
                        }
                        widget->setName(displayName);
                    }
                    break;
                }
            }
        }
    });

    QHBoxLayout* autoApplyLayout = new QHBoxLayout();
    m_checkAutoApply = new ToggleSwitch(this);
    m_checkAutoApply->setToolTip("Toggle Profile Active State: Green = Active (auto-applies layout on browsing folder), Gray = Inactive (disabled).");
    autoApplyLayout->addWidget(m_checkAutoApply);
    autoApplyLayout->addWidget(new QLabel("Enable Profile (Auto-apply when browsing matching folders)", this));
    autoApplyLayout->addStretch();
    triggerGrid->addLayout(autoApplyLayout, 1, 0, 1, 3);

    triggerGrid->addWidget(new QLabel("Match Condition:", this), 2, 0);
    m_comboRuleType = new QComboBox(this);
    m_comboRuleType->addItems({"Path", "Category"});
    m_comboRuleType->setToolTip("Rule match type: Select 'Path' to target a specific directory folder path, or 'Category' to target a media category type (Music, Videos, Images, Documents).");
    m_comboRuleType->setFixedWidth(400);
    triggerGrid->addWidget(m_comboRuleType, 2, 1, 1, 2, Qt::AlignLeft);
    connect(m_comboRuleType, &QComboBox::currentTextChanged, this, &FolderLayoutDialog::onRuleTypeChanged);

    triggerGrid->addWidget(new QLabel("Target Folder Path:", this), 3, 0);
    QHBoxLayout* pathRow = new QHBoxLayout();
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(8);
    m_editValue = new QLineEdit(this);
    m_editValue->setToolTip("The exact folder directory path (e.g. /home/user/Music or /media/Movies) to apply this profile layout to.");
    m_editValue->setFixedWidth(400);
    
    m_btnBrowse = new QPushButton("📂 Browse...", this);
    m_btnBrowse->setToolTip("Open file chooser dialog to select a folder directory path.");
    connect(m_btnBrowse, &QPushButton::clicked, this, &FolderLayoutDialog::onBrowseFolder);
    
    pathRow->addWidget(m_editValue);
    pathRow->addWidget(m_btnBrowse);
    pathRow->addStretch();
    triggerGrid->addLayout(pathRow, 3, 1, 1, 2);

    m_btnUseActivePath = new QPushButton("⚡ Use Active Path", this);
    m_btnUseActivePath->setToolTip("Insert the currently open folder path from the active file panel into the target path field.");
    m_btnUseActivePath->setFixedWidth(400);
    connect(m_btnUseActivePath, &QPushButton::clicked, this, &FolderLayoutDialog::onUseActivePath);
    triggerGrid->addWidget(m_btnUseActivePath, 4, 1, 1, 2, Qt::AlignLeft);

    triggerGrid->addWidget(new QLabel("Subfolder Depth Inheritance:", this), 5, 0);
    m_comboSubfolderDepth = new QComboBox(this);
    m_comboSubfolderDepth->addItem("Exact Folder Only (0 Levels Deep)", 0);
    m_comboSubfolderDepth->addItem("1 Subfolder Level Deep (Parent -> Children)", 1);
    m_comboSubfolderDepth->addItem("2 Subfolder Levels Deep (Parent -> Show -> Season)", 2);
    m_comboSubfolderDepth->addItem("3 Subfolder Levels Deep (TV Shows: Root -> Show -> Season -> Episodes)", 3);
    m_comboSubfolderDepth->addItem("Unlimited Subfolder Depth (All Nested Subdirectories)", 999);
    m_comboSubfolderDepth->setToolTip("Controls how many subfolder levels deep inside this directory will inherit this profile layout. For TV Shows, select '3 Subfolder Levels Deep' or 'Unlimited' so all Show, Season, and Episode folders automatically use this layout!");
    m_comboSubfolderDepth->setFixedWidth(400);
    triggerGrid->addWidget(m_comboSubfolderDepth, 5, 1, 1, 2, Qt::AlignLeft);

    QLabel* lblLink = new QLabel("Assigned Layout Template:", this);
    lblLink->setToolTip("Select the Layout Template (e.g. Movies Showcase, TV Series, Music Albums, Default Master) to use when opening this folder.");
    triggerGrid->addWidget(lblLink, 6, 0);

    m_comboLinkedProfile = new QComboBox(this);
    m_comboLinkedProfile->setToolTip("Choose a pre-configured Layout Template (Movies, TV Series, Music, Default, etc.) to apply to this folder, or select '(None - Custom)' to define custom rules.");
    m_comboLinkedProfile->setFixedWidth(400);
    triggerGrid->addWidget(m_comboLinkedProfile, 6, 1, 1, 2, Qt::AlignLeft);
    connect(m_comboLinkedProfile, &QComboBox::currentIndexChanged, this, &FolderLayoutDialog::onLinkedProfileChanged);

    m_labelInheritedInfo = new QLabel(this);
    m_labelInheritedInfo->setStyleSheet("color: #a6e3a1; font-weight: bold; margin-top: 4px;");
    m_labelInheritedInfo->setWordWrap(true);
    m_labelInheritedInfo->setVisible(false);
    triggerGrid->addWidget(m_labelInheritedInfo, 7, 0, 1, 3);

    scrollLayout->addWidget(triggerGroup);

    // 2. View Settings
    m_viewGroup = new QGroupBox("2. View Mode & Toolbars", this);
    QGridLayout* viewGrid = new QGridLayout(m_viewGroup);
    viewGrid->setSpacing(8);
    viewGrid->setColumnMinimumWidth(0, 180);
    viewGrid->setColumnStretch(0, 0);
    viewGrid->setColumnStretch(1, 1);

    viewGrid->addWidget(new QLabel("View Mode:", this), 0, 0);
    m_comboViewMode = new QComboBox(this);
    m_comboViewMode->addItems({"No Change", "List", "Grid", "Card", "Miller", "Timeline", "Filmstrip", "Movies Full Screen", "TV Shows Full Screen", "Music Full Screen", "Cover Flow Carousel"});
    m_comboViewMode->setFixedWidth(400);
    viewGrid->addWidget(m_comboViewMode, 0, 1, Qt::AlignLeft);

    m_lblCustomButtons = new QLabel("Filter Custom Buttons:", this);
    m_lblCustomButtons->setToolTip("Choose which user-defined custom toolbar buttons are visible in this folder profile. Manage all custom buttons via the 'Edit Toolbars...' button on the left panel.");
    viewGrid->addWidget(m_lblCustomButtons, 1, 0);

    m_btnChooseButtons = new QPushButton("All Buttons (Default)", this);
    m_btnChooseButtons->setToolTip("Select custom script/app buttons to enable for this profile. If empty, all are shown.");
    m_btnChooseButtons->setFixedWidth(400);
    connect(m_btnChooseButtons, &QPushButton::clicked, this, &FolderLayoutDialog::onChooseButtons);
    viewGrid->addWidget(m_btnChooseButtons, 1, 1, Qt::AlignLeft);

    scrollLayout->addWidget(m_viewGroup);

    // 3. Visibility States
    m_visGroup = new QGroupBox("3. Layout, Docks & Panels (On/Off States)", this);
    QGridLayout* visGrid = new QGridLayout(m_visGroup);
    visGrid->setSpacing(8);
    visGrid->setColumnMinimumWidth(0, 180);
    visGrid->setColumnStretch(0, 0);
    visGrid->setColumnStretch(1, 0);
    visGrid->setColumnStretch(2, 1);

    visGrid->addWidget(new QLabel("Layout Component", this), 0, 0);
    QLabel* hdrState = new QLabel("State (On / Off)", this);
    hdrState->setStyleSheet("font-weight: bold; color: #a6e3a1;");
    hdrState->setToolTip("Define the state when this profile is active: ON = Enabled/Visible, OFF = Disabled/Hidden.");
    visGrid->addWidget(hdrState, 0, 1);

    // Console
    m_stateConsole = new ToggleSwitch(this);
    m_stateConsole->setToolTip("Enforce Console Panel visibility: ON = Visible, OFF = Hidden.");
    visGrid->addWidget(new QLabel("Console Panel", this), 1, 0);
    visGrid->addWidget(m_stateConsole, 1, 1);

    // Preview Pane
    m_statePreview = new ToggleSwitch(this);
    m_statePreview->setToolTip("Enforce Preview Pane visibility: ON = Visible, OFF = Hidden.");
    visGrid->addWidget(new QLabel("Preview Pane", this), 2, 0);
    visGrid->addWidget(m_statePreview, 2, 1);

    // Favorites
    m_stateFavorites = new ToggleSwitch(this);
    m_stateFavorites->setToolTip("Enforce Favorites Sidebar visibility: ON = Visible, OFF = Hidden.");
    visGrid->addWidget(new QLabel("Favorites Sidebar", this), 3, 0);
    visGrid->addWidget(m_stateFavorites, 3, 1);

    // Zen
    m_stateZen = new ToggleSwitch(this);
    m_stateZen->setToolTip("Enforce Zen Mode state: ON = Enabled (Hidden Layout), OFF = Disabled.");
    visGrid->addWidget(new QLabel("Zen Mode State", this), 4, 0);
    visGrid->addWidget(m_stateZen, 4, 1);

    // Built-in Fullscreen Playback
    m_stateBuiltinPlayerDoubleclick = new ToggleSwitch(this);
    m_stateBuiltinPlayerDoubleclick->setToolTip("Enforce double-click playback preference: ON = Built-in player, OFF = System player.");
    visGrid->addWidget(new QLabel("Built-in Fullscreen", this), 5, 0);
    visGrid->addWidget(m_stateBuiltinPlayerDoubleclick, 5, 1);

    // Auto-Fullscreen playback
    m_stateFullScreenPlayer = new ToggleSwitch(this);
    m_stateFullScreenPlayer->setToolTip("Enforce auto-fullscreen on playback preference: ON = Auto Fullscreen, OFF = Standard player.");
    visGrid->addWidget(new QLabel("Auto Fullscreen", this), 6, 0);
    visGrid->addWidget(m_stateFullScreenPlayer, 6, 1);

    // Audio Visualizer
    m_stateVisualizer = new ToggleSwitch(this);
    m_stateVisualizer->setToolTip("Enforce audio spectrum visualizer visibility: ON = Visible, OFF = Hidden.");
    visGrid->addWidget(new QLabel("Audio Visualizer", this), 7, 0);
    visGrid->addWidget(m_stateVisualizer, 7, 1);

    // Dual Pane View
    m_stateDualPane = new ToggleSwitch(this);
    m_stateDualPane->setToolTip("Enforce Dual Pane view state: ON = Dual Pane, OFF = Single Pane.");
    visGrid->addWidget(new QLabel("Dual Pane View", this), 8, 0);
    visGrid->addWidget(m_stateDualPane, 8, 1);

    // Split Panels Horizontally
    m_stateHorizontalSplit = new ToggleSwitch(this);
    m_stateHorizontalSplit->setToolTip("Enforce horizontal panels split orientation: ON = Stacked horizontally (Top/Bottom), OFF = Vertical (Side-by-side).");
    visGrid->addWidget(new QLabel("Split Panels Horizontally", this), 9, 0);
    visGrid->addWidget(m_stateHorizontalSplit, 9, 1);

    // CD Artwork Overlays
    m_stateCasingOverlays = new ToggleSwitch(this);
    m_stateCasingOverlays->setToolTip("Enforce media casing overlays (CD/DVD cases): ON = Enabled, OFF = Disabled.");
    visGrid->addWidget(new QLabel("CD Artwork Overlay", this), 10, 0);
    visGrid->addWidget(m_stateCasingOverlays, 10, 1);

    // Toolbars
    m_stateToolbars = new ToggleSwitch(this);
    m_stateToolbars->setToolTip("Enforce active toolbars list.");
    m_btnSelectToolbars = new QPushButton("Select Toolbars...", this);
    m_btnSelectToolbars->setToolTip("Choose which toolbar panels are displayed.");
    m_btnSelectToolbars->setEnabled(false);
    m_btnSelectToolbars->setFixedWidth(400);
    connect(m_btnSelectToolbars, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectToolbars);
    connect(m_stateToolbars, &ToggleSwitch::toggled, m_btnSelectToolbars, &QPushButton::setEnabled);
    visGrid->addWidget(new QLabel("Toolbars", this), 12, 0);
    visGrid->addWidget(m_stateToolbars, 12, 1);
    visGrid->addWidget(m_btnSelectToolbars, 12, 2, Qt::AlignLeft);
 
    // Menus
    m_stateMenus = new ToggleSwitch(this);
    m_stateMenus->setToolTip("Enforce active context menus list.");
    m_btnSelectMenus = new QPushButton("Select Menus...", this);
    m_btnSelectMenus->setToolTip("Choose which right-click context menus are active.");
    m_btnSelectMenus->setEnabled(false);
    m_btnSelectMenus->setFixedWidth(400);
    connect(m_btnSelectMenus, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectMenus);
    connect(m_stateMenus, &ToggleSwitch::toggled, m_btnSelectMenus, &QPushButton::setEnabled);
    visGrid->addWidget(new QLabel("Menus", this), 13, 0);
    visGrid->addWidget(m_stateMenus, 13, 1);
    visGrid->addWidget(m_btnSelectMenus, 13, 2, Qt::AlignLeft);

    scrollLayout->addWidget(m_visGroup);

    // 4. Styling (Custom Background Color & Image)
    m_styleGroup = new QGroupBox("4. Look & Feel Custom Background", this);
    QVBoxLayout* styleLayout = new QVBoxLayout(m_styleGroup);
    styleLayout->setSpacing(8);

    QHBoxLayout* colorRow = new QHBoxLayout();
    m_useBgColor = new ToggleSwitch(this);
    m_btnSelectBgColor = new QPushButton("Select Color...", this);
    connect(m_btnSelectBgColor, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectBgColor);
    connect(m_useBgColor, &ToggleSwitch::toggled, m_btnSelectBgColor, &QPushButton::setEnabled);
    colorRow->addWidget(m_useBgColor);
    colorRow->addWidget(new QLabel("Use Custom Background Color", this));
    colorRow->addWidget(m_btnSelectBgColor);
    colorRow->addStretch();
    styleLayout->addLayout(colorRow);

    QHBoxLayout* imageRow = new QHBoxLayout();
    m_useBgImage = new ToggleSwitch(this);
    m_btnSelectBgImage = new QPushButton("Select Image...", this);
    m_lblBgImagePath = new QLabel("", this);
    m_lblBgImagePath->setStyleSheet("color: #a6adc8; font-size: 11px;");
    connect(m_btnSelectBgImage, &QPushButton::clicked, this, &FolderLayoutDialog::onSelectBgImage);
    connect(m_useBgImage, &ToggleSwitch::toggled, m_btnSelectBgImage, &QPushButton::setEnabled);
    imageRow->addWidget(m_useBgImage);
    imageRow->addWidget(new QLabel("Use Custom Background Image", this));
    imageRow->addWidget(m_btnSelectBgImage);
    imageRow->addWidget(m_lblBgImagePath);
    imageRow->addStretch();
    styleLayout->addLayout(imageRow);

    QHBoxLayout* opacityRow = new QHBoxLayout();
    opacityRow->setContentsMargins(40, 0, 0, 0); // Indent to align with the toggle switch
    m_sliderBgOpacity = new QSlider(Qt::Horizontal, this);
    m_sliderBgOpacity->setRange(0, 100);
    m_sliderBgOpacity->setValue(100);
    m_sliderBgOpacity->setFixedWidth(150);
    m_sliderBgOpacity->setEnabled(false);
    m_sliderBgOpacity->setStyleSheet(
        "QSlider::groove:horizontal { border: 1px solid #313244; height: 6px; background: #181825; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #b4befe; width: 12px; margin: -3px 0; border-radius: 6px; }"
    );

    m_lblBgOpacityValue = new QLabel("100%", this);
    m_lblBgOpacityValue->setFixedWidth(40);
    m_lblBgOpacityValue->setEnabled(false);

    connect(m_sliderBgOpacity, &QSlider::valueChanged, this, [this](int val) {
        m_lblBgOpacityValue->setText(QString("%1%").arg(val));
    });
    connect(m_useBgImage, &ToggleSwitch::toggled, m_sliderBgOpacity, &QSlider::setEnabled);
    connect(m_useBgImage, &ToggleSwitch::toggled, m_lblBgOpacityValue, &QLabel::setEnabled);

    opacityRow->addWidget(new QLabel("Wallpaper Opacity:", this));
    opacityRow->addWidget(m_sliderBgOpacity);
    opacityRow->addWidget(m_lblBgOpacityValue);
    opacityRow->addStretch();
    styleLayout->addLayout(opacityRow);

    scrollLayout->addWidget(m_styleGroup);

    // 5. Session Tab Snapshots
    m_tabsGroup = new QGroupBox("5. Tab Snapshots (For Manual Session Restoring)", this);
    QVBoxLayout* tabsLayout = new QVBoxLayout(m_tabsGroup);
    tabsLayout->setSpacing(6);

    QHBoxLayout* tabsSnapshotHeaderLayout = new QHBoxLayout();
    m_hasTabsSnapshot = new ToggleSwitch(this);
    tabsSnapshotHeaderLayout->addWidget(m_hasTabsSnapshot);
    tabsSnapshotHeaderLayout->addWidget(new QLabel("Include Open Tabs Snapshot", this));
    tabsSnapshotHeaderLayout->addStretch();
    tabsLayout->addLayout(tabsSnapshotHeaderLayout);

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

    scrollLayout->addWidget(m_tabsGroup);

    scrollArea->setWidget(scrollContent);
    editorLayout->addWidget(scrollArea);
    
    rightLayout->addWidget(m_editorWidget, 1);

    // Bottom Action Row inside details pane
    QHBoxLayout* activeActionsRow = new QHBoxLayout();
    m_btnCaptureUI = new QPushButton("📸 Capture Current UI State", this);
    m_btnCaptureUI->setStyleSheet("QPushButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; }");
    m_btnCaptureUI->setToolTip("Snapshot current window layout, view mode, and toolbar states directly into this profile template.");
    connect(m_btnCaptureUI, &QPushButton::clicked, this, &FolderLayoutDialog::onCaptureUI);

    m_btnApplyNow = new QPushButton("⚡ Apply Profile to Current View Now", this);
    m_btnApplyNow->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
    m_btnApplyNow->setToolTip("Immediately apply this profile's layout settings to the currently active file pane.");
    connect(m_btnApplyNow, &QPushButton::clicked, this, &FolderLayoutDialog::onApplyNow);

    activeActionsRow->addWidget(m_btnCaptureUI);
    activeActionsRow->addWidget(m_btnApplyNow);
    activeActionsRow->addStretch();
    rightLayout->addLayout(activeActionsRow);

    // Dialog buttons (Save & Cancel)
    QHBoxLayout* dialogButtons = new QHBoxLayout();
    dialogButtons->addStretch();
    QPushButton* btnSave = new QPushButton("💾 Save & Apply Profiles", this);
    btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
    btnSave->setToolTip("Save all folder profile assignments and layout templates and update active workspace.");
    connect(btnSave, &QPushButton::clicked, this, &FolderLayoutDialog::onSave);

    QPushButton* btnCancel = new QPushButton("❌ Close", this);
    btnCancel->setToolTip("Close layout profile editor.");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    dialogButtons->addWidget(btnSave);
    dialogButtons->addWidget(btnCancel);
    rightLayout->addLayout(dialogButtons);

    mainLayout->addLayout(rightLayout, 1);

    // Initialize Advanced Settings Mode based on QSettings
    QSettings settings("Amifiles", "Amifiles");
    bool advMode = settings.value("preferences/folder_layouts_advanced_mode", false).toBool();
    m_chkAdvancedMode->setChecked(advMode);
    
    // Connect advanced checkbox toggling
    connect(m_chkAdvancedMode, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings("Amifiles", "Amifiles");
        settings.setValue("preferences/folder_layouts_advanced_mode", checked);
        updateModeVisibility(checked);
    });

    updateModeVisibility(advMode);

    connect(m_listWidget, &QListWidget::currentRowChanged, this, &FolderLayoutDialog::onProfileSelected);
}

void FolderLayoutDialog::updateModeVisibility(bool advanced) {
    if (m_visGroup) m_visGroup->setVisible(true); // Always visible
    if (m_styleGroup) m_styleGroup->setVisible(advanced);
    if (m_tabsGroup) m_tabsGroup->setVisible(advanced);
    if (m_lblCustomButtons) m_lblCustomButtons->setVisible(advanced);
    if (m_btnChooseButtons) m_btnChooseButtons->setVisible(advanced);
}

void FolderLayoutDialog::populateList() {
    m_isPopulating = true;
    m_listWidget->clear();

    // --- SECTION 0: GLOBAL DEFAULT PROFILE ---
    QListWidgetItem* hdrDefault = new QListWidgetItem("★ GLOBAL DEFAULT PROFILE", m_listWidget);
    hdrDefault->setFlags(Qt::NoItemFlags);
    hdrDefault->setForeground(QColor("#f38ba8")); // Red
    hdrDefault->setBackground(QColor("#181825"));
    hdrDefault->setFont(QFont("sans-serif", 9, QFont::Bold));
    hdrDefault->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    int defaultIndex = -1;
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].name.toLower() == "default") {
            defaultIndex = i;
            break;
        }
    }

    if (defaultIndex != -1) {
        QListWidgetItem* item = new QListWidgetItem(m_listWidget);
        item->setSizeHint(QSize(200, 36));
        item->setData(Qt::UserRole, defaultIndex);

        ProfileListItemWidget* widget = new ProfileListItemWidget("Default Layout Profile (Fallback)", m_rules[defaultIndex].autoApply, this);
        widget->setTextColor("#f38ba8"); // Styled in red
        connect(widget, &ProfileListItemWidget::toggled, this, [this, defaultIndex](bool checked) {
            m_rules[defaultIndex].autoApply = checked;
            if (m_currentIndex == defaultIndex) {
                m_checkAutoApply->setChecked(checked);
            }
        });
        m_listWidget->setItemWidget(item, widget);
    }

    // --- SECTION 1: LAYOUT TEMPLATES HEADER ---
    QListWidgetItem* hdrTemplates = new QListWidgetItem("❖ LAYOUT TEMPLATES", m_listWidget);
    hdrTemplates->setFlags(Qt::NoItemFlags);
    hdrTemplates->setForeground(QColor("#89b4fa"));
    hdrTemplates->setBackground(QColor("#181825"));
    hdrTemplates->setFont(QFont("sans-serif", 9, QFont::Bold));
    hdrTemplates->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    for (int i = 0; i < m_rules.size(); ++i) {
        const auto& r = m_rules[i];
        if (r.name.toLower() == "default") continue; // Exclude default fallback rule

        if (r.ruleType == "Category" || r.ruleType == "Template" || (r.value.isEmpty() && r.linkedProfile.isEmpty())) {
            QListWidgetItem* item = new QListWidgetItem(m_listWidget);
            item->setSizeHint(QSize(200, 36));
            item->setData(Qt::UserRole, i);

            QString displayName = r.name.isEmpty() ? "(Unnamed Template)" : r.name;
            ProfileListItemWidget* widget = new ProfileListItemWidget(displayName, r.autoApply, this);
            connect(widget, &ProfileListItemWidget::toggled, this, [this, i](bool checked) {
                m_rules[i].autoApply = checked;
                if (m_currentIndex == i) {
                    m_checkAutoApply->setChecked(checked);
                }
            });
            m_listWidget->setItemWidget(item, widget);
        }
    }

    // --- SECTION 2: FOLDER PROFILES HEADER ---
    QListWidgetItem* hdrProfiles = new QListWidgetItem("📁 FOLDER PROFILES", m_listWidget);
    hdrProfiles->setFlags(Qt::NoItemFlags);
    hdrProfiles->setForeground(QColor("#a6e3a1"));
    hdrProfiles->setBackground(QColor("#181825"));
    hdrProfiles->setFont(QFont("sans-serif", 9, QFont::Bold));
    hdrProfiles->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    for (int i = 0; i < m_rules.size(); ++i) {
        const auto& r = m_rules[i];
        if (r.name.toLower() == "default") continue; // Exclude default fallback rule

        if (r.ruleType == "Path" && !r.value.isEmpty()) {
            QListWidgetItem* item = new QListWidgetItem(m_listWidget);
            item->setSizeHint(QSize(200, 36));
            item->setData(Qt::UserRole, i);

            QString displayName = r.name.isEmpty() ? "(Unnamed Profile)" : r.name;
            if (!r.linkedProfile.isEmpty()) {
                displayName += QString(" [%1]").arg(r.linkedProfile);
            }
            ProfileListItemWidget* widget = new ProfileListItemWidget(displayName, r.autoApply, this);
            connect(widget, &ProfileListItemWidget::toggled, this, [this, i](bool checked) {
                m_rules[i].autoApply = checked;
                if (m_currentIndex == i) {
                    m_checkAutoApply->setChecked(checked);
                }
            });
            m_listWidget->setItemWidget(item, widget);
        }
    }
    m_isPopulating = false;
    if (m_listWidget->count() == 0) {
        m_currentIndex = -1;
        m_editorWidget->setEnabled(false);
    }
}

void FolderLayoutDialog::populateFields(const FolderLayoutRule& r) {
    bool isDefault = (r.name.toLower() == "default");

    m_editName->setText(r.name);
    m_editName->setEnabled(!isDefault);

    m_checkAutoApply->setChecked(r.autoApply);
    m_checkAutoApply->setEnabled(!isDefault);

    m_comboRuleType->setCurrentText(r.ruleType);
    m_comboRuleType->setEnabled(!isDefault);

    m_editValue->setText(r.value);
    m_editValue->setEnabled(!isDefault);
    m_btnBrowse->setEnabled(!isDefault);
    m_btnUseActivePath->setEnabled(!isDefault);

    int depthIdx = m_comboSubfolderDepth->findData(r.subfolderDepth);
    if (depthIdx != -1) {
        m_comboSubfolderDepth->setCurrentIndex(depthIdx);
    } else {
        m_comboSubfolderDepth->setCurrentIndex(3); // Default to 3 levels deep
    }
    m_comboSubfolderDepth->setEnabled(!isDefault);

    updateLinkedProfileCombo();
    int linkIdx = m_comboLinkedProfile->findData(r.linkedProfile);
    if (linkIdx != -1) {
        m_comboLinkedProfile->setCurrentIndex(linkIdx);
    } else {
        m_comboLinkedProfile->setCurrentIndex(0);
    }
    m_comboLinkedProfile->setEnabled(!isDefault);

    bool isLinked = (m_comboLinkedProfile->currentIndex() > 0);
    m_viewGroup->setEnabled(!isLinked);
    m_visGroup->setEnabled(!isLinked);
    m_styleGroup->setEnabled(!isLinked);
    m_tabsGroup->setEnabled(!isLinked);
    if (m_btnCaptureUI) m_btnCaptureUI->setEnabled(!isLinked);
    if (m_btnApplyNow) m_btnApplyNow->setEnabled(!isLinked);

    if (isLinked) {
        m_labelInheritedInfo->setText(QString("Note: This profile inherits all layout settings from '%1'.").arg(m_comboLinkedProfile->currentText()));
        m_labelInheritedInfo->setVisible(true);
    } else {
        m_labelInheritedInfo->setVisible(false);
    }

    // View settings
    m_comboViewMode->setCurrentText(r.viewMode.isEmpty() ? "No Change" : r.viewMode);
    
    // Choose buttons button text
    m_btnChooseButtons->setProperty("selectedButtons", r.customButtons);
    m_btnChooseButtons->setText(r.customButtons.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(r.customButtons.size()));

    // Visibility checkboxes
    m_stateConsole->setChecked(r.consoleVisible);
    m_statePreview->setChecked(r.previewVisible);
    m_stateFavorites->setChecked(r.favoritesSidebarVisible);
    m_stateZen->setChecked(r.zenModeActive);
    m_stateBuiltinPlayerDoubleclick->setChecked(r.builtinPlayerDoubleclick);
    m_stateFullScreenPlayer->setChecked(r.fullScreenPlayerActive);
    m_stateVisualizer->setChecked(r.visualizerActive);
    m_stateDualPane->setChecked(r.dualPaneActive);
    m_stateHorizontalSplit->setChecked(r.horizontalSplitActive);
    m_stateCasingOverlays->setChecked(r.casingOverlaysActive);

    // Toolbar & Menu Overrides
    m_stateToolbars->setChecked(r.overrideToolbars);
    m_btnSelectToolbars->setEnabled(r.overrideToolbars);
    m_selectedToolbars = r.selectedToolbars;
    m_btnSelectToolbars->setText(r.selectedToolbars.isEmpty() ? "Select Toolbars..." : QString("%1 Selected").arg(r.selectedToolbars.size()));

    m_stateMenus->setChecked(r.overrideMenus);
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

    m_useBgImage->setChecked(r.useBgImage);
    m_selectedBgImage = r.bgImage;
    m_btnSelectBgImage->setEnabled(r.useBgImage);
    m_lblBgImagePath->setText(r.bgImage.isEmpty() ? "No image selected" : QFileInfo(r.bgImage).fileName());
    m_lblBgImagePath->setToolTip(r.bgImage);
    if (m_sliderBgOpacity && m_lblBgOpacityValue) {
        int val = static_cast<int>(r.bgOpacity * 100.0);
        m_sliderBgOpacity->setValue(val);
        m_sliderBgOpacity->setEnabled(r.useBgImage);
        m_lblBgOpacityValue->setText(QString("%1%").arg(val));
        m_lblBgOpacityValue->setEnabled(r.useBgImage);
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
    r.linkedProfile = m_comboLinkedProfile->currentData().toString();
    r.subfolderDepth = m_comboSubfolderDepth->currentData().toInt();

    r.viewMode = m_comboViewMode->currentText();
    r.customButtons = m_btnChooseButtons->property("selectedButtons").toStringList();

    r.overrideConsole = true;
    r.consoleVisible = m_stateConsole->isChecked();

    r.overridePreview = true;
    r.previewVisible = m_statePreview->isChecked();

    r.overrideFavoritesSidebar = true;
    r.favoritesSidebarVisible = m_stateFavorites->isChecked();

    r.overrideZenMode = true;
    r.zenModeActive = m_stateZen->isChecked();

    r.overrideBuiltinPlayerDoubleclick = true;
    r.builtinPlayerDoubleclick = m_stateBuiltinPlayerDoubleclick->isChecked();

    r.overrideFullScreenPlayer = true;
    r.fullScreenPlayerActive = m_stateFullScreenPlayer->isChecked();

    r.overrideVisualizer = true;
    r.visualizerActive = m_stateVisualizer->isChecked();

    r.overrideDualPane = true;
    r.dualPaneActive = m_stateDualPane->isChecked();

    r.overrideHorizontalSplit = true;
    r.horizontalSplitActive = m_stateHorizontalSplit->isChecked();

    r.overrideCasingOverlays = true;
    r.casingOverlaysActive = m_stateCasingOverlays->isChecked();

    r.overrideToolbars = m_stateToolbars->isChecked();
    r.selectedToolbars = m_selectedToolbars;

    r.overrideMenus = m_stateMenus->isChecked();
    r.selectedMenus = m_selectedMenus;

    r.useBgColor = m_useBgColor->isChecked();
    r.bgColor = m_selectedBgColor;

    r.useBgImage = m_useBgImage->isChecked();
    r.bgImage = m_selectedBgImage;
    if (m_sliderBgOpacity) {
        r.bgOpacity = m_sliderBgOpacity->value() / 100.0;
    }

    r.hasTabsSnapshot = m_hasTabsSnapshot->isChecked();
    r.windowState = m_capturedWindowState;

    // Update list widget item text and state dynamically
    for (int row = 0; row < m_listWidget->count(); ++row) {
        QListWidgetItem* item = m_listWidget->item(row);
        if (item && (item->flags() & Qt::ItemIsSelectable) && item->data(Qt::UserRole).toInt() == index) {
            ProfileListItemWidget* widget = qobject_cast<ProfileListItemWidget*>(m_listWidget->itemWidget(item));
            if (widget) {
                QString displayName = r.name.isEmpty() ? "(Unnamed Profile)" : r.name;
                if (r.name.toLower() == "default") {
                    displayName = "Default Layout Profile (Fallback)";
                } else if (r.ruleType == "Path" && !r.value.isEmpty() && !r.linkedProfile.isEmpty()) {
                    displayName += QString(" [%1]").arg(r.linkedProfile);
                }
                widget->setName(displayName);
                widget->setChecked(r.autoApply);
            }
            break;
        }
    }
}

void FolderLayoutDialog::updateLinkedProfileCombo() {
    m_comboLinkedProfile->blockSignals(true);
    m_comboLinkedProfile->clear();
    m_comboLinkedProfile->addItem("(None - Configure Custom Layout)", QString());
    
    for (int i = 0; i < m_rules.size(); ++i) {
        QString name = m_rules[i].name;
        if (!name.isEmpty()) {
            m_comboLinkedProfile->addItem(name, name);
        }
    }
    m_comboLinkedProfile->blockSignals(false);
}

void FolderLayoutDialog::onLinkedProfileChanged(int index) {
    if (m_currentIndex < 0 || m_currentIndex >= m_rules.size()) return;
    
    QString linked = m_comboLinkedProfile->itemData(index).toString();
    m_rules[m_currentIndex].linkedProfile = linked;
    
    bool isLinked = !linked.isEmpty();
    m_viewGroup->setEnabled(!isLinked);
    m_visGroup->setEnabled(!isLinked);
    m_styleGroup->setEnabled(!isLinked);
    m_tabsGroup->setEnabled(!isLinked);
    if (m_btnCaptureUI) m_btnCaptureUI->setEnabled(!isLinked);
    if (m_btnApplyNow) m_btnApplyNow->setEnabled(!isLinked);
    
    if (isLinked) {
        m_labelInheritedInfo->setText(QString("Note: This profile inherits all layout settings from '%1'.").arg(linked));
        m_labelInheritedInfo->setVisible(true);
    } else {
        m_labelInheritedInfo->setVisible(false);
    }
}

void FolderLayoutDialog::onProfileSelected(int row) {
    if (m_isPopulating) return;
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }
    if (row < 0 || row >= m_listWidget->count()) {
        m_currentIndex = -1;
        m_editorWidget->setEnabled(false);
        return;
    }

    QListWidgetItem* item = m_listWidget->item(row);
    if (!item || (item->flags() & Qt::ItemIsSelectable) == 0) {
        m_currentIndex = -1;
        m_editorWidget->setEnabled(false);
        return;
    }

    int realIndex = item->data(Qt::UserRole).toInt();
    m_currentIndex = realIndex;
    if (realIndex >= 0 && realIndex < m_rules.size()) {
        m_editorWidget->setEnabled(true);
        populateFields(m_rules[realIndex]);

        bool isDefault = (m_rules[realIndex].name.toLower() == "default");
        m_btnDelete->setEnabled(!isDefault);
        m_btnMoveUp->setEnabled(!isDefault);
        m_btnMoveDown->setEnabled(!isDefault);
    } else {
        m_editorWidget->setEnabled(false);
        m_btnDelete->setEnabled(false);
        m_btnMoveUp->setEnabled(false);
        m_btnMoveDown->setEnabled(false);
    }
}

void FolderLayoutDialog::onAddProfile() {
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }
    FolderLayoutRule r;
    r.name = QString("New Folder Profile %1").arg(m_rules.size() + 1);
    r.ruleType = "Path";
    r.value = "/path/to/folder";
    r.linkedProfile = "Default Master";
    r.viewMode = "No Change";
    m_rules.append(r);
    
    populateList();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item && item->data(Qt::UserRole).toInt() == m_rules.size() - 1) {
            m_listWidget->setCurrentRow(i);
            break;
        }
    }
}

void FolderLayoutDialog::onAddTemplate() {
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
    }
    FolderLayoutRule r;
    r.name = QString("Custom Template %1").arg(m_rules.size() + 1);
    r.ruleType = "Template";
    r.value = "";
    r.linkedProfile = "";
    r.viewMode = "Music Showcase";
    m_rules.append(r);
    
    populateList();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item && item->data(Qt::UserRole).toInt() == m_rules.size() - 1) {
            m_listWidget->setCurrentRow(i);
            break;
        }
    }
}

void FolderLayoutDialog::onDeleteProfile() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_listWidget->count()) return;
    QListWidgetItem* item = m_listWidget->item(row);
    if (!item || (item->flags() & Qt::ItemIsSelectable) == 0) return;

    int realIndex = item->data(Qt::UserRole).toInt();
    if (realIndex < 0 || realIndex >= m_rules.size()) return;

    const auto& r = m_rules[realIndex];
    if (r.name.toLower() == "default") {
        QMessageBox::warning(this, "Cannot Delete", "The Global Default Profile is the fallback layout for all folders and cannot be deleted.");
        return;
    }
    if (QMessageBox::question(this, "Confirm Delete", QString("Are you sure you want to delete '%1'?").arg(r.name)) == QMessageBox::Yes) {
        m_rules.removeAt(realIndex);
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
    if (row <= 0 || row >= m_listWidget->count()) return;
    QListWidgetItem* item = m_listWidget->item(row);
    if (!item || (item->flags() & Qt::ItemIsSelectable) == 0) return;

    int realIndex = item->data(Qt::UserRole).toInt();
    if (realIndex < 0 || realIndex >= m_rules.size()) return;

    harvestCurrentProfile(realIndex);

    int prevRow = row - 1;
    QListWidgetItem* prevItem = nullptr;
    while (prevRow >= 0) {
        QListWidgetItem* tempItem = m_listWidget->item(prevRow);
        if (tempItem && (tempItem->flags() & Qt::ItemIsSelectable)) {
            prevItem = tempItem;
            break;
        }
        prevRow--;
    }

    if (!prevItem) return;

    int prevRealIndex = prevItem->data(Qt::UserRole).toInt();
    if (prevRealIndex < 0 || prevRealIndex >= m_rules.size()) return;

    if (m_rules[prevRealIndex].name.toLower() == "default") return;

    m_currentIndex = -1;
    m_rules.swapItemsAt(realIndex, prevRealIndex);
    populateList();

    for (int r = 0; r < m_listWidget->count(); ++r) {
        QListWidgetItem* checkItem = m_listWidget->item(r);
        if (checkItem && (checkItem->flags() & Qt::ItemIsSelectable) && checkItem->data(Qt::UserRole).toInt() == prevRealIndex) {
            m_listWidget->setCurrentRow(r);
            break;
        }
    }
}

void FolderLayoutDialog::onMoveDownProfile() {
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_listWidget->count()) return;
    QListWidgetItem* item = m_listWidget->item(row);
    if (!item || (item->flags() & Qt::ItemIsSelectable) == 0) return;

    int realIndex = item->data(Qt::UserRole).toInt();
    if (realIndex < 0 || realIndex >= m_rules.size()) return;

    harvestCurrentProfile(realIndex);

    int nextRow = row + 1;
    QListWidgetItem* nextItem = nullptr;
    while (nextRow < m_listWidget->count()) {
        QListWidgetItem* tempItem = m_listWidget->item(nextRow);
        if (tempItem && (tempItem->flags() & Qt::ItemIsSelectable)) {
            nextItem = tempItem;
            break;
        }
        nextRow++;
    }

    if (!nextItem) return;

    int nextRealIndex = nextItem->data(Qt::UserRole).toInt();
    if (nextRealIndex < 0 || nextRealIndex >= m_rules.size()) return;

    m_currentIndex = -1;
    m_rules.swapItemsAt(realIndex, nextRealIndex);
    populateList();

    for (int r = 0; r < m_listWidget->count(); ++r) {
        QListWidgetItem* checkItem = m_listWidget->item(r);
        if (checkItem && (checkItem->flags() & Qt::ItemIsSelectable) && checkItem->data(Qt::UserRole).toInt() == nextRealIndex) {
            m_listWidget->setCurrentRow(r);
            break;
        }
    }
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
    QList<QPair<QString, QString>> items = {
        {"File Operations", "tb_file_ops"},
        {"View Options", "tb_view_ops"},
        {"Drives Toolbar", "tb_drives"},
        {"Operations Toolbar", "tb_center_ops"}
    };

    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_toolbars_v1").toString();
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject tbObj = arr[i].toObject();
            QString id = tbObj["id"].toString();
            QString name = tbObj["name"].toString();
            bool exists = false;
            for (const auto& pair : items) {
                if (pair.second == id) { exists = true; break; }
            }
            if (!exists) {
                items.append(QPair<QString, QString>(name, id));
            }
        }
    } else {
        items.append(QPair<QString, QString>("Custom Commands", "customToolBar"));
    }

    MultiSelectDialog dlg("Select Active Toolbars", "Select which toolbars should be visible under this profile:", items, m_selectedToolbars, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedToolbars = dlg.selectedIds();
        m_btnSelectToolbars->setText(m_selectedToolbars.isEmpty() ? "Select Toolbars..." : QString("%1 Selected").arg(m_selectedToolbars.size()));
    }
}

void FolderLayoutDialog::onSelectMenus() {
    QList<QPair<QString, QString>> items = {
        {"Main Menu", "Main Menu"}
    };

    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_menus_v2").toString();
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject mObj = arr[i].toObject();
            QString title = mObj["title"].toString();
            items.append(QPair<QString, QString>(title, title));
        }
    } else {
        items.append(QPair<QString, QString>("Custom commands", "Custom commands"));
    }

    MultiSelectDialog dlg("Select Active Menus", "Select which menus should be visible in the menu bar under this profile:", items, m_selectedMenus, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedMenus = dlg.selectedIds();
        m_btnSelectMenus->setText(m_selectedMenus.isEmpty() ? "Select Menus..." : QString("%1 Selected").arg(m_selectedMenus.size()));
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

void FolderLayoutDialog::onSelectBgImage() {
    QString path = QFileDialog::getOpenFileName(this, "Select Background Image", "", "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
    if (!path.isEmpty()) {
        m_selectedBgImage = path;
        m_lblBgImagePath->setText(QFileInfo(path).fileName());
        m_lblBgImagePath->setToolTip(path);
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
        else if (idx == 8) m_comboViewMode->setCurrentText("Movies Full Screen");
        else if (idx == 9) m_comboViewMode->setCurrentText("TV Shows Full Screen");
        else if (idx == 10) m_comboViewMode->setCurrentText("Music Full Screen");
        else if (idx == 11) m_comboViewMode->setCurrentText("Cover Flow Carousel");
    }

    // 2. Capture custom buttons filter list
    m_btnChooseButtons->setProperty("selectedButtons", mw->m_activeToolbarFilter);
    m_btnChooseButtons->setText(mw->m_activeToolbarFilter.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(mw->m_activeToolbarFilter.size()));

    // 3. Visibilities
    m_stateConsole->setChecked(mw->m_actToggleConsole && mw->m_actToggleConsole->isChecked());
    m_statePreview->setChecked(mw->m_actTogglePreview && mw->m_actTogglePreview->isChecked());
    m_stateFavorites->setChecked(mw->m_actToggleFavoritesSidebar && mw->m_actToggleFavoritesSidebar->isChecked());
    m_stateZen->setChecked(mw->m_zenMode);
    m_stateBuiltinPlayerDoubleclick->setChecked(mw->isBuiltinPlayerDoubleclickActive());

    QSettings settings("Amifiles", "Amifiles");
    m_stateFullScreenPlayer->setChecked(settings.value("preview/auto_fullscreen", true).toBool());

    if (mw->m_previewPanel) {
        m_stateVisualizer->setChecked(mw->m_previewPanel->isSpectrumVisualizerEnabled());
    }

    m_stateDualPane->setChecked(mw->m_actToggleDualPane && mw->m_actToggleDualPane->isChecked());
    m_stateHorizontalSplit->setChecked(mw->m_actToggleHorizontalSplit && mw->m_actToggleHorizontalSplit->isChecked());
    m_stateCasingOverlays->setChecked(mw->m_actToggleCasingOverlays && mw->m_actToggleCasingOverlays->isChecked());

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

    QString profileName = "All profiles";
    if (m_currentIndex >= 0 && m_currentIndex < m_rules.size()) {
        harvestCurrentProfile(m_currentIndex);
        profileName = QString("Profile '%1'").arg(m_rules[m_currentIndex].name);
    }

    QWidget* parentW = parentWidget();
    while (parentW && !parentW->inherits("MainWindow")) {
        parentW = parentW->parentWidget();
    }
    MainWindow* mw = qobject_cast<MainWindow*>(parentW);
    if (mw) {
        mw->setFolderRules(m_rules);
        mw->saveFolderRules();
        if (mw->activePanel()) {
            mw->applyFolderRules(mw->activePanel()->currentPath(), mw->activePanel());
        }
    }

    QMessageBox::information(this, "Profiles Saved", QString("Folder profiles successfully saved and applied!\nSaved/updated: %1").arg(profileName));
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
    ToolbarEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (mainWin) {
            mainWin->rebuildToolBars();
        }
    }
}
 
void FolderLayoutDialog::onEditMenusShortcut() {
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    CustomMenuEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (mainWin) {
            mainWin->rebuildCustomMenus();
        }
    }
}
 
void FolderLayoutDialog::onEditContextMenusShortcut() {
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    CustomMenuEditorDialog dlg("custom_context_menu_v4", this);
    if (dlg.exec() == QDialog::Accepted) {
        if (mainWin) {
            mainWin->rebuildCustomMenus();
        }
    }
}

void FolderLayoutDialog::onApplyToCurrentFolder() {
    if (m_currentIndex < 0 || m_currentIndex >= m_rules.size()) {
        QMessageBox::warning(this, "No Profile Selected", "Please select a profile on the left list first.");
        return;
    }

    // Harvest current active edits to the rule before making changes
    harvestCurrentProfile(m_currentIndex);

    QString profileName = m_rules[m_currentIndex].name;
    if (profileName.isEmpty()) return;

    MainWindow* mainWin = qobject_cast<MainWindow*>(parent());
    if (!mainWin || !mainWin->activePanel()) {
        QMessageBox::warning(this, "Error", "Cannot access MainWindow active panel.");
        return;
    }

    QString currentPath = mainWin->activePanel()->currentPath();
    if (currentPath.isEmpty()) {
        QMessageBox::warning(this, "Empty Folder", "Cannot apply to current folder: Active folder path is empty.");
        return;
    }

    // Check if a path-matching rule already exists for this path
    bool foundExisting = false;
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].ruleType == "Path" && QDir::cleanPath(m_rules[i].value) == QDir::cleanPath(currentPath)) {
            m_rules[i].linkedProfile = profileName;
            foundExisting = true;
            m_listWidget->setCurrentRow(i);
            break;
        }
    }

    if (!foundExisting) {
        FolderLayoutRule r;
        r.name = QString("%1 (%2)").arg(QFileInfo(currentPath).fileName()).arg(profileName);
        if (r.name.isEmpty()) {
            r.name = QString("Path (%1)").arg(profileName);
        }
        r.ruleType = "Path";
        r.value = currentPath;
        r.autoApply = true;
        r.linkedProfile = profileName;
        m_rules.append(r);
        
        populateList();
        m_listWidget->setCurrentRow(m_rules.size() - 1);
    }

    QMessageBox::information(this, "Profile Linked", 
        QString("Successfully linked folder '%1' to profile '%2'.\nPress OK to save changes.")
        .arg(QFileInfo(currentPath).fileName())
        .arg(profileName));
}

void FolderLayoutDialog::onListWidgetContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_listWidget->itemAt(pos);
    if (!item) return;

    int row = m_listWidget->row(item);
    if (row < 0 || row >= m_rules.size()) return;

    QString profileName = m_rules[row].name;

    QMenu menu(this);
    QSettings settings("Amifiles", "Amifiles");
    QStringList pinned = settings.value("dashboard/pinned_profiles").toStringList();
    bool isPinned = pinned.contains(profileName);

    QAction* actPin = menu.addAction(isPinned ? "📌 Unpin from Smart Home Dashboard" : "📌 Pin to Smart Home Dashboard");
    QAction* selected = menu.exec(m_listWidget->mapToGlobal(pos));

    if (selected == actPin) {
        if (isPinned) {
            pinned.removeAll(profileName);
        } else {
            pinned.append(profileName);
        }
        settings.setValue("dashboard/pinned_profiles", pinned);
        settings.sync();

        QWidget* parentW = parentWidget();
        while (parentW && !parentW->inherits("MainWindow")) {
            parentW = parentW->parentWidget();
        }
        MainWindow* mw = qobject_cast<MainWindow*>(parentW);
        if (mw) {
            mw->refreshAllDashboards();
        }

        QMessageBox::information(this, isPinned ? "Profile Unpinned" : "Profile Pinned",
            QString("Layout profile '%1' has been successfully %2 the Smart Home Dashboard.")
            .arg(profileName).arg(isPinned ? "removed from" : "pinned to"));
    }
}
