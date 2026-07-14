#include "copyqueue.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QTime>

// ================= CopyQueueWorker =================

CopyQueueWorker::CopyQueueWorker(QObject* parent)
    : QThread(parent), m_paused(false), m_cancelled(false), m_skipCurrent(false), m_running(true) {
    m_currentFileIndex = 0;
    m_totalFileCount = 0;
    m_batchBytesCopied = 0;
    m_batchBytesTotal = 0;
}

CopyQueueWorker::~CopyQueueWorker() {
    m_running = false;
    m_cond.wakeAll();
    wait();
}

void CopyQueueWorker::addJob(const CopyJob& job) {
    QMutexLocker locker(&m_mutex);
    m_queue.enqueue(job);
    m_cond.wakeOne();
    locker.unlock();
    emit jobAdded();
}

void CopyQueueWorker::pause() {
    m_paused = true;
}

void CopyQueueWorker::resume() {
    m_paused = false;
    m_cond.wakeAll();
}

void CopyQueueWorker::cancel() {
    m_cancelled = true;
    m_paused = false;
    m_cond.wakeAll();
}

void CopyQueueWorker::skipCurrent() {
    m_skipCurrent = true;
    m_paused = false;
    m_cond.wakeAll();
}

QList<CopyJob> CopyQueueWorker::getPendingJobs() const {
    QMutexLocker locker(&m_mutex);
    return QList<CopyJob>(m_queue.begin(), m_queue.end());
}

void CopyQueueWorker::run() {
    while (m_running) {
        CopyJob job;
        {
            QMutexLocker locker(&m_mutex);
            while (m_queue.isEmpty() && m_running) {
                m_cond.wait(&m_mutex);
            }
            if (!m_running) break;
            job = m_queue.dequeue();
        }

        m_cancelled = false;
        m_skipCurrent = false;

        emit jobStarted(job.srcPath, job.destPath, job.isMove);

        QStringList pathsToScan = { job.srcPath };
        m_batchBytesTotal = calculateTotalSize(pathsToScan);
        m_batchBytesCopied = 0;
        m_currentFileIndex = 0;

        m_totalFileCount = 0;
        QFileInfo srcInfo(job.srcPath);
        if (srcInfo.isDir()) {
            QStringList fileList;
            scanDirForFiles(job.srcPath, fileList);
            m_totalFileCount = fileList.size();
        } else {
            m_totalFileCount = 1;
        }

        bool success = processJob(job);
        emit jobFinished(job.srcPath, job.destPath, success);
        emit queueFinished();
    }
}

qint64 CopyQueueWorker::calculateTotalSize(const QStringList& paths) {
    qint64 total = 0;
    for (const QString& path : paths) {
        QFileInfo info(path);
        if (info.isDir()) {
            QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                total += it.fileInfo().size();
            }
        } else {
            total += info.size();
        }
    }
    return total;
}

void CopyQueueWorker::scanDirForFiles(const QString& dirPath, QStringList& fileList) {
    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        fileList.append(it.next());
    }
}

bool CopyQueueWorker::processJob(const CopyJob& job) {
    if (job.isMove) {
        if (QFile::rename(job.srcPath, job.destPath)) {
            return true;
        }
    }

    QFileInfo srcInfo(job.srcPath);
    bool success = false;
    if (srcInfo.isDir()) {
        success = copyDirRecursively(job.srcPath, job.destPath);
    } else {
        success = copyFileChunked(job.srcPath, job.destPath);
    }

    if (job.isMove && success) {
        if (srcInfo.isDir()) {
            QDir(job.srcPath).removeRecursively();
        } else {
            QFile::remove(job.srcPath);
        }
    }
    return success;
}

bool CopyQueueWorker::copyDirRecursively(const QString& srcDir, const QString& destDir) {
    QDir dest(destDir);
    if (!dest.exists() && !dest.mkpath(".")) {
        return false;
    }

    QDir src(srcDir);
    QStringList entries = src.entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    for (const QString& entry : entries) {
        if (m_cancelled) return false;
        
        QString subSrc = src.filePath(entry);
        QString subDest = dest.filePath(entry);

        QFileInfo subInfo(subSrc);
        if (subInfo.isDir()) {
            if (!copyDirRecursively(subSrc, subDest)) {
                return false;
            }
        } else {
            if (!copyFileChunked(subSrc, subDest)) {
                return false;
            }
        }
    }
    return true;
}

