#ifndef FOLDERIMAGERENAMERDIALOG_H
#define FOLDERIMAGERENAMERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QStringList>
#include <QGroupBox>
#include <QProgressBar>
#include <QThread>

class ImageScanWorker : public QThread {
    Q_OBJECT
public:
    ImageScanWorker(const QString& path, const QStringList& findNames, const QString& targetNamePattern, const QString& customTargetPattern, QObject* parent = nullptr);
    void stopScan() { m_stop = true; }

signals:
    void progressUpdated(int scannedCount);
    void matchFound(const QString& dir, const QString& oldName, const QString& newName, const QString& fullPath);
    void scanFinished(int matchCount);

protected:
    void run() override;

private:
    QString m_path;
    QStringList m_findNames;
    QString m_targetNamePattern;
    QString m_customTargetPattern;
    bool m_stop;
};

class FolderImageRenamerDialog : public QDialog {
    Q_OBJECT
public:
    explicit FolderImageRenamerDialog(const QString& initialDir, QWidget* parent = nullptr);
    ~FolderImageRenamerDialog() override;

private slots:
    void onBrowseDirectory();
    void onScan();
    void onApplyRename();
    void onToggleSelectAll(bool checked);
    void onTableSelectionChanged();
    void onScanFinished(int matchCount);
    void onMatchFound(const QString& dir, const QString& oldName, const QString& newName, const QString& fullPath);

private:
    void setupUI();

    QString m_initialDir;

    // UI elements
    QLineEdit* m_editDir = nullptr;
    QPushButton* m_btnBrowse = nullptr;
    
    // Find checkboxes
    QCheckBox* m_chkFolder = nullptr;
    QCheckBox* m_chkDvd = nullptr;
    QCheckBox* m_chkPoster = nullptr;
    QCheckBox* m_chkAlbum = nullptr;
    QCheckBox* m_chkFront = nullptr;
    QCheckBox* m_chkCd = nullptr;
    QCheckBox* m_chkMovie = nullptr;
    QCheckBox* m_chkCustom = nullptr;
    QLineEdit* m_editCustomFind = nullptr;

    // Target name combo/edit
    QComboBox* m_comboTargetName = nullptr;
    QLineEdit* m_editCustomTarget = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_btnScan = nullptr;
    QCheckBox* m_chkSelectAll = nullptr;
    QTableWidget* m_tablePreview = nullptr;
    QLabel* m_lblStatus = nullptr;
    QPushButton* m_btnApply = nullptr;

    QGroupBox* m_grpPreviewDetails = nullptr;
    QLabel* m_lblThumbnail = nullptr;
    QLabel* m_lblDimensions = nullptr;
    QLabel* m_lblSize = nullptr;
    QLabel* m_lblFormat = nullptr;

    ImageScanWorker* m_worker = nullptr;
};

#endif // FOLDERIMAGERENAMERDIALOG_H
