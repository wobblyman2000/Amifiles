#include "diskspaceanalyzerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QApplication>
#include <QHeaderView>

// TreeMapWidget Implementation

TreeMapWidget::TreeMapWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
}

TreeMapWidget::~TreeMapWidget() {
    clear();
}

void TreeMapWidget::clear() {
    delete m_rootNode;
    m_rootNode = nullptr;
    m_items.clear();
    m_hoveredIndex = -1;
}

void TreeMapWidget::setRootNode(Node* root) {
    clear();
    m_rootNode = root;
    if (m_rootNode) {
        computeLayout(m_rootNode, rect());
    }
    update();
}

void TreeMapWidget::computeLayout(Node* parentNode, const QRectF& rect) {
    m_items.clear();
    if (!parentNode || parentNode->size <= 0) return;
    
    // Sort children in descending order of size
    std::sort(parentNode->children.begin(), parentNode->children.end(), [](Node* a, Node* b) {
        return a->size > b->size;
    });

    sliceAndDice(parentNode->children, rect);
}

void TreeMapWidget::sliceAndDice(const QList<Node*>& nodes, const QRectF& rect) {
    if (nodes.isEmpty() || rect.width() < 1 || rect.height() < 1) return;

    qint64 totalSize = 0;
    for (Node* n : nodes) {
        totalSize += n->size;
    }
    if (totalSize <= 0) return;

    bool horizontal = rect.width() > rect.height();
    double currentPos = horizontal ? rect.x() : rect.y();

    for (Node* n : nodes) {
        double ratio = static_cast<double>(n->size) / totalSize;
        double extent = ratio * (horizontal ? rect.width() : rect.height());

        QRectF childRect;
        if (horizontal) {
            childRect = QRectF(currentPos, rect.y(), extent, rect.height());
            currentPos += extent;
        } else {
            childRect = QRectF(rect.x(), currentPos, rect.width(), extent);
            currentPos += extent;
        }

        // If the area is tiny, or it is a file node, paint it. Otherwise recurse to draw folder details.
        if (n->children.isEmpty() || childRect.width() < 30 || childRect.height() < 30) {
            RectItem item;
            item.rect = childRect.adjusted(1, 1, -1, -1);
            item.label = n->name;
            item.path = n->path;
            item.size = n->size;
            item.isDir = n->isDir;
            item.color = colorForNode(n->name, n->isDir);
            m_items.append(item);
        } else {
            sliceAndDice(n->children, childRect);
        }
    }
}

QColor TreeMapWidget::colorForNode(const QString& name, bool isDir) {
    if (isDir) {
        return QColor("#313244"); // Folder (deep slate grey)
    }
    QString ext = QFileInfo(name).suffix().toLower();
    static const QStringList audioExts = {"mp3", "flac", "wav", "aac", "m4a", "ogg", "wma", "opus"};
    static const QStringList videoExts = {"mp4", "mkv", "avi", "mov", "webm", "mpg", "mpeg", "m4v", "flv", "wmv"};
    static const QStringList imgExts = {"jpg", "jpeg", "png", "webp", "bmp", "gif", "tiff", "svg"};
    static const QStringList docExts = {"pdf", "txt", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "ods", "rtf", "epub"};
    static const QStringList archiveExts = {"zip", "rar", "7z", "tar", "gz", "bz2", "xz", "iso", "lzh", "lha"};

    if (audioExts.contains(ext)) return QColor("#a6e3a1");      // Green
    if (videoExts.contains(ext)) return QColor("#cba6f7");      // Purple
    if (imgExts.contains(ext)) return QColor("#f9e2af");        // Yellow
    if (archiveExts.contains(ext)) return QColor("#fab387");    // Peach
    if (docExts.contains(ext)) return QColor("#f38ba8");        // Red
    return QColor("#89b4fa");                                   // Blue (Default/Others)
}

QString TreeMapWidget::formatSize(qint64 bytes) const {
    double size = bytes;
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIdx = 0;
    while (size >= 1024.0 && unitIdx < units.size() - 1) {
        size /= 1024.0;
        unitIdx++;
    }
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIdx]);
}

void TreeMapWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_items.isEmpty()) {
        painter.fillRect(rect(), QColor("#11111b"));
        painter.setPen(QColor("#cdd6f4"));
        painter.drawText(rect(), Qt::AlignCenter, "TreeMap is empty.\nDouble-click or scan a directory to begin.");
        return;
    }

    QFont font = painter.font();
    font.setPixelSize(10);
    painter.setFont(font);

    for (int i = 0; i < m_items.size(); ++i) {
        const RectItem& item = m_items[i];
        
        // Draw Fill
        painter.fillRect(item.rect, item.color);

        // Highlight hovered
        if (i == m_hoveredIndex) {
            painter.fillRect(item.rect, QColor(255, 255, 255, 40));
            painter.setPen(QPen(QColor("#cba6f7"), 2));
        } else {
            painter.setPen(QPen(QColor("#181825"), 1));
        }

        // Draw Border
        painter.drawRect(item.rect);

        // Draw Label Text if fits
        if (item.rect.width() > 30 && item.rect.height() > 20) {
            painter.setPen(item.isDir ? QColor("#cdd6f4") : QColor("#11111b"));
            QString cleanLabel = item.label;
            
            // Truncate text if needed
            QFontMetrics fm(font);
            QString elidedLabel = fm.elidedText(cleanLabel, Qt::ElideRight, static_cast<int>(item.rect.width() - 6));
            
            QRectF textRect = item.rect.adjusted(3, 3, -3, -3);
            painter.drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, elidedLabel);

            // Size Text
            if (item.rect.height() > 35) {
                QString sizeStr = formatSize(item.size);
                QString elidedSize = fm.elidedText(sizeStr, Qt::ElideRight, static_cast<int>(item.rect.width() - 6));
                QRectF sizeRect = item.rect.adjusted(3, item.rect.height() - 15, -3, -1);
                painter.drawText(sizeRect, Qt::AlignBottom | Qt::AlignLeft, elidedSize);
            }
        }
    }
}

void TreeMapWidget::mouseMoveEvent(QMouseEvent* event) {
    int oldHovered = m_hoveredIndex;
    m_hoveredIndex = -1;

    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].rect.contains(event->position())) {
            m_hoveredIndex = i;
            emit itemHovered(m_items[i].path, m_items[i].size);
            break;
        }
    }

    if (m_hoveredIndex != oldHovered) {
        update();
    }
}

void TreeMapWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_hoveredIndex != -1) {
        emit fileDoubleClicked(m_items[m_hoveredIndex].path);
    }
}

void TreeMapWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        emit itemHovered("", 0);
        update();
    }
}

// ScanWorker Implementation

ScanWorker::ScanWorker(const QString& path, QObject* parent)
    : QThread(parent), m_path(path) {}

void ScanWorker::run() {
    TreeMapWidget::Node* root = scanDir(m_path);
    emit scanFinished(root);
}

TreeMapWidget::Node* ScanWorker::scanDir(const QString& path) {
    if (isInterruptionRequested()) return nullptr;

    QDir dir(path);
    TreeMapWidget::Node* node = new TreeMapWidget::Node();
    node->name = dir.dirName();
    node->path = path;
    node->isDir = true;

    emit progressUpdate(dir.dirName());

    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& fi : list) {
        if (isInterruptionRequested()) {
            delete node;
            return nullptr;
        }

        if (fi.isDir()) {
            TreeMapWidget::Node* child = scanDir(fi.absoluteFilePath());
            if (child) {
                node->size += child->size;
                node->children.append(child);
            }
        } else {
            TreeMapWidget::Node* child = new TreeMapWidget::Node();
            child->name = fi.fileName();
            child->path = fi.absoluteFilePath();
            child->isDir = false;
            child->size = fi.size();
            node->size += child->size;
            node->children.append(child);
        }
    }
    return node;
}

// DiskSpaceAnalyzerDialog Implementation

