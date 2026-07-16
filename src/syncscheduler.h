#pragma once

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QList>

struct SyncJob {
    QString id;
    QString name;
    QString sourcePath;
    QString destPath;
    int intervalMinutes = 60;
    QDateTime lastRun;
    bool active = true;
};

class SyncScheduler : public QObject {
    Q_OBJECT
public:
    static SyncScheduler& instance();

    QList<SyncJob> jobs() const { return m_jobs; }
    void setJobs(const QList<SyncJob>& jobs);

    void start();
    void stop();

signals:
    void syncStarted(const QString& jobName);
    void syncFinished(const QString& jobName, bool success);

private slots:
    void checkJobs();

private:
    explicit SyncScheduler(QObject* parent = nullptr);
    ~SyncScheduler() override = default;

    void loadJobs();
    void saveJobs();
    void executeSync(const SyncJob& job);
    bool copyPath(const QString& src, const QString& dest);

    QList<SyncJob> m_jobs;
    QTimer* m_timer = nullptr;
};
