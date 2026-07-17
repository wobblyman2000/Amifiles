#include "terminalpanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QPushButton>
#include <QKeyEvent>
#include <QScrollBar>
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
    document()->setMaximumBlockCount(2000);
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
                data.append("\177");
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
    e->accept();
}

// ================= SingleTerminalWidget =================

SingleTerminalWidget::SingleTerminalWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_terminalEdit = new TerminalEdit(this);
    m_terminalEdit->setStyleSheet(
        "QPlainTextEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 11px; }"
    );

    m_defaultFormat.setForeground(QColor("#cdd6f4"));
    m_currentFormat = m_defaultFormat;

    layout->addWidget(m_terminalEdit);
}

SingleTerminalWidget::~SingleTerminalWidget() {
    if (m_notifier) {
        m_notifier->setEnabled(false);
    }
    if (m_masterFd != -1) {
        ::close(m_masterFd);
    }
    if (m_pid > 0) {
        ::kill(m_pid, SIGKILL);
        ::waitpid(m_pid, nullptr, WNOHANG);
    }
}

void SingleTerminalWidget::startShell(const QString& workingDir) {
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
        setenv("TERM", "xterm-color", 1);
        if (!workingDir.isEmpty()) {
            chdir(workingDir.toLocal8Bit().constData());
        }
        execl("/bin/bash", "bash", nullptr);
        _exit(1);
    }

    m_masterFd = masterFd;
    m_pid = pid;
    m_terminalEdit->setMasterFd(m_masterFd);

    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &SingleTerminalWidget::onReadyRead);

    m_terminalEdit->appendPlainText("=== Terminal Session Active ===");
}

void SingleTerminalWidget::syncDirectory(const QString& path) {
    if (m_masterFd == -1) return;
    QString cmd = QString("cd \"%1\"\n").arg(path);
    ::write(m_masterFd, cmd.toLocal8Bit().constData(), cmd.length());
}

void SingleTerminalWidget::onReadyRead() {
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

void SingleTerminalWidget::parseAndWriteText(const QByteArray& data) {
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
                    idx = seqEnd;

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
            // skip
        } else {
            cursor.insertText(QString(ch), m_currentFormat);
        }
        idx++;
    }

    m_terminalEdit->ensureCursorVisible();
}

// ================= TerminalPanel =================

TerminalPanel::TerminalPanel(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void TerminalPanel::startShell(const QString& workingDir) {
    addNewTab(workingDir);
}

void TerminalPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Mini Controls Row
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(4, 0, 4, 0);

    QPushButton* btnAdd = new QPushButton("+ New Tab", this);
    QPushButton* btnSplit = new QPushButton("🔲 Split Vertically", this);

    btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; border-radius: 4px; padding: 4px 10px; font-weight: bold; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnSplit->setStyleSheet("QPushButton { background-color: #313244; color: #89b4fa; border: 1px solid #45475a; border-radius: 4px; padding: 4px 10px; font-weight: bold; } QPushButton:hover { background-color: #89b4fa; color: #11111b; }");

    connect(btnAdd, &QPushButton::clicked, this, [this]() { addNewTab(); });
    connect(btnSplit, &QPushButton::clicked, this, &TerminalPanel::splitActiveTab);

    controlsLayout->addWidget(btnAdd);
    controlsLayout->addWidget(btnSplit);
    controlsLayout->addStretch(1);
    mainLayout->addLayout(controlsLayout);

    // Tab Widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background-color: #181825; }"
        "QTabBar::tab { background-color: #313244; color: #a6adc8; padding: 6px 12px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #181825; color: #89b4fa; font-weight: bold; }"
    );
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &TerminalPanel::onTabCloseRequested);

    mainLayout->addWidget(m_tabWidget, 1);
}

void TerminalPanel::addNewTab(const QString& workingDir) {
    SingleTerminalWidget* term = new SingleTerminalWidget(m_tabWidget);
    term->startShell(workingDir.isEmpty() ? QDir::homePath() : workingDir);

    int idx = m_tabWidget->addTab(term, QString("Shell %1").arg(m_tabWidget->count() + 1));
    m_tabWidget->setCurrentIndex(idx);
}

void TerminalPanel::splitActiveTab() {
    int currentIdx = m_tabWidget->currentIndex();
    if (currentIdx < 0) return;

    QWidget* currentTab = m_tabWidget->widget(currentIdx);
    
    // Check if it's already a splitter
    QSplitter* splitter = qobject_cast<QSplitter*>(currentTab);
    if (!splitter) {
        // Create new splitter, replace single widget in tab
        splitter = new QSplitter(Qt::Horizontal, m_tabWidget);
        splitter->setStyleSheet("QSplitter::handle { background-color: #313244; }");

        // Remove old widget but keep it to put in splitter
        m_tabWidget->removeTab(currentIdx);
        
        // Add original widget to splitter
        splitter->addWidget(currentTab);
        
        // Add new side-by-side terminal widget
        SingleTerminalWidget* newTerm = new SingleTerminalWidget(splitter);
        newTerm->startShell(QDir::homePath());
        splitter->addWidget(newTerm);

        m_tabWidget->insertTab(currentIdx, splitter, QString("Split Shell %1").arg(currentIdx + 1));
        m_tabWidget->setCurrentIndex(currentIdx);
    } else {
        // Add another terminal to existing splitter
        SingleTerminalWidget* newTerm = new SingleTerminalWidget(splitter);
        newTerm->startShell(QDir::homePath());
        splitter->addWidget(newTerm);
    }
}

void TerminalPanel::onTabCloseRequested(int index) {
    if (m_tabWidget->count() <= 1) return; // Prevent closing the last tab
    QWidget* w = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    w->deleteLater();
}

void TerminalPanel::syncDirectory(const QString& path) {
    int idx = m_tabWidget->currentIndex();
    if (idx < 0) return;

    QWidget* currentTab = m_tabWidget->widget(idx);
    QSplitter* splitter = qobject_cast<QSplitter*>(currentTab);
    if (splitter) {
        // Sync the active focused terminal inside splitter
        for (int i = 0; i < splitter->count(); ++i) {
            SingleTerminalWidget* term = qobject_cast<SingleTerminalWidget*>(splitter->widget(i));
            if (term && term->hasFocus()) {
                term->syncDirectory(path);
                return;
            }
        }
        // Fallback sync first child
        SingleTerminalWidget* term = qobject_cast<SingleTerminalWidget*>(splitter->widget(0));
        if (term) term->syncDirectory(path);
    } else {
        SingleTerminalWidget* term = qobject_cast<SingleTerminalWidget*>(currentTab);
        if (term) term->syncDirectory(path);
    }
}