DiskSpaceAnalyzerDialog::DiskSpaceAnalyzerDialog(const QString& path, QWidget* parent)
    : QDialog(parent), m_path(path) {
    setWindowTitle("Disk Space TreeMap Analyzer");
    resize(850, 600);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }");

    setupUI();

    m_worker = new ScanWorker(m_path, this);
    connect(m_worker, &ScanWorker::progressUpdate, this, &DiskSpaceAnalyzerDialog::onProgressUpdate);
    connect(m_worker, &ScanWorker::scanFinished, this, &DiskSpaceAnalyzerDialog::onScanFinished);
    
    m_progress->setVisible(true);
    m_progress->setValue(50); // Indeterminate start
    m_progress->setRange(0, 0); 
    
    m_lblStatus->setText("Scanning directory structure...");
    m_worker->start();
}

DiskSpaceAnalyzerDialog::~DiskSpaceAnalyzerDialog() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->requestInterruption();
        m_worker->wait();
    }
}

void DiskSpaceAnalyzerDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // Header info
    QLabel* lblHeader = new QLabel(QString("Folder TreeMap Analysis: %1").arg(QDir::toNativeSeparators(m_path)), this);
    lblHeader->setStyleSheet("font-size: 14px; font-weight: bold; color: #89b4fa;");
    mainLayout->addWidget(lblHeader);

    // Interactive TreeMap Widget
    m_treeMap = new TreeMapWidget(this);
    connect(m_treeMap, &TreeMapWidget::itemHovered, this, &DiskSpaceAnalyzerDialog::onItemHovered);
    connect(m_treeMap, &TreeMapWidget::fileDoubleClicked, this, &DiskSpaceAnalyzerDialog::onFileDoubleClicked);
    mainLayout->addWidget(m_treeMap, 1);

    // Status Area / Progress
    m_lblStatus = new QLabel("Initializing...", this);
    m_lblStatus->setStyleSheet("color: #a6adc8; font-size: 11px;");
    mainLayout->addWidget(m_lblStatus);

    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet("QProgressBar { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; } QProgressBar::chunk { background-color: #a6e3a1; }");
    mainLayout->addWidget(m_progress);

    // Hover path indicator footer
    m_lblHoverInfo = new QLabel("Hover over blocks to inspect sizes | Double-click block to open and locate in file display", this);
    m_lblHoverInfo->setStyleSheet("background-color: #181825; padding: 6px; border: 1px solid #313244; border-radius: 6px; color: #f9e2af; font-family: monospace; font-size: 11px;");
    mainLayout->addWidget(m_lblHoverInfo);

    // Close button
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(1);
    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #45475a; }");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomLayout);
}

void DiskSpaceAnalyzerDialog::onProgressUpdate(const QString& folderName) {
    m_lblStatus->setText(QString("Indexing: .../%1").arg(folderName));
}

void DiskSpaceAnalyzerDialog::onScanFinished(TreeMapWidget::Node* rootNode) {
    m_progress->setVisible(false);
    
    if (!rootNode || rootNode->size <= 0) {
        m_lblStatus->setText("Scan failed or directory empty.");
        return;
    }

    m_lblStatus->setText(QString("Scan complete. Total Directory Size: %1").arg(formatSize(rootNode->size)));
    m_treeMap->setRootNode(rootNode);
}

void DiskSpaceAnalyzerDialog::onFileDoubleClicked(const QString& path) {
    emit locateFileRequested(path);
    accept();
}

void DiskSpaceAnalyzerDialog::onItemHovered(const QString& path, qint64 size) {
    if (path.isEmpty()) {
        m_lblHoverInfo->setText("Hover over blocks to inspect sizes | Double-click block to open and locate in file display");
    } else {
        m_lblHoverInfo->setText(QString("%1 (%2)").arg(QDir::toNativeSeparators(path)).arg(formatSize(size)));
    }
}

QString DiskSpaceAnalyzerDialog::formatSize(qint64 bytes) const {
    double size = bytes;
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIdx = 0;
    while (size >= 1024.0 && unitIdx < units.size() - 1) {
        size /= 1024.0;
        unitIdx++;
    }
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIdx]);
}
