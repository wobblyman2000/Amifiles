#include "filesplitterdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

FileSplitterDialog::FileSplitterDialog(const QString& targetPath, Mode mode, QWidget* parent)
    : QDialog(parent), m_targetPath(targetPath), m_mode(mode) {
    setWindowTitle(mode == Split ? "✂️ File Splitter" : "🔗 File Recombiner / Joiner");
    resize(480, 260);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; } "
                  "QLabel { color: #cdd6f4; font-size: 13px; } "
                  "QComboBox, QSpinBox { background-color: #313244; color: #cdd6f4; border-radius: 6px; padding: 4px; } "
                  "QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 6px; padding: 8px 16px; } "
                  "QPushButton:hover { background-color: #b4befe; } "
                  "QProgressBar { border: 1px solid #313244; border-radius: 6px; text-align: center; color: #cdd6f4; background-color: #181825; } "
                  "QProgressBar::chunk { background-color: #a6e3a1; border-radius: 5px; }");

    setupUI();
}

void FileSplitterDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QFileInfo fi(m_targetPath);
    QLabel* header = new QLabel(QString("Target: <b>%1</b> (%2)")
                                .arg(fi.fileName())
                                .arg(QString::number(fi.size() / (1024.0 * 1024.0), 'f', 2) + " MB"), this);
    mainLayout->addWidget(header);

    if (m_mode == Split) {
        QFormLayout* form = new QFormLayout();
        m_comboChunkSize = new QComboBox(this);
        m_comboChunkSize->addItem("50 MB Chunks", 50);
        m_comboChunkSize->addItem("100 MB Chunks", 100);
        m_comboChunkSize->addItem("700 MB (CD Size)", 700);
        m_comboChunkSize->addItem("4000 MB (FAT32 Limit)", 4000);
        m_comboChunkSize->addItem("Custom MB...", -1);

        m_spinCustomMb = new QSpinBox(this);
        m_spinCustomMb->setRange(1, 100000);
        m_spinCustomMb->setValue(25);
        m_spinCustomMb->setEnabled(false);

        connect(m_comboChunkSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
            m_spinCustomMb->setEnabled(m_comboChunkSize->currentData().toInt() == -1);
        });

        form->addRow("Chunk Size Preset:", m_comboChunkSize);
        form->addRow("Custom Chunk Size (MB):", m_spinCustomMb);
        mainLayout->addLayout(form);
    } else {
        QLabel* lblInfo = new QLabel("Will scan for sequential volume parts (.001, .002...) and join them into a single file.", this);
        lblInfo->setWordWrap(true);
        mainLayout->addWidget(lblInfo);
    }

    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    mainLayout->addWidget(m_progress);

    m_lblStatus = new QLabel("Ready.", this);
    mainLayout->addWidget(m_lblStatus);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnExecute = new QPushButton(m_mode == Split ? "✂️ Split File" : "🔗 Join File Parts", this);
    connect(m_btnExecute, &QPushButton::clicked, this, &FileSplitterDialog::onExecuteClicked);
    btnLayout->addWidget(m_btnExecute);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setStyleSheet("background-color: #313244; color: #cdd6f4;");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    mainLayout->addLayout(btnLayout);
}

void FileSplitterDialog::onExecuteClicked() {
    if (m_mode == Split) executeSplit();
    else executeJoin();
}

void FileSplitterDialog::executeSplit() {
    QFile srcFile(m_targetPath);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        m_lblStatus->setText("Error: Cannot open source file.");
        return;
    }

    qint64 chunkSizeMb = m_comboChunkSize->currentData().toInt();
    if (chunkSizeMb == -1) chunkSizeMb = m_spinCustomMb->value();
    qint64 chunkSizeBytes = chunkSizeMb * 1024 * 1024;

    qint64 totalSize = srcFile.size();
    int partCount = (totalSize + chunkSizeBytes - 1) / chunkSizeBytes;

    m_progress->setMaximum(partCount);
    m_progress->setValue(0);
    m_btnExecute->setEnabled(false);

    QByteArray buffer(64 * 1024, 0);
    for (int part = 1; part <= partCount; ++part) {
        QString partPath = QString("%1.%2").arg(m_targetPath).arg(part, 3, 10, QChar('0'));
        QFile partFile(partPath);
        if (!partFile.open(QIODevice::WriteOnly)) {
            m_lblStatus->setText("Error creating chunk: " + partPath);
            return;
        }

        qint64 bytesWritten = 0;
        while (bytesWritten < chunkSizeBytes && !srcFile.atEnd()) {
            qint64 toRead = qMin((qint64)buffer.size(), chunkSizeBytes - bytesWritten);
            qint64 read = srcFile.read(buffer.data(), toRead);
            if (read <= 0) break;
            partFile.write(buffer.constData(), read);
            bytesWritten += read;
        }
        partFile.close();

        m_progress->setValue(part);
        m_lblStatus->setText(QString("Created part %1 of %2...").arg(part).arg(partCount));
    }

    srcFile.close();
    m_lblStatus->setText("🎉 Split Complete!");
    QMessageBox::information(this, "Split Complete", QString("Successfully split into %1 volume parts.").arg(partCount));
    accept();
}

void FileSplitterDialog::executeJoin() {
    QFileInfo fi(m_targetPath);
    QString basePath = m_targetPath;
    if (basePath.endsWith(".001")) {
        basePath.chop(4);
    }

    QFile outFile(basePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        m_lblStatus->setText("Error creating output file.");
        return;
    }

    int part = 1;
    QByteArray buffer(64 * 1024, 0);
    while (true) {
        QString partPath = QString("%1.%2").arg(basePath).arg(part, 3, 10, QChar('0'));
        if (!QFile::exists(partPath)) break;

        QFile partFile(partPath);
        if (!partFile.open(QIODevice::ReadOnly)) break;

        while (!partFile.atEnd()) {
            qint64 read = partFile.read(buffer.data(), buffer.size());
            if (read <= 0) break;
            outFile.write(buffer.constData(), read);
        }
        partFile.close();
        part++;
    }

    outFile.close();
    m_lblStatus->setText("🎉 Join Complete!");
    QMessageBox::information(this, "Join Complete", QString("Successfully recombined %1 parts into '%2'.").arg(part - 1).arg(QFileInfo(basePath).fileName()));
    accept();
}
