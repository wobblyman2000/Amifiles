#include "bulkrename.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QInputDialog>
#include <QRegularExpression>
#include <QListWidget>
#include <QDate>
#include <QDebug>

BulkRenameDialog::BulkRenameDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Bulk Rename Tool");
    resize(900, 600);

    setupUI();
    loadPresetNames();
    onUpdatePreview();
}

void BulkRenameDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ================= LEFT PANEL: CONFIGURATION OPTIONS =================
    QWidget* optWidget = new QWidget(this);
    QVBoxLayout* optLayout = new QVBoxLayout(optWidget);
    optLayout->setContentsMargins(0, 0, 0, 0);
    optLayout->setSpacing(8);
    optWidget->setMaximumWidth(320);

    // 1. Presets Group
    QGroupBox* grpPresets = new QGroupBox("Renaming Presets", optWidget);
    QGridLayout* layoutPresets = new QGridLayout(grpPresets);
    m_comboPresets = new QComboBox(grpPresets);
    m_comboPresets->setPlaceholderText("Select a preset...");
    QPushButton* btnLoad = new QPushButton("Load", grpPresets);
    m_btnSavePreset = new QPushButton("Save", grpPresets);
    m_btnDeletePreset = new QPushButton("Delete", grpPresets);

    connect(btnLoad, &QPushButton::clicked, this, &BulkRenameDialog::onLoadPreset);
    connect(m_btnSavePreset, &QPushButton::clicked, this, &BulkRenameDialog::onSavePreset);
    connect(m_btnDeletePreset, &QPushButton::clicked, this, &BulkRenameDialog::onDeletePreset);

    layoutPresets->addWidget(m_comboPresets, 0, 0, 1, 3);
    layoutPresets->addWidget(btnLoad, 1, 0);
    layoutPresets->addWidget(m_btnSavePreset, 1, 1);
    layoutPresets->addWidget(m_btnDeletePreset, 1, 2);
    optLayout->addWidget(grpPresets);

    // 2. Case Conversion Group
    QGroupBox* grpCase = new QGroupBox("Case Conversion", optWidget);
    QVBoxLayout* layoutCase = new QVBoxLayout(grpCase);
    m_comboCase = new QComboBox(grpCase);
    m_comboCase->addItems({"No Change", "UPPERCASE", "lowercase", "Title Case", "Sentence Case"});
    connect(m_comboCase, &QComboBox::currentIndexChanged, this, &BulkRenameDialog::onUpdatePreview);
    layoutCase->addWidget(m_comboCase);
    optLayout->addWidget(grpCase);

    // 3. Find and Replace Group
    QGroupBox* grpReplace = new QGroupBox("Find and Replace", optWidget);
    QGridLayout* layoutReplace = new QGridLayout(grpReplace);
    
    layoutReplace->addWidget(new QLabel("Find:", grpReplace), 0, 0);
    m_txtFind = new QLineEdit(grpReplace);
    layoutReplace->addWidget(m_txtFind, 0, 1);

    layoutReplace->addWidget(new QLabel("Replace:", grpReplace), 1, 0);
    m_txtReplace = new QLineEdit(grpReplace);
    layoutReplace->addWidget(m_txtReplace, 1, 1);

    m_chkRegex = new QCheckBox("Use Regular Expressions (Regex)", grpReplace);
    m_chkCaseSensitive = new QCheckBox("Case Sensitive Matching", grpReplace);
    m_chkApplyToExt = new QCheckBox("Rename File Extensions as well", grpReplace);

    connect(m_txtFind, &QLineEdit::textChanged, this, &BulkRenameDialog::onUpdatePreview);
    connect(m_txtReplace, &QLineEdit::textChanged, this, &BulkRenameDialog::onUpdatePreview);
    connect(m_chkRegex, &QCheckBox::toggled, this, &BulkRenameDialog::onUpdatePreview);
    connect(m_chkCaseSensitive, &QCheckBox::toggled, this, &BulkRenameDialog::onUpdatePreview);
    connect(m_chkApplyToExt, &QCheckBox::toggled, this, &BulkRenameDialog::onUpdatePreview);

    layoutReplace->addWidget(m_chkRegex, 2, 0, 1, 2);
    layoutReplace->addWidget(m_chkCaseSensitive, 3, 0, 1, 2);
    layoutReplace->addWidget(m_chkApplyToExt, 4, 0, 1, 2);
    optLayout->addWidget(grpReplace);

    // 3.5. Quick Regex Cheatsheet Group
    QGroupBox* grpCheatsheet = new QGroupBox("Quick Regex Cheatsheet", optWidget);
    QVBoxLayout* layoutCheatsheet = new QVBoxLayout(grpCheatsheet);
    layoutCheatsheet->setContentsMargins(4, 4, 4, 4);
    QListWidget* listCheatsheet = new QListWidget(grpCheatsheet);
    listCheatsheet->setFixedHeight(100);
    listCheatsheet->addItems({
        "Remove Spaces (replace with _)",
        "Remove Digits",
        "Prepend Today's Date (YYYY-MM-DD_)",
        "Add Prefix 'prefix_'",
        "Add Suffix '_suffix'",
        "Strip Non-ASCII characters",
        "Convert Space to Hyphen"
    });
    connect(listCheatsheet, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        QString text = item->text();
        if (text.startsWith("Remove Spaces")) {
            m_txtFind->setText("\\s+");
            m_txtReplace->setText("_");
            m_chkRegex->setChecked(true);
        } else if (text.startsWith("Remove Digits")) {
            m_txtFind->setText("\\d+");
            m_txtReplace->setText("");
            m_chkRegex->setChecked(true);
        } else if (text.startsWith("Prepend Today's Date")) {
            QString today = QDate::currentDate().toString("yyyy-MM-dd_");
            m_txtFind->setText("^");
            m_txtReplace->setText(today);
            m_chkRegex->setChecked(true);
        } else if (text.startsWith("Add Prefix")) {
            m_txtFind->setText("^");
            m_txtReplace->setText("prefix_");
            m_chkRegex->setChecked(true);
        } else if (text.startsWith("Add Suffix")) {
            m_txtFind->setText("$");
            m_txtReplace->setText("_suffix");
            m_chkRegex->setChecked(true);
        } else if (text.startsWith("Strip Non-ASCII")) {
            m_txtFind->setText("[^\\x00-\\x7F]");
            m_txtReplace->setText("");
            m_chkRegex->setChecked(true);
        } else if (text.startsWith("Convert Space to Hyphen")) {
            m_txtFind->setText(" ");
            m_txtReplace->setText("-");
            m_chkRegex->setChecked(false);
        }
        onUpdatePreview();
    });
    layoutCheatsheet->addWidget(listCheatsheet);
    optLayout->addWidget(grpCheatsheet);

    // 4. Numbering Group
    QGroupBox* grpNum = new QGroupBox("Index Numbering", optWidget);
    QGridLayout* layoutNum = new QGridLayout(grpNum);
    
    m_chkNumbering = new QCheckBox("Enable Numbering", grpNum);
    connect(m_chkNumbering, &QCheckBox::toggled, this, &BulkRenameDialog::onUpdatePreview);

    m_comboNumPos = new QComboBox(grpNum);
    m_comboNumPos->addItems({"Append (as Suffix)", "Prepend (as Prefix)"});
    connect(m_comboNumPos, &QComboBox::currentIndexChanged, this, &BulkRenameDialog::onUpdatePreview);

    m_spinNumStart = new QSpinBox(grpNum);
    m_spinNumStart->setRange(0, 999999);
    m_spinNumStart->setValue(1);
    connect(m_spinNumStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &BulkRenameDialog::onUpdatePreview);

    m_spinNumStep = new QSpinBox(grpNum);
    m_spinNumStep->setRange(1, 100);
    m_spinNumStep->setValue(1);
    connect(m_spinNumStep, QOverload<int>::of(&QSpinBox::valueChanged), this, &BulkRenameDialog::onUpdatePreview);

    m_spinNumPadding = new QSpinBox(grpNum);
    m_spinNumPadding->setRange(1, 10);
    m_spinNumPadding->setValue(2);
    m_spinNumPadding->setSuffix(" digits");
    connect(m_spinNumPadding, QOverload<int>::of(&QSpinBox::valueChanged), this, &BulkRenameDialog::onUpdatePreview);

    layoutNum->addWidget(m_chkNumbering, 0, 0, 1, 2);
    layoutNum->addWidget(new QLabel("Position:", grpNum), 1, 0);
    layoutNum->addWidget(m_comboNumPos, 1, 1);
    layoutNum->addWidget(new QLabel("Start Index:", grpNum), 2, 0);
    layoutNum->addWidget(m_spinNumStart, 2, 1);
    layoutNum->addWidget(new QLabel("Step size:", grpNum), 3, 0);
    layoutNum->addWidget(m_spinNumStep, 3, 1);
    layoutNum->addWidget(new QLabel("Padding size:", grpNum), 4, 0);
    layoutNum->addWidget(m_spinNumPadding, 4, 1);
    optLayout->addWidget(grpNum);

    // Save and Cancel buttons
    QHBoxLayout* actionsLayout = new QHBoxLayout();
    QPushButton* btnRename = new QPushButton("Apply Rename", optWidget);
    btnRename->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
    QPushButton* btnCancel = new QPushButton("Cancel", optWidget);
    
    connect(btnRename, &QPushButton::clicked, this, &BulkRenameDialog::onApplyRename);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    actionsLayout->addStretch(1);
    actionsLayout->addWidget(btnRename);
    actionsLayout->addWidget(btnCancel);
    optLayout->addLayout(actionsLayout);

    // ================= RIGHT PANEL: LIVE PREVIEW TABLE =================
    QWidget* previewWidget = new QWidget(this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewWidget);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(6);

    QLabel* lblPreview = new QLabel("Live Renaming Preview:", previewWidget);
    lblPreview->setStyleSheet("font-weight: bold; color: #89b4fa;");
    previewLayout->addWidget(lblPreview);

    m_tablePreview = new QTableWidget(0, 3, previewWidget);
    m_tablePreview->setHorizontalHeaderLabels({"Original Name", "New Name", "Folder Location"});
    m_tablePreview->setAlternatingRowColors(true);
    m_tablePreview->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tablePreview->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tablePreview->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tablePreview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tablePreview->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tablePreview->setStyleSheet("QTableWidget { background-color: #11111b; border: 1px solid #313244; border-radius: 6px; }");

    previewLayout->addWidget(m_tablePreview, 1);

    mainLayout->addWidget(optWidget);
    mainLayout->addWidget(previewWidget, 1);

    // Load table initial values
    m_tablePreview->setRowCount(m_filePaths.size());
    for (int i = 0; i < m_filePaths.size(); ++i) {
        QFileInfo info(m_filePaths[i]);
        m_tablePreview->setItem(i, 0, new QTableWidgetItem(info.fileName()));
        m_tablePreview->setItem(i, 1, new QTableWidgetItem(""));
        m_tablePreview->setItem(i, 2, new QTableWidgetItem(QDir::toNativeSeparators(info.absolutePath())));
        
        // Store absolute path as data in col 0
        m_tablePreview->item(i, 0)->setData(Qt::UserRole, m_filePaths[i]);
    }

    m_tablePreview->setColumnWidth(0, 200);
    m_tablePreview->setColumnWidth(1, 200);
}

