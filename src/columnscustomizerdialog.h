#pragma once

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include "customfilesystemmodel.h"

class ColumnsCustomizerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColumnsCustomizerDialog(const QList<CustomColumn>& currentCols, QWidget* parent = nullptr);
    ~ColumnsCustomizerDialog() override = default;

    QList<CustomColumn> getSelectedColumns() const { return m_cols; }

private slots:
    void onTypeChanged(int index);
    void onAddColumn();
    void onRemoveColumn();
    void onMoveUp();
    void onMoveDown();
    void onResetDefaults();

private:
    void setupUI();
    void updateKeyField();
    void populateActiveList();

    QList<CustomColumn> m_cols;

    QListWidget* m_listActive = nullptr;
    QLineEdit* m_txtName = nullptr;
    QComboBox* m_comboType = nullptr;
    QComboBox* m_comboKeySelect = nullptr;
    QLineEdit* m_txtKeyInput = nullptr;

    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnUp = nullptr;
    QPushButton* m_btnDown = nullptr;
    QPushButton* m_btnReset = nullptr;
    QPushButton* m_btnSave = nullptr;
    QPushButton* m_btnCancel = nullptr;
};
