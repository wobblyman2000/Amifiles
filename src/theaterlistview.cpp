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
    viewport()->update();
}

void TheaterListView::onBackdropLoaded(const QString& path, const QImage& image) {
    if (m_backdropPath == path) {
        m_backdropImage = image;
        viewport()->update();
    }
}

void TheaterListView::paintEvent(QPaintEvent* event) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background color
    painter.fillRect(viewport()->rect(), QColor("#11111b"));

    // Draw backdrop image if loaded
    if (!m_backdropImage.isNull()) {
        painter.setOpacity(0.12);
        // Smoothly stretch across the viewport
        painter.drawImage(viewport()->rect(), m_backdropImage);
    }

    painter.end();

    // Call base paint event to render item cards
    QListView::paintEvent(event);
}
