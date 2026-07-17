#include "autotagdialog.h"
#include "tagmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QUuid>

AutoTagDialog::AutoTagDialog(QWidget* parent) : QDialog(parent) {
    m_rules = TagManager::instance().getAutoTagRules();
    setupUI();
    populateTable();
}

void AutoTagDialog::setupUI() {
    setWindowTitle("Configure Automated File Tagging Rules");
    resize(780, 420);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QLabel* title = new QLabel("Define criteria to tag and color-badge files matching specific filename wildcards or sizes automatically:", this);
    title->setStyleSheet("font-weight: bold; color: #fab387;");
    title->setWordWrap(true);
    mainLayout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"Rule Name", "Wildcard Pattern", "Min Size (MB)", "Tag Label", "Badge Color", "Active"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setStyleSheet("QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; gridline-color: #313244; }"
                           "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    mainLayout->addWidget(m_table);

    QHBoxLayout* controlLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("Add Rule", this);
    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    QPushButton* btnSave = new QPushButton("Save Rules", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnDelete->setStyleSheet("QPushButton { background-color: #313244; color: #f38ba8; } QPushButton:hover { background-color: #f38ba8; color: #11111b; }");
    btnSave->setStyleSheet("QPushButton { background-color: #fab387; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #f9e2af; }");
    btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; }");

    connect(btnAdd, &QPushButton::clicked, this, &AutoTagDialog::onAddRule);
    connect(btnDelete, &QPushButton::clicked, this, &AutoTagDialog::onDeleteRule);
    connect(btnSave, &QPushButton::clicked, this, &AutoTagDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    controlLayout->addWidget(btnAdd);
    controlLayout->addWidget(btnDelete);
    controlLayout->addStretch();
    controlLayout->addWidget(btnSave);
    controlLayout->addWidget(btnCancel);
    mainLayout->addLayout(controlLayout);
}

void AutoTagDialog::populateTable() {
    m_table->clearContents();
    m_table->setRowCount(m_rules.size());

    for (int i = 0; i < m_rules.size(); ++i) {
        const AutoTagRule& rule = m_rules[i];

        // 0. Rule Name
        QLineEdit* nameEdit = new QLineEdit(this);
        nameEdit->setText(rule.name);
        nameEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        m_table->setCellWidget(i, 0, nameEdit);

        // 1. Wildcard Pattern
        QLineEdit* patEdit = new QLineEdit(this);
        patEdit->setText(rule.pattern);
        patEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        m_table->setCellWidget(i, 1, patEdit);

        // 2. Min Size
        QSpinBox* sizeSpin = new QSpinBox(this);
        sizeSpin->setRange(0, 100000);
        sizeSpin->setSuffix(" MB");
        sizeSpin->setValue(rule.minSize == -1 ? 0 : rule.minSize);
        sizeSpin->setStyleSheet("QSpinBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        m_table->setCellWidget(i, 2, sizeSpin);

        // 3. Tag Label
        QLineEdit* tagEdit = new QLineEdit(this);
        tagEdit->setText(rule.tag);
        tagEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        m_table->setCellWidget(i, 3, tagEdit);

        // 4. Color Dropdown
        QComboBox* colCombo = new QComboBox(this);
        colCombo->addItem("None", "none");
        colCombo->addItem("Red", "red");
        colCombo->addItem("Orange", "orange");
        colCombo->addItem("Yellow", "yellow");
        colCombo->addItem("Green", "green");
        colCombo->addItem("Blue", "blue");
        colCombo->addItem("Purple", "purple");
        colCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        
        int idx = colCombo->findData(rule.color);
        if (idx != -1) colCombo->setCurrentIndex(idx);
        m_table->setCellWidget(i, 4, colCombo);

        // 5. Active Checkbox
        QCheckBox* actCheck = new QCheckBox(this);
        actCheck->setChecked(rule.active);
        m_table->setCellWidget(i, 5, actCheck);
    }
}

void AutoTagDialog::onAddRule() {
    AutoTagRule rule;
    rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.name = "New Automated Rule";
    rule.pattern = "*.mp3";
    rule.minSize = -1;
    rule.tag = "Music";
    rule.color = "blue";
    rule.active = true;
    m_rules.append(rule);
    populateTable();
}

void AutoTagDialog::onDeleteRule() {
    int r = m_table->currentRow();
    if (r < 0 || r >= m_rules.size()) {
        QMessageBox::warning(this, "No Selection", "Please click a row to delete.");
        return;
    }
    m_rules.removeAt(r);
    populateTable();
}

void AutoTagDialog::onSave() {
    QList<AutoTagRule> nextRules;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        AutoTagRule rule;
        rule.id = m_rules[i].id;

        QLineEdit* nameE = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 0));
        rule.name = nameE ? nameE->text().trimmed() : "Rule";

        QLineEdit* patE = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 1));
        rule.pattern = patE ? patE->text().trimmed() : "";

        QSpinBox* sizeS = qobject_cast<QSpinBox*>(m_table->cellWidget(i, 2));
        rule.minSize = (sizeS && sizeS->value() > 0) ? sizeS->value() : -1;

        QLineEdit* tagE = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 3));
        rule.tag = tagE ? tagE->text().trimmed() : "";

        QComboBox* colC = qobject_cast<QComboBox*>(m_table->cellWidget(i, 4));
        rule.color = colC ? colC->currentData().toString() : "none";

        QCheckBox* actC = qobject_cast<QCheckBox*>(m_table->cellWidget(i, 5));
        rule.active = actC ? actC->isChecked() : true;

        if (rule.pattern.isEmpty() && rule.minSize == -1) {
            QMessageBox::warning(this, "Invalid Entry", QString("Rule #%1 must define either a wildcard pattern or a min size.").arg(i + 1));
            return;
        }

        nextRules.append(rule);
    }

    TagManager::instance().setAutoTagRules(nextRules);
    accept();
}
