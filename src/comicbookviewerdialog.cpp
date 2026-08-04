#include "comicbookviewerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QKeyEvent>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QBuffer>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>

ComicBookViewerDialog::ComicBookViewerDialog(const QString& archivePath, QWidget* parent)
    : QDialog(parent), m_archivePath(archivePath) {
    setWindowTitle(QString("Comic Book Viewer - %1").arg(QFileInfo(archivePath).fileName()));
    resize(800, 900);
    setupUI();
    loadArchiveListing();
    displayPage();
}

void ComicBookViewerDialog::setupUI() {
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QScrollArea { background-color: #11111b; border: 1px solid #313244; border-radius: 6px; }"
        "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45475a; }"
        "QPushButton:disabled { background-color: #181825; color: #585b70; border: 1px solid #313244; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Scroll Area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_scrollArea->setWidget(m_imageLabel);

    mainLayout->addWidget(m_scrollArea, 1);

    // Navigation and Zoom Bar
    QHBoxLayout* barLayout = new QHBoxLayout();
    barLayout->setSpacing(8);

    // Left controls
    m_btnPrev = new QPushButton("◀ Prev", this);
    connect(m_btnPrev, &QPushButton::clicked, this, &ComicBookViewerDialog::onPrevPage);
    barLayout->addWidget(m_btnPrev);

    m_pageCombo = new QComboBox(this);
    connect(m_pageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ComicBookViewerDialog::onPageComboChanged);
    barLayout->addWidget(m_pageCombo);

    m_pageLabel = new QLabel("Page 0 of 0", this);
    m_pageLabel->setStyleSheet("font-weight: bold;");
    barLayout->addWidget(m_pageLabel);

    m_btnNext = new QPushButton("Next ▶", this);
    connect(m_btnNext, &QPushButton::clicked, this, &ComicBookViewerDialog::onNextPage);
    barLayout->addWidget(m_btnNext);

    barLayout->addStretch(1);

    // Zoom controls
    QPushButton* btnZoomOut = new QPushButton("➖ Zoom Out", this);
    connect(btnZoomOut, &QPushButton::clicked, this, &ComicBookViewerDialog::onZoomOut);
    barLayout->addWidget(btnZoomOut);

    QPushButton* btnZoomIn = new QPushButton("➕ Zoom In", this);
    connect(btnZoomIn, &QPushButton::clicked, this, &ComicBookViewerDialog::onZoomIn);
    barLayout->addWidget(btnZoomIn);

    QPushButton* btnFitWidth = new QPushButton("↔ Fit Width", this);
    connect(btnFitWidth, &QPushButton::clicked, this, &ComicBookViewerDialog::onFitWidth);
    barLayout->addWidget(btnFitWidth);

    QPushButton* btnFitPage = new QPushButton("⤢ Fit Page", this);
    connect(btnFitPage, &QPushButton::clicked, this, &ComicBookViewerDialog::onFitPage);
    barLayout->addWidget(btnFitPage);

    mainLayout->addLayout(barLayout);
}

void ComicBookViewerDialog::loadArchiveListing() {
    QFileInfo info(m_archivePath);
    QString ext = info.suffix().toLower();
    m_pages.clear();

    if (ext == "cbz") {
        QProcess proc;
        proc.start("unzip", { "-l", m_archivePath });
        if (proc.waitForFinished(5000)) {
            QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QStringList lines = stdoutText.split('\n');
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed.startsWith("Archive:") || trimmed.startsWith("Length") || trimmed.startsWith("---")) {
                    continue;
                }
                QStringList parts = trimmed.split(QRegularExpression("\\s+"));
                if (parts.size() >= 4) {
                    QString name = parts.mid(3).join(' ');
                    QString subExt = QFileInfo(name).suffix().toLower();
                    if (subExt == "jpg" || subExt == "jpeg" || subExt == "png" || subExt == "webp" || subExt == "gif" || subExt == "bmp") {
                        m_pages.append(name);
                    }
                }
            }
        }
    } else if (ext == "cbr") {
        QProcess proc;
        proc.start("unrar", { "lb", m_archivePath });
        if (proc.waitForFinished(5000)) {
            QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QStringList lines = stdoutText.split('\n');
            for (const QString& line : lines) {
                QString name = line.trimmed();
                if (name.isEmpty()) continue;
                QString subExt = QFileInfo(name).suffix().toLower();
                if (subExt == "jpg" || subExt == "jpeg" || subExt == "png" || subExt == "webp" || subExt == "gif" || subExt == "bmp") {
                    m_pages.append(name);
                }
            }
        }
    }

    // Sort naturally/alphabetically
    std::sort(m_pages.begin(), m_pages.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    m_pageCombo->blockSignals(true);
    m_pageCombo->clear();
    for (int i = 0; i < m_pages.size(); ++i) {
        m_pageCombo->addItem(QString("Page %1").arg(i + 1));
    }
    m_pageCombo->blockSignals(false);
}

