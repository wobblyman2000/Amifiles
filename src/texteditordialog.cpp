#include "texteditordialog.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileInfo>
#include <QShortcut>

TextEditorDialog::TextEditorDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent), m_filePath(filePath) {
    setWindowTitle(QString("Text Editor - %1").arg(QFileInfo(filePath).fileName()));
    resize(800, 600);
    setStyleSheet(Theme::Stylesheet);

    setupUI();
    loadFile();

    // Hotkey: Ctrl+S to save
    QShortcut* saveShortcut = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(saveShortcut, &QShortcut::activated, this, &TextEditorDialog::saveFile);
}

void TextEditorDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    m_editor = new QPlainTextEdit(this);
    // Custom monospace font & styling matching Theme
    m_editor->setStyleSheet(
        "QPlainTextEdit { "
        "  background-color: #11111b; "
        "  color: #cdd6f4; "
        "  border: 1px solid #313244; "
        "  border-radius: 6px; "
        "  font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "  font-size: 13px; "
        "  padding: 6px; "
        "}"
    );
    mainLayout->addWidget(m_editor, 1);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_btnSave = new QPushButton("Save (Ctrl+S)", this);
    m_btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnSave, &QPushButton::clicked, this, &TextEditorDialog::saveFile);

    m_btnCancel = new QPushButton("Close", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_btnSave);
    buttonLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(buttonLayout);
}

void TextEditorDialog::loadFile() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Could not open file for reading.");
        reject();
        return;
    }

    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    file.close();
}

void TextEditorDialog::saveFile() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Could not open file for writing.");
        return;
    }

    QTextStream out(&file);
    out << m_editor->toPlainText();
    file.close();

    // Show a quick status message in the dialog title
    setWindowTitle(QString("Text Editor - %1 [SAVED]").arg(QFileInfo(m_filePath).fileName()));
}
