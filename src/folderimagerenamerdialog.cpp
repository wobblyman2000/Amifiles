#include "folderimagerenamerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QDirIterator>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QApplication>

FolderImageRenamerDialog::FolderImageRenamerDialog(const QString& initialDir, QWidget* parent)
    : QDialog(parent), m_initialDir(initialDir)
{
    setupUI();
    
    // Set initial dir
    if (!m_initialDir.isEmpty() && QDir(m_initialDir).exists()) {
        m_editDir->setText(QDir::toNativeSeparators(m_initialDir));
    } else {
        m_editDir->setText(QDir::toNativeSeparators(QDir::homePath()));
    }
}

void FolderImageRenamerDialog::setupUI() {
    setWindowTitle("Recursive Image Renaming Tool");
    resize(780, 520);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; font-size: 12px; }"
                  "QGroupBox { font-weight: bold; border: 1px solid #313244; border-radius: 6px; margin-top: 10px; padding-top: 14px; color: #f5c2e7; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; }"
                  "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 5px; }"
                  "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
                  "QTableWidget { background-color: #181825; color: #cdd6f4; gridline-color: #313244; border: 1px solid #313244; border-radius: 4px; }"
                  "QTableWidget::item { padding: 4px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QCheckBox { color: #cdd6f4; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    // 1. Directory selector
    QHBoxLayout* dirLayout = new QHBoxLayout();
    QLabel* lblDir = new QLabel("Parent Directory:", this);
    m_editDir = new QLineEdit(this);
    m_btnBrowse = new QPushButton("Browse...", this);
    dirLayout->addWidget(lblDir);
    dirLayout->addWidget(m_editDir, 1);
    dirLayout->addWidget(m_btnBrowse);
    mainLayout->addLayout(dirLayout);

    // 2. Options layout (Match patterns & target name side-by-side)
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    
    // GroupBox: Source filenames
    QGroupBox* grpSource = new QGroupBox("Source Image Names to Match", this);
    QGridLayout* sourceGrid = new QGridLayout(grpSource);
    sourceGrid->setSpacing(6);

    m_chkFolder = new QCheckBox("folder (jpg/png)", this);
    m_chkDvd = new QCheckBox("dvd (jpg/png)", this);
    m_chkPoster = new QCheckBox("poster (jpg/png)", this);
    m_chkAlbum = new QCheckBox("album (jpg/png)", this);
    m_chkFront = new QCheckBox("front (jpg/png)", this);
    m_chkCd = new QCheckBox("cd (jpg/png)", this);
    m_chkMovie = new QCheckBox("movie (jpg/png)", this);
    m_chkCustom = new QCheckBox("Custom:", this);
    m_editCustomFind = new QLineEdit(this);
    m_editCustomFind->setPlaceholderText("e.g. cover.jpg, back.png");
    m_editCustomFind->setEnabled(false);

    m_chkFolder->setChecked(true);
    m_chkDvd->setChecked(true);
    m_chkPoster->setChecked(true);

    sourceGrid->addWidget(m_chkFolder, 0, 0);
    sourceGrid->addWidget(m_chkDvd, 0, 1);
    sourceGrid->addWidget(m_chkPoster, 1, 0);
    sourceGrid->addWidget(m_chkAlbum, 1, 1);
    sourceGrid->addWidget(m_chkFront, 2, 0);
    sourceGrid->addWidget(m_chkCd, 2, 1);
    sourceGrid->addWidget(m_chkMovie, 3, 0);
    sourceGrid->addWidget(m_chkCustom, 4, 0);
    sourceGrid->addWidget(m_editCustomFind, 4, 1);

    optionsLayout->addWidget(grpSource, 1);

    // GroupBox: Target filename
    QGroupBox* grpTarget = new QGroupBox("Target Image Name", this);
    QVBoxLayout* targetLayout = new QVBoxLayout(grpTarget);
    targetLayout->setSpacing(8);

    QLabel* lblTarget = new QLabel("Rename matched files to:", this);
    m_comboTargetName = new QComboBox(this);
    m_comboTargetName->addItem("poster.png");
    m_comboTargetName->addItem("dvd.png");
    m_comboTargetName->addItem("folder.jpg");
    m_comboTargetName->addItem("folder.png");
    m_comboTargetName->addItem("poster.jpg");
    m_comboTargetName->addItem("dvd.jpg");
    m_comboTargetName->addItem("album.jpg");
    m_comboTargetName->addItem("front.jpg");
    m_comboTargetName->addItem("cd.png");
    m_comboTargetName->addItem("movie.png");
    m_comboTargetName->addItem("Preserve original extension (e.g. poster.*)");
    m_comboTargetName->addItem("Preserve original extension (e.g. dvd.*)");
    m_comboTargetName->addItem("Preserve original extension (e.g. folder.*)");
    m_comboTargetName->addItem("Custom Filename...");

    m_editCustomTarget = new QLineEdit(this);
    m_editCustomTarget->setPlaceholderText("e.g. poster.png");
    m_editCustomTarget->setEnabled(false);

    targetLayout->addWidget(lblTarget);
    targetLayout->addWidget(m_comboTargetName);
    targetLayout->addWidget(m_editCustomTarget);
    targetLayout->addStretch(1);

    optionsLayout->addWidget(grpTarget, 1);
    mainLayout->addLayout(optionsLayout);

    // Scan bar
    m_btnScan = new QPushButton("🔍 Scan Parent Directory", this);
    m_btnScan->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; border: 1px solid #45475a; font-size: 13px; padding: 8px; }"
                            "QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    mainLayout->addWidget(m_btnScan);

    // Preview area
    m_tablePreview = new QTableWidget(this);
    m_tablePreview->setColumnCount(4);
    m_tablePreview->setHorizontalHeaderLabels({"Rename?", "Directory", "Original File", "Proposed Renamed File"});
    m_tablePreview->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tablePreview->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tablePreview->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tablePreview->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tablePreview->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_tablePreview, 1);

    // Status bar & select/deselect all
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_chkSelectAll = new QCheckBox("Select/Deselect All", this);
    m_chkSelectAll->setChecked(true);
    m_chkSelectAll->setEnabled(false);
    m_lblStatus = new QLabel("Ready to scan.", this);
    m_lblStatus->setStyleSheet("color: #a6adc8; font-style: italic;");
    statusLayout->addWidget(m_chkSelectAll);
    statusLayout->addWidget(m_lblStatus, 1, Qt::AlignRight);
    mainLayout->addLayout(statusLayout);

    // Dialog buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnApply = new QPushButton("Apply Rename", this);
    m_btnApply->setStyleSheet("QPushButton { background-color: #313244; color: #f9e2af; border: 1px solid #45475a; }"
                             "QPushButton:hover { background-color: #f9e2af; color: #11111b; }");
    m_btnApply->setEnabled(false);
    
    QPushButton* btnClose = new QPushButton("Close", this);
    
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_btnBrowse, &QPushButton::clicked, this, &FolderImageRenamerDialog::onBrowseDirectory);
    connect(m_btnScan, &QPushButton::clicked, this, &FolderImageRenamerDialog::onScan);
    connect(m_btnApply, &QPushButton::clicked, this, &FolderImageRenamerDialog::onApplyRename);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_chkSelectAll, &QCheckBox::toggled, this, &FolderImageRenamerDialog::onToggleSelectAll);
    
    connect(m_chkCustom, &QCheckBox::toggled, m_editCustomFind, &QLineEdit::setEnabled);
    connect(m_comboTargetName, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_editCustomTarget->setEnabled(m_comboTargetName->itemText(idx) == "Custom Filename...");
    });
}

