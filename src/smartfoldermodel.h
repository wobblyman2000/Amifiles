#pragma once

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QList>
#include <QRunnable>
#include <QMutex>
#include "favoritesmanager.h"

class SmartFolderModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit SmartFolderModel(QObject* parent = nullptr);
    ~SmartFolderModel() override = default;

    void setQueryRule(const DynamicFavoriteRule& rule);
    DynamicFavoriteRule queryRule() const { return m_rule; }

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
    DynamicFavoriteRule m_rule;
    QList<QFileInfo> m_files;
    mutable QMutex m_mutex;
};

class SmartScanWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    SmartScanWorker(const DynamicFavoriteRule& rule);
    void run() override;

signals:
    void scanFinished(const QList<QFileInfo>& files);

private:
    DynamicFavoriteRule m_rule;
};
