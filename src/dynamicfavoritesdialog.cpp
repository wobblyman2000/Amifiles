#include "dynamicfavoritesdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>

DynamicFavoritesDialog::DynamicFavoritesDialog(QWidget* parent) : QDialog(parent) {
    m_rules = FavoritesManager::instance().getDynamicRules();
    setupUI();
    populateTable();
}

void DynamicFavoritesDialog::setupUI() {
    setWindowTitle("Configure Dynamic Bookmark Rules");
    resize(650, 400);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel* title = new QLabel("Define rules to automatically bookmark matching directories:", this);
    title->setStyleSheet("font-weight: bold; font-size: 13px; color: #f5c2e7; margin-bottom: 6px;");
    mainLayout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Bookmark Label", "Filter Type", "Pattern / Value"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setStyleSheet("QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; gridline-color: #313244; }"
                           "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    mainLayout->addWidget(m_table);

    QHBoxLayout* controlLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("Add Rule", this);
    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    QPushButton* btnSave = new QPushButton("Save Rules", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnDelete->setStyleSheet("QPushButton { background-color: #313244; color: #f38ba8; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #f38ba8; color: #11111b; }");
    btnSave->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border: 1px solid #89b4fa; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #b4befe; }");
    btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #45475a; }");

    connect(btnAdd, &QPushButton::clicked, this, &DynamicFavoritesDialog::onAddRule);
    connect(btnDelete, &QPushButton::clicked, this, &DynamicFavoritesDialog::onDeleteRule);
    connect(btnSave, &QPushButton::clicked, this, &DynamicFavoritesDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    controlLayout->addWidget(btnAdd);
    controlLayout->addWidget(btnDelete);
    controlLayout->addStretch();
    controlLayout->addWidget(btnSave);
    controlLayout->addWidget(btnCancel);
    mainLayout->addLayout(controlLayout);
}

void DynamicFavoritesDialog::populateTable() {
    m_table->clearContents();
    m_table->setRowCount(m_rules.size());

    for (int i = 0; i < m_rules.size(); ++i) {
        const DynamicFavoriteRule& r = m_rules[i];

        // 1. Label/Name LineEdit
        QLineEdit* nameEdit = new QLineEdit(this);
        nameEdit->setText(r.name);
        nameEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 2px; }");
        m_table->setCellWidget(i, 0, nameEdit);

        // 2. Type Combo
        QComboBox* typeCombo = new QComboBox(this);
        typeCombo->addItem("Wildcard Path", "Wildcard");
        typeCombo->addItem("Folder Tag", "Tag");
        typeCombo->addItem("Recently Active (Hours)", "Recent");
        typeCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        
        int typeIdx = 0;
        if (r.ruleType == "Tag") typeIdx = 1;
        else if (r.ruleType == "Recent") typeIdx = 2;
        typeCombo->setCurrentIndex(typeIdx);
        m_table->setCellWidget(i, 1, typeCombo);

        // 3. Pattern / Value LineEdit
        QLineEdit* valEdit = new QLineEdit(this);
        valEdit->setText(r.value);
        valEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 2px; }");
        
        auto updatePlaceholder = [typeCombo, valEdit]() {
            QString type = typeCombo->currentData().toString();
            if (type == "Wildcard") valEdit->setPlaceholderText("e.g. /home/dave/projects/*");
            else if (type == "Tag") valEdit->setPlaceholderText("e.g. Work");
            else if (type == "Recent") valEdit->setPlaceholderText("e.g. 24 (for last 24h)");
        };
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updatePlaceholder);
        updatePlaceholder();

        m_table->setCellWidget(i, 2, valEdit);
    }
}

void DynamicFavoritesDialog::onAddRule() {
    DynamicFavoriteRule r;
    r.name = "New Rule";
    r.ruleType = "Wildcard";
    r.value = "";
    m_rules.append(r);
    populateTable();
}

void DynamicFavoritesDialog::onDeleteRule() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_rules.size()) {
        QMessageBox::warning(this, "No Selection", "Please click a row to delete.");
        return;
    }
    m_rules.removeAt(row);
    populateTable();
}

void DynamicFavoritesDialog::onSave() {
    QList<DynamicFavoriteRule> nextRules;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        DynamicFavoriteRule r;

        QLineEdit* nameEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 0));
        r.name = nameEdit ? nameEdit->text().trimmed() : "Rule";

        QComboBox* typeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 1));
        r.ruleType = typeCombo ? typeCombo->currentData().toString() : "Wildcard";

        QLineEdit* valEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 2));
        r.value = valEdit ? valEdit->text().trimmed() : "";

        if (r.name.isEmpty() || r.value.isEmpty()) {
            QMessageBox::warning(this, "Invalid Rule", QString("Rule %1 contains empty fields. Please set them or delete the rule.").arg(i + 1));
            return;
        }

        nextRules.append(r);
    }

    FavoritesManager::instance().setDynamicRules(nextRules);
    accept();
}
