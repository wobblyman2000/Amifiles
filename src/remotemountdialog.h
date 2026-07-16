#ifndef REMOTEMOUNTDIALOG_H
#define REMOTEMOUNTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>

#include <QListWidget>

class RemoteMountDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteMountDialog(QWidget* parent = nullptr);
    ~RemoteMountDialog() override = default;

private slots:
    void onTypeChanged(int index);
    void onMount();
    void onCancel();
    void onSaveAddress();
    void onDeleteAddress();
    void onAddressSelected(int row);

private:
    void setupUI();
    void loadAddresses();

    QListWidget* m_listAddresses = nullptr;
    QPushButton* m_btnSaveAddress = nullptr;
    QPushButton* m_btnDeleteAddress = nullptr;

    QComboBox* m_comboType = nullptr;
    QLineEdit* m_editHost = nullptr;
    QSpinBox* m_spinPort = nullptr;
    QLineEdit* m_editUser = nullptr;
    QLineEdit* m_editPassword = nullptr;
    QLineEdit* m_editPath = nullptr;
    QLineEdit* m_editName = nullptr;

    QPushButton* m_btnMount = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // REMOTEMOUNTDIALOG_H
