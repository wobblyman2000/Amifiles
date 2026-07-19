#ifndef FILEPANEL_H
#define FILEPANEL_H

#include <QWidget>
#include <QDialog>
#include <QStringListModel>
#include <QTimer>
#include <QThread>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include "tagmanager.h"
#include <QTreeView>
#include <QListView>
#include <QStackedWidget>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QSettings>
#include <QComboBox>
#include <QStringList>
#include <QModelIndex>
#include <QFileInfo>
#include <QDir>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QVariant>
#include <QPainter>
#include <QImage>
#include <QImageReader>
#include <QRunnable>
#include <QThreadPool>
#include <QPointer>
#include <QSet>
#include <QLinearGradient>
#include <QPolygon>
#include <QIcon>
#include <QSettings>
#include "foldersizecalculator.h"
#include "flatmodel.h"
#include "customfilesystemmodel.h"

class ArchiveModel;
class SearchWorker;
class FileFilterProxyModel;

class CasingRunnable : public QRunnable {
public:
    CasingRunnable(QPointer<FileFilterProxyModel> model, const QString& path);
    void run() override;
private:
    QPointer<FileFilterProxyModel> m_model;
    QString m_path;
};

// Custom filter proxy model to support prefix/substring matching and file type categories
struct AgeColorRule {
    QString op;     // "<=" or ">="
    int days;
    QString color;  // Hex code or "None"
    QString icon;   // Emoji/prefix string or "None"
    QString name;   // Friendly name
};

class FileFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    friend class CasingRunnable;

