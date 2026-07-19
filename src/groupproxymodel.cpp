#include "groupproxymodel.h"
#include "tagmanager.h"
#include <QFileInfo>
#include <QFont>
#include <QColor>

GroupProxyModel::GroupProxyModel(QObject* parent) : QIdentityProxyModel(parent) {}

void GroupProxyModel::setSourceModel(QAbstractItemModel* sourceModel) {
    if (this->sourceModel()) {
        disconnect(this->sourceModel(), &QAbstractItemModel::modelReset, this, &GroupProxyModel::rebuildGroups);
        disconnect(this->sourceModel(), &QAbstractItemModel::layoutChanged, this, &GroupProxyModel::rebuildGroups);
        disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged, this, &GroupProxyModel::onSourceDataChanged);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsInserted, this, &GroupProxyModel::rebuildGroups);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsRemoved, this, &GroupProxyModel::rebuildGroups);
    }
    
    QIdentityProxyModel::setSourceModel(sourceModel);
    
    if (this->sourceModel()) {
        connect(this->sourceModel(), &QAbstractItemModel::modelReset, this, &GroupProxyModel::rebuildGroups);
        connect(this->sourceModel(), &QAbstractItemModel::layoutChanged, this, &GroupProxyModel::rebuildGroups);
        connect(this->sourceModel(), &QAbstractItemModel::dataChanged, this, &GroupProxyModel::onSourceDataChanged);
        connect(this->sourceModel(), &QAbstractItemModel::rowsInserted, this, &GroupProxyModel::rebuildGroups);
        connect(this->sourceModel(), &QAbstractItemModel::rowsRemoved, this, &GroupProxyModel::rebuildGroups);
    }
    rebuildGroups();
}

void GroupProxyModel::setGrouping(bool active, const QString& type, const QString& customKey) {
    beginResetModel();
    m_groupingActive = active;
    m_groupType = type;
    m_customKey = customKey;
    endResetModel();
    rebuildGroups();
}

void GroupProxyModel::rebuildGroups() {
    beginResetModel();
    m_groups.clear();
    m_groupMap.clear();

    if (!m_groupingActive || !sourceModel()) {
        endResetModel();
        return;
    }

    int sourceRows = sourceModel()->rowCount(m_sourceRoot);
    for (int r = 0; r < sourceRows; ++r) {
        QModelIndex sourceIdx = sourceModel()->index(r, 0, m_sourceRoot);
        if (!sourceIdx.isValid()) continue;

        QString groupVal = getGroupValue(sourceIdx);
        if (groupVal.isEmpty()) groupVal = "Unassigned / Other";

        if (!m_groups.contains(groupVal)) {
            m_groups.append(groupVal);
        }
        m_groupMap[groupVal].append(sourceIdx);
    }
    endResetModel();
}

QString GroupProxyModel::getGroupValue(const QModelIndex& sourceIndex) const {
    if (!sourceIndex.isValid() || !sourceModel()) return "";
    
    QString filePath = sourceIndex.data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        filePath = sourceModel()->data(sourceIndex, Qt::UserRole).toString();
    }
    
    if (m_groupType == "Artist") {
        for (int c = 4; c < sourceModel()->columnCount(); ++c) {
            QString header = sourceModel()->headerData(c, Qt::Horizontal).toString();
            if (header == "Artist") {
                QModelIndex idx = sourceModel()->index(sourceIndex.row(), c, sourceIndex.parent());
                return sourceModel()->data(idx).toString();
            }
        }
    } else if (m_groupType == "Album") {
        for (int c = 4; c < sourceModel()->columnCount(); ++c) {
            QString header = sourceModel()->headerData(c, Qt::Horizontal).toString();
            if (header == "Album") {
                QModelIndex idx = sourceModel()->index(sourceIndex.row(), c, sourceIndex.parent());
                return sourceModel()->data(idx).toString();
            }
        }
    } else if (m_groupType == "Genre") {
        for (int c = 4; c < sourceModel()->columnCount(); ++c) {
            QString header = sourceModel()->headerData(c, Qt::Horizontal).toString();
            if (header == "Genre") {
                QModelIndex idx = sourceModel()->index(sourceIndex.row(), c, sourceIndex.parent());
                return sourceModel()->data(idx).toString();
            }
        }
    } else if (m_groupType == "Rating") {
        int r = TagManager::instance().getFileRating(filePath);
        if (r <= 0) return "No Rating";
        return QString(r, QChar(0x2605)) + QString(5 - r, QChar(0x2606));
    } else if (m_groupType == "Type") {
        QFileInfo info(filePath);
        return info.suffix().toUpper() + " Files";
    } else if (m_groupType == "CustomText" && !m_customKey.isEmpty()) {
        return TagManager::instance().getCustomAttribute(filePath, m_customKey);
    }
    
    return "";
}

QModelIndex GroupProxyModel::mapToSource(const QModelIndex& proxyIndex) const {
    if (!m_groupingActive) {
        return QIdentityProxyModel::mapToSource(proxyIndex);
    }
    if (!proxyIndex.isValid() || !sourceModel()) {
        return QModelIndex();
    }

    quintptr id = proxyIndex.internalId();
    if (id > 0 && id <= 10000) {
        return QModelIndex();
    }

    int groupIdx = (id >> 16) - 1;
    int childRow = (id & 0xFFFF) - 1;

    if (groupIdx >= 0 && groupIdx < m_groups.size()) {
        QString groupName = m_groups[groupIdx];
        const QList<QModelIndex>& list = m_groupMap[groupName];
        if (childRow >= 0 && childRow < list.size()) {
            QModelIndex sourceIdx = list[childRow];
            return sourceModel()->index(sourceIdx.row(), proxyIndex.column(), sourceIdx.parent());
        }
    }
    return QModelIndex();
}

