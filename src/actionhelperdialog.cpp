#include "actionhelperdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>

ActionHelperDialog::ActionHelperDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Internal Commands Helper");
    resize(600, 400);

    // Define all internal commands
    m_actions = {
        {"@internal:Copy", "Copy Files/Folders", "Copies selected files or directories to the sibling/opposite pane folder, or to the clipboard if only one pane is active."},
        {"@internal:Move", "Move Files/Folders", "Moves selected files or directories to the sibling/opposite pane folder."},
        {"@internal:Cut", "Cut selected Files/Folders", "Cuts selected items to the clipboard for moving."},
        {"@internal:Paste", "Paste from Clipboard", "Pastes files currently copied or cut on the clipboard into the active directory."},
        {"@internal:Delete", "Delete Files/Folders", "Sends selected items to the system trash or prompts to delete them permanently."},
        {"@internal:Rename", "Rename File/Folder", "Prompts a rename entry box for the currently highlighted file or folder."},
        {"@internal:NewFolder", "Create New Folder", "Prompts to create a new empty directory in the active file listing pane."},
        {"@internal:Refresh", "Refresh File Listing", "Reloads the active folder directory files list immediately."},
        {"@internal:ToggleDualPane", "Toggle Split View (Single/Dual)", "Switches the main window layout between a single folder panel and split side-by-side folder panels."},
        {"@internal:TogglePreview", "Toggle File Preview Panel", "Shows or hides the dockable, floatable file metadata and multimedia preview panel."},
        {"@internal:ToggleFlatView", "Toggle Flat Folder View (Recursive)", "Toggles the recursive flat view mode to list all subfolder contents in a single flattened table list."},
        {"@internal:CompareSync", "Compare & Sync Folders", "Launches the Folder Comparison and Synchronization utility between left and right directory panes."},
        {"@internal:DuplicateFinder", "Find Duplicate Files", "Opens the Duplicate Files Finder panel to locate identical file hashes in selected paths."},
        {"@internal:SpaceAnalyzer", "Visual Disk Space sunburst Analyzer", "Launches the sunburst disk usage visualization dashboard for the active folder."},
        {"@internal:BulkRename", "Bulk Rename Tool", "Launches the batch file and folder renaming utility."},
        {"@internal:SyncScheduler", "Sync Scheduler Settings", "Configure backups and automated file synchronization schedules."},
        {"@internal:ChecksumTool", "Calculate File Checksums", "Calculate and verify MD5, SHA-1, or SHA-256 cryptographic hashes."},
        {"@internal:CloudMount", "Mount Cloud Drive", "Opens the dialog to mount external cloud storage systems (Dropbox, Google Drive, OneDrive) as local folders."},
        {"@internal:Shred", "Secure File Shredder", "Permanently overwrites and destroys selected files/folders beyond recovery."},
        {"@internal:SmartHome", "Go to Landing Page Home Screen", "Navigates the active panel immediately to the dashboard Home Screen landing page."},
        {"@internal:LoadProfile <ProfileName>", "Load Folder Profile Layout", "Loads and applies the specified folder layout profile / template to the active file panel."},
        {"@internal:Go <path>", "Navigate to specific folder path", "Navigates the panel to a custom directory. Supports macro variables: ~ (home), {dir} (active directory), {dest} (sibling directory)."},
        {"@internal:ToggleWatched", "Toggle Watched Status", "Toggles the Watched/Unwatched metadata status overlay on the selected file or directory."},
        {"@internal:MediaInfoSheet", "Open Media Info Sheet", "Launches the movie/audio detail sheet showing genre, release date, runtime, and plot synopsis."},
        {"@internal:ScrapeVideoMetadata", "Scrape Video Metadata", "Runs background online scrapers to fetch movie or TV episode tags, actors, and cover art."},
        {"@internal:FetchAlbumMetadata", "Fetch Album Cover Art", "Scrapes MusicBrainz or local directories to fetch album tagging and art files."},
        {"@internal:ApplyDvdCasing", "Apply DVD Video Casing", "Wraps the cover art of the selected file or directory inside a high-fidelity DVD case overlay."},
        {"@internal:ApplyBluRayCasing", "Apply Blu-ray Casing", "Wraps the cover art of the selected file or directory inside a premium Blu-ray case overlay."},
        {"@internal:PlayCollection", "Play Folder Collection", "Triggers folder-wide media playback for the active directory or highlights."},
        {"@internal:LockFolder", "Lock Folder/File", "Recursively strips write permissions from selected directories and files to protect them against deletion or modification."},
        {"@internal:UnlockFolder", "Unlock Folder/File", "Recursively restores write permissions to selected directories and files to make them editable/deletable again."}
    };

    setupUI();
    populateList();
}

void ActionHelperDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Top search bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Search Command:", this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Filter internal commands by name or keyword...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ActionHelperDialog::filterActions);
    searchLayout->addWidget(m_searchEdit);
    mainLayout->addLayout(searchLayout);

    // Splitter look using layouts
    QHBoxLayout* centerLayout = new QHBoxLayout();
    
    // Left: Commands list
    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet("QListWidget { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }");
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &ActionHelperDialog::updateDetails);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &ActionHelperDialog::onInsertClicked);
    centerLayout->addWidget(m_listWidget, 3);

    // Right: Explanatory Details panel
    m_descBrowser = new QTextBrowser(this);
    m_descBrowser->setStyleSheet("QTextBrowser { background-color: #11111b; color: #a6adc8; border: 1px solid #313244; border-radius: 4px; padding: 6px; }");
    centerLayout->addWidget(m_descBrowser, 2);

    mainLayout->addLayout(centerLayout, 1);

    // Bottom Action Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_btnInsert = new QPushButton("Insert Action", this);
    m_btnInsert->setEnabled(false);
    m_btnInsert->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #94e2d5; }");
    connect(m_btnInsert, &QPushButton::clicked, this, &ActionHelperDialog::onInsertClicked);

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_btnInsert);
    buttonLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(buttonLayout);
}

void ActionHelperDialog::populateList() {
    m_listWidget->clear();
    for (int i = 0; i < m_actions.size(); ++i) {
        QListWidgetItem* item = new QListWidgetItem(m_actions[i].name, m_listWidget);
        item->setData(Qt::UserRole, i);
    }
}

void ActionHelperDialog::filterActions() {
    QString query = m_searchEdit->text().trimmed();
    m_listWidget->clear();

    for (int i = 0; i < m_actions.size(); ++i) {
        bool match = query.isEmpty() ||
                     m_actions[i].name.contains(query, Qt::CaseInsensitive) ||
                     m_actions[i].command.contains(query, Qt::CaseInsensitive) ||
                     m_actions[i].description.contains(query, Qt::CaseInsensitive);

        if (match) {
            QListWidgetItem* item = new QListWidgetItem(m_actions[i].name, m_listWidget);
            item->setData(Qt::UserRole, i);
        }
    }
}

void ActionHelperDialog::updateDetails() {
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        m_descBrowser->clear();
        m_btnInsert->setEnabled(false);
        return;
    }

    int idx = item->data(Qt::UserRole).toInt();
    const auto& act = m_actions[idx];

    QString details = QString("<h3>%1</h3>"
                              "<p><b>Syntax Keyword:</b><br/>"
                              "<font color='#f38ba8'><b>%2</b></font></p>"
                              "<p><b>Explanation:</b><br/>"
                              "%3</p>")
                      .arg(act.name)
                      .arg(act.command)
                      .arg(act.description);

    m_descBrowser->setHtml(details);
    m_btnInsert->setEnabled(true);
}

void ActionHelperDialog::onInsertClicked() {
    if (m_listWidget->currentItem()) {
        accept();
    }
}

QString ActionHelperDialog::selectedCommand() const {
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) return QString();
    
    int idx = item->data(Qt::UserRole).toInt();
    return m_actions[idx].command;
}
