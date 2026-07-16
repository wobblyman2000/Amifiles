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

QStringList FavoritesManager::getEvaluatedDynamicPaths() const {
    QStringList paths;
    QString home = QDir::homePath();
    QDirIterator it(home, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext() && count < 1000) {
        QString dirPath = it.next();
        count++;

        if (dirPath.contains("/.") || dirPath.contains("\\.")) {
            continue;
        }

        QFileInfo dirInfo(dirPath);
        for (const auto& r : m_dynamicRules) {
            bool matches = false;
            if (r.ruleType == "Wildcard") {
                QRegularExpression re(QRegularExpression::wildcardToRegularExpression(r.value));
                matches = re.match(dirPath).hasMatch();
            } else if (r.ruleType == "Tag") {
                QStringList tags = TagManager::instance().getFileTags(dirPath);
                if (tags.contains(r.value)) {
                    matches = true;
                }
            } else if (r.ruleType == "Recent") {
                int hours = r.value.toInt();
                if (hours <= 0) hours = 24;
                if (dirInfo.lastModified().secsTo(QDateTime::currentDateTime()) <= hours * 3600) {
                    matches = true;
                }
            }

            if (matches && !paths.contains(dirPath)) {
                paths.append(dirPath);
            }
        }
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
