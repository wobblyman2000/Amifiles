#pragma once

#include <QWidget>
#include <QListView>
#include <QLabel>
#include <QFileSystemModel>
#include <QModelIndex>

class FilmstripView : public QWidget {
    Q_OBJECT
public:
    explicit FilmstripView(QFileSystemModel* model, QWidget* parent = nullptr);
    ~FilmstripView() override = default;

    void setRootPath(const QString& path);
    QString rootPath() const { return m_rootPath; }
    QStringList selectedPaths() const;

signals:
    void fileSelected(const QString& path);
    void fileDoubleClicked(const QString& path);
    void customContextMenuRequested(const QPoint& pos);

private slots:
    void onItemClicked(const QModelIndex& index);
    void onItemDoubleClicked(const QModelIndex& index);

private:
    void updatePreview(const QString& path);

    QFileSystemModel* m_model = nullptr;
    QString m_rootPath;

    QLabel* m_previewLabel = nullptr;
    QListView* m_thumbList = nullptr;
};
