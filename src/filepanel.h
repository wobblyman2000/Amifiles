#ifndef FILEPANEL_H
#define FILEPANEL_H

#include <QWidget>
#include <QDialog>
#include <QStringListModel>
#include <QTimer>
#include <QThread>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QListView>
#include <QStackedWidget>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QStringList>
#include <QModelIndex>
#include <QFileInfo>
#include <QDir>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QVariant>
#include <QPainter>
#include <QLinearGradient>
#include <QPolygon>
#include <QIcon>
#include <QImageReader>
#include <QSettings>
#include "foldersizecalculator.h"
#include "flatmodel.h"
#include "customfilesystemmodel.h"

class ArchiveModel;
class SearchWorker;

// Custom filter proxy model to support prefix/substring matching and file type categories
class FileFilterProxyModel : public QSortFilterProxyModel {

public:
    enum FilterType { FilterAll, FilterAudio, FilterVideos, FilterPictures, FilterDocs, FilterArchive };

    explicit FileFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

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

    void setAgeColoringEnabled(bool enabled) {
        m_ageColoringEnabled = enabled;
        emit layoutChanged();
    }

    void clearCasingCache() {
        m_casingCache.clear();
        m_iconCache.clear();
        invalidate();
    }

    // Overriding data() to support dynamic file age text coloring
    QVariant data(const QModelIndex& index, int role) const override {
        if (role == Qt::ForegroundRole && m_ageColoringEnabled) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel) {
                QFileInfo info = fileModel->fileInfo(srcIndex);
                QDateTime lastMod = info.lastModified();
                QDateTime now = QDateTime::currentDateTime();
                qint64 secs = lastMod.secsTo(now);
                if (secs >= 0) {
                    if (secs <= 24 * 3600) {
                        return QBrush(QColor("#ff5555")); // Red for <24h
                    } else if (secs <= 7 * 24 * 3600) {
                        return QBrush(QColor("#89b4fa")); // Blue for 24h - 7d
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
                if (fileModel && fileModel->isDir(srcIndex)) {
                    QString path = fileModel->filePath(srcIndex);
                    
                    enum CasingType { CasingCD, CasingDVD, CasingBluRay };
                    
                    QString artPath;
                    int casingInt = CasingCD;
                    
                    // Check casing cache first
                    if (m_casingCache.contains(path)) {
                        QPair<QString, int> cachedVal = m_casingCache.value(path);
                        artPath = cachedVal.first;
                        casingInt = cachedVal.second;
                    } else {
                        // Not cached - run checks
                        casingInt = CasingCD;
                        
                        // 1. Check Blu-ray covers first
                        QStringList blurayChecks = { "bluray_cover.jpg", "bluray_cover.png", "bluray.jpg", "bluray.png" };
                        for (const QString& check : blurayChecks) {
                            QString test = QDir(path).filePath(check);
                            if (QFile::exists(test)) {
                                artPath = test;
                                casingInt = CasingBluRay;
                                break;
                            }
                        }

                        // 2. Check DVD covers next
                        if (artPath.isEmpty()) {
                            QStringList dvdChecks = { "dvd_cover.jpg", "dvd_cover.png", "dvd.jpg", "dvd.png", "movie.jpg", "movie.png" };
                            for (const QString& check : dvdChecks) {
                                QString test = QDir(path).filePath(check);
                                if (QFile::exists(test)) {
                                    artPath = test;
                                    casingInt = CasingDVD;
                                    break;
                                }
                            }
                        }

                        // 3. Fall back to CD covers
                        if (artPath.isEmpty()) {
                            QStringList cdChecks = { "folder.jpg", "folder.png", "cover.jpg", "cover.png" };
                            for (const QString& check : cdChecks) {
                                QString test = QDir(path).filePath(check);
                                if (QFile::exists(test)) {
                                    artPath = test;
                                    casingInt = CasingCD;
                                    break;
                                }
                            }
                        }
                        
                        // Cache resolved paths
                        m_casingCache.insert(path, qMakePair(artPath, casingInt));
                    }

                    if (!artPath.isEmpty()) {
                        // Check pre-rendered icon cache first
                        QString cacheKey = artPath + "_" + QString::number(casingInt);
                        if (m_iconCache.contains(cacheKey)) {
                            return m_iconCache.value(cacheKey);
                        }
                        
                        QPixmap cover;
                        QImageReader reader(artPath);
                        if (reader.canRead()) {
                            QSize origSize = reader.size();
                            if (origSize.isValid()) {
                                int targetW = 220;
                                int targetH = 220;
                                if (casingInt == CasingDVD) {
                                    targetW = 160;
                                    targetH = 230;
                                } else if (casingInt == CasingBluRay) {
                                    targetW = 190;
                                    targetH = 210;
                                }
                                QSize scaledSize = origSize.scaled(QSize(targetW, targetH), Qt::KeepAspectRatioByExpanding);
                                reader.setScaledSize(scaledSize);
                            }
                            cover = QPixmap::fromImage(reader.read());
                        }
                        if (!cover.isNull()) {
                             int caseW = 256;
                             int caseH = 256;
                             
                             if (casingInt == CasingDVD) {
                                 caseW = 180;
                             } else if (casingInt == CasingBluRay) {
                                 caseW = 210;
                             }
                             
                             double s = 256.0 / 48.0;

                             QPixmap casePixmap(caseW, caseH);
                             casePixmap.fill(Qt::transparent);

                             QPainter painter(&casePixmap);
                             painter.setRenderHint(QPainter::Antialiasing);

                             if (casingInt == CasingCD) {
                                 painter.setBrush(QColor("#313244"));
                                 painter.setPen(QPen(QColor("#45475a"), qMax(1, qRound(1 * s))));
                                 painter.drawRoundedRect(qRound(2 * s), qRound(2 * s), caseW - qRound(4 * s), caseH - qRound(4 * s), qRound(3 * s), qRound(3 * s));

                                 painter.setBrush(QColor("#11111b"));
                                 painter.setPen(Qt::NoPen);
                                 painter.drawRect(qRound(3 * s), qRound(3 * s), qRound(5 * s), caseH - qRound(6 * s));

                                 int coverX = qRound(10 * s);
                                 int coverY = qRound(4 * s);
                                 int coverW = caseW - qRound(14 * s);
                                 int coverH = caseH - qRound(8 * s);
                                 painter.drawPixmap(coverX, coverY, coverW, coverH, cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

                                 painter.setBrush(Qt::NoBrush);
                                 painter.setPen(QPen(QColor(255, 255, 255, 60), qMax(1, qRound(1 * s))));
                                 painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), caseH - qRound(6 * s), qRound(2 * s), qRound(2 * s));

                                 QLinearGradient gradient(0, 0, caseW, caseH);
                                 gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
                                 gradient.setColorAt(0.3, QColor(255, 255, 255, 120));
                                 gradient.setColorAt(0.35, QColor(255, 255, 255, 0));
                                 gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

                                 painter.setBrush(gradient);
                                 painter.setPen(Qt::NoPen);
                                 QPolygon gloss;
                                 gloss << QPoint(qRound(9 * s), qRound(4 * s))
                                       << QPoint(caseW - qRound(4 * s), qRound(4 * s))
                                       << QPoint(qRound(9 * s), caseH - qRound(4 * s));
                                 painter.drawPolygon(gloss);
                             }
                             else if (casingInt == CasingDVD) {
                                 painter.setBrush(QColor("#1e1e2e"));
                                 painter.setPen(QPen(QColor("#313244"), qMax(1, qRound(1.5 * s))));
                                 painter.drawRoundedRect(qRound(2 * s), qRound(2 * s), caseW - qRound(4 * s), caseH - qRound(4 * s), qRound(4 * s), qRound(4 * s));

                                 int coverX = qRound(4 * s);
                                 int coverY = qRound(4 * s);
                                 int coverW = caseW - qRound(8 * s);
                                 int coverH = caseH - qRound(8 * s);
                                 painter.drawPixmap(coverX, coverY, coverW, coverH, cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

                                 painter.setBrush(Qt::NoBrush);
                                 painter.setPen(QPen(QColor(255, 255, 255, 40), qMax(1, qRound(1 * s))));
                                 painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), caseH - qRound(6 * s), qRound(3 * s), qRound(3 * s));

                                 QLinearGradient gradient(0, 0, caseW, caseH);
                                 gradient.setColorAt(0.0, QColor(255, 255, 255, 60));
                                 gradient.setColorAt(0.25, QColor(255, 255, 255, 100));
                                 gradient.setColorAt(0.3, QColor(255, 255, 255, 0));
                                 gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

                                 painter.setBrush(gradient);
                                 painter.setPen(Qt::NoPen);
                                 QPolygon gloss;
                                 gloss << QPoint(qRound(4 * s), qRound(4 * s))
                                       << QPoint(caseW - qRound(4 * s), qRound(4 * s))
                                       << QPoint(qRound(4 * s), caseH - qRound(4 * s));
                                 painter.drawPolygon(gloss);
                             }
                             else if (casingInt == CasingBluRay) {
                                 painter.setBrush(QColor("#1e1e2e"));
                                 painter.setPen(QPen(QColor("#313244"), qMax(1, qRound(1.5 * s))));
                                 painter.drawRoundedRect(qRound(2 * s), qRound(2 * s), caseW - qRound(4 * s), caseH - qRound(4 * s), qRound(4 * s), qRound(4 * s));

                                 int coverX = qRound(4 * s);
                                 int coverY = qRound(9 * s);
                                 int coverW = caseW - qRound(8 * s);
                                 int coverH = caseH - qRound(13 * s);
                                 painter.drawPixmap(coverX, coverY, coverW, coverH, cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

                                 QLinearGradient blueGrad(0, 0, 0, coverY);
                                 blueGrad.setColorAt(0.0, QColor(14, 165, 233, 220));
                                 blueGrad.setColorAt(1.0, QColor(3, 105, 161, 180));
                                 
                                 painter.setBrush(blueGrad);
                                 painter.setPen(Qt::NoPen);
                                 painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), qRound(7 * s), qRound(2 * s), qRound(2 * s));

                                 painter.setPen(QPen(QColor(255, 255, 255, 140), 1));
                                 QFont logoFont = painter.font();
                                 logoFont.setPointSize(qRound(2.2 * s));
                                 logoFont.setBold(true);
                                 logoFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(0.4 * s));
                                 painter.setFont(logoFont);
                                 painter.drawText(QRect(0, qRound(3 * s), caseW, qRound(6 * s)), Qt::AlignCenter, "BLU-RAY");

                                 painter.setBrush(Qt::NoBrush);
                                 painter.setPen(QPen(QColor(255, 255, 255, 45), qMax(1, qRound(1 * s))));
                                 painter.drawRoundedRect(qRound(3 * s), qRound(3 * s), caseW - qRound(6 * s), caseH - qRound(6 * s), qRound(3 * s), qRound(3 * s));

                                 QLinearGradient gradient(0, 0, caseW, caseH);
                                 gradient.setColorAt(0.0, QColor(255, 255, 255, 60));
                                 gradient.setColorAt(0.25, QColor(255, 255, 255, 90));
                                 gradient.setColorAt(0.3, QColor(255, 255, 255, 0));
                                 gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

                                 painter.setBrush(gradient);
                                 painter.setPen(Qt::NoPen);
                                 QPolygon gloss;
                                 gloss << QPoint(qRound(4 * s), qRound(4 * s))
                                       << QPoint(caseW - qRound(4 * s), qRound(4 * s))
                                       << QPoint(qRound(4 * s), caseH - qRound(4 * s));
                                 painter.drawPolygon(gloss);
                             }

                             painter.end();
                             QIcon iconResult(casePixmap);
                             m_iconCache.insert(cacheKey, iconResult);
                             return iconResult;
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
    mutable QHash<QString, QPair<QString, int>> m_casingCache;
    mutable QHash<QString, QIcon> m_iconCache;
};

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(const QString& initialPath, QWidget* parent = nullptr);
    ~FilePanel() override;

    bool isArchiveViewActive() const { return m_archiveViewActive; }

    QString currentPath() const;
    void setPath(const QString& path);

    void setActive(bool active);
    bool isActive() const { return m_isActive; }

    QStringList selectedPaths() const;
    QString activeFilePath() const; // First selected file, or current directory if none
    QString folderArtPath() const { return m_folderArtPath; }

    void refresh();

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onNavigateUp();
    void onNavigateBack();
    void onNavigateForward();
    void onPathEntered();
    void onFavoriteClicked();
    void onFilterChanged(const QString& filterText);
    void onFilterTypeChanged();
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex& index);
    void updateFavoritesUI();
    void onCustomContextMenu(const QPoint& pos);
    void onFavoriteButtonContextMenu(const QPoint& pos);
    void onToggleViewMode();
    void onZoomChanged(int value);
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
    void updateStyles();

    bool m_isActive = false;
    bool m_categoryButtonsVisible = true;
    bool m_filterTextBarVisible = true;
    QString m_currentPath;
    QStringList m_history;
    int m_historyIndex = -1;

    // UI Elements
    QToolButton* m_btnBack = nullptr;
    QToolButton* m_btnForward = nullptr;
    QToolButton* m_btnUp = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QToolButton* m_btnGo = nullptr;
    QToolButton* m_btnFavorite = nullptr;
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

    FlatFileSystemModel* m_flatModel = nullptr;
    QSortFilterProxyModel* m_flatProxyModel = nullptr;
    bool m_flatViewEnabled = false;

    ArchiveModel* m_archiveModel = nullptr;
    bool m_archiveViewActive = false;

    QSlider* m_zoomSlider = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QListView* m_listView = nullptr;
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
