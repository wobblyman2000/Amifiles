#include "custommenueditordialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QSettings>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QHeaderView>

CustomMenuEditorDialog::CustomMenuEditorDialog(QWidget* parent)
    : CustomMenuEditorDialog("custom_menus_v2", parent) {}

CustomMenuEditorDialog::CustomMenuEditorDialog(const QString& settingsKey, QWidget* parent) 
    : QDialog(parent), m_settingsKey(settingsKey) {
    if (m_settingsKey == "custom_context_menu_v3") {
        setWindowTitle("Custom Context Menu Configuration Center");
    } else {
        setWindowTitle("Custom Menu Configuration Center");
    }
    resize(850, 550);
    setupUI();
    loadMenuStructure();
    onSelectionChanged();
}

void CustomMenuEditorDialog::setupUI() {
    // Apply styling
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QTreeWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QTreeWidget::item:hover { background-color: #313244; }"
        "QTreeWidget::item:selected { background-color: #45475a; color: #f5c2e7; }"
        "QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #f5c2e7; padding-top: 16px; }"
        "QLineEdit, QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnSave { background-color: #a6e3a1; color: #11111b; }"
        "QPushButton#btnSave:hover { background-color: #b4befe; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);

    // Left Panel: Tree & Tree Operations
    QVBoxLayout* leftLayout = new QVBoxLayout();
    
    QLabel* lblTree = new QLabel("Menu Structure Hierarchy:", this);
    lblTree->setStyleSheet("font-weight: bold; font-size: 11pt;");
    leftLayout->addWidget(lblTree);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->viewport()->installEventFilter(this);
    leftLayout->addWidget(m_tree);

    // Add buttons
    QHBoxLayout* addButtonsLayout = new QHBoxLayout();
    QPushButton* btnAddRoot = new QPushButton("Add Top Menu", this);
    m_btnAddSubmenu = new QPushButton("Add Submenu", this);
    m_btnAddAction = new QPushButton("Add Action", this);
    m_btnAddSeparator = new QPushButton("Add Separator", this);

    addButtonsLayout->addWidget(btnAddRoot);
    addButtonsLayout->addWidget(m_btnAddSubmenu);
    addButtonsLayout->addWidget(m_btnAddAction);
    addButtonsLayout->addWidget(m_btnAddSeparator);
    leftLayout->addLayout(addButtonsLayout);

    QHBoxLayout* orderButtonsLayout = new QHBoxLayout();
    QPushButton* btnMoveUp = new QPushButton("Move Up", this);
    QPushButton* btnMoveDown = new QPushButton("Move Down", this);
    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    btnDelete->setStyleSheet("color: #f38ba8;");

    orderButtonsLayout->addWidget(btnMoveUp);
    orderButtonsLayout->addWidget(btnMoveDown);
    orderButtonsLayout->addWidget(btnDelete);
    leftLayout->addLayout(orderButtonsLayout);

    mainLayout->addLayout(leftLayout, 3);

    // Right Panel: Item Properties Editor
    QGroupBox* grpEditor = new QGroupBox("Properties Editor", this);
    QFormLayout* form = new QFormLayout(grpEditor);

    m_comboType = new QComboBox(this);
    m_comboType->addItems({"menu", "action", "separator"});
    m_comboType->setEnabled(false); // Managed automatically by addition buttons
    form->addRow("Item Type:", m_comboType);

    m_txtTitle = new QLineEdit(this);
    form->addRow("Display Text:", m_txtTitle);

    m_txtCommand = new QLineEdit(this);
    m_txtCommand->setPlaceholderText("Folder path / executable / shell script");
    form->addRow("Target / Command:", m_txtCommand);

    QHBoxLayout* iconLayout = new QHBoxLayout();
    m_txtIcon = new QLineEdit(this);
    m_txtIcon->setPlaceholderText("Theme name or absolute icon path");
    m_btnBrowseIcon = new QPushButton("...", this);
    m_btnBrowseIcon->setMaximumWidth(40);
    iconLayout->addWidget(m_txtIcon);
    iconLayout->addWidget(m_btnBrowseIcon);
    form->addRow("Icon Path:", iconLayout);

    QHBoxLayout* colorLayout = new QHBoxLayout();
    m_txtColor = new QLineEdit(this);
    m_txtColor->setPlaceholderText("#hex or blank");
    m_btnPickColor = new QPushButton("Color Picker...", this);
    colorLayout->addWidget(m_txtColor);
    colorLayout->addWidget(m_btnPickColor);
    form->addRow("Text Color:", colorLayout);

    m_comboMode = new QComboBox(this);
    m_comboMode->addItems({"Normal", "IconOnly", "TextOnly"});
    form->addRow("Display Mode:", m_comboMode);

    // Save & Cancel Actions at bottom of Editor
    QHBoxLayout* dlgButtons = new QHBoxLayout();
    QPushButton* btnReset = new QPushButton("Reset to Defaults 🔄", this);
    btnReset->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 12px; } QPushButton:hover { background-color: #eba0ac; }");
    QPushButton* btnSave = new QPushButton("Apply & Save changes", this);
    btnSave->setObjectName("btnSave");
    QPushButton* btnCancel = new QPushButton("Cancel", this);
    dlgButtons->addWidget(btnReset);
    dlgButtons->addStretch();
    dlgButtons->addWidget(btnSave);
    dlgButtons->addWidget(btnCancel);

    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(grpEditor);
    rightLayout->addLayout(dlgButtons);
    mainLayout->addLayout(rightLayout, 2);

    // Wire slots
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &CustomMenuEditorDialog::onSelectionChanged);
    
    // Connect input edits to update tree item dynamically
    connect(m_txtTitle, &QLineEdit::textEdited, this, &CustomMenuEditorDialog::onFieldChanged);
    connect(m_txtCommand, &QLineEdit::textEdited, this, &CustomMenuEditorDialog::onFieldChanged);
    connect(m_txtIcon, &QLineEdit::textEdited, this, &CustomMenuEditorDialog::onFieldChanged);
    connect(m_txtColor, &QLineEdit::textEdited, this, &CustomMenuEditorDialog::onFieldChanged);
    connect(m_comboMode, &QComboBox::currentIndexChanged, this, &CustomMenuEditorDialog::onFieldChanged);

    connect(btnAddRoot, &QPushButton::clicked, this, &CustomMenuEditorDialog::onAddTopLevel);
    connect(m_btnAddSubmenu, &QPushButton::clicked, this, &CustomMenuEditorDialog::onAddSubmenu);
    connect(m_btnAddAction, &QPushButton::clicked, this, &CustomMenuEditorDialog::onAddAction);
    connect(m_btnAddSeparator, &QPushButton::clicked, this, &CustomMenuEditorDialog::onAddSeparator);
    connect(btnDelete, &QPushButton::clicked, this, &CustomMenuEditorDialog::onDeleteSelected);
    connect(btnMoveUp, &QPushButton::clicked, this, &CustomMenuEditorDialog::onMoveUp);
    connect(btnMoveDown, &QPushButton::clicked, this, &CustomMenuEditorDialog::onMoveDown);
    connect(m_btnBrowseIcon, &QPushButton::clicked, this, &CustomMenuEditorDialog::onBrowseIcon);
    connect(m_btnPickColor, &QPushButton::clicked, this, &CustomMenuEditorDialog::onPickColor);

    connect(btnReset, &QPushButton::clicked, this, &CustomMenuEditorDialog::onResetToDefaults);
    connect(btnSave, &QPushButton::clicked, this, &CustomMenuEditorDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void CustomMenuEditorDialog::loadMenuStructure() {
    m_tree->clear();
    
    QSettings settings("Amifiles", "Amifiles");
    QString jsonStr = settings.value(m_settingsKey).toString();
    QJsonArray arr;
    
    if (jsonStr.isEmpty()) {
        if (m_settingsKey == "custom_context_menu_v3") {
            arr = getDefaultContextMenuJson();
        } else {
            // Fallback default structure
            QJsonObject m1;
            m1["type"] = "menu";
        m1["title"] = "🚀 Quick Launch";
        
        QJsonArray c1;
        QJsonObject a1;
        a1["type"] = "action";
        a1["title"] = "Home Directory";
        a1["command"] = QDir::homePath();
        a1["icon"] = "user-home";
        a1["color"] = "#89b4fa";
        a1["mode"] = "Normal";
        c1.append(a1);
        
        QJsonObject s1;
        s1["type"] = "separator";
        c1.append(s1);
        
        QJsonObject a2;
        a2["type"] = "action";
        a2["title"] = "Launch Terminal";
        a2["command"] = "xterm";
        a2["icon"] = "utilities-terminal";
        a2["color"] = "#f9e2af";
        a2["mode"] = "Normal";
        c1.append(a2);

        QJsonObject subM;
        subM["type"] = "menu";
        subM["title"] = "Project Workspaces";
        QJsonArray subC;
        QJsonObject a3;
        a3["type"] = "action";
        a3["title"] = "Amifiles Workspace";
        a3["command"] = "/home/dave/cpp_projects/Amifiles";
        a3["icon"] = "folder-development";
        a3["color"] = "#a6e3a1";
        a3["mode"] = "Normal";
        subC.append(a3);
        subM["children"] = subC;
        c1.append(subM);
        
        m1["children"] = c1;
        arr.append(m1);
        }
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        arr = doc.array();
    }

    populateTree(nullptr, arr);
    m_tree->expandAll();
}

void CustomMenuEditorDialog::populateTree(QTreeWidgetItem* parentItem, const QJsonArray& arr) {
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QTreeWidgetItem* item = new QTreeWidgetItem();
        
        QString type = obj["type"].toString("action");
        QString title = obj["title"].toString();
        if (type == "separator") title = "------- Separator -------";
        
        item->setText(0, title);
        item->setData(0, Qt::UserRole + 1, type);
        item->setData(0, Qt::UserRole + 2, obj["command"].toString());
        item->setData(0, Qt::UserRole + 3, obj["icon"].toString());
        item->setData(0, Qt::UserRole + 4, obj["color"].toString());
        item->setData(0, Qt::UserRole + 5, obj["mode"].toString("Normal"));

        if (parentItem) {
            parentItem->addChild(item);
        } else {
            m_tree->addTopLevelItem(item);
        }

        if (type == "menu" && obj.contains("children")) {
            populateTree(item, obj["children"].toArray());
        }
    }
}

void CustomMenuEditorDialog::onSelectionChanged() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) {
        m_comboType->setCurrentIndex(-1);
        m_txtTitle->setEnabled(false);
        m_txtCommand->setEnabled(false);
        m_txtIcon->setEnabled(false);
        m_txtColor->setEnabled(false);
        m_comboMode->setEnabled(false);
        m_btnBrowseIcon->setEnabled(false);
        m_btnPickColor->setEnabled(false);
        m_btnAddSubmenu->setEnabled(false);
        m_btnAddAction->setEnabled(false);
        m_btnAddSeparator->setEnabled(false);
        
        m_txtTitle->clear();
        m_txtCommand->clear();
        m_txtIcon->clear();
        m_txtColor->clear();
        return;
    }

    m_updatingFields = true;

    QString type = item->data(0, Qt::UserRole + 1).toString();
    m_comboType->setCurrentText(type);

    m_txtTitle->setEnabled(type != "separator");
    m_txtCommand->setEnabled(type == "action");
    m_txtIcon->setEnabled(type != "separator");
    m_txtColor->setEnabled(type == "action");
    m_comboMode->setEnabled(type == "action");
    
    m_btnBrowseIcon->setEnabled(type != "separator");
    m_btnPickColor->setEnabled(type == "action");
    
    m_btnAddSubmenu->setEnabled(type == "menu");
    m_btnAddAction->setEnabled(type == "menu");
    m_btnAddSeparator->setEnabled(type == "menu");

    m_txtTitle->setText(item->text(0));
    m_txtCommand->setText(item->data(0, Qt::UserRole + 2).toString());
    m_txtIcon->setText(item->data(0, Qt::UserRole + 3).toString());
    m_txtColor->setText(item->data(0, Qt::UserRole + 4).toString());
    m_comboMode->setCurrentText(item->data(0, Qt::UserRole + 5).toString());

    m_updatingFields = false;
}

