#ifndef FOLDERSIZECALCULATOR_H
#define FOLDERSIZECALCULATOR_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QThreadPool>
#include <QRunnable>
#include <QMutex>

class FolderSizeCalculator : public QObject {
    Q_OBJECT
public:
    static FolderSizeCalculator& instance();

    // Returns -1 if size is not cached yet (submits a background job)
    qint64 getFolderSize(const QString& path);
    bool isCalculating(const QString& path) const;
    void clearCache();

signals:
    void sizeCalculated(const QString& path, qint64 size);

private slots:
    void onJobFinished(const QString& path, qint64 size);

private:
    explicit FolderSizeCalculator(QObject* parent = nullptr);
    ~FolderSizeCalculator() override = default;

    QHash<QString, qint64> m_cache;
    QSet<QString> m_activeJobs;
    mutable QMutex m_mutex;
};

class SizeWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    SizeWorker(const QString& path);
    void run() override;

signals:
    void finished(const QString& path, qint64 size);

private:
    QString m_path;
};

#endif // FOLDERSIZECALCULATOR_H
