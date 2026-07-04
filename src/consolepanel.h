#ifndef CONSOLEPANEL_H
#define CONSOLEPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>

class ConsolePanel : public QWidget {
    Q_OBJECT
public:
    explicit ConsolePanel(QWidget* parent = nullptr);
    ~ConsolePanel() override = default;

    void appendOutput(const QString& text);
    void appendError(const QString& text);
    void appendSystem(const QString& text);

public slots:
    void clearConsole();

private:
    QPlainTextEdit* m_textEdit = nullptr;
    QPushButton* m_btnClear = nullptr;
};

#endif // CONSOLEPANEL_H