void CustomMenuEditorDialog::onFieldChanged() {
    if (m_updatingFields) return;
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;

    QString type = item->data(0, Qt::UserRole + 1).toString();
    if (type != "separator") {
        item->setText(0, m_txtTitle->text());
    }
    item->setData(0, Qt::UserRole + 2, m_txtCommand->text());
    item->setData(0, Qt::UserRole + 3, m_txtIcon->text());
    item->setData(0, Qt::UserRole + 4, m_txtColor->text());
    item->setData(0, Qt::UserRole + 5, m_comboMode->currentText());
}

void CustomMenuEditorDialog::onAddTopLevel() {
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, "New Custom Menu");
    item->setData(0, Qt::UserRole + 1, "menu");
    item->setData(0, Qt::UserRole + 5, "Normal");
    
    m_tree->addTopLevelItem(item);
    m_tree->setCurrentItem(item);
}

void CustomMenuEditorDialog::onAddSubmenu() {
    QTreeWidgetItem* parent = m_tree->currentItem();
    if (!parent || parent->data(0, Qt::UserRole + 1).toString() != "menu") return;

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, "New Submenu");
    item->setData(0, Qt::UserRole + 1, "menu");
    item->setData(0, Qt::UserRole + 5, "Normal");
    
    parent->addChild(item);
    parent->setExpanded(true);
    m_tree->setCurrentItem(item);
}

