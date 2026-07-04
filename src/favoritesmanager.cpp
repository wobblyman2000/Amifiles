#include "favoritesmanager.h"
#include <QSettings>
#include <QDir>

FavoritesManager& FavoritesManager::instance() {
    static FavoritesManager inst;
    return inst;
}

FavoritesManager::FavoritesManager(QObject* parent) : QObject(parent) {
    loadFavorites();
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
