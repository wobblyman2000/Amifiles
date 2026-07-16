#include "agestylingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>

AgeStylingDialog::AgeStylingDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    loadRules();
    populateTable();
}

void AgeStylingDialog::setupUI() {
    setWindowTitle("File Age Style Rules & Customization");
    resize(700, 400);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QTableWidget { background-color: #181825; color: #cdd6f4; gridline-color: #313244; border: 1px solid #313244; border-radius: 4px; }"
        "QHeaderView::section { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; border: 1px solid #45475a; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    QLabel* header = new QLabel("<b>Customize File Colors & Emoji Badges by File Age</b>", this);
    header->setStyleSheet("font-size: 16px; color: #89b4fa;");
    mainLayout->addWidget(header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Rule Name", "Condition", "Days", "Text Color", "Icon Prefix (Emoji)"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    
    // Connect click signal for color selection cell
    connect(m_table, &QTableWidget::cellClicked, this, &AgeStylingDialog::onColorCellClicked);

    mainLayout->addWidget(m_table);

    // Rule Buttons Row
    QHBoxLayout* ruleButtons = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("Add New Rule", this);
    btnAdd->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; } QPushButton:hover { background-color: #b4befe; }");
    connect(btnAdd, &QPushButton::clicked, this, &AgeStylingDialog::onAddRule);
    ruleButtons->addWidget(btnAdd);

    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    btnDelete->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; } QPushButton:hover { background-color: #e08b8b; }");
    connect(btnDelete, &QPushButton::clicked, this, &AgeStylingDialog::onDeleteRule);
    ruleButtons->addWidget(btnDelete);

    QPushButton* btnReset = new QPushButton("Reset to Defaults", this);
    connect(btnReset, &QPushButton::clicked, this, &AgeStylingDialog::onResetDefaults);
    ruleButtons->addWidget(btnReset);

    ruleButtons->addStretch(1);
    mainLayout->addLayout(ruleButtons);

    // Bottom Action Row
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch(1);

    QPushButton* btnSave = new QPushButton("Save & Apply Rules", this);
    btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; } QPushButton:hover { background-color: #94e2d5; }");
    connect(btnSave, &QPushButton::clicked, this, &AgeStylingDialog::onSave);
    bottom->addWidget(btnSave);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottom->addWidget(btnCancel);

    mainLayout->addLayout(bottom);
}

void AgeStylingDialog::loadRules() {
    m_rules.clear();
    QSettings settings("Amifiles", "Amifiles");
    QStringList list = settings.value("preferences/age_rules").toStringList();
    if (list.isEmpty()) {
        m_rules = FileFilterProxyModel::defaultRules();
    } else {
        for (const QString& ruleStr : list) {
            QStringList parts = ruleStr.split(';');
            if (parts.size() >= 5) {
                AgeColorRule r;
                r.op = parts[0];
                r.days = parts[1].toInt();
                r.color = parts[2];
                r.icon = parts[3];
                r.name = parts[4];
                m_rules.append(r);
            }
        }
    }
}

void AgeStylingDialog::populateTable() {
    m_table->setRowCount(0);
    for (int i = 0; i < m_rules.size(); ++i) {
        const AgeColorRule& r = m_rules.at(i);
        m_table->insertRow(i);

        // 1. Rule Name
        QLineEdit* nameEdit = new QLineEdit(this);
        nameEdit->setText(r.name);
        nameEdit->setStyleSheet("QLineEdit { background: transparent; border: none; color: #cdd6f4; }");
        m_table->setCellWidget(i, 0, nameEdit);

        // 2. Condition
        QComboBox* opCombo = new QComboBox(this);
        opCombo->addItems({"Newer than (<=)", "Older than (>=)"});
        opCombo->setCurrentIndex(r.op == "<=" ? 0 : 1);
        opCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        m_table->setCellWidget(i, 1, opCombo);

        // 3. Days
        QSpinBox* daysSpin = new QSpinBox(this);
        daysSpin->setRange(1, 9999);
        daysSpin->setValue(r.days);
        daysSpin->setStyleSheet("QSpinBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        m_table->setCellWidget(i, 2, daysSpin);

        // 4. Color Cell (We show the color name/hex text, and set its background color!)
        QTableWidgetItem* colorItem = new QTableWidgetItem(r.color);
        colorItem->setTextAlignment(Qt::AlignCenter);
        if (r.color != "None" && !r.color.isEmpty()) {
            colorItem->setBackground(QBrush(QColor(r.color)));
            colorItem->setForeground(QBrush(QColor("#11111b")));
            colorItem->setFont(QFont("", -1, QFont::Bold));
        } else {
            colorItem->setText("None");
        }
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable); // Non-editable text, click triggers dialog
        m_table->setItem(i, 3, colorItem);

        // 5. Icon/Emoji Prefix
        QLineEdit* iconEdit = new QLineEdit(this);
        iconEdit->setText(r.icon);
        iconEdit->setStyleSheet("QLineEdit { background: transparent; border: none; color: #cdd6f4; }");
        m_table->setCellWidget(i, 4, iconEdit);
    }
}

void AgeStylingDialog::onAddRule() {
    AgeColorRule r;
    r.name = "New Rule";
    r.op = "<=";
    r.days = 30;
    r.color = "#a6e3a1";
    r.icon = "🟢";

    m_rules.append(r);
    populateTable();
}

void AgeStylingDialog::onDeleteRule() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_rules.size()) {
        QMessageBox::warning(this, "Selection Required", "Please select a rule row to delete.");
        return;
    }
    m_rules.removeAt(row);
    populateTable();
}

void AgeStylingDialog::onColorCellClicked(int row, int col) {
    if (col != 3) return; // Only column 3 is Color

    QTableWidgetItem* colorItem = m_table->item(row, col);
    if (!colorItem) return;

    QString currColorHex = colorItem->text();
    QColor initialColor = (currColorHex == "None") ? Qt::white : QColor(currColorHex);

    QColor chosen = QColorDialog::getColor(initialColor, this, "Choose Rule Text Color");
    if (chosen.isValid()) {
        QString hex = chosen.name();
        colorItem->setText(hex);
        colorItem->setBackground(QBrush(chosen));
        colorItem->setForeground(QBrush(QColor("#11111b")));
        colorItem->setFont(QFont("", -1, QFont::Bold));
    }
}

void AgeStylingDialog::onResetDefaults() {
    m_rules = FileFilterProxyModel::defaultRules();
    populateTable();
}

void AgeStylingDialog::onSave() {
    // Collect all rules from table widgets
    QList<AgeColorRule> updatedRules;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        AgeColorRule r;

        QLineEdit* nameEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 0));
        r.name = nameEdit ? nameEdit->text().trimmed() : "Rule";

        QComboBox* opCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 1));
        r.op = opCombo ? (opCombo->currentIndex() == 0 ? "<=" : ">=") : "<=";

        QSpinBox* daysSpin = qobject_cast<QSpinBox*>(m_table->cellWidget(i, 2));
        r.days = daysSpin ? daysSpin->value() : 30;

        QTableWidgetItem* colorItem = m_table->item(i, 3);
        r.color = colorItem ? colorItem->text().trimmed() : "None";

        QLineEdit* iconEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 4));
        r.icon = iconEdit ? iconEdit->text().trimmed() : "";

        updatedRules.append(r);
    }

    FileFilterProxyModel::saveRules(updatedRules);
    
    QMessageBox::information(this, "Success", "File age styling rules saved and applied!");
    accept();
}
