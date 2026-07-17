#pragma once

#include <QTreeWidget>
#include <QModelIndex>

class TimelineView : public QTreeWidget {
    Q_OBJECT
public:
    explicit TimelineView(QWidget* parent = nullptr);
    ~TimelineView() override = default;

    void setRootPath(const QString& path);
    QString rootPath() const { return m_rootPath; }
    QStringList selectedPaths() const;

signals:
    void fileSelected(const QString& path);
    void fileDoubleClicked(const QString& path);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    QString m_rootPath;
};
