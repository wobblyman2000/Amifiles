#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QStringList>

class FileTagsDialog : public QDialog {
    Q_OBJECT
public:
    FileTagsDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~FileTagsDialog() override = default;

private slots:
    void onSaveClicked();

private:
    void setupUI();

    QStringList m_filePaths;
    QLineEdit* m_tagEdit = nullptr;
    QComboBox* m_colorCombo = nullptr;
    class QCompleter* m_completer = nullptr;
};
