#pragma once

#include <QStyledItemDelegate>

class CardViewDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit CardViewDelegate(QObject* parent = nullptr);
    ~CardViewDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
