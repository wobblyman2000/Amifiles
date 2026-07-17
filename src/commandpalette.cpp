#include "commandpalette.h"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>

CommandPalette::CommandPalette(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(500, 320);

    // Main frame widget supporting rounded corners and drop shadows
    QFrame* mainFrame = new QFrame(this);
    mainFrame->setObjectName("MainFrame");
    mainFrame->setStyleSheet(
        "#MainFrame { background-color: #1e1e2e; border: 1px solid #313244; border-radius: 8px; }"
    );

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 4);
    mainFrame->setGraphicsEffect(shadow);

    QVBoxLayout* frameLayout = new QVBoxLayout(this);
    frameLayout->setContentsMargins(10, 10, 10, 10);
    frameLayout->addWidget(mainFrame);

    QVBoxLayout* lay = new QVBoxLayout(mainFrame);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    // Search Input
    m_searchEdit = new QLineEdit(mainFrame);
    m_searchEdit->setPlaceholderText("Type a command... (e.g. 'View', 'Diff', 'Tags')");
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #11111b; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px; font-size: 13px; }"
    );
    m_searchEdit->installEventFilter(this);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPalette::onTextChanged);

    // Results list
    m_listWidget = new QListWidget(mainFrame);
    m_listWidget->setStyleSheet(
        "QListWidget { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QListWidget::item { padding: 6px; border-radius: 4px; }"
        "QListWidget::item:hover { background-color: #313244; }"
        "QListWidget::item:selected { background-color: #89b4fa; color: #11111b; font-weight: bold; }"
    );
    connect(m_listWidget, &QListWidget::itemActivated, this, &CommandPalette::onItemActivated);
    connect(m_listWidget, &QListWidget::itemClicked, this, &CommandPalette::onItemActivated);

    lay->addWidget(m_searchEdit);
    lay->addWidget(m_listWidget);

    populateItems();
    onTextChanged("");
}

void CommandPalette::populateItems() {
    m_allItems = {
        {"View: Switch to Details Table", "view_details"},
        {"View: Switch to Grid / Icons", "view_icons"},
        {"View: Switch to Card / Tiles", "view_cards"},
        {"View: Switch to Miller Columns", "view_miller"},
        {"View: Switch to Chronological Timeline", "view_timeline"},
        {"View: Switch to Filmstrip View", "view_filmstrip"},
        {"View: Toggle Split Dual Pane", "toggle_dual"},
        {"View: Toggle Sidebar Bookmarks", "toggle_sidebar"},
        {"View: Toggle Age Colors Highlight", "toggle_age"},
        {"Tools: Run Batch Image Processor", "tool_image"},
        {"Tools: Run Visual Diff Tool", "tool_diff"},
        {"Tools: Run Duplicate Files Finder", "tool_duplicates"},
        {"Tools: Configure Auto-Tag Rules", "tool_autotags"},
        {"Tools: Configure Smart Auto-Organizer", "tool_autoorganizer"},
        {"Tools: Remote & Cloud Mounts Manager", "tool_remotemountmanager"},
        {"Navigation: Go to Parent Directory", "nav_up"},
        {"Navigation: Back in History", "nav_back"},
        {"Navigation: Forward in History", "nav_forward"},
        {"Tools: Compare Folder Structures", "tool_folder_diff"},
        {"Tools: Password Protect (Encrypt ZIP/7z)", "tool_encrypt"},
        {"Tools: View Disk Usage Metrics Dashboard", "open_disk_dashboard"},
        {"Help: Open Help dialog", "help_open"}
    };
}

void CommandPalette::onTextChanged(const QString& text) {
    m_listWidget->clear();
    for (const auto& item : m_allItems) {
        if (text.isEmpty() || item.text.contains(text, Qt::CaseInsensitive)) {
            QListWidgetItem* listIdx = new QListWidgetItem(item.text, m_listWidget);
            listIdx->setData(Qt::UserRole, item.action);
        }
    }
    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }
}

void CommandPalette::onItemActivated(QListWidgetItem* item) {
    if (item) {
        emit commandTriggered(item->data(Qt::UserRole).toString());
        accept();
    }
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEv = static_cast<QKeyEvent*>(event);
        if (keyEv->key() == Qt::Key_Down) {
            int row = m_listWidget->currentRow();
            if (row < m_listWidget->count() - 1) {
                m_listWidget->setCurrentRow(row + 1);
            }
            return true;
        } else if (keyEv->key() == Qt::Key_Up) {
            int row = m_listWidget->currentRow();
            if (row > 0) {
                m_listWidget->setCurrentRow(row - 1);
            }
            return true;
        } else if (keyEv->key() == Qt::Key_Enter || keyEv->key() == Qt::Key_Return) {
            onItemActivated(m_listWidget->currentItem());
            return true;
        } else if (keyEv->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}
