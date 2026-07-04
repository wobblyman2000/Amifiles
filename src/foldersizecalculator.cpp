#include "foldersizecalculator.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

FolderSizeCalculator& FolderSizeCalculator::instance() {
    static FolderSizeCalculator inst;
    return inst;
}

FolderSizeCalculator::FolderSizeCalculator(QObject* parent) : QObject(parent) {}

qint64 FolderSizeCalculator::getFolderSize(const QString& path) {
    QMutexLocker locker(&m_mutex);

    if (m_cache.contains(path)) {
        return m_cache.value(path);
    }

    if (m_activeJobs.contains(path)) {
        return -1;
    }

    m_activeJobs.insert(path);

    SizeWorker* worker = new SizeWorker(path);
    connect(worker, &SizeWorker::finished, this, &FolderSizeCalculator::onJobFinished, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(worker);

    return -1;
}

bool FolderSizeCalculator::isCalculating(const QString& path) const {
    QMutexLocker locker(&m_mutex);
    return m_activeJobs.contains(path);
}

void FolderSizeCalculator::clearCache() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

void FolderSizeCalculator::onJobFinished(const QString& path, qint64 size) {
    {
        QMutexLocker locker(&m_mutex);
        m_cache.insert(path, size);
        m_activeJobs.remove(path);
    }
    emit sizeCalculated(path, size);
}

// SizeWorker Implementation

SizeWorker::SizeWorker(const QString& path) : m_path(path) {
    setAutoDelete(true);
}

void SizeWorker::run() {
    qint64 totalSize = 0;
    // Iterate files recursively to calculate exact size
    QDirIterator it(m_path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
    }
    emit finished(m_path, totalSize);
}