public:
    enum FilterType { FilterAll, FilterAudio, FilterVideos, FilterPictures, FilterDocs, FilterArchive };

    explicit FileFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setZoomIconSize(int size) {
        if (m_zoomIconSize != size) {
            m_zoomIconSize = size;
            clearCasingCache();
        }
    }

    static QList<AgeColorRule> defaultRules() {
        return {
            {"<=", 1, "#ff5555", "🔥", "New (< 24h)"},
            {"<=", 7, "#89b4fa", "⚡", "Recent (< 1 week)"},
            {"<=", 30, "#a6e3a1", "🟢", "Active (< 1 month)"},
            {">=", 365, "#6c7086", "❄️", "Stale (> 1 year)"}
        };
    }

    void loadAgeRules() {
        m_ageRules.clear();
        QSettings settings("Amifiles", "Amifiles");
        QStringList list = settings.value("preferences/age_rules").toStringList();
        if (list.isEmpty()) {
            m_ageRules = defaultRules();
        } else {
            for (const QString& ruleStr : list) {
                QStringList parts = ruleStr.split(';');
                if (parts.size() >= 5) {
                    AgeColorRule r;
                    r.op = parts[0];
                    r.days = parts[1].toInt();
                    r.color = parts[2];
                    r.icon = parts[3];
                    r.name = parts[4];
                    m_ageRules.append(r);
                }
            }
        }
    }

    static void saveRules(const QList<AgeColorRule>& rules) {
        QSettings settings("Amifiles", "Amifiles");
        QStringList list;
        for (const auto& r : rules) {
            list << QString("%1;%2;%3;%4;%5").arg(r.op).arg(r.days).arg(r.color).arg(r.icon).arg(r.name);
        }
        settings.setValue("preferences/age_rules", list);
    }

    void setCurrentPath(const QString& path) {
        m_currentPath = QDir::cleanPath(path);
        invalidate();
    }

    void setFilterType(FilterType type) {
        m_filterType = type;
        invalidate();
    }

    FilterType filterType() const { return m_filterType; }

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidate();
    }

    void setSizeFilter(qint64 minSize, qint64 maxSize) {
        m_minSize = minSize;
        m_maxSize = maxSize;
        invalidate();
    }

    void setDateFilter(const QDateTime& minDate, const QDateTime& maxDate) {
        m_minDate = minDate;
        m_maxDate = maxDate;
        invalidate();
    }

    void setTagFilter(const QString& tag) {
        m_filterTag = tag;
        invalidate();
    }

    void clearAdvancedFilters() {
        m_minSize = -1;
        m_maxSize = -1;
        m_minDate = QDateTime();
        m_maxDate = QDateTime();
        m_filterTag = QString();
        invalidate();
    }

    void setAgeColoringEnabled(bool enabled) {
        m_ageColoringEnabled = enabled;
        emit layoutChanged();
    }

    void clearCasingCache() {
        m_casingCache.clear();
        m_iconCache.clear();
        invalidate();
    }

    bool hasCasingCover(const QString& path) const {
        return m_casingCache.contains(path) && !m_casingCache.value(path).first.isEmpty();
    }

    // Overriding data() to support dynamic file age text coloring and icon badges
    QVariant data(const QModelIndex& index, int role) const override {
        if (m_ageRules.isEmpty()) {
            const_cast<FileFilterProxyModel*>(this)->loadAgeRules();
        }

        if (role == Qt::ForegroundRole && m_ageColoringEnabled) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel) {
                QFileInfo info = fileModel->fileInfo(srcIndex);
                QDateTime lastMod = info.lastModified();
                QDateTime now = QDateTime::currentDateTime();
                qint64 secs = lastMod.secsTo(now);
                if (secs >= 0) {
                    double days = secs / (24.0 * 3600.0);
                    for (const auto& rule : m_ageRules) {
                        bool matches = false;
                        if (rule.op == "<=") {
                            matches = (days <= rule.days);
                        } else if (rule.op == ">=") {
                            matches = (days >= rule.days);
                        }
                        if (matches && !rule.color.isEmpty() && rule.color != "None") {
                            return QBrush(QColor(rule.color));
                        }
                    }
                }
            }
        }

        if (role == Qt::DisplayRole && index.column() == 0 && m_ageColoringEnabled) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel) {
                QFileInfo info = fileModel->fileInfo(srcIndex);
                QDateTime lastMod = info.lastModified();
                QDateTime now = QDateTime::currentDateTime();
                qint64 secs = lastMod.secsTo(now);
                if (secs >= 0) {
                    double days = secs / (24.0 * 3600.0);
                    for (const auto& rule : m_ageRules) {
                        bool matches = false;
                        if (rule.op == "<=") {
                            matches = (days <= rule.days);
                        } else if (rule.op == ">=") {
                            matches = (days >= rule.days);
                        }
                        if (matches && !rule.icon.isEmpty() && rule.icon != "None") {
                            QString originalName = QSortFilterProxyModel::data(index, role).toString();
                            return rule.icon + " " + originalName;
                        }
                    }
                }
            }
        }

        if (role == Qt::DecorationRole && index.column() == 0) {
            QSettings settings("Amifiles", "Amifiles");
            bool casingEnabled = settings.value("preferences/casing_overlays", true).toBool();
            if (casingEnabled) {
                QModelIndex srcIndex = mapToSource(index);
                QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
                if (fileModel) {
                    bool isDir = fileModel->isDir(srcIndex);
                    QString path = fileModel->filePath(srcIndex);
                    bool isMedia = false;
                    if (!isDir) {
                        QString ext = QFileInfo(path).suffix().toLower();
                        QStringList mediaExts = { "mp3", "flac", "wav", "ogg", "m4a", "wma", "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v" };
                        isMedia = mediaExts.contains(ext);
                    }
                    
                    if (isDir || isMedia) {
                        if (m_casingCache.contains(path)) {
                            QPair<QString, int> cachedVal = m_casingCache.value(path);
                            QString artPath = cachedVal.first;
                            int casingInt = cachedVal.second;
                            
                            if (artPath.isEmpty()) {
                                return QSortFilterProxyModel::data(index, role);
                            }
                            
                            QString cacheKey = artPath + "_" + QString::number(casingInt);
                            if (m_iconCache.contains(cacheKey)) {
                                return m_iconCache.value(cacheKey);
                            }
                        }
                        
                        if (!m_pendingCasingChecks.contains(path)) {
                            m_pendingCasingChecks.insert(path);
                            QPointer<FileFilterProxyModel> ptr(const_cast<FileFilterProxyModel*>(this));
                            QThreadPool::globalInstance()->start(new CasingRunnable(ptr, path));
                        }
                    }
                }
            }
        }

        if (role == Qt::DisplayRole && index.column() == 1) { // Size column
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel && fileModel->isDir(srcIndex)) {
                QString path = fileModel->filePath(srcIndex);
                qint64 size = FolderSizeCalculator::instance().getFolderSize(path);
                if (size == -1) {
                    return "Calculating...";
                } else {
                    double kb = size / 1024.0;
                    double mb = kb / 1024.0;
                    double gb = mb / 1024.0;
                    if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
                    if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
                    if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
                    return QString("%1 B").arg(size);
                }
            }
        }

        return QSortFilterProxyModel::data(index, role);
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override {
        QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
        if (!fileModel) return true;

        QModelIndex index = fileModel->index(source_row, 0, source_parent);
        QString filePath = QDir::cleanPath(fileModel->filePath(index));

        // 1. ALWAYS accept ancestors of the current path and the current path itself.
        if (m_currentPath.startsWith(filePath, Qt::CaseInsensitive)) {
            return true;
        }

        // 2. Check if this is a descendant of the current directory subtree.
        bool isDescendant = filePath.startsWith(m_currentPath + "/", Qt::CaseInsensitive);
        if (!isDescendant) {
            return true;
        }

        QString fileName = fileModel->fileName(index);
        bool isDir = fileModel->isDir(index);

        if (!isDir) {
            qint64 size = fileModel->size(index);
            if (m_minSize != -1 && size < m_minSize) {
                return false;
            }
            if (m_maxSize != -1 && size > m_maxSize) {
                return false;
            }

            QDateTime modDate = fileModel->lastModified(index);
            if (m_minDate.isValid() && modDate < m_minDate) {
                return false;
            }
            if (m_maxDate.isValid() && modDate > m_maxDate) {
                return false;
            }
        }

        // 2.5. Apply Tag Filter
        if (!m_filterTag.isEmpty()) {
            if (isDir) {
                // Keep directories navigable
            } else {
                if (!TagManager::instance().getFileTags(filePath).contains(m_filterTag, Qt::CaseInsensitive)) {
                    return false;
                }
            }
        }

        // 3. Apply Text Filter (case-insensitive substring contains check)
        if (!m_filterText.isEmpty()) {
            if (!fileName.contains(m_filterText, Qt::CaseInsensitive)) {
                return false;
            }
        }

        // 4. Apply Type Filter
        if (m_filterType == FilterAll) {
            return true;
        }

        // Hide directory nodes when a specific type filter is active to show only documents/media/etc.
        if (isDir) {
            return false;
        }

        QString ext = QFileInfo(fileName).suffix().toLower();
        if (m_filterType == FilterAudio) {
            static const QStringList audioExts = {
                "mp3", "wav", "flac", "ogg", "m4a", "wma", "aac", "mid", "midi"
            };
            return audioExts.contains(ext);
        } else if (m_filterType == FilterVideos) {
            static const QStringList videoExts = {
                "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg"
            };
            return videoExts.contains(ext);
        } else if (m_filterType == FilterPictures) {
            static const QStringList pictureExts = {
                "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tiff", "ico"
            };
            return pictureExts.contains(ext);
        } else if (m_filterType == FilterDocs) {
            static const QStringList docExts = {
                "txt", "log", "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt",
                "ods", "odp", "md", "csv", "rtf", "html", "xml", "json"
            };
            return docExts.contains(ext);
        } else if (m_filterType == FilterArchive) {
            static const QStringList archiveExts = {
                "zip", "tar", "gz", "bz2", "xz", "rar", "7z", "tgz"
            };
            return archiveExts.contains(ext);
        }

        return true;
    }

