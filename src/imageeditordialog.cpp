#include "imageeditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>

class ImageResizeDialog : public QDialog {
public:
    ImageResizeDialog(int currentW, int currentH, QWidget* parent = nullptr) : QDialog(parent), m_aspectRatio(double(currentW) / currentH) {
        setWindowTitle("Resize Image");
        
        QFormLayout* layout = new QFormLayout(this);
        
        m_spinWidth = new QSpinBox(this);
        m_spinWidth->setRange(1, 20000);
        m_spinWidth->setValue(currentW);
        
        m_spinHeight = new QSpinBox(this);
        m_spinHeight->setRange(1, 20000);
        m_spinHeight->setValue(currentH);
        
        m_checkAspect = new QCheckBox("Preserve aspect ratio", this);
        m_checkAspect->setChecked(true);
        
        layout->addRow("Width (px):", m_spinWidth);
        layout->addRow("Height (px):", m_spinHeight);
        layout->addRow(m_checkAspect);
        
        QHBoxLayout* buttons = new QHBoxLayout();
        QPushButton* ok = new QPushButton("OK", this);
        QPushButton* cancel = new QPushButton("Cancel", this);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        buttons->addWidget(ok);
        buttons->addWidget(cancel);
        layout->addRow(buttons);
        
        // Auto-update height/width when aspect ratio is preserved
        connect(m_spinWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int w) {
            if (m_checkAspect->isChecked() && !m_updating) {
                m_updating = true;
                m_spinHeight->setValue(qRound(w / m_aspectRatio));
                m_updating = false;
            }
        });
        
        connect(m_spinHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int h) {
            if (m_checkAspect->isChecked() && !m_updating) {
                m_updating = true;
                m_spinWidth->setValue(qRound(h * m_aspectRatio));
                m_updating = false;
            }
        });
        
        setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                      "QLabel { color: #cdd6f4; }"
                      "QSpinBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
                      "QCheckBox { color: #cdd6f4; }"
                      "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; padding: 6px 12px; border-radius: 4px; }"
                      "QPushButton:hover { background-color: #89b4fa; color: #11111b; }");
    }
    
    int widthValue() const { return m_spinWidth->value(); }
    int heightValue() const { return m_spinHeight->value(); }
    
private:
    QSpinBox* m_spinWidth;
    QSpinBox* m_spinHeight;
    QCheckBox* m_checkAspect;
    double m_aspectRatio;
    bool m_updating = false;
};

ImageEditorCanvas::ImageEditorCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 300);
}

void ImageEditorCanvas::setImage(const QImage& img) {
    m_baseImage = img;
    m_originalBaseImage = img;
    m_annotations.clear();
    update();
}

QImage ImageEditorCanvas::currentImage() const {
    // Bake all annotations and rotations into the final image
    QImage result = m_baseImage.copy();
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    // Map annotations onto image coordinate space
    double scaleX = static_cast<double>(m_baseImage.width()) / width();
    double scaleY = static_cast<double>(m_baseImage.height()) / height();

    for (const Annotation& ann : m_annotations) {
        painter.setPen(QPen(ann.color, 3 * scaleX));
        
        if (ann.tool == ToolPen && ann.points.size() > 1) {
            for (int i = 1; i < ann.points.size(); ++i) {
                painter.drawLine(ann.points[i-1] * scaleX, ann.points[i] * scaleX);
            }
        } else if (ann.tool == ToolLine && ann.points.size() == 2) {
            painter.drawLine(ann.points[0] * scaleX, ann.points[1] * scaleX);
        } else if (ann.tool == ToolRect && ann.points.size() == 2) {
            QRect rect(ann.points[0], ann.points[1]);
            painter.drawRect(QRect(rect.topLeft() * scaleX, rect.bottomRight() * scaleX));
        } else if (ann.tool == ToolText && !ann.points.isEmpty()) {
            painter.setFont(QFont("Outfit", 12 * scaleX, QFont::Bold));
            painter.drawText(ann.points[0] * scaleX, ann.text);
        }
    }
    return result;
}

