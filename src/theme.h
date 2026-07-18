#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QSettings>
#include <QColor>

namespace Theme {

inline QString getStylesheet() {
    QSettings settings("Amifiles", "Amifiles");
    
    // Theme presets
    QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();
    if (preset == "System Theme") {
        return "";
    }
    int fontSize = settings.value("theme/font_size", 13).toInt();
    int borderRadius = settings.value("theme/border_radius", 4).toInt();
    double opacity = settings.value("theme/sidebar_opacity", 1.0).toDouble();

    // Default Catppuccin Mocha colors
    QString bg = "#1e1e2e";
    QString sidebar = "#181825";
    QString border = "#313244";
    QString accent = "#89b4fa";
    QString green = "#a6e3a1";
    QString text = "#cdd6f4";
    QString secText = "#a6adc8";
    QString hover = "#45475a";

    if (preset == "Catppuccin Latte") {
        bg = "#eff1f5";
        sidebar = "#e6e9ef";
        border = "#ccd0da";
        accent = "#1e66f5";
        green = "#40a02b";
        text = "#4c4f69";
        secText = "#6c6f85";
        hover = "#acb0be";
    } else if (preset == "Catppuccin Macchiato") {
        bg = "#24273a";
        sidebar = "#1e2030";
        border = "#363a4f";
        accent = "#8aadf4";
        green = "#a6da95";
        text = "#cad3f5";
        secText = "#a5adcb";
        hover = "#494d64";
    } else if (preset == "Catppuccin Frappé") {
        bg = "#303446";
        sidebar = "#292c3c";
        border = "#414559";
        accent = "#8caaee";
        green = "#a6d189";
        text = "#c6d0f5";
        secText = "#a5adce";
        hover = "#51576d";
    } else if (preset == "Midnight High Contrast") {
        bg = "#0c0f12";
        sidebar = "#13181f";
        border = "#283241";
        accent = "#3b82f6";
        green = "#10b981";
        text = "#ffffff";
        secText = "#94a3b8";
        hover = "#1e293b";
    } else if (preset == "Cyber Obsidian") {
        bg = "#0d0d0d";
        sidebar = "#151515";
        border = "#2a2a2a";
        accent = "#ff007f";
        green = "#00ffcc";
        text = "#ffffff";
        secText = "#a0a0a0";
        hover = "#222222";
    } else if (preset == "Nordic Frost") {
        bg = "#0f172a";
        sidebar = "#1e293b";
        border = "#384252";
        accent = "#38bdf8";
        green = "#34d399";
        text = "#ffffff";
        secText = "#cbd5e1";
        hover = "#334155";
    } else if (preset == "Amiga Workbench Classic") {
        bg = "#0055aa";
        sidebar = "#aaaaaa";
        border = "#555555";
        accent = "#ff5555";
        green = "#55ff55";
        text = "#ffffff";
        secText = "#000000";
        hover = "#555555";
    } else if (preset == "Custom") {
        bg = settings.value("theme/custom_bg", "#1e1e2e").toString();
        sidebar = settings.value("theme/custom_sidebar", "#181825").toString();
        border = settings.value("theme/custom_border", "#313244").toString();
        accent = settings.value("theme/custom_accent", "#89b4fa").toString();
        green = settings.value("theme/custom_green", "#a6e3a1").toString();
        text = settings.value("theme/custom_text", "#cdd6f4").toString();
        secText = settings.value("theme/custom_sec_text", "#a6adc8").toString();
        hover = settings.value("theme/custom_hover", "#45475a").toString();
    }

    // Apply sidebar opacity if glassmorphism is used (e.g. converting sidebar color to rgba)
    QString sidebarBg = sidebar;
    if (opacity < 1.0) {
        QColor col(sidebar);
        if (col.isValid()) {
            sidebarBg = QString("rgba(%1, %2, %3, %4)").arg(col.red()).arg(col.green()).arg(col.blue()).arg(opacity);
        }
    }

    QString stylesheet = QString(R"(
        /* Global Styles */
        QWidget {
            background-color: @bg@;
            color: @text@;
            font-family: "Segoe UI", "Inter", "Helvetica Neue", sans-serif;
            font-size: @fontSize@px;
        }

        /* Menu Bar */
        QMenuBar {
            background-color: @sidebarBg@;
            border-bottom: 1px solid @border@;
            padding: 4px;
            color: @text@;
        }
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 10px;
            border-radius: @borderRadius@px;
            color: @text@;
        }
        QMenuBar::item:selected {
            background-color: @border@;
            color: @accent@;
        }

        QMenu {
            background-color: @sidebarBg@;
            border: 1px solid @border@;
            border-radius: @borderRadius@px;
            padding: 5px;
            color: @text@;
        }
        QMenu::item {
            background-color: transparent;
            padding: 4px 20px;
            border-radius: @borderRadius@px;
            color: @text@;
        }
        QMenu::item:selected {
            background-color: @hover@;
            color: @accent@;
        }

        /* Sidebars & Views */
        QTreeView, QListView {
            background-color: @bg@;
            border: 1px solid @border@;
            border-radius: @borderRadius@px;
            gridline-color: @border@;
        }
        QTreeView::item, QListView::item {
            padding: 6px;
            border-radius: @borderRadius@px;
        }
        QTreeView::item:hover, QListView::item:hover {
            background-color: @hover@;
        }
        QTreeView::item:selected, QListView::item:selected {
            background-color: @border@;
            color: @accent@;
        }

        /* Tab widgets */
        QTabWidget::pane {
            border: 1px solid @border@;
            background-color: @bg@;
        }
        QTabBar::tab {
            background-color: @sidebarBg@;
            color: @secText@;
            padding: 6px 12px;
            border-top-left-radius: @borderRadius@px;
            border-top-right-radius: @borderRadius@px;
            margin-right: 2px;
            border: 1px solid @border@;
            border-bottom: none;
        }
        QTabBar::tab:selected {
            background-color: @bg@;
            color: @accent@;
            border-bottom: 2px solid @accent@;
        }

        /* Buttons */
        QPushButton {
            background-color: @hover@;
            border: 1px solid @border@;
            border-radius: @borderRadius@px;
            padding: 6px 12px;
            color: @text@;
            min-width: 60px;
        }
        QPushButton:hover {
            background-color: @border@;
            border: 1px solid @accent@;
        }

        /* Status Bar */
        QStatusBar {
            background-color: @bg@;
            border-top: 1px solid @border@;
            color: @secText@;
        }
    )");

    stylesheet.replace("@bg@", bg);
    stylesheet.replace("@sidebarBg@", sidebarBg);
    stylesheet.replace("@border@", border);
    stylesheet.replace("@accent@", accent);
    stylesheet.replace("@green@", green);
    stylesheet.replace("@text@", text);
    stylesheet.replace("@secText@", secText);
    stylesheet.replace("@hover@", hover);
    stylesheet.replace("@fontSize@", QString::number(fontSize));
    stylesheet.replace("@borderRadius@", QString::number(borderRadius));

    return stylesheet;
}

// Fallback constant for backwards compatibility with parts of code referencing Theme::Stylesheet
const QString Stylesheet = "";

} // namespace Theme

#endif // THEME_H
