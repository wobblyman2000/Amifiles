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
        if (it.value().colorName.isEmpty() && it.value().tags.isEmpty()) {
            continue; // Skip empty entries to clean database
        }
        QJsonObject fileInfoObj;
        fileInfoObj["color"] = it.value().colorName;
        
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

void TagManager::setFileColor(const QString& filePath, const QString& colorName) {
    m_db[filePath].colorName = colorName;
    saveDatabase();
}

QString TagManager::getFileColor(const QString& filePath) const {
    return m_db.value(filePath).colorName;
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
    return m_db.value(filePath).tags;
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
