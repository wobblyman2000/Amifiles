#pragma once

#include <QScrollArea>
#include <QListView>
#include <QHBoxLayout>
#include <QFileSystemModel>
#include <QModelIndex>

class MillerColumnsView : public QScrollArea {
    Q_OBJECT
public:
    explicit MillerColumnsView(QFileSystemModel* model, QWidget* parent = nullptr);
    ~MillerColumnsView() override = default;

    void setRootPath(const QString& path);
    QString rootPath() const { return m_rootPath; }
    QStringList selectedPaths() const;

signals:
    void fileSelected(const QString& path);
    void fileDoubleClicked(const QString& path);
    void customContextMenuRequested(const QPoint& pos);

private slots:
    void onColumnClicked(const QModelIndex& index, int colIdx);
    void onColumnDoubleClicked(const QModelIndex& index);

private:
    void clearColumnsRightOf(int colIdx);
    void addColumnForPath(const QString& path, const QModelIndex& index);
    void scrollToEnd();

    QFileSystemModel* m_model = nullptr;
    QString m_rootPath;
    QWidget* m_container = nullptr;
    QHBoxLayout* m_layout = nullptr;
    QList<QListView*> m_lists;
    QList<QString> m_colPaths;
};
