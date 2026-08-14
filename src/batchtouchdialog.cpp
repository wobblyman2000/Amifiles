#include "batchtouchdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileInfo>
#include <QDirIterator>
#include <QFile>
#include <QMessageBox>
#include <utime.h>

BatchTouchDialog::BatchTouchDialog(const QStringList& filePaths, QWidget* parent)
    : QDialog(parent), m_filePaths(filePaths) {
    setWindowTitle("📅 Batch Touch Timestamps & Attributes");
    resize(480, 320);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; } "
                  "QLabel { color: #cdd6f4; font-size: 13px; } "
                  "QDateTimeEdit { background-color: #313244; color: #cdd6f4; border-radius: 6px; padding: 4px; } "
                  "QComboBox { background-color: #313244; color: #cdd6f4; border-radius: 6px; padding: 4px; } "
                  "QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 6px; padding: 8px 16px; } "
                  "QPushButton:hover { background-color: #b4befe; }");

    setupUI();
}

void BatchTouchDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QLabel* header = new QLabel(QString("Target: <b>%1 files/folders selected</b>").arg(m_filePaths.size()), this);
    mainLayout->addWidget(header);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(10);

    // Modified Date
    m_chkModified = new QCheckBox("Set Last Modified Date:", this);
    m_chkModified->setChecked(true);
    m_dtModified = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_dtModified->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    form->addRow(m_chkModified, m_dtModified);

    // Access Date
    m_chkAccessed = new QCheckBox("Set Last Access Date:", this);
    m_dtAccessed = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_dtAccessed->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    form->addRow(m_chkAccessed, m_dtAccessed);

    // Preset Options
    m_comboPreset = new QComboBox(this);
    m_comboPreset->addItem("Custom Date & Time", 0);
    m_comboPreset->addItem("Touch to Now (Current Time)", 1);
    m_comboPreset->addItem("Shift +1 Hour", 2);
    m_comboPreset->addItem("Shift -1 Day", 3);
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx == 1) {
            m_dtModified->setDateTime(QDateTime::currentDateTime());
            m_dtAccessed->setDateTime(QDateTime::currentDateTime());
        } else if (idx == 2) {
            m_dtModified->setDateTime(m_dtModified->dateTime().addSecs(3600));
        } else if (idx == 3) {
            m_dtModified->setDateTime(m_dtModified->dateTime().addDays(-1));
        }
    });
    form->addRow("Quick Preset:", m_comboPreset);

    // Recursive Option
    m_chkRecursive = new QCheckBox("Apply recursively to contents of selected folders", this);
    m_chkRecursive->setChecked(true);
    form->addRow("", m_chkRecursive);

    mainLayout->addLayout(form);

    m_lblStatus = new QLabel("", this);
    mainLayout->addWidget(m_lblStatus);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnApply = new QPushButton("📅 Touch Timestamps", this);
    connect(btnApply, &QPushButton::clicked, this, &BatchTouchDialog::onApplyClicked);
    btnLayout->addWidget(btnApply);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setStyleSheet("background-color: #313244; color: #cdd6f4;");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(btnLayout);
}

void BatchTouchDialog::onApplyClicked() {
    if (m_filePaths.isEmpty()) return;

    int touchedCount = 0;

    auto applyTimesToFile = [this, &touchedCount](const QString& path) {
        struct utimbuf buf;
        QFileInfo fi(path);

        time_t modTime = m_chkModified->isChecked() ? m_dtModified->dateTime().toSecsSinceEpoch() : fi.lastModified().toSecsSinceEpoch();
        time_t accTime = m_chkAccessed->isChecked() ? m_dtAccessed->dateTime().toSecsSinceEpoch() : fi.lastRead().toSecsSinceEpoch();

        buf.actime = accTime;
        buf.modtime = modTime;

        if (utime(path.toLocal8Bit().constData(), &buf) == 0) {
            touchedCount++;
        }
    };

    for (const QString& path : m_filePaths) {
        applyTimesToFile(path);

        if (m_chkRecursive->isChecked() && QFileInfo(path).isDir()) {
            QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                applyTimesToFile(it.filePath());
            }
        }
    }

    QMessageBox::information(this, "Batch Touch Complete", QString("Successfully updated timestamps for %1 files/directories.").arg(touchedCount));
    accept();
}
