#ifndef BUTTONEDITOR_H
#define BUTTONEDITOR_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class ButtonEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ButtonEditorDialog(QWidget* parent = nullptr);
    ~ButtonEditorDialog() override = default;

    void setButtonData(const QString& name, const QString& command, const QString& iconPath);
    QString name() const;
    QString command() const;
    QString iconPath() const;

private slots:
    void onBrowseIcon();
    void onSave();

private:
    void setupUI();

    QLineEdit* m_txtName = nullptr;
    QLineEdit* m_txtCommand = nullptr;
    QLineEdit* m_txtIconPath = nullptr;
};

#endif // BUTTONEDITOR_H
