#pragma once

#include "renameitemdelegate.h"

class CardViewDelegate : public RenameItemDelegate {
    Q_OBJECT
public:
    explicit CardViewDelegate(QObject* parent = nullptr);
    ~CardViewDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
