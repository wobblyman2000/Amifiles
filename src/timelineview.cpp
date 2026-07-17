#include "timelineview.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QFileIconProvider>
#include <QHeaderView>

TimelineView::TimelineView(QWidget* parent) : QTreeWidget(parent) {
    setColumnCount(3);
    setHeaderLabels({"File Name", "Date Modified", "Size"});
    setIndentation(15);
    setAnimated(true);

    header()->setSectionResizeMode(0, QHeaderView::Interactive);
    header()->setSectionResizeMode(1, QHeaderView::Interactive);
    header()->setSectionResizeMode(2, QHeaderView::Interactive);
    setColumnWidth(0, 250);
    setColumnWidth(1, 150);
    setColumnWidth(2, 100);

    setStyleSheet(
        "QTreeWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 11px; }"
        "QHeaderView::section { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; padding: 4px; }"
        "QTreeWidget::item { padding: 4px; }"
        "QTreeWidget::item:hover { background-color: #313244; }"
        "QTreeWidget::item:selected { background-color: #89b4fa; color: #11111b; }"
    );

    connect(this, &QTreeWidget::itemSelectionChanged, this, &TimelineView::onItemSelectionChanged);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &TimelineView::onItemDoubleClicked);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void TimelineView::setRootPath(const QString& path) {
    m_rootPath = path;
    clear();

    QDir dir(path);
    if (!dir.exists()) return;

    QList<QFileInfo> files = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

    QList<QFileInfo> todayList;
    QList<QFileInfo> yesterdayList;
    QList<QFileInfo> weekList;
    QList<QFileInfo> olderList;

    QDateTime now = QDateTime::currentDateTime();

    for (const QFileInfo& info : files) {
        int days = info.lastModified().daysTo(now);
        if (days <= 0) {
            todayList.append(info);
        } else if (days == 1) {
            yesterdayList.append(info);
        } else if (days > 1 && days <= 7) {
            weekList.append(info);
        } else {
            olderList.append(info);
        }
    }

    QFileIconProvider iconProvider;

    auto addGroup = [&](const QString& title, const QList<QFileInfo>& groupFiles, QColor headerColor) {
        if (groupFiles.isEmpty()) return;

        QTreeWidgetItem* groupHeader = new QTreeWidgetItem(this);
        groupHeader->setText(0, QString("%1 (%2 items)").arg(title).arg(groupFiles.size()));
        groupHeader->setFirstColumnSpanned(true);
        groupHeader->setFont(0, QFont("Inter", 10, QFont::Bold));
        groupHeader->setBackground(0, QBrush(headerColor));
        groupHeader->setForeground(0, QBrush(QColor("#ffffff")));
        addTopLevelItem(groupHeader);

        for (const QFileInfo& info : groupFiles) {
            QTreeWidgetItem* child = new QTreeWidgetItem(groupHeader);
            child->setText(0, info.fileName());
            child->setText(1, info.lastModified().toString("yyyy-MM-dd hh:mm"));
            
            double kb = info.size() / 1024.0;
            double mb = kb / 1024.0;
            QString sizeStr = info.isDir() ? "Folder" : ((mb >= 1.0) ? QString("%1 MB").arg(mb, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 1));
            child->setText(2, sizeStr);

            // Fetch system file/folder icon
            QIcon icon = iconProvider.icon(info);
            child->setIcon(0, icon);

            // Store full path
            child->setData(0, Qt::UserRole, info.absoluteFilePath());
        }
        
        groupHeader->setExpanded(true);
    };

    // Add groups with visual color coding headers
    addGroup("Today", todayList, QColor("#e64553"));      // Warm Coral Red
    addGroup("Yesterday", yesterdayList, QColor("#fe640b"));  // Bright Orange
    addGroup("Last 7 Days", weekList, QColor("#df8e1d"));     // Yellow-Gold
    addGroup("Older Files", olderList, QColor("#4c4f69"));    // Slate Gray
}

void TimelineView::onItemSelectionChanged() {
    QList<QTreeWidgetItem*> selected = selectedItems();
    if (selected.isEmpty()) return;

    QTreeWidgetItem* item = selected.first();
    // Verify it is not a category header
    if (item->parent()) {
        QString path = item->data(0, Qt::UserRole).toString();
        emit fileSelected(path);
    }
}

void TimelineView::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (item && item->parent()) {
        QString path = item->data(0, Qt::UserRole).toString();
        emit fileDoubleClicked(path);
    }
}

QStringList TimelineView::selectedPaths() const {
    QStringList paths;
    QList<QTreeWidgetItem*> selected = selectedItems();
    for (QTreeWidgetItem* item : selected) {
        if (item->parent()) {
            paths.append(item->data(0, Qt::UserRole).toString());
        }
    }
    return paths;
}