QString BulkRenameDialog::toTitleCase(const QString& str) const {
    QStringList words = str.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i) {
        QString w = words[i];
        if (!w.isEmpty()) {
            words[i] = w.at(0).toUpper() + w.mid(1).toLower();
        }
    }
    return words.join(' ');
}

QString BulkRenameDialog::toSentenceCase(const QString& str) const {
    if (str.isEmpty()) return str;
    return str.at(0).toUpper() + str.mid(1).toLower();
}

QString BulkRenameDialog::computeNewName(const QString& originalName, int index) const {
    QFileInfo info(originalName);
    QString name;
    QString ext;

    // Check complete file name vs completename and base completeBaseName
    if (m_chkApplyToExt->isChecked()) {
        name = originalName;
        ext = "";
    } else {
        name = info.completeBaseName();
        ext = info.suffix();
        if (!ext.isEmpty()) ext = "." + ext;
    }

    // 1. Find and Replace
    QString findText = m_txtFind->text();
    QString replaceText = m_txtReplace->text();
    if (!findText.isEmpty()) {
        if (m_chkRegex->isChecked()) {
            QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
            if (!m_chkCaseSensitive->isChecked()) {
                options |= QRegularExpression::CaseInsensitiveOption;
            }
            QRegularExpression regex(findText, options);
            name.replace(regex, replaceText);
        } else {
            Qt::CaseSensitivity cs = m_chkCaseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
            name.replace(findText, replaceText, cs);
        }
    }

    // 2. Case Conversion
    int caseIdx = m_comboCase->currentIndex();
    if (caseIdx == 1) {
        name = name.toUpper();
    } else if (caseIdx == 2) {
        name = name.toLower();
    } else if (caseIdx == 3) {
        name = toTitleCase(name);
    } else if (caseIdx == 4) {
        name = toSentenceCase(name);
    }

    // 3. Index Numbering
    if (m_chkNumbering->isChecked()) {
        int val = m_spinNumStart->value() + index * m_spinNumStep->value();
        QString numStr = QString::number(val).rightJustified(m_spinNumPadding->value(), '0');
        if (m_comboNumPos->currentIndex() == 0) {
            name += numStr;
        } else {
            name = numStr + name;
        }
    }

    return name + ext;
}