bool CopyQueueWorker::copyFileChunked(const QString& src, const QString& dest) {
    if (m_skipCurrent) return true;

    QFile srcFile(src);
    QFile destFile(dest);
    if (!srcFile.open(QIODevice::ReadOnly)) return false;
    if (!destFile.open(QIODevice::WriteOnly)) return false;

    qint64 fileSize = srcFile.size();
    qint64 fileCopied = 0;
    char buffer[65536];

    while (fileCopied < fileSize) {
        if (m_cancelled) {
            destFile.close();
            QFile::remove(dest);
            return false;
        }
        if (m_skipCurrent) {
            destFile.close();
            QFile::remove(dest);
            return true;
        }
        while (m_paused) {
            QThread::msleep(100);
            if (m_cancelled) {
                destFile.close();
                QFile::remove(dest);
                return false;
            }
        }

        qint64 read = srcFile.read(buffer, sizeof(buffer));
        if (read <= 0) break;
        qint64 written = destFile.write(buffer, read);
        if (written != read) return false;

        fileCopied += read;
        m_batchBytesCopied += read;

        emit fileProgress(QFileInfo(src).fileName(), fileCopied, fileSize);
        emit batchProgress(m_currentFileIndex, m_totalFileCount, m_batchBytesCopied, m_batchBytesTotal);
    }

    m_currentFileIndex++;
    return true;
}

// ================= CopyQueueManager =================

CopyQueueManager& CopyQueueManager::instance() {
    static CopyQueueManager inst;
    return inst;
}

CopyQueueManager::CopyQueueManager(QObject* parent) : QObject(parent) {
    m_worker = new CopyQueueWorker(this);
    m_worker->start();
}

void CopyQueueManager::queueCopy(const QStringList& srcPaths, const QString& destDir, bool isMove) {
    QDir dest(destDir);
    for (const QString& src : srcPaths) {
        QFileInfo info(src);
        QString destPath = dest.filePath(info.fileName());
        
        CopyJob job;
        job.srcPath = src;
        job.destPath = destPath;
        job.isMove = isMove;
        m_worker->addJob(job);
    }
}

void CopyQueueManager::showQueueDialog(QWidget* parent) {
    if (!m_activeDialog) {
        m_activeDialog = new CopyQueueDialog(parent);
        m_activeDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_activeDialog, &QObject::destroyed, this, [this]() {
            m_activeDialog = nullptr;
        });
        m_activeDialog->show();
    } else {
        m_activeDialog->raise();
        m_activeDialog->activateWindow();
    }
}

// ================= CopyQueueDialog =================

CopyQueueDialog::CopyQueueDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("File Transfer Queue");
    resize(500, 320);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QProgressBar { border: 1px solid #313244; border-radius: 4px; text-align: center; background-color: #181825; color: #cdd6f4; }"
                  "QProgressBar::chunk { background-color: #a6e3a1; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; }"
                  "QPushButton:hover { background-color: #45475a; }"
                  "QListWidget { background-color: #181825; border: 1px solid #313244; color: #cdd6f4; }");

    setupUI();

    CopyQueueWorker* worker = CopyQueueManager::instance().worker();
    connect(worker, &CopyQueueWorker::jobStarted, this, &CopyQueueDialog::onJobStarted);
    connect(worker, &CopyQueueWorker::fileProgress, this, &CopyQueueDialog::onFileProgress);
    connect(worker, &CopyQueueWorker::batchProgress, this, &CopyQueueDialog::onBatchProgress);
    connect(worker, &CopyQueueWorker::queueFinished, this, &CopyQueueDialog::onWorkerFinished);
    connect(worker, &CopyQueueWorker::jobAdded, this, &CopyQueueDialog::updatePendingList);

    updatePendingList();
}