void CustomMenuEditorDialog::onAddAction() {
    QTreeWidgetItem* parent = m_tree->currentItem();
    if (!parent || parent->data(0, Qt::UserRole + 1).toString() != "menu") return;

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, "New Action Shortcuts");
    item->setData(0, Qt::UserRole + 1, "action");
    item->setData(0, Qt::UserRole + 5, "Normal");
    
    parent->addChild(item);
    parent->setExpanded(true);
    m_tree->setCurrentItem(item);
}

void CustomMenuEditorDialog::onAddSeparator() {
    QTreeWidgetItem* parent = m_tree->currentItem();
    if (!parent || parent->data(0, Qt::UserRole + 1).toString() != "menu") return;

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, "------- Separator -------");
    item->setData(0, Qt::UserRole + 1, "separator");
    item->setData(0, Qt::UserRole + 5, "Normal");
    
    parent->addChild(item);
    parent->setExpanded(true);
    m_tree->setCurrentItem(item);
}

void CustomMenuEditorDialog::onDeleteSelected() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    delete item;
    onSelectionChanged();
}

void CustomMenuEditorDialog::onMoveUp() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;

    QTreeWidgetItem* parent = item->parent();
    int idx = parent ? parent->indexOfChild(item) : m_tree->indexOfTopLevelItem(item);
    if (idx <= 0) return;

    if (parent) {
        parent->takeChild(idx);
        parent->insertChild(idx - 1, item);
    } else {
        m_tree->takeTopLevelItem(idx);
        m_tree->insertTopLevelItem(idx - 1, item);
    }
    m_tree->setCurrentItem(item);
}

