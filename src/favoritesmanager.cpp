#include "favoritesmanager.h"
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QDateTime>
#include "tagmanager.h"

FavoritesManager& FavoritesManager::instance() {
    static FavoritesManager inst;
    return inst;
}

FavoritesManager::FavoritesManager(QObject* parent) : QObject(parent) {
    loadFavorites();
    loadDynamicRules();
}

QStringList FavoritesManager::getFavorites() const {
    return m_favorites;
}

void FavoritesManager::addFavorite(const QString& path) {
    QString cleanPath = QDir::cleanPath(path);
    if (!m_favorites.contains(cleanPath)) {
        m_favorites.append(cleanPath);
        saveFavorites();
        emit favoritesChanged();
    }
}

void FavoritesManager::removeFavorite(const QString& path) {
    QString cleanPath = QDir::cleanPath(path);
    if (m_favorites.contains(cleanPath)) {
        m_favorites.removeAll(cleanPath);
        saveFavorites();
        emit favoritesChanged();
    }
}

bool FavoritesManager::isFavorite(const QString& path) const {
    return m_favorites.contains(QDir::cleanPath(path));
}

void FavoritesManager::loadFavorites() {
    QSettings settings("Amifiles", "Amifiles");
    m_favorites = settings.value("favorites").toStringList();

    // Default favorites if empty
    if (m_favorites.isEmpty()) {
        m_favorites.append(QDir::homePath());
        saveFavorites();
    }
}

void FavoritesManager::saveFavorites() {
    QSettings settings("Amifiles", "Amifiles");
    settings.setValue("favorites", m_favorites);
}

void FavoritesManager::setDynamicRules(const QList<DynamicFavoriteRule>& rules) {
    m_dynamicRules = rules;
    saveDynamicRules();
    emit favoritesChanged();
}

static void scanDirRecursively(const QString& path, const QList<DynamicFavoriteRule>& rules, QStringList& matches, int& count) {
    if (count >= 2000) return;
    QDir dir(path);
    QStringList entryList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entryList) {
        if (entry.startsWith('.')) continue; // Skip hidden directories completely
        
        QString fullPath = dir.filePath(entry);
        count++;
        
        QFileInfo dirInfo(fullPath);
        for (const auto& r : rules) {
            bool matchesRule = false;
            if (r.ruleType == "Wildcard") {
                QRegularExpression re(QRegularExpression::wildcardToRegularExpression(r.value), QRegularExpression::CaseInsensitiveOption);
                matchesRule = re.match(fullPath).hasMatch();
            } else if (r.ruleType == "Recent") {
                int hours = r.value.toInt();
                if (hours <= 0) hours = 24;
                if (dirInfo.lastModified().secsTo(QDateTime::currentDateTime()) <= hours * 3600) {
                    matchesRule = true;
                }
            }
            if (matchesRule && !matches.contains(fullPath)) {
                matches.append(fullPath);
            }
        }
        
        scanDirRecursively(fullPath, rules, matches, count);
    }
}

QStringList FavoritesManager::getEvaluatedDynamicPaths() const {
    QStringList paths;

    // 1. Direct Tag Lookups (O(1) database queries, bypassing filesystem scans)
    for (const auto& r : m_dynamicRules) {
        if (r.ruleType == "Tag") {
            QStringList taggedFiles = TagManager::instance().getFilesWithTag(r.value);
            for (const QString& path : taggedFiles) {
                QFileInfo info(path);
                if (info.exists() && info.isDir() && !paths.contains(path)) {
                    paths.append(path);
                }
            }
        }
    }

    // 2. Scan visible home directories for Wildcard and Recent rules
    bool hasScanRules = false;
    for (const auto& r : m_dynamicRules) {
        if (r.ruleType == "Wildcard" || r.ruleType == "Recent") {
            hasScanRules = true;
            break;
        }
    }
    if (hasScanRules) {
        int count = 0;
        scanDirRecursively(QDir::homePath(), m_dynamicRules, paths, count);
    }

    return paths;
}

void FavoritesManager::loadDynamicRules() {
    m_dynamicRules.clear();
    QSettings settings("Amifiles", "Amifiles");
    QStringList list = settings.value("favorites/dynamic_rules").toStringList();
    for (const QString& s : list) {
        QStringList parts = s.split(';');
        if (parts.size() >= 3) {
            DynamicFavoriteRule r;
            r.name = parts[0];
            r.ruleType = parts[1];
            r.value = parts[2];
            m_dynamicRules.append(r);
        }
    }
}

void FavoritesManager::saveDynamicRules() {
    QSettings settings("Amifiles", "Amifiles");
    QStringList list;
    for (const auto& r : m_dynamicRules) {
        list.append(QString("%1;%2;%3").arg(r.name).arg(r.ruleType).arg(r.value));
    }
    settings.setValue("favorites/dynamic_rules", list);
}
