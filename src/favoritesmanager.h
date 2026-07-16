#ifndef FAVORITESMANAGER_H
#define FAVORITESMANAGER_H

#include <QObject>
#include <QStringList>
#include <QList>

struct DynamicFavoriteRule {
    QString name;     // Friendly name of rule
    QString ruleType; // "Wildcard", "Tag", "Recent"
    QString value;    // Wildcard path pattern, tag label, or hours threshold
};

class FavoritesManager : public QObject {
    Q_OBJECT
public:
    static FavoritesManager& instance();

    QStringList getFavorites() const;
    void addFavorite(const QString& path);
    void removeFavorite(const QString& path);
    bool isFavorite(const QString& path) const;

    QList<DynamicFavoriteRule> getDynamicRules() const { return m_dynamicRules; }
    void setDynamicRules(const QList<DynamicFavoriteRule>& rules);
    QStringList getEvaluatedDynamicPaths() const;

signals:
    void favoritesChanged();

private:
    explicit FavoritesManager(QObject* parent = nullptr);
    ~FavoritesManager() override = default;
    FavoritesManager(const FavoritesManager&) = delete;
    FavoritesManager& operator=(const FavoritesManager&) = delete;

    void loadFavorites();
    void saveFavorites();
    void loadDynamicRules();
    void saveDynamicRules();

    QStringList m_favorites;
    QList<DynamicFavoriteRule> m_dynamicRules;
};

#endif // FAVORITESMANAGER_H
