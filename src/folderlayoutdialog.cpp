#include "folderlayoutdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>

class ButtonChooserDialog : public QDialog {
public:
    ButtonChooserDialog(const QList<CustomButton>& available, const QStringList& selected, QWidget* parent) : QDialog(parent) {
        setWindowTitle("Select Custom Toolbar Buttons");
        QVBoxLayout* layout = new QVBoxLayout(this);
        
        QLabel* label = new QLabel("Select which custom script buttons to display in this folder's toolbar:", this);
        layout->addWidget(label);
        
        m_listWidget = new QListWidget(this);
        for (const auto& btn : available) {
            QListWidgetItem* item = new QListWidgetItem(btn.name, m_listWidget);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected.contains(btn.name) ? Qt::Checked : Qt::Unchecked);
        }
        layout->addWidget(m_listWidget);
        
        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("OK", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);
        
        setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                      "QLabel { color: #cdd6f4; }"
                      "QListWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
                      "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; }"
                      "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    }
    
    QStringList selectedButtons() const {
        QStringList res;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem* item = m_listWidget->item(i);
            if (item->checkState() == Qt::Checked) {
                res.append(item->text());
            }
        }
        return res;
    }
private:
    QListWidget* m_listWidget;
};

FolderLayoutDialog::FolderLayoutDialog(const QList<FolderLayoutRule>& existingRules, const QList<CustomButton>& availableButtons, QWidget* parent)
    : QDialog(parent), m_rules(existingRules), m_availableButtons(availableButtons) {
    setupUI();
    populateTable();
}

