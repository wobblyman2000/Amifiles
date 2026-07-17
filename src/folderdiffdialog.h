#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTreeWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

class FolderDiffDialog : public QDialog {
    Q_OBJECT
public:
    explicit FolderDiffDialog(const QString& leftPath, const QString& rightPath, QWidget* parent = nullptr);
    ~FolderDiffDialog() override = default;

private slots:
    void onCompareClicked();
    void onMirrorLeftToRight();
    void onMirrorRightToLeft();
    void onTwoWaySync();

private:
    struct SyncItem {
        QString relativePath;
        QString leftFullPath;
        QString rightFullPath;
        QString status; // "Identical", "Left Newer", "Right Newer", "Left Only", "Right Only"
        qint64 size = 0;
    };

    void scanDirectories(const QString& left, const QString& right);
    void syncFiles(const QList<SyncItem>& items, bool leftToRight, bool rightToLeft);

    QLineEdit* m_leftEdit = nullptr;
    QLineEdit* m_rightEdit = nullptr;
    QTreeWidget* m_treeWidget = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    
    QPushButton* m_btnCompare = nullptr;
    QPushButton* m_btnSyncL2R = nullptr;
    QPushButton* m_btnSyncR2L = nullptr;
    QPushButton* m_btnSyncTwoWay = nullptr;

    QList<SyncItem> m_scanResults;
};
