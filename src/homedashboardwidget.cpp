#include "homedashboardwidget.h"
#include "mainwindow.h"
#include <QStorageInfo>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QFrame>
#include <QToolButton>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QDrag>
#include <QMimeData>
#include "comicbookviewerdialog.h"

#include "archivedialog.h"
#include "videoscraperdialog.h"
#include "bulkrename.h"
#include "favoritesmanager.h"
#include "theme.h"

// Custom circular progress bar widget
class CircularProgressBar : public QWidget {
    Q_OBJECT
public:
    explicit CircularProgressBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(54, 54);
    }
    void setValue(int value) {
        m_value = qBound(0, value, 100);
        update();
    }
    void setRingColor(const QColor& color) {
        m_color = color;
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        int size = qMin(width(), height()) - 8;
        QRectF rect((width() - size)/2.0, (height() - size)/2.0, size, size);
        
        // Draw background track ring
        QPen trackPen(QColor("#242437"), 4);
        trackPen.setCapStyle(Qt::RoundCap);
        p.setPen(trackPen);
        p.drawEllipse(rect);
        
        // Draw active progress arc (starts at top, i.e., 90 degrees)
        QPen progressPen(m_color, 4);
        progressPen.setCapStyle(Qt::RoundCap);
        p.setPen(progressPen);
        int spanAngle = - (m_value * 360 * 16) / 100;
        p.drawArc(rect, 90 * 16, spanAngle);
        
        // Draw percentage text inside the circle
        p.setPen(QColor("#cdd6f4"));
        QFont font = p.font();
        font.setBold(true);
        font.setPointSize(9);
        p.setFont(font);
        p.drawText(rect, Qt::AlignCenter, QString("%1%").arg(m_value));
    }
private:
    int m_value = 0;
    QColor m_color = QColor("#89b4fa");
};

// Custom graphical folder illustration & cover art widget
class FolderGraphicWidget : public QWidget {
    Q_OBJECT
public:
    FolderGraphicWidget(const QString& path, int layoutIndex, const QColor& accentColor, QWidget* parent = nullptr)
        : QWidget(parent), m_path(path), m_layoutIndex(layoutIndex), m_accent(accentColor) {
        setFixedSize(42, 42);
        m_artwork = getFolderArtwork(path, 84, 84);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        QRectF rect(0, 0, width(), height());
        
        if (!m_artwork.isNull()) {
            QPainterPath path;
            path.addRoundedRect(rect, 6, 6);
            p.setClipPath(path);
            p.drawPixmap(rect.toRect(), m_artwork);
            return;
        }
        
        // Fallback custom vector drawing
        QPainterPath path;
        path.addRoundedRect(rect, 6, 6);
        
        QColor bg = m_accent;
        bg.setAlpha(25);
        p.fillPath(path, bg);
        
        QPen pen(m_accent, 1.5);
        p.setPen(pen);
        p.drawPath(path);
        
        p.setPen(m_accent);
        if (m_layoutIndex == 1) { // Grid (4x4 Grid layout design)
            int margin = 8;
            int size = (width() - margin * 2 - 4) / 2;
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 2; ++c) {
                    QRectF rRect(margin + c * (size + 4), margin + r * (size + 4), size, size);
                    p.drawRoundedRect(rRect, 1.5, 1.5);
                }
            }
        } else if (m_layoutIndex == 0) { // List (List lines layout design)
            int margin = 8;
            int w = width() - margin * 2;
            int h = 3;
            int spacing = 5;
            for (int i = 0; i < 3; ++i) {
                p.drawRoundedRect(QRectF(margin, margin + i * (h + spacing), w, h), 1, 1);
            }
        } else if (m_layoutIndex == 2 || m_layoutIndex == 11) { // Cover Flow or Tiles (Tilted cards mockup)
            p.save();
            p.translate(width()/2, height()/2);
            p.rotate(-12);
            p.drawRoundedRect(QRectF(-10, -13, 14, 19), 2, 2);
            p.restore();
            
            p.save();
            p.translate(width()/2, height()/2);
            p.rotate(10);
            p.drawRoundedRect(QRectF(-3, -11, 14, 19), 2, 2);
            p.restore();
        } else if (m_layoutIndex == 6 || m_layoutIndex == 10) { // Music note
            QFont font = p.font();
            font.setPointSize(13);
            p.setFont(font);
            p.drawText(rect, Qt::AlignCenter, "🎵");
        } else if (m_layoutIndex == 7 || m_layoutIndex == 8 || m_layoutIndex == 9) { // Video/Cinema
            QFont font = p.font();
            font.setPointSize(13);
            p.setFont(font);
            p.drawText(rect, Qt::AlignCenter, "🎬");
        } else if (m_layoutIndex == -2) {
            QFont font = p.font();
            font.setPointSize(13);
            p.setFont(font);
            p.drawText(rect, Qt::AlignCenter, "⚙️");
        } else { // Standard folder
            QFont font = p.font();
            font.setPointSize(13);
            p.setFont(font);
            p.drawText(rect, Qt::AlignCenter, "📁");
        }
    }
private:
    QPixmap getFolderArtwork(const QString& path, int targetWidth, int targetHeight) {
        QStringList candidates = {"folder.jpg", "folder.png", "cover.jpg", "cover.png", "album.jpg"};
        QDir dir(path);
        for (const QString& name : candidates) {
            if (dir.exists(name)) {
                QPixmap pm(dir.filePath(name));
                if (!pm.isNull()) return pm.scaled(targetWidth, targetHeight, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            }
        }
        QStringList entryImages = dir.entryList({"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp"}, QDir::Files);
        if (!entryImages.isEmpty()) {
            QPixmap pm(dir.filePath(entryImages.first()));
            if (!pm.isNull()) return pm.scaled(targetWidth, targetHeight, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        return QPixmap();
    }
    QString m_path;
    int m_layoutIndex;
    QColor m_accent;
    QPixmap m_artwork;
};

// Custom double-clickable card widget
class ClickableCardFrame : public QFrame {
    Q_OBJECT
public:
    enum CardType { PinnedFolder = 0, PinnedProfile = 1, QuickAccess = 2, RecentLocation = 3, RecentFile = 4 };

    explicit ClickableCardFrame(const QString& path, CardType type, int layoutIndex = -1, QWidget* parent = nullptr)
        : QFrame(parent), m_path(path), m_type(type), m_layoutIndex(layoutIndex) {
        setObjectName("cardFrame");
        setProperty("class", "cardFrame");
    }

    QString path() const { return m_path; }
    CardType cardType() const { return m_type; }

signals:
    void doubleClicked(const QString& path);
    void doubleClickedWithLayout(const QString& path, int layoutIndex);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (m_layoutIndex != -1) {
                emit doubleClickedWithLayout(m_path, m_layoutIndex);
            } else {
                emit doubleClicked(m_path);
            }
        }
        QFrame::mouseDoubleClickEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragStartPos = event->pos();
        }
        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton))
            return;
        if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
            return;

        QDrag* drag = new QDrag(this);
        QMimeData* mimeData = new QMimeData;
        mimeData->setText(QString("%1:%2").arg(static_cast<int>(m_type)).arg(m_path));
        drag->setMimeData(mimeData);

        QPixmap pixmap = grab();
        drag->setPixmap(pixmap);
        drag->setHotSpot(event->pos());

        drag->exec(Qt::MoveAction);
    }

