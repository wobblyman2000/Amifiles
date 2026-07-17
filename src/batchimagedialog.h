#pragma once

#include <QDialog>
#include <QStringList>
#include <QListWidget>
#include <QProgressBar>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QPushButton>
#include <QThread>
#include "imageprocessor.h"

class BatchImageWorker : public QThread {
    Q_OBJECT
public:
    BatchImageWorker(const QStringList& files, const QString& destDir, const ImageProcessOptions& opts, QObject* parent = nullptr);
    void run() override;

signals:
    void progress(int current, int total, const QString& fileName);
    void finished(int successCount, int failCount);

private:
    QStringList m_files;
    QString m_destDir;
    ImageProcessOptions m_opts;
};

class BatchImageDialog : public QDialog {
    Q_OBJECT
public:
    BatchImageDialog(const QStringList& selectedFiles, const QString& currentDir, QWidget* parent = nullptr);
    ~BatchImageDialog() override = default;

private slots:
    void onSelectDestDir();
    void onStartClicked();
    void onProgressUpdate(int current, int total, const QString& fileName);
    void onWorkerFinished(int successCount, int failCount);
    void onSelectColor();

private:
    void setupUI();
    ImageProcessOptions getOptions() const;

    QStringList m_files;
    QString m_currentDir;
    QString m_destDir;
    QColor m_watermarkColor = Qt::white;

    QListWidget* m_filesList = nullptr;
    QLineEdit* m_destDirEdit = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Resize controls
    QCheckBox* m_chkResize = nullptr;
    QSpinBox* m_spinWidth = nullptr;
    QSpinBox* m_spinHeight = nullptr;
    QCheckBox* m_chkAspect = nullptr;

    // Convert controls
    QCheckBox* m_chkConvert = nullptr;
    QComboBox* m_comboFormat = nullptr;

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

    QPushButton* m_btnStart = nullptr;
    BatchImageWorker* m_worker = nullptr;
};
