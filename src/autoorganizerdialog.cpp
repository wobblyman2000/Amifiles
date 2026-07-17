#include "autoorganizerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QRegularExpression>

AutoOrganizerDialog::AutoOrganizerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Smart Auto-Organizer Rules");
    resize(650, 420);
    setupUI();
    m_rules = loadRules();
    refreshTable();
}

void AutoOrganizerDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; }"
        "QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 11px; }"
        "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnSave { background-color: #a6e3a1; color: #11111b; }"
        "QPushButton#btnSave:hover { background-color: #b4befe; }"
        "QPushButton#btnAdd { background-color: #89b4fa; color: #11111b; }"
        "QPushButton#btnAdd:hover { background-color: #b4befe; }"
    );

    // Rule configurations table
    m_rulesTable = new QTableWidget(this);
    m_rulesTable->setColumnCount(5);
    m_rulesTable->setHorizontalHeaderLabels({"Rule Name", "Source Directory", "Patterns", "Destination", "Active"});
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mainLayout->addWidget(m_rulesTable, 1);

    // Add Rule input widgets
    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    m_ruleNameEdit = new QLineEdit(this);
    m_ruleNameEdit->setPlaceholderText("e.g. Organize Downloads Photos");
    form->addRow("Rule Name:", m_ruleNameEdit);

    // Source browse
    QHBoxLayout* sourceLayout = new QHBoxLayout();
    m_sourceEdit = new QLineEdit(this);
    sourceLayout->addWidget(m_sourceEdit);
    QPushButton* btnBrowseSource = new QPushButton("Browse...", this);
    connect(btnBrowseSource, &QPushButton::clicked, this, &AutoOrganizerDialog::onBrowseSource);
    sourceLayout->addWidget(btnBrowseSource);
    form->addRow("Source Folder:", sourceLayout);

    m_patternEdit = new QLineEdit(this);
    m_patternEdit->setPlaceholderText("Comma-separated wildcards (e.g. *.jpg, *.png)");
    form->addRow("Pattern filters:", m_patternEdit);

    // Target browse
    QHBoxLayout* targetLayout = new QHBoxLayout();
    m_targetEdit = new QLineEdit(this);
    targetLayout->addWidget(m_targetEdit);
    QPushButton* btnBrowseTarget = new QPushButton("Browse...", this);
    connect(btnBrowseTarget, &QPushButton::clicked, this, &AutoOrganizerDialog::onBrowseTarget);
    targetLayout->addWidget(btnBrowseTarget);
    form->addRow("Destination:", targetLayout);

    mainLayout->addLayout(form);

    // Dialog controls layout
    QHBoxLayout* controlsLayout = new QHBoxLayout();

    QPushButton* btnAdd = new QPushButton("Add Rule", this);
    btnAdd->setObjectName("btnAdd");
    connect(btnAdd, &QPushButton::clicked, this, &AutoOrganizerDialog::onAddRule);
    controlsLayout->addWidget(btnAdd);

    QPushButton* btnDelete = new QPushButton("Delete Selected", this);
    connect(btnDelete, &QPushButton::clicked, this, &AutoOrganizerDialog::onDeleteRule);
    controlsLayout->addWidget(btnDelete);

    controlsLayout->addStretch();

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    controlsLayout->addWidget(btnCancel);

    QPushButton* btnSave = new QPushButton("Save & Run", this);
    btnSave->setObjectName("btnSave");
    connect(btnSave, &QPushButton::clicked, this, &AutoOrganizerDialog::onSaveClicked);
    controlsLayout->addWidget(btnSave);

    mainLayout->addLayout(controlsLayout);
}