private:
    QString m_path;
    CardType m_type;
    int m_layoutIndex;
    QPoint m_dragStartPos;
};

static void clearLayout(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

struct CardTheme {
    QString bgStyle;
    QString borderStyle;
    QString textStyle;
    QString badgeStyle;
    QString badgeBgStyle;
    QString symbol;
};

static CardTheme getCardTheme(int index, int layoutIndex) {
    CardTheme theme;
    int choice = index % 8;
    
    // Choose symbol based on layoutIndex
    QString symbol = "📁";
    if (layoutIndex == 0) symbol = "📊"; // Details View
    else if (layoutIndex == 1) symbol = "⣿"; // Grid View
    else if (layoutIndex == 2) symbol = "🎴"; // Card View
    else if (layoutIndex == 3) symbol = "⚿"; // Miller
    else if (layoutIndex == 6 || layoutIndex == 10) symbol = "🎵"; // Audio
    else if (layoutIndex == 7 || layoutIndex == 8 || layoutIndex == 9) symbol = "🎬"; // Video/Cinema
    
    if (choice == 0) { // Purple / UI Designs
        theme.bgStyle = "background-color: #241c30; border: 1px solid #cba6f7;";
        theme.textStyle = "color: #f5c2e7; font-weight: bold;";
        theme.badgeStyle = "color: #f5c2e7;";
        theme.badgeBgStyle = "background-color: #3b2552; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "⣿";
    } else if (choice == 1) { // Blue / Development
        theme.bgStyle = "background-color: #162035; border: 1px solid #89b4fa;";
        theme.textStyle = "color: #89b4fa; font-weight: bold;";
        theme.badgeStyle = "color: #89b4fa;";
        theme.badgeBgStyle = "background-color: #223456; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "☷";
    } else if (choice == 2) { // Teal / Photos
        theme.bgStyle = "background-color: #16272b; border: 1px solid #94e2d5;";
        theme.textStyle = "color: #94e2d5; font-weight: bold;";
        theme.badgeStyle = "color: #94e2d5;";
        theme.badgeBgStyle = "background-color: #1e3f42; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "🖼️";
    } else if (choice == 3) { // Orange / Server Backups
        theme.bgStyle = "background-color: #302016; border: 1px solid #fab387;";
        theme.textStyle = "color: #fab387; font-weight: bold;";
        theme.badgeStyle = "color: #fab387;";
        theme.badgeBgStyle = "background-color: #4a2e1d; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "🗎";
    } else if (choice == 4) { // Red / Videos
        theme.bgStyle = "background-color: #30161b; border: 1px solid #f38ba8;";
        theme.textStyle = "color: #f38ba8; font-weight: bold;";
        theme.badgeStyle = "color: #f38ba8;";
        theme.badgeBgStyle = "background-color: #4a1d23; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "🎬";
    } else if (choice == 5) { // Yellow / Assets
        theme.bgStyle = "background-color: #2d2d16; border: 1px solid #f9e2af;";
        theme.textStyle = "color: #f9e2af; font-weight: bold;";
        theme.badgeStyle = "color: #f9e2af;";
        theme.badgeBgStyle = "background-color: #45451c; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "📁";
    } else if (choice == 6) { // Pink / Personal
        theme.bgStyle = "background-color: #2e1624; border: 1px solid #f5c2e7;";
        theme.textStyle = "color: #f5c2e7; font-weight: bold;";
        theme.badgeStyle = "color: #f5c2e7;";
        theme.badgeBgStyle = "background-color: #471c35; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "👤";
    } else { // Slate / Archive
        theme.bgStyle = "background-color: #222530; border: 1px solid #bac2de;";
        theme.textStyle = "color: #bac2de; font-weight: bold;";
        theme.badgeStyle = "color: #secText;";
        theme.badgeBgStyle = "background-color: #343946; border-radius: 9px; padding: 2px 8px;";
        theme.symbol = "📦";
    }
    
    if (symbol != "📁") {
        theme.symbol = symbol;
    }
    return theme;
}

struct QATheme {
    QString iconStyle;
    QString cardHoverStyle;
};

static QATheme getQATheme(const QString& name) {
    QATheme t;
    QString color;
    if (name == "Documents") color = "137, 180, 250"; // Blue
    else if (name == "Downloads") color = "166, 227, 161"; // Green
    else if (name == "Music") color = "203, 166, 247"; // Purple
    else if (name == "Desktop") color = "250, 179, 135"; // Orange
    else if (name == "Videos") color = "243, 139, 168"; // Red
    else if (name == "Pictures") color = "148, 226, 213"; // Teal
    else color = "180, 190, 254"; // Lavender (Home)
    
    t.iconStyle = QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(%1, 0.25), stop:1 rgba(%1, 0.05)); border: 1px solid rgba(%1, 0.4); border-radius: 10px; font-size: 22px; min-width: 44px; min-height: 44px;").arg(color);
    
    t.cardHoverStyle = QString("QFrame#cardFrame { background-color: rgba(30, 30, 46, 0.4); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 12px; } "
                               "QFrame#cardFrame:hover { background-color: rgba(%1, 0.08); border: 1px solid rgba(%1, 0.4); }").arg(color);
    return t;
}

