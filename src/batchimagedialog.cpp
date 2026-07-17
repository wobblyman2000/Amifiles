#include "batchimagedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QFileInfo>
#include <QDir>

BatchImageWorker::BatchImageWorker(const QStringList& files, const QString& destDir, const ImageProcessOptions& opts, QObject* parent)
    : QThread(parent), m_files(files), m_destDir(destDir), m_opts(opts) {}

void BatchImageWorker::run() {
    int successCount = 0;
    int failCount = 0;
    int total = m_files.size();

    for (int i = 0; i < total; ++i) {
        QString file = m_files[i];
        QFileInfo info(file);
        
        emit progress(i, total, info.fileName());

        QString targetDest = m_destDir.isEmpty() ? file : QDir(m_destDir).filePath(info.fileName());
        if (ImageProcessor::processImage(file, targetDest, m_opts)) {
            successCount++;
        } else {
            failCount++;
        }
    }
    emit finished(successCount, failCount);
}


BatchImageDialog::BatchImageDialog(const QStringList& selectedFiles, const QString& currentDir, QWidget* parent)
    : QDialog(parent), m_files(selectedFiles), m_currentDir(currentDir) {
    setupUI();
}

void BatchImageDialog::setupUI() {
    setWindowTitle("Batch Image Processing Suite");
    resize(900, 550);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Left Side: File List & Action Buttons
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(8);

    QLabel* lblFiles = new QLabel(QString("Images Queue (%1 files):").arg(m_files.size()), this);
    lblFiles->setStyleSheet("font-weight: bold; color: #f38ba8;");
    leftLayout->addWidget(lblFiles);

    m_filesList = new QListWidget(this);
    m_filesList->setStyleSheet("QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }");
    for (const QString& file : m_files) {
        m_filesList->addItem(QFileInfo(file).fileName());
    }
    leftLayout->addWidget(m_filesList, 2);

    QGroupBox* destGroup = new QGroupBox("Destination Directory", this);
    destGroup->setStyleSheet("QGroupBox { color: #f9e2af; font-weight: bold; border: 1px solid #313244; margin-top: 10px; padding: 10px; border-radius: 4px; }");
    QVBoxLayout* destLayout = new QVBoxLayout(destGroup);
    m_destDirEdit = new QLineEdit(destGroup);
    m_destDirEdit->setPlaceholderText("Overwrite original files (default)");
    m_destDirEdit->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }");
    QPushButton* btnDest = new QPushButton("Browse...", destGroup);
    btnDest->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; padding: 4px 10px; }");
    connect(btnDest, &QPushButton::clicked, this, &BatchImageDialog::onSelectDestDir);
    
    destLayout->addWidget(m_destDirEdit);
    destLayout->addWidget(btnDest);
    leftLayout->addWidget(destGroup);

    mainLayout->addLayout(leftLayout, 1);

    // Right Side: Scrollable Configuration Options
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background-color: transparent;");

    QWidget* optsWidget = new QWidget(scroll);
    QVBoxLayout* optsLayout = new QVBoxLayout(optsWidget);
    optsLayout->setContentsMargins(0, 0, 0, 0);
    optsLayout->setSpacing(12);

    // 1. Resize Settings
    QGroupBox* grpResize = new QGroupBox("1. Resize Image Dimensions", optsWidget);
    grpResize->setStyleSheet("QGroupBox { color: #89b4fa; font-weight: bold; border: 1px solid #313244; margin-top: 10px; padding: 10px; border-radius: 4px; }");
    QFormLayout* layResize = new QFormLayout(grpResize);
    m_chkResize = new QCheckBox("Enable Resizing", grpResize);
    m_spinWidth = new QSpinBox(grpResize);
    m_spinWidth->setRange(10, 10000);
    m_spinWidth->setValue(1280);
    m_spinHeight = new QSpinBox(grpResize);
    m_spinHeight->setRange(10, 10000);
    m_spinHeight->setValue(720);
    m_chkAspect = new QCheckBox("Preserve Aspect Ratio", grpResize);
    m_chkAspect->setChecked(true);
    layResize->addRow(m_chkResize);
    layResize->addRow("Width (px):", m_spinWidth);
    layResize->addRow("Height (px):", m_spinHeight);
    layResize->addRow(m_chkAspect);
    optsLayout->addWidget(grpResize);

    // 2. Format Conversion
    QGroupBox* grpConvert = new QGroupBox("2. Target File Format Conversion", optsWidget);
    grpConvert->setStyleSheet("QGroupBox { color: #a6e3a1; font-weight: bold; border: 1px solid #313244; margin-top: 10px; padding: 10px; border-radius: 4px; }");
    QFormLayout* layConvert = new QFormLayout(grpConvert);
    m_chkConvert = new QCheckBox("Enable Format Conversion", grpConvert);
    m_comboFormat = new QComboBox(grpConvert);
    m_comboFormat->addItems({"jpg", "png", "webp"});
    layConvert->addRow(m_chkConvert);
    layConvert->addRow("Target Format:", m_comboFormat);
    optsLayout->addWidget(grpConvert);

    // 3. Watermark Settings
    QGroupBox* grpWatermark = new QGroupBox("3. Text Watermark Settings", optsWidget);
    grpWatermark->setStyleSheet("QGroupBox { color: #fab387; font-weight: bold; border: 1px solid #313244; margin-top: 10px; padding: 10px; border-radius: 4px; }");
    QFormLayout* layWatermark = new QFormLayout(grpWatermark);
    m_chkWatermark = new QCheckBox("Enable Watermark Overlay", grpWatermark);
    m_txtWatermark = new QLineEdit("Amifiles Watermark", grpWatermark);
    m_txtWatermark->setStyleSheet("QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; }");
    m_comboPosition = new QComboBox(grpWatermark);
    m_comboPosition->addItems({"bottom-right", "bottom-left", "top-right", "top-left", "center"});
    m_spinWatermarkSize = new QSpinBox(grpWatermark);
    m_spinWatermarkSize->setRange(6, 120);
    m_spinWatermarkSize->setValue(28);
    m_sliderOpacity = new QSlider(Qt::Horizontal, grpWatermark);
    m_sliderOpacity->setRange(10, 100);
    m_sliderOpacity->setValue(60);
    m_btnColor = new QPushButton("Pick Color...", grpWatermark);
    m_btnColor->setStyleSheet("QPushButton { background-color: #313244; color: #ffffff; }");
    connect(m_btnColor, &QPushButton::clicked, this, &BatchImageDialog::onSelectColor);

    layWatermark->addRow(m_chkWatermark);
    layWatermark->addRow("Watermark Text:", m_txtWatermark);
    layWatermark->addRow("Positioning:", m_comboPosition);
    layWatermark->addRow("Font Size (pt):", m_spinWatermarkSize);
    layWatermark->addRow("Opacity (%):", m_sliderOpacity);
    layWatermark->addRow("Text Color:", m_btnColor);
    optsLayout->addWidget(grpWatermark);

    // 4. Image Filters
    QGroupBox* grpFilters = new QGroupBox("4. Dynamic Color & Correction Filters", optsWidget);
    grpFilters->setStyleSheet("QGroupBox { color: #cba6f7; font-weight: bold; border: 1px solid #313244; margin-top: 10px; padding: 10px; border-radius: 4px; }");
    QVBoxLayout* layFilters = new QVBoxLayout(grpFilters);
    m_chkFilters = new QCheckBox("Enable Color Filters", grpFilters);
    m_chkGrayscale = new QCheckBox("Grayscale (Monochrome) filter", grpFilters);
    m_chkSepia = new QCheckBox("Sepia (Retro brownish) tone", grpFilters);
    m_chkInvert = new QCheckBox("Invert image color channels", grpFilters);
    
    QFormLayout* layAdj = new QFormLayout();
    m_sliderBrightness = new QSlider(Qt::Horizontal, grpFilters);
    m_sliderBrightness->setRange(-100, 100);
    m_sliderBrightness->setValue(0);
    m_sliderContrast = new QSlider(Qt::Horizontal, grpFilters);
    m_sliderContrast->setRange(-100, 100);
    m_sliderContrast->setValue(0);
    
    layAdj->addRow("Brightness offset:", m_sliderBrightness);
    layAdj->addRow("Contrast factor:", m_sliderContrast);

    layFilters->addWidget(m_chkFilters);
    layFilters->addWidget(m_chkGrayscale);
    layFilters->addWidget(m_chkSepia);
    layFilters->addWidget(m_chkInvert);
    layFilters->addLayout(layAdj);
    optsLayout->addWidget(grpFilters);

    scroll->setWidget(optsWidget);
    mainLayout->addWidget(scroll, 1);

    // Bottom Layout: Execution Progress & Trigger Buttons
    QVBoxLayout* bottomLayout = new QVBoxLayout();
    bottomLayout->setSpacing(6);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet("QProgressBar { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; text-align: center; color: #cdd6f4; }"
                                 "QProgressBar::chunk { background-color: #a6e3a1; }");
    m_progressBar->hide();
    
    m_statusLabel = new QLabel("Select operations and press Start to process images batch.", this);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-style: italic;");
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnStart = new QPushButton("Start Processing", this);
    m_btnStart->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 16px; } QPushButton:hover { background-color: #cba6f7; }");
    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }");
    
    connect(m_btnStart, &QPushButton::clicked, this, &BatchImageDialog::onStartClicked);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(m_statusLabel);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(btnClose);

    QVBoxLayout* outerLayout = new QVBoxLayout();
    outerLayout->addLayout(mainLayout);
    outerLayout->addWidget(m_progressBar);
    outerLayout->addLayout(btnLayout);
    setLayout(outerLayout);
}

