#ifndef ARCHIVEDIALOG_H
#define ARCHIVEDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QProcess>

class QProgressBar;
class QLabel;
class QLineEdit;
class QComboBox;

class ArchiveDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { ModeCreate, ModeExtract };

    // Constructor for creating an archive from files
    ArchiveDialog(Mode mode, const QStringList& sourcePaths, const QString& currentDir, QWidget* parent = nullptr);
    // Constructor for extracting a single archive file
    ArchiveDialog(Mode mode, const QString& archivePath, const QString& currentDir, QWidget* parent = nullptr);
    ~ArchiveDialog() override;

private slots:
    void onStartClicked();
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onCancelClicked();

private:
    void setupUI();
    void startExtraction();
    void startCreation();
    void updateProgress(int percent, const QString& statusText);

    Mode m_mode;
    QStringList m_sourcePaths; // Files to compress
    QString m_archivePath;     // Archive to extract/create
    QString m_currentDir;      // Target directory for operation

    QProcess* m_process = nullptr;
    bool m_isRunning = false;

    // UI elements
    QLabel* m_lblTitle = nullptr;
    QLabel* m_lblStatus = nullptr;
    QLineEdit* m_txtTargetName = nullptr;
    QComboBox* m_comboFormat = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_btnAction = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // ARCHIVEDIALOG_H
