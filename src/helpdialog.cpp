#include "helpdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

HelpDialog::HelpDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Amifiles User Manual & Guide");
    resize(860, 580);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QListWidget { background-color: #181825; border: 1px solid #313244; color: #cdd6f4; border-radius: 4px; padding: 4px; }"
                  "QListWidget::item { padding: 8px 12px; border-radius: 4px; color: #a6adc8; }"
                  "QListWidget::item:hover { background-color: #313244; color: #cdd6f4; }"
                  "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
                  "QTextBrowser { background-color: #1e1e2e; border: 1px solid #313244; border-radius: 4px; color: #cdd6f4; padding: 10px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; }"
                  "QPushButton:hover { background-color: #45475a; }");
    setupUI();
}

void HelpDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Title Header
    QLabel* header = new QLabel("<b>Amifiles Help Center</b>", this);
    header->setStyleSheet("font-size: 16px; color: #89b4fa;");
    mainLayout->addWidget(header);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(10);

    // Sidebar navigation
    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(220);
    m_sidebar->addItems({
        "1. Welcome & Overview",
        "2. Layout & Tabs",
        "3. File Transfer Queue",
        "4. Advanced Search",
        "5. Media Preview & Covers",
        "6. Custom Script Buttons",
        "7. Disk Utilities",
        "8. Keyboard Shortcuts"
    });
    contentLayout->addWidget(m_sidebar);

    // Search and Browser Layout
    QVBoxLayout* browserLayout = new QVBoxLayout();
    
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search documentation (press Enter to Find Next)...");
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; font-size: 12px; }"
    );
    
    m_btnFindPrev = new QPushButton("Prev", this);
    m_btnFindPrev->setFixedWidth(60);
    m_btnFindNext = new QPushButton("Next", this);
    m_btnFindNext->setFixedWidth(60);
    
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_btnFindPrev);
    searchLayout->addWidget(m_btnFindNext);
    
    browserLayout->addLayout(searchLayout);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setSearchPaths({":"});
    browserLayout->addWidget(m_browser, 1);
    
    contentLayout->addLayout(browserLayout, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &HelpDialog::onSearchTextChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &HelpDialog::onFindNext);
    connect(m_btnFindPrev, &QPushButton::clicked, this, &HelpDialog::onFindPrev);
    connect(m_btnFindNext, &QPushButton::clicked, this, &HelpDialog::onFindNext);

    mainLayout->addLayout(contentLayout, 1);

    // Close button
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    QPushButton* btnClose = new QPushButton("Close Manual", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, &HelpDialog::onSectionChanged);
    m_sidebar->setCurrentRow(0); // Trigger initial load
}

