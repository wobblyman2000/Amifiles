#include "smartfoldermodel.h"
#include "tagmanager.h"
#include <QDirIterator>
#include <QThreadPool>
#include <QDateTime>
#include <QRegularExpression>
#include <QFileIconProvider>

SmartFolderModel::SmartFolderModel(QObject* parent) : QAbstractTableModel(parent) {}

void SmartFolderModel::setQueryRule(const DynamicFavoriteRule& rule) {
    m_rule = rule;
    emit scanStarted();

    SmartScanWorker* worker = new SmartScanWorker(rule);
    connect(worker, &SmartScanWorker::scanFinished, this, &SmartFolderModel::onScanFinished);
    QThreadPool::globalInstance()->start(worker);
}

int SmartFolderModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    QMutexLocker locker(&m_mutex);
    return m_files.size();
}

int SmartFolderModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 4; // Name, Size, Type, Modified Date
}

QVariant SmartFolderModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    QMutexLocker locker(&m_mutex);
    if (index.row() >= m_files.size()) return QVariant();

    const QFileInfo& info = m_files[index.row()];

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return info.fileName();
        } else if (index.column() == 1) {
            double kb = info.size() / 1024.0;
            double mb = kb / 1024.0;
            return (mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1);
        } else if (index.column() == 2) {
            return info.suffix().toUpper();
        } else if (index.column() == 3) {
            return info.lastModified().toString("yyyy-MM-dd hh:mm");
        }
    } else if (role == Qt::DecorationRole && index.column() == 0) {
        QFileIconProvider provider;
        return provider.icon(info);
    } else if (role == Qt::UserRole) {
        return info.absoluteFilePath();
    }
    return QVariant();
}

QVariant SmartFolderModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) return "Name";
        if (section == 1) return "Size";
        if (section == 2) return "Type";
        if (section == 3) return "Modified";
    }
    return QVariant();
}

QFileInfo SmartFolderModel::fileInfo(int row) const {
    QMutexLocker locker(&m_mutex);
    if (row >= 0 && row < m_files.size()) {
        return m_files[row];
    }
    return QFileInfo();
}

QString SmartFolderModel::filePath(const QModelIndex& index) const {
    if (!index.isValid()) return "";
    QMutexLocker locker(&m_mutex);
    if (index.row() >= 0 && index.row() < m_files.size()) {
        return m_files[index.row()].absoluteFilePath();
    }
    return "";
}

void SmartFolderModel::onScanFinished(const QList<QFileInfo>& files) {
    beginResetModel();
    {
        QMutexLocker locker(&m_mutex);
        m_files = files;
    }
    endResetModel();
    emit scanFinished();
}

SmartScanWorker::SmartScanWorker(const DynamicFavoriteRule& rule) : m_rule(rule) {}

void SmartScanWorker::run() {
    QList<QFileInfo> files;
    QString home = QDir::homePath();
    QDateTime now = QDateTime::currentDateTime();

    QDirIterator it(home, QDir::Files, QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext() && files.size() < 1500 && count < 8000) {
        it.next();
        count++;

        QString filePath = it.filePath();
        if (filePath.contains("/.") || filePath.contains("\\.")) continue;

        QFileInfo info = it.fileInfo();
        bool matches = false;

        if (m_rule.ruleType == "Wildcard") {
            QRegularExpression re(QRegularExpression::wildcardToRegularExpression(m_rule.value));
            matches = re.match(info.fileName()).hasMatch();
        } else if (m_rule.ruleType == "Tag") {
            QStringList tags = TagManager::instance().getFileTags(filePath);
            matches = tags.contains(m_rule.value);
        } else if (m_rule.ruleType == "Recent") {
            int hours = m_rule.value.toInt();
            if (hours <= 0) hours = 24;
            matches = (info.lastModified().secsTo(now) <= hours * 3600);
        }

        if (matches) {
            files.append(info);
        }
    }

    emit scanFinished(files);
}
