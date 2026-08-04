#ifndef COMICBOOKVIEWERDIALOG_H
#define COMICBOOKVIEWERDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QImage>
#include <QPixmap>

class QLabel;
class QScrollArea;
class QComboBox;
class QPushButton;

class ComicBookViewerDialog : public QDialog {
    Q_OBJECT
public:
    ComicBookViewerDialog(const QString& archivePath, QWidget* parent = nullptr);
    ~ComicBookViewerDialog() override = default;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onPrevPage();
    void onNextPage();
    void onPageComboChanged(int index);
    void onZoomIn();
    void onZoomOut();
    void onFitWidth();
    void onFitPage();

private:
    void setupUI();
    void loadArchiveListing();
    void displayPage();
    void updateNavigationUI();

    QString m_archivePath;
    QStringList m_pages;
    int m_currentIndex = 0;
    double m_zoomFactor = 1.0;
    bool m_fitWidthMode = false;
    bool m_fitPageMode = true;

    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_pageLabel = nullptr;
    QComboBox* m_pageCombo = nullptr;
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnNext = nullptr;

    QImage m_currentImage;
};

#endif // COMICBOOKVIEWERDIALOG_H