void BulkRenameDialog::onUpdatePreview() {
    for (int i = 0; i < m_tablePreview->rowCount(); ++i) {
        QString originalName = m_tablePreview->item(i, 0)->text();
        QString newName = computeNewName(originalName, i);
        
        QTableWidgetItem* itemNewName = m_tablePreview->item(i, 1);
        itemNewName->setText(newName);
        
        // Visual indicator: show difference in color if name changes
        if (originalName != newName) {
            itemNewName->setBackground(QBrush(QColor("#1e3a1e"))); // soft green bg tint
            itemNewName->setForeground(QBrush(QColor("#a6e3a1"))); // Highlight green text
        } else {
            itemNewName->setBackground(QBrush(Qt::transparent));
            itemNewName->setForeground(QBrush(QColor("#cdd6f4"))); // Default light gray
        }
    }
}

void BulkRenameDialog::onApplyRename() {
    QStringList failures;
    int renamedCount = 0;

    for (int i = 0; i < m_tablePreview->rowCount(); ++i) {
        QString oldPath = m_tablePreview->item(i, 0)->data(Qt::UserRole).toString();
        QString newName = m_tablePreview->item(i, 1)->text();
        QFileInfo oldInfo(oldPath);
        QString newPath = oldInfo.dir().filePath(newName);

        if (oldPath != newPath) {
            if (QFile::exists(newPath)) {
                failures.append(QString("%1 -> Target already exists!").arg(oldInfo.fileName()));
                continue;
            }
            if (QFile::rename(oldPath, newPath)) {
                renamedCount++;
            } else {
                failures.append(oldInfo.fileName());
            }
        }
    }

    if (!failures.isEmpty()) {
        QMessageBox::warning(this, "Renaming Completed with Errors",
                             QString("Renamed %1 files successfully.\n\n"
                                     "The following files could not be renamed:\n%2")
                             .arg(renamedCount).arg(failures.join("\n")));
    } else {
        QMessageBox::information(this, "Renaming Succeeded", 
                                 QString("Successfully renamed all %1 files!").arg(renamedCount));
    }

    accept();
}

