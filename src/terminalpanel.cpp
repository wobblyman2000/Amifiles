#include "terminalpanel.h"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QDebug>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pty.h>

// ================= TerminalEdit =================

TerminalEdit::TerminalEdit(QWidget* parent) : QPlainTextEdit(parent) {
    document()->setMaximumBlockCount(2000); // Prevent infinite memory growth
}

void TerminalEdit::keyPressEvent(QKeyEvent* e) {
    if (m_masterFd == -1) {
        QPlainTextEdit::keyPressEvent(e);
        return;
    }

    QByteArray data;
    int key = e->key();
    Qt::KeyboardModifiers mods = e->modifiers();

    if (mods & Qt::ControlModifier) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            data.append(static_cast<char>(key - Qt::Key_A + 1));
        }
    } else {
        switch (key) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                data.append("\r");
                break;
            case Qt::Key_Backspace:
                data.append("\177"); // ASCII Delete
                break;
            case Qt::Key_Tab:
                data.append("\t");
                break;
            case Qt::Key_Escape:
                data.append("\033");
                break;
            case Qt::Key_Up:
                data.append("\033[A");
                break;
            case Qt::Key_Down:
                data.append("\033[B");
                break;
            case Qt::Key_Right:
                data.append("\033[C");
                break;
            case Qt::Key_Left:
                data.append("\033[D");
                break;
            default:
                data.append(e->text().toLocal8Bit());
                break;
        }
    }

    if (!data.isEmpty()) {
        ::write(m_masterFd, data.constData(), data.size());
    }
    e->accept(); // Consume key event locally, shell will echo it back
}

// ================= TerminalPanel =================

TerminalPanel::TerminalPanel(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_terminalEdit = new TerminalEdit(this);
    m_terminalEdit->setStyleSheet(
        "QPlainTextEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 11px; }"
    );

    // Style baseline format matching Catppuccin Mocha theme text colors
    m_defaultFormat.setForeground(QColor("#cdd6f4"));
    m_currentFormat = m_defaultFormat;

    layout->addWidget(m_terminalEdit);
}

TerminalPanel::~TerminalPanel() {
    if (m_masterFd != -1) {
        ::close(m_masterFd);
    }
    if (m_pid > 0) {
        ::kill(m_pid, SIGKILL);
        ::waitpid(m_pid, nullptr, WNOHANG);
    }
}

void TerminalPanel::startShell(const QString& workingDir) {
    struct winsize ws;
    ws.ws_row = 24;
    ws.ws_col = 80;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    int masterFd = -1;
    pid_t pid = forkpty(&masterFd, nullptr, nullptr, &ws);

    if (pid < 0) {
        m_terminalEdit->appendPlainText("=== Failed to fork pseudo-terminal ===");
        return;
    }

    if (pid == 0) {
        // Child shell process
        setenv("TERM", "xterm-color", 1);
        if (!workingDir.isEmpty()) {
            chdir(workingDir.toLocal8Bit().constData());
        }
        execl("/bin/bash", "bash", nullptr);
        _exit(1);
    }

    // Parent GUI process
    m_masterFd = masterFd;
    m_pid = pid;
    m_terminalEdit->setMasterFd(m_masterFd);

    // Make master file descriptor non-blocking
    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &TerminalPanel::onReadyRead);

    m_terminalEdit->appendPlainText("=== Interactive Bash Terminal Initialized ===");
}

void TerminalPanel::syncDirectory(const QString& path) {
    if (m_masterFd == -1) return;
    // Safely cd to directory using single-line bash command execution
    QString cmd = QString("cd \"%1\"\n").arg(path);
    ::write(m_masterFd, cmd.toLocal8Bit().constData(), cmd.length());
}

void TerminalPanel::onReadyRead() {
    char buf[1024];
    QByteArray incoming;

    while (true) {
        int bytesRead = ::read(m_masterFd, buf, sizeof(buf));
        if (bytesRead > 0) {
            incoming.append(buf, bytesRead);
        } else {
            break;
        }
    }

    if (!incoming.isEmpty()) {
        parseAndWriteText(incoming);
    }
}

void TerminalPanel::parseAndWriteText(const QByteArray& data) {
    QTextCursor cursor = m_terminalEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_terminalEdit->setTextCursor(cursor);

    int idx = 0;
    int len = data.size();
    while (idx < len) {
        char ch = data[idx];
        if (ch == '\033') {
            if (idx + 1 < len && data[idx + 1] == '[') {
                int seqStart = idx + 2;
                int seqEnd = seqStart;
                while (seqEnd < len && !isalpha(data[seqEnd])) {
                    seqEnd++;
                }
                if (seqEnd < len) {
                    char code = data[seqEnd];
                    QByteArray seq = data.mid(seqStart, seqEnd - seqStart);
                    idx = seqEnd; // Skip processed code

                    if (code == 'm') {
                        QStringList codes = QString::fromLocal8Bit(seq).split(';');
                        for (const QString& cStr : codes) {
                            int val = cStr.toInt();
                            if (val == 0) {
                                m_currentFormat = m_defaultFormat;
                            } else if (val == 1) {
                                m_currentFormat.setFontWeight(QFont::Bold);
                            } else if (val >= 30 && val <= 37) {
                                QStringList colors = { "#313244", "#f38ba8", "#a6e3a1", "#f9e2af", "#89b4fa", "#cba6f7", "#89dceb", "#cdd6f4" };
                                m_currentFormat.setForeground(QColor(colors[val - 30]));
                            } else if (val >= 90 && val <= 97) {
                                QStringList colors = { "#585b70", "#f38ba8", "#a6e3a1", "#f9e2af", "#89b4fa", "#cba6f7", "#89dceb", "#cdd6f4" };
                                m_currentFormat.setForeground(QColor(colors[val - 90]));
                            }
                        }
                    }
                } else {
                    idx = len;
                }
            }
        } else if (ch == '\b') {
            cursor.deletePreviousChar();
        } else if (ch == '\r') {
            // Wait for newline
        } else {
            QString str = QString(ch);
            cursor.insertText(str, m_currentFormat);
        }
        idx++;
    }

    m_terminalEdit->ensureCursorVisible();
}
