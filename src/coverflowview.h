#pragma once

#include <QWidget>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QHash>

class CoverFlowView : public QWidget {
    Q_OBJECT
public:
    explicit CoverFlowView(QWidget* parent = nullptr);
    ~CoverFlowView() override = default;

    void setModel(QAbstractItemModel* model);
    void setRootIndex(const QModelIndex& index);
    void setSelectionModel(QItemSelectionModel* selectionModel);

    void setSelectedIndex(int index);
    int selectedIndex() const { return m_currentIndex; }
    QModelIndex indexAt(const QPoint& pos) const;

signals:
    void itemDoubleClicked(const QModelIndex& index);
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onModelReset();
    void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    QPixmap getItemPixmap(const QModelIndex& index);
    void updateSelection();
    QRect getCardRect(int itemIndex) const;

    QAbstractItemModel* m_model = nullptr;
    QPersistentModelIndex m_rootIndex;
    QItemSelectionModel* m_selectionModel = nullptr;

    int m_currentIndex = 0;
    QHash<QString, QPixmap> m_pixmapCache;
};
