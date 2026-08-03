#include "screengrabdialog.h"
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QEventLoop>
#include <QDateTime>
#include <QDir>
#include <QGroupBox>

ScreenshotOverlay::ScreenshotOverlay(const QPixmap& screenPixmap, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_fullScreenPixmap(screenPixmap)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowState(Qt::WindowFullScreen);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void ScreenshotOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_fullScreenPixmap);
    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    if (m_selecting && !m_selectionRect.isNull()) {
        painter.drawPixmap(m_selectionRect, m_fullScreenPixmap, m_selectionRect);
        painter.setPen(QPen(QColor(137, 180, 250), 2, Qt::SolidLine));
        painter.drawRect(m_selectionRect);
    }
}

void ScreenshotOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_startPoint = event->pos();
        m_selectionRect = QRect(m_startPoint, QSize());
        update();
    }
}

void ScreenshotOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (m_selecting) {
        m_selectionRect = QRect(m_startPoint, event->pos()).normalized();
        update();
    }
}

void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        if (m_selectionRect.width() > 5 && m_selectionRect.height() > 5) {
            QPixmap cropped = m_fullScreenPixmap.copy(m_selectionRect);
            emit selectionCaptured(cropped);
        }
        close();
    }
}

void ScreenshotOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    }
}

ScreenGrabDialog::ScreenGrabDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Screen Grab Tool");
    setMinimumSize(450, 480);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 10px; padding: 10px; color: #89b4fa; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }"
        "QRadioButton, QCheckBox { color: #cdd6f4; font-weight: normal; }"
        "QLabel { color: #cdd6f4; }"
        "QSpinBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 2px 6px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // Group 1: Capture Mode
    QGroupBox* modeGroup = new QGroupBox("Capture Mode", this);
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);
    m_radioFullScreen = new QRadioButton("Capture Entire Screen", this);
    m_radioActiveWindow = new QRadioButton("Capture Active Window", this);
    m_radioRegion = new QRadioButton("Select Custom Region", this);
    m_radioRegion->setChecked(true); // default to custom region snippet

    modeLayout->addWidget(m_radioFullScreen);
    modeLayout->addWidget(m_radioActiveWindow);
    modeLayout->addWidget(m_radioRegion);
    mainLayout->addWidget(modeGroup);

    // Group 2: Delay settings
    QGroupBox* delayGroup = new QGroupBox("Delay Capture", this);
    QHBoxLayout* delayLayout = new QHBoxLayout(delayGroup);
    m_checkDelay = new QCheckBox("Delay capture for", this);
    m_spinDelay = new QSpinBox(this);
    m_spinDelay->setRange(1, 10);
    m_spinDelay->setValue(3);
    m_spinDelay->setSuffix(" seconds");
    
    delayLayout->addWidget(m_checkDelay);
    delayLayout->addWidget(m_spinDelay);
    delayLayout->addStretch();
    mainLayout->addWidget(delayGroup);

    // Group 3: Preview Box
    QGroupBox* previewGroup = new QGroupBox("Preview", this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel("No capture taken yet.", this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 4px; color: #6c7086; min-height: 200px;");
    previewLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup);

    // Control buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_btnCapture = new QPushButton("Capture", this);
    m_btnCapture->setStyleSheet("QPushButton { background-color: #89b4fa; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 16px; }"
                                "QPushButton:hover { background-color: #b4befe; }");
    m_btnSave = new QPushButton("Save As...", this);
    m_btnSave->setEnabled(false);
    m_btnSave->setStyleSheet("QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 6px 16px; }"
                             "QPushButton:disabled { background-color: #45475a; color: #585b70; }"
                             "QPushButton:hover:enabled { background-color: #a6e3a1; opacity: 0.9; }");
    
    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; font-weight: bold; border-radius: 4px; padding: 6px 16px; }"
                            "QPushButton:hover { background-color: #45475a; }");

    btnLayout->addWidget(m_btnCapture);
    btnLayout->addWidget(m_btnSave);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    connect(m_btnCapture, &QPushButton::clicked, this, &ScreenGrabDialog::onCaptureClicked);
    connect(m_btnSave, &QPushButton::clicked, this, &ScreenGrabDialog::onSaveClicked);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void ScreenGrabDialog::onCaptureClicked() {
    performCapture();
}

void ScreenGrabDialog::performCapture() {
    // 1. Process Delay if requested
    if (m_checkDelay->isChecked()) {
        int seconds = m_spinDelay->value();
        hide();
        
        QEventLoop loop;
        QTimer::singleShot(seconds * 1000, &loop, &QEventLoop::quit);
        loop.exec();
    } else {
        hide();
        // Give the OS 150ms to animate window hide completely
        QEventLoop loop;
        QTimer::singleShot(150, &loop, &QEventLoop::quit);
        loop.exec();
    }

    QPixmap captured;
    if (m_radioFullScreen->isChecked()) {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            captured = screen->grabWindow(0);
        }
    } else if (m_radioActiveWindow->isChecked()) {
        QWidget* p = parentWidget();
        while (p && p->parentWidget()) {
            p = p->parentWidget();
        }
        if (p) {
            captured = p->grab();
        } else {
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) captured = screen->grabWindow(0);
        }
    } else if (m_radioRegion->isChecked()) {
        QScreen* screen = QGuiApplication::primaryScreen();
        QPixmap fullScreen;
        if (screen) {
            fullScreen = screen->grabWindow(0);
        }
        
        ScreenshotOverlay* overlay = new ScreenshotOverlay(fullScreen, nullptr);
        QEventLoop overlayLoop;
        connect(overlay, &ScreenshotOverlay::selectionCaptured, this, &ScreenGrabDialog::onSelectionCaptured);
        connect(overlay, &QWidget::destroyed, &overlayLoop, &QEventLoop::quit);
        overlay->show();
        overlay->activateWindow();
        overlay->setFocus();
        overlayLoop.exec();
        
        captured = m_capturedPixmap;
    }

    show();
    if (!captured.isNull()) {
        m_capturedPixmap = captured;
        m_previewLabel->setPixmap(m_capturedPixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_previewLabel->setStyleSheet("background-color: #11111b; border: 1px solid #a6e3a1; border-radius: 4px; min-height: 200px;");
        m_btnSave->setEnabled(true);
    }
}

void ScreenGrabDialog::onSelectionCaptured(const QPixmap& pixmap) {
    m_capturedPixmap = pixmap;
}

void ScreenGrabDialog::onSaveClicked() {
    if (m_capturedPixmap.isNull()) return;

    QString defaultName = QDir::homePath() + "/screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";
    QString fileName = QFileDialog::getSaveFileName(this, "Save Screenshot As", defaultName, "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp)");
    if (!fileName.isEmpty()) {
        if (m_capturedPixmap.save(fileName)) {
            QMessageBox::information(this, "Screenshot Saved", "Successfully saved screenshot to:\n" + fileName);
        } else {
            QMessageBox::warning(this, "Save Failed", "Failed to save screenshot.");
        }
    }
}
