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
#include <QProgressDialog>
#include <QTime>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QGridLayout>
#include <QTimer>

// ================= CopyQueueWorker =================

CopyQueueWorker::CopyQueueWorker(QObject* parent)
    : QThread(parent), m_paused(false), m_cancelled(false), m_skipCurrent(false), m_running(true), m_isBusy(false) {
    m_currentFileIndex = 0;
    m_totalFileCount = 0;
    m_batchBytesCopied = 0;
    m_batchBytesTotal = 0;
    m_defaultResolution.store(ResolvePrompt);
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

bool CopyQueueWorker::isBusy() const {
    QMutexLocker locker(&m_mutex);
    return !m_queue.isEmpty() || m_isBusy;
}

void CopyQueueWorker::run() {
    while (m_running) {
        CopyJob job;
        {
            QMutexLocker locker(&m_mutex);
            while (m_queue.isEmpty() && m_running) {
                m_isBusy = false;
                m_currentFileIndex = 0;
                m_cond.wait(&m_mutex);
            }
            if (!m_running) break;
            m_isBusy = true;
            job = m_queue.dequeue();
        }

        m_cancelled = false;
        m_skipCurrent = false;
        m_defaultResolution.store(ResolvePrompt);

        emit jobStarted(job.srcPath, job.destPath, job.isMove);

        // Reset overall batch stats if starting a new batch
        if (m_currentFileIndex == 0) {
            m_batchBytesTotal = 0;
            m_totalFileCount = 0;
            m_batchBytesCopied = 0;
        }

        // Calculate size of current job if not already done
        if (job.totalBytes == -1) {
            qint64 jBytes = 0;
            int jFiles = 0;
            QFileInfo srcInfo(job.srcPath);
            if (srcInfo.isDir()) {
                QDirIterator it(job.srcPath, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    jBytes += it.fileInfo().size();
                    jFiles++;
                }
            } else {
                jBytes = srcInfo.size();
                jFiles = 1;
            }
            job.totalBytes = jBytes;
            job.totalFiles = jFiles;
            
            m_batchBytesTotal += jBytes;
            m_totalFileCount += jFiles;
        }

        // Scan and update all pending jobs in the queue that haven't been calculated yet
        QList<CopyJob> pendingCopy;
        {
            QMutexLocker locker(&m_mutex);
            pendingCopy = QList<CopyJob>(m_queue.begin(), m_queue.end());
        }

        bool updatedAny = false;
        for (int i = 0; i < pendingCopy.size(); ++i) {
            if (pendingCopy[i].totalBytes == -1) {
                qint64 jBytes = 0;
                int jFiles = 0;
                QFileInfo srcInfo(pendingCopy[i].srcPath);
                if (srcInfo.isDir()) {
                    QDirIterator it(pendingCopy[i].srcPath, QDir::Files, QDirIterator::Subdirectories);
                    while (it.hasNext()) {
                        it.next();
                        jBytes += it.fileInfo().size();
                        jFiles++;
                    }
                } else {
                    jBytes = srcInfo.size();
                    jFiles = 1;
                }
                pendingCopy[i].totalBytes = jBytes;
                pendingCopy[i].totalFiles = jFiles;
                
                m_batchBytesTotal += jBytes;
                m_totalFileCount += jFiles;
                updatedAny = true;
            }
        }

        // If we updated any pending jobs, write the calculated sizes back into the queue
        if (updatedAny) {
            QMutexLocker locker(&m_mutex);
            for (int i = 0; i < m_queue.size() && i < pendingCopy.size(); ++i) {
                if (m_queue[i].srcPath == pendingCopy[i].srcPath) {
                    m_queue[i].totalBytes = pendingCopy[i].totalBytes;
                    m_queue[i].totalFiles = pendingCopy[i].totalFiles;
                }
            }
        }

        m_batchTimer.start();
        m_lastElapsed = 0;
        m_lastBytesCopied = m_batchBytesCopied;

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

int CopyQueueWorker::checkCollision(const QString& src, const QString& dest) {
    if (!QFile::exists(dest)) {
        return ResolveOverwrite;
    }

    int defRes = m_defaultResolution.load();
    if (defRes == ResolveOverwriteAll) return ResolveOverwrite;
    if (defRes == ResolveSkipAll) return ResolveSkip;
    if (defRes == ResolveCancel) return ResolveCancel;

    int resolution = ResolvePrompt;
    emit collisionDetected(src, dest, &resolution);

    if (resolution == ResolvePrompt || resolution == ResolveCancel) {
        m_defaultResolution.store(ResolveCancel);
        m_cancelled = true;
        return ResolveCancel;
    }

    if (resolution == ResolveOverwriteAll) {
        m_defaultResolution.store(ResolveOverwriteAll);
        return ResolveOverwrite;
    }
    if (resolution == ResolveSkipAll) {
        m_defaultResolution.store(ResolveSkipAll);
        return ResolveSkip;
    }

    return resolution;
}

bool CopyQueueWorker::processJob(const CopyJob& job) {
    m_defaultResolution.store(ResolvePrompt);

    if (job.isMove) {
        QFileInfo destInfo(job.destPath);
        if (!destInfo.exists()) {
            if (QFile::rename(job.srcPath, job.destPath)) {
                return true;
            }
        } else {
            int res = checkCollision(job.srcPath, job.destPath);
            if (res == ResolveSkip) {
                return true;
            }
            if (res == ResolveCancel) {
                return false;
            }
            if (res == ResolveOverwrite) {
                QFile::remove(job.destPath);
                if (QFile::rename(job.srcPath, job.destPath)) {
                    return true;
                }
            }
            if (res == ResolveKeepBoth) {
                QFileInfo info(job.destPath);
                QString dir = info.absolutePath();
                QString base = info.completeBaseName();
                QString suffix = info.suffix();
                QString newDest = job.destPath;
                int counter = 1;
                while (QFile::exists(newDest)) {
                    newDest = QDir(dir).filePath(QString("%1 (%2).%3").arg(base).arg(counter++).arg(suffix));
                }
                if (QFile::rename(job.srcPath, newDest)) {
                    return true;
                }
            }
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

    QString actualDest = dest;
    if (QFile::exists(actualDest)) {
        int res = checkCollision(src, actualDest);
        if (res == ResolveSkip) {
            m_currentFileIndex++;
            m_batchBytesCopied += QFileInfo(src).size();
            emit batchProgress(m_currentFileIndex, m_totalFileCount, m_batchBytesCopied, m_batchBytesTotal);
            return true;
        }
        if (res == ResolveCancel) {
            return false;
        }
        if (res == ResolveOverwrite) {
            QFile::remove(actualDest);
        }
        if (res == ResolveKeepBoth) {
            QFileInfo info(actualDest);
            QString dir = info.absolutePath();
            QString base = info.completeBaseName();
            QString suffix = info.suffix();
            int counter = 1;
            while (QFile::exists(actualDest)) {
                actualDest = QDir(dir).filePath(QString("%1 (%2).%3").arg(base).arg(counter++).arg(suffix));
            }
        }
    }

    QFile srcFile(src);
    QFile destFile(actualDest);
    if (!srcFile.open(QIODevice::ReadOnly)) return false;
    if (!destFile.open(QIODevice::WriteOnly)) return false;

    qint64 fileSize = srcFile.size();
    qint64 fileCopied = 0;
    char buffer[65536];

    while (fileCopied < fileSize) {
        if (m_cancelled) {
            destFile.close();
            QFile::remove(actualDest);
            return false;
        }
        if (m_skipCurrent) {
            destFile.close();
            QFile::remove(actualDest);
            return true;
        }
        while (m_paused) {
            QThread::msleep(100);
            if (m_cancelled) {
                destFile.close();
                QFile::remove(actualDest);
                return false;
            }
        }

        qint64 read = srcFile.read(buffer, sizeof(buffer));
        if (read <= 0) break;
        qint64 written = destFile.write(buffer, read);
        if (written != read) return false;

        fileCopied += read;
        m_batchBytesCopied += read;

        qint64 elapsedMs = m_batchTimer.elapsed();
        qint64 timeDiff = elapsedMs - m_lastElapsed;
        if (timeDiff >= 1000) {
            qint64 bytesDiff = m_batchBytesCopied - m_lastBytesCopied;
            double speed = (double)bytesDiff / (timeDiff / 1000.0);
            double avgSpeed = (double)m_batchBytesCopied / (elapsedMs / 1000.0);
            qint64 remainingBytes = m_batchBytesTotal - m_batchBytesCopied;
            qint64 remainingSecs = (avgSpeed > 0) ? (remainingBytes / avgSpeed) : 0;

            emit speedUpdated(speed, remainingSecs);

            m_lastElapsed = elapsedMs;
            m_lastBytesCopied = m_batchBytesCopied;
        }

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

static bool copyPrioritized(const QString& src, const QString& dest, bool isMove, QProgressDialog& progress) {
    QFileInfo info(src);
    if (info.isDir()) {
        QDir srcDir(src);
        QDir destDir(dest);
        if (!destDir.exists() && !destDir.mkpath(".")) return false;
        
        QStringList entries = srcDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
        for (const QString& entry : entries) {
            if (progress.wasCanceled()) return false;
            QCoreApplication::processEvents();
            if (!copyPrioritized(srcDir.filePath(entry), destDir.filePath(entry), isMove, progress)) {
                return false;
            }
        }
        if (isMove) {
            srcDir.removeRecursively();
        }
        return true;
    } else {
        if (QFile::exists(dest)) {
            QFile::remove(dest);
        }
        bool success = false;
        if (isMove) {
            success = QFile::rename(src, dest);
        } else {
            success = QFile::copy(src, dest);
        }
        return success;
    }
}

void CopyQueueManager::queueCopy(const QStringList& srcPaths, const QString& destDir, bool isMove, QWidget* parent) {
    bool isBusy = m_worker->isBusy();
    qint64 totalNewSize = 0;
    for (const QString& src : srcPaths) {
        QFileInfo info(src);
        if (info.isDir()) {
            QDirIterator it(src, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                totalNewSize += it.fileInfo().size();
            }
        } else {
            totalNewSize += info.size();
        }
    }

    bool prioritize = false;
    if (isBusy && totalNewSize > 0 && totalNewSize <= 50 * 1024 * 1024) {
        prioritize = true;
    }

    if (prioritize) {
        m_worker->pause();

        QProgressDialog progress("Prioritizing small transfer...", "Cancel", 0, srcPaths.size(), parent);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        for (int i = 0; i < srcPaths.size(); ++i) {
            if (progress.wasCanceled()) break;
            progress.setValue(i);
            progress.setLabelText(QString("Prioritizing: %1").arg(QFileInfo(srcPaths[i]).fileName()));
            QCoreApplication::processEvents();

            QFileInfo info(srcPaths[i]);
            QString destPath = QDir(destDir).filePath(info.fileName());
            copyPrioritized(srcPaths[i], destPath, isMove, progress);
        }
        progress.setValue(srcPaths.size());

        m_worker->resume();
    } else {
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
        showQueueDialog(parent);
    }
}

void CopyQueueManager::showQueueDialog(QWidget* parent) {
    if (!m_worker->isBusy()) {
        return;
    }
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

// ================= SpeedChartWidget =================

SpeedChartWidget::SpeedChartWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(80);
}

void SpeedChartWidget::addSpeedPoint(double speedMBPerSec) {
    m_speedHistory.append(speedMBPerSec);
    if (m_speedHistory.size() > 60) {
        m_speedHistory.removeFirst();
    }
    update();
}

void SpeedChartWidget::clearHistory() {
    m_speedHistory.clear();
    update();
}

void SpeedChartWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    painter.fillRect(rect(), QColor("#181825"));

    // Draw grid lines
    painter.setPen(QPen(QColor("#313244"), 1, Qt::DashLine));
    int numGridLines = 3;
    for (int i = 1; i < numGridLines; ++i) {
        int y = (height() * i) / numGridLines;
        painter.drawLine(0, y, width(), y);
    }

    if (m_speedHistory.isEmpty()) {
        painter.setPen(QColor("#a6adc8"));
        painter.drawText(rect(), Qt::AlignCenter, "Calculating speed...");
        return;
    }

    // Determine scale
    m_maxSpeed = 1.0;
    for (double speed : m_speedHistory) {
        if (speed > m_maxSpeed) {
            m_maxSpeed = speed;
        }
    }

    double yMax = m_maxSpeed * 1.1;

    QPainterPath path;
    QPainterPath fillPath;

    int w = width();
    int h = height();
    int points = m_speedHistory.size();

    QPolygonF poly;
    double xStep = (points > 1) ? (double)w / (points - 1) : (double)w;

    for (int i = 0; i < points; ++i) {
        double x = i * xStep;
        double y = h - (m_speedHistory[i] / yMax) * h;
        y = qBound(0.0, y, (double)h);
        poly.append(QPointF(x, y));
    }

    if (poly.isEmpty()) return;

    path.addPolygon(poly);

    fillPath.moveTo(0, h);
    for (const QPointF& pt : poly) {
        fillPath.lineTo(pt);
    }
    fillPath.lineTo(poly.last().x(), h);
    fillPath.closeSubpath();

    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0, QColor(137, 180, 250, 100));
    grad.setColorAt(1.0, QColor(137, 180, 250, 0));
    painter.fillPath(fillPath, grad);

    painter.setPen(QPen(QColor("#89b4fa"), 2));
    painter.drawPath(path);

    painter.setPen(QColor("#cdd6f4"));
    painter.drawText(10, 15, QString("Max Speed: %1 MB/s").arg(m_maxSpeed, 0, 'f', 1));
}

// ================= FileCollisionDialog =================

FileCollisionDialog::FileCollisionDialog(const QString& srcPath, const QString& destPath, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("File Already Exists");
    resize(550, 240);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; }"
                  "QPushButton:hover { background-color: #45475a; }");
    setupUI(srcPath, destPath);
}

void FileCollisionDialog::setupUI(const QString& srcPath, const QString& destPath) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    QLabel* title = new QLabel("<b>A file with the same name already exists at the destination.</b>", this);
    title->setStyleSheet("font-size: 13px; color: #f9e2af;");
    layout->addWidget(title);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(8);

    QLabel* lblHeader1 = new QLabel("<b>Source File (Copying)</b>", this);
    lblHeader1->setStyleSheet("color: #a6e3a1;");
    QLabel* lblHeader2 = new QLabel("<b>Destination File (Existing)</b>", this);
    lblHeader2->setStyleSheet("color: #f38ba8;");

    grid->addWidget(lblHeader1, 0, 1);
    grid->addWidget(lblHeader2, 0, 2);

    grid->addWidget(new QLabel("Name:", this), 1, 0);
    grid->addWidget(new QLabel(QFileInfo(srcPath).fileName(), this), 1, 1);
    grid->addWidget(new QLabel(QFileInfo(destPath).fileName(), this), 1, 2);

    QFileInfo srcInfo(srcPath);
    QFileInfo destInfo(destPath);
    grid->addWidget(new QLabel("Size:", this), 2, 0);
    grid->addWidget(new QLabel(formatBytes(srcInfo.size()), this), 2, 1);
    grid->addWidget(new QLabel(formatBytes(destInfo.size()), this), 2, 2);

    grid->addWidget(new QLabel("Modified:", this), 3, 0);
    grid->addWidget(new QLabel(formatDateTime(srcInfo.lastModified()), this), 3, 1);
    grid->addWidget(new QLabel(formatDateTime(destInfo.lastModified()), this), 3, 2);

    layout->addLayout(grid);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);

    QPushButton* btnOverwrite = new QPushButton("Overwrite", this);
    btnOverwrite->setStyleSheet("QPushButton { background-color: #f38ba8; color: #11111b; font-weight: bold; }");
    connect(btnOverwrite, &QPushButton::clicked, this, &FileCollisionDialog::onOverwrite);
    btnLayout->addWidget(btnOverwrite);

    QPushButton* btnOverwriteAll = new QPushButton("Overwrite All", this);
    connect(btnOverwriteAll, &QPushButton::clicked, this, &FileCollisionDialog::onOverwriteAll);
    btnLayout->addWidget(btnOverwriteAll);

    QPushButton* btnKeepBoth = new QPushButton("Keep Both", this);
    btnKeepBoth->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; }");
    connect(btnKeepBoth, &QPushButton::clicked, this, &FileCollisionDialog::onKeepBoth);
    btnLayout->addWidget(btnKeepBoth);

    QPushButton* btnSkip = new QPushButton("Skip", this);
    connect(btnSkip, &QPushButton::clicked, this, &FileCollisionDialog::onSkip);
    btnLayout->addWidget(btnSkip);

    QPushButton* btnSkipAll = new QPushButton("Skip All", this);
    connect(btnSkipAll, &QPushButton::clicked, this, &FileCollisionDialog::onSkipAll);
    btnLayout->addWidget(btnSkipAll);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    connect(btnCancel, &QPushButton::clicked, this, &FileCollisionDialog::onCancel);
    btnLayout->addWidget(btnCancel);

    layout->addLayout(btnLayout);
}