private:
    FilterType m_filterType = FilterAll;
    QString m_filterText;
    QString m_currentPath;
    bool m_ageColoringEnabled = true; // Enabled by default
    qint64 m_minSize = -1;
    qint64 m_maxSize = -1;
    QDateTime m_minDate;
    QDateTime m_maxDate;
    QString m_filterTag;
    mutable QHash<QString, QPair<QString, int>> m_casingCache;
    mutable QHash<QString, QIcon> m_iconCache;
    mutable QList<AgeColorRule> m_ageRules;
    int m_zoomIconSize = 24;
    mutable QSet<QString> m_pendingCasingChecks;

private slots:
    void onCasingRendered(const QString& path, const QString& artPath, int casingType, const QImage& image);
};

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(const QString& initialPath, QWidget* parent = nullptr);
    ~FilePanel() override;

    bool isArchiveViewActive() const { return m_archiveViewActive; }
    int viewModeIndex() const;
    void setViewModeIndex(int index);

    QString currentPath() const;
    void setPath(const QString& path);
    void focusActiveView();
    class QScrollBar* activeVerticalScrollBar() const;

    bool isPinned() const { return m_isPinned; }
    void setPinned(bool pin) { m_isPinned = pin; }
    bool isPathLocked() const { return m_isPathLocked; }
    void setPathLocked(bool lock) { m_isPathLocked = lock; if (lock) m_lockedPath = m_currentPath; }
    bool isPathLockedWithSubdirs() const { return m_isPathLockedWithSubdirs; }
    void setPathLockedWithSubdirs(bool lock) { m_isPathLockedWithSubdirs = lock; if (lock) m_lockedPath = m_currentPath; }

    void setActive(bool active);
    bool isActive() const { return m_isActive; }

    QStringList selectedPaths() const;
    QString activeFilePath() const; // First selected file, or current directory if none
    QString folderArtPath() const { return m_folderArtPath; }

    void refresh();
    void updateStyles();
    void autoSizeAllColumns();

    // Clipboard and File Operations
    void onCopy();
    void onCut();
    void onPaste();
    void onDelete();
    void onRename();
    void onNewFolder();
    void onShowProperties();

    // Age Coloring support
    FileFilterProxyModel* proxyModel() const { return m_proxyModel; }
    class GroupProxyModel* groupProxy() const { return m_groupProxy; }
    QAbstractItemModel* activeBaseModel() const;
    void updateActiveViewModel();

    // View modular filter components
    void setCategoryButtonsVisible(bool visible);
    void setFilterTextBarVisible(bool visible);
    bool isCategoryButtonsVisible() const;
    bool isFilterTextBarVisible() const;

    // Sibling panel mapping for cross-panel filtering
    void setSiblingPanel(FilePanel* sibling) { m_siblingPanel = sibling; }
    QString filterText() const;
    void syncFilterText(const QString& text);
    void syncFilterType(FileFilterProxyModel::FilterType type);
    void syncZoom(int value);
    void setViewModeGrid(bool grid) {
        if (grid && m_viewStack && m_viewStack->currentWidget() == m_treeView) {
            onToggleViewMode();
        } else if (!grid && m_viewStack && m_viewStack->currentWidget() == m_listView) {
            onToggleViewMode();
        }
    }

    // Flat View Support
    void setFlatViewEnabled(bool enabled);
    bool isFlatViewEnabled() const { return m_flatViewEnabled; }
    
    // Search Support
    void setSearchQuery(const QString& query);
    QString searchQuery() const;

