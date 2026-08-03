#include "advancedtageditordialog.h"
#include "tageditordialog.h"
#include "metadataextractor.h"
#include "metadatafetcherdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QInputDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSplitter>
#include <QToolBar>
#include <QProcess>
#include <QMimeData>
#include <QBuffer>
#include <QUrl>
#include <QToolButton>
#include <QMenu>
#include <QDirIterator>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

AdvancedTagEditorDialog::AdvancedTagEditorDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent)
    , m_initialPaths(filePaths)
{
    setWindowTitle("Advanced Music Tag Editor");
    resize(1100, 680);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QTableWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; gridline-color: #313244; border-radius: 4px; selection-background-color: #313244; selection-color: #89b4fa; }"
        "QTableWidget::item:selected { background-color: #313244; color: #89b4fa; }"
        "QHeaderView::section { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; padding: 4px; font-weight: bold; }"
        "QLineEdit { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px 8px; }"
        "QLineEdit:focus { border: 1px solid #89b4fa; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QGroupBox { border: 1px solid #313244; border-radius: 6px; margin-top: 8px; padding-top: 10px; font-weight: bold; color: #89b4fa; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );

    m_networkManager = new QNetworkAccessManager(this);

    setupUI();
    loadFiles();
    populateTable();
    updateFormFromSelection();
}

void AdvancedTagEditorDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Toolbar
    QToolBar* toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setStyleSheet("QToolBar { background-color: #181825; border: 1px solid #313244; border-radius: 4px; spacing: 8px; padding: 4px; }");

    QAction* actAutoTag = toolbar->addAction("Filename -> Tag");
    QAction* actRename = toolbar->addAction("Tag -> Filename");
    QAction* actTrackWizard = toolbar->addAction("Auto-Number Tracks");
    QAction* actCase = toolbar->addAction("Case Converter");
    QAction* actScrape = toolbar->addAction("Fetch Online (MusicBrainz)");

    QToolButton* btnQuickActions = new QToolButton(this);
    btnQuickActions->setText("Quick Actions");
    btnQuickActions->setPopupMode(QToolButton::InstantPopup);
    btnQuickActions->setStyleSheet("QToolButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px 8px; font-weight: bold; }"
                                   "QToolButton:hover { background-color: #45475a; }");
    QMenu* menuQuick = new QMenu(btnQuickActions);
    menuQuick->setStyleSheet("QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #313244; }"
                             "QMenu::item:selected { background-color: #313244; color: #89b4fa; }");
    
    QAction* actVarious = menuQuick->addAction("Set Album Artist to 'Various Artists'");
    actVarious->setData("various_artists");
    QAction* actSwap = menuQuick->addAction("Swap Artist <-> Title");
    actSwap->setData("swap_artist_title");
    QAction* actStripNum = menuQuick->addAction("Remove track number prefixes from Title");
    actStripNum->setData("strip_track_numbers");
    QAction* actCopyAA = menuQuick->addAction("Copy Artist to Album Artist");
    actCopyAA->setData("copy_artist_albumartist");
    QAction* actTrim = menuQuick->addAction("Trim whitespaces from all tags");
    actTrim->setData("trim_whitespaces");

    btnQuickActions->setMenu(menuQuick);
    toolbar->addWidget(btnQuickActions);

    connect(actVarious, &QAction::triggered, this, &AdvancedTagEditorDialog::onQuickActionTriggered);
    connect(actSwap, &QAction::triggered, this, &AdvancedTagEditorDialog::onQuickActionTriggered);
    connect(actStripNum, &QAction::triggered, this, &AdvancedTagEditorDialog::onQuickActionTriggered);
    connect(actCopyAA, &QAction::triggered, this, &AdvancedTagEditorDialog::onQuickActionTriggered);
    connect(actTrim, &QAction::triggered, this, &AdvancedTagEditorDialog::onQuickActionTriggered);

    mainLayout->addWidget(toolbar);

    // 2. Splitter (Left: Table, Right: Form)
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(6);
    splitter->setStyleSheet("QSplitter::handle { background-color: #313244; }");

    // Left widget
    QWidget* leftContainer = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    m_tableFiles = new QTableWidget(this);
    m_tableFiles->setColumnCount(8);
    m_tableFiles->setHorizontalHeaderLabels({"File Name", "Title", "Artist", "Album", "Track", "Disc #", "Year", "Genre"});
    m_tableFiles->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableFiles->horizontalHeader()->setStretchLastSection(true);
    m_tableFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftLayout->addWidget(m_tableFiles);

    m_lblStatus = new QLabel("Loaded 0 tracks", this);
    m_lblStatus->setStyleSheet("color: #a6adc8; font-weight: bold;");
    leftLayout->addWidget(m_lblStatus);

    splitter->addWidget(leftContainer);

    // Right widget (Scrollable form)
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");

    QWidget* rightContainer = new QWidget(this);
    rightContainer->setStyleSheet("background-color: #181825; border-radius: 6px;");
    QVBoxLayout* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    rightLayout->setSpacing(10);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    // Helper lambda to add form row with checkbox (for bulk write support) and lock button
    auto addFieldRow = [&](const QString& labelText, QLineEdit*& edit, QCheckBox*& chk, QToolButton*& lockBtn, const QString& fieldName) {
        QHBoxLayout* hLay = new QHBoxLayout();
        hLay->setSpacing(6);
        
        edit = new QLineEdit(rightContainer);
        chk = new QCheckBox(rightContainer);
        chk->setToolTip("Write in bulk to all selected files");
        
        lockBtn = new QToolButton(rightContainer);
        lockBtn->setText("🔓");
        lockBtn->setCheckable(true);
        lockBtn->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
        lockBtn->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");
        
        hLay->addWidget(edit, 1);
        hLay->addWidget(lockBtn);
        hLay->addWidget(chk);
        
        form->addRow(labelText, hLay);
        
        connect(edit, &QLineEdit::textEdited, this, [=]() {
            chk->setChecked(true);
            onFieldEdited();
        });
        connect(lockBtn, &QToolButton::clicked, this, [=](bool checked) {
            lockBtn->setText(checked ? "🔒" : "🔓");
            edit->setReadOnly(checked);
            QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
            for (QTableWidgetItem* item : selectedItems) {
                int row = item->row();
                if (checked) {
                    m_tracks[row].lockedFields.insert(fieldName);
                } else {
                    m_tracks[row].lockedFields.remove(fieldName);
                }
            }
        });
    };

    addFieldRow("Title:", m_editTitle, m_chkWTitle, m_lockTitle, "title");
    addFieldRow("Artist:", m_editArtist, m_chkWArtist, m_lockArtist, "artist");
    addFieldRow("Album:", m_editAlbum, m_chkWAlbum, m_lockAlbum, "album");

    // Genre combobox layout
    m_editGenre = new QComboBox(rightContainer);
    m_editGenre->setEditable(true);
    m_editGenre->addItems({"Alternative", "Ambient", "Blues", "Classical", "Comedy", "Country", "Dance", "Disco", "Electronic", "Folk", "Hip-Hop", "House", "Indie", "Industrial", "Jazz", "Metal", "New Age", "Other", "Pop", "Punk", "R&B", "Rap", "Reggae", "Rock", "Soul", "Soundtrack", "Spoken Word", "Techno", "Trance", "Vocal"});
    
    m_lockGenre = new QToolButton(rightContainer);
    m_lockGenre->setText("🔓");
    m_lockGenre->setCheckable(true);
    m_lockGenre->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockGenre->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");
    
    m_chkWGenre = new QCheckBox(rightContainer);
    m_chkWGenre->setToolTip("Write in bulk to all selected files");
    QHBoxLayout* genreLay = new QHBoxLayout();
    genreLay->setSpacing(6);
    genreLay->addWidget(m_editGenre, 1);
    genreLay->addWidget(m_lockGenre);
    genreLay->addWidget(m_chkWGenre);
    form->addRow("Genre:", genreLay);
    connect(m_editGenre->lineEdit(), &QLineEdit::textEdited, this, [=]() {
        m_chkWGenre->setChecked(true);
        onFieldEdited();
    });
    connect(m_editGenre, &QComboBox::activated, this, [=]() {
        m_chkWGenre->setChecked(true);
        onFieldEdited();
    });
    connect(m_lockGenre, &QToolButton::clicked, this, [=](bool checked) {
        m_lockGenre->setText(checked ? "🔒" : "🔓");
        m_editGenre->setEnabled(!checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            int row = item->row();
            if (checked) {
                m_tracks[row].lockedFields.insert("genre");
            } else {
                m_tracks[row].lockedFields.remove("genre");
            }
        }
    });

    addFieldRow("Year:", m_editYear, m_chkWYear, m_lockYear, "year");

    // Track layout
    QHBoxLayout* trackLay = new QHBoxLayout();
    m_editTrack = new QLineEdit(rightContainer);
    m_editTrackTotal = new QLineEdit(rightContainer);
    m_editTrackTotal->setPlaceholderText("Total");
    
    m_lockTrack = new QToolButton(rightContainer);
    m_lockTrack->setText("🔓");
    m_lockTrack->setCheckable(true);
    m_lockTrack->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockTrack->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_lockTrackTotal = new QToolButton(rightContainer);
    m_lockTrackTotal->setText("🔓");
    m_lockTrackTotal->setCheckable(true);
    m_lockTrackTotal->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockTrackTotal->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_chkWTrack = new QCheckBox(rightContainer);
    m_chkWTrackTotal = new QCheckBox(rightContainer);
    trackLay->addWidget(m_editTrack, 2);
    trackLay->addWidget(m_lockTrack);
    trackLay->addWidget(m_chkWTrack);
    trackLay->addWidget(m_editTrackTotal, 1);
    trackLay->addWidget(m_lockTrackTotal);
    trackLay->addWidget(m_chkWTrackTotal);
    form->addRow("Track # / Total:", trackLay);

    connect(m_editTrack, &QLineEdit::textEdited, this, [=]() { m_chkWTrack->setChecked(true); onFieldEdited(); });
    connect(m_editTrackTotal, &QLineEdit::textEdited, this, [=]() { m_chkWTrackTotal->setChecked(true); onFieldEdited(); });
    
    connect(m_lockTrack, &QToolButton::clicked, this, [=](bool checked) {
        m_lockTrack->setText(checked ? "🔒" : "🔓");
        m_editTrack->setReadOnly(checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("track");
            else m_tracks[item->row()].lockedFields.remove("track");
        }
    });
    connect(m_lockTrackTotal, &QToolButton::clicked, this, [=](bool checked) {
        m_lockTrackTotal->setText(checked ? "🔒" : "🔓");
        m_editTrackTotal->setReadOnly(checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("trackTotal");
            else m_tracks[item->row()].lockedFields.remove("trackTotal");
        }
    });

    addFieldRow("Album Artist:", m_editAlbumArtist, m_chkWAlbumArtist, m_lockAlbumArtist, "albumArtist");

    // Disc layout
    QHBoxLayout* discLay = new QHBoxLayout();
    m_editDisc = new QLineEdit(rightContainer);
    m_editDiscTotal = new QLineEdit(rightContainer);
    m_editDiscTotal->setPlaceholderText("Total");

    m_lockDisc = new QToolButton(rightContainer);
    m_lockDisc->setText("🔓");
    m_lockDisc->setCheckable(true);
    m_lockDisc->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockDisc->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_lockDiscTotal = new QToolButton(rightContainer);
    m_lockDiscTotal->setText("🔓");
    m_lockDiscTotal->setCheckable(true);
    m_lockDiscTotal->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockDiscTotal->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_chkWDisc = new QCheckBox(rightContainer);
    m_chkWDiscTotal = new QCheckBox(rightContainer);
    discLay->addWidget(m_editDisc, 2);
    discLay->addWidget(m_lockDisc);
    discLay->addWidget(m_chkWDisc);
    discLay->addWidget(m_editDiscTotal, 1);
    discLay->addWidget(m_lockDiscTotal);
    discLay->addWidget(m_chkWDiscTotal);
    form->addRow("Disc # / Total:", discLay);

    connect(m_editDisc, &QLineEdit::textEdited, this, [=]() { m_chkWDisc->setChecked(true); onFieldEdited(); });
    connect(m_editDiscTotal, &QLineEdit::textEdited, this, [=]() { m_chkWDiscTotal->setChecked(true); onFieldEdited(); });

    connect(m_lockDisc, &QToolButton::clicked, this, [=](bool checked) {
        m_lockDisc->setText(checked ? "🔒" : "🔓");
        m_editDisc->setReadOnly(checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("disc");
            else m_tracks[item->row()].lockedFields.remove("disc");
        }
    });
    connect(m_lockDiscTotal, &QToolButton::clicked, this, [=](bool checked) {
        m_lockDiscTotal->setText(checked ? "🔒" : "🔓");
        m_editDiscTotal->setReadOnly(checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("discTotal");
            else m_tracks[item->row()].lockedFields.remove("discTotal");
        }
    });

    addFieldRow("Composer:", m_editComposer, m_chkWComposer, m_lockComposer, "composer");
    addFieldRow("BPM:", m_editBpm, m_chkBpm, m_lockBpm, "bpm");
    addFieldRow("Comment:", m_editComment, m_chkWComment, m_lockComment, "comment");

    // Lyrics row
    QWidget* labelWidget = new QWidget(rightContainer);
    QHBoxLayout* lyricsLabelLay = new QHBoxLayout(labelWidget);
    lyricsLabelLay->setContentsMargins(0, 0, 0, 0);
    lyricsLabelLay->setSpacing(4);
    lyricsLabelLay->addWidget(new QLabel("Lyrics:", labelWidget));
    QPushButton* btnFetchLyrics = new QPushButton("Fetch", labelWidget);
    btnFetchLyrics->setStyleSheet("background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 3px; padding: 2px 4px; font-size: 10px;");
    btnFetchLyrics->setToolTip("Fetch lyrics from online databases");
    lyricsLabelLay->addWidget(btnFetchLyrics);
    lyricsLabelLay->addStretch();

    QHBoxLayout* lyricsLay = new QHBoxLayout();
    m_editLyrics = new QPlainTextEdit(rightContainer);
    m_editLyrics->setMaximumHeight(80);
    m_editLyrics->setStyleSheet("QPlainTextEdit { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }");
    
    m_lockLyrics = new QToolButton(rightContainer);
    m_lockLyrics->setText("🔓");
    m_lockLyrics->setCheckable(true);
    m_lockLyrics->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockLyrics->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_chkWLyrics = new QCheckBox(rightContainer);
    m_chkWLyrics->setToolTip("Write in bulk to all selected files");
    lyricsLay->addWidget(m_editLyrics, 1);
    lyricsLay->addWidget(m_lockLyrics);
    lyricsLay->addWidget(m_chkWLyrics);
    form->addRow(labelWidget, lyricsLay);

    connect(m_editLyrics, &QPlainTextEdit::textChanged, this, [=]() {
        m_chkWLyrics->setChecked(true);
        onFieldEdited();
    });
    connect(btnFetchLyrics, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onFetchLyricsClicked);
    connect(m_lockLyrics, &QToolButton::clicked, this, [=](bool checked) {
        m_lockLyrics->setText(checked ? "🔒" : "🔓");
        m_editLyrics->setReadOnly(checked);
        btnFetchLyrics->setEnabled(!checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("lyrics");
            else m_tracks[item->row()].lockedFields.remove("lyrics");
        }
    });

    // Compilation checkbox
    QHBoxLayout* compLay = new QHBoxLayout();
    m_chkCompilation = new QCheckBox(rightContainer);
    
    m_lockCompilation = new QToolButton(rightContainer);
    m_lockCompilation->setText("🔓");
    m_lockCompilation->setCheckable(true);
    m_lockCompilation->setToolTip("Lock this field to prevent accidental changes or scraping overwrites");
    m_lockCompilation->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_chkWCompilation = new QCheckBox(rightContainer);
    compLay->addWidget(m_chkCompilation);
    compLay->addWidget(m_lockCompilation);
    compLay->addStretch();
    compLay->addWidget(m_chkWCompilation);
    form->addRow("Compilation:", compLay);
    connect(m_chkCompilation, &QCheckBox::checkStateChanged, this, [=]() { m_chkWCompilation->setChecked(true); onFieldEdited(); });
    connect(m_lockCompilation, &QToolButton::clicked, this, [=](bool checked) {
        m_lockCompilation->setText(checked ? "🔒" : "🔓");
        m_chkCompilation->setEnabled(!checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("compilation");
            else m_tracks[item->row()].lockedFields.remove("compilation");
        }
    });

    rightLayout->addLayout(form);

    // Group 3: Cover Art Box
    QGroupBox* artGroup = new QGroupBox("Cover Artwork", rightContainer);
    QVBoxLayout* artLayout = new QVBoxLayout(artGroup);
    artLayout->setSpacing(6);

    m_lblArtworkPreview = new QLabel("No Artwork", artGroup);
    m_lblArtworkPreview->setAlignment(Qt::AlignCenter);
    m_lblArtworkPreview->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 4px; min-height: 120px; color: #6c7086;");
    artLayout->addWidget(m_lblArtworkPreview);

    QHBoxLayout* artBtns = new QHBoxLayout();
    m_btnBrowseArtwork = new QPushButton("Browse...", artGroup);
    m_btnPasteArtwork = new QPushButton("Paste", artGroup);
    QPushButton* btnExtractArtwork = new QPushButton("Extract", artGroup);
    btnExtractArtwork->setToolTip("Extract embedded artwork to file");
    connect(btnExtractArtwork, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onExtractArtwork);
    m_btnDeleteArtwork = new QPushButton("Clear", artGroup);
    
    m_lockArtwork = new QToolButton(artGroup);
    m_lockArtwork->setText("🔓");
    m_lockArtwork->setCheckable(true);
    m_lockArtwork->setToolTip("Lock cover artwork to prevent accidental changes or scraping overwrites");
    m_lockArtwork->setStyleSheet("QToolButton { border: none; background: transparent; font-size: 16px; }");

    m_chkWArtwork = new QCheckBox(artGroup);
    m_chkWArtwork->setToolTip("Apply artwork to all selected tracks");

    artBtns->addWidget(m_btnBrowseArtwork);
    artBtns->addWidget(m_btnPasteArtwork);
    artBtns->addWidget(btnExtractArtwork);
    artBtns->addWidget(m_btnDeleteArtwork);
    artBtns->addWidget(m_lockArtwork);
    artBtns->addWidget(m_chkWArtwork);
    artLayout->addLayout(artBtns);

    connect(m_lockArtwork, &QToolButton::clicked, this, [=](bool checked) {
        m_lockArtwork->setText(checked ? "🔒" : "🔓");
        m_btnBrowseArtwork->setEnabled(!checked);
        m_btnPasteArtwork->setEnabled(!checked);
        m_btnDeleteArtwork->setEnabled(!checked);
        QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
        for (QTableWidgetItem* item : selectedItems) {
            if (checked) m_tracks[item->row()].lockedFields.insert("artwork");
            else m_tracks[item->row()].lockedFields.remove("artwork");
        }
    });

    rightLayout->addWidget(artGroup);

    // Group 4: Custom Tags Box
    QGroupBox* customGroup = new QGroupBox("Custom Tags", rightContainer);
    QVBoxLayout* customLayout = new QVBoxLayout(customGroup);
    customLayout->setSpacing(6);

    m_tableCustomTags = new QTableWidget(customGroup);
    m_tableCustomTags->setColumnCount(2);
    m_tableCustomTags->setHorizontalHeaderLabels({"Tag Name", "Value"});
    m_tableCustomTags->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableCustomTags->setStyleSheet("QTableWidget { background-color: #11111b; border: 1px solid #313244; color: #cdd6f4; gridline-color: #313244; }"
                                     "QHeaderView::section { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; }");
    m_tableCustomTags->setFixedHeight(150);
    customLayout->addWidget(m_tableCustomTags);

    QHBoxLayout* customBtns = new QHBoxLayout();
    m_btnAddCustomTag = new QPushButton("+ Add Tag", customGroup);
    m_btnRemoveCustomTag = new QPushButton("- Remove Tag", customGroup);
    customBtns->addWidget(m_btnAddCustomTag);
    customBtns->addWidget(m_btnRemoveCustomTag);
    customLayout->addLayout(customBtns);

    rightLayout->addWidget(customGroup);
    rightLayout->addStretch();

    scrollArea->setWidget(rightContainer);
    splitter->addWidget(scrollArea);

    splitter->setStretchFactor(0, 7);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter);

    // 3. Dialog buttons
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    QPushButton* btnApply = new QPushButton("Apply Changes", this);
    btnApply->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
                            "QPushButton:hover { background-color: #b4befe; }");
    QPushButton* btnSave = new QPushButton("Save & Close", this);
    btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }"
                           "QPushButton:hover { background-color: #94e2d5; }");
    QPushButton* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; }"
                             "QPushButton:hover { background-color: #45475a; }");

    bottomLayout->addWidget(btnApply);
    bottomLayout->addWidget(btnSave);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);

    // Connections
    connect(m_tableFiles, &QTableWidget::itemSelectionChanged, this, &AdvancedTagEditorDialog::onTableSelectionChanged);
    
    connect(m_btnBrowseArtwork, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onBrowseArtwork);
    connect(m_btnPasteArtwork, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onPasteArtwork);
    connect(m_btnDeleteArtwork, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onDeleteArtwork);
    connect(m_btnAddCustomTag, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onAddCustomTag);
    connect(m_btnRemoveCustomTag, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onRemoveCustomTag);
    connect(m_tableCustomTags, &QTableWidget::itemChanged, this, &AdvancedTagEditorDialog::onCustomTagCellChanged);

    connect(actAutoTag, &QAction::triggered, this, &AdvancedTagEditorDialog::onAutoTagFromFilename);
    connect(actRename, &QAction::triggered, this, &AdvancedTagEditorDialog::onRenameFromTags);
    connect(actTrackWizard, &QAction::triggered, this, &AdvancedTagEditorDialog::onTrackNumberWizard);
    connect(actCase, &QAction::triggered, this, &AdvancedTagEditorDialog::onCaseConversion);
    connect(actScrape, &QAction::triggered, this, &AdvancedTagEditorDialog::onOnlineScrape);

    connect(btnApply, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onApplyClicked);
    connect(btnSave, &QPushButton::clicked, this, &AdvancedTagEditorDialog::onSaveClicked);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void AdvancedTagEditorDialog::loadFiles() {
    m_tracks.clear();
    QStringList finalFiles;
    
    for (const QString& path : m_initialPaths) {
        QFileInfo info(path);
        if (info.isDir()) {
            QString folderPath = path;
            QFileInfo folderInfo(folderPath);
            QString folderName = folderInfo.fileName().toLower().trimmed();
            QRegularExpression discFolderRe("^(cd|disc|disc |cd )\\d+$");
            if (discFolderRe.match(folderName).hasMatch()) {
                folderPath = folderInfo.absoluteDir().absolutePath();
            }
            
            QDirIterator it(folderPath, QStringList() << "*.mp3" << "*.flac", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                finalFiles.append(it.next());
            }
        } else {
            QString ext = info.suffix().toLower();
            if (ext == "mp3" || ext == "flac") {
                finalFiles.append(path);
            }
        }
    }

    finalFiles.removeDuplicates();
    finalFiles.sort();

    for (const QString& path : finalFiles) {
        TrackEditInfo info;
        info.originalPath = path;
        info.currentPath = path;
        info.metadata = MetadataExtractor::extract(path);
        
        // Read artwork bytes if it has it
        if (info.metadata.hasEmbeddedArtwork) {
            QProcess proc;
            proc.start("exiftool", {"-Picture", "-b", path});
            if (proc.waitForFinished(3000)) {
                info.coverData = proc.readAllStandardOutput();
                if (!info.coverData.isEmpty()) {
                    QProcess mimeProc;
                    mimeProc.start("exiftool", {"-MimeType", "-b", path});
                    if (mimeProc.waitForFinished(1000)) {
                        info.coverMimeType = QString::fromUtf8(mimeProc.readAllStandardOutput()).trimmed();
                    }
                    if (info.coverMimeType.isEmpty()) info.coverMimeType = "image/jpeg";
                }
            }
        }
        
        m_tracks.append(info);
    }
    
    m_lblStatus->setText(QString("Loaded %1 audio track(s)").arg(m_tracks.size()));
}

