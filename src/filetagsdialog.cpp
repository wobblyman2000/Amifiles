#include "filetagsdialog.h"
#include "tagmanager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCompleter>
#include <QStringListModel>
#include <QFileInfo>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>

FileTagsDialog::FileTagsDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("Edit File Tags & Badges");
    resize(450, 300);
    setupUI();
}

void FileTagsDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; }"
        "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton#btnSave { background-color: #89b4fa; color: #11111b; }"
        "QPushButton#btnSave:hover { background-color: #b4befe; }"
    );

    QLabel* infoLabel = new QLabel(this);
    if (m_filePaths.size() == 1) {
        infoLabel->setText(QString("Editing: <b>%1</b>").arg(QFileInfo(m_filePaths.first()).fileName()));
    } else {
        infoLabel->setText(QString("Editing tags for <b>%1 files</b>").arg(m_filePaths.size()));
    }
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(10);

    m_tagEdit = new QLineEdit(this);
    m_tagEdit->setPlaceholderText("Comma-separated tags (e.g. Work, Urgent)");
    m_tagEdit->setClearButtonEnabled(true);
    form->addRow("Tags:", m_tagEdit);

    m_colorCombo = new QComboBox(this);
    m_colorCombo->addItems({"None", "Red", "Orange", "Yellow", "Green", "Blue", "Purple"});
    form->addRow("Badge Color:", m_colorCombo);

    m_ratingCombo = new QComboBox(this);
    m_ratingCombo->addItems({"No Rating", "1 Star ★", "2 Stars ★★", "3 Stars ★★★", "4 Stars ★★★★", "5 Stars ★★★★★"});
    form->addRow("Rating:", m_ratingCombo);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText("Enter a text comment/note about this file");
    m_commentEdit->setClearButtonEnabled(true);
    form->addRow("Comment:", m_commentEdit);

    mainLayout->addLayout(form);

    // Populate initial values from the first file
    if (!m_filePaths.isEmpty()) {
        QString firstPath = m_filePaths.first();
        QStringList initialTags = TagManager::instance().getFileTags(firstPath);
        m_tagEdit->setText(initialTags.join(", "));

        QString col = TagManager::instance().getFileColor(firstPath);
        if (!col.isEmpty()) {
            col = col.left(1).toUpper() + col.mid(1).toLower();
        } else {
            col = "None";
        }
        int colIdx = m_colorCombo->findText(col);
        if (colIdx != -1) {
            m_colorCombo->setCurrentIndex(colIdx);
        }

        int r = TagManager::instance().getFileRating(firstPath);
        m_ratingCombo->setCurrentIndex(qBound(0, r, 5));

        m_commentEdit->setText(TagManager::instance().getFileComment(firstPath));
    }

    // Set up QCompleter with multi-word comma completion
    m_completer = new QCompleter(this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    if (m_completer->popup()) {
        m_completer->popup()->setStyleSheet("background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a;");
    }
    m_tagEdit->setCompleter(m_completer);

    connect(m_tagEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        int lastComma = text.lastIndexOf(',');
        QString currentWord = (lastComma == -1) ? text.trimmed() : text.mid(lastComma + 1).trimmed();
        if (currentWord.isEmpty()) {
            m_completer->popup()->hide();
        } else {
            QStringList allTags = TagManager::instance().getAllTags();
            m_completer->setModel(new QStringListModel(allTags, m_completer));
            m_completer->setCompletionPrefix(currentWord);
            m_completer->complete();
        }
    });

    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated), this, [this](const QString& tag) {
        QString text = m_tagEdit->text();
        int lastComma = text.lastIndexOf(',');
        if (lastComma == -1) {
            m_tagEdit->setText(tag + ", ");
        } else {
            m_tagEdit->setText(text.left(lastComma + 1) + " " + tag + ", ");
        }
    });

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    QPushButton* btnManage = new QPushButton("Manage Tags...", this);
    connect(btnManage, &QPushButton::clicked, this, &FileTagsDialog::onManageTagsClicked);
    btnLayout->addWidget(btnManage);

    btnLayout->addStretch();

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    QPushButton* btnSave = new QPushButton("Save", this);
    btnSave->setObjectName("btnSave");
    connect(btnSave, &QPushButton::clicked, this, &FileTagsDialog::onSaveClicked);
    btnLayout->addWidget(btnSave);

    mainLayout->addLayout(btnLayout);
}

void FileTagsDialog::onSaveClicked() {
    QString tagsText = m_tagEdit->text();
    QStringList tagsList = tagsText.split(',', Qt::SkipEmptyParts);
    for (QString& tag : tagsList) {
        tag = tag.trimmed();
    }

    QString colorName = m_colorCombo->currentText().toLower();
    if (colorName == "none") {
        colorName = "";
    }

    int rating = m_ratingCombo->currentIndex(); // 0 is No Rating, matches index perfectly
    QString comment = m_commentEdit->text();

    // Apply to all selected files
    for (const QString& path : m_filePaths) {
        TagManager::instance().setFileTags(path, tagsList);
        TagManager::instance().setFileColor(path, colorName);
        TagManager::instance().setFileRating(path, rating);
        TagManager::instance().setFileComment(path, comment);
    }

    accept();
}

void FileTagsDialog::onManageTagsClicked() {
    ManageTagsDialog dlg(this);
    dlg.exec();
}

ManageTagsDialog::ManageTagsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Manage Global Tags");
    resize(400, 350);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QListWidget { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    m_listWidget = new QListWidget(this);
    mainLayout->addWidget(m_listWidget, 1);

    QVBoxLayout* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton* btnRename = new QPushButton("Rename...", this);
    connect(btnRename, &QPushButton::clicked, this, &ManageTagsDialog::onRenameClicked);
    btnLayout->addWidget(btnRename);

    QPushButton* btnDelete = new QPushButton("Delete Globally", this);
    connect(btnDelete, &QPushButton::clicked, this, &ManageTagsDialog::onDeleteClicked);
    btnLayout->addWidget(btnDelete);

    btnLayout->addStretch();

    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnClose);

    mainLayout->addLayout(btnLayout);

    refreshList();
}

void ManageTagsDialog::refreshList() {
    m_listWidget->clear();
    QStringList allTags = TagManager::instance().getAllTags();
    m_listWidget->addItems(allTags);
}

void ManageTagsDialog::onRenameClicked() {
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Rename Tag", "Please select a tag to rename.");
        return;
    }
    QString oldTag = item->text();
    bool ok;
    QString newTag = QInputDialog::getText(this, "Rename Tag", QString("Rename tag '%1' to:").arg(oldTag), QLineEdit::Normal, oldTag, &ok);
    if (ok && !newTag.trimmed().isEmpty() && newTag != oldTag) {
        TagManager::instance().renameTagGlobally(oldTag, newTag);
        refreshList();
    }
}

void ManageTagsDialog::onDeleteClicked() {
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Delete Tag", "Please select a tag to delete.");
        return;
    }
    QString tag = item->text();
    auto result = QMessageBox::question(this, "Delete Tag", QString("Are you sure you want to delete the tag '%1' globally from all files and folders?").arg(tag), QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes) {
        TagManager::instance().deleteTagGlobally(tag);
        refreshList();
    }
}
