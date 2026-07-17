#ifndef IMAGECONVERTERDIALOG_H
#define IMAGECONVERTERDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QThread>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QLineEdit>

struct ImageProcessOptions {
    bool resizeEnabled = false;
    int resizeWidth = 800;
    int resizeHeight = 600;
    bool keepAspect = true;

    bool convertEnabled = false;
    QString targetFormat = "jpg"; // "jpg", "png", "webp"

    bool watermarkEnabled = false;
    QString watermarkText = "Amifiles";
    QColor watermarkColor = Qt::white;
    int watermarkSize = 24;
    double watermarkOpacity = 0.5;
    QString watermarkPosition = "bottom-right"; // "top-left", "top-right", "bottom-left", "bottom-right", "center"

    bool filtersEnabled = false;
    bool grayscale = false;
    bool sepia = false;
    bool invert = false;
    int brightness = 0; // -100 to 100
    int contrast = 0;   // -100 to 100
    
    int rotateAngle = 0;
    int quality = 90;
};

class ImageWorker : public QThread {
    Q_OBJECT
public:
    explicit ImageWorker(const QStringList& inputPaths, const QString& destDir, const ImageProcessOptions& opts, QObject* parent = nullptr);
    ~ImageWorker() override = default;

signals:
    void progress(int percentage, const QString& currentFile);
    void finished(int totalProcessed);
    void errorOccurred(const QString& errorMsg);

protected:
    void run() override;

private:
    QStringList m_inputPaths;
    QString m_destDir;
    ImageProcessOptions m_opts;
};

class ImageConverterDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageConverterDialog(const QStringList& inputPaths, QWidget* parent = nullptr);
    ~ImageConverterDialog() override;

private slots:
    void onResizeToggled(bool checked);
    void onWatermarkToggled(bool checked);
    void onFiltersToggled(bool checked);
    void onConvert();
    void onSelectDestDir();
    void onSelectColor();
    void onConvertProgress(int percentage, const QString& currentFile);
    void onConvertFinished(int totalProcessed);
    void onConvertError(const QString& errorMsg);

private:
    void setupUI();
    ImageProcessOptions getOptions() const;

    QStringList m_inputPaths;
    QString m_destDir;
    QColor m_watermarkColor = Qt::white;
    ImageWorker* m_worker = nullptr;

    // Output Destination
    QLineEdit* m_destDirEdit = nullptr;

    // Resize controls
    QCheckBox* m_chkResize = nullptr;
    QSpinBox* m_spinWidth = nullptr;
    QSpinBox* m_spinHeight = nullptr;
    QCheckBox* m_chkAspect = nullptr;

    // Format & rotation
    QComboBox* m_comboFormat = nullptr;
    QComboBox* m_comboRotate = nullptr;
    QLabel* m_lblQuality = nullptr;
    QSlider* m_sliderQuality = nullptr;

    // Watermark controls
    QCheckBox* m_chkWatermark = nullptr;
    QLineEdit* m_txtWatermark = nullptr;
    QComboBox* m_comboPosition = nullptr;
    QSpinBox* m_spinWatermarkSize = nullptr;
    QSlider* m_sliderOpacity = nullptr;
    QPushButton* m_btnColor = nullptr;

    // Filter controls
    QCheckBox* m_chkFilters = nullptr;
    QCheckBox* m_chkGrayscale = nullptr;
    QCheckBox* m_chkSepia = nullptr;
    QCheckBox* m_chkInvert = nullptr;
    QSlider* m_sliderBrightness = nullptr;
    QSlider* m_sliderContrast = nullptr;

    // Progress & buttons
    QLabel* m_lblStatus = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_btnConvert = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // IMAGECONVERTERDIALOG_H
