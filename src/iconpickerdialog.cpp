#include "iconpickerdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QApplication>
#include <QStyle>
#include <QSettings>
#include <QProgressDialog>
#include <QTimer>
#include <QFileDialog>

// Define static members
QMap<QString, QSet<QString>> IconPickerDialog::s_iconCache;
bool IconPickerDialog::s_cacheLoaded = false;

IconPickerDialog::IconPickerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("System Icon Picker");
    resize(750, 500);
    setupUI();
    loadIcons();
    onCategoryChanged();
}

void IconPickerDialog::setupUI() {
    // Style matching Catppuccin dark theme
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; font-size: 10pt; }"
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QListWidget::item:hover { background-color: #313244; }"
        "QListWidget::item:selected { background-color: #45475a; color: #f5c2e7; font-weight: bold; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnSelect { background-color: #a6e3a1; color: #11111b; }"
        "QPushButton#btnSelect:hover { background-color: #b4befe; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Search Layout
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 Search icons by keyword (e.g. folder, edit, game)...");
    m_lblCount = new QLabel("Found: 0", this);
    m_lblCount->setStyleSheet("color: #a6adc8; font-size: 9pt;");
    
    searchLayout->addWidget(m_searchEdit, 4);
    searchLayout->addWidget(m_lblCount, 1);
    mainLayout->addLayout(searchLayout);

    // Center Splitting Layout
    QHBoxLayout* centerLayout = new QHBoxLayout();

    // Category List (Left)
    m_categoryList = new QListWidget(this);
    m_categoryList->setMaximumWidth(160);
    centerLayout->addWidget(m_categoryList, 1);

    // Icon Grid (Right)
    m_iconGrid = new QListWidget(this);
    m_iconGrid->setViewMode(QListView::IconMode);
    m_iconGrid->setResizeMode(QListView::Adjust);
    m_iconGrid->setGridSize(QSize(80, 80));
    m_iconGrid->setIconSize(QSize(32, 32));
    m_iconGrid->setWordWrap(true);
    m_iconGrid->setDragEnabled(false);
    centerLayout->addWidget(m_iconGrid, 4);

    mainLayout->addLayout(centerLayout);

    // Bottom Action Layout
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_lblStatus = new QLabel("Select an icon...", this);
    m_lblStatus->setStyleSheet("color: #f5c2e7; font-weight: bold; font-size: 10pt;");

    QPushButton* btnSelect = new QPushButton("Select Icon", this);
    btnSelect->setObjectName("btnSelect");
    QPushButton* btnBrowse = new QPushButton("Browse File...", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    bottomLayout->addWidget(m_lblStatus);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnSelect);
    bottomLayout->addWidget(btnBrowse);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);

    // Connects
    connect(m_searchEdit, &QLineEdit::textChanged, this, &IconPickerDialog::onSearchChanged);
    connect(m_categoryList, &QListWidget::itemSelectionChanged, this, &IconPickerDialog::onCategoryChanged);
    connect(m_iconGrid, &QListWidget::itemDoubleClicked, this, &IconPickerDialog::onIconDoubleClicked);
    connect(m_iconGrid, &QListWidget::itemSelectionChanged, this, &IconPickerDialog::onIconSelectionChanged);

    connect(btnSelect, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Custom Icon File", QDir::homePath(), "Images (*.png *.jpg *.svg *.ico)");
        if (!file.isEmpty()) {
            m_selectedIcon = file;
            accept();
        }
    });
}

