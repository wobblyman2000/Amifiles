#include "millercolumnsview.h"
#include <QScrollBar>
#include <QTimer>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QDateTime>

MillerColumnsView::MillerColumnsView(QFileSystemModel* model, QWidget* parent)
    : QScrollArea(parent), m_model(model) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet("QScrollArea { background-color: #1e1e2e; }");

    m_container = new QWidget(this);
    m_container->setObjectName("MillerContainer");
    m_container->setStyleSheet("#MillerContainer { background-color: #1e1e2e; }");
    m_layout = new QHBoxLayout(m_container);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);

    setWidget(m_container);
}

void MillerColumnsView::setRootPath(const QString& path) {
    m_rootPath = path;
    clearColumnsRightOf(-1);
    addColumnForPath(path, m_model->index(path));
}

void MillerColumnsView::addColumnForPath(const QString& path, const QModelIndex& index) {
    Q_UNUSED(index);
    int colIdx = m_lists.size();

    QListView* list = new QListView(m_container);
    list->setModel(m_model);
    list->setRootIndex(m_model->index(path));
    list->setFixedWidth(200);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->setStyleSheet(
        "QListView { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 11px; }"
        "QListView::item { padding: 4px; border-radius: 4px; }"
        "QListView::item:hover { background-color: #313244; }"
        "QListView::item:selected { background-color: #89b4fa; color: #11111b; }"
    );

    connect(list, &QListView::clicked, this, [this, colIdx](const QModelIndex& idx) {
        onColumnClicked(idx, colIdx);
    });
    connect(list, &QListView::doubleClicked, this, &MillerColumnsView::onColumnDoubleClicked);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        emit customContextMenuRequested(pos);
    });

    list->setDragEnabled(true);
    list->setAcceptDrops(true);
    list->setDragDropMode(QAbstractItemView::DragDrop);

    QObject* p = parent();
    while (p && !p->inherits("FilePanel")) {
        p = p->parent();
    }
    if (p) {
        list->installEventFilter(p);
        if (list->viewport()) list->viewport()->installEventFilter(p);
    }

    m_layout->addWidget(list);
    m_lists.append(list);
    m_colPaths.append(path);

    m_layout->update();
    scrollToEnd();
}

void MillerColumnsView::clearColumnsRightOf(int colIdx) {
    while (m_lists.size() > colIdx + 1) {
        QWidget* w = m_layout->itemAt(colIdx + 1)->widget();
        m_layout->removeWidget(w);
        m_lists.removeAt(colIdx + 1);
        m_colPaths.removeAt(colIdx + 1);
        w->deleteLater();
    }
}

void MillerColumnsView::onColumnClicked(const QModelIndex& index, int colIdx) {
    QString path = m_model->filePath(index);
    emit fileSelected(path);

    QFileInfo info(path);
    if (info.isDir()) {
        clearColumnsRightOf(colIdx);
        addColumnForPath(path, index);
    } else {
        clearColumnsRightOf(colIdx);
        
        // Add a premium File Info preview pane as the final cascading block!
        QFrame* previewBox = new QFrame(m_container);
        previewBox->setFixedWidth(220);
        previewBox->setStyleSheet("QFrame { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; }");
        
        QVBoxLayout* lay = new QVBoxLayout(previewBox);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->setSpacing(8);

        QLabel* iconLbl = new QLabel(previewBox);
        QIcon icon = m_model->data(index, Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            iconLbl->setPixmap(icon.pixmap(64, 64));
            iconLbl->setAlignment(Qt::AlignCenter);
        }
        
        QLabel* nameLbl = new QLabel(info.fileName(), previewBox);
        nameLbl->setWordWrap(true);
        nameLbl->setAlignment(Qt::AlignCenter);
        nameLbl->setStyleSheet("font-weight: bold; color: #89b4fa; font-size: 12px;");

        QLabel* typeLbl = new QLabel(QString("Type: %1").arg(info.suffix().toUpper()), previewBox);
        typeLbl->setStyleSheet("color: #a6adc8;");
        
        double kb = info.size() / 1024.0;
        double mb = kb / 1024.0;
        QString sizeStr = (mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1);
        QLabel* sizeLbl = new QLabel(QString("Size: %1").arg(sizeStr), previewBox);
        sizeLbl->setStyleSheet("color: #a6adc8;");

        QLabel* dateLbl = new QLabel(QString("Modified:\n%1").arg(info.lastModified().toString("yyyy-MM-dd hh:mm")), previewBox);
        dateLbl->setStyleSheet("color: #585b70; font-size: 9px;");

        lay->addWidget(iconLbl);
        lay->addWidget(nameLbl);
        lay->addWidget(typeLbl);
        lay->addWidget(sizeLbl);
        lay->addWidget(dateLbl);
        lay->addStretch();

        m_layout->addWidget(previewBox);
        
        // Wrap preview box inside lists tracker so clearColumnsRightOf deletes it clean
        m_lists.append(reinterpret_cast<QListView*>(previewBox));
        m_colPaths.append(path);

        scrollToEnd();
    }
}

void MillerColumnsView::onColumnDoubleClicked(const QModelIndex& index) {
    QString path = m_model->filePath(index);
    emit fileDoubleClicked(path);
}

void MillerColumnsView::scrollToEnd() {
    QTimer::singleShot(80, this, [this]() {
        horizontalScrollBar()->setValue(horizontalScrollBar()->maximum());
    });
}

QStringList MillerColumnsView::selectedPaths() const {
    QStringList paths;
    for (int i = m_lists.size() - 1; i >= 0; --i) {
        QListView* list = m_lists[i];
        if (qobject_cast<QListView*>(list)) {
            QModelIndexList selected = list->selectionModel()->selectedRows();
            if (!selected.isEmpty()) {
                for (const QModelIndex& idx : selected) {
                    paths.append(m_model->filePath(idx));
                }
                break;
            }
        }
    }
    return paths;
}
