#ifndef FLATMODEL_H
#define FLATMODEL_H

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QList>
#include <QRunnable>
#include <QMutex>

class FlatFileSystemModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit FlatFileSystemModel(QObject* parent = nullptr);
    ~FlatFileSystemModel() override = default;

    void setRootPath(const QString& path);
    QString rootPath() const { return m_rootPath; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QFileInfo fileInfo(int row) const;
    QString filePath(const QModelIndex& index) const;

signals:
    void scanStarted();
    void scanFinished();

private slots:
    void onScanFinished(const QList<QFileInfo>& files);

private:
    QString m_rootPath;
    QList<QFileInfo> m_files;
    mutable QMutex m_mutex;
};

class FlatScanWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    FlatScanWorker(const QString& rootPath);
    void run() override;

signals:
    void finished(const QList<QFileInfo>& files);

private:
    QString m_root;
};

#endif // FLATMODEL_H