void HelpDialog::onSectionChanged(int index) {
    QString html = "<style>"
                   "body { font-family: sans-serif; font-size: 13px; line-height: 1.5; color: #cdd6f4; background-color: #1e1e2e; }"
                   "h2 { color: #89b4fa; margin-top: 0; font-size: 16px; border-bottom: 1px solid #313244; padding-bottom: 4px; }"
                   "h3 { color: #f9e2af; font-size: 14px; margin-top: 15px; margin-bottom: 5px; }"
                   "ul, ol { margin-left: 15px; padding-left: 5px; }"
                   "li { margin-bottom: 4px; }"
                   "code { font-family: monospace; background-color: #11111b; color: #f38ba8; padding: 2px 4px; border-radius: 3px; font-size: 12px; }"
                   "pre { font-family: monospace; background-color: #11111b; border: 1px solid #313244; padding: 8px; border-radius: 4px; color: #a6e3a1; font-size: 11px; }"
                   "b { color: #f9e2af; }"
                   "a { color: #89b4fa; text-decoration: none; }"
                   "table { width: 100%; border-collapse: collapse; margin-top: 10px; }"
                   "th { background-color: #313244; color: #cdd6f4; text-align: left; padding: 6px; font-size: 12px; border: 1px solid #45475a; }"
                   "td { padding: 6px; border: 1px solid #313244; font-size: 12px; }"
                   "tr:nth-child(even) { background-color: #181825; }"
                   "</style><body>";

    switch (index) {
        case 0: // Welcome & Overview
            html += "<h2>1. Welcome &amp; Overview</h2>"
                    "<p><b>Amifiles</b> is a highly-optimized dual-pane file manager for Linux, designed for speed, flexibility, and extensibility. Inspired by classic managers like Directory Opus and Amiga DirOpus, it provides advanced keyboard-driven workflows coupled with rich modern media tools.</p>"
                    "<h3>Key Architectures</h3>"
                    "<ul>"
                    "<li><b>Dual-Pane File Trees:</b> Move and manage files rapidly side-by-side.</li>"
                    "<li><b>Virtual Filesystem (VFS):</b> Seamlessly step inside archives (ZIP, TAR, RAR, 7z) and retro/PC disk images (ADF, D64, ISO, IMG) and work on files without manual unpacking.</li>"
                    "<li><b>Pausable Copy Queue:</b> Safe multi-threaded file copying that handles collisions and preserves system responsiveness.</li>"
                    "<li><b>Custom Shell/Native Scripting:</b> Define custom buttons to run terminal commands, pass active directories/files, or execute internal functions.</li>"
                    "</ul>"
                    "<p style=\"text-align: center;\"><svg width=\"320\" height=\"160\" viewBox=\"0 0 320 160\" style=\"background:#1e1e2e; border:1px solid #45475a; border-radius:6px;\">"
                    "  <rect x=\"10\" y=\"10\" width=\"100\" height=\"110\" rx=\"3\" fill=\"#181825\" stroke=\"#313244\" stroke-width=\"1\"/>"
                    "  <text x=\"15\" y=\"25\" fill=\"#89b4fa\" font-size=\"8\" font-family=\"sans-serif\" font-weight=\"bold\">Source Panel</text>"
                    "  <line x1=\"15\" y1=\"35\" x2=\"105\" y2=\"35\" stroke=\"#313244\"/>"
                    "  <rect x=\"15\" y=\"42\" width=\"90\" height=\"6\" rx=\"1\" fill=\"#313244\"/>"
                    "  <rect x=\"15\" y=\"52\" width=\"90\" height=\"6\" rx=\"1\" fill=\"#45475a\"/>"
                    "  <circle cx=\"20\" cy=\"55\" r=\"2\" fill=\"#f38ba8\"/>"
                    "  <rect x=\"15\" y=\"62\" width=\"90\" height=\"6\" rx=\"1\" fill=\"#313244\"/>"
                    "  <rect x=\"115\" y=\"10\" width=\"100\" height=\"110\" rx=\"3\" fill=\"#181825\" stroke=\"#313244\" stroke-width=\"1\"/>"
                    "  <text x=\"120\" y=\"25\" fill=\"#a6adc8\" font-size=\"8\" font-family=\"sans-serif\" font-weight=\"bold\">Destination Panel</text>"
                    "  <line x1=\"120\" y1=\"35\" x2=\"210\" y2=\"35\" stroke=\"#313244\"/>"
                    "  <rect x=\"120\" y=\"42\" width=\"90\" height=\"6\" rx=\"1\" fill=\"#313244\"/>"
                    "  <rect x=\"220\" y=\"10\" width=\"90\" height=\"110\" rx=\"3\" fill=\"#11111b\" stroke=\"#313244\"/>"
                    "  <text x=\"225\" y=\"25\" fill=\"#f9e2af\" font-size=\"8\" font-family=\"sans-serif\" font-weight=\"bold\">Preview Panel</text>"
                    "  <rect x=\"235\" y=\"35\" width=\"60\" height=\"50\" rx=\"4\" fill=\"#313244\"/>"
                    "  <text x=\"245\" y=\"60\" fill=\"#a6e3a1\" font-size=\"7\" font-family=\"sans-serif\">🎵 CD Case</text>"
                    "  <rect x=\"10\" y=\"125\" width=\"300\" height=\"25\" rx=\"3\" fill=\"#11111b\" stroke=\"#313244\"/>"
                    "  <text x=\"15\" y=\"140\" fill=\"#a6e3a1\" font-size=\"8\" font-family=\"monospace\">$ ls -la ~/Documents</text>"
                    "</svg></p>"
                    "<h3>User Guide Index &amp; Sitemap</h3>"
                    "<table>"
                    "<tr><th>Section</th><th>Key Topics Covered</th></tr>"
                    "<tr><td><b>1. Welcome &amp; Overview</b></td><td>System architecture, sitemap, third-party dependencies (VICE, amitools, poppler, rclone), and SVG graphics integration.</td></tr>"
                    "<tr><td><b>2. Layout &amp; Tabs</b></td><td>Tab management, horizontal/vertical split orientation, view styles (details, cards, miller, timeline), and flat recursion mode.</td></tr>"
                    "<tr><td><b>3. File Transfer Queue</b></td><td>Pausable background file copy/move threads, transfer queues, and collision resolution sheets.</td></tr>"
                    "<tr><td><b>4. Advanced Search</b></td><td>Regex search filters, file size/date thresholds, tags matching, and search history presets.</td></tr>"
                    "<tr><td><b>5. Media Preview &amp; Covers</b></td><td>CD/DVD glassmorphic casing overlays, audio cover art extraction, real-time spectrum visualizer, hex editor, and PDF viewer.</td></tr>"
                    "<tr><td><b>6. Custom Script Buttons</b></td><td>Interactive macros, custom commands, terminal variables ($f, $d, $p), and execution shell logs.</td></tr>"
                    "<tr><td><b>7. Disk Utilities</b></td><td>Secure file shredding, checksum hash calculators, ADF/D64 disk modification tools, and FTP/SFTP/Cloud mounts.</td></tr>"
                    "<tr><td><b>8. Keyboard Shortcuts</b></td><td>Dynamic keybindings mapper and default system-wide hotkeys (F1-F6, Ctrl+D, Ctrl+P, Ctrl+F).</td></tr>"
                    "</table>"
                    "<h3>Interactive Graphic Diagrams (Image Support)</h3>"
                    "<p>Amifiles Help Center natively supports scalable vector graphic illustrations (SVG). These images are rendered directly on the manual pages to depict system components, directories hierarchy reconstruction pipelines, and space treemaps. You can see these drawings on several sections below!</p>"
                    "<h3>System Dependencies &amp; Third-Party Programs</h3>"
                    "<p>To power some of its advanced virtual filesystem and preview capabilities, Amifiles leverages standard open-source Linux CLI tools. Please ensure the following packages are installed on your host system:</p>"
                    "<ul>"
                    "<li><b>Archive Engines:</b> <code>zip</code>, <code>unzip</code> (for standard ZIP files), and <code>tar</code> (for tarballs).</li>"
                    "<li><b>Extended Archives &amp; ISOs:</b> <code>p7zip-full</code> / <code>7z</code> (for .7z, RAR, ISO, and PC disk images).</li>"
                    "<li><b>Commodore 64 Disk Images (.d64):</b> <code>c1541</code> (part of the <b>VICE</b> emulator suite).</li>"
                    "<li><b>Amiga Disk Files (.adf):</b> <code>xdftool</code> (part of the Python-based <b>amitools</b> library).</li>"
                    "<li><b>PDF Page Viewer:</b> <code>pdftoppm</code> (part of the <b>poppler-utils</b> suite).</li>"
                    "<li><b>Document Text Extractor:</b> <code>pdftotext</code> (part of the <b>poppler-utils</b> suite).</li>"
                    "<li><b>Remote &amp; Cloud VFS Mounts:</b> <code>rclone</code> (for FTP, SFTP, Samba, and Google Drive mounts).</li>"
                    "</ul>";
            break;

        case 1: // Layout & Tabs
            html += "<h2>2. Layout, Splits, &amp; Tabs</h2>"
                    "<p>Amifiles splits screen real estate efficiently into left and right pane panels, a bottom command output console, and an on-demand right preview panel.</p>"
                    "<h3>Tabbed Folder Browsing</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li><b>Open a new Tab:</b> Click the <code>+</code> button on the tab bar or press <code>Ctrl+T</code>. This duplicates your active folder location into a new tab on the current side.</li>"
                    "<li><b>Switch between Tabs:</b> Click the tab header or use keyboard shortcuts (<code>Alt+Left/Right</code> or <code>Ctrl+PageUp/PageDown</code>).</li>"
                    "<li><b>Close a Tab:</b> Click the <code>x</code> icon on the tab header or press <code>Ctrl+W</code>. The application enforces a minimum of one tab per panel.</li>"
                    "<li><b>Reorder Tabs:</b> Drag and drop tab headers horizontally along the tab bar to rearrange their positions.</li>"
                    "<li><b>Pin &amp; Lock Tab:</b> Right-click any tab header to toggle locked states:</li>"
                    "<ul>"
                    "<li><b>Pin Tab:</b> Retains the tab on the tab bar and blocks closing it.</li>"
                    "<li><b>Lock Path:</b> Restricts the tab to the current folder. Navigating into folders will launch them in a new tab rather than replacing the current view.</li>"
                    "<li><b>Lock Path with Subdirs:</b> Confines navigation to within the current folder's descendants only.</li>"
                    "</ul>"
                    "</ol>"
                    "<h3>Favorites &amp; Bookmarks</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ul>"
                    "<li><b>Add Current Directory:</b> Select <b>Favorites -> Add Current Directory to Favorites</b> to add your active path to the bookmarks list.</li>"
                    "<li><b>Remove Favorite:</b> Right-click any entry inside the <b>Favorites</b> menu directly to remove it (or select from the <i>Remove from Favorites...</i> submenu).</li>"
                    "</ul>"
                    "<h3>Synchronized Scrolling</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ul>"
                    "<li>Select <b>View -> Synchronize Scrolling</b> (or press <code>Ctrl+Shift+S</code>) to link active panels. Scrolling either file list scrolls the opposite sibling panel in parallel, enabling rapid comparisons between folder contents.</li>"
                    "</ul>"
                    "<h3>Dual Pane Split Orientation</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ul>"
                    "<li><b>Toggle Pane:</b> Press <code>Ctrl+D</code> or click the <b>Dual Pane View</b> button in the toolbar to hide/show the right panel.</li>"
                    "<li><b>Change Split Direction:</b> Click the drop-down arrow next to the <b>Dual Pane View</b> toolbar button (or select <b>View -> Split Panels Horizontally</b>):</li>"
                    "<ul>"
                    "<li><b>Vertical Split (Side-by-side):</b> Layout is split horizontally (traditional side-by-side view).</li>"
                    "<li><b>Horizontal Split (Top/Bottom):</b> Stacks folder displays vertically: Right panel is placed on the Top, and Left panel on the Bottom. This is highly useful for copying files vertically or working on small displays.</li>"
                    "</ul>"
                    "</ul>"
                    "<h3>Interactive View Modes</h3>"
                    "<p>Select one of the seven views from the toolbar dropdown to dynamically re-render directories:</p>"
                    "<ul>"
                    "<li><b>Details Table:</b> Traditional file list. Drag column headers to resize them, or double-click to auto-fit. Custom column widths are automatically saved and preserved on directory changes. Right-click column headers (or select <b>View -> Auto-Size All Columns</b>, shortcut <code>Ctrl+Shift+A</code>) to auto-size columns to fit their contents. Sort indicator column and order (sorted by Name alphabetically by default) are automatically remembered and restored. Integrates EXIF metadata columns (Camera Model, Exposure Time, ISO) that automatically parse and populate for image directories.</li>"
                    "<li><b>Grid / Icons:</b> Icon-based grid layout. Double-click icons to run files, or use arrow keys to navigate. Ideal for image or video folders.</li>"
                    "<li><b>Card / Tiles:</b> Displays medium-sized thumbnails with file name, format, size, and custom tags listed directly inside the card.</li>"
                    "<li><b>Miller Columns:</b> Finder-style cascading directory hierarchy. Select a directory to slide open its contents in a column to the right. Highlighting a file opens a quick file details card in the rightmost column.</li>"
                    "<li><b>Chronological Timeline:</b> Sorts files by modified date into collapsible groups (Today, Yesterday, Last 7 Days, Older). Click group headers to expand or collapse items.</li>"
                    "<li><b>Filmstrip View:</b> Splits the file panel into a large media preview screen at the top and a horizontal scrolling thumbnail reel at the bottom. Use the mouse wheel or arrow keys to scroll.</li>"
                    "<li><b>Theater View:</b> Optimized for media collections. Renders files and folders as large vertical cards displaying DVD/Blu-ray/CD case sleeve overlays. Generates cinematic slate poster mockups for items without cover art. Automatically renders standard fanart/backdrops (e.g. <code>backdrop.jpg</code>, <code>fanart.png</code>) as blurred ambient backgrounds behind the grid. Supports the following custom configurations:</li>"
                    "<ul>"
                    "<li><b>Clean Interface (Zen Mode):</b> Right-click and check <i>Clean Interface Mode (Zen Mode)</i> or press <code>F11</code> to hide all toolbars (including file operations and custom command bars), menus, consoles, search bars, status bars, and technical tabs in the preview panel (hiding Hex Viewer, Properties, Equalizer, and Document Text tabs, leaving only the media player and Playlist Queue visible). Right-click or press <code>F11</code> again to toggle back. Supports Backspace and Alt+Left/Right navigation.</li>"
                    "<li><b>Built-in Fullscreen Playback:</b> Right-click and check <i>Double-click Plays Media in Built-in Fullscreen Player</i> to play media built-in and go fullscreen. If a folder is played, all media inside (including nested CD1/CD2/CD3 or compilation subdirectories) are resolved and queued in alphabetical/track order.</li>"
                    "<li><b>Dedicated Context Menu & Tagging:</b> Strips out unrelated file actions (like copy/paste) when in Theater View. Overhauls tagging and metadata fetching so you can select and tag parent album folders directly; all audio files in subdirectories are recursively resolved and loaded in the batch tag editor.</li>"
                    "</ul>"
                    "</ul>"
                    "<h3>Flat View (Recursion Mode)</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Press <code>Ctrl+F</code>, select <b>View -> Flat View</b>, or click the state-aware icon-only <b>Flat View</b> button in the toolbar (folder/list icon toggle).</li>"
                    "<li>The directory tree is scanned and flattened into a single list view.</li>"
                    "<li>This allows you to view, sort, and batch-rename files located across multiple subdirectories simultaneously without manually traversing folders.</li>"
                    "</ol>"
                    "<h3>File Tags, Annotations &amp; Custom Columns</h3>"
                    "<ul>"
                    "<li>Right-click files/folders and select <b>File Tags...</b> to add persistent ratings, comments, custom text tags, and color badges.</li>"
                    "<li>Right-click details headers and select <b>Customize Columns...</b> to open the Columns Customizer. You can add, remove, rename, and reorder columns, selecting sources from Built-in metadata, ID3/EXIF tags, sidecar annotations, custom text keys (e.g. <i>hot dogs</i>), or embedded album artwork.</li>"
                    "</ul>"
                    "<h3>Dynamic File Grouping</h3>"
                    "<ul>"
                    "<li>Use the **Group by** combobox selector in the status bar to group your files instantly by Artist, Album, Genre, File Extension Type, Rating, or custom attributes.</li>"
                    "<li>Group headers display the group name and total matching item counts, rendering as collapsible/expandable nodes.</li>"
                    "</ul>"
                    "<h3>Smart Collections &amp; Virtual Folders</h3>"
                    "<ul>"
                    "<li>Select <b>Tools -> Create Smart Collection...</b> to define a virtual search folder. You can configure folders to scan, name wildcards, size limits, and age ranges.</li>"
                    "<li>Access saved smart collections in the Bookmarks sidebar under **Smart Virtual Folders** to browse matching files dynamically.</li>"
                    "</ul>"
                    "<h3>Drives Toolbar Custom Shortcuts &amp; Icons</h3>"
                    "<ul>"
                    "<li><b>Drag &amp; Drop Shortcuts:</b> Drag any folder from either list view and drop it directly onto the <b>Drives Toolbar</b>. It automatically registers a persistent navigation shortcut button.</li>"
                    "<li><b>Remove Shortcut:</b> Right-click any custom shortcut button on the Drives Toolbar and select **Remove Shortcut** to delete it.</li>"
                    "<li><b>Built-in Thematic Icons:</b> Directories named with common keywords (e.g. <i>Games</i>, <i>Code</i>, <i>Programming</i>, <i>Dev</i>, <i>Music</i>, <i>Videos</i>, <i>Pictures</i>, <i>Documents</i>, and <i>Downloads</i>) automatically render with distinct, modern category icons in both the Drives Toolbar and the main file panels.</li>"
                    "</ul>";
            break;

        case 2: // File Transfer Queue
            html += "<h2>3. File Transfer Queue &amp; Collisions</h2>"
                    "<p>Amifiles runs file transfer operations (Copy and Move) in a dedicated background worker thread to keep the interface completely responsive during large tasks.</p>"
                    "<h3>Transfer Dashboard Controls</h3>"
                    "<p>When copying (<code>F5</code>) or moving (<code>F6</code>) files, the progress drawer opens at the bottom:</p>"
                    "<ul>"
                    "<li><b>Speed Chart Widget:</b> A rolling 60-second line chart plotting copy speeds in real-time.</li>"
                    "<li><b>Pause / Resume:</b> Click <b>Pause</b> to halt the active worker thread instantly (releasing CPU and disk write locks for other applications). Click <b>Resume</b> to pick up where it left off.</li>"
                    "<li><b>Skip:</b> Click <b>Skip File</b> to bypass the active file transfer step and move to the next item in the queue.</li>"
                    "<li><b>Cancel:</b> Abort the remaining items in the transfer queue.</li>"
                    "</ul>"
                    "<h3>Duplicate Conflict Resolver</h3>"
                    "<p>If a file with the same name exists at the destination, an interactive grid comparison dialog displays modified dates, sizes, and previews of both files, offering six resolution modes:</p>"
                    "<ul>"
                    "<li><b>Overwrite:</b> Overwrite the destination file.</li>"
                    "<li><b>Overwrite All:</b> Automatically overwrite all future conflicts in the active queue.</li>"
                    "<li><b>Skip:</b> Skip copying this file, keeping the destination intact.</li>"
                    "<li><b>Skip All:</b> Automatically skip all future conflicts in the active queue.</li>"
                    "<li><b>Keep Both (Auto-Rename):</b> Copies the file and automatically appends an integer suffix (e.g., <code>document (1).txt</code>) to avoid name collisions.</li>"
                    "<li><b>Cancel:</b> Terminate the active copy/move queue immediately.</li>"
                    "</ul>"
                    "<p style=\"text-align: center;\"><svg width=\"320\" height=\"100\" viewBox=\"0 0 320 100\" style=\"background:#1e1e2e; border:1px solid #45475a; border-radius:6px;\">"
                    "  <text x=\"15\" y=\"20\" fill=\"#a6e3a1\" font-size=\"8\" font-family=\"sans-serif\" font-weight=\"bold\">Transfer Speed Dashboard (MB/s)</text>"
                    "  <path d=\"M 20,80 L 50,70 L 80,40 L 110,65 L 140,30 L 170,25 L 200,50 L 230,20 L 260,15 L 300,45\" fill=\"none\" stroke=\"#a6e3a1\" stroke-width=\"2\"/>"
                    "  <line x1=\"20\" y1=\"80\" x2=\"300\" y2=\"80\" stroke=\"#313244\"/>"
                    "  <line x1=\"20\" y1=\"20\" x2=\"20\" y2=\"80\" stroke=\"#313244\"/>"
                    "  <line x1=\"20\" y1=\"50\" x2=\"300\" y2=\"50\" stroke=\"#313244\" stroke-dasharray=\"3,3\"/>"
                    "  <text x=\"280\" y=\"90\" fill=\"#bac2de\" font-size=\"7\" font-family=\"sans-serif\">60s</text>"
                    "</svg></p>";
            break;

        case 3: // Advanced Search
            html += "<h2>4. Advanced Search &amp; Presets</h2>"
                    "<p>Run progressive, recursive wildcard searches through directories instantly.</p>"
                    "<h3>Step-by-Step Search Guide</h3>"
                    "<ol>"
                    "<li><b>Type Query:</b> Click inside the search text bar at the top of either file panel.</li>"
                    "<li><b>Debouncing:</b> As you type, the search worker waits 300ms before starting to scan. This prevents keyboard lag.</li>"
                    "<li><b>Navigation:</b> Double-click a search result to immediately navigate to its parent directory and highlight the file.</li>"
                    "</ol>"
                    "<h3>Wildcards &amp; Regular Expressions (Regex)</h3>"
                    "<ul>"
                    "<li>Check the <b>Use Regex</b> box to enable pattern matching.</li>"
                    "<li>Example: Type <code>^image_\\d{3}\\.(png|jpg)$</code> to find files matching 'image_' followed by exactly three digits and a png/jpg extension.</li>"
                    "</ul>"
                    "<h3>Metadata Operators</h3>"
                    "<p>Refine queries using logical search filters directly in the search field:</p>"
                    "<ul>"
                    "<li><b>Size Operators:</b> Filter by size using <code>&lt;</code>, <code>&gt;</code>, or <code>=</code> (e.g. <code>&gt; 100 MB</code> or <code>&lt; 5 KB</code>).</li>"
                    "<li><b>Date Range:</b> Type <code>modified: today</code>, <code>modified: week</code>, or <code>modified: month</code> to match time thresholds.</li>"
                    "<li><b>Tag Matches:</b> Type <code>tag: work</code> to filter files matching that tag.</li>"
                    "</ul>"
                    "<h3>Search Presets</h3>"
                    "<ul>"
                    "<li><b>Save Preset:</b> Select <b>Search -> Save Current Search as Preset...</b>, name your preset, and click Save.</li>"
                    "<li><b>Load Preset:</b> Select your saved query from the <b>Search -> Saved Presets</b> dynamic dropdown menu to execute it instantly.</li>"
                    "</ul>";
            break;

        case 4: // Media Preview & Covers
            html += "<h2>5. Media Preview &amp; Docks</h2>"
                    "<p>An intelligent media preview panel displays context-aware summaries of highlighted items in the right-side dock.</p>"
                    "<h3>Supported Preview Formats &amp; Interactive Players</h3>"
                    "<ul>"
                    "<li><b>Audio/Video Player:</b> Highlight a music or video file to load it in the player.</li>"
                    "<ul>"
                    "<li><b>CC Subtitles:</b> Click the <b>CC</b> button to load subtitle tracks (e.g. .srt, .vtt) and choose active subtitles.</li>"
                    "<li><b>Playback Modes:</b> Toggle <b>Shuffle (🔀)</b> to randomize the queue, or cycle <b>Repeat (🔁)</b> (None, Repeat Track, Repeat All).</li>"
                    "<li><b>Status Bar Mini-Player:</b> Collapsing the preview panel transfers media controls to a compact status bar player.</li>"
                    "<li><b>Playlist Context Menu:</b> Right-click any track in the queue to <i>Play Track</i>, <i>Remove Track</i>, <i>Stop</i>, or <i>Clear Queue</i>.</li>"
                    "<li><b>Folder Drop:</b> Drag and drop a folder into the Preview Panel to enqueue all media files inside.</li>"
                    "</ul>"
                    "<li><b>Interactive Audio Visualizers:</b> Under the Equalizer tab, click buttons to choose between retro spectrum bars, circular radial bars, or a real-time oscilloscope waveform graph.</li>"
                    "<li><b>CD/DVD Case Overlays:</b> Album folders containing audio files or movie directories automatically render with glossy physical media cases. These are rendered asynchronously in a background thread pool for zero-lag scrolling and have realistic physical aspect ratios. You can toggle visibility via the View menu (<i>Enable Media Casing Overlays</i>).</li>"
                    "<ul>"
                    "<li><b>File-Specific &amp; Folder Artwork:</b> For files (like <code>movie1.mp4</code>), it checks for file-specific artwork first (e.g., <code>movie1_cover.jpg</code> or <code>movie1.jpg</code>), and falls back to folder-wide artwork (e.g., <code>folder.jpg</code>) if no specific art is found. All checks are fully <b>case-insensitive</b> on Linux.</li>"
                    "<li><b>Supported Artwork Names (Case-Insensitive):</b></li>"
                    "<ul>"
                    "<li><b>Blu-ray Cases:</b> <code>bluray_cover.jpg/jpeg/png</code>, <code>bluray.jpg/jpeg/png</code>. Automatically selected for video files containing <i>bluray</i> or <i>blu-ray</i> in their name.</li>"
                    "<li><b>DVD Cases:</b> <code>dvd_cover.jpg/jpeg/png</code>, <code>dvd.jpg/jpeg/png</code>, <code>movie.jpg/jpeg/png</code>, <code>poster.jpg/jpeg/png</code>. Default for folders or video files.</li>"
                    "<li><b>CD Jewel Cases:</b> <code>folder.jpg/jpeg/png</code>, <code>cover.jpg/jpeg/png</code>. Default for audio files or directories containing music.</li>"
                    "</ul>"
                    "<li><b>Supported Media Files:</b></li>"
                    "<ul>"
                    "<li><b>Audio (CD Jeweling):</b> <code>.mp3</code>, <code>.flac</code>, <code>.wav</code>, <code>.ogg</code>, <code>.m4a</code>, <code>.wma</code>.</li>"
                    "<li><b>Video (DVD/Blu-ray Sleeves):</b> <code>.mp4</code>, <code>.mkv</code>, <code>.avi</code>, <code>.mov</code>, <code>.wmv</code>, <code>.flv</code>, <code>.webm</code>, <code>.m4v</code>.</li>"
                    "</ul>"
                    "</ul>"
                    "<li><b>Online Video &amp; TV Scraper:</b> Right-click any folder or video file and select <b>Scrape Video Metadata...</b> to search online databases (TVmaze or TMDb) for movies or show information.</li>"
                    "<ul>"
                    "<li><b>Poster Art:</b> Automatically downloads and writes artwork to <code>poster.jpg</code> and <code>folder.jpg</code>, rendering as a 3D physical sleeve casing.</li>"
                    "<li><b>Local NFOs:</b> Serializes XML details (Title, Plot, Year, Rating, Genre) into standard Kodi/Plex-compatible <code>.nfo</code> files.</li>"
                    "<li><b>Auto-Renaming:</b> Renames files/directories to the clean, standard <code>Title (Year)</code> format.</li>"
                    "</ul>"
                    "<li><b>In-place Text Editor:</b> Highlight a text or code document to edit inline. Click the <b>Save</b> button to write changes immediately.</li>"
                    "<li><b>PDF Viewer:</b> Browse PDFs page-by-page. Click the <b>Document Text</b> tab to view raw text extracted asynchronously in the background.</li>"
                    "</ul>"
                    "<h3>Interactive Properties &amp; Tag Editor</h3>"
                    "<ol>"
                    "<li>Click the <b>Properties</b> tab on the preview panel.</li>"
                    "<li>Inspect file sizes, dimensions, permissions, audio ID3 parameters, and EXIF parameters.</li>"
                    "<li>Type tag strings (comma-separated) in the tags line edit.</li>"
                    "<li>Select a color label, choose a custom icon overlay using the icon picker, and click <b>Apply</b>. The changes will be applied to all selected items simultaneously.</li>"
                    "<li>Right-click files to assign color labels and custom icon overlays directly via the context menu.</li>"
                    "</ol>"
                    "<h3>Docking &amp; Floating Panel Controls</h3>"
                    "<ul>"
                    "<li><b>Double-Click SNAP:</b> Double-click the title bar of the floating preview panel to snap it back to its last docked position instantly.</li>"
                    "<li><b>Helper Context Menu:</b> Right-click anywhere on the Preview Panel or its title bar to open a context menu. Select <b>Dock to Right Side</b>, <b>Dock to Left Side</b>, <b>Float Window</b>, or <b>Hide Preview Panel</b> to position it with one click.</li>"
                    "</ul>";
            break;

        case 5: // Custom Script Buttons
            html += "<h2>6. Custom Script Buttons</h2>"
                    "<p>Right-click the custom toolbar to edit, add, delete, or import/export script buttons. These buttons can run terminal scripts or invoke internal file manager functions.</p>"
                    "<h3>Step-by-Step Command Configuration</h3>"
                    "<ol>"
                    "<li>Right-click the custom toolbar and select <b>Add Button...</b>.</li>"
                    "<li>Fill in the button name, description (tooltip), and pick an icon.</li>"
                    "<li><b>Choose Command Mode:</b></li>"
                    "<ul>"
                    "<li><b>Shell Script Mode:</b> Runs a standard Bash script asynchronously. You can use macro replacements:</li>"
                    "<ul>"
                    "<li><code>{filepath}</code>: Absolute path of the highlighted file.</li>"
                    "<li><code>{dir}</code>: Active panel directory.</li>"
                    "<li><code>{dest}</code>: Sibling panel directory.</li>"
                    "<li>Bash Environment variables: <code>$AMIFILES_CURRENT_DIR</code> and <code>$AMIFILES_SELECTED</code> are exported.</li>"
                    "</ul>"
                    "<li><b>Native Internal Mode:</b> Prefix your script command with <code>@internal:</code> to trigger internal functions:</li>"
                    "<pre>@internal:Copy - Copy selection\n"
                    "@internal:Move - Move selection\n"
                    "@internal:Delete - Delete selection\n"
                    "@internal:CompareSync - Open Sync tool\n"
                    "@internal:DuplicateFinder - Open Duplicates tool\n"
                    "@internal:Go {dest} - Jump to sibling path</pre>"
                    "</ul>"
                    "<li>Click Save. The button is added to the toolbar.</li>"
                    "</ol>"
                    "<h3>Split Terminal Console &amp; Shell Logs</h3>"
                    "<ul>"
                    "<li>Custom shell commands execute in the background. Output is piped to the bottom console tab.</li>"
                    "<li>You can split the console layout vertically or open multiple command output logs side-by-side.</li>"
                    "<li>Double-click directories to automatically sync the active path to the interactive bash shell prompt.</li>"
                    "</ul>";
            break;

        case 6: // Disk Utilities
            html += "<h2>7. Disk Utilities &amp; Premium Tools</h2>"
                    "<p>Amifiles integrates advanced power-user tools for file maintenance, virtual filesystem manipulation, and system monitoring.</p>"
                    "<h3>Compare &amp; Sync</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Navigate your Left and Right panels to the directories you want to synchronize.</li>"
                    "<li>Press <code>Ctrl+G</code> or click <b>Tools -> Compare &amp; Sync Folders...</b>.</li>"
                    "<li>Select your sync direction (Left to Right, Right to Left, or Two-way Merge).</li>"
                    "<li>Click <b>Scan</b> to view visual counts of matching, missing, or modified files.</li>"
                    "<li>Click <b>Sync</b> to execute.</li>"
                    "</ol>"
                    "<h3>Duplicate Finder &amp; Smart Assistant</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Press <code>Ctrl+U</code> or select <b>Tools -> Find Duplicate Files...</b>.</li>"
                    "<li>Select scanning paths, minimum size thresholds, and criteria (e.g. MD5 Hash matches).</li>"
                    "<li>Click <b>Scan</b> to search.</li>"
                    "<li>Use the selection helper (Keep Oldest, Keep Newest) to automatically mark duplicates, then click <b>Shred</b> or <b>Delete</b>.</li>"
                    "</ol>"
                    "<h3>Secure File Shredder</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select the file/folder you wish to destroy.</li>"
                    "<li>Select <b>Tools -> Secure Shred files...</b>.</li>"
                    "<li>Confirm target files in the checklist, and choose the number of overwrite cycles (DoD 3-pass).</li>"
                    "<li>Click <b>Shred</b>. The file data blocks are overwritten with raw patterns before deletion, preventing filesystem recovery.</li>"
                    "</ol>"
                    "<h3>Checksum Hash Checker</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select a file and select <b>Tools -> Calculate Checksum Hash...</b>.</li>"
                    "<li>Choose hash type: MD5, SHA-1, or SHA-256.</li>"
                    "<li>The hash is calculated asynchronously in the background.</li>"
                    "<li>Paste a verification hash inside the validation box. It renders **Green** for matches and **Red** for mismatches immediately.</li>"
                    "</ol>"
                    "<h3>Password-Protected Archives &amp; ADF/D64 VFS</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ul>"
                    "<li><b>Mount retro disks:</b> Double-click any Amiga <code>.adf</code>, Commodore <code>.d64</code>, CD/DVD <code>.iso</code>, or partition <code>.img</code>. The panel opens a virtual filesystem.</li>"
                    "<li><b>Modify disk contents:</b> Ensure <b>View -> Allow Modifications to Archives / Disk Images</b> is toggled ON (or check it in Preferences). Drag and drop files inside the panel, or press Delete to modify ADF and D64 formats.</li>"
                    "<li><b>Extract &amp; Post-Actions:</b> Right-click an archive and choose <b>Extract Archive...</b>. In the dialog box, select post-extraction options from the dropdown:</li>"
                    "<ul>"
                    "<li><b>Keep Archive Intact:</b> Unpacks files and leaves the source archive untouched.</li>"
                    "<li><b>Move Archive to Trash:</b> Automatically moves the source archive to system Trash.</li>"
                    "<li><b>Permanently Delete:</b> Erases the archive from disk after successful extraction.</li>"
                    "</ul>"
                    "</ul>"
                    "<h3>Visual Side-by-Side Diff &amp; Merge</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Highlight exactly two text files.</li>"
                    "<li>Right-click and select **Compare Files (Diff)**.</li>"
                    "<li>Scroll through synced visual panels displaying red (removed lines) and green (added lines) highlights.</li>"
                    "<li>Edit either pane inline, or click the gutter arrows to merge line additions directionally. Click **Save** to write back.</li>"
                    "</ol>"
                    "<h3>Interactive Image Editor</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select an image file. It loads in the right Preview Panel.</li>"
                    "<li>Click the **Edit Image** tab.</li>"
                    "<li>Select tools from the top bar (crop, rotate, brush, lines, text) to markup drawings, then click **Save** to overwrite.</li>"
                    "<li>Double-click the image to enter borderless **Fullscreen Viewer** gallery. Use arrow keys to cycle, <code>R</code> to rotate, <code>F</code> to flip, <code>I</code> to toggle the information metadata overlay, and <code>Esc</code> to exit. The control HUD menu and mouse cursor automatically hide after 2 seconds of inactivity or when playing a slideshow, and reappear instantly when you move the mouse or press any key.</li>"
                    "</ol>"
                    "<h3>Disk Space TreeMap Analyzer</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select **Tools -> Folder Space Analyzer...** (or press <code>Ctrl+L</code>).</li>"
                    "<li>Wait for scan. Hover over nested blocks to inspect file details, or double-click to navigate deeper.</li>"
                    "<li>Toggle the **Treemap / Sunburst** visualizer modes in the toolbar to swap display styles instantly.</li>"
                    "</ol>"
                    "<h3>Disk Usage Dashboard (smart://disk_dashboard)</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Go to your navigation address bar and type <code>smart://disk_dashboard</code>.</li>"
                    "<li>The panel will scan active folder trees and load visual charts, including file categories distribution (Donut chart), tags coverage, and a list of the 20 largest files.</li>"
                    "</ol>"
                    "<h3>Workspace Session Manager</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Set up your window tabs, layouts, view modes, and split orientations.</li>"
                    "<li>Click **View -> Save Layout -> Workspace Session Manager...** (or under Favorites).</li>"
                    "<li>To save: type a profile name and click **Save**.</li>"
                    "<li>To load: click a saved profile in the list and click **Load** to instantly restore your workspace state.</li>"
                    "</ol>"
                    "<h3>Rclone Cloud Mounts</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select **Tools -> Mount Remote Share...** or **Rclone Cloud Mounts...**.</li>"
                    "<li>Enter connection settings to mount remote directories as local local nodes.</li>"
                    "<li>Use the **Remote Mounts Manager...** to inspect or unmount active remote hooks.</li>"
                    "</ol>"
                    "<h3>Custom Dynamic Menu Launcher System</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select **Edit -> Configure Custom Menus...** to launch the editor dialog.</li>"
                    "<li>Use the buttons at the bottom of the hierarchy tree to add new top-level menus, nested submenus, separators, or action launchers.</li>"
                    "<li>Select any item to customize its properties in the right editor: change its display text, click the **...** button to select from the built-in **System Icon Theme Picker** (supporting real-time keyword search, sorted categories, and local image file browsing), style custom hex text colors, and configure display layout modes (Normal, IconOnly, TextOnly).</li>"
                    "<li>Click **Apply &amp; Save changes** to dynamically insert the new custom menus directly in the main top menu bar. Custom actions support macro parameters like `{dir}` and `{dest}`.</li>"
                    "<li>**Drag &amp; Drop Shortcuts**: Drag folders or files directly from pane list views and drop them onto the **Custom Commands Toolbar** to instantly append quick navigation or execution buttons, or drop them directly into the **Custom Menu Editor Hierarchy Tree** viewport to structure new items dynamically with automatic keyword-based icon assignments.</li>"
                    "<li>**Custom Button Editor**: Right-click any custom button on the toolbar and select **Edit Button...** to modify its name, script command, or icon. The **Browse...** button in this dialog is now integrated with the new **System Icon Theme Picker** for instant visual lookup of system icons and local image files.</li>"
                    "</ol>"
                    "<h3>Custom Multi-Toolbar System</h3>"
                    "<p><b>How to use:</b></p>"
                    "<ol>"
                    "<li>Select **Edit -> Configure Toolbars...** to open the Toolbar Studio dialog.</li>"
                    "<li>On the left column, you can create, rename, delete, or reorder toolbars.</li>"
                    "<li>In the middle column, customize buttons and separators for the active toolbar.</li>"
                    "<li>In the right editor, configure properties for the selected toolbar (Visible, Render Style) or selected button (Action Type, Label, Icon path, Command script).</li>"
                    "<li>**Internal Command Helper**: When creating or editing custom buttons/menus, click the **Insert Action...** button next to the command script field. This displays a searchable list of all 14 internal actions along with detailed explanations and syntax examples (e.g. `@internal:SpaceAnalyzer`). Double-click any item to insert it directly at the cursor.</li>"
                    "<li>**Central Operations Bar**: Enable the **View -> Central Operations Button Bar** setting to display a thin vertical button bar (Copy, Move, Cut, Paste, Delete, Rename, Refresh) directly between the left and right split folder panels.</li>"
                    "<li>Click **Save Config** to rebuild all toolbars dynamically. You can drag toolbars to float them or dock them to any window border (Top, Bottom, Left, Right).</li>"
                    "<li>Click **Reset Defaults** to revert to the factory layout layout.</li>"
                    "</ol>"
                    "<p style=\"text-align: center;\"><svg width=\"320\" height=\"140\" viewBox=\"0 0 320 140\" style=\"background:#1e1e2e; border:1px solid #45475a; border-radius:6px;\">"
                    "  <circle cx=\"70\" cy=\"70\" r=\"45\" fill=\"none\" stroke=\"#f38ba8\" stroke-width=\"20\" stroke-dasharray=\"282\" stroke-dashoffset=\"70\"/>"
                    "  <circle cx=\"70\" cy=\"70\" r=\"45\" fill=\"none\" stroke=\"#a6e3a1\" stroke-width=\"20\" stroke-dasharray=\"282\" stroke-dashoffset=\"180\"/>"
                    "  <circle cx=\"70\" cy=\"70\" r=\"45\" fill=\"none\" stroke=\"#89b4fa\" stroke-width=\"20\" stroke-dasharray=\"282\" stroke-dashoffset=\"240\"/>"
                    "  <circle cx=\"70\" cy=\"70\" r=\"30\" fill=\"#1e1e2e\"/>"
                    "  <text x=\"70\" y=\"73\" text-anchor=\"middle\" fill=\"#cdd6f4\" font-size=\"8\" font-family=\"sans-serif\" font-weight=\"bold\">1.2 GB</text>"
                    "  <rect x=\"150\" y=\"20\" width=\"70\" height=\"100\" fill=\"#f38ba8\" stroke=\"#1e1e2e\" stroke-width=\"1\"/>"
                    "  <text x=\"155\" y=\"35\" fill=\"#11111b\" font-size=\"7\" font-weight=\"bold\">Videos</text>"
                    "  <rect x=\"220\" y=\"20\" width=\"80\" height=\"50\" fill=\"#a6e3a1\" stroke=\"#1e1e2e\" stroke-width=\"1\"/>"
                    "  <text x=\"225\" y=\"35\" fill=\"#11111b\" font-size=\"7\" font-weight=\"bold\">Images</text>"
                    "  <rect x=\"220\" y=\"70\" width=\"40\" height=\"50\" fill=\"#89b4fa\" stroke=\"#1e1e2e\" stroke-width=\"1\"/>"
                    "  <rect x=\"260\" y=\"70\" width=\"40\" height=\"50\" fill=\"#cba6f7\" stroke=\"#1e1e2e\" stroke-width=\"1\"/>"
                    "</svg></p>";
            break;

        case 7: // Keyboard Shortcuts
            html += "<h2>8. Keyboard Shortcuts</h2>"
                    "<p>Keyboard shortcuts are fully customizable! Select <b>Edit -&gt; Keyboard Shortcuts...</b> to assign new key combinations to any of the actions listed below.</p>"
                    "<h3>Default Hotkeys</h3>"
                    "<table>"
                    "<tr><th>Action</th><th>Shortcut</th><th>Description</th></tr>"
                    "<tr><td><b>New Tab</b></td><td><code>Ctrl+T</code></td><td>Open new folder tab on active side</td></tr>"
                    "<tr><td><b>Close Tab</b></td><td><code>Ctrl+W</code></td><td>Close the active folder tab</td></tr>"
                    "<tr><td><b>Copy to Sibling</b></td><td><code>F5</code></td><td>Copy selection to opposite panel directory</td></tr>"
                    "<tr><td><b>Move to Sibling</b></td><td><code>F6</code></td><td>Move selection to opposite panel directory</td></tr>"
                    "<tr><td><b>Clone Path to Sibling</b></td><td><code>F9</code></td><td>Navigate opposite panel to active panel directory</td></tr>"
                    "<tr><td><b>Rename Item</b></td><td><code>F2</code></td><td>Rename the selected file/folder</td></tr>"
                    "<tr><td><b>Refresh Directory</b></td><td><code>Ctrl+R</code> (or <code>F5</code>)</td><td>Force reload active directory contents</td></tr>"
                    "<tr><td><b>Command Palette</b></td><td><code>Ctrl+Shift+P</code></td><td>Launch the Spotlight action overlay</td></tr>"
                    "<tr><td><b>Dual Pane Toggle</b></td><td><code>Ctrl+D</code></td><td>Toggle split panel on/off</td></tr>"
                    "<tr><td><b>Preview Toggle</b></td><td><code>F3</code> (or <code>Ctrl+P</code>)</td><td>Show or hide the media preview dock</td></tr>"
                    "<tr><td><b>Edit Selected File</b></td><td><code>F4</code></td><td>Edit the selected text/code file in-place</td></tr>"
                    "<tr><td><b>Flat View Toggle</b></td><td><code>Ctrl+F</code></td><td>Toggle flat recursive listing mode</td></tr>"
                    "<tr><td><b>Compare &amp; Sync</b></td><td><code>Ctrl+G</code></td><td>Launch the Folder Sync utility</td></tr>"
                    "<tr><td><b>Duplicate Finder</b></td><td><code>Ctrl+U</code></td><td>Launch the Duplicate File Finder</td></tr>"
                    "<tr><td><b>Bulk Rename Tool</b></td><td><code>Ctrl+Shift+R</code></td><td>Launch the Batch Renamer</td></tr>"
                    "<tr><td><b>Toggle Console</b></td><td><code>Ctrl+Shift+C</code></td><td>Toggle bottom terminal logs console</td></tr>"
                    "<tr><td><b>New Folder</b></td><td><code>F7</code> (or <code>Ctrl+N</code>)</td><td>Create a new directory</td></tr>"
                    "<tr><td><b>Delete Items</b></td><td><code>F8</code> (or <code>Del</code>)</td><td>Permanently delete the selected items</td></tr>"
                    "<tr><td><b>Sync Scroll Toggle</b></td><td><code>Ctrl+Shift+S</code></td><td>Link vertical scrollbars of active panels</td></tr>"
                    "<tr><td><b>Theme Studio</b></td><td><code>Ctrl+Shift+T</code></td><td>Open Catppuccin Theme Studio &amp; Customizer</td></tr>"
                    "<tr><td><b>Show Properties</b></td><td><code>Alt+Enter</code></td><td>Show properties for selected item</td></tr>"
                    "</table>";
            break;
    }

    html += "</body>";
    m_browser->setHtml(html);
}

void HelpDialog::onSearchTextChanged(const QString& text) {
    if (text.isEmpty()) {
        QTextCursor cursor = m_browser->textCursor();
        cursor.clearSelection();
        m_browser->setTextCursor(cursor);
        return;
    }
    QTextCursor cursor = m_browser->textCursor();
    cursor.setPosition(0);
    m_browser->setTextCursor(cursor);
    m_browser->find(text);
}

void HelpDialog::onFindNext() {
    QString text = m_searchEdit->text();
    if (!text.isEmpty()) {
        if (!m_browser->find(text)) {
            QTextCursor cursor = m_browser->textCursor();
            cursor.setPosition(0);
            m_browser->setTextCursor(cursor);
            m_browser->find(text);
        }
    }
}

void HelpDialog::onFindPrev() {
    QString text = m_searchEdit->text();
    if (!text.isEmpty()) {
        if (!m_browser->find(text, QTextDocument::FindBackward)) {
            QTextCursor cursor = m_browser->textCursor();
            cursor.movePosition(QTextCursor::End);
            m_browser->setTextCursor(cursor);
            m_browser->find(text, QTextDocument::FindBackward);
        }
    }
}