void ComicBookViewerDialog::displayPage() {
    if (m_pages.isEmpty() || m_currentIndex < 0 || m_currentIndex >= m_pages.size()) {
        m_imageLabel->setText("No pages found in archive.");
        return;
    }

    QString targetPage = m_pages[m_currentIndex];
    QFileInfo info(m_archivePath);
    QString ext = info.suffix().toLower();
    QByteArray rawData;

    if (ext == "cbz") {
        QProcess proc;
        proc.start("unzip", { "-p", m_archivePath, targetPage });
        if (proc.waitForFinished(5000)) {
            rawData = proc.readAllStandardOutput();
        }
    } else if (ext == "cbr") {
        QProcess proc;
        proc.start("unrar", { "p", "-inul", m_archivePath, targetPage });
        if (proc.waitForFinished(5000)) {
            rawData = proc.readAllStandardOutput();
        }
    }

    if (!m_currentImage.loadFromData(rawData)) {
        m_imageLabel->setText(QString("Failed to load page %1").arg(m_currentIndex + 1));
        return;
    }

    m_pageCombo->blockSignals(true);
    m_pageCombo->setCurrentIndex(m_currentIndex);
    m_pageCombo->blockSignals(false);

    updateNavigationUI();
    resizeEvent(nullptr); // Force scale refresh
}

void ComicBookViewerDialog::updateNavigationUI() {
    m_btnPrev->setEnabled(m_currentIndex > 0);
    m_btnNext->setEnabled(m_currentIndex < m_pages.size() - 1);
    m_pageLabel->setText(QString("Page %1 of %2").arg(m_currentIndex + 1).arg(m_pages.size()));
}

void ComicBookViewerDialog::onPrevPage() {
    if (m_currentIndex > 0) {
        m_currentIndex--;
        displayPage();
    }
}

void ComicBookViewerDialog::onNextPage() {
    if (m_currentIndex < m_pages.size() - 1) {
        m_currentIndex++;
        displayPage();
    }
}

void ComicBookViewerDialog::onPageComboChanged(int index) {
    if (index >= 0 && index < m_pages.size()) {
        m_currentIndex = index;
        displayPage();
    }
}

void ComicBookViewerDialog::onZoomIn() {
    m_fitWidthMode = false;
    m_fitPageMode = false;
    m_zoomFactor *= 1.25;
    resizeEvent(nullptr);
}

void ComicBookViewerDialog::onZoomOut() {
    m_fitWidthMode = false;
    m_fitPageMode = false;
    m_zoomFactor *= 0.8;
    resizeEvent(nullptr);
}

void ComicBookViewerDialog::onFitWidth() {
    m_fitWidthMode = true;
    m_fitPageMode = false;
    resizeEvent(nullptr);
}

void ComicBookViewerDialog::onFitPage() {
    m_fitWidthMode = false;
    m_fitPageMode = true;
    resizeEvent(nullptr);
}

void ComicBookViewerDialog::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event);
    if (m_currentImage.isNull()) return;

    QSize vpSize = m_scrollArea->viewport()->size();
    QSize targetSize;

    if (m_fitPageMode) {
        targetSize = m_currentImage.size().scaled(vpSize, Qt::KeepAspectRatio);
    } else if (m_fitWidthMode) {
        double ratio = (double)vpSize.width() / m_currentImage.width();
        targetSize = QSize(vpSize.width(), m_currentImage.height() * ratio);
    } else {
        targetSize = m_currentImage.size() * m_zoomFactor;
    }

    QPixmap pix = QPixmap::fromImage(m_currentImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    m_imageLabel->setPixmap(pix);
    m_imageLabel->resize(targetSize);
}

void ComicBookViewerDialog::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_PageDown:
        case Qt::Key_Space:
            onNextPage();
            break;
        case Qt::Key_Left:
        case Qt::Key_PageUp:
        case Qt::Key_Backspace:
            onPrevPage();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            onZoomIn();
            break;
        case Qt::Key_Minus:
            onZoomOut();
            break;
        case Qt::Key_Escape:
            accept();
            break;
        default:
            QDialog::keyPressEvent(event);
    }
}
