#include "syncscheduler.h"
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QThread>

SyncScheduler& SyncScheduler::instance() {
    static SyncScheduler inst;
    return inst;
}

SyncScheduler::SyncScheduler(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SyncScheduler::checkJobs);
    loadJobs();
}

void SyncScheduler::start() {
    m_timer->start(30000); // Check every 30 seconds
    qDebug() << "Background Sync Scheduler started.";
}

void SyncScheduler::stop() {
    m_timer->stop();
    qDebug() << "Background Sync Scheduler stopped.";
}

void SyncScheduler::setJobs(const QList<SyncJob>& jobs) {
    m_jobs = jobs;
    saveJobs();
}

void SyncScheduler::checkJobs() {
    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < m_jobs.size(); ++i) {
        SyncJob& job = m_jobs[i];
        if (!job.active) continue;

        bool run = false;
        if (!job.lastRun.isValid()) {
            run = true;
        } else {
            qint64 secs = job.lastRun.secsTo(now);
            if (secs >= job.intervalMinutes * 60) {
                run = true;
            }
        }

        if (run) {
            job.lastRun = now;
            saveJobs();
            
            // Execute sync in a separate thread or background logic
            // To keep it simple, fast, and safe, we run it in Qt Concurrent or QThread run
            QThread* thread = QThread::create([this, job]() {
                executeSync(job);
            });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            thread->start();
        }
    }
}

void SyncScheduler::executeSync(const SyncJob& job) {
    emit syncStarted(job.name);
    qDebug() << "Executing sync job:" << job.name << "from" << job.sourcePath << "to" << job.destPath;

    bool success = copyPath(job.sourcePath, job.destPath);
    
    emit syncFinished(job.name, success);
    qDebug() << "Sync job finished:" << job.name << (success ? "Success" : "Failed");
}

bool SyncScheduler::copyPath(const QString& src, const QString& dest) {
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) return false;

    if (srcInfo.isFile()) {
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (QFile::exists(dest)) {
            // If target file exists and has same size and modified time, skip to be super fast!
            QFileInfo destInfo(dest);
            if (srcInfo.size() == destInfo.size() && qAbs(srcInfo.lastModified().secsTo(destInfo.lastModified())) < 2) {
                return true;
            }
            QFile::remove(dest);
        }
        return QFile::copy(src, dest);
    } else if (srcInfo.isDir()) {
        QDir srcDir(src);
        QDir destDir(dest);
        if (!destDir.exists()) {
            if (!destDir.mkpath(".")) return false;
        }

        QStringList entries = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            QString srcEntry = srcDir.absoluteFilePath(entry);
            QString destEntry = destDir.absoluteFilePath(entry);
            if (!copyPath(srcEntry, destEntry)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void SyncScheduler::loadJobs() {
    m_jobs.clear();
    QSettings settings("Amifiles", "Amifiles");
    settings.beginGroup("SyncScheduler");
    QStringList keys = settings.childGroups();
    for (const QString& key : keys) {
        settings.beginGroup(key);
        SyncJob job;
        job.id = key;
        job.name = settings.value("name").toString();
        job.sourcePath = settings.value("source").toString();
        job.destPath = settings.value("dest").toString();
        job.intervalMinutes = settings.value("interval", 60).toInt();
        job.lastRun = settings.value("lastRun").toDateTime();
        job.active = settings.value("active", true).toBool();
        m_jobs.append(job);
        settings.endGroup();
    }
    settings.endGroup();
}

void SyncScheduler::saveJobs() {
    QSettings settings("Amifiles", "Amifiles");
    settings.remove("SyncScheduler"); // Clear group
    settings.beginGroup("SyncScheduler");
    for (const auto& job : m_jobs) {
        settings.beginGroup(job.id);
        settings.setValue("name", job.name);
        settings.setValue("source", job.sourcePath);
        settings.setValue("dest", job.destPath);
        settings.setValue("interval", job.intervalMinutes);
        settings.setValue("lastRun", job.lastRun);
        settings.setValue("active", job.active);
        settings.endGroup();
    }
    settings.endGroup();
}
