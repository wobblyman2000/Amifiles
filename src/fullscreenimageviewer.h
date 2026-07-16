#pragma once

#include <QDialog>
#include <QStringList>
#include <QPixmap>
#include <QTimer>

class FullscreenImageViewer : public QDialog {
    Q_OBJECT
public:
    explicit FullscreenImageViewer(const QStringList& imageFiles, int startIndex, QWidget* parent = nullptr);
    ~FullscreenImageViewer() override = default;

    QString currentFilePath() const {
        if (m_currentIndex >= 0 && m_currentIndex < m_files.size()) {
            return m_files[m_currentIndex];
        }
        return QString();
    }

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void loadImage();
    void rotateImage();
    void flipImage();
    void toggleSlideshow();
    QString formatBytes(qint64 bytes) const;

    QStringList m_files;
    int m_currentIndex = 0;
    QPixmap m_originalPixmap;
    QPixmap m_transformedPixmap;
    int m_rotation = 0;
    bool m_flipped = false;

    // Slideshow support
    QTimer* m_slideshowTimer = nullptr;
    bool m_slideshowActive = false;

    // GUI HUD rects for mouse click handling
    QRect m_hudRect;
    QRect m_btnPrevRect;
    QRect m_btnNextRect;
    QRect m_btnRotateRect;
    QRect m_btnFlipRect;
    QRect m_btnPlayRect;
    QRect m_btnCloseRect;

    bool m_showHelp = true;
};