// ================= Presets Loading & Persistence =================

void BulkRenameDialog::loadPresetNames() {
    m_comboPresets->clear();
    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("bulk_rename_presets");
    QStringList presets = settings.childGroups();
    settings.endGroup();

    m_comboPresets->addItems(presets);
}

void BulkRenameDialog::onSavePreset() {
    bool ok;
    QString name = QInputDialog::getText(this, "Save Preset", 
                                         "Enter preset name:", QLineEdit::Normal, 
                                         "", &ok);
    if (ok && !name.isEmpty()) {
        QSettings settings("Amifiles", "Amifiles");
        QString prefix = QString("bulk_rename_presets/%1/").arg(name);

        settings.setValue(prefix + "case_index", m_comboCase->currentIndex());
        settings.setValue(prefix + "find_text", m_txtFind->text());
        settings.setValue(prefix + "replace_text", m_txtReplace->text());
        settings.setValue(prefix + "regex", m_chkRegex->isChecked());
        settings.setValue(prefix + "case_sensitive", m_chkCaseSensitive->isChecked());
        settings.setValue(prefix + "apply_to_ext", m_chkApplyToExt->isChecked());
        settings.setValue(prefix + "numbering", m_chkNumbering->isChecked());
        settings.setValue(prefix + "num_pos", m_comboNumPos->currentIndex());
        settings.setValue(prefix + "num_start", m_spinNumStart->value());
        settings.setValue(prefix + "num_step", m_spinNumStep->value());
        settings.setValue(prefix + "num_padding", m_spinNumPadding->value());

        loadPresetNames();
        m_comboPresets->setCurrentText(name);
    }
}

