#pragma once

#include <QListView>
#include <QImage>
#include <QPointer>
#include <QColor>

class TheaterListView : public QListView {
    Q_OBJECT
public:
    explicit TheaterListView(QWidget* parent = nullptr);
    ~TheaterListView() override = default;

    void setBackdropPath(const QString& path);
    void clearBackdrop();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onBackdropLoaded(const QString& path, const QImage& image);

private:
    QString m_backdropPath;
    QImage m_backdropImage;
    QColor m_dominantColor = QColor("#11111b");
};