signals:
    void pathChanged(const QString& path);
    void fileSelected(const QString& filePath);
    void folderArtDetected(const QString& artPath);
    void panelActivated(FilePanel* panel);
    void playlistPlayRequested(const QStringList& filePaths);
    void zoomChanged(int value);
    void sigStartSearch(const QString& query, const QString& path);
    void clonePathRequested(const QString& path);
    void tabPressed();
    void viewModeChanged();
    void openNewTabRequested(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

public slots:
    void onNavigateUp();
    void onNavigateBack();
    void onNavigateForward();
    void onViewModeChanged(int index);

private slots:
    void onPathEntered();
    void onFavoriteClicked();
    void onClonePathClicked();
    void onFilterChanged(const QString& filterText);
    void onFilterTypeChanged();
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex& index);
    void onDoubleClickedPath(const QString& path);
    void updateFavoritesUI();
    void updateCloneButtonIcon();
    void onCustomContextMenu(const QPoint& pos);
    void onFavoriteButtonContextMenu(const QPoint& pos);
    void onToggleViewMode();
    void onZoomChanged(int value);
    void onGroupingChanged(int index);
    void onHeaderContextMenu(const QPoint& pos);
    void onGlobalSearchChanged(const QString& text);
    void startSearch();
    void onSearchResultsReady(const QStringList& results);
    void onSearchFinished();
    void onSearchResultSelected(const QModelIndex& index);
    void onSearchResultDoubleClicked(const QModelIndex& index);
    void zoomIn();
    void zoomOut();

