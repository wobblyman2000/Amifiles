#pragma once

#include <QStyledItemDelegate>

class TheaterViewDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TheaterViewDelegate(QObject* parent = nullptr);
    ~TheaterViewDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