void FolderLayoutDialog::setupUI() {
    setWindowTitle("Configure Folder-Specific Layouts");
    resize(750, 450);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* title = new QLabel("Define custom view modes and custom script toolbars for specific folders or media types:", this);
    title->setStyleSheet("font-weight: bold; font-size: 13px; color: #f5c2e7; margin-bottom: 6px;");
    mainLayout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Match Type", "Target Path / Category", "", "View Mode", "Custom Buttons"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setStyleSheet("QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; gridline-color: #313244; }"
                           "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    mainLayout->addWidget(m_table);

    QHBoxLayout* controlLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("Add Rule", this);
    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    QPushButton* btnSave = new QPushButton("Save & Apply", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    btnAdd->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnDelete->setStyleSheet("QPushButton { background-color: #313244; color: #f38ba8; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #f38ba8; color: #11111b; }");
    btnSave->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border: 1px solid #89b4fa; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #b4befe; }");
    btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #45475a; }");

    connect(btnAdd, &QPushButton::clicked, this, &FolderLayoutDialog::onAddRule);
    connect(btnDelete, &QPushButton::clicked, this, &FolderLayoutDialog::onDeleteRule);
    connect(btnSave, &QPushButton::clicked, this, &FolderLayoutDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    controlLayout->addWidget(btnAdd);
    controlLayout->addWidget(btnDelete);
    controlLayout->addStretch();
    controlLayout->addWidget(btnSave);
    controlLayout->addWidget(btnCancel);
    mainLayout->addLayout(controlLayout);
}

void FolderLayoutDialog::populateTable() {
    m_table->clearContents();
    m_table->setRowCount(m_rules.size());

    for (int i = 0; i < m_rules.size(); ++i) {
        const FolderLayoutRule& r = m_rules[i];

        // 1. Rule Type Combo
        QComboBox* typeCombo = new QComboBox(this);
        typeCombo->addItem("Exact Path", "Path");
        typeCombo->addItem("Folder Category", "Category");
        typeCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        int typeIdx = (r.ruleType == "Category") ? 1 : 0;
        typeCombo->setCurrentIndex(typeIdx);
        m_table->setCellWidget(i, 0, typeCombo);

        // 2. Target Path / Category (Combo for Category type, LineEdit for Path type)
        // We will make a custom container layout to switch dynamically, or just a QWidget switch!
        // To keep it simple: we can construct a line edit, and if it's Category we can load Category dropdown!
        // Actually, we can use a combo box for Category selection or text editing.
        // Let's use a dynamic switcher widget!
        QWidget* targetWidget = new QWidget(this);
        QHBoxLayout* targetLayout = new QHBoxLayout(targetWidget);
        targetLayout->setContentsMargins(0, 0, 0, 0);
        targetLayout->setSpacing(2);

        QLineEdit* pathEdit = new QLineEdit(targetWidget);
        pathEdit->setText(r.value);
        pathEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 2px; }");
        
        QComboBox* categoryCombo = new QComboBox(targetWidget);
        categoryCombo->addItem("Music Documents (Audio Files)", "Music");
        categoryCombo->addItem("Video / Movie Clips (Video Files)", "Videos");
        categoryCombo->addItem("Text Documents (Doc/Text Files)", "Documents");
        categoryCombo->addItem("Images & Photographs (Image Files)", "Images");
        categoryCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        
        int catIdx = categoryCombo->findData(r.value);
        if (catIdx >= 0) categoryCombo->setCurrentIndex(catIdx);

        targetLayout->addWidget(pathEdit);
        targetLayout->addWidget(categoryCombo);

        auto updateVisibleWidgets = [typeCombo, pathEdit, categoryCombo]() {
            bool isPath = (typeCombo->currentData().toString() == "Path");
            pathEdit->setVisible(isPath);
            categoryCombo->setVisible(!isPath);
        };

        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateVisibleWidgets);
        updateVisibleWidgets();

        m_table->setCellWidget(i, 1, targetWidget);

        // 3. Browse Button (only for path rules)
        QPushButton* browseBtn = new QPushButton("...", this);
        browseBtn->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; } QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
        connect(browseBtn, &QPushButton::clicked, this, [this, i]() { onBrowsePath(i); });
        
        auto updateBrowseVisible = [typeCombo, browseBtn]() {
            browseBtn->setVisible(typeCombo->currentData().toString() == "Path");
        };
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateBrowseVisible);
        updateBrowseVisible();

        m_table->setCellWidget(i, 2, browseBtn);

        // 4. View Mode Combo
        QComboBox* modeCombo = new QComboBox(this);
        modeCombo->addItem("Keep Default", "Default");
        modeCombo->addItem("Details Table", "List");
        modeCombo->addItem("Grid / Icons", "Grid");
        modeCombo->addItem("Card / Tiles", "Card");
        modeCombo->addItem("Miller Columns", "Miller");
        modeCombo->addItem("Chronological Timeline", "Timeline");
        modeCombo->addItem("Filmstrip View", "Filmstrip");
        modeCombo->addItem("Theater View", "Theater");
        modeCombo->setStyleSheet("QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
        int modeIdx = 0;
        if (r.viewMode == "List") modeIdx = 1;
        else if (r.viewMode == "Grid") modeIdx = 2;
        else if (r.viewMode == "Card") modeIdx = 3;
        else if (r.viewMode == "Miller") modeIdx = 4;
        else if (r.viewMode == "Timeline") modeIdx = 5;
        else if (r.viewMode == "Filmstrip") modeIdx = 6;
        else if (r.viewMode == "Theater") modeIdx = 7;
        modeCombo->setCurrentIndex(modeIdx);
        m_table->setCellWidget(i, 3, modeCombo);

        // 5. Buttons Chooser Button
        QPushButton* buttonsBtn = new QPushButton(this);
        buttonsBtn->setStyleSheet("QPushButton { background-color: #11111b; color: #f5c2e7; text-align: left; padding: 4px; border: 1px solid #313244; } QPushButton:hover { background-color: #313244; }");
        
        QString btnText = r.customButtons.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(r.customButtons.size());
        buttonsBtn->setText(btnText);
        buttonsBtn->setProperty("selectedButtons", r.customButtons);

        connect(buttonsBtn, &QPushButton::clicked, this, [this, i]() { onChooseButtons(i); });
        m_table->setCellWidget(i, 4, buttonsBtn);
    }
}