private:
    void setupUI();
    void updateNavigationButtons();
    void checkFolderArt();
    void updateStatusText();
    void navigateTo(const QString& path, bool addHistory = true);
    bool copyRecursively(const QString& srcPath, const QString& destPath);
    void loadSortSettings();
    void loadColumnWidths();
    void saveColumnWidth(int logicalIndex, int width);

    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    bool m_isActive = false;
    bool m_categoryButtonsVisible = true;
    bool m_filterTextBarVisible = true;
    QString m_currentPath;
    QStringList m_history;
    int m_historyIndex = -1;

    bool m_isPinned = false;
    bool m_isPathLocked = false;
    bool m_isPathLockedWithSubdirs = false;
    QString m_lockedPath;

    // UI Elements
    QToolButton* m_btnBack = nullptr;
    QToolButton* m_btnForward = nullptr;
    QToolButton* m_btnUp = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QToolButton* m_btnGo = nullptr;
    QToolButton* m_btnFavorite = nullptr;
    QToolButton* m_btnClonePath = nullptr;
    QToolButton* m_btnFlatView = nullptr;
    QToolButton* m_btnViewMode = nullptr; // keep single instance
    QHeaderView* m_header = nullptr;
    // New global search components
    QLineEdit* m_globalSearchEdit = nullptr;
    QListView* m_searchResultsView = nullptr;
    QTimer* m_searchDebounceTimer = nullptr;
    QThread* m_searchThread = nullptr;
    SearchWorker* m_searchWorker = nullptr;
    // Model for displaying results
    QStringListModel* m_searchResultModel = nullptr;

    QTreeView* m_treeView = nullptr;
    CustomFileSystemModel* m_fileModel = nullptr;
    FileFilterProxyModel* m_proxyModel = nullptr;
    class GroupProxyModel* m_groupProxy = nullptr;
    QComboBox* m_comboGrouping = nullptr;

    FlatFileSystemModel* m_flatModel = nullptr;
    QSortFilterProxyModel* m_flatProxyModel = nullptr;
    bool m_flatViewEnabled = false;

    class SmartFolderModel* m_smartModel = nullptr;
    bool m_smartViewActive = false;

    class DiskDashboardWidget* m_dashboardWidget = nullptr;
    bool m_dashboardActive = false;

    ArchiveModel* m_archiveModel = nullptr;
    bool m_archiveViewActive = false;

    QSlider* m_zoomSlider = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QListView* m_listView = nullptr;
    class MillerColumnsView* m_millerView = nullptr;
    class TimelineView* m_timelineView = nullptr;
    class FilmstripView* m_filmstripView = nullptr;
    class TheaterListView* m_theaterListView = nullptr;
    class QComboBox* m_comboViewMode = nullptr;
    class QAbstractItemDelegate* m_defaultDelegate = nullptr;
    class CardViewDelegate* m_cardDelegate = nullptr;
    class TheaterViewDelegate* m_theaterDelegate = nullptr;
    int m_zoomLevel = -1;

    // Bottom Filter Bar
    QLineEdit* m_filterEdit = nullptr;
    QToolButton* m_btnFilterAll = nullptr;
    QToolButton* m_btnFilterAudio = nullptr;
    QToolButton* m_btnFilterVideos = nullptr;
    QToolButton* m_btnFilterPictures = nullptr;
    QToolButton* m_btnFilterDocs = nullptr;
    QToolButton* m_btnFilterArchive = nullptr;
    QLabel* m_statusLabel = nullptr;

    QWidget* m_categoryWidget = nullptr;
    QWidget* m_filterTextWidget = nullptr;
    QWidget* m_statusWidget = nullptr;
    FilePanel* m_siblingPanel = nullptr;

    QString m_folderArtPath;
};

class QCheckBox;

class ColumnSelectorDialog : public QDialog {
    Q_OBJECT
public:
    ColumnSelectorDialog(const QStringList& columnNames, const QList<bool>& visibilities, QWidget* parent = nullptr);
    ~ColumnSelectorDialog() override = default;

    QList<bool> selectedVisibilities() const;

private:
    QList<QCheckBox*> m_checkboxes;
};

#endif // FILEPANEL_H
