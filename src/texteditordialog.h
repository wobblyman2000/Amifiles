#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QPushButton>

class TextEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextEditorDialog(const QString& filePath, QWidget* parent = nullptr);
    ~TextEditorDialog() override = default;

private slots:
    void saveFile();

private:
    void loadFile();
    void setupUI();

    QString m_filePath;
    QPlainTextEdit* m_editor = nullptr;
    QPushButton* m_btnSave = nullptr;
    QPushButton* m_btnCancel = nullptr;
};
