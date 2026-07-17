#include "columnscustomizerdialog.h"
#include "theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>

ColumnsCustomizerDialog::ColumnsCustomizerDialog(const QList<CustomColumn>& currentCols, QWidget* parent)
    : QDialog(parent), m_cols(currentCols) {
    setWindowTitle("Details View Columns Customizer");
    resize(520, 480);
    setStyleSheet(Theme::getStylesheet());

    setupUI();
    populateActiveList();
    onTypeChanged(0);
}

void ColumnsCustomizerDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // Left Side: Active Columns List & Move Buttons
    QGroupBox* leftGroup = new QGroupBox("Active Details Columns", this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftGroup);
    m_listActive = new QListWidget(this);
    
    QHBoxLayout* listButtons = new QHBoxLayout();
    m_btnUp = new QPushButton("Move Up", this);
    m_btnDown = new QPushButton("Move Down", this);
    m_btnRemove = new QPushButton("Remove", this);
    listButtons->addWidget(m_btnUp);
    listButtons->addWidget(m_btnDown);
    listButtons->addWidget(m_btnRemove);
    listButtons->addStretch();

    leftLayout->addWidget(m_listActive);
    leftLayout->addLayout(listButtons);
    mainLayout->addWidget(leftGroup, 2);

    connect(m_btnUp, &QPushButton::clicked, this, &ColumnsCustomizerDialog::onMoveUp);
    connect(m_btnDown, &QPushButton::clicked, this, &ColumnsCustomizerDialog::onMoveDown);
    connect(m_btnRemove, &QPushButton::clicked, this, &ColumnsCustomizerDialog::onRemoveColumn);

    // Right Side: Add New Column Form & Save Actions
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(12);

    QGroupBox* rightGroup = new QGroupBox("Add Custom Column", this);
    QFormLayout* form = new QFormLayout(rightGroup);
    form->setSpacing(8);

    m_txtName = new QLineEdit(this);
    m_txtName->setPlaceholderText("Header Name (e.g. Hot Dogs)");
    form->addRow("Header Name:", m_txtName);

    m_comboType = new QComboBox(this);
    m_comboType->addItems({"Metadata", "Annotation", "CustomText", "EmbeddedArtwork", "BuiltIn"});
    form->addRow("Source Type:", m_comboType);

    m_comboKeySelect = new QComboBox(this);
    form->addRow("Source Key:", m_comboKeySelect);

    m_txtKeyInput = new QLineEdit(this);
    m_txtKeyInput->setPlaceholderText("Enter custom metadata key name");
    m_txtKeyInput->hide();
    form->addRow("Custom Key:", m_txtKeyInput);

    m_btnAdd = new QPushButton("Add Column", this);
    m_btnAdd->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; }");
    form->addRow("", m_btnAdd);

    rightLayout->addWidget(rightGroup);

    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColumnsCustomizerDialog::onTypeChanged);
    connect(m_btnAdd, &QPushButton::clicked, this, &ColumnsCustomizerDialog::onAddColumn);

    // Bottom Action Buttons
    m_btnReset = new QPushButton("Reset to Defaults", this);
    connect(m_btnReset, &QPushButton::clicked, this, &ColumnsCustomizerDialog::onResetDefaults);
    rightLayout->addWidget(m_btnReset);

    rightLayout->addStretch(1);

    QHBoxLayout* actionsLayout = new QHBoxLayout();
    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    actionsLayout->addWidget(m_btnCancel);

    m_btnSave = new QPushButton("Save & Apply", this);
    m_btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; }");
    connect(m_btnSave, &QPushButton::clicked, this, &QDialog::accept);
    actionsLayout->addWidget(m_btnSave);

    rightLayout->addLayout(actionsLayout);
    mainLayout->addLayout(rightLayout, 1);
}

void ColumnsCustomizerDialog::onTypeChanged(int index) {
    Q_UNUSED(index);
    updateKeyField();
}

void ColumnsCustomizerDialog::updateKeyField() {
    QString type = m_comboType->currentText();
    m_comboKeySelect->clear();
    m_comboKeySelect->show();
    m_txtKeyInput->hide();

    if (type == "Metadata") {
        m_comboKeySelect->addItems({"Title", "Artist", "Album", "Genre", "Year", "Track", "Duration", "Bitrate", "Resolution", "Codec"});
    } else if (type == "Annotation") {
        m_comboKeySelect->addItems({"Tags", "Rating", "Comment"});
    } else if (type == "BuiltIn") {
        m_comboKeySelect->addItems({"Size", "Type", "Date"});
    } else if (type == "CustomText") {
        m_comboKeySelect->hide();
        m_txtKeyInput->show();
    } else if (type == "EmbeddedArtwork") {
        m_comboKeySelect->hide();
    }
}

void ColumnsCustomizerDialog::populateActiveList() {
    m_listActive->clear();
    for (const auto& col : m_cols) {
        QString details = QString("%1 (%2: %3)").arg(col.name).arg(col.type).arg(col.key);
        if (col.type == "EmbeddedArtwork") {
            details = QString("%1 (Artwork)").arg(col.name);
        }
        QListWidgetItem* item = new QListWidgetItem(m_listActive);
        item->setText(details);
        item->setData(Qt::UserRole, col.name); // Store name to map back
    }
}

void ColumnsCustomizerDialog::onAddColumn() {
    QString name = m_txtName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please specify a column header name.");
        return;
    }

    CustomColumn col;
    col.name = name;
    col.type = m_comboType->currentText();

    if (col.type == "CustomText") {
        col.key = m_txtKeyInput->text().trimmed();
        if (col.key.isEmpty()) {
            QMessageBox::warning(this, "Input Required", "Please enter a key name for the custom text attribute.");
            return;
        }
    } else if (col.type == "EmbeddedArtwork") {
        col.key = "";
    } else {
        col.key = m_comboKeySelect->currentText();
    }

    m_cols.append(col);
    populateActiveList();
    
    // Clear inputs
    m_txtName->clear();
    m_txtKeyInput->clear();
}

void ColumnsCustomizerDialog::onRemoveColumn() {
    int row = m_listActive->currentRow();
    if (row >= 0 && row < m_cols.size()) {
        m_cols.removeAt(row);
        populateActiveList();
    }
}

void ColumnsCustomizerDialog::onMoveUp() {
    int row = m_listActive->currentRow();
    if (row > 0 && row < m_cols.size()) {
        m_cols.swapItemsAt(row, row - 1);
        populateActiveList();
        m_listActive->setCurrentRow(row - 1);
    }
}

void ColumnsCustomizerDialog::onMoveDown() {
    int row = m_listActive->currentRow();
    if (row >= 0 && row < m_cols.size() - 1) {
        m_cols.swapItemsAt(row, row + 1);
        populateActiveList();
        m_listActive->setCurrentRow(row + 1);
    }
}

void ColumnsCustomizerDialog::onResetDefaults() {
    m_cols = {
        {"Title", "Metadata", "Title"},
        {"Artist", "Metadata", "Artist"},
        {"Album", "Metadata", "Album"},
        {"Genre", "Metadata", "Genre"},
        {"Duration", "Metadata", "Duration"},
        {"Tags", "Annotation", "Tags"},
        {"Rating", "Annotation", "Rating"},
        {"Comment", "Annotation", "Comment"}
    };
    populateActiveList();
}