void FolderImageRenamerDialog::onBrowseDirectory() {
    QString selected = QFileDialog::getExistingDirectory(this, "Select Parent Directory", m_editDir->text());
    if (!selected.isEmpty()) {
        m_editDir->setText(QDir::toNativeSeparators(selected));
    }
}

void FolderImageRenamerDialog::onScan() {
    QString path = m_editDir->text().trimmed();
    if (path.isEmpty() || !QDir(path).exists()) {
        QMessageBox::warning(this, "Invalid Path", "Please enter or browse to a valid parent directory.");
        return;
    }

    m_btnScan->setEnabled(false);
    m_lblStatus->setText("Scanning directory recursively...");
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Build target match list
    QStringList findNames;
    auto addExtensions = [&](const QString& base) {
        findNames.append(base + ".jpg");
        findNames.append(base + ".jpeg");
        findNames.append(base + ".png");
    };

    if (m_chkFolder->isChecked()) addExtensions("folder");
    if (m_chkDvd->isChecked()) addExtensions("dvd");
    if (m_chkPoster->isChecked()) addExtensions("poster");
    if (m_chkAlbum->isChecked()) addExtensions("album");
    if (m_chkFront->isChecked()) addExtensions("front");
    if (m_chkCd->isChecked()) addExtensions("cd");
    if (m_chkMovie->isChecked()) addExtensions("movie");

    if (m_chkCustom->isChecked()) {
        QStringList customParts = m_editCustomFind->text().split(',', Qt::SkipEmptyParts);
        for (QString p : customParts) {
            p = p.trimmed();
            if (!p.isEmpty()) findNames.append(p);
        }
    }

    // Determine target pattern
    QString targetNamePattern = m_comboTargetName->currentText();
    QString customTargetPattern = m_editCustomTarget->text().trimmed();

    m_tablePreview->setRowCount(0);
    
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    int matchCount = 0;

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        QString fileName = fi.fileName();
        
        bool matched = false;
        for (const QString& pattern : findNames) {
            if (fileName.compare(pattern, Qt::CaseInsensitive) == 0) {
                matched = true;
                break;
            }
        }

        if (matched) {
            // Determine the proposed target filename
            QString newName;
            if (targetNamePattern == "Custom Filename...") {
                newName = customTargetPattern;
                if (newName.isEmpty()) newName = fileName; // Fallback
            } else if (targetNamePattern.startsWith("Preserve original extension")) {
                // e.g. "Preserve original extension (e.g. poster.*)"
                QString basePart = targetNamePattern.split("e.g. ").last().split(".*").first();
                newName = basePart + "." + fi.suffix().toLower();
            } else {
                newName = targetNamePattern;
            }

            // Skip if the file already has the target name to avoid redundant renames
            if (fileName.compare(newName, Qt::CaseInsensitive) == 0) {
                continue;
            }

            int row = m_tablePreview->rowCount();
            m_tablePreview->insertRow(row);

            // Col 0: Checkbox
            QTableWidgetItem* checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Checked);
            m_tablePreview->setItem(row, 0, checkItem);

            // Col 1: Directory
            QTableWidgetItem* dirItem = new QTableWidgetItem(QDir::toNativeSeparators(fi.absolutePath()));
            dirItem->setFlags(dirItem->flags() & ~Qt::ItemIsEditable);
            m_tablePreview->setItem(row, 1, dirItem);

            // Col 2: Original Name
            QTableWidgetItem* origItem = new QTableWidgetItem(fileName);
            origItem->setFlags(origItem->flags() & ~Qt::ItemIsEditable);
            m_tablePreview->setItem(row, 2, origItem);

            // Col 3: Target Name
            QTableWidgetItem* targetItem = new QTableWidgetItem(newName);
            m_tablePreview->setItem(row, 3, targetItem);

            // Store full source path in column 0 user data
            checkItem->setData(Qt::UserRole, fi.absoluteFilePath());

            matchCount++;
        }
    }

    QApplication::restoreOverrideCursor();
    m_btnScan->setEnabled(true);
    m_chkSelectAll->setEnabled(matchCount > 0);
    m_btnApply->setEnabled(matchCount > 0);
    
    m_lblStatus->setText(QString("Found %1 renameable file(s).").arg(matchCount));
}

