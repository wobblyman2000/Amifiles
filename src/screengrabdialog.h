#pragma once

#include <QDialog>
#include <QPixmap>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QKeyEvent>

class ScreenshotOverlay : public QWidget {
    Q_OBJECT
public:
    ScreenshotOverlay(const QPixmap& screenPixmap, QWidget* parent = nullptr);

signals:
    void selectionCaptured(const QPixmap& pixmap);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPixmap m_fullScreenPixmap;
    bool m_selecting = false;
    QPoint m_startPoint;
    QRect m_selectionRect;
};

class ScreenGrabDialog : public QDialog {
    Q_OBJECT
public:
    ScreenGrabDialog(QWidget* parent = nullptr);

private slots:
    void onCaptureClicked();
    void onSaveClicked();
    void onSelectionCaptured(const QPixmap& pixmap);

private:
    void performCapture();

    QRadioButton* m_radioFullScreen;
    QRadioButton* m_radioActiveWindow;
    QRadioButton* m_radioRegion;
    QCheckBox* m_checkDelay;
    QSpinBox* m_spinDelay;
    QLabel* m_previewLabel;
    QPushButton* m_btnSave;
    QPushButton* m_btnCapture;
    QPixmap m_capturedPixmap;
};
