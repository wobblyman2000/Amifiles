#ifndef PDFVIEWERWIDGET_H
#define PDFVIEWERWIDGET_H

#include <QWidget>
#include <QStringList>
#include <QPixmap>
#include <QTemporaryDir>

class QLabel;
class QPushButton;
class QScrollArea;
class QProcess;

class PdfViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PdfViewerWidget(QWidget* parent = nullptr);
    ~PdfViewerWidget() override;

    void loadPdf(const QString& filePath);
    void clear();

private slots:
    void onPrevPage();
    void onNextPage();
    void onZoomIn();
    void onZoomOut();
    void onRenderFinished(int exitCode);

private:
    void renderPages();
    void displayPage();
    void cleanupTempFiles();

    QString m_pdfPath;
    QTemporaryDir m_tempDir;
    QStringList m_pageFiles;
    int m_currentPage = 0;
    double m_zoomFactor = 1.0;
    QProcess* m_renderProcess = nullptr;

    // UI elements
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnNext = nullptr;
    QLabel* m_lblPageStatus = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_lblCanvas = nullptr;
    QLabel* m_lblStatusText = nullptr;
};

#endif // PDFVIEWERWIDGET_H