void CustomMenuEditorDialog::onMoveDown() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;

    QTreeWidgetItem* parent = item->parent();
    int idx = parent ? parent->indexOfChild(item) : m_tree->indexOfTopLevelItem(item);
    int count = parent ? parent->childCount() : m_tree->topLevelItemCount();
    if (idx < 0 || idx >= count - 1) return;

    if (parent) {
        parent->takeChild(idx);
        parent->insertChild(idx + 1, item);
    } else {
        m_tree->takeTopLevelItem(idx);
        m_tree->insertTopLevelItem(idx + 1, item);
    }
    m_tree->setCurrentItem(item);
}

#include "iconpickerdialog.h"

void CustomMenuEditorDialog::onBrowseIcon() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;

    IconPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString icon = dlg.selectedIconName();
        if (!icon.isEmpty()) {
            m_txtIcon->setText(icon);
            onFieldChanged();
        }
    }
}

void CustomMenuEditorDialog::onPickColor() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;

    QColor orig = QColor(m_txtColor->text());
    if (!orig.isValid()) orig = QColor("#cdd6f4");

    QColor color = QColorDialog::getColor(orig, this, "Pick Custom Item Font color");
    if (color.isValid()) {
        m_txtColor->setText(color.name());
        onFieldChanged();
    }
}