void AdvancedTagEditorDialog::populateTable() {
    m_tableFiles->setRowCount(0);
    m_tableFiles->blockSignals(true);

    for (int i = 0; i < m_tracks.size(); ++i) {
        const TrackEditInfo& track = m_tracks[i];
        m_tableFiles->insertRow(i);

        QFileInfo info(track.currentPath);
        m_tableFiles->setItem(i, 0, new QTableWidgetItem(info.fileName()));
        m_tableFiles->setItem(i, 1, new QTableWidgetItem(track.metadata.title));
        m_tableFiles->setItem(i, 2, new QTableWidgetItem(track.metadata.artist));
        m_tableFiles->setItem(i, 3, new QTableWidgetItem(track.metadata.album));
        m_tableFiles->setItem(i, 4, new QTableWidgetItem(track.metadata.track));
        m_tableFiles->setItem(i, 5, new QTableWidgetItem(track.metadata.discNumber));
        m_tableFiles->setItem(i, 6, new QTableWidgetItem(track.metadata.year));
        m_tableFiles->setItem(i, 7, new QTableWidgetItem(track.metadata.genre));
        
        // Mark changed items in italics or visual color
        if (track.isModified) {
            for (int col = 0; col < 8; ++col) {
                QTableWidgetItem* item = m_tableFiles->item(i, col);
                if (item) {
                    QFont f = item->font();
                    f.setItalic(true);
                    item->setFont(f);
                    item->setForeground(QBrush(QColor(166, 227, 161))); // Mocha green
                }
            }
        }
    }
    
    m_tableFiles->blockSignals(false);
}

void AdvancedTagEditorDialog::onTableSelectionChanged() {
    if (m_blockFormUpdates) return;
    updateFormFromSelection();
}