void IconPickerDialog::loadIcons() {
    if (!s_cacheLoaded) {
        // Fast populate categories mapping
        QMap<QString, QSet<QString>> categories;
        
        // Scan standard paths
        QString activeTheme = QIcon::themeName();
        QStringList scanPaths;
        scanPaths << "/usr/share/icons/hicolor";
        scanPaths << "/usr/share/pixmaps";
        
        if (!activeTheme.isEmpty() && activeTheme != "hicolor") {
            scanPaths << "/usr/share/icons/" + activeTheme;
            scanPaths << QDir::homePath() + "/.local/share/icons/" + activeTheme;
            scanPaths << QDir::homePath() + "/.icons/" + activeTheme;
        }

        // Add standard fallback directories if present
        scanPaths << "/usr/share/icons/Adwaita";
        scanPaths << "/usr/share/icons/breeze";

        for (const QString& path : scanPaths) {
            QDir dir(path);
            if (!dir.exists()) continue;

            // Scan recursively using QDirIterator for speed
            QDirIterator it(path, QStringList() << "*.png" << "*.svg", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo info = it.fileInfo();
                QString name = info.baseName();
                if (name.isEmpty() || name.contains("@")) continue; // Skip size variants

                // Classify by standard categories
                QString pathLower = info.absoluteFilePath().toLower();
                QString catName = "Other";
                if (pathLower.contains("/actions/")) catName = "Actions";
                else if (pathLower.contains("/apps/") || pathLower.contains("/devices/")) catName = "Apps & Hardware";
                else if (pathLower.contains("/categories/")) catName = "Categories";
                else if (pathLower.contains("/mimetypes/")) catName = "File Types";
                else if (pathLower.contains("/places/")) catName = "Places & Folders";
                else if (pathLower.contains("/status/")) catName = "Status Emblems";

                categories[catName].insert(name);
                categories["All"].insert(name);
            }
        }

        s_iconCache = categories;
        s_cacheLoaded = true;
    }

    // Populate category list widget
    m_categoryList->clear();
    QStringList catKeys = s_iconCache.keys();
    // Ensure "All" is first
    catKeys.removeAll("All");
    catKeys.prepend("All");

    for (const QString& cat : catKeys) {
        if (s_iconCache[cat].isEmpty()) continue;
        
        QIcon catIcon;
        if (cat == "All") catIcon = QIcon::fromTheme("dialog-information");
        else if (cat == "Actions") catIcon = QIcon::fromTheme("edit-copy");
        else if (cat == "Apps & Hardware") catIcon = QIcon::fromTheme("applications-system");
        else if (cat == "Categories") catIcon = QIcon::fromTheme("preferences-other");
        else if (cat == "File Types") catIcon = QIcon::fromTheme("text-x-generic");
        else if (cat == "Places & Folders") catIcon = QIcon::fromTheme("folder");
        else if (cat == "Status Emblems") catIcon = QIcon::fromTheme("dialog-warning");
        else catIcon = QIcon::fromTheme("image-x-generic");

        new QListWidgetItem(catIcon, cat, m_categoryList);
    }

    if (m_categoryList->count() > 0) {
        m_categoryList->setCurrentRow(0);
    }
}

void IconPickerDialog::onCategoryChanged() {
    filterIcons();
}

void IconPickerDialog::onSearchChanged(const QString&) {
    filterIcons();
}

void IconPickerDialog::filterIcons() {
    m_iconGrid->clear();
    QListWidgetItem* catItem = m_categoryList->currentItem();
    if (!catItem) return;

    QString cat = catItem->text();
    QString searchText = m_searchEdit->text().trimmed();

    QSet<QString> rawSet = s_iconCache.value(cat);
    
    // Sort names alphabetically
    QStringList sortedNames = QStringList(rawSet.begin(), rawSet.end());
    sortedNames.sort(Qt::CaseInsensitive);

    int matchingCount = 0;
    int displayCount = 0;

    for (const QString& name : sortedNames) {
        if (!searchText.isEmpty() && !name.contains(searchText, Qt::CaseInsensitive)) {
            continue;
        }

        matchingCount++;

        // Cap grid items to 400 to prevent lags
        if (displayCount < 400) {
            QIcon itemIcon = QIcon::fromTheme(name);
            if (!itemIcon.isNull()) {
                QListWidgetItem* gridItem = new QListWidgetItem(itemIcon, name, m_iconGrid);
                gridItem->setSizeHint(QSize(75, 75));
                gridItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
                gridItem->setToolTip(name);
                displayCount++;
            }
        }
    }

    if (matchingCount > displayCount) {
        m_lblCount->setText(QString("Showing %1 / %2").arg(displayCount).arg(matchingCount));
    } else {
        m_lblCount->setText(QString("Found: %1").arg(matchingCount));
    }
}

void IconPickerDialog::onIconSelectionChanged() {
    QListWidgetItem* item = m_iconGrid->currentItem();
    if (item) {
        m_selectedIcon = item->text();
        m_lblStatus->setText(QString("Selected: %1").arg(m_selectedIcon));
    } else {
        m_selectedIcon.clear();
        m_lblStatus->setText("Select an icon...");
    }
}

void IconPickerDialog::onIconDoubleClicked(QListWidgetItem* item) {
    if (item) {
        m_selectedIcon = item->text();
        accept();
    }
}

QString IconPickerDialog::selectedIconName() const {
    return m_selectedIcon;
}
