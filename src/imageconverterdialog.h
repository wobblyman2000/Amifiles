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

class ImageWorker : public QThread {
    Q_OBJECT
public:
    explicit ImageWorker(const QStringList& inputPaths, const QString& format, int width, int height, bool keepAspect, int rotateAngle, int quality, QObject* parent = nullptr);
    ~ImageWorker() override = default;

signals:
    void progress(int percentage, const QString& currentFile);
    void finished(int totalProcessed);
    void errorOccurred(const QString& errorMsg);

protected:
    void run() override;

private:
    QStringList m_inputPaths;
    QString m_format;
    int m_width;
    int m_height;
    bool m_keepAspect;
    int m_rotateAngle;
    int m_quality;
};

class ImageConverterDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageConverterDialog(const QStringList& inputPaths, QWidget* parent = nullptr);
    ~ImageConverterDialog() override;

private slots:
    void onResizeToggled(bool checked);
    void onConvert();
    void onConvertProgress(int percentage, const QString& currentFile);
    void onConvertFinished(int totalProcessed);
    void onConvertError(const QString& errorMsg);

private:
    void setupUI();

    QStringList m_inputPaths;
    ImageWorker* m_worker = nullptr;

    QCheckBox* m_chkResize = nullptr;
    QSpinBox* m_spinWidth = nullptr;
    QSpinBox* m_spinHeight = nullptr;
    QCheckBox* m_chkAspect = nullptr;

    QComboBox* m_comboFormat = nullptr;
    QComboBox* m_comboRotate = nullptr;

    QLabel* m_lblQuality = nullptr;
    QSlider* m_sliderQuality = nullptr;

    QLabel* m_lblStatus = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_btnConvert = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // IMAGECONVERTERDIALOG_H