void FolderImageRenamerDialog::onApplyRename() {
    int totalChecked = 0;
    for (int i = 0; i < m_tablePreview->rowCount(); ++i) {
        QTableWidgetItem* item = m_tablePreview->item(i, 0);
        if (item && item->checkState() == Qt::Checked) {
            totalChecked++;
        }
    }

    if (totalChecked == 0) {
        QMessageBox::information(this, "Apply Rename", "No files are checked/selected for renaming.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Rename",
        QString("Are you sure you want to rename these %1 file(s) on disk? This cannot be easily undone.").arg(totalChecked),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    int successCount = 0;
    int failCount = 0;

    for (int i = 0; i < m_tablePreview->rowCount(); ++i) {
        QTableWidgetItem* checkItem = m_tablePreview->item(i, 0);
        if (!checkItem || checkItem->checkState() != Qt::Checked) continue;

        QString origPath = checkItem->data(Qt::UserRole).toString();
        QString targetDir = m_tablePreview->item(i, 1)->text();
        QString targetName = m_tablePreview->item(i, 3)->text().trimmed();

        if (targetName.isEmpty()) {
            failCount++;
            continue;
        }

        QString newPath = QDir(targetDir).filePath(targetName);

        // Check if destination already exists to prevent accidental overwrites
        if (QFile::exists(newPath)) {
            failCount++;
            continue;
        }

        if (QFile::rename(origPath, newPath)) {
            successCount++;
        } else {
            failCount++;
        }
    }

    QMessageBox::information(
        this,
        "Rename Results",
        QString("Successfully renamed %1 file(s).\nFailed/Skipped %2 file(s).").arg(successCount).arg(failCount)
    );

    // Rescan directory to update table view
    onScan();
}

void FolderImageRenamerDialog::onToggleSelectAll(bool checked) {
    m_tablePreview->blockSignals(true);
    for (int i = 0; i < m_tablePreview->rowCount(); ++i) {
        QTableWidgetItem* item = m_tablePreview->item(i, 0);
        if (item) {
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    }
    m_tablePreview->blockSignals(false);
}