void CopyQueueDialog::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    m_lblStatus = new QLabel("Starting file transfer operations...", this);
    m_lblStatus->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_lblStatus);

    m_lblFile = new QLabel("Active File: None", this);
    m_lblFile->setStyleSheet("font-size: 11px; color: #a6adc8;");
    layout->addWidget(m_lblFile);

    m_progFile = new QProgressBar(this);
    m_progFile->setRange(0, 100);
    m_progFile->setValue(0);
    layout->addWidget(new QLabel("Current File Progress:", this));
    layout->addWidget(m_progFile);

    m_progBatch = new QProgressBar(this);
    m_progBatch->setRange(0, 100);
    m_progBatch->setValue(0);
    layout->addWidget(new QLabel("Overall Queue Progress:", this));
    layout->addWidget(m_progBatch);

    layout->addWidget(new QLabel("Pending Operations Queue:", this));
    m_listPending = new QListWidget(this);
    m_listPending->setMaximumHeight(80);
    layout->addWidget(m_listPending);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnPause = new QPushButton("Pause", this);
    connect(m_btnPause, &QPushButton::clicked, this, &CopyQueueDialog::onPauseToggled);
    btnLayout->addWidget(m_btnPause);

    m_btnSkip = new QPushButton("Skip File", this);
    connect(m_btnSkip, &QPushButton::clicked, this, &CopyQueueDialog::onSkipClicked);
    btnLayout->addWidget(m_btnSkip);

    m_btnCancel = new QPushButton("Cancel Queue", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &CopyQueueDialog::onCancelClicked);
    btnLayout->addWidget(m_btnCancel);

    layout->addLayout(btnLayout);
}

void CopyQueueDialog::onJobStarted(const QString& src, const QString& dest, bool isMove) {
    Q_UNUSED(dest);
    QString action = isMove ? "Moving" : "Copying";
    m_lblStatus->setText(QString("%1: %2").arg(action).arg(QFileInfo(src).fileName()));
    updatePendingList();
}

void CopyQueueDialog::onFileProgress(const QString& filename, qint64 bytesCopied, qint64 bytesTotal) {
    m_lblFile->setText(QString("Transferring chunk: %1 (%2 / %3)")
                       .arg(filename)
                       .arg(formatBytes(bytesCopied))
                       .arg(formatBytes(bytesTotal)));
    if (bytesTotal > 0) {
        m_progFile->setValue(static_cast<int>((bytesCopied * 100) / bytesTotal));
    }
}

void CopyQueueDialog::onBatchProgress(int currentFile, int totalFiles, qint64 totalBytesCopied, qint64 totalBytesTotal) {
    if (totalBytesTotal > 0) {
        m_progBatch->setValue(static_cast<int>((totalBytesCopied * 100) / totalBytesTotal));
        m_progBatch->setFormat(QString("%1% (%2 of %3 files)")
                               .arg(m_progBatch->value())
                               .arg(currentFile)
                               .arg(totalFiles));
    }
}

void CopyQueueDialog::onPauseToggled() {
    CopyQueueWorker* worker = CopyQueueManager::instance().worker();
    if (worker->isPaused()) {
        worker->resume();
        m_btnPause->setText("Pause");
    } else {
        worker->pause();
        m_btnPause->setText("Resume");
    }
}

void CopyQueueDialog::onSkipClicked() {
    CopyQueueManager::instance().worker()->skipCurrent();
}

void CopyQueueDialog::onCancelClicked() {
    CopyQueueManager::instance().worker()->cancel();
    reject();
}

void CopyQueueDialog::onWorkerFinished() {
    updatePendingList();
    if (m_listPending->count() == 0) {
        accept();
    }
}

void CopyQueueDialog::updatePendingList() {
    m_listPending->clear();
    QList<CopyJob> pending = CopyQueueManager::instance().worker()->getPendingJobs();
    for (const CopyJob& job : pending) {
        QString action = job.isMove ? "Move" : "Copy";
        m_listPending->addItem(QString("[%1] %2 -> %3")
                               .arg(action)
                               .arg(QFileInfo(job.srcPath).fileName())
                               .arg(QFileInfo(job.destPath).absolutePath()));
    }
}

QString CopyQueueDialog::formatBytes(qint64 bytes) const {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;
    if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
    if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
    if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
    return QString("%1 B").arg(bytes);
}
