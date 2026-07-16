#include "pdfviewerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

PdfViewerWidget::PdfViewerWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);

    m_btnPrev = new QPushButton("◀ Prev", this);
    m_btnPrev->setEnabled(false);
    m_btnPrev->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background-color: #45475a; }");
    connect(m_btnPrev, &QPushButton::clicked, this, &PdfViewerWidget::onPrevPage);
    toolbar->addWidget(m_btnPrev);

    m_lblPageStatus = new QLabel("Page 0 of 0", this);
    m_lblPageStatus->setAlignment(Qt::AlignCenter);
    m_lblPageStatus->setStyleSheet("color: #cdd6f4; font-weight: bold; padding: 0 10px;");
    toolbar->addWidget(m_lblPageStatus);

    m_btnNext = new QPushButton("Next ▶", this);
    m_btnNext->setEnabled(false);
    m_btnNext->setStyleSheet(m_btnPrev->styleSheet());
    connect(m_btnNext, &QPushButton::clicked, this, &PdfViewerWidget::onNextPage);
    toolbar->addWidget(m_btnNext);

    toolbar->addStretch(1);

    QPushButton* btnZoomOut = new QPushButton("🔍-", this);
    btnZoomOut->setStyleSheet(m_btnPrev->styleSheet());
    connect(btnZoomOut, &QPushButton::clicked, this, &PdfViewerWidget::onZoomOut);
    toolbar->addWidget(btnZoomOut);

    QPushButton* btnZoomIn = new QPushButton("🔍+", this);
    btnZoomIn->setStyleSheet(m_btnPrev->styleSheet());
    connect(btnZoomIn, &QPushButton::clicked, this, &PdfViewerWidget::onZoomIn);
    toolbar->addWidget(btnZoomIn);

    mainLayout->addLayout(toolbar);

    // Scroll Area for Canvas
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setStyleSheet("background-color: #11111b; border: 1px solid #313244; border-radius: 4px;");

    m_lblCanvas = new QLabel(m_scrollArea);
    m_lblCanvas->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_lblCanvas);

    mainLayout->addWidget(m_scrollArea, 1);

    m_lblStatusText = new QLabel("Select a PDF file to preview.", this);
    m_lblStatusText->setStyleSheet("color: #a6adc8; font-size: 11px;");
    mainLayout->addWidget(m_lblStatusText);
}

PdfViewerWidget::~PdfViewerWidget() {
    clear();
}

void PdfViewerWidget::clear() {
    if (m_renderProcess) {
        m_renderProcess->kill();
        m_renderProcess->waitForFinished();
        delete m_renderProcess;
        m_renderProcess = nullptr;
    }
    m_pageFiles.clear();
    m_currentPage = 0;
    m_zoomFactor = 1.0;
    m_btnPrev->setEnabled(false);
    m_btnNext->setEnabled(false);
    m_lblPageStatus->setText("Page 0 of 0");
    m_lblCanvas->clear();
    m_lblStatusText->setText("Ready");
}

void PdfViewerWidget::loadPdf(const QString& filePath) {
    clear();
    m_pdfPath = filePath;

    if (m_pdfPath.isEmpty() || !QFile::exists(m_pdfPath)) {
        m_lblStatusText->setText("Error: PDF file does not exist.");
        return;
    }

    renderPages();
}

void PdfViewerWidget::renderPages() {
    // Check if pdftoppm is installed
    QProcess checkProc;
    checkProc.start("which", {"pdftoppm"});
    checkProc.waitForFinished();
    if (checkProc.exitCode() != 0) {
        m_lblStatusText->setText("Error: pdftoppm not found. Please install poppler-utils package.");
        m_lblStatusText->setStyleSheet("color: #f38ba8;");
        return;
    }

    m_lblStatusText->setText("Rendering PDF pages asynchronously...");
    m_lblStatusText->setStyleSheet("color: #89b4fa;");

    m_renderProcess = new QProcess(this);
    QString prefix = m_tempDir.path() + "/page";

    // Start pdftoppm process to extract all pages as PNG
    m_renderProcess->start("pdftoppm", {"-png", "-r", "130", m_pdfPath, prefix});
    connect(m_renderProcess, &QProcess::finished, this, &PdfViewerWidget::onRenderFinished);
}

void PdfViewerWidget::onRenderFinished(int exitCode) {
    if (exitCode != 0) {
        m_lblStatusText->setText("Error: Failed to render PDF document.");
        m_lblStatusText->setStyleSheet("color: #f38ba8;");
        return;
    }

    // Scan temporary directory for generated page files
    QDir dir(m_tempDir.path());
    QStringList filters;
    filters << "page-*.png";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    // Sort numerically to prevent page sorting issues (e.g., page-10 before page-2)
    std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) {
        auto getNum = [](const QString& str) {
            QString numPart = str;
            numPart.remove("page-").remove(".png");
            return numPart.toInt();
        };
        return getNum(a) < getNum(b);
    });

    m_pageFiles.clear();
    for (const QString& file : files) {
        m_pageFiles << dir.filePath(file);
    }

    if (m_pageFiles.isEmpty()) {
        m_lblStatusText->setText("Error: No pages rendered.");
        m_lblStatusText->setStyleSheet("color: #f38ba8;");
        return;
    }

    m_currentPage = 0;
    m_zoomFactor = 1.0;
    displayPage();
    m_lblStatusText->setText("PDF document loaded successfully.");
    m_lblStatusText->setStyleSheet("color: #a6e3a1;");
}

void PdfViewerWidget::displayPage() {
    if (m_pageFiles.isEmpty() || m_currentPage < 0 || m_currentPage >= m_pageFiles.size()) return;

    QPixmap pix(m_pageFiles[m_currentPage]);
    if (pix.isNull()) return;

    QSize newSize = pix.size() * m_zoomFactor;
    m_lblCanvas->setPixmap(pix.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_lblPageStatus->setText(QString("Page %1 of %2").arg(m_currentPage + 1).arg(m_pageFiles.size()));
    m_btnPrev->setEnabled(m_currentPage > 0);
    m_btnNext->setEnabled(m_currentPage < m_pageFiles.size() - 1);
}

void PdfViewerWidget::onPrevPage() {
    if (m_currentPage > 0) {
        m_currentPage--;
        displayPage();
    }
}

void PdfViewerWidget::onNextPage() {
    if (m_currentPage < m_pageFiles.size() - 1) {
        m_currentPage++;
        displayPage();
    }
}

void PdfViewerWidget::onZoomIn() {
    m_zoomFactor += 0.15;
    if (m_zoomFactor > 3.0) m_zoomFactor = 3.0;
    displayPage();
}

void PdfViewerWidget::onZoomOut() {
    m_zoomFactor -= 0.15;
    if (m_zoomFactor < 0.3) m_zoomFactor = 0.3;
    displayPage();
}