void AutoOrganizerDialog::refreshTable() {
    m_rulesTable->setRowCount(0);
    for (int i = 0; i < m_rules.size(); ++i) {
        const auto& r = m_rules[i];
        m_rulesTable->insertRow(i);

        QTableWidgetItem* nameItem = new QTableWidgetItem(r.name);
        nameItem->setForeground(QBrush(QColor("#cdd6f4")));

        QTableWidgetItem* srcItem = new QTableWidgetItem(r.sourcePath);
        srcItem->setForeground(QBrush(QColor("#a6adc8")));

        QTableWidgetItem* patItem = new QTableWidgetItem(r.pattern);
        patItem->setForeground(QBrush(QColor("#f9e2af")));

        QTableWidgetItem* targetItem = new QTableWidgetItem(r.targetPath);
        targetItem->setForeground(QBrush(QColor("#a6e3a1")));

        QTableWidgetItem* activeItem = new QTableWidgetItem(r.active ? "Yes" : "No");
        activeItem->setForeground(QBrush(r.active ? QColor("#a6e3a1") : QColor("#f38ba8")));

        m_rulesTable->setItem(i, 0, nameItem);
        m_rulesTable->setItem(i, 1, srcItem);
        m_rulesTable->setItem(i, 2, patItem);
        m_rulesTable->setItem(i, 3, targetItem);
        m_rulesTable->setItem(i, 4, activeItem);
    }
}

void AutoOrganizerDialog::onAddRule() {
    QString name = m_ruleNameEdit->text().trimmed();
    QString src = m_sourceEdit->text().trimmed();
    QString pat = m_patternEdit->text().trimmed();
    QString target = m_targetEdit->text().trimmed();

    if (name.isEmpty() || src.isEmpty() || pat.isEmpty() || target.isEmpty()) {
        QMessageBox::warning(this, "Add Rule", "Please fill in all rule fields.");
        return;
    }

    AutoOrganizerRule r;
    r.name = name;
    r.sourcePath = src;
    r.pattern = pat;
    r.targetPath = target;
    r.active = true;

    m_rules.append(r);
    refreshTable();

    m_ruleNameEdit->clear();
    m_sourceEdit->clear();
    m_patternEdit->clear();
    m_targetEdit->clear();
}

void AutoOrganizerDialog::onDeleteRule() {
    int row = m_rulesTable->currentRow();
    if (row >= 0 && row < m_rules.size()) {
        m_rules.removeAt(row);
        refreshTable();
    }
}

void AutoOrganizerDialog::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Source Directory", QDir::homePath());
    if (!dir.isEmpty()) {
        m_sourceEdit->setText(dir);
    }
}

void AutoOrganizerDialog::onBrowseTarget() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Directory", QDir::homePath());
    if (!dir.isEmpty()) {
        m_targetEdit->setText(dir);
    }
}

void AutoOrganizerDialog::onSaveClicked() {
    saveRules(m_rules);
    executeRules();
    accept();
}

QList<AutoOrganizerRule> AutoOrganizerDialog::loadRules() {
    QList<AutoOrganizerRule> list;
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized = settings.value("auto_organizer/rules").toStringList();
    for (const QString& s : serialized) {
        QStringList parts = s.split(';');
        if (parts.size() >= 5) {
            AutoOrganizerRule r;
            r.name = parts[0];
            r.sourcePath = parts[1];
            r.pattern = parts[2];
            r.targetPath = parts[3];
            r.active = (parts[4] == "1");
            list.append(r);
        }
    }
    return list;
}

void AutoOrganizerDialog::saveRules(const QList<AutoOrganizerRule>& rules) {
    QSettings settings("Amifiles", "Amifiles");
    QStringList serialized;
    for (const auto& r : rules) {
        serialized.append(QString("%1;%2;%3;%4;%5")
                          .arg(r.name)
                          .arg(r.sourcePath)
                          .arg(r.pattern)
                          .arg(r.targetPath)
                          .arg(r.active ? "1" : "0"));
    }
    settings.setValue("auto_organizer/rules", serialized);
}

void AutoOrganizerDialog::executeRules() {
    QList<AutoOrganizerRule> rules = loadRules();
    for (const auto& r : rules) {
        if (!r.active) continue;

        QDir srcDir(r.sourcePath);
        if (!srcDir.exists()) continue;

        // Split comma-separated wildcard filters
        QStringList filters = r.pattern.split(',', Qt::SkipEmptyParts);
        for (QString& f : filters) {
            f = f.trimmed();
        }

        // Get matching files
        QFileInfoList files = srcDir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
        if (files.isEmpty()) continue;

        // Ensure target directory exists
        QDir().mkpath(r.targetPath);

        for (const QFileInfo& fi : files) {
            QString targetFile = r.targetPath + "/" + fi.fileName();
            // Prevent overriding existing files
            if (!QFileInfo::exists(targetFile)) {
                QFile::rename(fi.absoluteFilePath(), targetFile);
            }
        }
    }
}
