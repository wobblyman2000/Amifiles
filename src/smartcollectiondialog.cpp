#include "smartcollectiondialog.h"
#include "favoritesmanager.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

SmartCollectionDialog::SmartCollectionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Create Smart Collection");
    resize(480, 480);
    setStyleSheet(Theme::getStylesheet());
    setupUI();
}

void SmartCollectionDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(10);

    m_txtName = new QLineEdit(this);
    m_txtName->setPlaceholderText("Collection Name (e.g. Big PDF Documents)");
    form->addRow("Collection Name:", m_txtName);

    mainLayout->addLayout(form);

    // Group: Search Paths
    QGroupBox* pathGroup = new QGroupBox("Directories to Search", this);
    QVBoxLayout* pathLayout = new QVBoxLayout(pathGroup);
    m_listDirs = new QListWidget(this);
    m_listDirs->setFixedHeight(120);

    QHBoxLayout* pathButtons = new QHBoxLayout();
    m_btnAddDir = new QPushButton("Add Folder...", this);
    m_btnRemoveDir = new QPushButton("Remove Selected", this);
    pathButtons->addWidget(m_btnAddDir);
    pathButtons->addWidget(m_btnRemoveDir);
    pathButtons->addStretch();

    pathLayout->addWidget(m_listDirs);
    pathLayout->addLayout(pathButtons);
    mainLayout->addWidget(pathGroup);

    connect(m_btnAddDir, &QPushButton::clicked, this, &SmartCollectionDialog::onAddDir);
    connect(m_btnRemoveDir, &QPushButton::clicked, this, &SmartCollectionDialog::onRemoveDir);

    // Group: Search Parameters
    QGroupBox* filterGroup = new QGroupBox("Filter Criteria", this);
    QFormLayout* filterForm = new QFormLayout(filterGroup);
    filterForm->setSpacing(8);

    m_txtPattern = new QLineEdit(this);
    m_txtPattern->setPlaceholderText("Wildcard filename (e.g. *.pdf, *invoice*)");
    filterForm->addRow("Filename Pattern:", m_txtPattern);

    m_spinMinSize = new QDoubleSpinBox(this);
    m_spinMinSize->setRange(0.0, 10000.0);
    m_spinMinSize->setSuffix(" MB");
    filterForm->addRow("Min File Size:", m_spinMinSize);

    m_spinMaxSize = new QDoubleSpinBox(this);
    m_spinMaxSize->setRange(0.0, 100000.0);
    m_spinMaxSize->setValue(0.0); // 0 means no max
    m_spinMaxSize->setSuffix(" MB (0 for no limit)");
    filterForm->addRow("Max File Size:", m_spinMaxSize);

    m_spinAgeDays = new QSpinBox(this);
    m_spinAgeDays->setRange(0, 3650);
    m_spinAgeDays->setValue(0); // 0 means no age limit
    m_spinAgeDays->setSuffix(" Days (0 for no limit)");
    filterForm->addRow("Max File Age:", m_spinAgeDays);

    mainLayout->addWidget(filterGroup);

    // Group: Dialog actions
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnSave = new QPushButton("Save Collection", this);
    m_btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }");
    connect(m_btnSave, &QPushButton::clicked, this, &SmartCollectionDialog::onSave);
    btnLayout->addWidget(m_btnSave);

    mainLayout->addLayout(btnLayout);
}

void SmartCollectionDialog::onAddDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory", QDir::homePath());
    if (!dir.isEmpty()) {
        m_listDirs->addItem(dir);
    }
}

void SmartCollectionDialog::onRemoveDir() {
    QListWidgetItem* item = m_listDirs->currentItem();
    if (item) {
        delete m_listDirs->takeItem(m_listDirs->row(item));
    }
}

void SmartCollectionDialog::onSave() {
    QString name = m_txtName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please specify a name for this smart collection.");
        return;
    }

    QStringList dirs;
    for (int i = 0; i < m_listDirs->count(); ++i) {
        dirs.append(m_listDirs->item(i)->text());
    }

    if (dirs.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please add at least one directory to search.");
        return;
    }

    // Build the query JSON string
    QJsonObject obj;
    QJsonArray dirsArr;
    for (const QString& d : dirs) {
        dirsArr.append(d);
    }
    obj["dirs"] = dirsArr;
    obj["pattern"] = m_txtPattern->text().trimmed();
    
    double minS = m_spinMinSize->value();
    if (minS > 0.0) obj["minSizeMB"] = minS;
    
    double maxS = m_spinMaxSize->value();
    if (maxS > 0.0) obj["maxSizeMB"] = maxS;
    
    int age = m_spinAgeDays->value();
    if (age > 0) obj["ageDays"] = age;

    QJsonDocument doc(obj);
    QString jsonStr(doc.toJson(QJsonDocument::Compact));

    // Register rule
    DynamicFavoriteRule rule;
    rule.name = name;
    rule.ruleType = "Smart";
    rule.value = jsonStr;

    QList<DynamicFavoriteRule> rules = FavoritesManager::instance().getDynamicRules();
    rules.append(rule);
    FavoritesManager::instance().setDynamicRules(rules);

    accept();
}
