#include "toolbareditordialog.h"
#include "iconpickerdialog.h"
#include "custombuttondialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QSettings>
#include <QJsonDocument>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

static QJsonObject createItemObj(const QString& type, const QString& id, const QString& name, const QString& icon, const QString& command = "") {
    QJsonObject obj;
    obj["type"] = type;
    obj["id"] = id;
    obj["name"] = name;
    obj["icon"] = icon;
    obj["command"] = command;
    return obj;
}

QJsonArray ToolbarEditorDialog::getDefaultToolbarsJson() {
    QJsonArray root;

    // 1. File Operations Toolbar
    QJsonObject tbFile;
    tbFile["name"] = "File Operations";
    tbFile["visible"] = true;
    tbFile["style"] = "TextBesideIcon";
    tbFile["id"] = "tb_file_ops";
    QJsonArray itemsFile;
    itemsFile.append(createItemObj("internal", "Copy", "Copy", "edit-copy"));
    itemsFile.append(createItemObj("internal", "Cut", "Cut", "edit-cut"));
    itemsFile.append(createItemObj("internal", "Paste", "Paste", "edit-paste"));
    itemsFile.append(createItemObj("separator", "", "", ""));
    itemsFile.append(createItemObj("internal", "Delete", "Delete", "edit-delete"));
    itemsFile.append(createItemObj("internal", "Rename", "Rename", "edit-rename"));
    itemsFile.append(createItemObj("internal", "NewFolder", "New Folder", "folder-new"));
    tbFile["items"] = itemsFile;
    root.append(tbFile);

    // 2. View Options Toolbar
    QJsonObject tbView;
    tbView["name"] = "View Options";
    tbView["visible"] = true;
    tbView["style"] = "TextBesideIcon";
    tbView["id"] = "tb_view_ops";
    QJsonArray itemsView;
    itemsView.append(createItemObj("internal", "ToggleDualPane", "Dual Pane", "view-split-left-right"));
    itemsView.append(createItemObj("internal", "TogglePreview", "Preview", "view-preview"));
    itemsView.append(createItemObj("internal", "ToggleFlatView", "Flat View", "view-list-tree"));
    itemsView.append(createItemObj("separator", "", "", ""));
    itemsView.append(createItemObj("internal", "Refresh", "Refresh", "view-refresh"));
    tbView["items"] = itemsView;
    root.append(tbView);

    // 3. Drives Toolbar
    QJsonObject tbDrives;
    tbDrives["name"] = "Drives";
    tbDrives["visible"] = true;
    tbDrives["style"] = "IconOnly";
    tbDrives["id"] = "tb_drives";
    QJsonArray itemsDrives;
    itemsDrives.append(createItemObj("custom", "", "Home", "user-home", "@internal:Go ~"));
    tbDrives["items"] = itemsDrives;
    root.append(tbDrives);

    // 4. Custom Commands Toolbar
    QJsonObject tbCustom;
    tbCustom["name"] = "Custom Commands";
    tbCustom["visible"] = true;
    tbCustom["style"] = "TextBesideIcon";
    tbCustom["id"] = "customToolBar";
    QJsonArray itemsCustom;
    tbCustom["items"] = itemsCustom;
    root.append(tbCustom);

    return root;
}

ToolbarEditorDialog::ToolbarEditorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Toolbar Configuration Studio");
    resize(950, 580);
    setupUI();
    loadToolbarStructure();
}