QJsonArray CustomMenuEditorDialog::getTreeJson(QTreeWidgetItem* parentItem) const {
    QJsonArray arr;
    int childCount = parentItem ? parentItem->childCount() : m_tree->topLevelItemCount();
    for (int i = 0; i < childCount; ++i) {
        QTreeWidgetItem* child = parentItem ? parentItem->child(i) : m_tree->topLevelItem(i);
        QJsonObject obj;
        obj["type"] = child->data(0, Qt::UserRole + 1).toString();
        obj["title"] = child->text(0);
        obj["command"] = child->data(0, Qt::UserRole + 2).toString();
        obj["icon"] = child->data(0, Qt::UserRole + 3).toString();
        obj["color"] = child->data(0, Qt::UserRole + 4).toString();
        QString mode = child->data(0, Qt::UserRole + 5).toString();
        if (mode.isEmpty()) mode = "Normal";
        obj["mode"] = mode;
        
        if (obj["type"].toString() == "menu") {
            obj["children"] = getTreeJson(child);
        }
        arr.append(obj);
    }
    return arr;
}

QJsonArray CustomMenuEditorDialog::getMenuStructure() const {
    return getTreeJson(nullptr);
}

void CustomMenuEditorDialog::onSave() {
    QJsonArray treeData = getMenuStructure();
    QJsonDocument doc(treeData);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    QSettings settings("Amifiles", "Amifiles");
    settings.setValue(m_settingsKey, jsonStr);
    accept();
}

