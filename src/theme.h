#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QSettings>
#include <QColor>

namespace Theme {

struct ThemeColors {
    QString bg;
    QString sidebar;
    QString border;
    QString accent;
    QString green;
    QString text;
    QString secText;
    QString hover;
};

inline ThemeColors getThemeColors() {
    QSettings settings("Amifiles", "Amifiles");
    QString preset = settings.value("theme/preset", "Catppuccin Mocha").toString();

    ThemeColors c;
    c.bg = "#1e1e2e";
    c.sidebar = "#181825";
    c.border = "#313244";
    c.accent = "#89b4fa";
    c.green = "#a6e3a1";
    c.text = "#cdd6f4";
    c.secText = "#a6adc8";
    c.hover = "#45475a";

    if (preset == "Catppuccin Latte") {
        c.bg = "#eff1f5";
        c.sidebar = "#e6e9ef";
        c.border = "#ccd0da";
        c.accent = "#1e66f5";
        c.green = "#40a02b";
        c.text = "#4c4f69";
        c.secText = "#6c6f85";
        c.hover = "#acb0be";
    } else if (preset == "Catppuccin Macchiato") {
        c.bg = "#24273a";
        c.sidebar = "#1e2030";
        c.border = "#363a4f";
        c.accent = "#8aadf4";
        c.green = "#a6da95";
        c.text = "#cad3f5";
        c.secText = "#a5adcb";
        c.hover = "#494d64";
    } else if (preset == "Catppuccin Frappé") {
        c.bg = "#303446";
        c.sidebar = "#292c3c";
        c.border = "#414559";
        c.accent = "#8caaee";
        c.green = "#a6d189";
        c.text = "#c6d0f5";
        c.secText = "#a5adce";
        c.hover = "#51576d";
    } else if (preset == "Midnight High Contrast") {
        c.bg = "#0c0f12";
        c.sidebar = "#13181f";
        c.border = "#283241";
        c.accent = "#3b82f6";
        c.green = "#10b981";
        c.text = "#ffffff";
        c.secText = "#94a3b8";
        c.hover = "#1e293b";
    } else if (preset == "Cyber Obsidian") {
        c.bg = "#0d0d0d";
        c.sidebar = "#151515";
        c.border = "#2a2a2a";
        c.accent = "#ff007f";
        c.green = "#00ffcc";
        c.text = "#ffffff";
        c.secText = "#a0a0a0";
        c.hover = "#222222";
    } else if (preset == "Nordic Frost") {
        c.bg = "#0f172a";
        c.sidebar = "#1e293b";
        c.border = "#384252";
        c.accent = "#38bdf8";
        c.green = "#34d399";
        c.text = "#ffffff";
        c.secText = "#cbd5e1";
        c.hover = "#334155";
    } else if (preset == "Amiga Workbench Classic") {
        c.bg = "#0055aa";
        c.sidebar = "#aaaaaa";
        c.border = "#555555";
        c.accent = "#ff5555";
        c.green = "#55ff55";
        c.text = "#ffffff";
        c.secText = "#000000";
        c.hover = "#555555";
    } else if (preset == "Custom") {
        c.bg = settings.value("theme/custom_bg", "#1e1e2e").toString();
        c.sidebar = settings.value("theme/custom_sidebar", "#181825").toString();
        c.border = settings.value("theme/custom_border", "#313244").toString();
        c.accent = settings.value("theme/custom_accent", "#89b4fa").toString();
        c.green = settings.value("theme/custom_green", "#a6e3a1").toString();
        c.text = settings.value("theme/custom_text", "#cdd6f4").toString();
        c.secText = settings.value("theme/custom_sec_text", "#a6adc8").toString();
        c.hover = settings.value("theme/custom_hover", "#45475a").toString();
    }
    return c;
}

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

    ThemeColors colors = getThemeColors();
    QString bg = colors.bg;
    QString sidebar = colors.sidebar;
    QString border = colors.border;
    QString accent = colors.accent;
    QString green = colors.green;
    QString text = colors.text;
    QString secText = colors.secText;
    QString hover = colors.hover;

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
