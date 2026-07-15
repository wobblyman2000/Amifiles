#ifndef SHREDDIALOG_H
#define SHREDDIALOG_H

#include <QDialog>
#include <QThread>
#include <QStringList>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

class ShredWorker : public QThread {
    Q_OBJECT
public:
    explicit ShredWorker(const QStringList& paths, QObject* parent = nullptr);
    ~ShredWorker() override = default;

signals:
    void progress(int percentage, const QString& currentFile);
    void finished(int totalShredded);
    void errorOccurred(const QString& errorMsg);

protected:
    void run() override;

private:
    bool shredFile(const QString& filePath);
    void collectFiles(const QString& path, QStringList& filesList, QStringList& dirsList);

    QStringList m_paths;
};

class ShredDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShredDialog(const QStringList& paths, QWidget* parent = nullptr);
    ~ShredDialog() override;

private slots:
    void onStartShredding();
    void onShredProgress(int percentage, const QString& currentFile);
    void onShredFinished(int totalShredded);
    void onShredError(const QString& errorMsg);

private:
    void setupUI();

    QStringList m_paths;
    ShredWorker* m_worker = nullptr;

    QLabel* m_lblStatus = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_btnShred = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // SHREDDIALOG_H
