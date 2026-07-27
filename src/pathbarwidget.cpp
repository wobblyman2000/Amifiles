#include "pathbarwidget.h"
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>

PathBarWidget::PathBarWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "PathBarWidget { background-color: #181825; border: 1px solid #313244; border-radius: 4px; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: none; padding: 4px 8px; font-size: 13px; }"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(0);

    m_editPath = new QLineEdit(this);
    m_editPath->setPlaceholderText("Type path here (e.g. /run/media/dave/)...");
    m_editPath->installEventFilter(this);
    mainLayout->addWidget(m_editPath);

    m_popupList = new QListWidget(this);
    m_popupList->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    m_popupList->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_popupList->setFocusPolicy(Qt::NoFocus);
    m_popupList->setStyleSheet(
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:hover { background-color: #313244; color: #89b4fa; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
    );
    m_popupList->installEventFilter(this);
    connect(m_popupList, &QListWidget::itemClicked, this, &PathBarWidget::onPopupItemClicked);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &PathBarWidget::updatePopupList);

    connect(m_editPath, &QLineEdit::textChanged, this, &PathBarWidget::onTextChanged);
    connect(m_editPath, &QLineEdit::returnPressed, this, [this]() {
        m_debounceTimer->stop();
        m_popupList->hide();
        emit pathEntered(m_editPath->text());
    });
}

void PathBarWidget::setPath(const QString& path) {
    m_currentPath = QDir::cleanPath(path);
    m_blockPopup = true;
    m_editPath->setText(QDir::toNativeSeparators(m_currentPath));
    m_blockPopup = false;
    m_debounceTimer->stop();
    m_popupList->hide();
}

void PathBarWidget::onTextChanged(const QString& text) {
    Q_UNUSED(text);
    if (m_blockPopup || !m_editPath || !m_editPath->hasFocus()) return;
    m_debounceTimer->start(250); // 250ms debounce
}

void PathBarWidget::onPopupItemClicked(QListWidgetItem* item) {
    if (!item) return;
    
    QString text = m_editPath->text();
    QString cleanText = QDir::fromNativeSeparators(text);
    int lastSlash = cleanText.lastIndexOf('/');
    QString parentDir;
    if (lastSlash != -1) {
        parentDir = cleanText.left(lastSlash + 1);
    }
    
    QString completed = parentDir + item->text() + "/";
    m_blockPopup = true;
    m_editPath->setText(QDir::toNativeSeparators(completed));
    m_blockPopup = false;
    
    m_debounceTimer->stop();
    m_popupList->hide();
    m_editPath->setFocus();
    
    // Automatically trigger updating the popup for the new directory path!
    updatePopupList();
}

void PathBarWidget::updatePopupList() {
    QString text = m_editPath->text();
    if (text.isEmpty()) {
        m_popupList->hide();
        return;
    }

    QString cleanText = QDir::fromNativeSeparators(text);
    int lastSlash = cleanText.lastIndexOf('/');
    QString parentDir;
    QString prefix;
    if (lastSlash != -1) {
        parentDir = cleanText.left(lastSlash + 1);
        prefix = cleanText.mid(lastSlash + 1);
    } else {
        parentDir = "./";
        prefix = cleanText;
    }

    QDir dir(parentDir);
    if (dir.exists()) {
        // Find subdirectories matching the prefix
        QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QStringList matches;
        for (const QString& entry : entries) {
            if (entry.startsWith(prefix, Qt::CaseInsensitive)) {
                matches.append(entry);
            }
        }

        if (!matches.isEmpty()) {
            m_popupList->clear();
            for (const QString& match : matches) {
                m_popupList->addItem(match);
            }
            m_popupList->setCurrentItem(nullptr);
            m_popupList->clearSelection();

            // Position and show popup
            QPoint pos = m_editPath->mapToGlobal(QPoint(0, m_editPath->height()));
            m_popupList->setGeometry(pos.x(), pos.y(), m_editPath->width(), qMin(200, m_popupList->count() * 24 + 4));
            m_popupList->show();
        } else {
            m_popupList->hide();
        }
    } else {
        m_popupList->hide();
    }
}

bool PathBarWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_editPath) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (m_popupList->isVisible()) {
                if (keyEvent->key() == Qt::Key_Down) {
                    int row = m_popupList->currentRow();
                    if (row < 0) {
                        m_popupList->setCurrentRow(0);
                    } else if (row < m_popupList->count() - 1) {
                        m_popupList->setCurrentRow(row + 1);
                    }
                    return true;
                } else if (keyEvent->key() == Qt::Key_Up) {
                    int row = m_popupList->currentRow();
                    if (row > 0) {
                        m_popupList->setCurrentRow(row - 1);
                    } else {
                        m_popupList->setCurrentRow(-1);
                        m_popupList->clearSelection();
                    }
                    return true;
                } else if (keyEvent->key() == Qt::Key_Escape) {
                    m_popupList->hide();
                    return true;
                } else if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Tab) {
                    QListWidgetItem* item = nullptr;
                    if (m_popupList->selectedItems().count() > 0) {
                        item = m_popupList->currentItem();
                    }
                    if (item) {
                        onPopupItemClicked(item);
                    } else {
                        m_popupList->hide();
                        emit pathEntered(m_editPath->text());
                    }
                    return true;
                }
            }
        } else if (event->type() == QEvent::FocusOut) {
            QTimer::singleShot(100, this, [this]() {
                if (m_popupList && !m_popupList->hasFocus() && m_editPath && !m_editPath->hasFocus()) {
                    m_popupList->hide();
                }
            });
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PathBarWidget::focusOutEvent(QFocusEvent* event) {
    Q_UNUSED(event);
    m_popupList->hide();
}

void PathBarWidget::hideEvent(QHideEvent* event) {
    Q_UNUSED(event);
    m_popupList->hide();
}
