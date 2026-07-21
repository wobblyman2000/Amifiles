#include "pathbarwidget.h"
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>

PathBarWidget::PathBarWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "PathBarWidget { background-color: #181825; border: 1px solid #313244; border-radius: 4px; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: none; padding: 4px 8px; font-size: 13px; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);

    // Page 0: Breadcrumb Buttons Container
    m_breadcrumbPage = new QWidget(this);
    m_breadcrumbPage->setStyleSheet("background: transparent;");
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbPage);
    m_breadcrumbLayout->setContentsMargins(4, 0, 4, 0);
    m_breadcrumbLayout->setSpacing(2);

    m_stack->addWidget(m_breadcrumbPage);

    // Page 1: Manual Path Entry LineEdit
    m_editPath = new QLineEdit(this);
    m_editPath->setPlaceholderText("Enter directory path...");
    connect(m_editPath, &QLineEdit::returnPressed, this, &PathBarWidget::onEditReturnPressed);
    
    // Synchronize breadcrumbs automatically when text is set programmatically
    connect(m_editPath, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!m_isEditing && !text.isEmpty()) {
            m_currentPath = QDir::cleanPath(text);
            rebuildBreadcrumbs();
        }
    });

    m_stack->addWidget(m_editPath);

    mainLayout->addWidget(m_stack);
    showBreadcrumbMode();
}

void PathBarWidget::setPath(const QString& path) {
    QString cleanP = QDir::cleanPath(path);
    m_currentPath = cleanP;
    
    bool wasEditing = m_isEditing;
    m_isEditing = false;
    m_editPath->setText(QDir::toNativeSeparators(m_currentPath));
    m_isEditing = wasEditing;

    rebuildBreadcrumbs();
    if (!m_isEditing) {
        showBreadcrumbMode();
    }
}

