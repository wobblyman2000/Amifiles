#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>

class SmartCollectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SmartCollectionDialog(QWidget* parent = nullptr);
    ~SmartCollectionDialog() override = default;

private slots:
    void onAddDir();
    void onRemoveDir();
    void onSave();

private:
    void setupUI();

    QLineEdit* m_txtName = nullptr;
    QListWidget* m_listDirs = nullptr;
    QLineEdit* m_txtPattern = nullptr;
    QDoubleSpinBox* m_spinMinSize = nullptr;
    QDoubleSpinBox* m_spinMaxSize = nullptr;
    QSpinBox* m_spinAgeDays = nullptr;

    QPushButton* m_btnAddDir = nullptr;
    QPushButton* m_btnRemoveDir = nullptr;
    QPushButton* m_btnSave = nullptr;
    QPushButton* m_btnCancel = nullptr;
};
