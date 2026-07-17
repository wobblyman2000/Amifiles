#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTextBrowser>
#include <QPushButton>

class ActionHelperDialog : public QDialog {
    Q_OBJECT
public:
    explicit ActionHelperDialog(QWidget* parent = nullptr);
    ~ActionHelperDialog() override = default;

    QString selectedCommand() const;

private slots:
    void filterActions();
    void updateDetails();
    void onInsertClicked();

private:
    struct InternalAction {
        QString command;
        QString name;
        QString description;
    };

    void setupUI();
    void populateList();

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_listWidget = nullptr;
    QTextBrowser* m_descBrowser = nullptr;
    QPushButton* m_btnInsert = nullptr;
    QPushButton* m_btnCancel = nullptr;

    QList<InternalAction> m_actions;
};
