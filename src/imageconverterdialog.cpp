#include "imageconverterdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileInfo>
#include <QImage>
#include <QTransform>
#include <QMessageBox>
#include <QTabWidget>
#include <QFileDialog>
#include <QColorDialog>
#include <QPainter>
#include <QDir>
#include <QFontMetrics>

ImageWorker::ImageWorker(const QStringList& inputPaths, const QString& destDir, const ImageProcessOptions& opts, QObject* parent)
    : QThread(parent), m_inputPaths(inputPaths), m_destDir(destDir), m_opts(opts) {}

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
        if (m_opts.rotateAngle != 0) {
            QTransform trans;
            trans.rotate(m_opts.rotateAngle);
            img = img.transformed(trans);
        }

        // Apply filters
        if (m_opts.filtersEnabled) {
            if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_RGB32) {
                img = img.convertToFormat(QImage::Format_ARGB32);
            }

            int w = img.width();
            int h = img.height();

            for (int y = 0; y < h; ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < w; ++x) {
                    QRgb pixel = line[x];
                    int a = qAlpha(pixel);
                    int r = qRed(pixel);
                    int g = qGreen(pixel);
                    int b = qBlue(pixel);

                    // Brightness & Contrast
                    if (m_opts.brightness != 0) {
                        r = qBound(0, r + m_opts.brightness, 255);
                        g = qBound(0, g + m_opts.brightness, 255);
                        b = qBound(0, b + m_opts.brightness, 255);
                    }
                    if (m_opts.contrast != 0) {
                        double factor = (259.0 * (m_opts.contrast + 255.0)) / (255.0 * (259.0 - m_opts.contrast));
                        r = qBound(0, static_cast<int>(factor * (r - 128) + 128), 255);
                        g = qBound(0, static_cast<int>(factor * (g - 128) + 128), 255);
                        b = qBound(0, static_cast<int>(factor * (b - 128) + 128), 255);
                    }

                    // Grayscale
                    if (m_opts.grayscale) {
                        int gray = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
                        r = g = b = gray;
                    }

                    // Sepia
                    if (m_opts.sepia) {
                        int tr = static_cast<int>(0.393 * r + 0.769 * g + 0.189 * b);
                        int tg = static_cast<int>(0.349 * r + 0.686 * g + 0.168 * b);
                        int tb = static_cast<int>(0.272 * r + 0.534 * g + 0.131 * b);
                        r = qBound(0, tr, 255);
                        g = qBound(0, tg, 255);
                        b = qBound(0, tb, 255);
                    }

                    // Invert
                    if (m_opts.invert) {
                        r = 255 - r;
                        g = 255 - g;
                        b = 255 - b;
                    }

                    line[x] = qRgba(r, g, b, a);
                }
            }
        }

        // Apply resize
        if (m_opts.resizeEnabled && m_opts.resizeWidth > 0 && m_opts.resizeHeight > 0) {
            Qt::AspectRatioMode mode = m_opts.keepAspect ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
            img = img.scaled(m_opts.resizeWidth, m_opts.resizeHeight, mode, Qt::SmoothTransformation);
        }

        // Apply watermark
        if (m_opts.watermarkEnabled && !m_opts.watermarkText.isEmpty()) {
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing);

            QFont font("Inter", m_opts.watermarkSize);
            font.setBold(true);
            painter.setFont(font);

            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(m_opts.watermarkText);
            
            int pad = 15;
            int x = pad;
            int y = img.height() - pad - textRect.height();

            if (m_opts.watermarkPosition == "top-left") {
                x = pad;
                y = pad + textRect.height();
            } else if (m_opts.watermarkPosition == "top-right") {
                x = img.width() - pad - textRect.width();
                y = pad + textRect.height();
            } else if (m_opts.watermarkPosition == "bottom-left") {
                x = pad;
                y = img.height() - pad - textRect.height();
            } else if (m_opts.watermarkPosition == "bottom-right") {
                x = img.width() - pad - textRect.width();
                y = img.height() - pad - textRect.height();
            } else if (m_opts.watermarkPosition == "center") {
                x = (img.width() - textRect.width()) / 2;
                y = (img.height() - textRect.height()) / 2 + textRect.height() / 2;
            }

            // Draw shadow
            painter.setPen(QColor(0, 0, 0, static_cast<int>(m_opts.watermarkOpacity * 180)));
            painter.drawText(x + 2, y + 2, m_opts.watermarkText);

            // Draw primary text
            QColor wColor = m_opts.watermarkColor;
            wColor.setAlphaF(m_opts.watermarkOpacity);
            painter.setPen(wColor);
            painter.drawText(x, y, m_opts.watermarkText);
            painter.end();
        }

        // Output destination calculation
        QFileInfo info(path);
        QString destPath;
        if (m_destDir.isEmpty()) {
            destPath = info.absolutePath() + "/" + info.baseName() + "_converted." + m_opts.targetFormat.toLower();
        } else {
            destPath = QDir(m_destDir).filePath(info.baseName() + "_converted." + m_opts.targetFormat.toLower());
        }

        if (img.save(destPath, m_opts.targetFormat.toUpper().toLatin1().constData(), m_opts.quality)) {
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
    setWindowTitle("Batch Image Processing Suite");
    resize(520, 480);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QTabWidget::pane { border: 1px solid #313244; border-radius: 4px; background-color: #181825; }"
        "QTabBar::tab { background-color: #313244; color: #a6adc8; padding: 8px 12px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: #181825; color: #89b4fa; font-weight: bold; }"
        "QLabel { color: #cdd6f4; }"
        "QCheckBox { color: #cdd6f4; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QComboBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QSpinBox { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QProgressBar { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; text-align: center; color: #cdd6f4; }"
        "QProgressBar::chunk { background-color: #fab387; border-radius: 3px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    QTabWidget* tabs = new QTabWidget(this);

    // Tab 1: Resize & Format
    QWidget* tabResize = new QWidget(this);
    QFormLayout* layResize = new QFormLayout(tabResize);
    layResize->setContentsMargins(12, 12, 12, 12);

    m_chkResize = new QCheckBox("Enable Resizing", tabResize);
    m_spinWidth = new QSpinBox(tabResize);
    m_spinWidth->setRange(10, 10000);
    m_spinWidth->setValue(1920);
    m_spinWidth->setEnabled(false);
    m_spinHeight = new QSpinBox(tabResize);
    m_spinHeight->setRange(10, 10000);
    m_spinHeight->setValue(1080);
    m_spinHeight->setEnabled(false);
    m_chkAspect = new QCheckBox("Preserve Aspect Ratio", tabResize);
    m_chkAspect->setChecked(true);
    m_chkAspect->setEnabled(false);

    connect(m_chkResize, &QCheckBox::toggled, this, &ImageConverterDialog::onResizeToggled);

    m_comboFormat = new QComboBox(tabResize);
    m_comboFormat->addItems({"jpg", "png", "webp"});
    m_comboRotate = new QComboBox(tabResize);
    m_comboRotate->addItem("0° (No Rotation)", 0);
    m_comboRotate->addItem("90° Clockwise", 90);
    m_comboRotate->addItem("180° Half-turn", 180);
    m_comboRotate->addItem("270° Counter-clockwise", 270);

    m_sliderQuality = new QSlider(Qt::Horizontal, tabResize);
    m_sliderQuality->setRange(10, 100);
    m_sliderQuality->setValue(90);

    layResize->addRow(m_chkResize);
    layResize->addRow("Width (px):", m_spinWidth);
    layResize->addRow("Height (px):", m_spinHeight);
    layResize->addRow(m_chkAspect);
    layResize->addRow(new QWidget(tabResize)); // spacer
    layResize->addRow("Target Format:", m_comboFormat);
    layResize->addRow("Rotate Angle:", m_comboRotate);
    layResize->addRow("Save Quality (%):", m_sliderQuality);
    tabs->addTab(tabResize, "Format & Size");

    // Tab 2: Watermark Overlay
    QWidget* tabWatermark = new QWidget(this);
    QFormLayout* layWatermark = new QFormLayout(tabWatermark);
    layWatermark->setContentsMargins(12, 12, 12, 12);

    m_chkWatermark = new QCheckBox("Enable Text Watermark", tabWatermark);
    m_txtWatermark = new QLineEdit("Amifiles", tabWatermark);
    m_txtWatermark->setEnabled(false);
    m_comboPosition = new QComboBox(tabWatermark);
    m_comboPosition->addItems({"bottom-right", "bottom-left", "top-right", "top-left", "center"});
    m_comboPosition->setEnabled(false);
    m_spinWatermarkSize = new QSpinBox(tabWatermark);
    m_spinWatermarkSize->setRange(6, 120);
    m_spinWatermarkSize->setValue(24);
    m_spinWatermarkSize->setEnabled(false);
    m_sliderOpacity = new QSlider(Qt::Horizontal, tabWatermark);
    m_sliderOpacity->setRange(10, 100);
    m_sliderOpacity->setValue(50);
    m_sliderOpacity->setEnabled(false);
    m_btnColor = new QPushButton("Choose Color...", tabWatermark);
    m_btnColor->setEnabled(false);
    m_btnColor->setStyleSheet("QPushButton { background-color: #313244; color: #ffffff; }");
    connect(m_btnColor, &QPushButton::clicked, this, &ImageConverterDialog::onSelectColor);
    connect(m_chkWatermark, &QCheckBox::toggled, this, &ImageConverterDialog::onWatermarkToggled);

    layWatermark->addRow(m_chkWatermark);
    layWatermark->addRow("Watermark Text:", m_txtWatermark);
    layWatermark->addRow("Positioning:", m_comboPosition);
    layWatermark->addRow("Font Size (pt):", m_spinWatermarkSize);
    layWatermark->addRow("Opacity (%):", m_sliderOpacity);
    layWatermark->addRow("Text Color:", m_btnColor);
    tabs->addTab(tabWatermark, "Watermark");

    // Tab 3: Color Filters
    QWidget* tabFilters = new QWidget(this);
    QVBoxLayout* layFilters = new QVBoxLayout(tabFilters);
    layFilters->setContentsMargins(12, 12, 12, 12);
    layFilters->setSpacing(6);

    m_chkFilters = new QCheckBox("Enable Color Filters", tabFilters);
    m_chkGrayscale = new QCheckBox("Grayscale (Monochrome)", tabFilters);
    m_chkGrayscale->setEnabled(false);
    m_chkSepia = new QCheckBox("Sepia Vintage tone", tabFilters);
    m_chkSepia->setEnabled(false);
    m_chkInvert = new QCheckBox("Invert Color Channels", tabFilters);
    m_chkInvert->setEnabled(false);

    QFormLayout* layAdj = new QFormLayout();
    m_sliderBrightness = new QSlider(Qt::Horizontal, tabFilters);
    m_sliderBrightness->setRange(-100, 100);
    m_sliderBrightness->setValue(0);
    m_sliderBrightness->setEnabled(false);
    m_sliderContrast = new QSlider(Qt::Horizontal, tabFilters);
    m_sliderContrast->setRange(-100, 100);
    m_sliderContrast->setValue(0);
    m_sliderContrast->setEnabled(false);
    layAdj->addRow("Brightness:", m_sliderBrightness);
    layAdj->addRow("Contrast:", m_sliderContrast);

    connect(m_chkFilters, &QCheckBox::toggled, this, &ImageConverterDialog::onFiltersToggled);

    layFilters->addWidget(m_chkFilters);
    layFilters->addWidget(m_chkGrayscale);
    layFilters->addWidget(m_chkSepia);
    layFilters->addWidget(m_chkInvert);
    layFilters->addLayout(layAdj);
    tabs->addTab(tabFilters, "Color Filters");

    // Tab 4: Destination Directory
    QWidget* tabDest = new QWidget(this);
    QVBoxLayout* layDest = new QVBoxLayout(tabDest);
    layDest->setContentsMargins(12, 12, 12, 12);
    layDest->setSpacing(10);

    QLabel* lblDestHint = new QLabel("Select target folder to place processed images. Leave empty to place inside original directories with '_converted' suffix.", tabDest);
    lblDestHint->setWordWrap(true);
    lblDestHint->setStyleSheet("color: #a6adc8;");
    
    m_destDirEdit = new QLineEdit(tabDest);
    m_destDirEdit->setPlaceholderText("Output inside original directory");
    QPushButton* btnBrowse = new QPushButton("Browse Output Folder...", tabDest);
    btnBrowse->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; padding: 6px; }");
    connect(btnBrowse, &QPushButton::clicked, this, &ImageConverterDialog::onSelectDestDir);

    layDest->addWidget(lblDestHint);
    layDest->addWidget(m_destDirEdit);
    layDest->addWidget(btnBrowse);
    layDest->addStretch();
    tabs->addTab(tabDest, "Destination");

    mainLayout->addWidget(tabs);

    // Progress Indicator & Action Buttons
    m_lblStatus = new QLabel(QString("Ready to process %1 selected images.").arg(m_inputPaths.size()), this);
    m_lblStatus->setStyleSheet("color: #a6adc8; font-style: italic;");
    
    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_progress->hide();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnConvert = new QPushButton("Run Processing", this);
    m_btnConvert->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background-color: #89b4fa; }");
    m_btnCancel = new QPushButton("Cancel", this);
    m_btnCancel->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 16px; }");

    connect(m_btnConvert, &QPushButton::clicked, this, &ImageConverterDialog::onConvert);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(m_lblStatus, 1);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnConvert);
    btnLayout->addWidget(m_btnCancel);

    mainLayout->addWidget(m_progress);
    mainLayout->addLayout(btnLayout);
}

