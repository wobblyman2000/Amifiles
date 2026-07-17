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
    void onFindNext();
    void onFindPrev();
    void onSearchTextChanged(const QString& text);

private:
    void setupUI();

    QListWidget* m_sidebar = nullptr;
    QTextBrowser* m_browser = nullptr;
    class QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_btnFindNext = nullptr;
    QPushButton* m_btnFindPrev = nullptr;
};

#endif // HELPDIALOG_H