void ToolbarEditorDialog::setupUI() {
    // Style matching Catppuccin dark theme
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QListWidget::item:hover { background-color: #313244; }"
        "QListWidget::item:selected { background-color: #45475a; color: #f5c2e7; font-weight: bold; }"
        "QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #f5c2e7; padding-top: 16px; }"
        "QLineEdit, QComboBox, QCheckBox { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnSave { background-color: #a6e3a1; color: #11111b; }"
        "QPushButton#btnSave:hover { background-color: #b4befe; }"
        "QPushButton#btnReset { background-color: #f38ba8; color: #11111b; }"
        "QPushButton#btnReset:hover { background-color: #f5e0dc; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);

    // Left Column: Toolbars List & Operations
    QVBoxLayout* col1 = new QVBoxLayout();
    QLabel* lblTb = new QLabel("Defined Toolbars:", this);
    lblTb->setStyleSheet("font-weight: bold; font-size: 10pt; color: #f5c2e7;");
    col1->addWidget(lblTb);

    m_listToolbars = new QListWidget(this);
    col1->addWidget(m_listToolbars, 4);

    QHBoxLayout* btnTbRow1 = new QHBoxLayout();
    QPushButton* btnAddTb = new QPushButton("Add", this);
    QPushButton* btnDelTb = new QPushButton("Delete", this);
    QPushButton* btnRenameTb = new QPushButton("Rename", this);
    btnTbRow1->addWidget(btnAddTb);
    btnTbRow1->addWidget(btnDelTb);
    btnTbRow1->addWidget(btnRenameTb);
    col1->addLayout(btnTbRow1);

    QHBoxLayout* btnTbRow2 = new QHBoxLayout();
    QPushButton* btnTbUp = new QPushButton("Move Up", this);
    QPushButton* btnTbDown = new QPushButton("Move Down", this);
    btnTbRow2->addWidget(btnTbUp);
    btnTbRow2->addWidget(btnTbDown);
    col1->addLayout(btnTbRow2);

    mainLayout->addLayout(col1, 2);

    // Middle Column: Selected Toolbar items
    QVBoxLayout* col2 = new QVBoxLayout();
    QLabel* lblItems = new QLabel("Toolbar Buttons:", this);
    lblItems->setStyleSheet("font-weight: bold; font-size: 10pt; color: #f5c2e7;");
    col2->addWidget(lblItems);

    m_listItems = new QListWidget(this);
    col2->addWidget(m_listItems, 4);

    QHBoxLayout* btnItemRow1 = new QHBoxLayout();
    m_btnAddItem = new QPushButton("Add Action...", this);
    m_btnAddCustom = new QPushButton("Add Custom Script...", this);
    m_btnDeleteItem = new QPushButton("Remove", this);
    btnItemRow1->addWidget(m_btnAddItem);
    btnItemRow1->addWidget(m_btnAddCustom);
    btnItemRow1->addWidget(m_btnDeleteItem);
    col2->addLayout(btnItemRow1);

    QHBoxLayout* btnItemRow2 = new QHBoxLayout();
    m_btnMoveItemUp = new QPushButton("Move Up", this);
    m_btnMoveItemDown = new QPushButton("Move Down", this);
    btnItemRow2->addWidget(m_btnMoveItemUp);
    btnItemRow2->addWidget(m_btnMoveItemDown);
    col2->addLayout(btnItemRow2);

    mainLayout->addLayout(col2, 2);

    // Right Column: Properties stacked panel
    m_propertiesStack = new QStackedWidget(this);
    
    // Page 0: Toolbar properties page
    m_pageToolbarProps = new QWidget(this);
    QVBoxLayout* layTbPage = new QVBoxLayout(m_pageToolbarProps);
    QGroupBox* grpTb = new QGroupBox("Toolbar Settings", this);
    QFormLayout* formTb = new QFormLayout(grpTb);
    
    m_txtToolbarName = new QLineEdit(this);
    formTb->addRow("Toolbar Title:", m_txtToolbarName);
    
    m_chkToolbarVisible = new QCheckBox("Display Toolbar", this);
    formTb->addRow("Visibility:", m_chkToolbarVisible);

    m_comboToolbarStyle = new QComboBox(this);
    m_comboToolbarStyle->addItems({"IconOnly", "TextOnly", "TextBesideIcon", "TextUnderIcon"});
    formTb->addRow("Render Style:", m_comboToolbarStyle);

    layTbPage->addWidget(grpTb);
    layTbPage->addStretch();
    m_propertiesStack->addWidget(m_pageToolbarProps);

    // Page 1: Item properties page
    m_pageItemProps = new QWidget(this);
    QVBoxLayout* layItemPage = new QVBoxLayout(m_pageItemProps);
    QGroupBox* grpItem = new QGroupBox("Button Settings", this);
    QFormLayout* formItem = new QFormLayout(grpItem);

    m_comboItemType = new QComboBox(this);
    m_comboItemType->addItems({"internal", "custom", "separator"});
    formItem->addRow("Action Type:", m_comboItemType);

    m_comboInternalAction = new QComboBox(this);
    m_comboInternalAction->addItems({
        "Copy", "Cut", "Paste", "Delete", "Rename", "NewFolder", "Refresh", 
        "ToggleDualPane", "TogglePreview", "ToggleFlatView", "CompareSync", 
        "DuplicateFinder", "SpaceAnalyzer"
    });
    formItem->addRow("Internal Action:", m_comboInternalAction);

    m_txtItemName = new QLineEdit(this);
    formItem->addRow("Button Label:", m_txtItemName);

    m_txtItemCommand = new QLineEdit(this);
    m_txtItemCommand->setPlaceholderText("Command script or @internal:Go path");
    formItem->addRow("Target Target:", m_txtItemCommand);

    QHBoxLayout* iconRow = new QHBoxLayout();
    m_txtItemIcon = new QLineEdit(this);
    m_btnBrowseIcon = new QPushButton("...", this);
    m_btnBrowseIcon->setMaximumWidth(40);
    iconRow->addWidget(m_txtItemIcon);
    iconRow->addWidget(m_btnBrowseIcon);
    formItem->addRow("Icon Asset:", iconRow);

    layItemPage->addWidget(grpItem);
    layItemPage->addStretch();
    m_propertiesStack->addWidget(m_pageItemProps);

    // Add Stack to main right
    QVBoxLayout* col3 = new QVBoxLayout();
    col3->addWidget(m_propertiesStack, 4);

    // Bottom Action Buttons
    QHBoxLayout* rowBottom = new QHBoxLayout();
    QPushButton* btnSave = new QPushButton("Save Config", this);
    btnSave->setObjectName("btnSave");
    QPushButton* btnReset = new QPushButton("Reset Defaults", this);
    btnReset->setObjectName("btnReset");
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    rowBottom->addWidget(btnReset);
    rowBottom->addStretch();
    rowBottom->addWidget(btnSave);
    rowBottom->addWidget(btnCancel);
    col3->addLayout(rowBottom);

    mainLayout->addLayout(col3, 3);

    // Wire selection triggers
    connect(m_listToolbars, &QListWidget::itemSelectionChanged, this, &ToolbarEditorDialog::onToolbarSelected);
    connect(m_listItems, &QListWidget::itemSelectionChanged, this, &ToolbarEditorDialog::onItemSelected);

    // Wire live field changes
    connect(m_txtToolbarName, &QLineEdit::textEdited, this, &ToolbarEditorDialog::onToolbarFieldChanged);
    connect(m_chkToolbarVisible, &QCheckBox::clicked, this, &ToolbarEditorDialog::onToolbarFieldChanged);
    connect(m_comboToolbarStyle, &QComboBox::currentIndexChanged, this, &ToolbarEditorDialog::onToolbarFieldChanged);

    connect(m_comboItemType, &QComboBox::currentIndexChanged, this, &ToolbarEditorDialog::onItemFieldChanged);
    connect(m_comboInternalAction, &QComboBox::currentIndexChanged, this, &ToolbarEditorDialog::onItemFieldChanged);
    connect(m_txtItemName, &QLineEdit::textEdited, this, &ToolbarEditorDialog::onItemFieldChanged);
    connect(m_txtItemCommand, &QLineEdit::textEdited, this, &ToolbarEditorDialog::onItemFieldChanged);
    connect(m_txtItemIcon, &QLineEdit::textEdited, this, &ToolbarEditorDialog::onItemFieldChanged);
    connect(m_btnBrowseIcon, &QPushButton::clicked, this, &ToolbarEditorDialog::onBrowseItemIcon);

    // Wire Operation triggers
    connect(btnAddTb, &QPushButton::clicked, this, &ToolbarEditorDialog::onAddToolbar);
    connect(btnDelTb, &QPushButton::clicked, this, &ToolbarEditorDialog::onDeleteToolbar);
    connect(btnRenameTb, &QPushButton::clicked, this, &ToolbarEditorDialog::onRenameToolbar);
    connect(btnTbUp, &QPushButton::clicked, this, &ToolbarEditorDialog::onMoveToolbarUp);
    connect(btnTbDown, &QPushButton::clicked, this, &ToolbarEditorDialog::onMoveToolbarDown);

    connect(m_btnAddItem, &QPushButton::clicked, this, &ToolbarEditorDialog::onAddItem);
    connect(m_btnAddCustom, &QPushButton::clicked, this, &ToolbarEditorDialog::onAddCustomScript);
    connect(m_btnDeleteItem, &QPushButton::clicked, this, &ToolbarEditorDialog::onDeleteItem);
    connect(m_btnMoveItemUp, &QPushButton::clicked, this, &ToolbarEditorDialog::onMoveItemUp);
    connect(m_btnMoveItemDown, &QPushButton::clicked, this, &ToolbarEditorDialog::onMoveItemDown);

    connect(btnReset, &QPushButton::clicked, this, &ToolbarEditorDialog::onResetDefaults);
    connect(btnSave, &QPushButton::clicked, this, &ToolbarEditorDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void ToolbarEditorDialog::loadToolbarStructure() {
    m_listToolbars->clear();
    m_listItems->clear();

    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value("custom_toolbars_v1").toString();
    QJsonArray arr;

    if (jsonStr.isEmpty()) {
        arr = getDefaultToolbarsJson();
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        arr = doc.array();
    }

    populateToolbarsList(arr);
}

void ToolbarEditorDialog::populateToolbarsList(const QJsonArray& arr) {
    m_updatingFields = true;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QString name = obj["name"].toString();
        
        QListWidgetItem* item = new QListWidgetItem(QIcon::fromTheme("document-properties"), name, m_listToolbars);
        item->setData(Qt::UserRole, obj);
    }
    m_updatingFields = false;
    
    if (m_listToolbars->count() > 0) {
        m_listToolbars->setCurrentRow(0);
    }
}

// Save active items in middle list back into currently selected toolbar's Json data
void ToolbarEditorDialog::onToolbarSelected() {
    if (m_updatingFields) return;

    m_listItems->clear();
    QListWidgetItem* item = m_listToolbars->currentItem();
    if (!item) {
        m_propertiesStack->setCurrentWidget(m_pageToolbarProps);
        m_txtToolbarName->setEnabled(false);
        m_chkToolbarVisible->setEnabled(false);
        m_comboToolbarStyle->setEnabled(false);
        
        m_btnAddItem->setEnabled(false);
        m_btnAddCustom->setEnabled(false);
        m_btnDeleteItem->setEnabled(false);
        m_btnMoveItemUp->setEnabled(false);
        m_btnMoveItemDown->setEnabled(false);
        return;
    }

    m_updatingFields = true;

    QJsonObject obj = item->data(Qt::UserRole).toJsonObject();
    m_txtToolbarName->setEnabled(true);
    m_chkToolbarVisible->setEnabled(true);
    m_comboToolbarStyle->setEnabled(true);

    m_btnAddItem->setEnabled(true);
    m_btnAddCustom->setEnabled(true);
    m_btnDeleteItem->setEnabled(true);
    m_btnMoveItemUp->setEnabled(true);
    m_btnMoveItemDown->setEnabled(true);

    m_txtToolbarName->setText(obj["name"].toString());
    m_chkToolbarVisible->setChecked(obj["visible"].toBool(true));
    m_comboToolbarStyle->setCurrentText(obj["style"].toString("TextBesideIcon"));

    // Populate items list
    QJsonArray itemsArr = obj["items"].toArray();
    for (int i = 0; i < itemsArr.size(); ++i) {
        QJsonObject itemObj = itemsArr[i].toObject();
        QString type = itemObj["type"].toString();
        QString name = itemObj["name"].toString();
        
        QIcon icon;
        if (type == "separator") {
            name = "------- Separator -------";
            icon = QIcon::fromTheme("edit-clear-all");
        } else {
            icon = QIcon::fromTheme(itemObj["icon"].toString());
        }

        QListWidgetItem* childItem = new QListWidgetItem(icon, name, m_listItems);
        childItem->setData(Qt::UserRole, itemObj);
    }

    m_updatingFields = false;
    
    // Default show toolbar properties
    m_propertiesStack->setCurrentWidget(m_pageToolbarProps);
}

void ToolbarEditorDialog::onItemSelected() {
    if (m_updatingFields) return;

    QListWidgetItem* item = m_listItems->currentItem();
    if (!item) {
        m_propertiesStack->setCurrentWidget(m_pageToolbarProps);
        return;
    }

    m_updatingFields = true;

    QJsonObject obj = item->data(Qt::UserRole).toJsonObject();
    QString type = obj["type"].toString();

    m_comboItemType->setCurrentText(type);
    
    m_comboInternalAction->setEnabled(type == "internal");
    m_txtItemCommand->setEnabled(type == "custom");
    m_txtItemName->setEnabled(type != "separator");
    m_txtItemIcon->setEnabled(type != "separator");
    m_btnBrowseIcon->setEnabled(type != "separator");

    m_comboInternalAction->setCurrentText(obj["id"].toString());
    m_txtItemCommand->setText(obj["command"].toString());
    m_txtItemName->setText(obj["name"].toString());
    m_txtItemIcon->setText(obj["icon"].toString());

    m_propertiesStack->setCurrentWidget(m_pageItemProps);
    m_updatingFields = false;
}

void ToolbarEditorDialog::onToolbarFieldChanged() {
    if (m_updatingFields) return;
    QListWidgetItem* item = m_listToolbars->currentItem();
    if (!item) return;

    m_updatingFields = true;
    QJsonObject obj = item->data(Qt::UserRole).toJsonObject();
    obj["name"] = m_txtToolbarName->text();
    obj["visible"] = m_chkToolbarVisible->isChecked();
    obj["style"] = m_comboToolbarStyle->currentText();
    
    item->setText(m_txtToolbarName->text());
    item->setData(Qt::UserRole, obj);
    m_updatingFields = false;
}

void ToolbarEditorDialog::onItemFieldChanged() {
    if (m_updatingFields) return;
    QListWidgetItem* childItem = m_listItems->currentItem();
    if (!childItem) return;

    m_updatingFields = true;
    QJsonObject obj = childItem->data(Qt::UserRole).toJsonObject();

    QString type = m_comboItemType->currentText();
    obj["type"] = type;

    m_comboInternalAction->setEnabled(type == "internal");
    m_txtItemCommand->setEnabled(type == "custom");
    m_txtItemName->setEnabled(type != "separator");
    m_txtItemIcon->setEnabled(type != "separator");
    m_btnBrowseIcon->setEnabled(type != "separator");

    if (type == "separator") {
        obj["name"] = "";
        obj["id"] = "";
        obj["command"] = "";
        obj["icon"] = "";
        childItem->setText("------- Separator -------");
        childItem->setIcon(QIcon::fromTheme("edit-clear-all"));
    } else {
        obj["name"] = m_txtItemName->text();
        obj["id"] = m_comboInternalAction->currentText();
        obj["command"] = m_txtItemCommand->text();
        obj["icon"] = m_txtItemIcon->text();
        
        childItem->setText(m_txtItemName->text());
        childItem->setIcon(QIcon::fromTheme(m_txtItemIcon->text()));
    }

    childItem->setData(Qt::UserRole, obj);
    m_updatingFields = false;

    // Update parent list metadata JSON copy
    QListWidgetItem* tbItem = m_listToolbars->currentItem();
    if (tbItem) {
        QJsonObject tbObj = tbItem->data(Qt::UserRole).toJsonObject();
        QJsonArray itemsArr;
        for (int i = 0; i < m_listItems->count(); ++i) {
            itemsArr.append(m_listItems->item(i)->data(Qt::UserRole).toJsonObject());
        }
        tbObj["items"] = itemsArr;
        tbItem->setData(Qt::UserRole, tbObj);
    }
}

void ToolbarEditorDialog::onAddToolbar() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Toolbar", "Enter Toolbar Title:", QLineEdit::Normal, "My Toolbar", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_updatingFields = true;
    QJsonObject obj;
    obj["name"] = name;
    obj["visible"] = true;
    obj["style"] = "TextBesideIcon";
    obj["id"] = QString("tb_custom_%1").arg(QDateTime::currentMSecsSinceEpoch());
    obj["items"] = QJsonArray();

    QListWidgetItem* item = new QListWidgetItem(QIcon::fromTheme("document-properties"), name, m_listToolbars);
    item->setData(Qt::UserRole, obj);
    
    m_updatingFields = false;
    m_listToolbars->setCurrentItem(item);
}

void ToolbarEditorDialog::onDeleteToolbar() {
    QListWidgetItem* item = m_listToolbars->currentItem();
    if (!item) return;
    delete item;
    onToolbarSelected();
}

void ToolbarEditorDialog::onRenameToolbar() {
    QListWidgetItem* item = m_listToolbars->currentItem();
    if (!item) return;

    bool ok;
    QString name = QInputDialog::getText(this, "Rename Toolbar", "Enter New Title:", QLineEdit::Normal, item->text(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_updatingFields = true;
    QJsonObject obj = item->data(Qt::UserRole).toJsonObject();
    obj["name"] = name;
    
    item->setText(name);
    item->setData(Qt::UserRole, obj);
    m_updatingFields = false;

    onToolbarSelected();
}

void ToolbarEditorDialog::onMoveToolbarUp() {
    int row = m_listToolbars->currentRow();
    if (row <= 0) return;

    m_updatingFields = true;
    QListWidgetItem* item = m_listToolbars->takeItem(row);
    m_listToolbars->insertItem(row - 1, item);
    m_updatingFields = false;
    m_listToolbars->setCurrentRow(row - 1);
}

void ToolbarEditorDialog::onMoveToolbarDown() {
    int row = m_listToolbars->currentRow();
    if (row < 0 || row >= m_listToolbars->count() - 1) return;

    m_updatingFields = true;
    QListWidgetItem* item = m_listToolbars->takeItem(row);
    m_listToolbars->insertItem(row + 1, item);
    m_updatingFields = false;
    m_listToolbars->setCurrentRow(row + 1);
}

void ToolbarEditorDialog::onAddItem() {
    QListWidgetItem* tbItem = m_listToolbars->currentItem();
    if (!tbItem) return;

    m_updatingFields = true;

    QJsonObject obj = createItemObj("internal", "Copy", "Copy File", "edit-copy");
    QListWidgetItem* item = new QListWidgetItem(QIcon::fromTheme("edit-copy"), "Copy File", m_listItems);
    item->setData(Qt::UserRole, obj);

    m_updatingFields = false;
    m_listItems->setCurrentItem(item);

    // Force synchronize parent Json items array
    onItemFieldChanged();
}

void ToolbarEditorDialog::onAddCustomScript() {
    QListWidgetItem* tbItem = m_listToolbars->currentItem();
    if (!tbItem) return;

    CustomButtonDialog dlg("", "", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString name = dlg.buttonName();
        QString script = dlg.script();
        QString icon = dlg.iconPath();

        if (!name.isEmpty()) {
            m_updatingFields = true;

            QJsonObject obj = createItemObj("custom", "", name, icon, script);
            
            QIcon qicon;
            if (!icon.isEmpty()) {
                if (icon.startsWith("theme:")) qicon = QIcon::fromTheme(icon.mid(6));
                else if (QFileInfo(icon).exists()) qicon = QIcon(icon);
                else qicon = QIcon::fromTheme(icon);
            } else {
                qicon = QIcon::fromTheme("system-run");
            }

            QListWidgetItem* item = new QListWidgetItem(qicon, name, m_listItems);
            item->setData(Qt::UserRole, obj);

            m_updatingFields = false;
            m_listItems->setCurrentItem(item);

            // Force synchronize parent Json items array
            onItemFieldChanged();
        }
    }
}


void ToolbarEditorDialog::onDeleteItem() {
    QListWidgetItem* item = m_listItems->currentItem();
    if (!item) return;
    delete item;
    
    // Synchronize parent Json items array
    m_updatingFields = true;
    QListWidgetItem* tbItem = m_listToolbars->currentItem();
    if (tbItem) {
        QJsonObject tbObj = tbItem->data(Qt::UserRole).toJsonObject();
        QJsonArray itemsArr;
        for (int i = 0; i < m_listItems->count(); ++i) {
            itemsArr.append(m_listItems->item(i)->data(Qt::UserRole).toJsonObject());
        }
        tbObj["items"] = itemsArr;
        tbItem->setData(Qt::UserRole, tbObj);
    }
    m_updatingFields = false;

    onItemSelected();
}

void ToolbarEditorDialog::onMoveItemUp() {
    int row = m_listItems->currentRow();
    if (row <= 0) return;

    m_updatingFields = true;
    QListWidgetItem* item = m_listItems->takeItem(row);
    m_listItems->insertItem(row - 1, item);
    m_updatingFields = false;
    m_listItems->setCurrentRow(row - 1);

    // Resync parent items order
    onItemFieldChanged();
}

void ToolbarEditorDialog::onMoveItemDown() {
    int row = m_listItems->currentRow();
    if (row < 0 || row >= m_listItems->count() - 1) return;

    m_updatingFields = true;
    QListWidgetItem* item = m_listItems->takeItem(row);
    m_listItems->insertItem(row + 1, item);
    m_updatingFields = false;
    m_listItems->setCurrentRow(row + 1);

    // Resync parent items order
    onItemFieldChanged();
}

void ToolbarEditorDialog::onBrowseItemIcon() {
    QListWidgetItem* item = m_listItems->currentItem();
    if (!item) return;

    IconPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString iconName = dlg.selectedIconName();
        if (!iconName.isEmpty()) {
            m_txtItemIcon->setText(iconName);
            onItemFieldChanged();
        }
    }
}

void ToolbarEditorDialog::onResetDefaults() {
    if (QMessageBox::question(this, "Reset Layout", "Are you sure you want to discard your custom toolbars and reset to the factory layout?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_listToolbars->clear();
        m_listItems->clear();
        populateToolbarsList(getDefaultToolbarsJson());
    }
}

QJsonArray ToolbarEditorDialog::getDialogJson() const {
    QJsonArray arr;
    for (int i = 0; i < m_listToolbars->count(); ++i) {
        arr.append(m_listToolbars->item(i)->data(Qt::UserRole).toJsonObject());
    }
    return arr;
}

QJsonArray ToolbarEditorDialog::getToolbarStructure() const {
    return getDialogJson();
}

void ToolbarEditorDialog::onSave() {
    QJsonArray treeData = getToolbarStructure();
    QJsonDocument doc(treeData);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("custom_toolbars_v1", jsonStr);
    accept();
}
