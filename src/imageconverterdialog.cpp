#include "imageconverterdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileInfo>
#include <QImage>
#include <QTransform>
#include <QMessageBox>

ImageWorker::ImageWorker(const QStringList& inputPaths, const QString& format, int width, int height, bool keepAspect, int rotateAngle, int quality, QObject* parent)
    : QThread(parent), m_inputPaths(inputPaths), m_format(format), m_width(width), m_height(height), m_keepAspect(keepAspect), m_rotateAngle(rotateAngle), m_quality(quality) {}

void ImageWorker::run() {
    int total = m_inputPaths.size();
    int count = 0;

    for (int i = 0; i < total; ++i) {
        if (isInterruptionRequested()) return;

        QString path = m_inputPaths[i];
        emit progress((i * 100) / total, QFileInfo(path).fileName());

        QImage img;
        if (!img.load(path)) {
            continue; // Skip failed loads
        }

        // Apply rotation
        if (m_rotateAngle != 0) {
            QTransform trans;
            trans.rotate(m_rotateAngle);
            img = img.transformed(trans);
        }

        // Apply resize
        if (m_width > 0 && m_height > 0) {
            Qt::AspectRatioMode mode = m_keepAspect ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
            img = img.scaled(m_width, m_height, mode, Qt::SmoothTransformation);
        }

        // Write out file
        QFileInfo info(path);
        QString destPath = info.absolutePath() + "/" + info.baseName() + "_converted." + m_format.toLower();
        
        // Save using format and quality settings
        if (img.save(destPath, m_format.toUpper().toLatin1().constData(), m_quality)) {
            count++;
        }
    }

    emit finished(count);
}

ImageConverterDialog::ImageConverterDialog(const QStringList& inputPaths, QWidget* parent)
    : QDialog(parent), m_inputPaths(inputPaths) {
    setupUI();
}

ImageConverterDialog::~ImageConverterDialog() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->requestInterruption();
        m_worker->wait();
    }
}

void ImageConverterDialog::setupUI() {
    setWindowTitle("Batch Image Converter & Resizer");
    resize(460, 360);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QCheckBox { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QSpinBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QProgressBar { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #fab387; border-radius: 3px; }" // Peach chunk for images!
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_lblStatus = new QLabel(QString("Ready to process %1 selected image(s).").arg(m_inputPaths.size()), this);
    m_lblStatus->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(m_lblStatus);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(10);

    // Format selection
    grid->addWidget(new QLabel("Export Format:", this), 0, 0);
    m_comboFormat = new QComboBox(this);
    m_comboFormat->addItems({"JPEG", "PNG", "WEBP", "BMP"});
    grid->addWidget(m_comboFormat, 0, 1);

    // Resize controls
    m_chkResize = new QCheckBox("Resize Images", this);
    connect(m_chkResize, &QCheckBox::toggled, this, &ImageConverterDialog::onResizeToggled);
    grid->addWidget(m_chkResize, 1, 0, 1, 2);

    QHBoxLayout* sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel("Width:", this));
    m_spinWidth = new QSpinBox(this);
    m_spinWidth->setRange(1, 10000);
    m_spinWidth->setValue(800);
    m_spinWidth->setEnabled(false);
    sizeLayout->addWidget(m_spinWidth);

    sizeLayout->addWidget(new QLabel("Height:", this));
    m_spinHeight = new QSpinBox(this);
    m_spinHeight->setRange(1, 10000);
    m_spinHeight->setValue(600);
    m_spinHeight->setEnabled(false);
    sizeLayout->addWidget(m_spinHeight);

    grid->addLayout(sizeLayout, 2, 1);

    m_chkAspect = new QCheckBox("Keep Aspect Ratio", this);
    m_chkAspect->setChecked(true);
    m_chkAspect->setEnabled(false);
    grid->addWidget(m_chkAspect, 3, 1);

    // Rotation controls
    grid->addWidget(new QLabel("Rotate:", this), 4, 0);
    m_comboRotate = new QComboBox(this);
    m_comboRotate->addItems({"No Rotation", "90° Clockwise", "90° Counter-Clockwise", "180°"});
    grid->addWidget(m_comboRotate, 4, 1);

    // Quality slider
    m_lblQuality = new QLabel("Quality (JPEG/WEBP): 90%", this);
    grid->addWidget(m_lblQuality, 5, 0);

    m_sliderQuality = new QSlider(Qt::Horizontal, this);
    m_sliderQuality->setRange(1, 100);
    m_sliderQuality->setValue(90);
    connect(m_sliderQuality, &QSlider::valueChanged, this, [this](int val) {
        m_lblQuality->setText(QString("Quality (JPEG/WEBP): %1%").arg(val));
    });
    grid->addWidget(m_sliderQuality, 5, 1);

    mainLayout->addLayout(grid);

    // Progress bar
    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    // Action buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnConvert = new QPushButton("Process Images", this);
    m_btnConvert->setStyleSheet(
        "QPushButton { background-color: #fab387; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(m_btnConvert, &QPushButton::clicked, this, &ImageConverterDialog::onConvert);

    m_btnCancel = new QPushButton("Cancel", this);
    m_btnCancel->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch();
    btnLayout->addWidget(m_btnConvert);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);
}

void ImageConverterDialog::onResizeToggled(bool checked) {
    m_spinWidth->setEnabled(checked);
    m_spinHeight->setEnabled(checked);
    m_chkAspect->setEnabled(checked);
}

void ImageConverterDialog::onConvert() {
    int w = m_chkResize->isChecked() ? m_spinWidth->value() : 0;
    int h = m_chkResize->isChecked() ? m_spinHeight->value() : 0;
    bool keep = m_chkAspect->isChecked();
    
    int rotateAngle = 0;
    int rotateIndex = m_comboRotate->currentIndex();
    if (rotateIndex == 1) rotateAngle = 90;
    else if (rotateIndex == 2) rotateAngle = 270;
    else if (rotateIndex == 3) rotateAngle = 180;

    m_btnConvert->setEnabled(false);
    m_btnCancel->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);

    m_worker = new ImageWorker(
        m_inputPaths,
        m_comboFormat->currentText(),
        w, h, keep,
        rotateAngle,
        m_sliderQuality->value(),
        this
    );
    connect(m_worker, &ImageWorker::progress, this, &ImageConverterDialog::onConvertProgress);
    connect(m_worker, &ImageWorker::finished, this, &ImageConverterDialog::onConvertFinished);
    connect(m_worker, &ImageWorker::errorOccurred, this, &ImageConverterDialog::onConvertError);
    m_worker->start();
}

void ImageConverterDialog::onConvertProgress(int percentage, const QString& currentFile) {
    m_progress->setValue(percentage);
    m_lblStatus->setText("Processing: " + currentFile);
}

void ImageConverterDialog::onConvertFinished(int totalProcessed) {
    m_progress->setVisible(false);
    QMessageBox::information(this, "Success", QString("Successfully processed %1 images.").arg(totalProcessed));
    accept();
}

void ImageConverterDialog::onConvertError(const QString& errorMsg) {
    m_progress->setVisible(false);
    m_btnConvert->setEnabled(true);
    m_btnCancel->setEnabled(true);
    QMessageBox::critical(this, "Conversion Error", errorMsg);
}