void AdvancedTagEditorDialog::updateFormFromSelection() {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    m_blockFormUpdates = true;

    // Reset lock states of all UI lock buttons
    auto updateLockState = [&](QToolButton* lockBtn, QWidget* editWidget, const QString& fieldName) {
        if (!lockBtn || !editWidget) return;
        bool allLocked = !selectedRows.isEmpty();
        for (int row : selectedRows) {
            if (!m_tracks[row].lockedFields.contains(fieldName)) {
                allLocked = false;
                break;
            }
        }
        lockBtn->blockSignals(true);
        lockBtn->setChecked(allLocked);
        lockBtn->setText(allLocked ? "🔒" : "🔓");
        lockBtn->blockSignals(false);
        
        if (QLineEdit* le = qobject_cast<QLineEdit*>(editWidget)) {
            le->setReadOnly(allLocked);
        } else if (QComboBox* cb = qobject_cast<QComboBox*>(editWidget)) {
            cb->setEnabled(!allLocked);
        } else if (QPlainTextEdit* te = qobject_cast<QPlainTextEdit*>(editWidget)) {
            te->setReadOnly(allLocked);
        } else if (QCheckBox* ck = qobject_cast<QCheckBox*>(editWidget)) {
            ck->setEnabled(!allLocked);
        }
    };

    updateLockState(m_lockTitle, m_editTitle, "title");
    updateLockState(m_lockArtist, m_editArtist, "artist");
    updateLockState(m_lockAlbum, m_editAlbum, "album");
    updateLockState(m_lockGenre, m_editGenre, "genre");
    updateLockState(m_lockYear, m_editYear, "year");
    updateLockState(m_lockTrack, m_editTrack, "track");
    updateLockState(m_lockTrackTotal, m_editTrackTotal, "trackTotal");
    updateLockState(m_lockAlbumArtist, m_editAlbumArtist, "albumArtist");
    updateLockState(m_lockDisc, m_editDisc, "disc");
    updateLockState(m_lockDiscTotal, m_editDiscTotal, "discTotal");
    updateLockState(m_lockComposer, m_editComposer, "composer");
    updateLockState(m_lockBpm, m_editBpm, "bpm");
    updateLockState(m_lockComment, m_editComment, "comment");
    updateLockState(m_lockLyrics, m_editLyrics, "lyrics");
    updateLockState(m_lockCompilation, m_chkCompilation, "compilation");

    // For Artwork lock state
    {
        bool artworkLocked = !selectedRows.isEmpty();
        for (int row : selectedRows) {
            if (!m_tracks[row].lockedFields.contains("artwork")) {
                artworkLocked = false;
                break;
            }
        }
        if (m_lockArtwork) {
            m_lockArtwork->blockSignals(true);
            m_lockArtwork->setChecked(artworkLocked);
            m_lockArtwork->setText(artworkLocked ? "🔒" : "🔓");
            m_lockArtwork->blockSignals(false);
        }
        if (m_btnBrowseArtwork) m_btnBrowseArtwork->setEnabled(!artworkLocked);
        if (m_btnPasteArtwork) m_btnPasteArtwork->setEnabled(!artworkLocked);
        if (m_btnDeleteArtwork) m_btnDeleteArtwork->setEnabled(!artworkLocked);
    }

    // Reset checkboxes
    m_chkWTitle->setChecked(false);
    m_chkWArtist->setChecked(false);
    m_chkWAlbum->setChecked(false);
    m_chkWGenre->setChecked(false);
    m_chkWYear->setChecked(false);
    m_chkWTrack->setChecked(false);
    m_chkWTrackTotal->setChecked(false);
    m_chkWAlbumArtist->setChecked(false);
    m_chkWDisc->setChecked(false);
    m_chkWDiscTotal->setChecked(false);
    m_chkWComposer->setChecked(false);
    m_chkBpm->setChecked(false);
    m_chkWComment->setChecked(false);
    m_chkWLyrics->setChecked(false);
    m_chkWCompilation->setChecked(false);
    m_chkWArtwork->setChecked(false);

    if (selectedRows.isEmpty()) {
        // Clear all
        m_editTitle->clear();
        m_editArtist->clear();
        m_editAlbum->clear();
        m_editGenre->setCurrentText("");
        m_editYear->clear();
        m_editTrack->clear();
        m_editTrackTotal->clear();
        m_editAlbumArtist->clear();
        m_editDisc->clear();
        m_editDiscTotal->clear();
        m_editComposer->clear();
        m_editBpm->clear();
        m_editComment->clear();
        m_editLyrics->clear();
        m_chkCompilation->setChecked(false);
        m_lblArtworkPreview->setText("No selection");
        m_lblArtworkPreview->setPixmap(QPixmap());
        
        m_blockFormUpdates = false;
        return;
    }

    if (selectedRows.size() == 1) {
        int idx = *selectedRows.begin();
        const TrackEditInfo& track = m_tracks[idx];

        m_editTitle->setText(track.metadata.title);
        m_editArtist->setText(track.metadata.artist);
        m_editAlbum->setText(track.metadata.album);
        m_editGenre->setCurrentText(track.metadata.genre);
        m_editYear->setText(track.metadata.year);
        m_editTrack->setText(track.metadata.track);
        m_editTrackTotal->setText(track.metadata.trackTotal);
        m_editAlbumArtist->setText(track.metadata.albumArtist);
        m_editDisc->setText(track.metadata.discNumber);
        m_editDiscTotal->setText(track.metadata.discTotal);
        m_editComposer->setText(track.metadata.composer);
        m_editBpm->setText(track.metadata.bpm);
        m_editComment->setText(track.metadata.comment);
        m_editLyrics->setPlainText(track.metadata.lyrics);
        m_chkCompilation->setChecked(track.metadata.compilation);

        m_currentArtworkData = track.coverData;
        m_currentArtworkMimeType = track.coverMimeType;
        m_currentArtworkChanged = false;

        QPixmap pix = loadArtworkPixmap(m_currentArtworkData, m_currentArtworkMimeType);
        if (!pix.isNull()) {
            m_lblArtworkPreview->setPixmap(pix.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_lblArtworkPreview->setText("No Artwork");
            m_lblArtworkPreview->setPixmap(QPixmap());
        }

        // Hide checkboxes for single edit
        m_chkWTitle->setVisible(false);
        m_chkWArtist->setVisible(false);
        m_chkWAlbum->setVisible(false);
        m_chkWGenre->setVisible(false);
        m_chkWYear->setVisible(false);
        m_chkWTrack->setVisible(false);
        m_chkWTrackTotal->setVisible(false);
        m_chkWAlbumArtist->setVisible(false);
        m_chkWDisc->setVisible(false);
        m_chkWDiscTotal->setVisible(false);
        m_chkWComposer->setVisible(false);
        m_chkBpm->setVisible(false);
        m_chkWComment->setVisible(false);
        m_chkWLyrics->setVisible(false);
        m_chkWCompilation->setVisible(false);
        m_chkWArtwork->setVisible(false);

    } else {
        // Multi-select bulk edit mode: display common values
        m_chkWTitle->setVisible(true);
        m_chkWArtist->setVisible(true);
        m_chkWAlbum->setVisible(true);
        m_chkWGenre->setVisible(true);
        m_chkWYear->setVisible(true);
        m_chkWTrack->setVisible(true);
        m_chkWTrackTotal->setVisible(true);
        m_chkWAlbumArtist->setVisible(true);
        m_chkWDisc->setVisible(true);
        m_chkWDiscTotal->setVisible(true);
        m_chkWComposer->setVisible(true);
        m_chkBpm->setVisible(true);
        m_chkWComment->setVisible(true);
        m_chkWLyrics->setVisible(true);
        m_chkWCompilation->setVisible(true);
        m_chkWArtwork->setVisible(true);

        auto list = selectedRows.values();
        const TrackEditInfo& first = m_tracks[list.first()];
        
        QString title = first.metadata.title;
        QString artist = first.metadata.artist;
        QString album = first.metadata.album;
        QString genre = first.metadata.genre;
        QString year = first.metadata.year;
        QString trackNum = first.metadata.track;
        QString trackTotal = first.metadata.trackTotal;
        QString albumArtist = first.metadata.albumArtist;
        QString discNum = first.metadata.discNumber;
        QString discTotal = first.metadata.discTotal;
        QString composer = first.metadata.composer;
        QString bpm = first.metadata.bpm;
        QString comment = first.metadata.comment;
        QString lyrics = first.metadata.lyrics;
        bool compilation = first.metadata.compilation;
        QByteArray artwork = first.coverData;
        QString mime = first.coverMimeType;

        for (int i = 1; i < list.size(); ++i) {
            const TrackEditInfo& t = m_tracks[list[i]];
            if (t.metadata.title != title) title = "";
            if (t.metadata.artist != artist) artist = "";
            if (t.metadata.album != album) album = "";
            if (t.metadata.genre != genre) genre = "";
            if (t.metadata.year != year) year = "";
            if (t.metadata.track != trackNum) trackNum = "";
            if (t.metadata.trackTotal != trackTotal) trackTotal = "";
            if (t.metadata.albumArtist != albumArtist) albumArtist = "";
            if (t.metadata.discNumber != discNum) discNum = "";
            if (t.metadata.discTotal != discTotal) discTotal = "";
            if (t.metadata.composer != composer) composer = "";
            if (t.metadata.bpm != bpm) bpm = "";
            if (t.metadata.comment != comment) comment = "";
            if (t.metadata.lyrics != lyrics) lyrics = "";
            if (t.metadata.compilation != compilation) compilation = false;
            if (t.coverData != artwork) artwork = QByteArray();
        }

        m_editTitle->setText(title);
        m_editArtist->setText(artist);
        m_editAlbum->setText(album);
        m_editGenre->setCurrentText(genre);
        m_editYear->setText(year);
        m_editTrack->setText(trackNum);
        m_editTrackTotal->setText(trackTotal);
        m_editAlbumArtist->setText(albumArtist);
        m_editDisc->setText(discNum);
        m_editDiscTotal->setText(discTotal);
        m_editComposer->setText(composer);
        m_editBpm->setText(bpm);
        m_editComment->setText(comment);
        m_editLyrics->setPlainText(lyrics);
        m_chkCompilation->setChecked(compilation);

        m_editTitle->setPlaceholderText(title.isEmpty() ? "<Multiple Values>" : "");
        m_editArtist->setPlaceholderText(artist.isEmpty() ? "<Multiple Values>" : "");
        m_editAlbum->setPlaceholderText(album.isEmpty() ? "<Multiple Values>" : "");
        m_editGenre->lineEdit()->setPlaceholderText(genre.isEmpty() ? "<Multiple Values>" : "");
        m_editYear->setPlaceholderText(year.isEmpty() ? "<Multiple Values>" : "");
        m_editTrack->setPlaceholderText(trackNum.isEmpty() ? "<Multiple Values>" : "");
        m_editTrackTotal->setPlaceholderText(trackTotal.isEmpty() ? "<Multiple Values>" : "");
        m_editAlbumArtist->setPlaceholderText(albumArtist.isEmpty() ? "<Multiple Values>" : "");
        m_editDisc->setPlaceholderText(discNum.isEmpty() ? "<Multiple Values>" : "");
        m_editDiscTotal->setPlaceholderText(discTotal.isEmpty() ? "<Multiple Values>" : "");
        m_editComposer->setPlaceholderText(composer.isEmpty() ? "<Multiple Values>" : "");
        m_editBpm->setPlaceholderText(bpm.isEmpty() ? "<Multiple Values>" : "");
        m_editComment->setPlaceholderText(comment.isEmpty() ? "<Multiple Values>" : "");
        m_editLyrics->setPlaceholderText(lyrics.isEmpty() ? "<Multiple Values>" : "");

        m_currentArtworkData = artwork;
        m_currentArtworkMimeType = mime;
        m_currentArtworkChanged = false;

        QPixmap pix = loadArtworkPixmap(artwork, mime);
        if (!pix.isNull()) {
            m_lblArtworkPreview->setPixmap(pix.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_lblArtworkPreview->setText("<Multiple Artwork / Empty>");
            m_lblArtworkPreview->setPixmap(QPixmap());
        }
    }

    populateCustomTagsTable(selectedRows);
    m_blockFormUpdates = false;
}

void AdvancedTagEditorDialog::onFieldEdited() {
    // Auto-check bulk edit checkboxes when values change
}

QPixmap AdvancedTagEditorDialog::loadArtworkPixmap(const QByteArray& data, const QString& mimeType) {
    Q_UNUSED(mimeType);
    if (data.isEmpty()) return QPixmap();
    QPixmap pix;
    if (pix.loadFromData(data)) {
        return pix;
    }
    return QPixmap();
}

void AdvancedTagEditorDialog::onBrowseArtwork() {
    QString startDir = QDir::homePath();
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    if (!selectedItems.isEmpty()) {
        int idx = selectedItems.first()->row();
        if (idx >= 0 && idx < m_tracks.size()) {
            startDir = QFileInfo(m_tracks[idx].currentPath).absolutePath();
        }
    }
    QString fileName = QFileDialog::getOpenFileName(this, "Select Cover Image File", startDir, "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            m_currentArtworkData = file.readAll();
            m_currentArtworkMimeType = fileName.endsWith(".png", Qt::CaseInsensitive) ? "image/png" : "image/jpeg";
            m_currentArtworkChanged = true;
            m_chkWArtwork->setChecked(true);
            
            QPixmap pix = loadArtworkPixmap(m_currentArtworkData, m_currentArtworkMimeType);
            m_lblArtworkPreview->setPixmap(pix.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void AdvancedTagEditorDialog::onPasteArtwork() {
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if (mimeData->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimeData->imageData());
        if (!img.isNull()) {
            QBuffer buffer(&m_currentArtworkData);
            buffer.open(QIODevice::WriteOnly);
            img.save(&buffer, "JPG");
            m_currentArtworkMimeType = "image/jpeg";
            m_currentArtworkChanged = true;
            m_chkWArtwork->setChecked(true);
            
            QPixmap pix = QPixmap::fromImage(img);
            m_lblArtworkPreview->setPixmap(pix.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } else if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                m_currentArtworkData = file.readAll();
                m_currentArtworkMimeType = path.endsWith(".png", Qt::CaseInsensitive) ? "image/png" : "image/jpeg";
                m_currentArtworkChanged = true;
                m_chkWArtwork->setChecked(true);
                
                QPixmap pix = loadArtworkPixmap(m_currentArtworkData, m_currentArtworkMimeType);
                m_lblArtworkPreview->setPixmap(pix.scaled(m_lblArtworkPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    }
}

void AdvancedTagEditorDialog::onDeleteArtwork() {
    m_currentArtworkData.clear();
    m_currentArtworkMimeType.clear();
    m_currentArtworkChanged = true;
    m_chkWArtwork->setChecked(true);
    m_lblArtworkPreview->setText("No Artwork / Stripped");
    m_lblArtworkPreview->setPixmap(QPixmap());
}

void AdvancedTagEditorDialog::onAutoTagFromFilename() {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more tracks in the list to apply filename parsing.");
        return;
    }

    QDialog dlgPattern(this);
    dlgPattern.setWindowTitle("Parse Tags from Filename");
    QVBoxLayout* lay = new QVBoxLayout(&dlgPattern);
    
    lay->addWidget(new QLabel("Select or enter a filename parsing format pattern:", &dlgPattern));
    
    QHBoxLayout* comboLayout = new QHBoxLayout();
    
    QComboBox* combo = new QComboBox(&dlgPattern);
    combo->setEditable(true);
    
    QSettings settings("Amifiles", "Amifiles");
    QStringList presets = settings.value("autotag_presets").toStringList();
    if (presets.isEmpty()) {
        presets = {
            "%track% - %artist% - %title%",
            "%track% - %title%",
            "%artist% - %title%",
            "%artist% - %album% - %track% - %title%",
            "Disc %disc%/%track% - %title%",
            "%artist% - %album% - Disc %disc% - %track% - %title%"
        };
    }
    combo->addItems(presets);
    comboLayout->addWidget(combo, 1);
    
    QPushButton* btnAddPreset = new QPushButton("+", &dlgPattern);
    btnAddPreset->setToolTip("Save current pattern as preset");
    btnAddPreset->setFixedWidth(30);
    comboLayout->addWidget(btnAddPreset);
    
    QPushButton* btnDelPreset = new QPushButton("-", &dlgPattern);
    btnDelPreset->setToolTip("Delete selected preset");
    btnDelPreset->setFixedWidth(30);
    comboLayout->addWidget(btnDelPreset);
    
    lay->addLayout(comboLayout);
    
    connect(btnAddPreset, &QPushButton::clicked, [&]() {
        QString text = combo->currentText().trimmed();
        if (text.isEmpty()) return;
        
        int idx = combo->findText(text);
        if (idx == -1) {
            combo->addItem(text);
            combo->setCurrentText(text);
        }
        
        QStringList updated;
        for (int i = 0; i < combo->count(); ++i) {
            updated.append(combo->itemText(i));
        }
        QSettings s("Amifiles", "Amifiles");
        s.setValue("autotag_presets", updated);
        QMessageBox::information(&dlgPattern, "Preset Saved", "Pattern saved to your auto-tag presets!");
    });

    connect(btnDelPreset, &QPushButton::clicked, [&]() {
        int idx = combo->currentIndex();
        if (idx != -1) {
            combo->removeItem(idx);
            
            QStringList updated;
            for (int i = 0; i < combo->count(); ++i) {
                updated.append(combo->itemText(i));
            }
            QSettings s("Amifiles", "Amifiles");
            s.setValue("autotag_presets", updated);
        }
    });

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* btnOk = new QPushButton("Parse", &dlgPattern);
    QPushButton* btnCan = new QPushButton("Cancel", &dlgPattern);
    btns->addWidget(btnOk);
    btns->addWidget(btnCan);
    lay->addLayout(btns);
    
    connect(btnOk, &QPushButton::clicked, &dlgPattern, &QDialog::accept);
    connect(btnCan, &QPushButton::clicked, &dlgPattern, &QDialog::reject);
    
    if (dlgPattern.exec() != QDialog::Accepted) return;
    
    QString pattern = combo->currentText();
    if (pattern.isEmpty()) return;

    // Convert pattern to regular expression
    QString regexStr = pattern;
    regexStr = QRegularExpression::escape(regexStr);
    
    // Unescape the custom tokens
    regexStr.replace(QRegularExpression::escape("%title%"), "(?<title>.+?)");
    regexStr.replace(QRegularExpression::escape("%artist%"), "(?<artist>.+?)");
    regexStr.replace(QRegularExpression::escape("%album%"), "(?<album>.+?)");
    regexStr.replace(QRegularExpression::escape("%track%"), "(?<track>\\d+)");
    regexStr.replace(QRegularExpression::escape("%year%"), "(?<year>\\d{4})");
    regexStr.replace(QRegularExpression::escape("%genre%"), "(?<genre>.+?)");
    regexStr.replace(QRegularExpression::escape("%disc%"), "(?<disc>\\d+)");
    
    // Match whole string or support extensions
    regexStr = "^" + regexStr + "(?:\\.[a-zA-Z0-9]+)?$";
    
    QRegularExpression re(regexStr);
    
    int matchedCount = 0;
    
    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        QFileInfo info(track.currentPath);
        
        QString matchStr;
        if (pattern.contains("/")) {
            // Match against parent folder name + file name (e.g. "Disc 2/01 - Time.mp3")
            matchStr = info.dir().dirName() + "/" + info.fileName();
        } else {
            matchStr = info.fileName();
        }
        
        QRegularExpressionMatch match = re.match(matchStr);
        if (match.hasMatch()) {
            matchedCount++;
            track.isModified = true;
            
            if (match.capturedTexts().contains("title") && !track.lockedFields.contains("title")) track.metadata.title = match.captured("title").trimmed();
            if (match.capturedTexts().contains("artist") && !track.lockedFields.contains("artist")) track.metadata.artist = match.captured("artist").trimmed();
            if (match.capturedTexts().contains("album") && !track.lockedFields.contains("album")) track.metadata.album = match.captured("album").trimmed();
            if (match.capturedTexts().contains("track") && !track.lockedFields.contains("track")) track.metadata.track = match.captured("track").trimmed();
            if (match.capturedTexts().contains("year") && !track.lockedFields.contains("year")) track.metadata.year = match.captured("year").trimmed();
            if (match.capturedTexts().contains("genre") && !track.lockedFields.contains("genre")) track.metadata.genre = match.captured("genre").trimmed();
            if (match.capturedTexts().contains("disc") && !track.lockedFields.contains("disc")) track.metadata.discNumber = match.captured("disc").trimmed();
        }
    }
    
    if (matchedCount > 0) {
        populateTable();
        updateFormFromSelection();
        QMessageBox::information(this, "Auto-Tag Complete", QString("Successfully parsed tags for %1 track(s).").arg(matchedCount));
    } else {
        QMessageBox::warning(this, "Match Failed", "No filenames matched the selected parsing format pattern.");
    }
}

void AdvancedTagEditorDialog::onRenameFromTags() {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more tracks in the list to apply renaming.");
        return;
    }

    QDialog dlgPattern(this);
    dlgPattern.setWindowTitle("Rename Files from Tags");
    QVBoxLayout* lay = new QVBoxLayout(&dlgPattern);
    
    lay->addWidget(new QLabel("Enter the renaming format pattern:", &dlgPattern));
    
    QHBoxLayout* comboLayout = new QHBoxLayout();
    
    QComboBox* combo = new QComboBox(&dlgPattern);
    combo->setEditable(true);
    
    QSettings settings("Amifiles", "Amifiles");
    QStringList presets = settings.value("rename_presets").toStringList();
    if (presets.isEmpty()) {
        presets = {
            "%track% - %title%",
            "%artist% - %title%",
            "%artist% - %album% - %track% - %title%",
            "%artist%/%album%/%track% - %title%",
            "%artist%/%album% (CD %disc%)/%track% - %title%"
        };
    }
    combo->addItems(presets);
    comboLayout->addWidget(combo, 1);
    
    QPushButton* btnAddPreset = new QPushButton("+", &dlgPattern);
    btnAddPreset->setToolTip("Save current pattern as preset");
    btnAddPreset->setFixedWidth(30);
    comboLayout->addWidget(btnAddPreset);
    
    QPushButton* btnDelPreset = new QPushButton("-", &dlgPattern);
    btnDelPreset->setToolTip("Delete selected preset");
    btnDelPreset->setFixedWidth(30);
    comboLayout->addWidget(btnDelPreset);
    
    lay->addLayout(comboLayout);
    
    connect(btnAddPreset, &QPushButton::clicked, [&]() {
        QString text = combo->currentText().trimmed();
        if (text.isEmpty()) return;
        
        int idx = combo->findText(text);
        if (idx == -1) {
            combo->addItem(text);
            combo->setCurrentText(text);
        }
        
        QStringList updated;
        for (int i = 0; i < combo->count(); ++i) {
            updated.append(combo->itemText(i));
        }
        QSettings s("Amifiles", "Amifiles");
        s.setValue("rename_presets", updated);
        QMessageBox::information(&dlgPattern, "Preset Saved", "Pattern saved to your renaming presets!");
    });

    connect(btnDelPreset, &QPushButton::clicked, [&]() {
        int idx = combo->currentIndex();
        if (idx != -1) {
            combo->removeItem(idx);
            
            QStringList updated;
            for (int i = 0; i < combo->count(); ++i) {
                updated.append(combo->itemText(i));
            }
            QSettings s("Amifiles", "Amifiles");
            s.setValue("rename_presets", updated);
        }
    });

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* btnOk = new QPushButton("Preview & Rename", &dlgPattern);
    QPushButton* btnCan = new QPushButton("Cancel", &dlgPattern);
    btns->addWidget(btnOk);
    btns->addWidget(btnCan);
    lay->addLayout(btns);
    
    connect(btnOk, &QPushButton::clicked, &dlgPattern, &QDialog::accept);
    connect(btnCan, &QPushButton::clicked, &dlgPattern, &QDialog::reject);
    
    if (dlgPattern.exec() != QDialog::Accepted) return;
    
    QString pattern = combo->currentText();
    if (pattern.isEmpty()) return;

    // Show Preview Dialog before applying
    QDialog dlgPreview(this);
    dlgPreview.setWindowTitle("Renaming Preview");
    dlgPreview.resize(600, 400);
    QVBoxLayout* previewLay = new QVBoxLayout(&dlgPreview);
    
    QTableWidget* previewTable = new QTableWidget(&dlgPreview);
    previewTable->setColumnCount(2);
    previewTable->setHorizontalHeaderLabels({"Original Name", "Proposed New Name"});
    previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    previewLay->addWidget(previewTable);

    struct RenameMap {
        int index;
        QString oldPath;
        QString newPath;
    };
    QList<RenameMap> renames;
    
    previewTable->setRowCount(0);
    int rowIdx = 0;
    
    // Sort rows numerically by Disc Number first, then by Track Number (falling back to table order)
    QList<int> sortedRows = selectedRows.values();
    std::sort(sortedRows.begin(), sortedRows.end(), [this](int a, int b) {
        QString discA = m_tracks[a].metadata.discNumber;
        QString discB = m_tracks[b].metadata.discNumber;
        
        bool okDiscA = false, okDiscB = false;
        int numDiscA = discA.toInt(&okDiscA);
        int numDiscB = discB.toInt(&okDiscB);
        
        if (!okDiscA) numDiscA = 1;
        if (!okDiscB) numDiscB = 1;
        
        if (numDiscA != numDiscB) {
            return numDiscA < numDiscB;
        }
        
        QString trackA = m_tracks[a].metadata.track;
        QString trackB = m_tracks[b].metadata.track;
        bool okTrackA = false, okTrackB = false;
        int numTrackA = trackA.toInt(&okTrackA);
        int numTrackB = trackB.toInt(&okTrackB);
        if (okTrackA && okTrackB) {
            return numTrackA < numTrackB;
        } else if (okTrackA) {
            return true;
        } else if (okTrackB) {
            return false;
        }
        return a < b;
    });

    for (int row : sortedRows) {
        const TrackEditInfo& track = m_tracks[row];
        QFileInfo info(track.currentPath);
        QString ext = info.suffix();
        
        QString newName = pattern;
        newName.replace("%title%", track.metadata.title.isEmpty() ? "Unknown Title" : track.metadata.title);
        newName.replace("%artist%", track.metadata.artist.isEmpty() ? "Unknown Artist" : track.metadata.artist);
        newName.replace("%album%", track.metadata.album.isEmpty() ? "Unknown Album" : track.metadata.album);
        
        QString trackStr = track.metadata.track;
        if (trackStr.length() == 1) trackStr = "0" + trackStr; // pad
        newName.replace("%track%", trackStr.isEmpty() ? "00" : trackStr);
        newName.replace("%year%", track.metadata.year.isEmpty() ? "0000" : track.metadata.year);
        newName.replace("%genre%", track.metadata.genre.isEmpty() ? "Genre" : track.metadata.genre);
        
        if (track.metadata.discNumber.isEmpty()) {
            newName.replace("(CD %disc%)", "");
            newName.replace("(Disc %disc%)", "");
            newName.replace("CD %disc%", "");
            newName.replace("Disc %disc%", "");
            newName.replace("%disc%", "");
        } else {
            newName.replace("%disc%", track.metadata.discNumber);
        }
        
        // Remove illegal characters but preserve forward slash for directories
        newName.replace("\\", "/");
        newName.remove(QRegularExpression("[\\:*?\"<>|]"));
        
        newName.replace(QRegularExpression("/+"), "/");
        newName = newName.trimmed();
        if (newName.startsWith("/")) newName = newName.mid(1);
        if (newName.endsWith("/")) newName = newName.left(newName.length() - 1);
        
        newName = newName + "." + ext;
        
        QString newPath = QDir::cleanPath(info.absoluteDir().absoluteFilePath(newName));
        
        previewTable->insertRow(rowIdx);
        previewTable->setItem(rowIdx, 0, new QTableWidgetItem(info.fileName()));
        previewTable->setItem(rowIdx, 1, new QTableWidgetItem(newName));
        
        RenameMap rm;
        rm.index = row;
        rm.oldPath = track.currentPath;
        rm.newPath = newPath;
        renames.append(rm);
        rowIdx++;
    }

    QHBoxLayout* previewBtns = new QHBoxLayout();
    QPushButton* btnApply = new QPushButton("Apply Rename", &dlgPreview);
    QPushButton* btnCancelPreview = new QPushButton("Cancel", &dlgPreview);
    previewBtns->addWidget(btnApply);
    previewBtns->addWidget(btnCancelPreview);
    previewLay->addLayout(previewBtns);

    connect(btnApply, &QPushButton::clicked, &dlgPreview, &QDialog::accept);
    connect(btnCancelPreview, &QPushButton::clicked, &dlgPreview, &QDialog::reject);

    if (dlgPreview.exec() != QDialog::Accepted) return;

    // Apply renames
    int renameSuccess = 0;
    for (const RenameMap& rm : renames) {
        if (rm.oldPath == rm.newPath) continue;
        
        // Recursively create directory structure for target path
        QFileInfo targetFileInfo(rm.newPath);
        QDir().mkpath(targetFileInfo.absolutePath());
        
        if (QFile::rename(rm.oldPath, rm.newPath)) {
            m_tracks[rm.index].currentPath = rm.newPath;
            m_tracks[rm.index].isModified = true;
            renameSuccess++;
        }
    }

    populateTable();
    updateFormFromSelection();
    
    QMessageBox::information(this, "Renaming Complete", QString("Successfully renamed %1 file(s) on disk.").arg(renameSuccess));
}

void AdvancedTagEditorDialog::onTrackNumberWizard() {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more tracks in the list to number.");
        return;
    }

    bool okStart = false;
    int startVal = QInputDialog::getInt(this, "Auto-Number Tracks", "Starting track number:", 1, 1, 100, 1, &okStart);
    if (!okStart) return;

    bool okPad = false;
    QStringList options = {"Yes (e.g. 01, 02)", "No (e.g. 1, 2)"};
    QString padOpt = QInputDialog::getItem(this, "Zero Padding", "Padding:", options, 0, false, &okPad);
    if (!okPad) return;
    
    bool zeroPad = (padOpt == options.first());

    // Sort rows so tracks are numbered in table order
    auto list = selectedRows.values();
    std::sort(list.begin(), list.end());

    int currentNum = startVal;
    int totalCount = list.size();

    for (int row : list) {
        TrackEditInfo& track = m_tracks[row];
        QString val = QString::number(currentNum);
        if (zeroPad && val.length() == 1) val = "0" + val;
        
        bool modified = false;
        if (!track.lockedFields.contains("track")) {
            track.metadata.track = val;
            modified = true;
        }
        if (!track.lockedFields.contains("trackTotal")) {
            track.metadata.trackTotal = QString::number(totalCount);
            modified = true;
        }
        if (modified) {
            track.isModified = true;
        }
        currentNum++;
    }

    populateTable();
    updateFormFromSelection();
}

void AdvancedTagEditorDialog::onCaseConversion() {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more tracks in the list to convert case.");
        return;
    }

    QDialog dlgCase(this);
    dlgCase.setWindowTitle("Tag Case Converter");
    QVBoxLayout* lay = new QVBoxLayout(&dlgCase);

    lay->addWidget(new QLabel("Choose casing style:", &dlgCase));
    QComboBox* comboCase = new QComboBox(&dlgCase);
    comboCase->addItems({"Title Case", "UPPER CASE", "lower case", "Sentence case"});
    lay->addWidget(comboCase);

    lay->addWidget(new QLabel("Fields to apply:", &dlgCase));
    QCheckBox* chkTitle = new QCheckBox("Title", &dlgCase);
    QCheckBox* chkArtist = new QCheckBox("Artist", &dlgCase);
    QCheckBox* chkAlbum = new QCheckBox("Album", &dlgCase);
    QCheckBox* chkGenre = new QCheckBox("Genre", &dlgCase);
    chkTitle->setChecked(true);
    chkArtist->setChecked(true);
    chkAlbum->setChecked(true);

    lay->addWidget(chkTitle);
    lay->addWidget(chkArtist);
    lay->addWidget(chkAlbum);
    lay->addWidget(chkGenre);

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* btnOk = new QPushButton("Apply", &dlgCase);
    QPushButton* btnCan = new QPushButton("Cancel", &dlgCase);
    btns->addWidget(btnOk);
    btns->addWidget(btnCan);
    lay->addLayout(btns);

    connect(btnOk, &QPushButton::clicked, &dlgCase, &QDialog::accept);
    connect(btnCan, &QPushButton::clicked, &dlgCase, &QDialog::reject);

    if (dlgCase.exec() != QDialog::Accepted) return;

    QString casing = comboCase->currentText();
    
    auto toTitleCase = [](const QString& str) -> QString {
        QStringList words = str.split(' ', Qt::SkipEmptyParts);
        for (QString& w : words) {
            if (!w.isEmpty()) {
                w = w.left(1).toUpper() + w.mid(1).toLower();
            }
        }
        return words.join(' ');
    };

    auto toSentenceCase = [](const QString& str) -> QString {
        if (str.isEmpty()) return str;
        return str.left(1).toUpper() + str.mid(1).toLower();
    };

    auto applyCasing = [&](QString& field) {
        if (casing == "Title Case") field = toTitleCase(field);
        else if (casing == "UPPER CASE") field = field.toUpper();
        else if (casing == "lower case") field = field.toLower();
        else if (casing == "Sentence case") field = toSentenceCase(field);
    };

    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        bool changed = false;

        if (chkTitle->isChecked() && !track.lockedFields.contains("title")) { applyCasing(track.metadata.title); changed = true; }
        if (chkArtist->isChecked() && !track.lockedFields.contains("artist")) { applyCasing(track.metadata.artist); changed = true; }
        if (chkAlbum->isChecked() && !track.lockedFields.contains("album")) { applyCasing(track.metadata.album); changed = true; }
        if (chkGenre->isChecked() && !track.lockedFields.contains("genre")) { applyCasing(track.metadata.genre); changed = true; }

        if (changed) track.isModified = true;
    }

    populateTable();
    updateFormFromSelection();
}

void AdvancedTagEditorDialog::onOnlineScrape() {
    QStringList paths;
    for (const auto& t : m_tracks) {
        paths.append(t.currentPath);
    }
    if (paths.isEmpty()) return;

    MetadataFetcherDialog fetcher(paths, this);
    if (fetcher.exec() == QDialog::Accepted) {
        auto fetchedResults = fetcher.getResults();
        QByteArray fetchedArtwork = fetcher.getArtworkData();
        QString artworkMime = fetcher.getArtworkMimeType();

        for (int i = 0; i < m_tracks.size(); ++i) {
            if (fetchedResults.contains(i)) {
                FetchedTrack ft = fetchedResults[i];
                TrackEditInfo& track = m_tracks[i];
                bool modified = false;
                if (!track.lockedFields.contains("title")) { track.metadata.title = ft.title; modified = true; }
                if (!track.lockedFields.contains("artist")) { track.metadata.artist = ft.artist.isEmpty() ? track.metadata.artist : ft.artist; modified = true; }
                if (!track.lockedFields.contains("album")) { track.metadata.album = ft.album.isEmpty() ? track.metadata.album : ft.album; modified = true; }
                if (!track.lockedFields.contains("year")) { track.metadata.year = ft.year.isEmpty() ? track.metadata.year : ft.year; modified = true; }
                if (!track.lockedFields.contains("genre")) { track.metadata.genre = ft.genre.isEmpty() ? track.metadata.genre : ft.genre; modified = true; }
                if (!track.lockedFields.contains("track")) { track.metadata.track = QString::number(ft.trackNumber); modified = true; }
                if (!track.lockedFields.contains("trackTotal")) { track.metadata.trackTotal = QString::number(ft.trackCount); modified = true; }
                
                if (!fetchedArtwork.isEmpty() && !track.lockedFields.contains("artwork")) {
                    track.coverData = fetchedArtwork;
                    track.coverMimeType = artworkMime;
                    track.coverChanged = true;
                    modified = true;
                }
                if (modified) {
                    track.isModified = true;
                }
            }
        }

        populateTable();
        updateFormFromSelection();
        QMessageBox::information(this, "Metadata Fetched", "Successfully fetched and mapped online tracks!");
    }
}

void AdvancedTagEditorDialog::onApplyClicked() {
    applyFieldsToSelection();
    populateTable();
    updateFormFromSelection();
}

void AdvancedTagEditorDialog::applyFieldsToSelection() {
    if (m_tableCustomTags) {
        m_tableCustomTags->setCurrentItem(nullptr);
    }

    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) return;

    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        bool changed = false;

        // Apply fields only if checkbox is checked
        if (m_chkWTitle->isChecked()) { track.metadata.title = m_editTitle->text(); changed = true; }
        if (m_chkWArtist->isChecked()) { track.metadata.artist = m_editArtist->text(); changed = true; }
        if (m_chkWAlbum->isChecked()) { track.metadata.album = m_editAlbum->text(); changed = true; }
        if (m_chkWGenre->isChecked()) { track.metadata.genre = m_editGenre->currentText(); changed = true; }
        if (m_chkWYear->isChecked()) { track.metadata.year = m_editYear->text(); changed = true; }
        if (m_chkWTrack->isChecked()) { track.metadata.track = m_editTrack->text(); changed = true; }
        if (m_chkWTrackTotal->isChecked()) { track.metadata.trackTotal = m_editTrackTotal->text(); changed = true; }
        if (m_chkWAlbumArtist->isChecked()) { track.metadata.albumArtist = m_editAlbumArtist->text(); changed = true; }
        if (m_chkWDisc->isChecked()) { track.metadata.discNumber = m_editDisc->text(); changed = true; }
        if (m_chkWDiscTotal->isChecked()) { track.metadata.discTotal = m_editDiscTotal->text(); changed = true; }
        if (m_chkWComposer->isChecked()) { track.metadata.composer = m_editComposer->text(); changed = true; }
        if (m_chkBpm->isChecked()) { track.metadata.bpm = m_editBpm->text(); changed = true; }
        if (m_chkWComment->isChecked()) { track.metadata.comment = m_editComment->text(); changed = true; }
        if (m_chkWLyrics->isChecked()) { track.metadata.lyrics = m_editLyrics->toPlainText(); changed = true; }
        if (m_chkWCompilation->isChecked()) { track.metadata.compilation = m_chkCompilation->isChecked(); changed = true; }
        
        if (m_chkWArtwork->isChecked()) {
            track.coverData = m_currentArtworkData;
            track.coverMimeType = m_currentArtworkMimeType;
            track.coverChanged = true;
            changed = true;
        }

        if (changed) track.isModified = true;
    }
}

