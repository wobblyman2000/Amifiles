#include "fullscreenimageviewer.h"
#include <QPainter>
#include <QKeyEvent>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QTransform>
#include <QBrush>
#include <QPen>

FullscreenImageViewer::FullscreenImageViewer(const QStringList& imageFiles, int startIndex, QWidget* parent)
    : QDialog(parent), m_files(imageFiles), m_currentIndex(startIndex) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    m_slideshowTimer = new QTimer(this);
    connect(m_slideshowTimer, &QTimer::timeout, this, [this]() {
        if (!m_files.isEmpty()) {
            m_currentIndex = (m_currentIndex + 1) % m_files.size();
            m_rotation = 0;
            m_flipped = false;
            loadImage();
        }
    });

    loadImage();
    showFullScreen();
}

void FullscreenImageViewer::loadImage() {
    if (m_files.isEmpty() || m_currentIndex < 0 || m_currentIndex >= m_files.size()) return;

    m_originalPixmap.load(m_files[m_currentIndex]);
    m_transformedPixmap = m_originalPixmap;
    
    // Reapply transformations if needed
    if (m_rotation != 0 || m_flipped) {
        QTransform trans;
        if (m_rotation != 0) trans.rotate(m_rotation);
        if (m_flipped) trans.scale(-1, 1);
        m_transformedPixmap = m_originalPixmap.transformed(trans, Qt::SmoothTransformation);
    }
    update();
}

void FullscreenImageViewer::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Black background
    painter.fillRect(rect(), QColor("#11111b"));

    if (m_transformedPixmap.isNull()) {
        painter.setPen(QColor("#f38ba8"));
        painter.drawText(rect(), Qt::AlignCenter, "Failed to load image file.");
        return;
    }

    // Centered scaled image rendering
    QSize viewSize = size();
    QPixmap scaled = m_transformedPixmap.scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int imgX = (width() - scaled.width()) / 2;
    int imgY = (height() - scaled.height()) / 2;
    painter.drawPixmap(imgX, imgY, scaled);

    // Dynamic HUD layout calculations
    int hudW = qMin(800, width() - 40);
    int hudX = (width() - hudW) / 2;
    m_hudRect = QRect(hudX, height() - 90, hudW, 70);

    // Draw HUD background panel
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor(17, 17, 27, 220)));
    painter.drawRoundedRect(m_hudRect, 8, 8);

    // HUD Text Information (left-aligned)
    QFileInfo info(m_files[m_currentIndex]);
    QString displayTitle = info.fileName();
    QString displayMeta = QString("%1x%2 — %3 (Image %4 of %5)")
                          .arg(m_originalPixmap.width())
                          .arg(m_originalPixmap.height())
                          .arg(formatBytes(info.size()))
                          .arg(m_currentIndex + 1)
                          .arg(m_files.size());

    painter.setPen(QColor("#cdd6f4"));
    QFont fontTitle = font();
    fontTitle.setPointSize(10);
    fontTitle.setBold(true);
    painter.setFont(fontTitle);
    painter.drawText(QRect(m_hudRect.left() + 16, m_hudRect.top() + 15, m_hudRect.width() / 2, 20), Qt::AlignLeft | Qt::AlignVCenter, displayTitle);

    painter.setPen(QColor("#a6adc8"));
    QFont fontMeta = font();
    fontMeta.setPointSize(8);
    painter.setFont(fontMeta);
    painter.drawText(QRect(m_hudRect.left() + 16, m_hudRect.top() + 38, m_hudRect.width() / 2, 16), Qt::AlignLeft | Qt::AlignVCenter, displayMeta);

    // Draw Interactive Buttons (right-aligned in HUD)
    int btnW = 55;
    int btnH = 36;
    int btnY = m_hudRect.top() + 17;
    int rightBound = m_hudRect.right() - 16;

    m_btnCloseRect = QRect(rightBound - btnW, btnY, btnW, btnH);
    m_btnFlipRect = QRect(m_btnCloseRect.left() - btnW - 6, btnY, btnW, btnH);
    m_btnRotateRect = QRect(m_btnFlipRect.left() - btnW - 6, btnY, btnW, btnH);
    m_btnNextRect = QRect(m_btnRotateRect.left() - btnW - 12, btnY, btnW, btnH);
    m_btnPlayRect = QRect(m_btnNextRect.left() - btnW - 6, btnY, btnW, btnH);
    m_btnPrevRect = QRect(m_btnPlayRect.left() - btnW - 6, btnY, btnW, btnH);

    auto drawBtn = [&painter, this](const QRect& r, const QString& symbol, const QString& tooltip, QColor hoverCol) {
        bool hover = r.contains(mapFromGlobal(QCursor::pos()));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(hover ? hoverCol : QColor("#313244")));
        painter.drawRoundedRect(r, 4, 4);

        painter.setPen(QColor(hover ? "#11111b" : "#cdd6f4"));
        QFont fSym = font();
        fSym.setPointSize(11);
        fSym.setBold(true);
        painter.setFont(fSym);
        painter.drawText(r, Qt::AlignCenter, symbol);
    };

    drawBtn(m_btnPrevRect, "⏮", "Previous", QColor("#89b4fa"));
    drawBtn(m_btnPlayRect, m_slideshowActive ? "⏸" : "▶", m_slideshowActive ? "Pause Slideshow" : "Play Slideshow", QColor("#a6e3a1"));
    drawBtn(m_btnNextRect, "⏭", "Next", QColor("#89b4fa"));
    drawBtn(m_btnRotateRect, "🔄", "Rotate 90°", QColor("#f9e2af"));
    drawBtn(m_btnFlipRect, "↔️", "Flip Horizontal", QColor("#cba6f7"));
    drawBtn(m_btnCloseRect, "✖", "Close Viewer", QColor("#f38ba8"));

    // Draw Shortcut Help Card (top-left overlay)
    if (m_showHelp) {
        QRect helpRect(20, 20, 310, 160);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(17, 17, 27, 210)));
        painter.drawRoundedRect(helpRect, 6, 6);

        painter.setPen(QColor("#f5c2e7"));
        QFont fHelpHeader = font();
        fHelpHeader.setPointSize(10);
        fHelpHeader.setBold(true);
        painter.setFont(fHelpHeader);
        painter.drawText(helpRect.adjusted(12, 10, -12, -10), Qt::AlignTop | Qt::AlignLeft, "Interactive Shortcuts:");

        painter.setPen(QColor("#cdd6f4"));
        QFont fHelpBody = font();
        fHelpBody.setPointSize(9);
        painter.setFont(fHelpBody);
        QString helpTxt = "• [← / A] : Previous Image\n"
                          "• [→ / D] : Next Image\n"
                          "• [Space] : Toggle Slideshow (3s)\n"
                          "• [R] : Rotate 90° Clockwise\n"
                          "• [F] : Flip Horizontally\n"
                          "• [H] : Toggle Help Box\n"
                          "• [Esc / Q] : Close Fullscreen Viewer";
        painter.drawText(helpRect.adjusted(12, 32, -12, -10), Qt::AlignTop | Qt::AlignLeft, helpTxt);
    }
}