QJsonArray CustomMenuEditorDialog::getDefaultContextMenuJson() const {
    QJsonArray arr;
    
    auto makeAction = [](const QString& title, const QString& command, const QString& icon = "") {
        QJsonObject obj;
        obj["type"] = "action";
        obj["title"] = title;
        obj["command"] = command;
        obj["icon"] = icon;
        obj["color"] = "";
        obj["mode"] = "Normal";
        return obj;
    };
    
    auto makeSeparator = []() {
        QJsonObject obj;
        obj["type"] = "separator";
        return obj;
    };

    // 1. Top-level essential commands
    arr.append(makeAction("Open", "app.open", "document-open"));
    arr.append(makeSeparator());
    arr.append(makeAction("Cut", "app.cut", "edit-cut"));
    arr.append(makeAction("Copy", "app.copy", "edit-copy"));
    arr.append(makeAction("Paste", "app.paste", "edit-paste"));
    arr.append(makeAction("Delete", "app.delete", "user-trash"));
    arr.append(makeAction("Rename", "app.rename", "edit-rename"));
    arr.append(makeAction("Bulk Rename...", "app.bulk_rename", ""));
    arr.append(makeSeparator());

    // 2. New Folder / Files Submenu
    {
        QJsonObject newMenu;
        newMenu["type"] = "menu";
        newMenu["title"] = "New";
        newMenu["icon"] = "folder-new";
        
        QJsonArray newKids;
        newKids.append(makeAction("New Folder", "app.new_folder", "folder-new"));
        newKids.append(makeAction("Advanced New Folder...", "app.advanced_new_folder", ""));
        newKids.append(makeSeparator());
        
        QJsonObject newFileMenu;
        newFileMenu["type"] = "menu";
        newFileMenu["title"] = "New File";
        newFileMenu["icon"] = "document-new";
        
        QJsonArray fileKids;
        fileKids.append(makeAction("Text Document (.txt)", "app.new_file_txt", ""));
        fileKids.append(makeAction("Markdown Document (.md)", "app.new_file_md", ""));
        fileKids.append(makeAction("HTML Document (.html)", "app.new_file_html", ""));
        fileKids.append(makeAction("Python Script (.py)", "app.new_file_py", ""));
        fileKids.append(makeAction("Blank PNG Image (.png)", "app.new_file_png", ""));
        
        newFileMenu["children"] = fileKids;
        newKids.append(newFileMenu);
        
        newMenu["children"] = newKids;
        arr.append(newMenu);
    }
    arr.append(makeSeparator());

    // 3. Clipboard Actions Submenu
    {
        QJsonObject clipMenu;
        clipMenu["type"] = "menu";
        clipMenu["title"] = "Clipboard Actions";
        clipMenu["icon"] = "edit-copy";
        
        QJsonArray clipKids;
        clipKids.append(makeAction("Copy File Name(s)", "app.copy_filename", ""));
        clipKids.append(makeAction("Copy Full Path(s)", "app.copy_path", ""));
        clipKids.append(makeAction("Copy Folder Contents (Paths List)", "app.copy_folder_contents", ""));
        clipKids.append(makeSeparator());
        clipKids.append(makeAction("Copy to Sibling Panel", "app.copy_sibling", "go-next"));
        clipKids.append(makeAction("Move to Sibling Panel", "app.move_sibling", "go-jump"));
        
        clipMenu["children"] = clipKids;
        arr.append(clipMenu);
    }

    // 4. Color Label & Overlay Submenu
    {
        QJsonObject colMenu;
        colMenu["type"] = "menu";
        colMenu["title"] = "Color Label & Overlay";
        colMenu["icon"] = "preferences-desktop-color";
        
        QJsonArray colKids;
        colKids.append(makeAction("None", "app.color_none", ""));
        colKids.append(makeAction("Red", "app.color_red", ""));
        colKids.append(makeAction("Orange", "app.color_orange", ""));
        colKids.append(makeAction("Yellow", "app.color_yellow", ""));
        colKids.append(makeAction("Green", "app.color_green", ""));
        colKids.append(makeAction("Blue", "app.color_blue", ""));
        colKids.append(makeAction("Purple", "app.color_purple", ""));
        colKids.append(makeSeparator());
        colKids.append(makeAction("Custom Icon Overlay...", "app.color_custom_overlay", ""));
        colKids.append(makeAction("Clear Icon Overlay", "app.color_clear_overlay", ""));
        
        colMenu["children"] = colKids;
        arr.append(colMenu);
    }
    arr.append(makeSeparator());

    // 5. Media & Tagging Tools Submenu
    {
        QJsonObject mediaMenu;
        mediaMenu["type"] = "menu";
        mediaMenu["title"] = "Media & Tagging Tools";
        mediaMenu["icon"] = "applications-multimedia";
        
        QJsonArray mediaKids;
        mediaKids.append(makeAction("Edit Audio Tags...", "app.edit_tags", ""));
        mediaKids.append(makeAction("Advanced Tag Editor...", "app.advanced_tag_editor", ""));
        mediaKids.append(makeAction("Fetch MusicBrainz Album Info...", "app.fetch_musicbrainz", ""));
        mediaKids.append(makeAction("Scrape Video Metadata...", "app.scrape_video", ""));
        mediaKids.append(makeAction("Fetch Cover Art & Wallpaper...", "app.fetch_folder_art", ""));
        mediaKids.append(makeSeparator());
        mediaKids.append(makeAction("File Tags...", "app.file_tags", ""));
        mediaKids.append(makeAction("🎬 Media Info Sheet... (ℹ)", "app.media_info_sheet", "dialog-information"));
        mediaKids.append(makeAction("✔ Toggle Watch Status (Watched/Unwatched)", "app.toggle_watch", ""));
        mediaKids.append(makeSeparator());
        mediaKids.append(makeAction("Play Folder/Album in Preview", "app.play_preview", "media-playback-start"));
        mediaKids.append(makeAction("Play Folder/Album in Fullscreen", "app.play_fullscreen_playlist", "media-playback-start"));
        
        mediaMenu["children"] = mediaKids;
        arr.append(mediaMenu);
    }

    // 6. System & Advanced Tools Submenu
    {
        QJsonObject sysMenu;
        sysMenu["type"] = "menu";
        sysMenu["title"] = "System & Advanced Tools";
        sysMenu["icon"] = "applications-system";
        
        QJsonArray sysKids;
        sysKids.append(makeAction("Toggle Executable Status", "app.toggle_executable", ""));
        sysKids.append(makeAction("Change File Permissions (chmod)...", "app.change_permissions", ""));
        sysKids.append(makeAction("Remove Green Screen 🟢", "app.remove_green_screen", ""));
        sysKids.append(makeSeparator());
        sysKids.append(makeAction("Compare Selected Files", "app.compare_selected", ""));
        sysKids.append(makeAction("Compare with Sibling Pane File", "app.compare_sibling", ""));
        sysKids.append(makeSeparator());
        sysKids.append(makeAction("Calculate Checksum Hash...", "app.calculate_checksum", ""));
        sysKids.append(makeAction("Secure Shred (Delete Permanently)...", "app.secure_shred", "user-trash"));
        sysKids.append(makeAction("Batch Convert/Resize Images...", "app.image_convert", ""));
        
        sysMenu["children"] = sysKids;
        arr.append(sysMenu);
    }

    // 7. Virtual Drives & Archives Submenu
    {
        QJsonObject archiveMenu;
        archiveMenu["type"] = "menu";
        archiveMenu["title"] = "Virtual Drives & Archives";
        archiveMenu["icon"] = "package-x-generic";
        
        QJsonArray archiveKids;
        archiveKids.append(makeAction("Vault Encryption/Decryption", "app.vault_toggle", ""));
        archiveKids.append(makeAction("ISO Virtual Drive", "app.iso_toggle", ""));
        archiveKids.append(makeAction("VHD Virtual Drive", "app.vhd_toggle", ""));
        archiveKids.append(makeSeparator());
        archiveKids.append(makeAction("Create Archive...", "app.create_archive", ""));
        archiveKids.append(makeAction("Create Secure Archive (AES-256)...", "app.create_secure_archive", ""));
        archiveKids.append(makeAction("Extract Archive...", "app.extract_archive", ""));
        
        archiveMenu["children"] = archiveKids;
        arr.append(archiveMenu);
    }

    // 8. Favorites & Pins Submenu
    {
        QJsonObject favMenu;
        favMenu["type"] = "menu";
        favMenu["title"] = "Favorites & Pins";
        favMenu["icon"] = "emblem-favorite";
        
        QJsonArray favKids;
        favKids.append(makeAction("Add/Remove Favorites", "app.favorites", ""));
        favKids.append(makeAction("Pin/Unpin Home Screen", "app.pin_home", ""));
        
        favMenu["children"] = favKids;
        arr.append(favMenu);
    }
    arr.append(makeSeparator());

    // 9. Folder Layout Profiles Submenu
    {
        QJsonObject profSub;
        profSub["type"] = "menu";
        profSub["title"] = "Folder Layout Profiles";
        profSub["icon"] = "preferences-other";
        
        QJsonArray profKids;
        
        QJsonObject applySub;
        applySub["type"] = "menu";
        applySub["title"] = "Apply Profile Layout to Current Folder";
        applySub["command"] = "app.apply_profile_submenu";
        profKids.append(applySub);
        
        profKids.append(makeSeparator());
        profKids.append(makeAction("Save Current Layout as Folder Profile...", "app.save_folder_profile", ""));
        profKids.append(makeAction("Save Current Layout as Default Profile", "app.save_default_profile", ""));
        profKids.append(makeAction("Load Default Profile", "app.load_default_profile", ""));
        
        profSub["children"] = profKids;
        arr.append(profSub);
    }

    return arr;
}

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

