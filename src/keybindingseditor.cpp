#include "keybindingseditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QMessageBox>

class ShortcutCaptureDialog : public QDialog {
public:
    ShortcutCaptureDialog(const QString& actionName, const QKeySequence& current, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Record Shortcut");
        setMinimumWidth(320);
        setStyleSheet("background-color: #1e1e2e; color: #cdd6f4;");

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        QLabel* titleLabel = new QLabel(QString("Assign shortcut for: <b>%1</b>").arg(actionName), this);
        titleLabel->setStyleSheet("font-size: 13px;");
        layout->addWidget(titleLabel);

        m_keyEdit = new QKeySequenceEdit(current, this);
        m_keyEdit->setStyleSheet(
            "QKeySequenceEdit { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; font-size: 14px; }"
        );
        layout->addWidget(m_keyEdit);

        QLabel* helpLabel = new QLabel("Press your key combination. Click 'Assign' when done.", this);
        helpLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
        layout->addWidget(helpLabel);

        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(8);
        QPushButton* btnOk = new QPushButton("Assign", this);
        btnOk->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background-color: #b4befe; }");
        
        QPushButton* btnCancel = new QPushButton("Cancel", this);
        btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background-color: #45475a; }");
        
        btnLayout->addStretch();
        btnLayout->addWidget(btnOk);
        btnLayout->addWidget(btnCancel);
        layout->addLayout(btnLayout);

        connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

        m_keyEdit->setFocus();
    }

    QKeySequence keySequence() const {
        return m_keyEdit->keySequence();
    }
private:
    QKeySequenceEdit* m_keyEdit;
};

KeybindingsEditorDialog::KeybindingsEditorDialog(const QMap<QString, QKeySequence>& currentBindings, QWidget* parent)
    : QDialog(parent), m_bindings(currentBindings) {
    
    // Set default actions labels and mappings
    m_actionLabels["copy"] = "Copy";
    m_actionLabels["cut"] = "Cut";
    m_actionLabels["paste"] = "Paste";
    m_actionLabels["delete"] = "Delete";
    m_actionLabels["rename"] = "Rename";
    m_actionLabels["new_folder"] = "New Folder";
    m_actionLabels["properties"] = "Properties";
    m_actionLabels["refresh"] = "Refresh";
    m_actionLabels["toggle_dual_pane"] = "Toggle Dual Pane";
    m_actionLabels["toggle_preview"] = "Toggle Preview Panel";
    m_actionLabels["mute_preview"] = "Mute Preview";
    m_actionLabels["toggle_flat_view"] = "Toggle Flat View";
    m_actionLabels["compare_sync"] = "Compare Sync";
    m_actionLabels["space_analyzer"] = "Visual Space Analyzer";
    m_actionLabels["dup_finder"] = "Duplicate File Finder";
    m_actionLabels["help"] = "Help Manual";

    // Set defaults values
    m_defaults["copy"] = QKeySequence::Copy;
    m_defaults["cut"] = QKeySequence::Cut;
    m_defaults["paste"] = QKeySequence::Paste;
    m_defaults["delete"] = QKeySequence::Delete;
    m_defaults["rename"] = QKeySequence(Qt::Key_F2);
    m_defaults["new_folder"] = QKeySequence("Ctrl+N");
    m_defaults["properties"] = QKeySequence("Alt+Return");
    m_defaults["refresh"] = QKeySequence(Qt::Key_F5);
    m_defaults["toggle_dual_pane"] = QKeySequence("Ctrl+D");
    m_defaults["toggle_preview"] = QKeySequence("Ctrl+P");
    m_defaults["mute_preview"] = QKeySequence("Ctrl+M");
    m_defaults["toggle_flat_view"] = QKeySequence("Ctrl+F");
    m_defaults["compare_sync"] = QKeySequence("Ctrl+S");
    m_defaults["space_analyzer"] = QKeySequence("Ctrl+K");
    m_defaults["dup_finder"] = QKeySequence("Ctrl+J");
    m_defaults["help"] = QKeySequence(Qt::Key_F1);

    setupUI();
    populateTable();
}

void KeybindingsEditorDialog::setupUI() {
    setWindowTitle("Configure Keyboard Shortcuts");
    resize(500, 450);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel* title = new QLabel("Double-click any row to customize its keyboard shortcut:", this);
    title->setStyleSheet("font-size: 13px; font-weight: bold;");
    mainLayout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({"Action", "Shortcut"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #181825; color: #cdd6f4; gridline-color: #313244; border: 1px solid #313244; border-radius: 4px; }"
        "QHeaderView::section { background-color: #11111b; color: #a6adc8; border: none; padding: 6px; font-weight: bold; }"
        "QTableWidget::item:selected { background-color: #313244; color: #f5c2e7; }"
    );
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &KeybindingsEditorDialog::onCellDoubleClicked);
    mainLayout->addWidget(m_table);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);

    QPushButton* btnReset = new QPushButton("Reset All to Default", this);
    btnReset->setStyleSheet(
        "QPushButton { background-color: #e64553; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #ea76cb; }"
    );
    connect(btnReset, &QPushButton::clicked, this, &KeybindingsEditorDialog::onResetAll);

    QPushButton* btnSave = new QPushButton("Save Bindings", this);
    btnSave->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(btnSave, &QPushButton::clicked, this, &KeybindingsEditorDialog::onAccept);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    bottomLayout->addWidget(btnReset);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnSave);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);
}

void KeybindingsEditorDialog::populateTable() {
    m_table->setRowCount(0);
    int row = 0;
    for (auto it = m_actionLabels.begin(); it != m_actionLabels.end(); ++it) {
        m_table->insertRow(row);
        
        QTableWidgetItem* actionItem = new QTableWidgetItem(it.value());
        actionItem->setData(Qt::UserRole, it.key()); // Store key identifier
        
        QKeySequence currentSeq = m_bindings.value(it.key());
        QString shortcutStr = currentSeq.isEmpty() ? "None" : currentSeq.toString();
        QTableWidgetItem* shortcutItem = new QTableWidgetItem(shortcutStr);
        shortcutItem->setTextAlignment(Qt::AlignCenter);
        
        m_table->setItem(row, 0, actionItem);
        m_table->setItem(row, 1, shortcutItem);
        row++;
    }
}

void KeybindingsEditorDialog::onCellDoubleClicked(int row, int /*column*/) {
    QTableWidgetItem* actionItem = m_table->item(row, 0);
    if (!actionItem) return;

    QString actionId = actionItem->data(Qt::UserRole).toString();
    QString label = actionItem->text();
    QKeySequence current = m_bindings.value(actionId);

    ShortcutCaptureDialog captureDlg(label, current, this);
    if (captureDlg.exec() == QDialog::Accepted) {
        QKeySequence chosen = captureDlg.keySequence();

        // Check if there are any conflicting shortcuts (excluding the current action)
        for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
            if (it.key() != actionId && !it.value().isEmpty() && it.value() == chosen) {
                QMessageBox::warning(this, "Shortcut Conflict",
                    QString("This shortcut is already assigned to '%1'. Please choose a different shortcut.")
                    .arg(m_actionLabels.value(it.key())));
                return;
            }
        }

        m_bindings[actionId] = chosen;
        populateTable();
    }
}

void KeybindingsEditorDialog::onResetAll() {
    if (QMessageBox::question(this, "Reset Custom Keybindings", 
        "Are you sure you want to reset all keyboard shortcuts to their factory defaults?",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_bindings = m_defaults;
        populateTable();
    }
}

void KeybindingsEditorDialog::onAccept() {
    accept();
}
