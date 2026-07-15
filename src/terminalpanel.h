#ifndef TERMINALPANEL_H
#define TERMINALPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QSocketNotifier>
#include <QTextCharFormat>

class TerminalEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit TerminalEdit(QWidget* parent = nullptr);
    void setMasterFd(int fd) { m_masterFd = fd; }

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    int m_masterFd = -1;
};

class TerminalPanel : public QWidget {
    Q_OBJECT
public:
    explicit TerminalPanel(QWidget* parent = nullptr);
    ~TerminalPanel() override;

    void startShell(const QString& workingDir);
    void syncDirectory(const QString& path);

private slots:
    void onReadyRead();

private:
    void parseAndWriteText(const QByteArray& data);

    int m_masterFd = -1;
    qint64 m_pid = -1;
    QSocketNotifier* m_notifier = nullptr;
    TerminalEdit* m_terminalEdit = nullptr;

    QTextCharFormat m_defaultFormat;
    QTextCharFormat m_currentFormat;
};

#endif // TERMINALPANEL_H
