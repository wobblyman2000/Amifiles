#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>

class WorkspaceProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit WorkspaceProfileDialog(QWidget* parent = nullptr);
    ~WorkspaceProfileDialog() override = default;

private slots:
    void onSaveClicked();
    void onLoadClicked();
    void onDeleteClicked();

private:
    void setupUI();
    void loadProfilesList();

    QLineEdit* m_profileNameEdit = nullptr;
    QComboBox* m_profilesCombo = nullptr;
};
