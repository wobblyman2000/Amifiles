#include "helpdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

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

    // Text Browser
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setSearchPaths({":"});
    contentLayout->addWidget(m_browser, 1);

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
                    "<li><b>Virtual Filesystem (VFS):</b> Seamlessly step inside archives (ZIP, TAR) and work on files without manual unpacking.</li>"
                    "<li><b>Pausable Copy Queue:</b> Safe multi-threaded file copying that handles collisions and preserves system responsiveness.</li>"
                    "<li><b>Custom Shell/Native Scripting:</b> Define custom buttons to run terminal commands, pass active directories/files, or execute internal functions.</li>"
                    "</ul>";
            break;

        case 1: // Layout & Tabs
            html += "<h2>2. Layout &amp; Tabs</h2>"
                    "<p>Amifiles divides screen real estate efficiently into left and right pane panels, a bottom command output console, and an on-demand right preview panel.</p>"
                    "<h3>Tabbed Folder Browsing</h3>"
                    "<ul>"
                    "<li>Open multiple folder locations in independent tabs on both sides.</li>"
                    "<li>Press <code>Ctrl+T</code> to duplicate the active tab's location in a new tab.</li>"
                    "<li>Press <code>Ctrl+W</code> or click the tab close buttons to discard tabs (a minimum of 1 tab is enforced on both sides).</li>"
                    "<li>Tab headers automatically update to show the current folder name.</li>"
                    "</ul>"
                    "<h3>Layout View Modes</h3>"
                    "<ul>"
                    "<li><b>Dual Pane (Ctrl+D):</b> Toggle split pane view on/off. When hidden, the active pane expands to full screen.</li>"
                    "<li><b>Flat View (Ctrl+F):</b> Enter recursive folder mode. Subdirectories are flattened into a single list view for quick filtering.</li>"
                    "<li><b>Drives Toolbar:</b> Access mounted disk partition buttons directly above the file panels. Toggle via View Menu.</li>"
                    "<li><b>Age Coloring:</b> Highlight recent files (Red for &lt; 24h, Blue for &lt; 7 days). Toggle via View Menu.</li>"
                    "</ul>"
                    "<h3>Layout Persistence &amp; Folder Rules</h3>"
                    "<ul>"
                    "<li><b>Save Layout Options:</b> Persist toolbar docking positions and main window dimensions. Toggle <b>Auto-save Layout on Close</b> (under the View menu) or use <b>Save Current Layout Now</b> / <b>Reset Layout to Defaults</b>.</li>"
                    "<li><b>Folder-Specific Layouts:</b> Map exact directories or file categories (Music, Videos, Documents, Images) to custom view modes and specific subsets of custom command buttons. Configure under <b>Tools -> Configure Folder-Specific Layouts</b>.</li>"
                    "</ul>"
                    "<h3>Tabbed Sidebar Panel (Ctrl+Shift+F)</h3>"
                    "<ul>"
                    "<li><b>Bookmarks Sidebar:</b> Displays folder bookmarks favorited using star toggles. Double-click items to navigate.</li>"
                    "<li><b>Filters Sidebar:</b> Instantly filter directory contents by file size categories (Large, Medium, Small) or modification dates (Today, This Week, This Month).</li>"
                    "</ul>";
            break;

        case 2: // File Transfer Queue
            html += "<h2>3. File Transfer Queue</h2>"
                    "<p>Amifiles runs file transfer operations (Copy and Move) in a dedicated background worker thread to keep the interface completely responsive during large tasks.</p>"
                    "<h3>Transfer Dashboard</h3>"
                    "<ul>"
                    "<li><b>Speed Chart Widget:</b> A beautiful, rolling 60-second line chart plotting instantaneous copy speeds in Catppuccin styles.</li>"
                    "<li><b>Pausable Queue:</b> Click <b>Pause/Resume</b> to halt transfers instantly.</li>"
                    "<li><b>Skip:</b> Click <b>Skip File</b> to discard copying the current file.</li>"
                    "<li><b>Queue list:</b> Inspect pending operations in the scrolling bottom list queue.</li>"
                    "</ul>"
                    "<h3>Interactive Duplicate Collision Resolver</h3>"
                    "<p>If a file with the same name exists at the destination, a grid comparison dialog presents modified timestamps and sizes of both files, offering six options:</p>"
                    "<ul>"
                    "<li><b>Overwrite / Overwrite All:</b> Replace the target file(s).</li>"
                    "<li><b>Skip / Skip All:</b> Skip copy operations for conflicting files.</li>"
                    "<li><b>Keep Both:</b> Retain both files. The copied file is automatically renamed to <code>filename (1).ext</code>.</li>"
                    "<li><b>Cancel:</b> Abort the rest of the queue.</li>"
                    "</ul>";
            break;

        case 3: // Advanced Search
            html += "<h2>4. Advanced Search &amp; Presets</h2>"
                    "<p>Run progressive, recursive wildcard searches through directories instantly.</p>"
                    "<h3>Debounced Search Worker</h3>"
                    "<ul>"
                    "<li>Type inside the search bar at the top of either file panel.</li>"
                    "<li>Execution is automatically debounced by 300ms to prevent lag during rapid typing.</li>"
                    "<li>Double-click search results to instantly navigate to the file's parent folder and highlight it in the panel.</li>"
                    "</ul>"
                    "<h3>Search Presets</h3>"
                    "<ul>"
                    "<li>Save complex queries by selecting <b>Search -&gt; Save Current Search as Preset...</b></li>"
                    "<li>Quickly recall saved queries from the <b>Search -&gt; Saved Presets</b> dynamic menu.</li>"
                    "</ul>";
            break;

        case 4: // Media Preview & Covers
            html += "<h2>5. Media Preview &amp; Covers</h2>"
                    "<p>An intelligent media preview panel displays context-aware summaries of highlighted items in the right-side dock.</p>"
                    "<h3>Supported Preview Formats</h3>"
                    "<ul>"
                    "<li><b>Audio/Video Player:</b> Embeds a media player. Integrates controls for play/pause/mute and volume. Drag and drop folders or files directly into the preview panel to automatically extract and play all playable media tracks. Synchronizes tracks with a mini player in the main status bar when the preview panel is collapsed.</li>"
                    "<li><b>CC Subtitles Selector:</b> Click the <b>CC</b> button to dynamically load video subtitle tracks and toggle or switch subtitles.</li>"
                    "<li><b>Playlist Playback Modes:</b> Toggle <b>Shuffle (🔀)</b> to randomize track selections. Cycle <b>Repeat (🔁)</b> to cycle between Repeat Off, Repeat One track (🔂), and Repeat All playlist. Right-click any track inside the Playlist Queue to show options to <b>Play Selected Track</b>, <b>Remove Selected Track</b>, <b>Stop &amp; Clear Current Track</b>, or <b>Clear Entire Queue</b>.</li>"
                    "<li><b>DVD/CD Case Overlays:</b> If a folder contains cover art matching audio files, Amifiles wraps the cover in a high-fidelity CD case overlay.</li>"
                    "<li><b>Text Editor:</b> Preview text/code files inside an interactive editor. Make changes and click <b>Save</b> to write updates back to disk immediately.</li>"
                    "</ul>";
            break;

        case 5: // Custom Script Buttons
            html += "<h2>6. Custom Script Buttons</h2>"
                    "<p>Right-click the custom toolbar to edit, add, or delete script buttons. These buttons can run terminal scripts or invoke internal file manager functions.</p>"
                    "<h3>1. Shell Script Mode</h3>"
                    "<p>Run any bash commands, including interactive GUI dialogs like <code>zenity</code>. You can write macros directly in your script which are replaced before run:</p>"
                    "<ul>"
                    "<li><code>{filepath}</code>: Absolute path of the first selected file.</li>"
                    "<li><code>{dir}</code>: Active panel directory path.</li>"
                    "<li><code>{dest}</code>: Opposite/Sibling panel directory path.</li>"
                    "<li>Environment variables: <code>$AMIFILES_CURRENT_DIR</code> and <code>$AMIFILES_SELECTED</code>.</li>"
                    "</ul>"
                    "<h3>2. Native Internal Command Mode</h3>"
                    "<p>To map a custom toolbar button to native Amifiles functions, start the command script with <code>@internal:</code> followed by one of these actions:</p>"
                    "<pre>@internal:Copy\n"
                    "@internal:Move\n"
                    "@internal:Cut\n"
                    "@internal:Paste\n"
                    "@internal:Delete\n"
                    "@internal:Rename\n"
                    "@internal:NewFolder\n"
                    "@internal:Refresh\n"
                    "@internal:ToggleDualPane\n"
                    "@internal:TogglePreview\n"
                    "@internal:ToggleFlatView\n"
                    "@internal:CompareSync\n"
                    "@internal:DuplicateFinder\n"
                    "@internal:Go {dest}  (or Go /path/to/folder)</pre>";
            break;

        case 6: // Disk Utilities
            html += "<h2>7. Disk Utilities &amp; Premium Tools</h2>"
                    "<h3>Compare &amp; Sync</h3>"
                    "<p>Quickly align two directories. Launches a comparison window showing missing, modified, or matching file counts. Run directional folder synchronization immediately.</p>"
                    "<h3>Duplicate Finder</h3>"
                    "<p>Scans selected paths and groups files that share identical MD5 checksum hashes, letting you locate and delete duplicate clutter immediately.</p>"
                    "<h3>Checksum Hash Checker</h3>"
                    "<p>Generates MD5, SHA-1, and SHA-256 checksums asynchronously using a background worker thread. Paste a hash in the validation field to compare matching states with real-time green/red highlights.</p>"
                    "<h3>Secure File Shredder</h3>"
                    "<p>Securely overwrites file data blocks using a DoD 5220.22-M 3-pass overwrite protocol before unlinking, preventing standard data recovery software from restoring shredded data.</p>"
                    "<h3>Embedded System Console &amp; Shell</h3>"
                    "<p>The bottom panel captures stdout, stderr, and execution messages from custom toolbar buttons in real-time, letting you debug shell scripts or review output immediately.</p>"
                    "<h3>Interactive Image Editor &amp; Annotator</h3>"
                    "<p>Edit images directly from the preview pane! Support cropping, 90-degree rotations, color palettes, and markup drawings (Freehand Pen, Rectangles, Lines, Text annotations).</p>"
                    "<h3>Password-Protected Archive Manager</h3>"
                    "<p>Compress and extract ZIP and <code>.7z</code> folders with password protection using secure back-end command pipelines.</p>"
                    "<h3>Hex Editor &amp; Binary Viewer</h3>"
                    "<p>Inspect binary files up to 64KB showing color-coded offset addresses, hex bytes, and printable ASCII text directly in the bottom tabs.</p>"
                    "<h3>Rclone Cloud VFS mounts</h3>"
                    "<p>Mount Google Drive, OneDrive, SFTP, FTP, and Dropbox dynamically using standard <code>rclone</code> and FUSE mounts. Remotes populate the drive bar automatically.</p>"
                    "<h3>Portable PDF Viewer</h3>"
                    "<p>View PDF documents asynchronously page-by-page via the <code>pdftoppm</code> engine, supporting full zoom and pagination controls.</p>"
                    "<h3>Customizable File Age Rules &amp; Badges</h3>"
                    "<p>Define custom color and emoji rules (e.g., <code>🔥</code> for &lt;24h, <code>❄️</code> for &gt;1 year) using the <b>Configure File Age Styles</b> option under the <b>View</b> menu. These rules are managed via the <code>AgeColorRule</code> structure (defined in <code>src/filepanel.h</code>) containing fields for operator (<code>&lt;=</code> or <code>&gt;=</code>), days threshold, text color, and emoji icon.</p>";
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
                    "<tr><td><b>Rename Item</b></td><td><code>F2</code></td><td>Rename the selected file/folder</td></tr>"
                    "<tr><td><b>Refresh Directory</b></td><td><code>Ctrl+R</code> (or <code>F5</code>)</td><td>Force reload active directory contents</td></tr>"
                    "<tr><td><b>Dual Pane Toggle</b></td><td><code>Ctrl+D</code></td><td>Toggle split panel on/off</td></tr>"
                    "<tr><td><b>Preview Toggle</b></td><td><code>Ctrl+P</code></td><td>Show or hide the media preview dock</td></tr>"
                    "<tr><td><b>Flat View Toggle</b></td><td><code>Ctrl+F</code></td><td>Toggle flat recursive listing mode</td></tr>"
                    "<tr><td><b>Compare &amp; Sync</b></td><td><code>Ctrl+G</code></td><td>Launch the Folder Sync utility</td></tr>"
                    "<tr><td><b>Duplicate Finder</b></td><td><code>Ctrl+U</code></td><td>Launch the Duplicate File Finder</td></tr>"
                    "<tr><td><b>Bulk Rename Tool</b></td><td><code>Ctrl+Shift+R</code></td><td>Launch the Batch Renamer</td></tr>"
                    "<tr><td><b>Toggle Console</b></td><td><code>Ctrl+Shift+C</code></td><td>Toggle bottom terminal logs console</td></tr>"
                    "<tr><td><b>New Folder</b></td><td><code>Ctrl+N</code></td><td>Create a new directory</td></tr>"
                    "<tr><td><b>Show Properties</b></td><td><code>Alt+Enter</code></td><td>Show properties for selected item</td></tr>"
                    "</table>";
            break;
    }

    html += "</body>";
    m_browser->setHtml(html);
}