void ImageEditorCanvas::setTool(Tool tool) {
    m_currentTool = tool;
}

void ImageEditorCanvas::setColor(const QColor& color) {
    m_currentColor = color;
}

void ImageEditorCanvas::rotateImage(bool clockwise) {
    QTransform transform;
    transform.rotate(clockwise ? 90 : -90);
    m_baseImage = m_baseImage.transformed(transform);
    m_annotations.clear(); // Clear markup on rotation to prevent coordinate mismatch
    update();
}

void ImageEditorCanvas::removeGreenScreen() {
    removeGreenScreen(0.40);
}

void ImageEditorCanvas::removeGreenScreen(double threshold) {
    if (m_originalBaseImage.isNull()) return;
    m_baseImage = m_originalBaseImage.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < m_baseImage.height(); ++y) {
        QRgb* row = (QRgb*)m_baseImage.scanLine(y);
        for (int x = 0; x < m_baseImage.width(); ++x) {
            QRgb pixel = row[x];
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            double sum = r + g + b;
            if (sum > 0) {
                double g_norm = (double)g / sum;
                if (g_norm > threshold && g > 60 && g > r * 1.15 && g > b * 1.15) {
                    row[x] = qRgba(0, 0, 0, 0);
                }
            }
        }
    }
    update();
}

void ImageEditorCanvas::resizeImage(int w, int h) {
    if (m_baseImage.isNull()) return;
    m_baseImage = m_baseImage.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (!m_originalBaseImage.isNull()) {
        m_originalBaseImage = m_originalBaseImage.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    m_annotations.clear(); // Clear markup to avoid coordinate mismatch on scale change
    update();
}

void ImageEditorCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Center fill background
    painter.fillRect(rect(), QColor("#11111b"));

    if (m_baseImage.isNull()) return;

    // Draw scaled base image
    m_displayPixmap = QPixmap::fromImage(m_baseImage).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int startX = (width() - m_displayPixmap.width()) / 2;
    int startY = (height() - m_displayPixmap.height()) / 2;
    painter.drawPixmap(startX, startY, m_displayPixmap);

    // Draw historical annotations
    for (const Annotation& ann : m_annotations) {
        painter.setPen(QPen(ann.color, 3));
        if (ann.tool == ToolPen && ann.points.size() > 1) {
            for (int i = 1; i < ann.points.size(); ++i) {
                painter.drawLine(ann.points[i-1], ann.points[i]);
            }
        } else if (ann.tool == ToolLine && ann.points.size() == 2) {
            painter.drawLine(ann.points[0], ann.points[1]);
        } else if (ann.tool == ToolRect && ann.points.size() == 2) {
            painter.drawRect(QRect(ann.points[0], ann.points[1]));
        } else if (ann.tool == ToolText && !ann.points.isEmpty()) {
            painter.setFont(QFont("Outfit", 12, QFont::Bold));
            painter.drawText(ann.points[0], ann.text);
        }
    }

    // Draw current active drawing markup
    if (m_drawing) {
        painter.setPen(QPen(m_currentColor, 3));
        if (m_currentTool == ToolPen && m_currentPoints.size() > 1) {
            for (int i = 1; i < m_currentPoints.size(); ++i) {
                painter.drawLine(m_currentPoints[i-1], m_currentPoints[i]);
            }
        } else if (m_currentTool == ToolLine && m_currentPoints.size() == 2) {
            painter.drawLine(m_currentPoints[0], m_currentPoints[1]);
        } else if (m_currentTool == ToolRect && m_currentPoints.size() == 2) {
            painter.drawRect(QRect(m_currentPoints[0], m_currentPoints[1]));
        }
    }
}