QModelIndex GroupProxyModel::mapFromSource(const QModelIndex& sourceIndex) const {
    if (!sourceIndex.isValid() || !sourceModel()) return QModelIndex();

    if (!m_groupingActive) {
        return QIdentityProxyModel::mapFromSource(sourceIndex);
    }

    for (int g = 0; g < m_groups.size(); ++g) {
        QString gName = m_groups[g];
        const QList<QModelIndex>& list = m_groupMap[gName];
        for (int r = 0; r < list.size(); ++r) {
            if (list[r].row() == sourceIndex.row() && list[r].parent() == sourceIndex.parent()) {
                quintptr id = ((g + 1) << 16) | (r + 1);
                return createIndex(r, sourceIndex.column(), id);
            }
        }
    }
    return QModelIndex();
}

QModelIndex GroupProxyModel::index(int row, int column, const QModelIndex& parent) const {
    if (!sourceModel()) return QModelIndex();

    if (!m_groupingActive) {
        return QIdentityProxyModel::index(row, column, parent);
    }

    if (!parent.isValid()) {
        if (row >= 0 && row < m_groups.size()) {
            return createIndex(row, column, row + 1);
        }
        return QModelIndex();
    }

    quintptr pid = parent.internalId();
    if (pid > 0 && pid <= 10000) {
        int groupIdx = pid - 1;
        if (groupIdx >= 0 && groupIdx < m_groups.size()) {
            QString groupName = m_groups[groupIdx];
            int childCount = m_groupMap[groupName].size();
            if (row >= 0 && row < childCount) {
                quintptr id = ((groupIdx + 1) << 16) | (row + 1);
                return createIndex(row, column, id);
            }
        }
    }
    return QModelIndex();
}

QModelIndex GroupProxyModel::parent(const QModelIndex& child) const {
    if (!m_groupingActive) {
        return QIdentityProxyModel::parent(child);
    }
    if (!child.isValid()) return QModelIndex();

    quintptr id = child.internalId();
    if (id > 0 && id <= 10000) {
        return QModelIndex();
    }

    int groupIdx = (id >> 16) - 1;
    if (groupIdx >= 0 && groupIdx < m_groups.size()) {
        return createIndex(groupIdx, 0, groupIdx + 1);
    }
    return QModelIndex();
}

int GroupProxyModel::rowCount(const QModelIndex& parent) const {
    if (!sourceModel()) return 0;

    if (!m_groupingActive) {
        return QIdentityProxyModel::rowCount(parent);
    }

    if (!parent.isValid()) {
        return m_groups.size();
    }

    quintptr pid = parent.internalId();
    if (pid > 0 && pid <= 10000) {
        int groupIdx = pid - 1;
        if (groupIdx >= 0 && groupIdx < m_groups.size()) {
            return m_groupMap[m_groups[groupIdx]].size();
        }
    }
    return 0;
}

int GroupProxyModel::columnCount(const QModelIndex& parent) const {
    if (!sourceModel()) return 0;
    
    if (!m_groupingActive) {
        return QIdentityProxyModel::columnCount(parent);
    }
    
    return sourceModel()->columnCount();
}

QVariant GroupProxyModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !sourceModel()) return QVariant();

    if (!m_groupingActive) {
        return QIdentityProxyModel::data(index, role);
    }

    quintptr id = index.internalId();
    if (id > 0 && id <= 10000) {
        int groupIdx = id - 1;
        if (groupIdx >= 0 && groupIdx < m_groups.size()) {
            QString groupName = m_groups[groupIdx];
            int count = m_groupMap[groupName].size();
            
            if (role == Qt::DisplayRole) {
                if (index.column() == 0) {
                    return QString("%1 (%2 items)").arg(groupName).arg(count);
                }
                return "";
            } else if (role == Qt::BackgroundRole) {
                return QColor("#1e1e2e");
            } else if (role == Qt::ForegroundRole) {
                return QColor("#89b4fa");
            } else if (role == Qt::FontRole) {
                QFont font;
                font.setBold(true);
                return font;
            }
        }
        return QVariant();
    }

    QModelIndex sourceIdx = mapToSource(index);
    return sourceModel()->data(sourceIdx, role);
}

QVariant GroupProxyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    return QIdentityProxyModel::headerData(section, orientation, role);
}

void GroupProxyModel::setSourceRoot(const QModelIndex& root) {
    if (m_sourceRoot != root) {
        m_sourceRoot = root;
        rebuildGroups();
    }
}

void GroupProxyModel::onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) {
    if (roles.size() == 1 && roles.first() == Qt::DecorationRole) {
        QModelIndex proxyTopLeft = mapFromSource(topLeft);
        QModelIndex proxyBottomRight = mapFromSource(bottomRight);
        if (proxyTopLeft.isValid() && proxyBottomRight.isValid()) {
            emit dataChanged(proxyTopLeft, proxyBottomRight, roles);
        }
        return;
    }
    rebuildGroups();
}
