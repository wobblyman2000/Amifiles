#ifndef FILEPANEL_H
#define FILEPANEL_H

#include <QWidget>
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
#include <QSettings>
#include "foldersizecalculator.h"
#include "flatmodel.h"

class ArchiveModel;

// Custom filter proxy model to support prefix/substring matching and file type categories
class FileFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
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
                    QString artPath;
                    QStringList checks = { "folder.jpg", "folder.png", "cover.jpg", "cover.png" };
                    for (const QString& check : checks) {
                        QString test = QDir(path).filePath(check);
                        if (QFile::exists(test)) {
                            artPath = test;
                            break;
                        }
                    }

                    if (!artPath.isEmpty()) {
                        QPixmap cover(artPath);
                        if (!cover.isNull()) {
                            int caseW = 48;
                            int caseH = 48;
                            QPixmap casePixmap(caseW, caseH);
                            casePixmap.fill(Qt::transparent);

                            QPainter painter(&casePixmap);
                            painter.setRenderHint(QPainter::Antialiasing);

                            painter.setBrush(QColor("#313244"));
                            painter.setPen(QPen(QColor("#45475a"), 1));
                            painter.drawRoundedRect(2, 2, caseW - 4, caseH - 4, 3, 3);

                            painter.setBrush(QColor("#11111b"));
                            painter.setPen(Qt::NoPen);
                            painter.drawRect(3, 3, 5, caseH - 6);

                            int coverX = 10;
                            int coverY = 4;
                            int coverW = caseW - 14;
                            int coverH = caseH - 8;
                            painter.drawPixmap(coverX, coverY, coverW, coverH, cover.scaled(coverW, coverH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

                            painter.setBrush(Qt::NoBrush);
                            painter.setPen(QPen(QColor(255, 255, 255, 60), 1));
                            painter.drawRoundedRect(3, 3, caseW - 6, caseH - 6, 2, 2);

                            QLinearGradient gradient(0, 0, caseW, caseH);
                            gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
                            gradient.setColorAt(0.3, QColor(255, 255, 255, 120));
                            gradient.setColorAt(0.35, QColor(255, 255, 255, 0));
                            gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

                            painter.setBrush(gradient);
                            painter.setPen(Qt::NoPen);
                            QPolygon gloss;
                            gloss << QPoint(9, 4) << QPoint(caseW - 4, 4) << QPoint(9, caseH - 4);
                            painter.drawPolygon(gloss);

                            painter.end();
                            return QIcon(casePixmap);
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
};

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(const QString& initialPath, QWidget* parent = nullptr);
    ~FilePanel() override = default;

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

    // Flat View Support
    void setFlatViewEnabled(bool enabled);
    bool isFlatViewEnabled() const { return m_flatViewEnabled; }

signals:
    void pathChanged(const QString& path);
    void fileSelected(const QString& filePath);
    void folderArtDetected(const QString& artPath);
    void panelActivated(FilePanel* panel);
    void playlistPlayRequested(const QStringList& filePaths);

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
    void zoomIn();
    void zoomOut();

private:
    void setupUI();
    void updateNavigationButtons();
    void checkFolderArt();
    void updateStatusText();
    void navigateTo(const QString& path, bool addHistory = true);
    bool copyRecursively(const QString& srcPath, const QString& destPath);

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

    QTreeView* m_treeView = nullptr;
    QFileSystemModel* m_fileModel = nullptr;
    FileFilterProxyModel* m_proxyModel = nullptr;

    FlatFileSystemModel* m_flatModel = nullptr;
    QSortFilterProxyModel* m_flatProxyModel = nullptr;
    bool m_flatViewEnabled = false;

    ArchiveModel* m_archiveModel = nullptr;
    bool m_archiveViewActive = false;

    QToolButton* m_btnViewMode = nullptr;
    QSlider* m_zoomSlider = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QListView* m_listView = nullptr;
    int m_zoomLevel = 1;

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
    FilePanel* m_siblingPanel = nullptr;

    QString m_folderArtPath;
};

#endif // FILEPANEL_H
