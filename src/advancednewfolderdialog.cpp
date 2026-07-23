#include "advancednewfolderdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDir>
#include <QApplication>

AdvancedNewFolderDialog::AdvancedNewFolderDialog(const QString& basePath, QWidget* parent)
    : QDialog(parent), m_basePath(basePath) {
    setWindowTitle("Advanced New Folder Builder");
    resize(480, 420);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QLineEdit, QPlainTextEdit { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; font-family: monospace; }"
                  "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; }"
                  "QPushButton:hover { background-color: #45475a; }");

    setupUI();
}

void AdvancedNewFolderDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    m_lblPathPreview = new QLabel(this);
    m_lblPathPreview->setStyleSheet("font-weight: bold; color: #89b4fa;");
    m_lblPathPreview->setWordWrap(true);
    m_lblPathPreview->setText("Creating folders in:\n" + QDir::toNativeSeparators(m_basePath));
    mainLayout->addWidget(m_lblPathPreview);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);

    m_comboTemplates = new QComboBox(this);
    m_comboTemplates->addItems({
        "Custom (Enter manually)",
        "A to Z Folders (A, B, C...)",
        "Months of the Year (January-December)",
        "Months of the Year (01-Jan to 12-Dec)",
        "Numbered Folders (01 to 10)",
        "Numbered Folders (01 to 100)",
        "Days of the Week (Monday-Sunday)"
    });
    form->addRow("Choose Template:", m_comboTemplates);
    mainLayout->addLayout(form);

    mainLayout->addWidget(new QLabel("Enter Folder Names (one per line):", this));

    m_txtFolders = new QPlainTextEdit(this);
    m_txtFolders->setPlaceholderText("Enter folder names...\nExample:\nFolder A\nFolder B\nFolder C");
    mainLayout->addWidget(m_txtFolders, 1);

    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch();

    QPushButton* btnCreate = new QPushButton("⚡ Create All", this);
    btnCreate->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }"
                             "QPushButton:hover { background-color: #b4befe; }");
    connect(btnCreate, &QPushButton::clicked, this, &AdvancedNewFolderDialog::onCreateClicked);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    buttonsLayout->addWidget(btnCreate);
    buttonsLayout->addWidget(btnCancel);
    mainLayout->addLayout(buttonsLayout);

    connect(m_comboTemplates, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvancedNewFolderDialog::onTemplateChanged);
}

void AdvancedNewFolderDialog::onTemplateChanged(int index) {
    QStringList items;
    switch (index) {
        case 1: // A to Z
            for (char c = 'A'; c <= 'Z'; ++c) {
                items.append(QString(c));
            }
            break;
        case 2: // Months (names)
            items = {"January", "February", "March", "April", "May", "June",
                     "July", "August", "September", "October", "November", "December"};
            break;
        case 3: // Months (numbered)
            items = {"01-Jan", "02-Feb", "03-Mar", "04-Apr", "05-May", "06-Jun",
                     "07-Jul", "08-Aug", "09-Sep", "10-Oct", "11-Nov", "12-Dec"};
            break;
        case 4: // Numbered 1-10
            for (int i = 1; i <= 10; ++i) {
                items.append(QString("%1").arg(i, 2, 10, QChar('0')));
            }
            break;
        case 5: // Numbered 1-100
            for (int i = 1; i <= 100; ++i) {
                items.append(QString("%1").arg(i, 2, 10, QChar('0')));
            }
            break;
        case 6: // Days of week
            items = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
            break;
        default:
            return;
    }
    m_txtFolders->setPlainText(items.join("\n"));
}

void AdvancedNewFolderDialog::onCreateClicked() {
    QString text = m_txtFolders->toPlainText();
    QStringList lines = text.split('\n');
    m_foldersToCreate.clear();

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            m_foldersToCreate.append(trimmed);
        }
    }

    if (m_foldersToCreate.isEmpty()) {
        QMessageBox::warning(this, "Empty List", "Please enter at least one folder name.");
        return;
    }

    QDir dir(m_basePath);
    if (!dir.exists()) {
        QMessageBox::critical(this, "Error", "Target directory does not exist.");
        return;
    }

    int createdCount = 0;
    int failedCount = 0;
    QStringList failures;

    for (const QString& folder : m_foldersToCreate) {
        if (dir.mkdir(folder)) {
            createdCount++;
        } else {
            failedCount++;
            failures.append(folder);
        }
    }

    if (failedCount > 0) {
        QMessageBox::warning(this, "Batch Result",
                             QString("Successfully created %1 folders.\nFailed to create %2 folders:\n%3")
                             .arg(createdCount)
                             .arg(failedCount)
                             .arg(failures.join(", ")));
    } else {
        QMessageBox::information(this, "Batch Result",
                                 QString("Successfully created all %1 folders!").arg(createdCount));
    }

    accept();
}

QStringList AdvancedNewFolderDialog::folderNames() const {
    return m_foldersToCreate;
}
