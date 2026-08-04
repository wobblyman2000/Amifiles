#ifndef FILEPANEL_H
#define FILEPANEL_H

#include <QWidget>
#include <QDialog>
#include <QRegularExpression>
#include <QStringListModel>
#include <QTimer>
#include <QThread>
#include <QJsonArray>
#include <QJsonObject>
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
#include <QRubberBand>
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
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QPainterPath>
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

#include <QRandomGenerator>

class AudioVisualizerWidget : public QWidget {
    Q_OBJECT
public:
    enum Style {
        VerticalBars = 0,
        CrtOscilloscope = 1,
        LedMatrix = 2
    };

    explicit AudioVisualizerWidget(QWidget* parent = nullptr);
    ~AudioVisualizerWidget() override;

    void setPlaying(bool playing);
    bool isPlaying() const { return m_playing; }
    
    void setStyle(Style style);
    Style style() const { return m_style; }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimate();

private:
    void updateAudioPath();
    void loadWavData(const QString& wavPath);

    QTimer* m_timer = nullptr;
    double m_heights[15];
    bool m_playing = false;
    Style m_style = VerticalBars;
    double m_phase = 0.0;

    QString m_loadedAudioPath;
    QByteArray m_audioData;
    const int16_t* m_samples = nullptr;
    int m_numSamples = 0;
    int m_sampleRate = 22050;
    int m_numChannels = 1;
};

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
    enum FilterType { FilterAll, FilterAudio, FilterVideos, FilterPictures, FilterDocs, FilterArchive, FilterThreeD, FilterFiles, FilterFolders };

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

    struct LabelStyleRule {
        QString type;
        QString pattern;
        QString color;
        QString icon;
        QString bgColor;
    };
    mutable QList<LabelStyleRule> m_labelRules;

    void loadLabelRules() const {
        m_labelRules.clear();
        QSettings settings("Amifiles", "Amifiles");
        QStringList list = settings.value("preferences/label_rules").toStringList();
        if (list.isEmpty()) {
            m_labelRules = {
                {"Extension", "pdf", "#f38ba8", "📕", ""},
                {"Extension", "zip", "#fab387", "📦", ""},
                {"Extension", "mp3", "#a6e3a1", "🎵", ""},
                {"Extension", "mp4", "#cba6f7", "🎥", ""},
                {"Tag", "starred", "#f9e2af", "⭐", ""},
                {"Tag", "important", "#eba0ac", "⚠️", ""}
            };
        } else {
            for (const QString& s : list) {
                QStringList parts = s.split(';');
                if (parts.size() >= 4) {
                    LabelStyleRule r;
                    r.type = parts[0];
                    r.pattern = parts[1];
                    r.color = parts[2];
                    r.icon = parts[3];
                    if (parts.size() >= 5) r.bgColor = parts[4];
                    m_labelRules.append(r);
                }
            }
        }
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
        m_filterTypes.clear();
        m_filterTypes.insert(type);
        invalidate();
    }

    FilterType filterType() const {
        if (m_filterTypes.contains(FilterAll) || m_filterTypes.isEmpty()) return FilterAll;
        return *m_filterTypes.begin();
    }

    void setFilterTypes(const QSet<FilterType>& types) {
        m_filterTypes = types;
        invalidate();
    }

    QSet<FilterType> filterTypes() const { return m_filterTypes; }

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidate();
    }

    QString filterText() const { return m_filterText; }

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

    void setRatingFilter(int rating) {
        m_filterRating = rating;
        invalidate();
    }

    void setCommentFilter(const QString& comment) {
        m_filterComment = comment;
        invalidate();
    }

    void setShowRecentOnly(bool enabled) {
        if (m_showRecentOnly != enabled) {
            m_showRecentOnly = enabled;
            invalidate();
        }
    }
    bool showRecentOnly() const { return m_showRecentOnly; }

    void clearAdvancedFilters() {
        m_minSize = -1;
        m_maxSize = -1;
        m_minDate = QDateTime();
        m_maxDate = QDateTime();
        m_filterTag = QString();
        m_filterRating = -1;
        m_filterComment = QString();
        m_showRecentOnly = false;
        invalidate();
    }

    void setAgeColoringEnabled(bool enabled) {
        m_ageColoringEnabled = enabled;
        emit layoutChanged();
    }

    void setGroupMultiDiscActive(bool active) {
        if (m_groupMultiDiscActive != active) {
            m_groupMultiDiscActive = active;
            invalidate();
        }
    }
    bool isGroupMultiDiscActive() const { return m_groupMultiDiscActive; }

    void setHideAuxiliaryFilesActive(bool active) {
        if (m_hideAuxiliaryFilesActive != active) {
            m_hideAuxiliaryFilesActive = active;
            invalidate();
        }
    }
    bool isHideAuxiliaryFilesActive() const { return m_hideAuxiliaryFilesActive; }

    void setShowcaseMode(int mode) {
        if (m_showcaseMode != mode) {
            m_showcaseMode = mode;
            invalidate();
        }
    }
    int showcaseMode() const { return m_showcaseMode; }

    void setHidePatterns(const QStringList& patterns) {
        m_hidePatterns = patterns;
        invalidate();
    }

    void setHiddenExtensions(const QStringList& exts) {
        m_hiddenExtensions = exts;
        invalidate();
    }
    QStringList hiddenExtensions() const { return m_hiddenExtensions; }

    static QString cleanAlbumFolderName(const QString& name) {
        static const QRegularExpression re(
            R"(\b(?:cd|disc|disk|d)\s*\d+\b|[\(\[]\s*(?:cd|disc|disk|d)\s*\d+\s*[\)\]])", 
            QRegularExpression::CaseInsensitiveOption
        );
        QString cleaned = name;
        cleaned.remove(re);
        cleaned = cleaned.trimmed();
        while (!cleaned.isEmpty() && (cleaned.endsWith('-') || cleaned.endsWith('_') || cleaned.endsWith(' '))) {
            cleaned.chop(1);
            cleaned = cleaned.trimmed();
        }
        return cleaned;
    }

    void clearCasingCache() {
        m_casingCache.clear();
        m_iconCache.clear();
        invalidate();
    }

    bool hasCasingCover(const QString& path) const {
        return m_casingCache.contains(path) && !m_casingCache.value(path).first.isEmpty();
    }

    QString getCasingArtPath(const QString& path) const {
        return m_casingCache.contains(path) ? m_casingCache.value(path).first : QString();
    }

    int getCasingType(const QString& path) const {
        return m_casingCache.contains(path) ? m_casingCache.value(path).second : 0;
    }

    bool hasCachedCasingIcon(const QString& cacheKey) const {
        return m_iconCache.contains(cacheKey);
    }

    QIcon getCachedCasingIcon(const QString& cacheKey) const {
        return m_iconCache.value(cacheKey);
    }

    // Overriding data() to support dynamic file age text coloring and icon badges
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.model() != this) {
            return QVariant();
        }
        if (m_ageRules.isEmpty()) {
            const_cast<FileFilterProxyModel*>(this)->loadAgeRules();
        }
        if (m_labelRules.isEmpty()) {
            const_cast<FileFilterProxyModel*>(this)->loadLabelRules();
        }

        if (role == Qt::DisplayRole && m_groupMultiDiscActive) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel && fileModel->isDir(srcIndex)) {
                QString name = fileModel->fileName(srcIndex);
                return cleanAlbumFolderName(name);
            }
        }

        if (role == Qt::ForegroundRole && m_ageColoringEnabled) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel) {
                QString filePath = fileModel->filePath(srcIndex);
                QFileInfo info = fileModel->fileInfo(srcIndex);

                // 1. Tag rules matching (Foreground)
                QStringList fileTags = TagManager::instance().getFileTags(filePath);
                for (const auto& rule : m_labelRules) {
                    if (rule.type == "Tag") {
                        bool tagMatches = false;
                        for (const QString& t : fileTags) {
                            if (t.compare(rule.pattern, Qt::CaseInsensitive) == 0) {
                                tagMatches = true;
                                break;
                            }
                        }
                        if (tagMatches && !rule.color.isEmpty() && rule.color != "None") {
                            return QBrush(QColor(rule.color));
                        }
                    }
                }

                // 2. Extension rules matching (Foreground)
                QString ext = info.suffix().toLower();
                for (const auto& rule : m_labelRules) {
                    if (rule.type == "Extension" && rule.pattern.toLower() == ext) {
                        if (!rule.color.isEmpty() && rule.color != "None") {
                            return QBrush(QColor(rule.color));
                        }
                    }
                }

                // Fall back to original age rules
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

        if (role == Qt::BackgroundRole && m_ageColoringEnabled) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel) {
                QString filePath = fileModel->filePath(srcIndex);
                QFileInfo info = fileModel->fileInfo(srcIndex);

                // 1. Tag BG
                QStringList fileTags = TagManager::instance().getFileTags(filePath);
                for (const auto& rule : m_labelRules) {
                    if (rule.type == "Tag") {
                        bool tagMatches = false;
                        for (const QString& t : fileTags) {
                            if (t.compare(rule.pattern, Qt::CaseInsensitive) == 0) {
                                tagMatches = true;
                                break;
                            }
                        }
                        if (tagMatches && !rule.bgColor.isEmpty() && rule.bgColor != "None" && !rule.bgColor.trimmed().isEmpty()) {
                            return QBrush(QColor(rule.bgColor));
                        }
                    }
                }

                // 2. Ext BG
                QString ext = info.suffix().toLower();
                for (const auto& rule : m_labelRules) {
                    if (rule.type == "Extension" && rule.pattern.toLower() == ext) {
                        if (!rule.bgColor.isEmpty() && rule.bgColor != "None" && !rule.bgColor.trimmed().isEmpty()) {
                            return QBrush(QColor(rule.bgColor));
                        }
                    }
                }
            }
        }

        if (role == Qt::DisplayRole && index.column() == 0 && m_ageColoringEnabled) {
            QModelIndex srcIndex = mapToSource(index);
            QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
            if (fileModel) {
                QString filePath = fileModel->filePath(srcIndex);
                QFileInfo info = fileModel->fileInfo(srcIndex);

                // 1. Tag Icon badges
                QStringList fileTags = TagManager::instance().getFileTags(filePath);
                for (const auto& rule : m_labelRules) {
                    if (rule.type == "Tag" && !rule.icon.isEmpty() && rule.icon != "None") {
                        bool tagMatches = false;
                        for (const QString& t : fileTags) {
                            if (t.compare(rule.pattern, Qt::CaseInsensitive) == 0) {
                                tagMatches = true;
                                break;
                            }
                        }
                        if (tagMatches) {
                            QString originalName = QSortFilterProxyModel::data(index, role).toString();
                            return rule.icon + " " + originalName;
                        }
                    }
                }

                // 2. Ext Icon badges
                QString ext = info.suffix().toLower();
                for (const auto& rule : m_labelRules) {
                    if (rule.type == "Extension" && rule.pattern.toLower() == ext && !rule.icon.isEmpty() && rule.icon != "None") {
                        QString originalName = QSortFilterProxyModel::data(index, role).toString();
                        return rule.icon + " " + originalName;
                    }
                }

                // Fall back to original age rules
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
            bool casingEnabled = true;
            QObject* pObj = const_cast<FileFilterProxyModel*>(this)->parent();
            while (pObj) {
                if (pObj->inherits("MainWindow")) {
                    casingEnabled = pObj->property("casingOverlaysEnabled").toBool();
                    break;
                }
                pObj = pObj->parent();
            }
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
                
                // Skip recursive size calculations for remote/network shares to prevent huge performance hits
                bool isRemote = false;
                if (path.startsWith("/run/user/") && path.contains("/gvfs/")) {
                    isRemote = true;
                } else if (path.contains("CloudMounts") || path.startsWith(QDir::homePath() + "/CloudMounts")) {
                    isRemote = true;
                } else if (path.startsWith("ftp://") || path.startsWith("sftp://") || path.startsWith("smb://")) {
                    isRemote = true;
                }
                
                if (isRemote) {
                    return "";
                }

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
        QString filePath;
        QString fileName;
        bool isDir = false;
        qint64 size = 0;
        QDateTime modDate;
        QString ext;

        QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
        FlatFileSystemModel* flatModel = qobject_cast<FlatFileSystemModel*>(sourceModel());

        if (fileModel) {
            QModelIndex index = fileModel->index(source_row, 0, source_parent);
            filePath = QDir::cleanPath(fileModel->filePath(index));
            fileName = fileModel->fileName(index);
            isDir = fileModel->isDir(index);
            size = fileModel->size(index);
            modDate = fileModel->lastModified(index);
            ext = fileModel->fileInfo(index).suffix().toLower();

            // 1. ALWAYS accept ancestors of the current path and the current path itself.
            if (m_currentPath.startsWith(filePath, Qt::CaseInsensitive)) {
                return true;
            }

            // 2. Check if this is a descendant of the current directory subtree.
            bool isDescendant = filePath.startsWith(m_currentPath + "/", Qt::CaseInsensitive);
            if (!isDescendant) {
                return true;
            }
        } else if (flatModel) {
            QFileInfo info = flatModel->fileInfo(source_row);
            filePath = QDir::cleanPath(info.absoluteFilePath());
            fileName = info.fileName();
            isDir = info.isDir();
            size = info.size();
            modDate = info.lastModified();
            ext = info.suffix().toLower();
        } else {
            return true;
        }

        if (m_hideAuxiliaryFilesActive && !isDir) {
            if (QDir::match(m_hidePatterns, fileName)) {
                return false;
            }

            if (m_hiddenExtensions.contains(ext)) {
                return false;
            }

            if (m_showcaseMode == 1 || m_showcaseMode == 5) { // Audio Showcase / Music Showcase v2
                static const QStringList imgExts = {"jpg", "jpeg", "png", "webp", "bmp", "gif", "tiff"};
                static const QStringList videoExts = {"mp4", "mkv", "avi", "mov", "webm", "mpg", "mpeg", "m4v", "flv", "wmv"};
                if (imgExts.contains(ext) || videoExts.contains(ext)) {
                    return false;
                }
            } else if (m_showcaseMode == 2 || m_showcaseMode == 3 || m_showcaseMode == 4) { // Video Showcase / Movie Showcase v2 / TV Show Showcase v2
                static const QStringList imgExts = {"jpg", "jpeg", "png", "webp", "bmp", "gif", "tiff"};
                static const QStringList audioExts = {"mp3", "flac", "wav", "aac", "m4a", "ogg", "wma", "opus"};
                static const QStringList auxExts = {"xml", "nfo", "txt", "sub", "idx", "ini", "db"};
                if (imgExts.contains(ext) || audioExts.contains(ext) || auxExts.contains(ext)) {
                    return false;
                }
            }
        }

        if (m_groupMultiDiscActive && isDir) {
            QString parentDir = QFileInfo(filePath).absolutePath();
            QDir dir(parentDir);
            QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            QString currentCleaned = cleanAlbumFolderName(fileName);
            QString firstMatch;
            for (const QString& subDirName : subDirs) {
                if (cleanAlbumFolderName(subDirName) == currentCleaned) {
                    firstMatch = subDirName;
                    break;
                }
            }
            if (!firstMatch.isEmpty() && firstMatch != fileName) {
                return false;
            }
        }

        if (!isDir) {
            if (m_minSize != -1 && size < m_minSize) {
                return false;
            }
            if (m_maxSize != -1 && size > m_maxSize) {
                return false;
            }

            if (m_minDate.isValid() && modDate < m_minDate) {
                return false;
            }
            if (m_maxDate.isValid() && modDate > m_maxDate) {
                return false;
            }
        }

        // 2.6. Apply Show Recent Only (Modified in last 24 hours) Filter
        if (m_showRecentOnly) {
            if (!modDate.isValid() || modDate.secsTo(QDateTime::currentDateTime()) > 24 * 3600) {
                return false;
            }
        }

        // 2.5. Apply Tag Filter
        if (!m_filterTag.isEmpty()) {
            if (!TagManager::instance().getFileTags(filePath).contains(m_filterTag, Qt::CaseInsensitive)) {
                return false;
            }
        }

        // Apply Rating Filter
        if (m_filterRating != -1) {
            if (TagManager::instance().getFileRating(filePath) != m_filterRating) {
                return false;
            }
        }

        // Apply Comment Filter
        if (!m_filterComment.isEmpty()) {
            QString fileComment = TagManager::instance().getFileComment(filePath);
            
            // Split filter text by space, comma, or semicolon
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QStringList keywords = m_filterComment.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
#else
            QStringList keywords = m_filterComment.split(QRegularExpression("[,;\\s]+"), QString::SkipEmptyParts);
#endif
            if (!keywords.isEmpty()) {
                bool matchAny = false;
                for (const QString& kw : keywords) {
                    if (kw.contains('*') || kw.contains('?')) {
                        QRegularExpression re(QRegularExpression::wildcardToRegularExpression(kw), QRegularExpression::CaseInsensitiveOption);
                        if (re.match(fileComment).hasMatch()) {
                            matchAny = true;
                            break;
                        }
                    } else {
                        if (fileComment.contains(kw, Qt::CaseInsensitive)) {
                            matchAny = true;
                            break;
                        }
                    }
                }
                if (!matchAny) {
                    return false;
                }
            }
        }

        // 3. Apply Text Filter (normalized case-insensitive check ignoring spaces, dots, hyphens, etc.)
        if (!m_filterText.isEmpty()) {
            QString cleanQuery;
            cleanQuery.reserve(m_filterText.size());
            for (QChar c : m_filterText) {
                if (c.isLetterOrNumber()) {
                    cleanQuery.append(c.toLower());
                }
            }
            if (!cleanQuery.isEmpty()) {
                QString cleanName;
                cleanName.reserve(fileName.size());
                for (QChar c : fileName) {
                    if (c.isLetterOrNumber()) {
                        cleanName.append(c.toLower());
                    }
                }
                if (!cleanName.contains(cleanQuery)) {
                    return false;
                }
            }
        }

        // 4. Apply Type Filter
        if (m_filterTypes.contains(FilterAll) || m_filterTypes.isEmpty()) {
            return true;
        }

        if (isDir) {
            return m_filterTypes.contains(FilterFolders);
        }
        
        if (m_filterTypes.contains(FilterFiles)) {
            return true;
        }

        if (m_filterTypes.contains(FilterAudio)) {
            static const QStringList audioExts = { "mp3", "wav", "flac", "ogg", "m4a", "wma", "aac", "mid", "midi" };
            if (audioExts.contains(ext)) return true;
        }
        if (m_filterTypes.contains(FilterVideos)) {
            static const QStringList videoExts = { "mp4", "avi", "mkv", "mov", "webm", "flv", "wmv", "m4v", "mpg", "mpeg" };
            if (videoExts.contains(ext)) return true;
        }
        if (m_filterTypes.contains(FilterPictures)) {
            static const QStringList pictureExts = { "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tiff", "ico" };
            if (pictureExts.contains(ext)) return true;
        }
        if (m_filterTypes.contains(FilterDocs)) {
            static const QStringList docExts = { "txt", "log", "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "ods", "odp", "md", "csv", "rtf", "html", "xml", "json", "nfo", "info", "diz", "conf", "config", "ini", "cfg", "me", "readme", "asc" };
            if (docExts.contains(ext)) return true;
        }
        if (m_filterTypes.contains(FilterArchive)) {
            static const QStringList archiveExts = { "zip", "tar", "gz", "bz2", "xz", "rar", "7z", "tgz", "adf", "adz", "d64", "d71", "d81", "g64", "iso", "img" };
            if (archiveExts.contains(ext)) return true;
        }
        if (m_filterTypes.contains(FilterThreeD)) {
            static const QStringList threeDExts = { "obj", "fbx", "3ds", "stl", "ply", "gltf", "glb", "dae", "blend" };
            if (threeDExts.contains(ext)) return true;
        }

        return false;
    }

private:
    QSet<FilterType> m_filterTypes = { FilterAll };
    QString m_filterText;
    QString m_currentPath;
    bool m_ageColoringEnabled = true; // Enabled by default
    bool m_groupMultiDiscActive = false;
    bool m_hideAuxiliaryFilesActive = false;
    int m_showcaseMode = 0; // 0 = Standard, 1 = Audio Showcase, 2 = Video Showcase
    QStringList m_hidePatterns;
    QStringList m_hiddenExtensions;
    qint64 m_minSize = -1;
    qint64 m_maxSize = -1;
    QDateTime m_minDate;
    QDateTime m_maxDate;
    QString m_filterTag;
    int m_filterRating = -1;
    QString m_filterComment;
    bool m_showRecentOnly = false;
    mutable QHash<QString, QPair<QString, int>> m_casingCache;
    mutable QHash<QString, QIcon> m_iconCache;
    mutable QList<AgeColorRule> m_ageRules;
    int m_zoomIconSize = 24;
    mutable QSet<QString> m_pendingCasingChecks;

private slots:
    void onCasingRendered(const QString& path, const QString& artPath, int casingType, const QImage& image, const QImage& hoverImage = QImage());
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
    void syncPlaylist(const QStringList& playlistPaths, int currentIndex);
    void setCustomBgColor(const QString& hexColor);
    QString customBgColor() const { return m_customBgColor; }
    void setCustomBgImage(const QString& imagePath);
    QString customBgImage() const { return m_customBgImage; }
    void setCustomBgOpacity(double opacity);
    double customBgOpacity() const { return m_customBgOpacity; }
    void setPath(const QString& path);
    void focusActiveView();
    void toggleWatchStatus(const QString& path);
    void scrapeVideoMetadata();
    void editAudioTags(bool autoFetch = false);
    void applyDvdCasing();
    void applyBluRayCasing();
    void playCollection();
    void createArchive(bool secure);
    void extractArchive();
    void showInfoSheet(const QString& path);
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
    QString filePathFromIndex(const QModelIndex& index) const;
    QString activeFilePath() const; // First selected file, or current directory if none
    void selectFilePath(const QString& filePath);
    QString folderArtPath() const { return m_folderArtPath; }

    void refresh();
    void writeTempFileToArchive(const QString& tempPath);
    void updateStyles();
    void autoSizeAllColumns();
    void setNavigationAndFilterVisible(bool visible);

    // Clipboard and File Operations
    void onCopy();
    void onCut();
    void onPaste();
    void onDelete();
    void onRename();
    void onNewFolder();
    void onAdvancedNewFolder();
    void onCopyFileName();
    void onCopyPath();
    void onCopyFolderContents();
    void onShowProperties();

    // Age Coloring support
    FileFilterProxyModel* proxyModel() const { return m_proxyModel; }
    class GroupProxyModel* groupProxy() const { return m_groupProxy; }
    QAbstractItemModel* activeBaseModel() const;
    void updateActiveViewModel();
    void updateTheaterGridSize();

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
    void syncFilterTypes(const QSet<FileFilterProxyModel::FilterType>& types);
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
    
    // Playlist Track list helpers
    int getTrackListCurrentIndex() const;
    int getTrackListCount() const;
    QString getTrackListPathAt(int index) const;
    QStringList getTrackListPaths() const;
    void setShuffleState(bool enabled);
    void setRepeatState(int mode);

    // Search Support
    void setSearchQuery(const QString& query);
    QString searchQuery() const;

signals:
    void playMediaBuiltinRequested(const QStringList& filePaths);
    void playMediaFullscreenRequested(const QStringList& filePaths);
    void queueMediaBuiltinRequested(const QStringList& filePaths);
    void playQueueFullscreenRequested();
    void zenModeToggled(bool enabled);
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
    void saveDefaultProfileRequested();
    void loadDefaultProfileRequested();
    void saveFolderProfileRequested();
    void configureFolderLayoutsRequested();
    void playPauseRequested();
    void volumeChangedRequested(int value);
    void mediaPlaybackSettingsChanged();
    void prevTrackRequested();
    void nextTrackRequested();
    void shuffleToggledRequested();
    void repeatClickedRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

public slots:
    void notifyPathDataChanged(const QString& path);
    void updatePlaybackProgress(qint64 position, qint64 duration);
    void onNavigateUp();
    void onNavigateBack();
    void onNavigateForward();
    void onViewModeChanged(int index);
    void onPlaybackStateChanged(int state);
    void playCurrentOrSelectedFolder();
    void queueCurrentOrSelectedFolder();
    void playPlaylistQueue();

private slots:
    void onPathEntered();
    void onFavoriteClicked();
    void onHomeClicked();
    void onHomeContextMenu(const QPoint& pos);
    void onClonePathClicked();
    void onFilterChanged(const QString& filterText);
    void onFilterTypeChanged();
    void onRecentFilterToggled(bool checked);
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex& index);
    void onDoubleClickedPath(const QString& path);
    void updateFavoritesUI();
    void updateCloneButtonIcon();
    void onRatingFilterClicked();
    void onTagFilterComboChanged(int index);
    void onCommentFilterChanged(const QString& text);
    void onCloseTagsRatingsFilterBar();
    void onToggleTagsRatingsFilterBar();
    void populateFilterTagsCombo();
    void onCustomContextMenu(const QPoint& pos);
    void showAudioShowcaseContextMenu(const QPoint& pos);
    void showMusicShowcaseContextMenu(const QPoint& pos);
    void showVideoShowcaseContextMenu(const QPoint& pos);
    void onFavoriteButtonContextMenu(const QPoint& pos);
    void onToggleViewMode();
    void onZoomChanged(int value);
    void onGroupingChanged(int index);
    void queueRebuildTheaterGroups();
    void onHeaderContextMenu(const QPoint& pos);
    void onGlobalSearchChanged(const QString& text);
    void startSearch();
    void onSearchResultsReady(const QStringList& results);
    void onSearchFinished();
    void onSearchResultSelected(const QModelIndex& index);
    void onSearchResultDoubleClicked(const QModelIndex& index);
    void onSearchContextMenu(const QPoint& pos);
    void onSearchEditContextMenu(const QPoint& pos);
    void onToggleSearchFilterMode();
    void onSearchUpdateTimeout();
    void onToggleSidePane();
    void zoomIn();
    void zoomOut();

private:
    QJsonArray getDefaultContextMenuJson() const;
    QIcon getIconForPathOrTheme(const QString& pathOrTheme);
    QAction* createContextMenuAction(QMenu* parentMenu, const QJsonObject& obj, const QStringList& selected, const QModelIndex& index, QMap<QAction*, QString>& actionCommands);
    void createNewFileTemplate(const QString& ext);
    void toggleSelectedExecutable();
    void changeSelectedPermissions();
    void removeSelectedGreenScreen();

    void updateDrawerVisibility();
    QIcon getTrackArtworkIcon(const QString& trackPath);
    QString formatDuration(qint64 ms) const;
    void setupUI();
    void updateNavigationButtons();
    void checkFolderArt();
    void rebuildTheaterGroups();
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
    QString m_customBgColor;
    QString m_customBgImage;
    double m_customBgOpacity = 1.0;
    QStringList m_history;
    int m_historyIndex = -1;

    bool m_isPinned = false;
    bool m_isPathLocked = false;
    bool m_isPathLockedWithSubdirs = false;
    QString m_lockedPath;

    // UI Elements
    QWidget* m_navContainer = nullptr;
    QToolButton* m_btnBack = nullptr;
    QToolButton* m_btnForward = nullptr;
    QToolButton* m_btnUp = nullptr;
    class PathBarWidget* m_pathBar = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QToolButton* m_btnGo = nullptr;
    QToolButton* m_btnFavorite = nullptr;
    QToolButton* m_btnHome = nullptr;
    QToolButton* m_btnClonePath = nullptr;
    QToolButton* m_btnFlatView = nullptr;
    QToolButton* m_btnToggleSidePane = nullptr;
    QTimer* m_playlistCollapseTimer = nullptr;
    int m_lastPlaylistSize = 0;
    bool m_blockCollapseTimerStop = false;
    QToolButton* m_btnViewMode = nullptr; // keep single instance
    QHeaderView* m_header = nullptr;
    // New global search components
    QLineEdit* m_globalSearchEdit = nullptr;
    QListView* m_searchResultsView = nullptr;
    QTimer* m_searchDebounceTimer = nullptr;
    QThread* m_searchThread = nullptr;
    SearchWorker* m_searchWorker = nullptr;
    QStringListModel* m_searchResultModel = nullptr;

    bool m_isSearchModeActive = false;
    QToolButton* m_btnToggleSearchMode = nullptr;
    QTimer* m_searchUpdateTimer = nullptr;
    QStringList m_bufferedSearchResults;

    QRubberBand* m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_isRubberBandActive = false;
    QWidget* m_rubberBandTargetView = nullptr;

    QTreeView* m_treeView = nullptr;
    CustomFileSystemModel* m_fileModel = nullptr;
    FileFilterProxyModel* m_proxyModel = nullptr;
    class GroupProxyModel* m_groupProxy = nullptr;
    QComboBox* m_comboGrouping = nullptr;

    FlatFileSystemModel* m_flatModel = nullptr;
    FileFilterProxyModel* m_flatProxyModel = nullptr;
    bool m_flatViewEnabled = false;

    class SmartFolderModel* m_smartModel = nullptr;
    bool m_smartViewActive = false;

    class DiskDashboardWidget* m_dashboardWidget = nullptr;
    bool m_dashboardActive = false;
    class HomeDashboardWidget* m_homeDashboardWidget = nullptr;
    bool m_homeDashboardActive = false;

    ArchiveModel* m_archiveModel = nullptr;
    bool m_archiveViewActive = false;

    QSlider* m_zoomSlider = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QListView* m_listView = nullptr;
    class MillerColumnsView* m_millerView = nullptr;
    class TimelineView* m_timelineView = nullptr;
    class FilmstripView* m_filmstripView = nullptr;
    class CoverFlowView* m_coverFlowView = nullptr;
    class TheaterListView* m_theaterListView = nullptr;
    class QComboBox* m_comboViewMode = nullptr;
    class QAbstractItemDelegate* m_defaultDelegate = nullptr;
    class CardViewDelegate* m_cardDelegate = nullptr;
    class TheaterViewDelegate* m_theaterDelegate = nullptr;
    QWidget* m_theaterContainer = nullptr;
    class QSplitter* m_theaterSplitter = nullptr;
    QWidget* m_theaterSideContainer = nullptr;
    QWidget* m_bottomInfoPanel = nullptr;
    QLabel* m_bottomTitle = nullptr;
    QLabel* m_bottomMeta = nullptr;
    QLabel* m_bottomSynopsis = nullptr;
    QPushButton* m_bottomPlayBtn = nullptr;
    QPushButton* m_bottomEnterBtn = nullptr;
    QWidget* m_musicControlsWidget = nullptr;
    QToolButton* m_btnShuffle = nullptr;
    QToolButton* m_btnPrev = nullptr;
    QToolButton* m_btnPlayPause = nullptr;
    QToolButton* m_btnNext = nullptr;
    QToolButton* m_btnRepeat = nullptr;
    QSlider* m_musicVolumeSlider = nullptr;
    QLabel* m_musicProgressLabel = nullptr;
    AudioVisualizerWidget* m_visualizerWidget = nullptr;
    class QListWidget* m_trackListWidget = nullptr;
    QWidget* m_drawerBtnContainer = nullptr;

    QWidget* m_cinemaButtonsWidget = nullptr;
    QPushButton* m_btnWatchTrailer = nullptr;
    QPushButton* m_btnEditMetadata = nullptr;

    QString m_bottomPanelPath;
    class QScrollArea* m_theaterScrollArea = nullptr;
    QWidget* m_theaterScrollWidget = nullptr;
    class QVBoxLayout* m_theaterScrollLayout = nullptr;
    QList<QPushButton*> m_theaterHeaders;
    QList<QListView*> m_theaterGrids;
    QTimer* m_rebuildGroupsTimer = nullptr;
    int m_zoomLevel = -1;
    class MetadataHoverCard* m_hoverCard = nullptr;
    QTimer* m_hoverTimer = nullptr;
    QModelIndex m_pendingHoverIndex;
    QPoint m_pendingHoverPos;

    // Bottom Filter Bar
    QLineEdit* m_filterEdit = nullptr;
    QToolButton* m_btnFilterAll = nullptr;
    QToolButton* m_btnFilterAudio = nullptr;
    QToolButton* m_btnFilterVideos = nullptr;
    QToolButton* m_btnFilterPictures = nullptr;
    QToolButton* m_btnFilterDocs = nullptr;
    QToolButton* m_btnFilterArchive = nullptr;
    QToolButton* m_btnFilterThreeD = nullptr;
    QToolButton* m_btnFilterFiles = nullptr;
    QToolButton* m_btnFilterFolders = nullptr;
    QToolButton* m_btnStickyFilters = nullptr;
    QToolButton* m_btnFilterRecent = nullptr;
    QLabel* m_statusLabel = nullptr;

    QWidget* m_categoryWidget = nullptr;
    QWidget* m_filterTextWidget = nullptr;
    QWidget* m_statusWidget = nullptr;
    
    // Tag & Rating Filter Bar
    QToolButton* m_btnToggleTRFilter = nullptr;
    QWidget* m_tagsRatingsFilterWidget = nullptr;
    QToolButton* m_btnRateAll = nullptr;
    QList<QToolButton*> m_btnStars;
    QComboBox* m_comboFilterTag = nullptr;
    class QLineEdit* m_editFilterComment = nullptr;
    FilePanel* m_siblingPanel = nullptr;

    QString m_folderArtPath;
    QString m_cachedBgPath;
    QString m_cachedBgStyle;

    class QMediaPlayer* m_themePlayer = nullptr;
    class QAudioOutput* m_themeAudio = nullptr;
    QString m_currentThemePath;

    void promptHideExtensions();
    void updateHideSettings();

public:
    void focusFirstItemInActiveView();
    void updateThemeMusic();
    void stopThemeMusic();
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