void FileCollisionDialog::onOverwrite() { m_resolution = ResolveOverwrite; accept(); }
void FileCollisionDialog::onOverwriteAll() { m_resolution = ResolveOverwriteAll; accept(); }
void FileCollisionDialog::onSkip() { m_resolution = ResolveSkip; accept(); }
void FileCollisionDialog::onSkipAll() { m_resolution = ResolveSkipAll; accept(); }
void FileCollisionDialog::onKeepBoth() { m_resolution = ResolveKeepBoth; accept(); }
void FileCollisionDialog::onCancel() { m_resolution = ResolveCancel; reject(); }

QString FileCollisionDialog::formatBytes(qint64 bytes) const {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;
    if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
    if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
    if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
    return QString("%1 B").arg(bytes);
}

QString FileCollisionDialog::formatDateTime(const QDateTime& dt) const {
    return dt.toString("yyyy-MM-dd hh:mm:ss");
}

// ================= CopyQueueDialog =================

CopyQueueDialog::CopyQueueDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("File Transfer Queue");
    resize(500, 430);
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
    
    connect(worker, &CopyQueueWorker::collisionDetected, this, &CopyQueueDialog::onCollisionDetected, Qt::BlockingQueuedConnection);
    connect(worker, &CopyQueueWorker::speedUpdated, this, &CopyQueueDialog::onSpeedUpdated);

    updatePendingList();

    // If the queue has already finished by the time the dialog is initialized, close it immediately!
    if (!worker->isBusy() && m_listPending->count() == 0) {
        QTimer::singleShot(0, this, &QDialog::accept);
    }
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

    m_lblSpeedTime = new QLabel("Speed: 0 B/s | Time Remaining: Calculating...", this);
    m_lblSpeedTime->setStyleSheet("font-size: 11px; color: #a6e3a1; font-weight: bold;");
    layout->addWidget(m_lblSpeedTime);

    m_chart = new SpeedChartWidget(this);
    layout->addWidget(m_chart);

    layout->addWidget(new QLabel("Pending Operations Queue:", this));
    m_listPending = new QListWidget(this);
    m_listPending->setMaximumHeight(70);
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
    m_chart->clearHistory();
    m_lblSpeedTime->setText("Speed: 0 B/s | Time Remaining: Calculating...");
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

void CopyQueueDialog::onCollisionDetected(const QString& srcPath, const QString& destPath, int* resolution) {
    FileCollisionDialog dlg(srcPath, destPath, this);
    dlg.exec();
    *resolution = dlg.resolution();
}

void CopyQueueDialog::onSpeedUpdated(double speedBytesPerSec, qint64 remainingSeconds) {
    double speedMB = speedBytesPerSec / (1024.0 * 1024.0);
    m_chart->addSpeedPoint(speedMB);
    m_lblSpeedTime->setText(QString("Speed: %1/s | Time Remaining: %2")
                            .arg(formatBytes(static_cast<qint64>(speedBytesPerSec)))
                            .arg(formatTime(remainingSeconds)));
}

QString CopyQueueDialog::formatTime(qint64 seconds) const {
    if (seconds <= 0) return "Calculating...";
    qint64 mins = seconds / 60;
    qint64 secs = seconds % 60;
    qint64 hours = mins / 60;
    mins = mins % 60;
    
    if (hours > 0) {
        return QString("%1h %2m %3s").arg(hours).arg(mins).arg(secs);
    } else if (mins > 0) {
        return QString("%1m %2s").arg(mins).arg(secs);
    }
    return QString("%1s").arg(secs);
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