void FolderLayoutDialog::onAddRule() {
    harvestRules();
    FolderLayoutRule r;
    r.ruleType = "Path";
    r.value = "";
    r.viewMode = "Default";
    
    m_rules.append(r);
    populateTable();
}

void FolderLayoutDialog::onDeleteRule() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount()) {
        QMessageBox::warning(this, "No Selection", "Please click a row to delete.");
        return;
    }
    harvestRules();
    m_rules.removeAt(row);
    populateTable();
}

void FolderLayoutDialog::onBrowsePath(int row) {
    QWidget* w = m_table->cellWidget(row, 1);
    if (!w) return;
    QLineEdit* pathEdit = w->findChild<QLineEdit*>();
    if (!pathEdit) return;

    QString dir = QFileDialog::getExistingDirectory(this, "Select Rule Target Folder", pathEdit->text());
    if (!dir.isEmpty()) {
        pathEdit->setText(dir);
    }
}

void FolderLayoutDialog::onChooseButtons(int row) {
    QPushButton* buttonsBtn = qobject_cast<QPushButton*>(m_table->cellWidget(row, 4));
    if (!buttonsBtn) return;

    QStringList current = buttonsBtn->property("selectedButtons").toStringList();
    ButtonChooserDialog dlg(m_availableButtons, current, this);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList next = dlg.selectedButtons();
        buttonsBtn->setProperty("selectedButtons", next);
        QString btnText = next.isEmpty() ? "All Buttons (Default)" : QString("%1 Selected").arg(next.size());
        buttonsBtn->setText(btnText);
    }
}

void FolderLayoutDialog::onSave() {
    QList<FolderLayoutRule> nextRules;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        FolderLayoutRule r;

        QComboBox* typeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 0));
        r.ruleType = typeCombo ? typeCombo->currentData().toString() : "Path";

        QWidget* w = m_table->cellWidget(i, 1);
        if (w) {
            QLineEdit* pathEdit = w->findChild<QLineEdit*>();
            QComboBox* categoryCombo = w->findChild<QComboBox*>();
            if (r.ruleType == "Path") {
                r.value = pathEdit ? pathEdit->text().trimmed() : "";
            } else {
                r.value = categoryCombo ? categoryCombo->currentData().toString() : "Music";
            }
        }

        QComboBox* modeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 3));
        r.viewMode = modeCombo ? modeCombo->currentData().toString() : "Default";

        QPushButton* buttonsBtn = qobject_cast<QPushButton*>(m_table->cellWidget(i, 4));
        r.customButtons = buttonsBtn ? buttonsBtn->property("selectedButtons").toStringList() : QStringList();

        if (r.value.isEmpty()) {
            QMessageBox::warning(this, "Invalid Rule", QString("Rule %1 has an empty target path. Please configure it or delete the rule.").arg(i + 1));
            return;
        }

        nextRules.append(r);
    }

    m_rules = nextRules;
    accept();
}

void FolderLayoutDialog::harvestRules() {
    QList<FolderLayoutRule> nextRules;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        FolderLayoutRule r;
        QComboBox* typeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 0));
        r.ruleType = typeCombo ? typeCombo->currentData().toString() : "Path";
        QWidget* w = m_table->cellWidget(i, 1);
        if (w) {
            QLineEdit* pathEdit = w->findChild<QLineEdit*>();
            QComboBox* categoryCombo = w->findChild<QComboBox*>();
            if (r.ruleType == "Path") {
                r.value = pathEdit ? pathEdit->text().trimmed() : "";
            } else {
                r.value = categoryCombo ? categoryCombo->currentData().toString() : "Music";
            }
        }
        QComboBox* modeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(i, 3));
        r.viewMode = modeCombo ? modeCombo->currentData().toString() : "Default";
        QPushButton* buttonsBtn = qobject_cast<QPushButton*>(m_table->cellWidget(i, 4));
        r.customButtons = buttonsBtn ? buttonsBtn->property("selectedButtons").toStringList() : QStringList();
        nextRules.append(r);
    }
    m_rules = nextRules;
}