void AdvancedTagEditorDialog::onSaveClicked() {
    applyFieldsToSelection(); // Make sure current form edits are applied
    saveTagsToDisk();
    accept();
}

void AdvancedTagEditorDialog::saveTagsToDisk() {
    int successCount = 0;
    
    for (const auto& track : m_tracks) {
        if (!track.isModified) continue;
        
        QString path = track.currentPath;
        QString ext = QFileInfo(path).suffix().toLower();
        bool success = false;
        
        if (ext == "mp3") {
            success = TagEditorDialog::writeMp3Tags(
                path, track.metadata.title, track.metadata.artist, track.metadata.album,
                track.metadata.genre, track.metadata.year, track.metadata.albumArtist,
                track.metadata.discNumber, track.metadata.compilation,
                track.coverChanged && track.coverData.isEmpty(), track.coverData, track.coverMimeType,
                track.metadata.track, track.metadata.trackTotal, track.metadata.discTotal,
                track.metadata.composer, track.metadata.bpm, track.metadata.comment,
                track.metadata.lyrics, track.metadata.customTags
            );
        } else if (ext == "flac") {
            success = TagEditorDialog::writeFlacTags(
                path, track.metadata.title, track.metadata.artist, track.metadata.album,
                track.metadata.genre, track.metadata.year, track.metadata.albumArtist,
                track.metadata.discNumber, track.metadata.compilation,
                track.metadata.track, track.metadata.trackTotal, track.metadata.discTotal,
                track.metadata.composer, track.metadata.bpm, track.metadata.comment,
                track.metadata.lyrics, track.metadata.customTags
            );
            if (success && track.coverChanged) {
                if (track.coverData.isEmpty()) {
                    TagEditorDialog::stripFlacArtwork(path);
                } else {
                    TagEditorDialog::writeFlacArtwork(path, track.coverData, track.coverMimeType);
                }
            }
        }
        
        if (success) successCount++;
    }
    
    if (successCount > 0) {
        QMessageBox::information(this, "Tags Saved", QString("Successfully saved metadata for %1 file(s) on disk.").arg(successCount));
    }
}

