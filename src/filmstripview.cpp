#include "filmstripview.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QFileIconProvider>
#include <QDir>

FilmstripView::FilmstripView(QFileSystemModel* model, QWidget* parent)
    : QWidget(parent), m_model(model) {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(6);

    // Top: Large Preview Scroll Area
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; }");

    m_previewLabel = new QLabel(scroll);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("color: #a6adc8; font-style: italic; background-color: #11111b;");
    m_previewLabel->setText("Select an image from the filmstrip below");
    scroll->setWidget(m_previewLabel);

    mainLayout->addWidget(scroll, 1);

    // Bottom: Horizontal Thumbnail Carousel QListView
    m_thumbList = new QListView(this);
    m_thumbList->setModel(m_model);
    m_thumbList->setViewMode(QListView::IconMode);
    m_thumbList->setFlow(QListView::LeftToRight);
    m_thumbList->setMovement(QListView::Static);
    m_thumbList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_thumbList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_thumbList->setIconSize(QSize(48, 48));
    m_thumbList->setFixedHeight(95);
    m_thumbList->setSpacing(8);
    m_thumbList->setStyleSheet(
        "QListView { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }"
        "QListView::item { border: 1px solid transparent; padding: 4px; border-radius: 4px; }"
        "QListView::item:hover { background-color: #313244; }"
        "QListView::item:selected { background-color: #89b4fa; color: #11111b; border: 1px solid #89b4fa; }"
    );

    connect(m_thumbList, &QListView::clicked, this, &FilmstripView::onItemClicked);
    connect(m_thumbList, &QListView::doubleClicked, this, &FilmstripView::onItemDoubleClicked);
    m_thumbList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_thumbList, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        emit customContextMenuRequested(pos);
    });

    mainLayout->addWidget(m_thumbList);
}

void FilmstripView::setRootPath(const QString& path) {
    m_rootPath = path;
    m_thumbList->setRootIndex(m_model->index(path));
    m_previewLabel->clear();
    m_previewLabel->setText("Select an image from the filmstrip below");

    // Select the first image automatically if possible
    QDir dir(path);
    QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.webp", "*.bmp", "*.gif"};
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
    if (!list.isEmpty()) {
        updatePreview(list.first().absoluteFilePath());
    }
}

void FilmstripView::onItemClicked(const QModelIndex& index) {
    QString path = m_model->filePath(index);
    emit fileSelected(path);
    updatePreview(path);
}

void FilmstripView::onItemDoubleClicked(const QModelIndex& index) {
    QString path = m_model->filePath(index);
    emit fileDoubleClicked(path);
}

void FilmstripView::updatePreview(const QString& path) {
    QFileInfo info(path);
    QStringList imageExts = {"jpg", "jpeg", "png", "webp", "bmp", "gif"};

    if (info.isFile() && imageExts.contains(info.suffix().toLower())) {
        QPixmap pix(path);
        if (!pix.isNull()) {
            // Scale keeping aspect ratio to preview pane boundaries
            int maxW = m_previewLabel->width() > 100 ? m_previewLabel->width() : 400;
            int maxH = m_previewLabel->height() > 100 ? m_previewLabel->height() : 300;
            m_previewLabel->setPixmap(pix.scaled(maxW - 20, maxH - 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }

    // Non-image fallback: show large file icon and details
    QFileIconProvider provider;
    QIcon icon = provider.icon(info);
    if (!icon.isNull()) {
        m_previewLabel->setPixmap(icon.pixmap(128, 128));
    } else {
        m_previewLabel->setText(QString("No visual preview available:\n%1").arg(info.fileName()));
    }
}

QStringList FilmstripView::selectedPaths() const {
    QStringList paths;
    QModelIndexList selected = m_thumbList->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        paths.append(m_model->filePath(idx));
    }
    return paths;
}
