#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>

class CustomMenuEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomMenuEditorDialog(QWidget* parent = nullptr);
    ~CustomMenuEditorDialog() override = default;

    QJsonArray getMenuStructure() const;

private slots:
    void onSelectionChanged();
    void onFieldChanged();
    void onAddTopLevel();
    void onAddSubmenu();
    void onAddAction();
    void onAddSeparator();
    void onDeleteSelected();
    void onMoveUp();
    void onMoveDown();
    void onBrowseIcon();
    void onPickColor();
    void onSave();

private:
    void setupUI();
    void loadMenuStructure();
    void populateTree(QTreeWidgetItem* parentItem, const QJsonArray& arr);
    QJsonArray getTreeJson(QTreeWidgetItem* parentItem = nullptr) const;

    QTreeWidget* m_tree = nullptr;

    // Properties Editor Widgets
    QComboBox* m_comboType = nullptr;
    QLineEdit* m_txtTitle = nullptr;
    QLineEdit* m_txtCommand = nullptr;
    QLineEdit* m_txtIcon = nullptr;
    QLineEdit* m_txtColor = nullptr;
    QComboBox* m_comboMode = nullptr;

    // Controls
    QPushButton* m_btnBrowseIcon = nullptr;
    QPushButton* m_btnPickColor = nullptr;
    QPushButton* m_btnAddSubmenu = nullptr;
    QPushButton* m_btnAddAction = nullptr;
    QPushButton* m_btnAddSeparator = nullptr;

    bool m_updatingFields = false;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
