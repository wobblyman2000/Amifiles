#include "tagmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

TagManager& TagManager::instance() {
    static TagManager inst;
    return inst;
}

TagManager::TagManager(QObject* parent) : QObject(parent) {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    m_dbPath = configDir + "/tags.json";
    loadDatabase();
    loadAutoRules();
}

void TagManager::loadDatabase() {
    QFile file(m_dbPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return;

    QJsonObject mainObj = doc.object();
    QJsonObject filesObj = mainObj["files"].toObject();

    for (auto it = filesObj.begin(); it != filesObj.end(); ++it) {
        QJsonObject fileInfoObj = it.value().toObject();
        FileTagInfo info;
        info.colorName = fileInfoObj["color"].toString();
        info.rating = fileInfoObj["rating"].toInt();
        info.comment = fileInfoObj["comment"].toString();
        info.overlayIconName = fileInfoObj["overlayIcon"].toString();
        
        QJsonObject attrsObj = fileInfoObj["customAttributes"].toObject();
        for (auto aIt = attrsObj.begin(); aIt != attrsObj.end(); ++aIt) {
            info.customAttributes[aIt.key()] = aIt.value().toString();
        }
        
        QJsonArray tagsArr = fileInfoObj["tags"].toArray();
        for (const auto& tagVal : tagsArr) {
            info.tags.append(tagVal.toString());
        }
        m_db[it.key()] = info;
    }
}

void TagManager::saveDatabase() {
    QFile file(m_dbPath);
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonObject mainObj;
    QJsonObject filesObj;

    for (auto it = m_db.begin(); it != m_db.end(); ++it) {
        if (it.value().colorName.isEmpty() && it.value().tags.isEmpty() && it.value().rating == 0 &&
            it.value().comment.isEmpty() && it.value().customAttributes.isEmpty() && it.value().overlayIconName.isEmpty()) {
            continue; // Skip empty entries to clean database
        }
        QJsonObject fileInfoObj;
        fileInfoObj["color"] = it.value().colorName;
        fileInfoObj["rating"] = it.value().rating;
        fileInfoObj["comment"] = it.value().comment;
        fileInfoObj["overlayIcon"] = it.value().overlayIconName;
        
        QJsonObject attrsObj;
        for (auto aIt = it.value().customAttributes.begin(); aIt != it.value().customAttributes.end(); ++aIt) {
            attrsObj[aIt.key()] = aIt.value();
        }
        fileInfoObj["customAttributes"] = attrsObj;
        
        QJsonArray tagsArr;
        for (const QString& tag : it.value().tags) {
            tagsArr.append(tag);
        }
        fileInfoObj["tags"] = tagsArr;
        filesObj[it.key()] = fileInfoObj;
    }

    mainObj["files"] = filesObj;
    QJsonDocument doc(mainObj);
    file.write(doc.toJson());
}

void TagManager::setFileOverlayIcon(const QString& filePath, const QString& iconName) {
    m_db[filePath].overlayIconName = iconName;
    saveDatabase();
}

QString TagManager::getFileOverlayIcon(const QString& filePath) const {
    return m_db.value(filePath).overlayIconName;
}

void TagManager::setFileColor(const QString& filePath, const QString& colorName) {
    m_db[filePath].colorName = colorName;
    saveDatabase();
}

QString TagManager::getFileColor(const QString& filePath) const {
    // 1. Static User Override
    QString staticColor = m_db.value(filePath).colorName;
    if (!staticColor.isEmpty() && staticColor != "none") {
        return staticColor;
    }

    // 2. Evaluate Dynamic Auto-Rules
    QFileInfo info(filePath);
    if (info.exists() && info.isFile()) {
        qint64 sizeMB = info.size() / (1024 * 1024);
        for (const auto& rule : m_autoRules) {
            if (!rule.active) continue;

            bool matchesPattern = false;
            if (!rule.pattern.isEmpty()) {
                QRegularExpression re(QRegularExpression::wildcardToRegularExpression(rule.pattern), QRegularExpression::CaseInsensitiveOption);
                matchesPattern = re.match(info.fileName()).hasMatch();
            }

            bool matchesSize = (rule.minSize != -1 && sizeMB >= rule.minSize);

            bool match = false;
            if (!rule.pattern.isEmpty() && rule.minSize != -1) {
                match = matchesPattern && matchesSize;
            } else if (!rule.pattern.isEmpty()) {
                match = matchesPattern;
            } else if (rule.minSize != -1) {
                match = matchesSize;
            }

            if (match && !rule.color.isEmpty() && rule.color != "none") {
                return rule.color;
            }
        }
    }

    return "none";
}

QColor TagManager::getColorValue(const QString& colorName) const {
    if (colorName == "red") return QColor("#f38ba8"); // Catppuccin red
    if (colorName == "orange") return QColor("#fab387"); // Catppuccin peach
    if (colorName == "yellow") return QColor("#f9e2af"); // Catppuccin yellow
    if (colorName == "green") return QColor("#a6e3a1"); // Catppuccin green
    if (colorName == "blue") return QColor("#89b4fa"); // Catppuccin blue
    if (colorName == "purple") return QColor("#cba6f7"); // Catppuccin mauve
    return QColor();
}

void TagManager::setFileTags(const QString& filePath, const QStringList& tags) {
    m_db[filePath].tags = tags;
    saveDatabase();
}

QStringList TagManager::getFileTags(const QString& filePath) const {
    QStringList tags = m_db.value(filePath).tags;

    // Evaluate Auto-rules to append tags dynamically
    QFileInfo info(filePath);
    if (info.exists() && info.isFile()) {
        qint64 sizeMB = info.size() / (1024 * 1024);
        for (const auto& rule : m_autoRules) {
            if (!rule.active) continue;

            bool matchesPattern = false;
            if (!rule.pattern.isEmpty()) {
                QRegularExpression re(QRegularExpression::wildcardToRegularExpression(rule.pattern), QRegularExpression::CaseInsensitiveOption);
                matchesPattern = re.match(info.fileName()).hasMatch();
            }

            bool matchesSize = (rule.minSize != -1 && sizeMB >= rule.minSize);

            bool match = false;
            if (!rule.pattern.isEmpty() && rule.minSize != -1) {
                match = matchesPattern && matchesSize;
            } else if (!rule.pattern.isEmpty()) {
                match = matchesPattern;
            } else if (rule.minSize != -1) {
                match = matchesSize;
            }

            if (match && !rule.tag.isEmpty()) {
                if (!tags.contains(rule.tag)) {
                    tags.append(rule.tag);
                }
            }
        }
    }

    return tags;
}

void TagManager::setFileRating(const QString& filePath, int rating) {
    m_db[filePath].rating = qBound(0, rating, 5);
    saveDatabase();
}

int TagManager::getFileRating(const QString& filePath) const {
    return m_db.value(filePath).rating;
}

void TagManager::setFileComment(const QString& filePath, const QString& comment) {
    m_db[filePath].comment = comment;
    saveDatabase();
}

QString TagManager::getFileComment(const QString& filePath) const {
    return m_db.value(filePath).comment;
}

void TagManager::setCustomAttribute(const QString& filePath, const QString& key, const QString& val) {
    if (val.isEmpty()) {
        m_db[filePath].customAttributes.remove(key);
    } else {
        m_db[filePath].customAttributes[key] = val;
    }
    saveDatabase();
}

QString TagManager::getCustomAttribute(const QString& filePath, const QString& key) const {
    return m_db.value(filePath).customAttributes.value(key);
}

QStringList TagManager::getAllTags() const {
    QStringList all;
    for (const auto& info : m_db) {
        for (const QString& tag : info.tags) {
            if (!all.contains(tag)) {
                all.append(tag);
            }
        }
    }
    all.sort(Qt::CaseInsensitive);
    return all;
}

QStringList TagManager::getFilesWithTag(const QString& tag) const {
    QStringList files;
    for (auto it = m_db.begin(); it != m_db.end(); ++it) {
        if (it.value().tags.contains(tag)) {
            files.append(it.key());
        }
    }
    return files;
}

void TagManager::loadAutoRules() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile file(configDir + "/autorules.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return;

    m_autoRules.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        AutoTagRule r;
        r.id = obj["id"].toString();
        r.name = obj["name"].toString();
        r.pattern = obj["pattern"].toString();
        r.minSize = obj["minSize"].toVariant().toLongLong();
        r.tag = obj["tag"].toString();
        r.color = obj["color"].toString();
        r.active = obj["active"].toBool();
        m_autoRules.append(r);
    }
}

void TagManager::saveAutoRules() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile file(configDir + "/autorules.json");
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonArray arr;
    for (const auto& r : m_autoRules) {
        QJsonObject obj;
        obj["id"] = r.id;
        obj["name"] = r.name;
        obj["pattern"] = r.pattern;
        obj["minSize"] = r.minSize;
        obj["tag"] = r.tag;
        obj["color"] = r.color;
        obj["active"] = r.active;
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    file.write(doc.toJson());
}

QList<AutoTagRule> TagManager::getAutoTagRules() const {
    return m_autoRules;
}

void TagManager::setAutoTagRules(const QList<AutoTagRule>& rules) {
    m_autoRules = rules;
    saveAutoRules();
}
