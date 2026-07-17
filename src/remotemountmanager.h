#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>

struct ActiveMount {
    QString name;
    QString path;
    QString type; // e.g. "Google Drive", "SFTP", "FTP"
};

class RemoteMountManager : public QDialog {
    Q_OBJECT
public:
    explicit RemoteMountManager(QWidget* parent = nullptr);
    ~RemoteMountManager() override = default;

    static void addActiveMount(const QString& name, const QString& path, const QString& type);
    static void removeActiveMount(const QString& path);
    static QList<ActiveMount> getActiveMounts();

private slots:
    void onUnmountClicked();
    void onAddRemoteClicked();
    void onAddCloudClicked();
    void refreshList();

private:
    void setupUI();

    QListWidget* m_mountsList = nullptr;
};
