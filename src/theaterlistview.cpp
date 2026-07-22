#include "theaterlistview.h"
#include <QPainter>
#include <QPaintEvent>
#include <QImageReader>
#include <QRunnable>
#include <QThreadPool>
#include <QPointer>

class BackdropLoader : public QRunnable {
public:
    BackdropLoader(QPointer<TheaterListView> view, const QString& path)
        : m_view(view), m_path(path) {
        setAutoDelete(true);
    }

    void run() override {
        if (!m_view) return;

        QImageReader reader(m_path);
        if (reader.canRead()) {
            QSize size = reader.size();
            if (size.isValid()) {
                size.scale(400, 300, Qt::KeepAspectRatio);
                reader.setScaledSize(size);
            }
            QImage img = reader.read();
            if (!img.isNull()) {
                QMetaObject::invokeMethod(m_view.data(), "onBackdropLoaded", Qt::QueuedConnection,
                                          Q_ARG(QString, m_path),
                                          Q_ARG(QImage, img));
            }
        }
    }

private:
    QPointer<TheaterListView> m_view;
    QString m_path;
};

TheaterListView::TheaterListView(QWidget* parent) : QListView(parent) {
    setViewMode(QListView::IconMode);
    setGridSize(QSize(170, 270));
    setSpacing(12);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setFlow(QListView::LeftToRight);
    setWrapping(true);

    // Apply dark, semi-transparent stylesheet to look unified with custom themes
    setStyleSheet("QListView { background: transparent; color: #cdd6f4; border: none; }"
                  "QListView::item { border: none; }"
                  "QListView::item:hover { background: transparent; }"
                  "QListView::item:selected { background: transparent; }");
}

void TheaterListView::setBackdropPath(const QString& path) {
    if (m_backdropPath == path) return;
    m_backdropPath = path;

    if (path.isEmpty()) {
        clearBackdrop();
        return;
    }

    QPointer<TheaterListView> ptr(this);
    QThreadPool::globalInstance()->start(new BackdropLoader(ptr, path));
}

void TheaterListView::clearBackdrop() {
    m_backdropPath = "";
    m_backdropImage = QImage();
    m_dominantColor = QColor("#11111b");
    viewport()->update();
}

void TheaterListView::onBackdropLoaded(const QString& path, const QImage& image) {
    if (m_backdropPath == path) {
        m_backdropImage = image;
        m_dominantColor = QColor("#11111b");
        if (!image.isNull()) {
            QImage small = image.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (!small.isNull() && small.width() > 0 && small.height() > 0) {
                m_dominantColor = QColor::fromRgba(small.pixel(0, 0));
            }
        }
        viewport()->update();
    }
}

void TheaterListView::paintEvent(QPaintEvent* event) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background color with ambient linear gradient
    QLinearGradient bgGrad(viewport()->rect().topLeft(), viewport()->rect().bottomRight());
    if (!m_backdropImage.isNull()) {
        QColor mixColor = m_dominantColor;
        mixColor.setAlpha(60); // subtle ambient aura
        bgGrad.setColorAt(0.0, mixColor);
        bgGrad.setColorAt(1.0, QColor("#11111b"));
    } else {
        bgGrad.setColorAt(0.0, QColor("#1e1e2e"));
        bgGrad.setColorAt(1.0, QColor("#11111b"));
    }
    painter.fillRect(viewport()->rect(), bgGrad);

    // Draw backdrop image if loaded
    if (!m_backdropImage.isNull()) {
        painter.setOpacity(0.06); // subtle overlay texture
        painter.drawImage(viewport()->rect(), m_backdropImage);
    }

    painter.end();

    // Call base paint event to render item cards
    QListView::paintEvent(event);
}
