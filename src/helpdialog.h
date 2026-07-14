#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#include <QDialog>
#include <QTextBrowser>
#include <QListWidget>

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget* parent = nullptr);
    ~HelpDialog() override = default;

private slots:
    void onSectionChanged(int index);

private:
    void setupUI();

    QListWidget* m_sidebar = nullptr;
    QTextBrowser* m_browser = nullptr;
};

#endif // HELPDIALOG_H
