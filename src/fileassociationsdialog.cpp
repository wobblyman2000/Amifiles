#include "fileassociationsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

FileAssociationsDialog::FileAssociationsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("🔗 Custom File Type Associations & Handlers");
    resize(700, 450);
    setupUI();
    loadAssociations();
}

void FileAssociationsDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QTableWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 6px; padding: 4px; }"
        "QTableWidget::item { padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
    );

    QLabel* lblHeader = new QLabel("Override desktop defaults and assign custom shell commands/binaries to file extensions (e.g. <code>.mod</code> -> <code>xmp %1</code>):", this);
    lblHeader->setWordWrap(true);
    mainLayout->addWidget(lblHeader);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Extension (e.g. mod, adf, flac)", "Custom Command / Executable (%1 = File Path)"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mainLayout->addWidget(m_table, 1);

    QHBoxLayout* addLayout = new QHBoxLayout();
    m_txtExt = new QLineEdit(this);
    m_txtExt->setPlaceholderText("Extension (e.g. mod)");

    m_txtCmd = new QLineEdit(this);
    m_txtCmd->setPlaceholderText("Command (e.g. xmp \"%1\" or vlc \"%1\")");

    m_btnAdd = new QPushButton("➕ Add Association", this);
    connect(m_btnAdd, &QPushButton::clicked, this, &FileAssociationsDialog::onAddAssociation);

    addLayout->addWidget(m_txtExt);
    addLayout->addWidget(m_txtCmd, 1);
    addLayout->addWidget(m_btnAdd);
    mainLayout->addLayout(addLayout);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnDelete = new QPushButton("🗑️ Delete Selected", this);
    connect(m_btnDelete, &QPushButton::clicked, this, &FileAssociationsDialog::onDeleteSelected);
    btnLayout->addWidget(m_btnDelete);

    btnLayout->addStretch(1);

    m_btnSave = new QPushButton("💾 Save Associations", this);
    m_btnSave->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; } QPushButton:hover { background-color: #b4befe; }");
    connect(m_btnSave, &QPushButton::clicked, this, &FileAssociationsDialog::onSave);
    btnLayout->addWidget(m_btnSave);

    mainLayout->addLayout(btnLayout);
}

void FileAssociationsDialog::loadAssociations() {
    m_table->setRowCount(0);
    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("associations");
    QStringList keys = settings.childKeys();
    for (const QString& ext : keys) {
        QString cmd = settings.value(ext).toString();
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(ext));
        m_table->setItem(row, 1, new QTableWidgetItem(cmd));
    }
    settings.endGroup();
}

void FileAssociationsDialog::onAddAssociation() {
    QString ext = m_txtExt->text().trimmed().toLower();
    if (ext.startsWith(".")) ext = ext.mid(1);
    QString cmd = m_txtCmd->text().trimmed();

    if (ext.isEmpty() || cmd.isEmpty()) {
        QMessageBox::warning(this, "Add Association", "Please specify both extension and command.");
        return;
    }

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(ext));
    m_table->setItem(row, 1, new QTableWidgetItem(cmd));

    m_txtExt->clear();
    m_txtCmd->clear();
}

void FileAssociationsDialog::onDeleteSelected() {
    int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
    }
}

void FileAssociationsDialog::onSave() {
    QSettings settings("Amifiles", "Amifiles");
    settings.remove("associations");
    settings.beginGroup("associations");
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString ext = m_table->item(r, 0)->text().trimmed().toLower();
        QString cmd = m_table->item(r, 1)->text().trimmed();
        if (!ext.isEmpty() && !cmd.isEmpty()) {
            settings.setValue(ext, cmd);
        }
    }
    settings.endGroup();
    QMessageBox::information(this, "File Associations", "File type associations saved successfully.");
    accept();
}

QString FileAssociationsDialog::getCustomHandler(const QString& extension) {
    QString ext = extension.toLower();
    if (ext.startsWith(".")) ext = ext.mid(1);

    QSettings settings("Amifiles", "Amifiles");
    return settings.value("associations/" + ext).toString();
}

bool FileAssociationsDialog::launchFile(const QString& filePath) {
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    QString cmd = getCustomHandler(ext);

    if (!cmd.isEmpty()) {
        QString finalCmd = cmd;
        if (finalCmd.contains("%1")) {
            finalCmd = finalCmd.arg(filePath);
        } else {
            finalCmd += " \"" + filePath + "\"";
        }
        return QProcess::startDetached("/bin/sh", QStringList() << "-c" << finalCmd);
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}
