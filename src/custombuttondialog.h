#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include "iconpickerdialog.h"

class CustomButtonDialog : public QDialog {
public:
    CustomButtonDialog(const QString& name, const QString& script, const QString& iconPath, QWidget* parent = nullptr) 
        : QDialog(parent), m_iconPath(iconPath) {
        setWindowTitle("Custom Command Button Editor");
        resize(500, 500);

        QVBoxLayout* layout = new QVBoxLayout(this);

        layout->addWidget(new QLabel("Button Label:"));
        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setText(name);
        m_nameEdit->setPlaceholderText("Enter button text...");
        layout->addWidget(m_nameEdit);

        layout->addWidget(new QLabel("Select System Icon (Theme):"));
        m_comboSysIcon = new QComboBox(this);
        m_comboSysIcon->addItems({
            "(No System Icon)",
            "📁 Folder (folder)",
            "💽 Hard Drive (drive-harddisk)",
            "🖥️ Terminal (utilities-terminal)",
            "⚡ Run / Execute (system-run)",
            "🔄 Refresh / Sync (view-refresh)",
            "ℹ️ Info / Help (help-about)",
            "📄 Document (document-properties)",
            "📋 Copy (edit-copy)",
            "✂️ Cut (edit-cut)",
            "📥 Paste (edit-paste)",
            "🗑️ Delete (edit-delete)",
            "⚙️ Settings (preferences-system)",
            "🔍 Search (edit-find)",
            "➕ Add (list-add)"
        });

        const QStringList themeKeys = {
            "",
            "folder",
            "drive-harddisk",
            "utilities-terminal",
            "system-run",
            "view-refresh",
            "help-about",
            "document-properties",
            "edit-copy",
            "edit-cut",
            "edit-paste",
            "edit-delete",
            "preferences-system",
            "edit-find",
            "list-add"
        };

        if (m_iconPath.startsWith("theme:")) {
            QString currentThemeKey = m_iconPath.mid(6);
            int idx = themeKeys.indexOf(currentThemeKey);
            if (idx >= 0) {
                m_comboSysIcon->setCurrentIndex(idx);
            }
        }

        connect(m_comboSysIcon, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, themeKeys](int index) {
            if (index > 0) {
                m_iconPath = "theme:" + themeKeys[index];
                m_lblIconPath->setText("System Theme: " + themeKeys[index]);
            } else {
                if (m_iconPath.startsWith("theme:")) {
                    m_iconPath.clear();
                    m_lblIconPath->setText("(No Icon Selected)");
                }
            }
        });
        layout->addWidget(m_comboSysIcon);

        layout->addWidget(new QLabel("Or Browse for Custom Image/Theme Icon:"));
        QHBoxLayout* iconLayout = new QHBoxLayout();
        m_lblIconPath = new QLabel(m_iconPath.isEmpty() ? "(No Icon Selected)" : m_iconPath, this);
        m_lblIconPath->setStyleSheet("color: #a6adc8; font-size: 11px;");
        m_lblIconPath->setWordWrap(true);
        
        QPushButton* btnSelectIcon = new QPushButton("Browse...", this);
        btnSelectIcon->setMaximumWidth(100);
        connect(btnSelectIcon, &QPushButton::clicked, this, [this]() {
            IconPickerDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted) {
                QString path = dlg.selectedIconName();
                if (!path.isEmpty()) {
                    if (!path.contains("/") && !path.contains("\\")) {
                        m_iconPath = "theme:" + path;
                    } else {
                        m_iconPath = path;
                    }
                    m_lblIconPath->setText(m_iconPath);
                    m_comboSysIcon->setCurrentIndex(0);
                }
            }
        });

        QPushButton* btnClearIcon = new QPushButton("Clear", this);
        btnClearIcon->setMaximumWidth(80);
        connect(btnClearIcon, &QPushButton::clicked, this, [this]() {
            m_iconPath.clear();
            m_lblIconPath->setText("(No Icon Selected)");
            m_comboSysIcon->setCurrentIndex(0);
        });

        iconLayout->addWidget(m_lblIconPath, 1);
        iconLayout->addWidget(btnSelectIcon);
        iconLayout->addWidget(btnClearIcon);
        layout->addLayout(iconLayout);

        layout->addWidget(new QLabel("Shell Command / Script to Execute:"));
        m_scriptEdit = new QPlainTextEdit(this);
        m_scriptEdit->setPlainText(script);
        m_scriptEdit->setPlaceholderText("Enter command or script path...\nExample:\nbash /home/user/myscript.sh\nOr navigation: @internal:Go /path\nSupports: {dir} (active panel dir), {dest} (sibling panel dir)");
        layout->addWidget(m_scriptEdit);

        // Save & Cancel buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* btnSave = new QPushButton("Save", this);
        QPushButton* btnCancel = new QPushButton("Cancel", this);
        
        buttonLayout->addStretch();
        buttonLayout->addWidget(btnSave);
        buttonLayout->addWidget(btnCancel);
        layout->addLayout(buttonLayout);

        connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    QString buttonName() const { return m_nameEdit->text().trimmed(); }
    QString script() const { return m_scriptEdit->toPlainText().trimmed(); }
    QString iconPath() const { return m_iconPath; }

private:
    QLineEdit* m_nameEdit;
    QPlainTextEdit* m_scriptEdit;
    QComboBox* m_comboSysIcon;
    QLabel* m_lblIconPath;
    QString m_iconPath;
};