void BatchImageDialog::onSelectDestDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Target Destination Folder", m_currentDir);
    if (!dir.isEmpty()) {
        m_destDir = dir;
        m_destDirEdit->setText(dir);
    }
}

void BatchImageDialog::onSelectColor() {
    QColor col = QColorDialog::getColor(m_watermarkColor, this, "Choose Watermark Text Color");
    if (col.isValid()) {
        m_watermarkColor = col;
        m_btnColor->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; }")
                                  .arg(col.name())
                                  .arg((col.lightness() > 128) ? "#000000" : "#ffffff"));
    }
}

ImageProcessOptions BatchImageDialog::getOptions() const {
    ImageProcessOptions opts;
    
    opts.resizeEnabled = m_chkResize->isChecked();
    opts.resizeWidth = m_spinWidth->value();
    opts.resizeHeight = m_spinHeight->value();
    opts.keepAspectRatio = m_chkAspect->isChecked();

    opts.convertEnabled = m_chkConvert->isChecked();
    opts.targetFormat = m_comboFormat->currentText();

    opts.watermarkEnabled = m_chkWatermark->isChecked();
    opts.watermarkText = m_txtWatermark->text().trimmed();
    opts.watermarkColor = m_watermarkColor;
    opts.watermarkSize = m_spinWatermarkSize->value();
    opts.watermarkOpacity = m_sliderOpacity->value() / 100.0;
    opts.watermarkPosition = m_comboPosition->currentText();

    opts.filtersEnabled = m_chkFilters->isChecked();
    opts.grayscale = m_chkGrayscale->isChecked();
    opts.sepia = m_chkSepia->isChecked();
    opts.invert = m_chkInvert->isChecked();
    opts.brightness = m_sliderBrightness->value();
    opts.contrast = m_sliderContrast->value();

    return opts;
}