void AdvancedTagEditorDialog::onQuickActionTriggered() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more tracks in the list to apply transformations.");
        return;
    }
    
    QString command = act->data().toString();
    
    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        bool changed = false;
        
        if (command == "various_artists") {
            if (!track.lockedFields.contains("albumArtist")) {
                track.metadata.albumArtist = "Various Artists";
                changed = true;
            }
        } else if (command == "swap_artist_title") {
            if (!track.lockedFields.contains("artist") && !track.lockedFields.contains("title")) {
                QString temp = track.metadata.artist;
                track.metadata.artist = track.metadata.title;
                track.metadata.title = temp;
                changed = true;
            }
        } else if (command == "strip_track_numbers") {
            if (!track.lockedFields.contains("title")) {
                QRegularExpression re("^\\d+\\s*(?:-|\\.)?\\s*");
                track.metadata.title.remove(re);
                changed = true;
            }
        } else if (command == "copy_artist_albumartist") {
            if (!track.lockedFields.contains("albumArtist")) {
                track.metadata.albumArtist = track.metadata.artist;
                changed = true;
            }
        } else if (command == "trim_whitespaces") {
            if (!track.lockedFields.contains("title")) { track.metadata.title = track.metadata.title.trimmed(); changed = true; }
            if (!track.lockedFields.contains("artist")) { track.metadata.artist = track.metadata.artist.trimmed(); changed = true; }
            if (!track.lockedFields.contains("album")) { track.metadata.album = track.metadata.album.trimmed(); changed = true; }
            if (!track.lockedFields.contains("genre")) { track.metadata.genre = track.metadata.genre.trimmed(); changed = true; }
            if (!track.lockedFields.contains("year")) { track.metadata.year = track.metadata.year.trimmed(); changed = true; }
            if (!track.lockedFields.contains("albumArtist")) { track.metadata.albumArtist = track.metadata.albumArtist.trimmed(); changed = true; }
            if (!track.lockedFields.contains("composer")) { track.metadata.composer = track.metadata.composer.trimmed(); changed = true; }
            if (!track.lockedFields.contains("comment")) { track.metadata.comment = track.metadata.comment.trimmed(); changed = true; }
        }
        
        if (changed) track.isModified = true;
    }
    
    populateTable();
    updateFormFromSelection();
}