bool CustomMenuEditorDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_tree->viewport()) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent* dragMoveEvent = static_cast<QDragMoveEvent*>(event);
            if (dragMoveEvent->mimeData()->hasUrls()) {
                dragMoveEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            if (dropEvent->mimeData()->hasUrls()) {
                QList<QUrl> urls = dropEvent->mimeData()->urls();
                
                QPoint pos = dropEvent->position().toPoint();
                QTreeWidgetItem* parentItem = m_tree->itemAt(pos);
                
                QTreeWidgetItem* lastAddedItem = nullptr;
                for (const QUrl& url : urls) {
                    QString localPath = url.toLocalFile();
                    if (!localPath.isEmpty()) {
                        QString name = QFileInfo(localPath).fileName();
                        if (name.isEmpty()) name = localPath;

                        QTreeWidgetItem* item = new QTreeWidgetItem();
                        item->setText(0, name);
                        item->setData(0, Qt::UserRole + 1, "action");
                        item->setData(0, Qt::UserRole + 2, localPath);

                        QString themeName = "folder";
                        if (QFileInfo(localPath).isDir()) {
                            QString folderLower = name.toLower();
                            if (folderLower.contains("game")) themeName = "applications-games";
                            else if (folderLower.contains("code") || folderLower.contains("prog") || folderLower.contains("dev") || folderLower.contains("src")) themeName = "applications-development";
                            else if (folderLower.contains("music") || folderLower.contains("song") || folderLower.contains("audio")) themeName = "folder-music";
                            else if (folderLower.contains("video") || folderLower.contains("movie") || folderLower.contains("film")) themeName = "folder-videos";
                            else if (folderLower.contains("picture") || folderLower.contains("photo") || folderLower.contains("image")) themeName = "folder-pictures";
                            else if (folderLower.contains("doc") || folderLower.contains("paper") || folderLower.contains("text")) themeName = "folder-documents";
                            else if (folderLower.contains("download")) themeName = "folder-download";
                        } else {
                            themeName = "system-run";
                        }
                        
                        item->setData(0, Qt::UserRole + 3, themeName);
                        item->setData(0, Qt::UserRole + 4, "");
                        item->setData(0, Qt::UserRole + 5, "Normal");

                        if (parentItem && parentItem->data(0, Qt::UserRole + 1).toString() == "menu") {
                            parentItem->addChild(item);
                            parentItem->setExpanded(true);
                        } else {
                            m_tree->addTopLevelItem(item);
                        }
                        lastAddedItem = item;
                    }
                }
                
                if (lastAddedItem) {
                    m_tree->setCurrentItem(lastAddedItem);
                }
                
                dropEvent->acceptProposedAction();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CustomMenuEditorDialog::onResetToDefaults() {
    if (QMessageBox::question(this, "Reset to Defaults", 
                              "Are you sure you want to discard all customizations and reset this menu to its default layout?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        QSettings settings("Amifiles", "Amifiles");
        settings.remove(m_settingsKey);
        loadMenuStructure();
        QMessageBox::information(this, "Reset Complete", "Menu configuration successfully reset to default layout.");
    }
}