void ImageConverterDialog::onResizeToggled(bool checked) {
    m_spinWidth->setEnabled(checked);
    m_spinHeight->setEnabled(checked);
    m_chkAspect->setEnabled(checked);
}

void ImageConverterDialog::onWatermarkToggled(bool checked) {
    m_txtWatermark->setEnabled(checked);
    m_comboPosition->setEnabled(checked);
    m_spinWatermarkSize->setEnabled(checked);
    m_sliderOpacity->setEnabled(checked);
    m_btnColor->setEnabled(checked);
}

void ImageConverterDialog::onFiltersToggled(bool checked) {
    m_chkGrayscale->setEnabled(checked);
    m_chkSepia->setEnabled(checked);
    m_chkInvert->setEnabled(checked);
    m_sliderBrightness->setEnabled(checked);
    m_sliderContrast->setEnabled(checked);
}

void ImageConverterDialog::onSelectDestDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Destination Folder");
    if (!dir.isEmpty()) {
        m_destDir = dir;
        m_destDirEdit->setText(dir);
    }
}

void ImageConverterDialog::onSelectColor() {
    QColor col = QColorDialog::getColor(m_watermarkColor, this, "Choose Watermark Color");
    if (col.isValid()) {
        m_watermarkColor = col;
        m_btnColor->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; }")
                                  .arg(col.name())
                                  .arg((col.lightness() > 128) ? "#000000" : "#ffffff"));
    }
}

