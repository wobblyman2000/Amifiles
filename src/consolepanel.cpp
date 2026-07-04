#include "consolepanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>

ConsolePanel::ConsolePanel(QWidget* parent) : QWidget(parent) {
    // Beautiful dark terminal styling
    setStyleSheet("QWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 4px 8px; border: 1px solid #45475a; }"
                  "QPushButton:hover { background-color: #45475a; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // Top header layout
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(6);

    QLabel* titleLabel = new QLabel("⌨️ Script Output Console", this);
    titleLabel->setStyleSheet("font-weight: bold; color: #f5c2e7; border: none;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);

    m_btnClear = new QPushButton("Clear Log", this);
    connect(m_btnClear, &QPushButton::clicked, this, &ConsolePanel::clearConsole);
    headerLayout->addWidget(m_btnClear);

    mainLayout->addLayout(headerLayout);

    // Terminal log text edit
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet("font-family: monospace; font-size: 12px; background-color: #11111b; border: 1px solid #313244; border-radius: 4px; padding: 4px;");
    mainLayout->addWidget(m_textEdit, 1);
}

void ConsolePanel::appendOutput(const QString& text) {
    if (text.isEmpty()) return;
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    // Preserve formatting and colors
    m_textEdit->appendHtml(QString("<span style='color: #a6adc8;'>[%1]</span> <span style='color: #cdd6f4;'>%2</span>")
                               .arg(timeStr)
                               .arg(text.toHtmlEscaped().replace("\n", "<br>")));
}

void ConsolePanel::appendError(const QString& text) {
    if (text.isEmpty()) return;
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_textEdit->appendHtml(QString("<span style='color: #a6adc8;'>[%1]</span> <span style='color: #f38ba8; font-weight: bold;'>%2</span>")
                               .arg(timeStr)
                               .arg(text.toHtmlEscaped().replace("\n", "<br>")));
}

void ConsolePanel::appendSystem(const QString& text) {
    if (text.isEmpty()) return;
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_textEdit->appendHtml(QString("<span style='color: #a6adc8;'>[%1]</span> <span style='color: #89b4fa; font-style: italic;'>%2</span>")
                               .arg(timeStr)
                               .arg(text.toHtmlEscaped().replace("\n", "<br>")));
}

void ConsolePanel::clearConsole() {
    if (m_textEdit) {
        m_textEdit->clear();
    }
}
