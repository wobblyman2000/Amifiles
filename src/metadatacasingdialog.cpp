#include "metadatacasingdialog.h"
#include "metadataextractor.h"
#include "tageditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>

MetadataCasingDialog::MetadataCasingDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Batch Casing & Cleanup Wizard");
    resize(700, 500);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    setupUI();
    updatePreviews();
}

void MetadataCasingDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // Target Options
    QWidget* targetGroup = new QWidget(this);
    QHBoxLayout* targetLayout = new QHBoxLayout(targetGroup);
    targetLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* lblTarget = new QLabel("Casing Targets:", this);
    lblTarget->setStyleSheet("font-weight: bold; color: #89b4fa;");
    targetLayout->addWidget(lblTarget);

    m_chkFilename = new QCheckBox("File Names", this);
    m_chkTitle = new QCheckBox("Audio Title Tag", this);
    m_chkArtist = new QCheckBox("Artist Tag", this);
    m_chkAlbum = new QCheckBox("Album Tag", this);

    // Load last checked values or set defaults
    QSettings settings("Amifiles", "Amifiles");
    m_chkFilename->setChecked(settings.value("casing/target_filename", true).toBool());
    m_chkTitle->setChecked(settings.value("casing/target_title", true).toBool());
    m_chkArtist->setChecked(settings.value("casing/target_artist", false).toBool());
    m_chkAlbum->setChecked(settings.value("casing/target_album", false).toBool());

    targetLayout->addWidget(m_chkFilename);
    targetLayout->addWidget(m_chkTitle);
    targetLayout->addWidget(m_chkArtist);
    targetLayout->addWidget(m_chkAlbum);
    targetLayout->addStretch(1);
    mainLayout->addWidget(targetGroup);

    // Mid Options
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    
    // Left: Casing Style
    QWidget* casingGroup = new QWidget(this);
    casingGroup->setStyleSheet("QWidget { background-color: #242535; border-radius: 8px; }");
    QVBoxLayout* casingLayout = new QVBoxLayout(casingGroup);
    QLabel* lblCasing = new QLabel("Casing Style", this);
    lblCasing->setStyleSheet("font-weight: bold; color: #a6e3a1; background: transparent;");
    casingLayout->addWidget(lblCasing);

    m_radTitleCase = new QRadioButton("Smart Title Case", this);
    m_radSentenceCase = new QRadioButton("Sentence case", this);
    m_radUppercase = new QRadioButton("UPPERCASE", this);
    m_radLowercase = new QRadioButton("lowercase", this);

    m_radTitleCase->setStyleSheet("background: transparent;");
    m_radSentenceCase->setStyleSheet("background: transparent;");
    m_radUppercase->setStyleSheet("background: transparent;");
    m_radLowercase->setStyleSheet("background: transparent;");

    QString style = settings.value("casing/style", "title").toString();
    if (style == "upper") m_radUppercase->setChecked(true);
    else if (style == "lower") m_radLowercase->setChecked(true);
    else if (style == "sentence") m_radSentenceCase->setChecked(true);
    else m_radTitleCase->setChecked(true);

    casingLayout->addWidget(m_radTitleCase);
    casingLayout->addWidget(m_radSentenceCase);
    casingLayout->addWidget(m_radUppercase);
    casingLayout->addWidget(m_radLowercase);
    optionsLayout->addWidget(casingGroup, 1);

    // Right: Cleanups
    QWidget* cleanGroup = new QWidget(this);
    cleanGroup->setStyleSheet("QWidget { background-color: #242535; border-radius: 8px; }");
    QVBoxLayout* cleanLayout = new QVBoxLayout(cleanGroup);
    QLabel* lblClean = new QLabel("Text Cleanups", this);
    lblClean->setStyleSheet("font-weight: bold; color: #f9e2af; background: transparent;");
    cleanLayout->addWidget(lblClean);

    m_chkUnderscores = new QCheckBox("Replace underscores with spaces", this);
    m_chkUrlEscapes = new QCheckBox("Replace %20 with spaces", this);
    m_chkCleanSpaces = new QCheckBox("Simplify double spaces & trim", this);

    m_chkUnderscores->setStyleSheet("background: transparent;");
    m_chkUrlEscapes->setStyleSheet("background: transparent;");
    m_chkCleanSpaces->setStyleSheet("background: transparent;");

    m_chkUnderscores->setChecked(settings.value("casing/clean_underscores", true).toBool());
    m_chkUrlEscapes->setChecked(settings.value("casing/clean_url_escapes", true).toBool());
    m_chkCleanSpaces->setChecked(settings.value("casing/clean_spaces", true).toBool());

    cleanLayout->addWidget(m_chkUnderscores);
    cleanLayout->addWidget(m_chkUrlEscapes);
    cleanLayout->addWidget(m_chkCleanSpaces);
    optionsLayout->addWidget(cleanGroup, 1);

    mainLayout->addLayout(optionsLayout);

    // Table Preview
    m_tablePreview = new QTableWidget(this);
    m_tablePreview->setColumnCount(4);
    m_tablePreview->setHorizontalHeaderLabels({"File / Tag Type", "Original Value", "Proposed Value", "Status"});
    m_tablePreview->setStyleSheet(
        "QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; gridline-color: #313244; border-radius: 8px; }"
        "QHeaderView::section { background-color: #313244; color: #cdd6f4; padding: 4px; border: none; font-weight: bold; }"
    );
    m_tablePreview->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tablePreview->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_tablePreview, 1);

    // Bottom status & buttons
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_lblStatus = new QLabel("Ready", this);
    bottomLayout->addWidget(m_lblStatus, 1);

    m_chkDryRun = new QCheckBox("Dry-Run Simulation Mode", this);
    m_chkDryRun->setToolTip("Simulate execution and log pending renames and tag edits inside the preview table without modifying actual files on disk.");
    m_chkDryRun->setChecked(false);
    bottomLayout->addWidget(m_chkDryRun);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    m_progress->setStyleSheet("QProgressBar { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; text-align: center; color: #cdd6f4; } QProgressBar::chunk { background-color: #a6e3a1; }");
    bottomLayout->addWidget(m_progress, 1);

    QPushButton* btnApply = new QPushButton("Apply Casing", this);
    btnApply->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #94e2d5; }");
    connect(btnApply, &QPushButton::clicked, this, &MetadataCasingDialog::onApply);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; padding: 6px 12px; border-radius: 4px; } QPushButton:hover { background-color: #45475a; }");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    bottomLayout->addWidget(btnApply);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);

    // Wire up dynamic previews
    connect(m_chkFilename, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_chkTitle, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_chkArtist, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_chkAlbum, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
    
    connect(m_radTitleCase, &QRadioButton::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_radSentenceCase, &QRadioButton::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_radUppercase, &QRadioButton::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_radLowercase, &QRadioButton::toggled, this, &MetadataCasingDialog::updatePreviews);

    connect(m_chkUnderscores, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_chkUrlEscapes, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
    connect(m_chkCleanSpaces, &QCheckBox::toggled, this, &MetadataCasingDialog::updatePreviews);
}

QString MetadataCasingDialog::computeCasedString(const QString& input, const QString& mode, bool cleanUnderscores, bool cleanUrlEscapes, bool cleanSpaces) {
    QString res = input;
    if (cleanUrlEscapes) {
        res.replace("%20", " ");
    }
    if (cleanUnderscores) {
        res.replace("_", " ");
    }
    if (cleanSpaces) {
        res = res.simplified();
    }

    if (mode == "upper") {
        return res.toUpper();
    } else if (mode == "lower") {
        return res.toLower();
    } else if (mode == "sentence") {
        if (res.isEmpty()) return res;
        QString lower = res.toLower();
        return lower.left(1).toUpper() + lower.mid(1);
    } else if (mode == "title") {
        if (res.isEmpty()) return res;
        // English smart Title Case minor words
        static const QStringList minorWords = {
            "a", "an", "the", "and", "but", "or", "for", "nor", "on", "at", "to", "by", 
            "for", "from", "in", "of", "on", "with", "de", "la", "le", "en"
        };
        QStringList words = res.split(' ', Qt::SkipEmptyParts);
        for (int i = 0; i < words.size(); ++i) {
            QString w = words[i].toLower();
            if (w.isEmpty()) continue;
            // Capitalize if it's the first word, last word, or not in the minor list
            if (i == 0 || i == words.size() - 1 || !minorWords.contains(w)) {
                w = w.left(1).toUpper() + w.mid(1);
            }
            words[i] = w;
        }
        return words.join(' ');
    }
    return res;
}

void MetadataCasingDialog::updatePreviews() {
    m_tablePreview->setRowCount(0);

    bool cFilename = m_chkFilename->isChecked();
    bool cTitle = m_chkTitle->isChecked();
    bool cArtist = m_chkArtist->isChecked();
    bool cAlbum = m_chkAlbum->isChecked();

    QString mode = "title";
    if (m_radUppercase->isChecked()) mode = "upper";
    else if (m_radLowercase->isChecked()) mode = "lower";
    else if (m_radSentenceCase->isChecked()) mode = "sentence";

    bool cleanUnder = m_chkUnderscores->isChecked();
    bool cleanUrl = m_chkUrlEscapes->isChecked();
    bool cleanSpaces = m_chkCleanSpaces->isChecked();

    auto addPreviewRow = [this](const QString& typeName, const QString& orig, const QString& proposed, const QString& status) {
        int row = m_tablePreview->rowCount();
        m_tablePreview->insertRow(row);
        
        QTableWidgetItem* itemType = new QTableWidgetItem(typeName);
        QTableWidgetItem* itemOrig = new QTableWidgetItem(orig);
        QTableWidgetItem* itemProp = new QTableWidgetItem(proposed);
        QTableWidgetItem* itemStatus = new QTableWidgetItem(status);
        
        itemOrig->setForeground(QColor("#f38ba8")); // soft Catppuccin red
        itemProp->setForeground(QColor("#a6e3a1")); // soft Catppuccin green
        itemStatus->setForeground(QColor("#fab387")); // soft Catppuccin orange
        
        m_tablePreview->setItem(row, 0, itemType);
        m_tablePreview->setItem(row, 1, itemOrig);
        m_tablePreview->setItem(row, 2, itemProp);
        m_tablePreview->setItem(row, 3, itemStatus);
    };

    for (const QString& path : m_filePaths) {
        QFileInfo fi(path);
        FileMetadata meta = MetadataExtractor::extract(path);

        // Preview Filename
        if (cFilename) {
            QString orig = fi.completeBaseName();
            QString proposed = computeCasedString(orig, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (orig != proposed) {
                addPreviewRow(fi.fileName() + " (Name)", orig, proposed, "Rename Pending");
            }
        }

        // Preview Title
        if (cTitle && !meta.title.isEmpty()) {
            QString orig = meta.title;
            QString proposed = computeCasedString(orig, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (orig != proposed) {
                addPreviewRow(fi.fileName() + " [Title]", orig, proposed, "Tag Write Pending");
            }
        }

        // Preview Artist
        if (cArtist && !meta.artist.isEmpty()) {
            QString orig = meta.artist;
            QString proposed = computeCasedString(orig, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (orig != proposed) {
                addPreviewRow(fi.fileName() + " [Artist]", orig, proposed, "Tag Write Pending");
            }
        }

        // Preview Album
        if (cAlbum && !meta.album.isEmpty()) {
            QString orig = meta.album;
            QString proposed = computeCasedString(orig, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (orig != proposed) {
                addPreviewRow(fi.fileName() + " [Album]", orig, proposed, "Tag Write Pending");
            }
        }
    }

    m_lblStatus->setText(QString("Found %1 proposed adjustments.").arg(m_tablePreview->rowCount()));
}

void MetadataCasingDialog::onApply() {
    // Save selections
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("casing/target_filename", m_chkFilename->isChecked());
    settings.setValue("casing/target_title", m_chkTitle->isChecked());
    settings.setValue("casing/target_artist", m_chkArtist->isChecked());
    settings.setValue("casing/target_album", m_chkAlbum->isChecked());

    QString mode = "title";
    if (m_radUppercase->isChecked()) mode = "upper";
    else if (m_radLowercase->isChecked()) mode = "lower";
    else if (m_radSentenceCase->isChecked()) mode = "sentence";
    settings.setValue("casing/style", mode);

    settings.setValue("casing/clean_underscores", m_chkUnderscores->isChecked());
    settings.setValue("casing/clean_url_escapes", m_chkUrlEscapes->isChecked());
    settings.setValue("casing/clean_spaces", m_chkCleanSpaces->isChecked());

    bool cFilename = m_chkFilename->isChecked();
    bool cTitle = m_chkTitle->isChecked();
    bool cArtist = m_chkArtist->isChecked();
    bool cAlbum = m_chkAlbum->isChecked();

    bool cleanUnder = m_chkUnderscores->isChecked();
    bool cleanUrl = m_chkUrlEscapes->isChecked();
    bool cleanSpaces = m_chkCleanSpaces->isChecked();

    m_progress->setVisible(true);
    m_progress->setValue(0);
    
    bool isDryRun = m_chkDryRun->isChecked();
    m_lblStatus->setText(isDryRun ? "Simulating..." : "Processing...");

    int successCount = 0;
    int renameCount = 0;
    int tagCount = 0;
    int total = m_filePaths.size();

    for (int i = 0; i < total; ++i) {
        QString path = m_filePaths[i];
        QFileInfo fi(path);
        FileMetadata meta = MetadataExtractor::extract(path);
        bool modified = false;

        QString newTitle = meta.title;
        QString newArtist = meta.artist;
        QString newAlbum = meta.album;

        if (cTitle && !meta.title.isEmpty()) {
            QString proposed = computeCasedString(meta.title, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (meta.title != proposed) {
                newTitle = proposed;
                modified = true;
            }
        }
        if (cArtist && !meta.artist.isEmpty()) {
            QString proposed = computeCasedString(meta.artist, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (meta.artist != proposed) {
                newArtist = proposed;
                modified = true;
            }
        }
        if (cAlbum && !meta.album.isEmpty()) {
            QString proposed = computeCasedString(meta.album, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (meta.album != proposed) {
                newAlbum = proposed;
                modified = true;
            }
        }

        // Write Tags
        if (modified) {
            tagCount++;
            if (!isDryRun) {
                QString ext = fi.suffix().toLower();
                if (ext == "mp3") {
                    TagEditorDialog::writeMp3Tags(path, newTitle, newArtist, newAlbum, meta.genre, meta.year,
                                                 meta.albumArtist, meta.discNumber, false, false, QByteArray(), "image/jpeg",
                                                 meta.track, meta.trackTotal, meta.discTotal, meta.composer, meta.bpm, meta.comment);
                } else if (ext == "flac") {
                    TagEditorDialog::writeFlacTags(path, newTitle, newArtist, newAlbum, meta.genre, meta.year,
                                                  meta.albumArtist, meta.discNumber, false,
                                                  meta.track, meta.trackTotal, meta.discTotal, meta.composer, meta.bpm, meta.comment);
                }
            }
        }

        // Rename Filename
        if (cFilename) {
            QString origName = fi.completeBaseName();
            QString proposedName = computeCasedString(origName, mode, cleanUnder, cleanUrl, cleanSpaces);
            if (origName != proposedName) {
                renameCount++;
                if (!isDryRun) {
                    QString newPath = fi.absolutePath() + "/" + proposedName + "." + fi.suffix();
                    QFile::rename(path, newPath);
                }
            }
        }

        successCount++;
        m_progress->setValue((i + 1) * 100 / total);
        QCoreApplication::processEvents();
    }

    if (isDryRun) {
        QMessageBox::information(this, "Dry-Run Simulation Complete",
            QString("Dry-Run simulation successful!\n\n"
                    "- Filenames that would be renamed: %1\n"
                    "- Metadata tags that would be updated: %2\n\n"
                    "No actual files on disk were modified.")
            .arg(renameCount).arg(tagCount));
        m_progress->setVisible(false);
        m_lblStatus->setText(QString("Simulation complete. %1 changes pending.").arg(renameCount + tagCount));
    } else {
        QMessageBox::information(this, "Batch Casing Complete", QString("Successfully processed %1 file(s).").arg(successCount));
        accept();
    }
}
