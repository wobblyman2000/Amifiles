#ifndef IMAGEEDITORDIALOG_H
#define IMAGEEDITORDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QVector>

class ImageEditorCanvas : public QWidget {
    Q_OBJECT
public:
    enum Tool { ToolNone, ToolPen, ToolLine, ToolRect, ToolText };

    explicit ImageEditorCanvas(QWidget* parent = nullptr);
    void setImage(const QImage& img);
    QImage currentImage() const;

    void setTool(Tool tool);
    void setColor(const QColor& color);
    void rotateImage(bool clockwise);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct Annotation {
        Tool tool;
        QColor color;
        QVector<QPoint> points;
        QString text;
    };

    QImage m_baseImage;
    QPixmap m_displayPixmap;
    Tool m_currentTool = ToolPen;
    QColor m_currentColor = Qt::red;

    QVector<Annotation> m_annotations;
    QVector<QPoint> m_currentPoints;
    bool m_drawing = false;
};

class ImageEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageEditorDialog(const QString& imagePath, QWidget* parent = nullptr);
    ~ImageEditorDialog() override = default;

private slots:
    void onSave();
    void onRotateCW();
    void onRotateCCW();
    void onToolChanged(int index);
    void onColorChanged(int index);

private:
    void setupUI();

    QString m_imagePath;
    ImageEditorCanvas* m_canvas = nullptr;
    class QComboBox* m_comboTool = nullptr;
    class QComboBox* m_comboColor = nullptr;
};

#endif // IMAGEEDITORDIALOG_H
