#pragma once

#include <QIdentityProxyModel>
#include <QModelIndex>
#include <QList>
#include <QMap>

class GroupProxyModel : public QIdentityProxyModel {
    Q_OBJECT
public:
    explicit GroupProxyModel(QObject* parent = nullptr);
    ~GroupProxyModel() override = default;

    void setSourceModel(QAbstractItemModel* sourceModel) override;

    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QModelIndex sibling(int row, int column, const QModelIndex& idx) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool isGroupingActive() const { return m_groupingActive; }
    void setGrouping(bool active, const QString& type, const QString& customKey = "");

    void setSourceRoot(const QModelIndex& root);
    QModelIndex sourceRoot() const { return m_sourceRoot; }

public slots:
    void rebuildGroups();
    void onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);

private:
    QString getGroupValue(const QModelIndex& sourceIndex) const;

    bool m_groupingActive = false;
    QString m_groupType; // "Artist", "Album", "Genre", "Type", "Rating", "CustomText"
    QString m_customKey;

    QList<QString> m_groups;
    QMap<QString, QList<QModelIndex>> m_groupMap;
    QModelIndex m_sourceRoot;
};