static QString cleanQueryString(const QString& input) {
    QString res = input;
    res.remove(QRegularExpression("\\s*\\([^)]*(?:feat|remaster|explicit|deluxe|version)[^)]*\\)", QRegularExpression::CaseInsensitiveOption));
    res.remove(QRegularExpression("\\s*\\[[^]]*(?:feat|remaster|explicit|deluxe|version)[^]]*\\]", QRegularExpression::CaseInsensitiveOption));
    return res.trimmed();
}

void AdvancedTagEditorDialog::onFetchLyricsClicked() {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more tracks in the list to fetch lyrics.");
        return;
    }
    
    int queryCount = 0;
    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        if (!track.metadata.artist.trimmed().isEmpty() && !track.metadata.title.trimmed().isEmpty() && !track.lockedFields.contains("lyrics")) {
            queryCount++;
        }
    }
    
    if (queryCount == 0) {
        m_lblStatus->setText("No unlocked tracks with valid Artist and Title selected.");
        return;
    }
    
    m_activeLyricsQueries = queryCount;
    m_lblStatus->setText(QString("Fetching lyrics for %1 track(s)...").arg(queryCount));
    
    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        if (!track.metadata.artist.trimmed().isEmpty() && !track.metadata.title.trimmed().isEmpty() && !track.lockedFields.contains("lyrics")) {
            fetchLyricsForTrack(row);
        }
    }
}