void FullscreenImageViewer::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Q) {
        close();
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_A) {
        if (!m_files.isEmpty()) {
            m_currentIndex = (m_currentIndex - 1 + m_files.size()) % m_files.size();
            m_rotation = 0;
            m_flipped = false;
            loadImage();
        }
    } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_D) {
        if (!m_files.isEmpty()) {
            m_currentIndex = (m_currentIndex + 1) % m_files.size();
            m_rotation = 0;
            m_flipped = false;
            loadImage();
        }
    } else if (event->key() == Qt::Key_R) {
        rotateImage();
    } else if (event->key() == Qt::Key_F) {
        flipImage();
    } else if (event->key() == Qt::Key_Space) {
        toggleSlideshow();
    } else if (event->key() == Qt::Key_H) {
        m_showHelp = !m_showHelp;
        update();
    } else {
        QDialog::keyPressEvent(event);
    }
}

void FullscreenImageViewer::mousePressEvent(QMouseEvent* event) {
    QPoint clickPos = event->pos();
    if (m_btnPrevRect.contains(clickPos)) {
        m_currentIndex = (m_currentIndex - 1 + m_files.size()) % m_files.size();
        m_rotation = 0;
        m_flipped = false;
        loadImage();
    } else if (m_btnNextRect.contains(clickPos)) {
        m_currentIndex = (m_currentIndex + 1) % m_files.size();
        m_rotation = 0;
        m_flipped = false;
        loadImage();
    } else if (m_btnPlayRect.contains(clickPos)) {
        toggleSlideshow();
    } else if (m_btnRotateRect.contains(clickPos)) {
        rotateImage();
    } else if (m_btnFlipRect.contains(clickPos)) {
        flipImage();
    } else if (m_btnCloseRect.contains(clickPos)) {
        close();
    } else {
        QDialog::mousePressEvent(event);
    }
}

void FullscreenImageViewer::mouseMoveEvent(QMouseEvent* /*event*/) {
    // Force redraw to update button hover states
    update();
}

void FullscreenImageViewer::rotateImage() {
    m_rotation = (m_rotation + 90) % 360;
    loadImage();
}

void FullscreenImageViewer::flipImage() {
    m_flipped = !m_flipped;
    loadImage();
}

void FullscreenImageViewer::toggleSlideshow() {
    m_slideshowActive = !m_slideshowActive;
    if (m_slideshowActive) {
        m_slideshowTimer->start(3000); // 3 seconds transitions
    } else {
        m_slideshowTimer->stop();
    }
    update();
}

QString FullscreenImageViewer::formatBytes(qint64 bytes) const {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    if (mb >= 1.0) return QString::number(mb, 'f', 1) + " MB";
    if (kb >= 1.0) return QString::number(kb, 'f', 1) + " KB";
    return QString::number(bytes) + " B";
}
