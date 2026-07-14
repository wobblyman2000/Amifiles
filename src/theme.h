#ifndef THEME_H
#define THEME_H

#include <QString>

namespace Theme {

// Sleek dark theme stylesheet inspired by modern IDEs and Directory Opus dark mode
const QString Stylesheet = R"(
    /* Global Styles */
    QWidget {
        background-color: #1e1e2e;
        color: #cdd6f4;
        font-family: "Segoe UI", "Inter", "Helvetica Neue", sans-serif;
        font-size: 13px;
    }

    /* Menu Bar */
    QMenuBar {
        background-color: #181825;
        border-bottom: 1px solid #313244;
        padding: 4px;
    }
    QMenuBar::item {
        background-color: transparent;
        padding: 4px 10px;
        border-radius: 4px;
    }
    QMenuBar::item:selected {
        background-color: #313244;
        color: #89b4fa;
    }

    QMenu {
        background-color: #181825;
        border: 1px solid #313244;
        border-radius: 6px;
        padding: 5px;
    }
    QMenu::item {
        padding: 6px 25px 6px 20px;
        border-radius: 4px;
    }
    QMenu::item:selected {
        background-color: #89b4fa;
        color: #11111b;
    }
    QMenu::separator {
        height: 1px;
        background-color: #313244;
        margin: 5px 0px;
    }

    /* Toolbars */
    QToolBar {
        background-color: #1e1e2e;
        border-bottom: 1px solid #313244;
        spacing: 6px;
        padding: 4px;
    }
    QToolButton {
        background-color: transparent;
        border: 1px solid transparent;
        border-radius: 4px;
        padding: 5px;
        color: #cdd6f4;
    }
    QToolButton:hover {
        background-color: #313244;
        border: 1px solid #45475a;
    }
    QToolButton:pressed {
        background-color: #45475a;
    }
    QToolButton:checked {
        background-color: #3a86ff;
        color: white;
    }

    /* Splitters */
    QSplitter::handle {
        background-color: #313244;
    }
    QSplitter::handle:horizontal {
        width: 4px;
    }
    QSplitter::handle:vertical {
        height: 4px;
    }
    QSplitter::handle:hover {
        background-color: #89b4fa;
    }

    /* Input Fields & Line Edits */
    QLineEdit {
        background-color: #181825;
        border: 1px solid #313244;
        border-radius: 4px;
        padding: 5px 8px;
        color: #cdd6f4;
        selection-background-color: #89b4fa;
        selection-color: #11111b;
    }
    QLineEdit:focus {
        border: 1px solid #89b4fa;
    }

    /* Tree View / List View (File Displays) */
    QTreeView, QListView {
        background-color: #181825;
        border: 2px solid #313244;
        border-radius: 6px;
        outline: 0;
    }
    QTreeView {
        alternate-background-color: #1e1e2e;
        show-decoration-selected: 1;
    }
    QTreeView::item, QListView::item {
        padding: 4px;
        color: #cdd6f4;
    }
    QTreeView::item {
        border-bottom: 1px solid #1e1e2e;
    }
    QTreeView::item:hover, QListView::item:hover {
        background-color: #2d2d3f;
    }
    QTreeView::item:selected, QListView::item:selected {
        background-color: #3a86ff;
        color: #ffffff;
    }
    QTreeView::item:selected:active, QListView::item:selected:active {
        background-color: #3a86ff;
        color: #ffffff;
    }
    QTreeView::item:selected:!active, QListView::item:selected:!active {
        background-color: #45475a;
        color: #cdd6f4;
    }

    /* Header View (Columns) */
    QHeaderView::section {
        background-color: #11111b;
        color: #a6adc8;
        padding: 6px;
        border: none;
        border-right: 1px solid #313244;
        border-bottom: 2px solid #313244;
        font-weight: bold;
    }
    QHeaderView::section:hover {
        background-color: #313244;
    }

    /* Scrollbars */
    QScrollBar:vertical {
        background-color: #1e1e2e;
        width: 10px;
        margin: 0px 0 0px 0;
    }
    QScrollBar::handle:vertical {
        background-color: #45475a;
        min-height: 20px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical:hover {
        background-color: #89b4fa;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        background: none;
    }

    QScrollBar:horizontal {
        background-color: #1e1e2e;
        height: 10px;
        margin: 0 0px 0 0px;
    }
    QScrollBar::handle:horizontal {
        background-color: #45475a;
        min-width: 20px;
        border-radius: 5px;
    }
    QScrollBar::handle:horizontal:hover {
        background-color: #89b4fa;
    }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
        width: 0px;
    }
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
        background: none;
    }

    /* Tab Bars */
    QTabBar::tab {
        background-color: #11111b;
        color: #a6adc8;
        padding: 6px 12px;
        border-top-left-radius: 4px;
        border-top-right-radius: 4px;
        margin-right: 2px;
        border: 1px solid #313244;
        border-bottom: none;
    }
    QTabBar::tab:selected {
        background-color: #181825;
        color: #89b4fa;
        border-bottom: 2px solid #89b4fa;
    }
    QTabBar::tab:hover:!selected {
        background-color: #313244;
    }

    /* Status Bar */
    QStatusBar {
        background-color: #11111b;
        border-top: 1px solid #313244;
        color: #a6adc8;
    }
    QStatusBar::item {
        border: none;
    }

    /* Buttons */
    QPushButton {
        background-color: #313244;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 6px 12px;
        color: #cdd6f4;
        min-width: 60px;
    }
    QPushButton:hover {
        background-color: #45475a;
        border: 1px solid #89b4fa;
    }
    QPushButton:pressed {
        background-color: #585b70;
    }
    QPushButton:disabled {
        background-color: #11111b;
        color: #585b70;
        border: 1px solid #1e1e2e;
    }

    /* Labels */
    QLabel {
        background-color: transparent;
    }

    /* Sliders */
    QSlider::groove:horizontal {
        height: 6px;
        background: #313244;
        border-radius: 3px;
    }
    QSlider::sub-page:horizontal {
        background: #89b4fa;
        border-radius: 3px;
    }
    QSlider::handle:horizontal {
        background: #cdd6f4;
        width: 14px;
        margin-top: -4px;
        margin-bottom: -4px;
        border-radius: 7px;
    }
    QSlider::handle:horizontal:hover {
        background: #89b4fa;
    }
)";

} // namespace Theme

#endif // THEME_H