void AdvancedTagEditorDialog::fetchLyricsForTrack(int idx) {
    TrackEditInfo& track = m_tracks[idx];
    QString artist = cleanQueryString(track.metadata.artist);
    QString title = cleanQueryString(track.metadata.title);
    if (artist.isEmpty() || title.isEmpty()) {
        m_activeLyricsQueries--;
        if (m_activeLyricsQueries <= 0) {
            m_lblStatus->setText("Lyrics fetch completed.");
        }
        return;
    }
    
    QUrl url("https://lrclib.net/api/get");
    QUrlQuery query;
    query.addQueryItem("artist_name", artist);
    query.addQueryItem("track_name", title);
    url.setQuery(query);
    
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Amifiles/1.0 ( dave@example.com )");
    
    QNetworkReply* reply = m_networkManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, idx]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fetchLyricsOvh(idx);
            return;
        }
        
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QString lyrics = obj["plainLyrics"].toString();
        if (!lyrics.isEmpty()) {
            m_tracks[idx].metadata.lyrics = lyrics;
            m_tracks[idx].isModified = true;
            updateUIIfSelected(idx);
            
            m_activeLyricsQueries--;
            if (m_activeLyricsQueries <= 0) {
                m_lblStatus->setText("Lyrics fetch completed.");
            }
        } else {
            fetchLyricsOvh(idx);
        }
    });
}

void AdvancedTagEditorDialog::fetchLyricsOvh(int idx) {
    TrackEditInfo& track = m_tracks[idx];
    QString artist = cleanQueryString(track.metadata.artist);
    QString title = cleanQueryString(track.metadata.title);
    if (artist.isEmpty() || title.isEmpty()) {
        m_activeLyricsQueries--;
        if (m_activeLyricsQueries <= 0) {
            m_lblStatus->setText("Lyrics fetch completed.");
        }
        return;
    }
    
    QUrl url(QString("https://api.lyrics.ovh/v1/%1/%2")
             .arg(QString(QUrl::toPercentEncoding(artist)))
             .arg(QString(QUrl::toPercentEncoding(title))));
             
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Amifiles/1.0 ( dave@example.com )");
    
    QNetworkReply* reply = m_networkManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, idx]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            QString lyrics = obj["lyrics"].toString();
            if (!lyrics.isEmpty()) {
                m_tracks[idx].metadata.lyrics = lyrics;
                m_tracks[idx].isModified = true;
                updateUIIfSelected(idx);
            }
        }
        
        m_activeLyricsQueries--;
        if (m_activeLyricsQueries <= 0) {
            m_lblStatus->setText("Lyrics fetch completed.");
        }
    });
}

void AdvancedTagEditorDialog::updateUIIfSelected(int idx) {
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    if (selectedRows.contains(idx)) {
        populateTable();
        updateFormFromSelection();
    }
}

void AdvancedTagEditorDialog::onExtractArtwork() {
    if (m_currentArtworkData.isEmpty()) {
        QMessageBox::warning(this, "No Artwork", "There is no artwork to extract.");
        return;
    }
    
    QString startDir = QDir::homePath();
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    if (!selectedItems.isEmpty()) {
        int idx = selectedItems.first()->row();
        if (idx >= 0 && idx < m_tracks.size()) {
            startDir = QFileInfo(m_tracks[idx].currentPath).absolutePath();
        }
    }
    
    QString defaultExt = (m_currentArtworkMimeType == "image/png") ? ".png" : ".jpg";
    QString defaultPath = QDir(startDir).filePath("cover" + defaultExt);
    
    QString fileName = QFileDialog::getSaveFileName(this, "Extract/Save Cover Artwork", defaultPath, "Images (*.jpg *.jpeg *.png)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentArtworkData);
            QMessageBox::information(this, "Artwork Saved", "Successfully saved cover artwork to file.");
        } else {
            QMessageBox::warning(this, "Error", "Failed to save the artwork file.");
        }
    }
}

void AdvancedTagEditorDialog::onAddCustomTag() {
    bool ok;
    QString tagName = QInputDialog::getText(this, "Add Custom Tag", "Tag Name (e.g. RECORDLABEL, MOOD, CATALOGNUMBER):", QLineEdit::Normal, "", &ok).trimmed().toUpper();
    if (!ok || tagName.isEmpty()) return;

    // Check if tag already exists in the table
    for (int r = 0; r < m_tableCustomTags->rowCount(); ++r) {
        QTableWidgetItem* item = m_tableCustomTags->item(r, 0);
        if (item && item->text() == tagName) {
            QMessageBox::warning(this, "Tag Exists", "This custom tag is already in the list.");
            return;
        }
    }

    m_tableCustomTags->blockSignals(true);
    int r = m_tableCustomTags->rowCount();
    m_tableCustomTags->insertRow(r);
    m_tableCustomTags->setItem(r, 0, new QTableWidgetItem(tagName));
    m_tableCustomTags->setItem(r, 1, new QTableWidgetItem(""));
    m_tableCustomTags->blockSignals(false);

    onCustomTagCellChanged();
}

void AdvancedTagEditorDialog::onRemoveCustomTag() {
    int currRow = m_tableCustomTags->currentRow();
    if (currRow < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a custom tag row in the table to remove.");
        return;
    }
    m_tableCustomTags->removeRow(currRow);
    onCustomTagCellChanged();
}

void AdvancedTagEditorDialog::onCustomTagCellChanged() {
    // Flag changes to selected tracks
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    if (selectedRows.isEmpty()) return;

    // Read custom tags from table
    QMap<QString, QString> tableCustomTags;
    for (int r = 0; r < m_tableCustomTags->rowCount(); ++r) {
        QTableWidgetItem* keyItem = m_tableCustomTags->item(r, 0);
        QTableWidgetItem* valItem = m_tableCustomTags->item(r, 1);
        if (keyItem && !keyItem->text().trimmed().isEmpty()) {
            QString val = valItem ? valItem->text() : "";
            tableCustomTags[keyItem->text().trimmed().toUpper()] = val;
        }
    }

    // Apply to selected tracks
    for (int row : selectedRows) {
        TrackEditInfo& track = m_tracks[row];
        QMap<QString, QString> merged = track.metadata.customTags;
        
        // Remove tags not in table
        for (auto it = merged.begin(); it != merged.end(); ) {
            if (!tableCustomTags.contains(it.key())) {
                it = merged.erase(it);
                track.isModified = true;
            } else {
                ++it;
            }
        }
        
        // Add or update tags from table
        for (auto it = tableCustomTags.constBegin(); it != tableCustomTags.constEnd(); ++it) {
            QString key = it.key();
            QString val = it.value();
            if (!val.isEmpty() || selectedRows.size() == 1) {
                if (merged[key] != val) {
                    merged[key] = val;
                    track.isModified = true;
                }
            }
        }
        track.metadata.customTags = merged;
    }

    populateTable(); // Redraw main list to show modified state indicators
}

void AdvancedTagEditorDialog::populateCustomTagsTable(const QSet<int>& selectedRows) {
    m_tableCustomTags->blockSignals(true);
    m_tableCustomTags->setRowCount(0);

    if (selectedRows.isEmpty()) {
        m_tableCustomTags->blockSignals(false);
        return;
    }

    // Map tag key -> set of values
    QMap<QString, QSet<QString>> allCustomTags;
    for (int idx : selectedRows) {
        const TrackEditInfo& track = m_tracks[idx];
        for (auto it = track.metadata.customTags.constBegin(); it != track.metadata.customTags.constEnd(); ++it) {
            allCustomTags[it.key().trimmed().toUpper()].insert(it.value().trimmed());
        }
    }

    int r = 0;
    for (auto it = allCustomTags.constBegin(); it != allCustomTags.constEnd(); ++it) {
        m_tableCustomTags->insertRow(r);
        
        QTableWidgetItem* keyItem = new QTableWidgetItem(it.key());
        m_tableCustomTags->setItem(r, 0, keyItem);
        
        QTableWidgetItem* valItem = new QTableWidgetItem();
        if (it.value().size() == 1) {
            valItem->setText(*it.value().begin());
        } else {
            valItem->setText("");
            valItem->setToolTip(QString("Multiple values: %1").arg(it.value().values().join(", ")));
        }
        m_tableCustomTags->setItem(r, 1, valItem);
        
        r++;
    }
    m_tableCustomTags->blockSignals(false);
}
