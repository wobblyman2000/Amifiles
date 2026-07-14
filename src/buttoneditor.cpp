#include "buttoneditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

ButtonEditorDialog::ButtonEditorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Edit Toolbar Button");
    resize(500, 240);
    setupUI();
}

void ButtonEditorDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(8);

    grid->addWidget(new QLabel("Button Name:"), 0, 0);
    m_txtName = new QLineEdit(this);
    m_txtName->setPlaceholderText("e.g. Open Terminal");
    grid->addWidget(m_txtName, 0, 1, 1, 2);

    grid->addWidget(new QLabel("Shell Command:"), 1, 0);
    m_txtCommand = new QLineEdit(this);
    m_txtCommand->setPlaceholderText("e.g. kitty -e bash -c \"echo {filepath}; exec bash\"");
    grid->addWidget(m_txtCommand, 1, 1, 1, 2);

    grid->addWidget(new QLabel("Icon Path:"), 2, 0);
    m_txtIconPath = new QLineEdit(this);
    m_txtIconPath->setPlaceholderText("Optional path to .png or theme icon name");
    QPushButton* btnBrowse = new QPushButton("Browse...", this);
    connect(btnBrowse, &QPushButton::clicked, this, &ButtonEditorDialog::onBrowseIcon);
    grid->addWidget(m_txtIconPath, 2, 1);
    grid->addWidget(btnBrowse, 2, 2);

    mainLayout->addLayout(grid);

    QLabel* lblHelper = new QLabel(
        "Supported Macros:\n"
        "  {filepath} - Path of the selected file\n"
        "  {dir} - Path of the active panel folder\n"
        "  {dest} - Path of the opposite panel folder", this);
    lblHelper->setStyleSheet("color: #89b4fa; font-size: 11px; font-weight: bold;");
    mainLayout->addWidget(lblHelper);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    QPushButton* btnSave = new QPushButton("Save Button", this);
    btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnSave, &QPushButton::clicked, this, &ButtonEditorDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(btnLayout);
}

void ButtonEditorDialog::setButtonData(const QString& name, const QString& command, const QString& iconPath) {
    m_txtName->setText(name);
    m_txtCommand->setText(command);
    m_txtIconPath->setText(iconPath);
}

QString ButtonEditorDialog::name() const { return m_txtName->text().trimmed(); }
QString ButtonEditorDialog::command() const { return m_txtCommand->text().trimmed(); }
QString ButtonEditorDialog::iconPath() const { return m_txtIconPath->text().trimmed(); }

void ButtonEditorDialog::onBrowseIcon() {
    QString path = QFileDialog::getOpenFileName(this, "Select Icon File", QDir::homePath(), "Images (*.png *.jpg *.svg *.xpm)");
    if (!path.isEmpty()) {
        m_txtIconPath->setText(path);
    }
}

void ButtonEditorDialog::onSave() {
    if (name().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter a button name.");
        return;
    }
    if (command().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter a shell command.");
        return;
    }
    accept();
}
