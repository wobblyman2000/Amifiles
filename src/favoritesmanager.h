#ifndef FAVORITESMANAGER_H
#define FAVORITESMANAGER_H

#include <QObject>
#include <QStringList>

class FavoritesManager : public QObject {
    Q_OBJECT
public:
    static FavoritesManager& instance();

    QStringList getFavorites() const;
    void addFavorite(const QString& path);
    void removeFavorite(const QString& path);
    bool isFavorite(const QString& path) const;

signals:
    void favoritesChanged();

private:
    explicit FavoritesManager(QObject* parent = nullptr);
    ~FavoritesManager() override = default;
    FavoritesManager(const FavoritesManager&) = delete;
    FavoritesManager& operator=(const FavoritesManager&) = delete;

    void loadFavorites();
    void saveFavorites();

    QStringList m_favorites;
};

#endif // FAVORITESMANAGER_H