void BatchImageDialog::onStartClicked() {
    if (m_files.isEmpty()) {
        QMessageBox::warning(this, "Empty Queue", "No files loaded in the processing queue.");
        return;
    }

    ImageProcessOptions opts = getOptions();
    m_btnStart->setEnabled(false);
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(m_files.size());
    m_progressBar->show();

    m_worker = new BatchImageWorker(m_files, m_destDir, opts, this);
    connect(m_worker, &BatchImageWorker::progress, this, &BatchImageDialog::onProgressUpdate);
    connect(m_worker, &BatchImageWorker::finished, this, &BatchImageDialog::onWorkerFinished);
    
    m_statusLabel->setText("Preparing worker thread...");
    m_worker->start();
}

void BatchImageDialog::onProgressUpdate(int current, int total, const QString& fileName) {
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("[%1/%2] Processing: %3").arg(current + 1).arg(total).arg(fileName));
}

void BatchImageDialog::onWorkerFinished(int successCount, int failCount) {
    m_progressBar->setValue(m_files.size());
    m_statusLabel->setText(QString("Batch completed: %1 succeeded, %2 failed.").arg(successCount).arg(failCount));
    m_btnStart->setEnabled(true);
    
    QMessageBox::information(this, "Batch Complete", QString("Bulk operations finished successfully:\n- Success: %1\n- Failed: %2").arg(successCount).arg(failCount));
    m_worker->deleteLater();
    m_worker = nullptr;
}