void ImageEditorCanvas::mousePressEvent(QMouseEvent* event) {
    if (m_baseImage.isNull()) return;

    if (m_currentTool == ToolText) {
        bool ok;
        QString text = QInputDialog::getText(this, "Add Text Annotation", "Enter text:", QLineEdit::Normal, QString(), &ok);
        if (ok && !text.isEmpty()) {
            Annotation ann;
            ann.tool = ToolText;
            ann.color = m_currentColor;
            ann.points.append(event->pos());
            ann.text = text;
            m_annotations.append(ann);
            update();
        }
    } else {
        m_drawing = true;
        m_currentPoints.clear();
        m_currentPoints.append(event->pos());
    }
}

void ImageEditorCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!m_drawing) return;

    if (m_currentTool == ToolPen) {
        m_currentPoints.append(event->pos());
    } else if (m_currentTool == ToolLine || m_currentTool == ToolRect) {
        if (m_currentPoints.size() < 2) {
            m_currentPoints.append(event->pos());
        } else {
            m_currentPoints[1] = event->pos();
        }
    }
    update();
}

void ImageEditorCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_drawing) return;
    m_drawing = false;

    if (m_currentTool == ToolPen) {
        m_currentPoints.append(event->pos());
    } else if (m_currentTool == ToolLine || m_currentTool == ToolRect) {
        if (m_currentPoints.size() >= 2) {
            m_currentPoints[1] = event->pos();
        }
    }

    if (m_currentPoints.size() >= 1) {
        Annotation ann;
        ann.tool = m_currentTool;
        ann.color = m_currentColor;
        ann.points = m_currentPoints;
        m_annotations.append(ann);
    }
    update();
}

ImageEditorDialog::ImageEditorDialog(const QString& imagePath, QWidget* parent)
    : QDialog(parent), m_imagePath(imagePath) {
    setupUI();
    
    QImage img(imagePath);
    if (!img.isNull()) {
        m_canvas->setImage(img);
    }
}