void BulkRenameDialog::onLoadPreset() {
    QString name = m_comboPresets->currentText();
    if (name.isEmpty()) return;

    QSettings settings("Amifiles", "Amifiles");
    QString prefix = QString("bulk_rename_presets/%1/").arg(name);

    m_comboCase->setCurrentIndex(settings.value(prefix + "case_index", 0).toInt());
    m_txtFind->setText(settings.value(prefix + "find_text", "").toString());
    m_txtReplace->setText(settings.value(prefix + "replace_text", "").toString());
    m_chkRegex->setChecked(settings.value(prefix + "regex", false).toBool());
    m_chkCaseSensitive->setChecked(settings.value(prefix + "case_sensitive", false).toBool());
    m_chkApplyToExt->setChecked(settings.value(prefix + "apply_to_ext", false).toBool());
    m_chkNumbering->setChecked(settings.value(prefix + "numbering", false).toBool());
    m_comboNumPos->setCurrentIndex(settings.value(prefix + "num_pos", 0).toInt());
    m_spinNumStart->setValue(settings.value(prefix + "num_start", 1).toInt());
    m_spinNumStep->setValue(settings.value(prefix + "num_step", 1).toInt());
    m_spinNumPadding->setValue(settings.value(prefix + "num_padding", 2).toInt());

    onUpdatePreview();
}

void BulkRenameDialog::onDeletePreset() {
    QString name = m_comboPresets->currentText();
    if (name.isEmpty()) return;

    auto btn = QMessageBox::question(this, "Delete Preset", 
                                     QString("Are you sure you want to delete the preset '%1'?").arg(name),
                                     QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes) {
        QSettings settings("Amifiles", "Amifiles");
        settings.remove(QString("bulk_rename_presets/%1").arg(name));
        loadPresetNames();
        onUpdatePreview();
    }
}