HomeDashboardWidget::HomeDashboardWidget(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setupUi();
    refreshDashboard();
}

void HomeDashboardWidget::setupUi() {
    Theme::ThemeColors c = Theme::getThemeColors();
    
    // Style the overall widget with Catppuccin glassmorphism
    setStyleSheet(QString(R"(
        QWidget { background-color: %1; color: %2; font-family: 'Segoe UI', 'Inter', 'sans-serif'; }
        QLabel#sectionTitle { color: %3; font-size: 13px; font-weight: bold; padding-top: 15px; padding-bottom: 5px; text-transform: uppercase; letter-spacing: 1px; }
        QFrame#cardFrame { background-color: rgba(30, 30, 46, 0.4); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 12px; }
        QFrame#cardFrame:hover { border: 1px solid %3; background-color: rgba(45, 45, 68, 0.6); }
        QProgressBar { border: 1px solid %4; border-radius: 4px; background: #11111b; text-align: center; color: %2; font-size: 10px; font-weight: bold; }
        QProgressBar::chunk { background-color: %5; border-radius: 3px; }
        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { border: none; background: %6; width: 8px; margin: 0px; border-radius: 4px; }
        QScrollBar::handle:vertical { background: %4; min-height: 20px; border-radius: 4px; }
        QScrollBar::handle:vertical:hover { background: %7; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QPushButton.unpinButton { background: transparent; border: none; color: #f38ba8; font-size: 13px; font-weight: bold; }
        QPushButton.unpinButton:hover { color: #f2cdcd; background: %8; border-radius: 4px; }
    )").arg(c.bg, c.text, c.accent, c.border, c.green, c.sidebar, c.hover, c.hover));

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left scroll area for dashboard content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    QWidget* scrollContent = new QWidget(scrollArea);
    scrollContent->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(scrollContent, &QWidget::customContextMenuRequested, this, &HomeDashboardWidget::onDashboardContextMenu);
    scrollArea->setWidget(scrollContent);

    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(25, 20, 25, 20);
    contentLayout->setSpacing(8);

    QSettings settings("Amifiles", "Amifiles");
    bool showBanner = settings.value("dashboard/show_welcome_banner", true).toBool();

    // Welcome Banner Header
    m_bannerFrame = new QFrame(scrollContent);
    m_bannerFrame->setStyleSheet("background-color: rgba(17, 17, 27, 0.5); border: 1px solid rgba(255, 255, 255, 0.04); border-radius: 12px; padding: 12px;");
    m_bannerFrame->setVisible(showBanner);
    QVBoxLayout* bannerLayout = new QVBoxLayout(m_bannerFrame);
    bannerLayout->setContentsMargins(15, 8, 15, 8);
    bannerLayout->setSpacing(3);

    QLabel* titleLabel = new QLabel("Welcome to Amifiles", m_bannerFrame);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #cba6f7;");
    bannerLayout->addWidget(titleLabel);

    QLabel* subLabel = new QLabel("Your ultimate dual-pane Amiga & Linux file manager. Double-click storage cards or folders to jump straight in.", m_bannerFrame);
    subLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
    bannerLayout->addWidget(subLabel);

    contentLayout->addWidget(m_bannerFrame);

    // 1. Storage Drives Section
    QLabel* drivesTitle = new QLabel("Storage Drive", scrollContent);
    drivesTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(drivesTitle);

    QFrame* drivesContainer = new QFrame(scrollContent);
    m_drivesLayout = new QGridLayout(drivesContainer);
    m_drivesLayout->setContentsMargins(0, 0, 0, 0);
    m_drivesLayout->setSpacing(12);
    contentLayout->addWidget(drivesContainer);

    // 2. Quick Access Section
    QLabel* quickTitle = new QLabel("Quick Access", scrollContent);
    quickTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(quickTitle);

    QFrame* quickContainer = new QFrame(scrollContent);
    m_quickAccessLayout = new QGridLayout(quickContainer);
    m_quickAccessLayout->setContentsMargins(0, 0, 0, 0);
    m_quickAccessLayout->setSpacing(12);
    contentLayout->addWidget(quickContainer);

    // 3. Pinned Folders Section
    QLabel* pinnedTitle = new QLabel("Pinned Folders", scrollContent);
    pinnedTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(pinnedTitle);

    QFrame* pinnedContainer = new QFrame(scrollContent);
    m_pinnedLayout = new QGridLayout(pinnedContainer);
    m_pinnedLayout->setContentsMargins(0, 0, 0, 0);
    m_pinnedLayout->setSpacing(12);
    contentLayout->addWidget(pinnedContainer);

    // 4. Pinned Layout Profiles Section
    QLabel* profilesTitle = new QLabel("Pinned Layout Profiles", scrollContent);
    profilesTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(profilesTitle);

    QFrame* profilesContainer = new QFrame(scrollContent);
    m_pinnedProfilesLayout = new QGridLayout(profilesContainer);
    m_pinnedProfilesLayout->setContentsMargins(0, 0, 0, 0);
    m_pinnedProfilesLayout->setSpacing(12);
    contentLayout->addWidget(profilesContainer);

    // 5. Recent Locations Section
    QLabel* recentsTitle = new QLabel("Recent Locations", scrollContent);
    recentsTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(recentsTitle);

    QFrame* recentsContainer = new QFrame(scrollContent);
    m_recentLocationsLayout = new QGridLayout(recentsContainer);
    m_recentLocationsLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLocationsLayout->setSpacing(12);
    contentLayout->addWidget(recentsContainer);

    // 6. Recent Files & Media Section
    QLabel* filesTitle = new QLabel("Recent Files & Media", scrollContent);
    filesTitle->setObjectName("sectionTitle");
    contentLayout->addWidget(filesTitle);

    QFrame* filesContainer = new QFrame(scrollContent);
    m_recentFilesLayout = new QGridLayout(filesContainer);
    m_recentFilesLayout->setContentsMargins(0, 0, 0, 0);
    m_recentFilesLayout->setSpacing(12);
    contentLayout->addWidget(filesContainer);

    contentLayout->addStretch(1);
    mainLayout->addWidget(scrollArea, 1);

    // Right Sidebar Toolbar (Width: 230px)
    QFrame* sidebar = new QFrame(this);
    sidebar->setObjectName("sidebarFrame");
    sidebar->setStyleSheet(QString("background-color: %1; border-left: 1px solid %2;").arg(c.sidebar, c.border));
    sidebar->setFixedWidth(230);

    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(15, 20, 15, 20);
    sidebarLayout->setSpacing(12);

    QLabel* toolboxTitle = new QLabel("Toolbar", sidebar);
    toolboxTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #cdd6f4; padding-bottom: 5px; text-transform: uppercase; letter-spacing: 1px;");
    sidebarLayout->addWidget(toolboxTitle);

    QToolButton* btnRename = new QToolButton(sidebar);
    btnRename->setText("⚡  Bulk Rename");
    btnRename->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnRename->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnRename->setStyleSheet("QToolButton { background-color: #cba6f7; color: #11111b; border-radius: 18px; padding: 10px 15px; font-size: 12px; font-weight: bold; } QToolButton:hover { background-color: #f5c2e7; }");
    connect(btnRename, &QToolButton::clicked, this, [this]() { onToolButtonClicked("bulk_rename"); });
    sidebarLayout->addWidget(btnRename);

    QToolButton* btnImageRenamer = new QToolButton(sidebar);
    btnImageRenamer->setText("🖼️  Image Renamer");
    btnImageRenamer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnImageRenamer->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnImageRenamer->setStyleSheet("QToolButton { background-color: #fab387; color: #11111b; border-radius: 18px; padding: 10px 15px; font-size: 12px; font-weight: bold; } QToolButton:hover { background-color: #f9e2af; }");
    connect(btnImageRenamer, &QToolButton::clicked, this, [this]() { onToolButtonClicked("recursive_renamer"); });
    sidebarLayout->addWidget(btnImageRenamer);

    QToolButton* btnScraper = new QToolButton(sidebar);
    btnScraper->setText("🎬  Video Scraper");
    btnScraper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnScraper->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnScraper->setStyleSheet("QToolButton { background-color: #89b4fa; color: #11111b; border-radius: 18px; padding: 10px 15px; font-size: 12px; font-weight: bold; } QToolButton:hover { background-color: #b4befe; }");
    connect(btnScraper, &QToolButton::clicked, this, [this]() { onToolButtonClicked("video_scraper"); });
    sidebarLayout->addWidget(btnScraper);

    QToolButton* btnArchive = new QToolButton(sidebar);
    btnArchive->setText("📦  Archive Manager");
    btnArchive->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnArchive->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnArchive->setStyleSheet("QToolButton { background-color: #f5c2e7; color: #11111b; border-radius: 18px; padding: 10px 15px; font-size: 12px; font-weight: bold; } QToolButton:hover { background-color: #f2cdcd; }");
    connect(btnArchive, &QToolButton::clicked, this, [this]() { onToolButtonClicked("archive_creator"); });
    sidebarLayout->addWidget(btnArchive);

    QToolButton* btnNewFolder = new QToolButton(sidebar);
    btnNewFolder->setText("📁  New Folder");
    btnNewFolder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnNewFolder->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnNewFolder->setStyleSheet("QToolButton { background-color: rgba(30, 30, 46, 0.4); color: #cdd6f4; border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 18px; padding: 10px 15px; font-size: 12px; font-weight: bold; } QToolButton:hover { background-color: rgba(45, 45, 68, 0.6); }");
    connect(btnNewFolder, &QToolButton::clicked, this, [this]() {
        bool ok;
        QString path = QFileDialog::getExistingDirectory(this, "Select Location to Create New Folder", QDir::homePath());
        if (!path.isEmpty()) {
            QString name = QInputDialog::getText(this, "Create New Folder", "Folder Name:", QLineEdit::Normal, "", &ok);
            if (ok && !name.isEmpty()) {
                QDir(path).mkdir(name);
                refreshDashboard();
            }
        }
    });
    sidebarLayout->addWidget(btnNewFolder);

    sidebarLayout->addStretch(1);

    QFrame* separator = new QFrame(sidebar);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QString("background-color: %1; max-height: 1px;").arg(c.border));
    sidebarLayout->addWidget(separator);

    QToolButton* btnSettings = new QToolButton(sidebar);
    btnSettings->setText("⚙️  Settings");
    btnSettings->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnSettings->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnSettings->setStyleSheet("QToolButton { background-color: rgba(30, 30, 46, 0.4); color: #cdd6f4; border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 18px; padding: 10px 15px; font-size: 12px; font-weight: bold; } QToolButton:hover { background-color: rgba(45, 45, 68, 0.6); }");
    connect(btnSettings, &QToolButton::clicked, this, [this]() { onToolButtonClicked("preferences"); });
    sidebarLayout->addWidget(btnSettings);

    mainLayout->addWidget(sidebar);
}

void HomeDashboardWidget::refreshDashboard() {
    populateDrives();
    populateQuickAccess();
    populatePinnedFolders();
    populatePinnedProfiles();
    populateRecentLocations();
    populateRecentFiles();
}

void HomeDashboardWidget::populateDrives() {
    clearLayout(m_drivesLayout);
    m_drivesLayout->setSpacing(10);
    for (int i = 0; i < 10; ++i) m_drivesLayout->setColumnStretch(i, 0);

    int row = 0;
    int col = 0;

    for (const QStorageInfo& storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        if (storage.bytesTotal() == 0) continue;

        QString type = storage.fileSystemType();
        if (type == "tmpfs" || type == "devtmpfs" || type == "sysfs" || type == "proc" || type == "cgroup" || type == "squashfs") continue;

        ClickableCardFrame* card = new ClickableCardFrame(storage.rootPath(), ClickableCardFrame::QuickAccess, -1, this);
        connect(card, &ClickableCardFrame::doubleClicked, this, &HomeDashboardWidget::onDriveDoubleClicked);

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(15, 12, 15, 12);
        layout->setSpacing(12);

        qint64 total = storage.bytesTotal();
        qint64 free = storage.bytesAvailable();
        qint64 used = total - free;
        double pct = (double)used / (double)total * 100.0;

        CircularProgressBar* circular = new CircularProgressBar(card);
        circular->setValue((int)pct);
        
        QColor ringColor;
        if (col == 0) ringColor = QColor("#cba6f7"); // Purple
        else if (col == 1) ringColor = QColor("#89b4fa"); // Blue
        else ringColor = QColor("#89dceb"); // Cyan
        circular->setRingColor(ringColor);
        layout->addWidget(circular);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(2);

        QString name = storage.displayName();
        if (name.isEmpty() || name == "/") {
            name = QString("System SSD");
        }
        QLabel* nameLabel = new QLabel(name, card);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #cdd6f4;");
        details->addWidget(nameLabel);

        QString usageText = QString("%1 GB free of %2 GB")
            .arg(QString::number((double)free / (1024.0 * 1024.0 * 1024.0), 'f', 0))
            .arg(QString::number((double)total / (1024.0 * 1024.0 * 1024.0), 'f', 0));

        QLabel* usageLabel = new QLabel(usageText, card);
        usageLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
        details->addWidget(usageLabel);

        QProgressBar* spaceBar = new QProgressBar(card);
        spaceBar->setRange(0, 100);
        spaceBar->setValue((int)pct);
        spaceBar->setTextVisible(false);
        spaceBar->setFixedHeight(5);
        spaceBar->setStyleSheet(QString(
            "QProgressBar { background-color: #1e1e2e; border: 1px solid #313244; border-radius: 3px; }"
            "QProgressBar::chunk { background-color: %1; border-radius: 2px; }"
        ).arg(ringColor.name()));
        details->addWidget(spaceBar);

        layout->addLayout(details, 1);

        m_drivesLayout->addWidget(card, row, col, Qt::AlignLeft | Qt::AlignVCenter);

        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
    m_drivesLayout->setColumnStretch(3, 1);
}

void HomeDashboardWidget::populateQuickAccess() {
    clearLayout(m_quickAccessLayout);
    m_quickAccessLayout->setSpacing(8);
    for (int i = 0; i < 10; ++i) m_quickAccessLayout->setColumnStretch(i, 0);

    struct QAEntry {
        QString name;
        QString path;
        QString icon;
    };

    QList<QAEntry> entries = {
        {"Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "📄"},
        {"Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "📥"},
        {"Music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "🎵"},
        {"Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "🖥️"},
        {"Videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), "🎬"},
        {"Pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), "🖼️"},
        {"Home Directory", QDir::homePath(), "🏠"}
    };

    int row = 0;
    int col = 0;

    for (const auto& entry : entries) {
        if (entry.path.isEmpty() || !QDir(entry.path).exists()) continue;

        QATheme qat = getQATheme(entry.name);

        ClickableCardFrame* card = new ClickableCardFrame(entry.path, ClickableCardFrame::QuickAccess, -1, this);
        card->setFixedSize(240, 84);
        card->setStyleSheet(qat.cardHoverStyle);
        connect(card, &ClickableCardFrame::doubleClicked, this, &HomeDashboardWidget::onQuickAccessClicked);

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(14);

        QLabel* iconLabel = new QLabel(card);
        iconLabel->setText(entry.icon);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet(qat.iconStyle);
        layout->addWidget(iconLabel);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(2);

        QLabel* nameLabel = new QLabel(entry.name, card);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #cdd6f4;");
        details->addWidget(nameLabel);

        // Get dynamic item count
        int itemCount = QDir(entry.path).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).count();
        QString countText;
        if (itemCount >= 1000) {
            countText = QString("%1k items").arg(QString::number((double)itemCount / 1000.0, 'f', 1));
        } else {
            countText = QString("%1 items").arg(itemCount);
        }
        QLabel* pathLabel = new QLabel(countText, card);
        pathLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
        details->addWidget(pathLabel);

        layout->addLayout(details, 1);

        m_quickAccessLayout->addWidget(card, row, col, Qt::AlignLeft | Qt::AlignVCenter);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }
    m_quickAccessLayout->setColumnStretch(4, 1);
}

void HomeDashboardWidget::populatePinnedFolders() {
    clearLayout(m_pinnedLayout);
    m_pinnedLayout->setSpacing(6);
    for (int i = 0; i < 10; ++i) m_pinnedLayout->setColumnStretch(i, 0);

    QSettings settings("Amifiles", "Amifiles");
    QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();

    if (pinned.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No folders pinned yet. Right-click any directory in standard view and click '📌 Pin to Home Screen' to create shortcuts with view memory.", this);
        emptyLabel->setStyleSheet("color: #a6adc8; font-size: 11px; font-style: italic; padding: 10px;");
        m_pinnedLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int row = 0;
    int col = 0;

    QStringList viewModeNames = {
        "Details Table", "Grid / Icons", "Card / Tiles", "Miller Columns",
        "Chronological Timeline", "Filmstrip View", "Audio Showcase",
        "Video Showcase", "Movies Full Screen", "TV Shows Full Screen", "Music Full Screen"
    };

    for (const QString& item : pinned) {
        QStringList parts = item.split(';');
        if (parts.size() < 2) continue;

        QString path = parts[0];
        QString name = parts[1];
        int layoutIndex = 0;
        if (parts.size() >= 3) {
            layoutIndex = parts[2].toInt();
        }

        CardTheme theme = getCardTheme(col + row * 4, layoutIndex);
        ClickableCardFrame* card = new ClickableCardFrame(path, ClickableCardFrame::PinnedFolder, layoutIndex, this);
        card->setFixedSize(190, 74);
        card->setStyleSheet(QString("QFrame#cardFrame { %1 border-radius: 12px; } QFrame#cardFrame:hover { background-color: rgba(255, 255, 255, 0.05); }").arg(theme.bgStyle));
        connect(card, &ClickableCardFrame::doubleClickedWithLayout, this, &HomeDashboardWidget::onPinnedFolderClicked);

        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QWidget::customContextMenuRequested, this, [this, path, card](const QPoint& pos) {
            QMenu menu(card);
            QAction* actUnpin = menu.addAction("📌 Unpin from Home Screen");
            QAction* selected = menu.exec(card->mapToGlobal(pos));
            if (selected == actUnpin) {
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    "Unpin Folder",
                    QString("Are you sure you want to unpin '%1' from the Home Screen?").arg(QFileInfo(path).fileName()),
                    QMessageBox::Yes | QMessageBox::No
                );
                if (reply == QMessageBox::Yes) {
                    onUnpinFolderClicked(path);
                }
            }
        });

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(8);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(4);

        QLabel* nameLabel = new QLabel(name, card);
        nameLabel->setStyleSheet(theme.textStyle + " font-size: 13px;");
        details->addWidget(nameLabel);

        int itemCount = QDir(path).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).count();
        QString countText = QString("%1 items").arg(itemCount);
        QLabel* countLabel = new QLabel(countText, card);
        countLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
        details->addWidget(countLabel);

        QString layoutName = "Details View";
        if (layoutIndex >= 0 && layoutIndex < viewModeNames.size()) {
            layoutName = viewModeNames[layoutIndex];
        }
        if (layoutIndex == 1) layoutName = "4x4 Grid";
        else if (layoutIndex == 2) layoutName = "Tiles";
        else if (layoutIndex == 11) layoutName = "Cover Flow";

        QHBoxLayout* badgeLayout = new QHBoxLayout();
        badgeLayout->setContentsMargins(0, 0, 0, 0);
        QLabel* layoutBadge = new QLabel(layoutName, card);
        layoutBadge->setStyleSheet(theme.badgeStyle + " font-size: 9px; font-weight: bold; border-radius: 9px; padding: 3px 8px; " + theme.badgeBgStyle);
        badgeLayout->addWidget(layoutBadge);
        badgeLayout->addStretch(1);
        details->addLayout(badgeLayout);

        layout->addLayout(details, 1);

        QColor accentColor(theme.textStyle.split(';').first().split(' ').last().trimmed());
        FolderGraphicWidget* graphic = new FolderGraphicWidget(path, layoutIndex, accentColor, card);
        layout->addWidget(graphic);

        m_pinnedLayout->addWidget(card, row, col, Qt::AlignLeft | Qt::AlignVCenter);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }
    m_pinnedLayout->setColumnStretch(4, 1);
}

void HomeDashboardWidget::onDriveDoubleClicked(const QString& path) {
    emit navigateRequested(path);
}

void HomeDashboardWidget::onQuickAccessClicked(const QString& path) {
    emit navigateRequested(path);
}

void HomeDashboardWidget::onPinnedFolderClicked(const QString& path, int layoutIndex) {
    emit navigateWithLayoutRequested(path, layoutIndex);
}

void HomeDashboardWidget::onUnpinFolderClicked(const QString& path) {
    QSettings settings("Amifiles", "Amifiles");
    QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
    for (int i = 0; i < pinned.size(); ++i) {
        if (pinned[i].startsWith(path + ";")) {
            pinned.removeAt(i);
            break;
        }
    }
    settings.setValue("dashboard/pinned_folders", pinned);
    populatePinnedFolders();
}

void HomeDashboardWidget::onToolButtonClicked(const QString& action) {
    QWidget* parentW = parentWidget();
    while (parentW && !parentW->inherits("MainWindow")) {
        parentW = parentW->parentWidget();
    }
    MainWindow* mw = qobject_cast<MainWindow*>(parentW);
    if (!mw) return;

    if (action == "bulk_rename") {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Bulk Rename", QDir::homePath(), "All Files (*)");
        if (!files.isEmpty()) {
            BulkRenameDialog dlg(files, this);
            dlg.exec();
        }
    } else if (action == "recursive_renamer") {
        QMetaObject::invokeMethod(mw, "onFolderImageRenamer");
    } else if (action == "video_scraper") {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Videos to Scrape Metadata", QDir::homePath(), "Video Files (*.mp4 *.mkv *.avi *.mov *.webm *.mpeg *.mpg)");
        if (!files.isEmpty()) {
            VideoScraperDialog dlg(files, this);
            dlg.exec();
        }
    } else if (action == "archive_creator") {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Compress", QDir::homePath(), "All Files (*)");
        if (!files.isEmpty()) {
            ArchiveDialog dlg(ArchiveDialog::ModeCreate, files, QDir::homePath(), false, this);
            dlg.exec();
        }
    } else if (action == "visual_diff") {
        QMetaObject::invokeMethod(mw, "onCompareSyncAction");
    } else if (action == "dup_finder") {
        QMetaObject::invokeMethod(mw, "onDuplicateFinderAction");
    } else if (action == "preferences") {
        QMetaObject::invokeMethod(mw, "onOpenPreferences");
    }
}

void HomeDashboardWidget::populatePinnedProfiles() {
    clearLayout(m_pinnedProfilesLayout);
    m_pinnedProfilesLayout->setSpacing(6);
    for (int i = 0; i < 10; ++i) m_pinnedProfilesLayout->setColumnStretch(i, 0);

    QSettings settings("Amifiles", "Amifiles");
    QStringList pinned = settings.value("dashboard/pinned_profiles").toStringList();

    if (pinned.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No layout profiles pinned yet. Right-click any profile inside the Folder Profiles & Layouts Dialog and click '📌 Pin to Smart Home' to create shortcuts.", this);
        emptyLabel->setStyleSheet("color: #a6adc8; font-size: 11px; font-style: italic; padding: 10px;");
        m_pinnedProfilesLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int row = 0;
    int col = 0;

    for (const QString& profileName : pinned) {
        CardTheme theme = getCardTheme(col + row * 4, 0);
        theme.symbol = "⚙️";

        ClickableCardFrame* card = new ClickableCardFrame(profileName, ClickableCardFrame::PinnedProfile, -1, this);
        card->setFixedSize(190, 74);
        card->setStyleSheet(QString("QFrame#cardFrame { %1 border-radius: 12px; } QFrame#cardFrame:hover { background-color: rgba(255, 255, 255, 0.05); }").arg(theme.bgStyle));
        
        connect(card, &ClickableCardFrame::doubleClicked, this, [this](const QString& name) {
            emit applyProfileRequested(name);
        });

        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QWidget::customContextMenuRequested, this, [this, profileName, card](const QPoint& pos) {
            QMenu menu(card);
            QAction* actUnpin = menu.addAction("📌 Unpin from Home Screen");
            QAction* selected = menu.exec(card->mapToGlobal(pos));
            if (selected == actUnpin) {
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this,
                    "Unpin Profile",
                    QString("Are you sure you want to unpin layout profile '%1' from the Home Screen?").arg(profileName),
                    QMessageBox::Yes | QMessageBox::No
                );
                if (reply == QMessageBox::Yes) {
                    QSettings settings("Amifiles", "Amifiles");
                    QStringList pinnedProfs = settings.value("dashboard/pinned_profiles").toStringList();
                    pinnedProfs.removeAll(profileName);
                    settings.setValue("dashboard/pinned_profiles", pinnedProfs);
                    settings.sync();
                    populatePinnedProfiles();
                }
            }
        });

        QHBoxLayout* layout = new QHBoxLayout(card);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(8);

        QVBoxLayout* details = new QVBoxLayout();
        details->setSpacing(4);

        QLabel* nameLabel = new QLabel(profileName, card);
        nameLabel->setStyleSheet(theme.textStyle + " font-size: 13px;");
        details->addWidget(nameLabel);

        QLabel* subLabel = new QLabel("Layout Profile", card);
        subLabel->setStyleSheet("font-size: 11px; color: #a6adc8;");
        details->addWidget(subLabel);

        QHBoxLayout* badgeLayout = new QHBoxLayout();
        badgeLayout->setContentsMargins(0, 0, 0, 0);
        QLabel* layoutBadge = new QLabel("Double-Click to Apply", card);
        layoutBadge->setStyleSheet(theme.badgeStyle + " font-size: 9px; font-weight: bold; border-radius: 9px; padding: 3px 8px; " + theme.badgeBgStyle);
        badgeLayout->addWidget(layoutBadge);
        badgeLayout->addStretch(1);
        details->addLayout(badgeLayout);

        layout->addLayout(details, 1);

        QColor accentColor(theme.textStyle.split(';').first().split(' ').last().trimmed());
        FolderGraphicWidget* graphic = new FolderGraphicWidget("", -2, accentColor, card);
        layout->addWidget(graphic);

        m_pinnedProfilesLayout->addWidget(card, row, col, Qt::AlignLeft | Qt::AlignVCenter);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }
    m_pinnedProfilesLayout->setColumnStretch(4, 1);
}

void HomeDashboardWidget::populateRecentLocations() {
    clearLayout(m_recentLocationsLayout);
    m_recentLocationsLayout->setSpacing(12);
    for (int i = 0; i < 10; ++i) m_recentLocationsLayout->setColumnStretch(i, 0);

    QSettings settings("Amifiles", "Amifiles");
    QStringList recents = settings.value("recents/folders").toStringList();

    if (recents.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No recent locations visited yet. Navigate to some folders to build your history.", this);
        emptyLabel->setStyleSheet("color: #a6adc8; font-size: 11px; font-style: italic; padding: 10px;");
        m_recentLocationsLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int row = 0;
    int col = 0;

    for (const QString& path : recents) {
        CardTheme theme = getCardTheme(col + row * 4, 1);
        theme.symbol = "🕒";

        QFileInfo fi(path);
        QString dispName = fi.fileName();
        if (dispName.isEmpty()) dispName = path;

        ClickableCardFrame* card = new ClickableCardFrame(path, ClickableCardFrame::RecentLocation, -1, this);
        card->setFixedSize(190, 74);
        card->setStyleSheet(QString("QFrame#cardFrame { %1 border-radius: 12px; } QFrame#cardFrame:hover { background-color: rgba(255, 255, 255, 0.05); }").arg(theme.bgStyle));
        
        connect(card, &ClickableCardFrame::doubleClicked, this, [this, path](const QString&) {
            emit navigateRequested(path);
        });

        QVBoxLayout* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 10);
        cardLay->setSpacing(4);

        QHBoxLayout* titleLay = new QHBoxLayout();
        QLabel* lblIcon = new QLabel(theme.symbol, card);
        lblIcon->setStyleSheet("font-size: 16px;");
        QLabel* lblTitle = new QLabel(dispName, card);
        lblTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #cdd6f4;");
        lblTitle->setWordWrap(true);
        titleLay->addWidget(lblIcon);
        titleLay->addWidget(lblTitle, 1);
        cardLay->addLayout(titleLay);

        QLabel* lblPath = new QLabel(QDir::toNativeSeparators(path), card);
        lblPath->setStyleSheet("font-size: 10px; color: #a6adc8;");
        lblPath->setWordWrap(false);
        lblPath->setText(lblPath->fontMetrics().elidedText(lblPath->text(), Qt::ElideMiddle, 166));
        cardLay->addWidget(lblPath);

        m_recentLocationsLayout->addWidget(card, row, col);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
            if (row >= 2) break;
        }
    }
}

void HomeDashboardWidget::onDashboardContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }"
        "QMenu::item:selected { background-color: #313244; color: #a6e3a1; }"
    );

    QSettings settings("Amifiles", "Amifiles");
    bool showBanner = settings.value("dashboard/show_welcome_banner", true).toBool();

    QAction* actToggleBanner = menu.addAction("Show Welcome Banner Header");
    actToggleBanner->setCheckable(true);
    actToggleBanner->setChecked(showBanner);

    QWidget* scrollContent = qobject_cast<QWidget*>(sender());
    QPoint globalPos = scrollContent ? scrollContent->mapToGlobal(pos) : QCursor::pos();
    QAction* selected = menu.exec(globalPos);
    if (selected == actToggleBanner) {
        bool newValue = !showBanner;
        settings.setValue("dashboard/show_welcome_banner", newValue);
        settings.sync();
        if (m_bannerFrame) {
            m_bannerFrame->setVisible(newValue);
        }
    }
}

void HomeDashboardWidget::populateRecentFiles() {
    clearLayout(m_recentFilesLayout);
    m_recentFilesLayout->setSpacing(12);
    for (int i = 0; i < 10; ++i) m_recentFilesLayout->setColumnStretch(i, 0);

    QSettings settings("Amifiles", "Amifiles");
    QStringList recents = settings.value("dashboard/recent_files").toStringList();

    if (recents.isEmpty()) {
        QLabel* emptyLabel = new QLabel("No recent files opened yet.", this);
        emptyLabel->setStyleSheet("color: #a6adc8; font-size: 11px; font-style: italic; padding: 10px;");
        m_recentFilesLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int row = 0;
    int col = 0;

    for (const QString& path : recents) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;

        CardTheme theme = getCardTheme(col + row * 4, 3);
        theme.symbol = "📄";
        QString ext = fi.suffix().toLower();
        if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov") theme.symbol = "🎬";
        else if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg") theme.symbol = "🎵";
        else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif") theme.symbol = "🖼️";

        ClickableCardFrame* card = new ClickableCardFrame(path, ClickableCardFrame::RecentFile, -1, this);
        card->setFixedSize(190, 74);
        card->setStyleSheet(QString("QFrame#cardFrame { %1 border-radius: 12px; } QFrame#cardFrame:hover { background-color: rgba(255, 255, 255, 0.05); }").arg(theme.bgStyle));

        connect(card, &ClickableCardFrame::doubleClicked, this, [this, path](const QString&) {
            QWidget* p = parentWidget();
            while (p && !p->inherits("MainWindow")) {
                p = p->parentWidget();
            }
            MainWindow* mw = qobject_cast<MainWindow*>(p);
            if (mw) {
                QString ext = QFileInfo(path).suffix().toLower();
                static const QStringList mediaExts = {
                    "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "mod", "sid", "s3m", "xm", "it",
                    "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg"
                };
                if (mediaExts.contains(ext)) {
                    mw->onPlayMediaBuiltin({path});
                } else if (ext == "cbz" || ext == "cbr") {
                    ComicBookViewerDialog* dlg = new ComicBookViewerDialog(path, mw);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->show();
                } else {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                }
            }
        });

        QVBoxLayout* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 10);
        cardLay->setSpacing(4);

        QHBoxLayout* titleLay = new QHBoxLayout();
        QLabel* lblIcon = new QLabel(theme.symbol, card);
        lblIcon->setStyleSheet("font-size: 16px;");
        QLabel* lblTitle = new QLabel(fi.fileName(), card);
        lblTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #cdd6f4;");
        lblTitle->setWordWrap(true);
        titleLay->addWidget(lblIcon);
        titleLay->addWidget(lblTitle, 1);
        cardLay->addLayout(titleLay);

        QLabel* lblPath = new QLabel(QDir::toNativeSeparators(fi.absolutePath()), card);
        lblPath->setStyleSheet("font-size: 10px; color: #a6adc8;");
        lblPath->setWordWrap(false);
        lblPath->setText(lblPath->fontMetrics().elidedText(lblPath->text(), Qt::ElideMiddle, 166));
        cardLay->addWidget(lblPath);

        m_recentFilesLayout->addWidget(card, row, col);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
            if (row >= 2) break;
        }
    }
}

void HomeDashboardWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void HomeDashboardWidget::dragMoveEvent(QDragMoveEvent* event) {
    event->acceptProposedAction();
}

void HomeDashboardWidget::dropEvent(QDropEvent* event) {
    QString mimeText = event->mimeData()->text();
    QStringList parts = mimeText.split(":");
    if (parts.size() < 2) return;
    int typeVal = parts[0].toInt();
    QString srcPath = parts.mid(1).join(":");

    QWidget* targetWidget = childAt(event->position().toPoint());
    if (!targetWidget) return;

    ClickableCardFrame* targetCard = nullptr;
    QWidget* w = targetWidget;
    while (w && w != this) {
        if (ClickableCardFrame* c = qobject_cast<ClickableCardFrame*>(w)) {
            targetCard = c;
            break;
        }
        w = w->parentWidget();
    }

    if (!targetCard) return;

    QString destPath = targetCard->path();
    if (srcPath == destPath) return;

    QSettings settings("Amifiles", "Amifiles");
    if (typeVal == 0 && targetCard->cardType() == ClickableCardFrame::PinnedFolder) {
        QStringList pinned = settings.value("dashboard/pinned_folders").toStringList();
        int srcIdx = -1, destIdx = -1;
        for (int i = 0; i < pinned.size(); ++i) {
            if (pinned[i].startsWith(srcPath + ";")) srcIdx = i;
            if (pinned[i].startsWith(destPath + ";")) destIdx = i;
        }
        if (srcIdx != -1 && destIdx != -1) {
            QString item = pinned.takeAt(srcIdx);
            destIdx = -1;
            for (int i = 0; i < pinned.size(); ++i) {
                if (pinned[i].startsWith(destPath + ";")) destIdx = i;
            }
            pinned.insert(destIdx, item);
            settings.setValue("dashboard/pinned_folders", pinned);
            populatePinnedFolders();
        }
    } else if (typeVal == 1 && targetCard->cardType() == ClickableCardFrame::PinnedProfile) {
        QStringList pinned = settings.value("dashboard/pinned_profiles").toStringList();
        int srcIdx = pinned.indexOf(srcPath);
        int destIdx = pinned.indexOf(destPath);
        if (srcIdx != -1 && destIdx != -1) {
            QString item = pinned.takeAt(srcIdx);
            destIdx = pinned.indexOf(destPath);
            pinned.insert(destIdx, item);
            settings.setValue("dashboard/pinned_profiles", pinned);
            populatePinnedProfiles();
        }
    } else if (typeVal == 3 && targetCard->cardType() == ClickableCardFrame::RecentLocation) {
        QStringList recents = settings.value("recents/folders").toStringList();
        int srcIdx = recents.indexOf(srcPath);
        int destIdx = recents.indexOf(destPath);
        if (srcIdx != -1 && destIdx != -1) {
            QString item = recents.takeAt(srcIdx);
            destIdx = recents.indexOf(destPath);
            recents.insert(destIdx, item);
            settings.setValue("recents/folders", recents);
            populateRecentLocations();
        }
    } else if (typeVal == 4 && targetCard->cardType() == ClickableCardFrame::RecentFile) {
        QStringList recents = settings.value("dashboard/recent_files").toStringList();
        int srcIdx = recents.indexOf(srcPath);
        int destIdx = recents.indexOf(destPath);
        if (srcIdx != -1 && destIdx != -1) {
            QString item = recents.takeAt(srcIdx);
            destIdx = recents.indexOf(destPath);
            recents.insert(destIdx, item);
            settings.setValue("dashboard/recent_files", recents);
            populateRecentFiles();
        }
    }
}

#include "homedashboardwidget.moc"