void ImageEditorDialog::setupUI() {
    setWindowTitle("Interactive Image Editor & Annotator");
    resize(1000, 650);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QComboBox { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Toolbar Layout
    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    toolbar->addWidget(new QLabel("Tool:", this));
    m_comboTool = new QComboBox(this);
    m_comboTool->addItems({"Freehand Pen", "Line", "Rectangle", "Text Annotation"});
    connect(m_comboTool, &QComboBox::currentIndexChanged, this, &ImageEditorDialog::onToolChanged);
    toolbar->addWidget(m_comboTool);

    toolbar->addWidget(new QLabel("Color:", this));
    m_comboColor = new QComboBox(this);
    m_comboColor->addItems({"Red", "Blue", "Green", "Yellow", "White"});
    connect(m_comboColor, &QComboBox::currentIndexChanged, this, &ImageEditorDialog::onColorChanged);
    toolbar->addWidget(m_comboColor);

    QPushButton* btnCW = new QPushButton("Rotate 90° ↻", this);
    btnCW->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 6px 12px; } QPushButton:hover { background-color: #45475a; }");
    connect(btnCW, &QPushButton::clicked, this, &ImageEditorDialog::onRotateCW);
    toolbar->addWidget(btnCW);

    QPushButton* btnCCW = new QPushButton("Rotate 90° ↺", this);
    btnCCW->setStyleSheet(btnCW->styleSheet());
    connect(btnCCW, &QPushButton::clicked, this, &ImageEditorDialog::onRotateCCW);
    toolbar->addWidget(btnCCW);

    QPushButton* btnChroma = new QPushButton("Remove Green Screen 🟢", this);
    btnChroma->setStyleSheet(btnCW->styleSheet());
    connect(btnChroma, &QPushButton::clicked, this, &ImageEditorDialog::onRemoveGreenScreen);
    toolbar->addWidget(btnChroma);

    QPushButton* btnResize = new QPushButton("Resize... 📐", this);
    btnResize->setStyleSheet(btnCW->styleSheet());
    connect(btnResize, &QPushButton::clicked, this, &ImageEditorDialog::onResizeImage);
    toolbar->addWidget(btnResize);

    toolbar->addWidget(new QLabel("Sensitivity:", this));
    m_sliderChroma = new QSlider(Qt::Horizontal, this);
    m_sliderChroma->setRange(10, 90);
    m_sliderChroma->setValue(40);
    m_sliderChroma->setFixedWidth(100);
    m_sliderChroma->setStyleSheet(
        "QSlider::groove:horizontal { border: 1px solid #313244; height: 6px; background: #181825; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #b4befe; width: 12px; margin: -3px 0; border-radius: 6px; }"
    );
    m_sliderChroma->setEnabled(false);
    toolbar->addWidget(m_sliderChroma);

    m_lblChroma = new QLabel("40%", this);
    m_lblChroma->setFixedWidth(30);
    m_lblChroma->setEnabled(false);
    toolbar->addWidget(m_lblChroma);

    connect(m_sliderChroma, &QSlider::valueChanged, this, &ImageEditorDialog::onChromaThresholdChanged);

    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    // Canvas
    m_canvas = new ImageEditorCanvas(this);
    mainLayout->addWidget(m_canvas, 1);

    // Action buttons
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->setSpacing(8);

    QPushButton* btnSave = new QPushButton("Save Annotations", this);
    btnSave->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(btnSave, &QPushButton::clicked, this, &ImageEditorDialog::onSave);
    bottom->addWidget(btnSave);

    QPushButton* btnCancel = new QPushButton("Cancel", this);
    btnCancel->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottom->addWidget(btnCancel);

    mainLayout->addLayout(bottom);
}

void ImageEditorDialog::onToolChanged(int index) {
    if (index == 0) m_canvas->setTool(ImageEditorCanvas::ToolPen);
    else if (index == 1) m_canvas->setTool(ImageEditorCanvas::ToolLine);
    else if (index == 2) m_canvas->setTool(ImageEditorCanvas::ToolRect);
    else if (index == 3) m_canvas->setTool(ImageEditorCanvas::ToolText);
}

void ImageEditorDialog::onColorChanged(int index) {
    if (index == 0) m_canvas->setColor(Qt::red);
    else if (index == 1) m_canvas->setColor(Qt::blue);
    else if (index == 2) m_canvas->setColor(Qt::green);
    else if (index == 3) m_canvas->setColor(Qt::yellow);
    else if (index == 4) m_canvas->setColor(Qt::white);
}

void ImageEditorDialog::onRotateCW() {
    m_canvas->rotateImage(true);
}

void ImageEditorDialog::onRotateCCW() {
    m_canvas->rotateImage(false);
}

void ImageEditorDialog::onRemoveGreenScreen() {
    if (m_sliderChroma && m_lblChroma) {
        m_sliderChroma->setEnabled(true);
        m_lblChroma->setEnabled(true);
        m_canvas->removeGreenScreen(m_sliderChroma->value() / 100.0);
    } else {
        m_canvas->removeGreenScreen();
    }
}

void ImageEditorDialog::onChromaThresholdChanged(int value) {
    if (m_lblChroma) {
        m_lblChroma->setText(QString("%1%").arg(value));
    }
    m_canvas->removeGreenScreen(value / 100.0);
}

void ImageEditorDialog::onSave() {
    QImage finalImg = m_canvas->currentImage();
    if (finalImg.save(m_imagePath)) {
        QMessageBox::information(this, "Success", "Image saved successfully!");
        accept();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save the image. Check file permissions.");
    }
}

void ImageEditorDialog::onResizeImage() {
    QImage curImg = m_canvas->currentImage();
    if (curImg.isNull()) return;
    
    ImageResizeDialog dlg(curImg.width(), curImg.height(), this);
    if (dlg.exec() == QDialog::Accepted) {
        int w = dlg.widthValue();
        int h = dlg.heightValue();
        m_canvas->resizeImage(w, h);
    }
}
