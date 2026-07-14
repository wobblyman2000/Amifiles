#ifndef SEARCHWORKER_H
#define SEARCHWORKER_H

#include <QObject>
#include <QStringList>
#include <atomic>

class SearchWorker : public QObject {
    Q_OBJECT
public:
    explicit SearchWorker(QObject* parent = nullptr);
    ~SearchWorker() override = default;

    void cancel();

public slots:
    void doSearch(const QString& query, const QString& path);

signals:
    void resultsReady(const QStringList& results);
    void searchFinished();

private:
    std::atomic<bool> m_cancel;
};

#endif // SEARCHWORKER_H
