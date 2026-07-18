#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include "autotagdialog.h"
#include <QObject>
#include <QStringList>
#include <QMap>
#include <QColor>

struct FileTagInfo {
    QString colorName; // "red", "orange", "yellow", "green", "blue", "purple" or empty
    QStringList tags;
    int rating = 0; // 0 to 5
    QString comment;
    QMap<QString, QString> customAttributes;
    QString overlayIconName; // Chosen custom overlay icon from IconPickerDialog
};

class TagManager : public QObject {
    Q_OBJECT
public:
    static TagManager& instance();

    void setFileColor(const QString& filePath, const QString& colorName);
    QString getFileColor(const QString& filePath) const;
    QColor getColorValue(const QString& colorName) const;

    void setFileOverlayIcon(const QString& filePath, const QString& iconName);
    QString getFileOverlayIcon(const QString& filePath) const;

    void setFileTags(const QString& filePath, const QStringList& tags);
    QStringList getFileTags(const QString& filePath) const;

    void setFileRating(const QString& filePath, int rating);
    int getFileRating(const QString& filePath) const;
    void setFileComment(const QString& filePath, const QString& comment);
    QString getFileComment(const QString& filePath) const;

    void setCustomAttribute(const QString& filePath, const QString& key, const QString& val);
    QString getCustomAttribute(const QString& filePath, const QString& key) const;

    QStringList getAllTags() const;
    QStringList getFilesWithTag(const QString& tag) const;

    // Auto-Tagging Rules
    QList<struct AutoTagRule> getAutoTagRules() const;
    void setAutoTagRules(const QList<struct AutoTagRule>& rules);

private:
    explicit TagManager(QObject* parent = nullptr);
    ~TagManager() override = default;

    void loadDatabase();
    void saveDatabase();
    void loadAutoRules();
    void saveAutoRules();

    QString m_dbPath;
    QMap<QString, FileTagInfo> m_db;
    QList<struct AutoTagRule> m_autoRules;
};

#endif // TAGMANAGER_H
