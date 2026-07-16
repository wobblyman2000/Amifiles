#ifndef HEXEDITORWIDGET_H
#define HEXEDITORWIDGET_H

#include <QWidget>
#include <QString>

class QTextBrowser;

class HexEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit HexEditorWidget(QWidget* parent = nullptr);
    ~HexEditorWidget() override = default;

    void loadFile(const QString& filePath);
    void clear();

private:
    QTextBrowser* m_textBrowser = nullptr;
};

#endif // HEXEDITORWIDGET_H
