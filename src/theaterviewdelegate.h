#pragma once

#include <QStyledItemDelegate>

class TheaterViewDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TheaterViewDelegate(QObject* parent = nullptr);
    ~TheaterViewDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void setCinemaMode(bool cinema) { m_isCinemaMode = cinema; }
    bool isCinemaMode() const { return m_isCinemaMode; }

    void setShowcaseViewMode(int mode) { m_showcaseViewMode = mode; }
    int showcaseViewMode() const { return m_showcaseViewMode; }

private:
    bool m_isCinemaMode = false;
    int m_showcaseViewMode = 6; // Default to existing Audio Showcase
    mutable QMap<QString, QList<QPixmap>> m_hoverFramesCache;
    mutable QMap<QString, bool> m_hoverLoading;
    mutable QString m_activeHoverPath;
    mutable int m_hoverFrameIndex = 0;
    mutable QTimer* m_hoverTimer = nullptr;
    void startHoverAnimation(const QString& path, QWidget* view) const;
    void stopHoverAnimation() const;
    void extractFramesInBackground(const QString& path, QWidget* view) const;
};
