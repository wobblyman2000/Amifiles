#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

class AdvancedNewFolderDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedNewFolderDialog(const QString& basePath, QWidget* parent = nullptr);
    ~AdvancedNewFolderDialog() override = default;

    QStringList folderNames() const;

private slots:
    void onTemplateChanged(int index);
    void onCreateClicked();

private:
    void setupUI();

    QString m_basePath;
    QPlainTextEdit* m_txtFolders = nullptr;
    QComboBox* m_comboTemplates = nullptr;
    QLabel* m_lblPathPreview = nullptr;
    QStringList m_foldersToCreate;
};