void PathBarWidget::rebuildBreadcrumbs() {
    // Clear old breadcrumb layout
    QLayoutItem* item;
    while ((item = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (m_currentPath.isEmpty()) {
        m_breadcrumbLayout->addStretch(1);
        return;
    }

    QString normalized = QDir::fromNativeSeparators(m_currentPath);
    QStringList parts = normalized.split('/', Qt::SkipEmptyParts);

    // Root button
    QPushButton* btnRoot = new QPushButton("📁 /", m_breadcrumbPage);
    btnRoot->setCursor(Qt::PointingHandCursor);
    btnRoot->setStyleSheet("QPushButton { background: transparent; color: #89b4fa; border: none; font-weight: bold; padding: 3px 6px; border-radius: 3px; } QPushButton:hover { background-color: #313244; color: #f5c2e7; }");
    connect(btnRoot, &QPushButton::clicked, this, [this]() {
        onSegmentClicked("/");
    });
    m_breadcrumbLayout->addWidget(btnRoot);

    // Root Dropdown arrow
    QPushButton* btnRootDrop = new QPushButton("▾", m_breadcrumbPage);
    btnRootDrop->setFixedWidth(18);
    btnRootDrop->setCursor(Qt::PointingHandCursor);
    btnRootDrop->setStyleSheet("QPushButton { background: transparent; color: #a6adc8; border: none; padding: 3px 2px; } QPushButton:hover { background-color: #313244; color: #89b4fa; }");
    connect(btnRootDrop, &QPushButton::clicked, this, [this, btnRootDrop]() {
        onDropdownClicked("/", btnRootDrop);
    });
    m_breadcrumbLayout->addWidget(btnRootDrop);

    QString accumulatedPath = "";
    for (int i = 0; i < parts.size(); ++i) {
        accumulatedPath += "/" + parts[i];
        QString segPath = accumulatedPath;
        QString segName = parts[i];

        // Separator slash label
        QLabel* lblSep = new QLabel("/", m_breadcrumbPage);
        lblSep->setStyleSheet("color: #45475a; font-weight: bold; padding: 0 2px;");
        m_breadcrumbLayout->addWidget(lblSep);

        // Segment button
        QPushButton* btnSeg = new QPushButton(segName, m_breadcrumbPage);
        btnSeg->setCursor(Qt::PointingHandCursor);
        btnSeg->setStyleSheet(
            (i == parts.size() - 1)
                ? "QPushButton { background: #313244; color: #a6e3a1; border: none; font-weight: bold; padding: 3px 8px; border-radius: 3px; } QPushButton:hover { background-color: #45475a; }"
                : "QPushButton { background: transparent; color: #cdd6f4; border: none; font-weight: bold; padding: 3px 6px; border-radius: 3px; } QPushButton:hover { background-color: #313244; color: #89b4fa; }"
        );
        connect(btnSeg, &QPushButton::clicked, this, [this, segPath]() {
            onSegmentClicked(segPath);
        });
        m_breadcrumbLayout->addWidget(btnSeg);

        // Dropdown arrow for sibling subfolders
        QPushButton* btnDrop = new QPushButton("▾", m_breadcrumbPage);
        btnDrop->setFixedWidth(18);
        btnDrop->setCursor(Qt::PointingHandCursor);
        btnDrop->setStyleSheet("QPushButton { background: transparent; color: #a6adc8; border: none; padding: 3px 2px; } QPushButton:hover { background-color: #313244; color: #89b4fa; }");
        connect(btnDrop, &QPushButton::clicked, this, [this, segPath, btnDrop]() {
            onDropdownClicked(segPath, btnDrop);
        });
        m_breadcrumbLayout->addWidget(btnDrop);
    }

    m_breadcrumbLayout->addStretch(1);
}

void PathBarWidget::onSegmentClicked(const QString& targetPath) {
    emit pathEntered(targetPath);
}

void PathBarWidget::onDropdownClicked(const QString& parentPath, QWidget* anchorWidget) {
    QMenu* menu = new QMenu(anchorWidget);
    menu->setStyleSheet(
        "QMenu { background-color: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; padding: 6px; }"
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 5px 8px; margin-bottom: 6px; }"
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; }"
        "QListWidget::item { padding: 6px 10px; border-radius: 3px; color: #cdd6f4; }"
        "QListWidget::item:hover { background-color: #313244; color: #89b4fa; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
    );

    QWidget* popupWidget = new QWidget(menu);
    QVBoxLayout* popLayout = new QVBoxLayout(popupWidget);
    popLayout->setContentsMargins(4, 4, 4, 4);
    popLayout->setSpacing(6);

    // Search filter input at top
    QLineEdit* editFilter = new QLineEdit(popupWidget);
    editFilter->setPlaceholderText("🔍 Filter subfolders (find as you type)...");
    popLayout->addWidget(editFilter);

    // Subdirectories list widget
    QListWidget* listDirs = new QListWidget(popupWidget);
    listDirs->setFixedHeight(200);
    listDirs->setFixedWidth(260);

    QDir dir(parentPath);
    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& sub : subDirs) {
        QListWidgetItem* item = new QListWidgetItem("📁  " + sub.fileName(), listDirs);
        item->setData(Qt::UserRole, sub.absoluteFilePath());
    }

    popLayout->addWidget(listDirs);

    // Filter list as user types
    connect(editFilter, &QLineEdit::textChanged, this, [listDirs](const QString& text) {
        for (int i = 0; i < listDirs->count(); ++i) {
            QListWidgetItem* item = listDirs->item(i);
            bool match = item->text().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    // On double click or Enter select item
    auto handleSelect = [this, listDirs, menu]() {
        QListWidgetItem* cur = listDirs->currentItem();
        if (cur) {
            QString targetPath = cur->data(Qt::UserRole).toString();
            menu->close();
            emit pathEntered(targetPath);
        }
    };

    connect(listDirs, &QListWidget::itemActivated, this, handleSelect);
    connect(editFilter, &QLineEdit::returnPressed, this, handleSelect);

    QWidgetAction* widgetAction = new QWidgetAction(menu);
    widgetAction->setDefaultWidget(popupWidget);
    menu->addAction(widgetAction);

    menu->exec(anchorWidget->mapToGlobal(QPoint(0, anchorWidget->height())));
    menu->deleteLater();
}

void PathBarWidget::mousePressEvent(QMouseEvent* event) {
    if (m_stack->currentIndex() == 0 && event->button() == Qt::LeftButton) {
        showEditMode();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void PathBarWidget::showEditMode() {
    m_isEditing = true;
    m_stack->setCurrentIndex(1);
    m_editPath->setFocus();
    m_editPath->selectAll();
}

void PathBarWidget::showBreadcrumbMode() {
    m_isEditing = false;
    m_stack->setCurrentIndex(0);
}

void PathBarWidget::onEditReturnPressed() {
    QString typed = m_editPath->text().trimmed();
    showBreadcrumbMode();
    if (!typed.isEmpty()) {
        emit pathEntered(typed);
    }
}
