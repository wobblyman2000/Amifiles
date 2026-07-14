#ifndef COPYQUEUE_H
#define COPYQUEUE_H

#include <QDialog>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <atomic>

struct CopyJob {
    QString srcPath;
    QString destPath;
    bool isMove;
};

class CopyQueueWorker : public QThread {
    Q_OBJECT
public:
    CopyQueueWorker(QObject* parent = nullptr);
    ~CopyQueueWorker() override;

    void addJob(const CopyJob& job);
    void pause();
    void resume();
    void cancel();
    void skipCurrent();

    bool isPaused() const { return m_paused; }
    QList<CopyJob> getPendingJobs() const;

signals:
    void jobStarted(const QString& src, const QString& dest, bool isMove);
    void jobAdded();
    void fileProgress(const QString& filename, qint64 bytesCopied, qint64 bytesTotal);
    void batchProgress(int currentFile, int totalFiles, qint64 totalBytesCopied, qint64 totalBytesTotal);
    void jobFinished(const QString& src, const QString& dest, bool success);
    void queueFinished();

protected:
    void run() override;

private:
    bool processJob(const CopyJob& job);
    bool copyDirRecursively(const QString& srcDir, const QString& destDir);
    bool copyFileChunked(const QString& src, const QString& dest);
    qint64 calculateTotalSize(const QStringList& paths);
    void scanDirForFiles(const QString& dirPath, QStringList& fileList);

    QQueue<CopyJob> m_queue;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_cancelled;
    std::atomic<bool> m_skipCurrent;
    std::atomic<bool> m_running;

    // Batch details
    int m_currentFileIndex;
    int m_totalFileCount;
    qint64 m_batchBytesCopied;
    qint64 m_batchBytesTotal;
};

class CopyQueueDialog;

class CopyQueueManager : public QObject {
    Q_OBJECT
public:
    static CopyQueueManager& instance();

    void queueCopy(const QStringList& srcPaths, const QString& destDir, bool isMove);
    void showQueueDialog(QWidget* parent = nullptr);
    CopyQueueWorker* worker() const { return m_worker; }

private:
    explicit CopyQueueManager(QObject* parent = nullptr);
    ~CopyQueueManager() override = default;

    CopyQueueWorker* m_worker = nullptr;
    CopyQueueDialog* m_activeDialog = nullptr;
};

class CopyQueueDialog : public QDialog {
    Q_OBJECT
public:
    explicit CopyQueueDialog(QWidget* parent = nullptr);
    ~CopyQueueDialog() override = default;

private slots:
    void onJobStarted(const QString& src, const QString& dest, bool isMove);
    void onFileProgress(const QString& filename, qint64 bytesCopied, qint64 bytesTotal);
    void onBatchProgress(int currentFile, int totalFiles, qint64 totalBytesCopied, qint64 totalBytesTotal);
    void onPauseToggled();
    void onSkipClicked();
    void onCancelClicked();
    void onWorkerFinished();
    void updatePendingList();

private:
    void setupUI();
    QString formatBytes(qint64 bytes) const;

    QLabel* m_lblStatus = nullptr;
    QLabel* m_lblFile = nullptr;
    QProgressBar* m_progFile = nullptr;
    QProgressBar* m_progBatch = nullptr;
    QPushButton* m_btnPause = nullptr;
    QPushButton* m_btnSkip = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QListWidget* m_listPending = nullptr;
};

#endif // COPYQUEUE_H