ImageProcessOptions ImageConverterDialog::getOptions() const {
    ImageProcessOptions opts;
    
    opts.resizeEnabled = m_chkResize->isChecked();
    opts.resizeWidth = m_spinWidth->value();
    opts.resizeHeight = m_spinHeight->value();
    opts.keepAspect = m_chkAspect->isChecked();

    opts.convertEnabled = true;
    opts.targetFormat = m_comboFormat->currentText();
    opts.rotateAngle = m_comboRotate->currentData().toInt();
    opts.quality = m_sliderQuality->value();

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

void ImageConverterDialog::onConvert() {
    if (m_inputPaths.isEmpty()) return;

    m_btnConvert->setEnabled(false);
    m_progress->setValue(0);
    m_progress->show();

    ImageProcessOptions opts = getOptions();
    m_worker = new ImageWorker(m_inputPaths, m_destDir, opts, this);
    connect(m_worker, &ImageWorker::progress, this, &ImageConverterDialog::onConvertProgress);
    connect(m_worker, &ImageWorker::finished, this, &ImageConverterDialog::onConvertFinished);
    connect(m_worker, &ImageWorker::errorOccurred, this, &ImageConverterDialog::onConvertError);

    m_lblStatus->setText("Launching worker thread...");
    m_worker->start();
}

void ImageConverterDialog::onConvertProgress(int percentage, const QString& currentFile) {
    m_progress->setValue(percentage);
    m_lblStatus->setText(QString("Processing (%1%): %2").arg(percentage).arg(currentFile));
}

void ImageConverterDialog::onConvertFinished(int totalProcessed) {
    m_progress->setValue(100);
    m_lblStatus->setText(QString("Processing complete. Successfully converted %1 images.").arg(totalProcessed));
    m_btnConvert->setEnabled(true);

    QMessageBox::information(this, "Success", QString("Batch processing completed successfully. Processed %1 image files.").arg(totalProcessed));
    accept();
}

void ImageConverterDialog::onConvertError(const QString& errorMsg) {
    m_progress->hide();
    m_lblStatus->setText("Bulk processing failed.");
    m_btnConvert->setEnabled(true);
    QMessageBox::critical(this, "Error", QString("An error occurred during batch image operations:\n%1").arg(errorMsg));
}
