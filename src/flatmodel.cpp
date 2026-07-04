#include "flatmodel.h"
#include <QDirIterator>
#include <QDateTime>
#include <QFileIconProvider>
#include <QDir>
#include <QThreadPool>

FlatFileSystemModel::FlatFileSystemModel(QObject* parent) : QAbstractTableModel(parent) {}

void FlatFileSystemModel::setRootPath(const QString& path) {
    {
        QMutexLocker locker(&m_mutex);
        m_rootPath = QDir::cleanPath(path);
    }
    
    emit scanStarted();

    FlatScanWorker* worker = new FlatScanWorker(m_rootPath);
    connect(worker, &FlatScanWorker::finished, this, &FlatFileSystemModel::onScanFinished, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(worker);
}

void FlatFileSystemModel::onScanFinished(const QList<QFileInfo>& files) {
    beginResetModel();
    {
        QMutexLocker locker(&m_mutex);
        m_files = files;
    }
    endResetModel();
    emit scanFinished();
}

int FlatFileSystemModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    QMutexLocker locker(&m_mutex);
    return m_files.size();
}

int FlatFileSystemModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 5;
}

QVariant FlatFileSystemModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    int row = index.row();
    int col = index.column();

    QFileInfo info;
    {
        QMutexLocker locker(&m_mutex);
        if (row < 0 || row >= m_files.size()) return QVariant();
        info = m_files.at(row);
    }

    if (role == Qt::DisplayRole) {
        switch (col) {
            case 0:
                return info.fileName();
            case 1: {
                if (info.isDir()) return "";
                qint64 size = info.size();
                double kb = size / 1024.0;
                double mb = kb / 1024.0;
                double gb = mb / 1024.0;
                if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
                if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
                if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
                return QString("%1 B").arg(size);
            }
            case 2:
                return info.suffix().toUpper();
            case 3:
                return info.lastModified().toString("yyyy-MM-dd hh:mm:ss");
            case 4: {
                QMutexLocker locker(&m_mutex);
                QString rel = QDir(m_rootPath).relativeFilePath(info.absolutePath());
                return rel == "." ? "" : rel;
            }
        }
    } else if (role == Qt::DecorationRole && col == 0) {
        QFileIconProvider provider;
        return provider.icon(info);
    }

    return QVariant();
}

QVariant FlatFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "Name";
            case 1: return "Size";
            case 2: return "Type";
            case 3: return "Date Modified";
            case 4: return "Sub-Path";
        }
    }
    return QVariant();
}

QFileInfo FlatFileSystemModel::fileInfo(int row) const {
    QMutexLocker locker(&m_mutex);
    if (row < 0 || row >= m_files.size()) return QFileInfo();
    return m_files.at(row);
}

QString FlatFileSystemModel::filePath(const QModelIndex& index) const {
    if (!index.isValid()) return "";
    QMutexLocker locker(&m_mutex);
    int row = index.row();
    if (row < 0 || row >= m_files.size()) return "";
    return m_files.at(row).absoluteFilePath();
}

// FlatScanWorker

FlatScanWorker::FlatScanWorker(const QString& rootPath) : m_root(rootPath) {
    setAutoDelete(true);
}

void FlatScanWorker::run() {
    QList<QFileInfo> fileList;
    // Iterate directories recursively to build mixed flat representation
    QDirIterator it(m_root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        fileList.append(it.fileInfo());
    }
    emit finished(fileList);
}
