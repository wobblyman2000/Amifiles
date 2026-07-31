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
    QString targetNavigatePath() const { return m_targetNavigatePath; }

    static void addActiveMount(const QString& name, const QString& path, const QString& type);
    static void removeActiveMount(const QString& path);
    static QList<ActiveMount> getActiveMounts();
    static bool mountIso(const QString& isoPath, QString& errorMsg, QString& mountPath);
    static bool unmountIso(const QString& isoPath, QString& errorMsg);
    static bool isIsoMounted(const QString& isoPath, QString& mountPath);
    static bool mountVhd(const QString& vhdPath, QString& errorMsg, QString& mountPath);
    static bool unmountVhd(const QString& vhdPath, QString& errorMsg);
    static bool isVhdMounted(const QString& vhdPath, QString& mountPath);

private slots:
    void onUnmountClicked();
    void onAddRemoteClicked();
    void onAddCloudClicked();
    void refreshList();

private:
    void setupUI();

    QListWidget* m_mountsList = nullptr;
    QString m_targetNavigatePath;
};
