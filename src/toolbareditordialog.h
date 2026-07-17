#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QStackedWidget>

class ToolbarEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ToolbarEditorDialog(QWidget* parent = nullptr);
    ~ToolbarEditorDialog() override = default;

    static QJsonArray getDefaultToolbarsJson();
    QJsonArray getToolbarStructure() const;

private slots:
    void onToolbarSelected();
    void onItemSelected();
    void onToolbarFieldChanged();
    void onItemFieldChanged();
    
    // Toolbar operations
    void onAddToolbar();
    void onDeleteToolbar();
    void onRenameToolbar();
    void onMoveToolbarUp();
    void onMoveToolbarDown();

    // Item operations
    void onAddItem();
    void onDeleteItem();
    void onMoveItemUp();
    void onMoveItemDown();
    
    // Buttons helper
    void onBrowseItemIcon();
    void onResetDefaults();
    void onSave();

private:
    void setupUI();
    void loadToolbarStructure();
    void populateToolbarsList(const QJsonArray& arr);
    void populateItemsList(QListWidgetItem* tbItem);
    QJsonArray getDialogJson() const;

    // Controls Left
    QListWidget* m_listToolbars = nullptr;
    
    // Controls Middle
    QListWidget* m_listItems = nullptr;
    QPushButton* m_btnAddItem = nullptr;
    QPushButton* m_btnDeleteItem = nullptr;
    QPushButton* m_btnMoveItemUp = nullptr;
    QPushButton* m_btnMoveItemDown = nullptr;

    // Stacked Right Pane Editor (mode switching between Toolbar properties vs Item properties)
    QStackedWidget* m_propertiesStack = nullptr;
    QWidget* m_pageToolbarProps = nullptr;
    QWidget* m_pageItemProps = nullptr;

    // Toolbar Editor widgets
    QLineEdit* m_txtToolbarName = nullptr;
    QCheckBox* m_chkToolbarVisible = nullptr;
    QComboBox* m_comboToolbarStyle = nullptr;

    // Item Editor widgets
    QComboBox* m_comboItemType = nullptr; // internal, custom, separator
    QComboBox* m_comboInternalAction = nullptr;
    QLineEdit* m_txtItemCommand = nullptr;
    QLineEdit* m_txtItemName = nullptr;
    QLineEdit* m_txtItemIcon = nullptr;
    QPushButton* m_btnBrowseIcon = nullptr;

    bool m_updatingFields = false;
};
