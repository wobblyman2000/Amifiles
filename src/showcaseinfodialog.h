#pragma once

#include <QDialog>
#include <QString>
#include <QLabel>
#include <QPushButton>

class ShowcaseInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShowcaseInfoDialog(const QString& itemPath, QWidget* parent = nullptr);
    ~ShowcaseInfoDialog() override = default;

signals:
    void playRequested(const QString& path);
    void openFolderRequested(const QString& path);
    void watchStatusChanged(const QString& path, bool isWatched);

private:
    void setupUI();
    void loadMetadata();

    QString m_path;
    QLabel* m_lblPoster = nullptr;
    QLabel* m_lblTitle = nullptr;
    QLabel* m_lblSpecs = nullptr;
    class QTextBrowser* m_lblSynopsis = nullptr;
    QPushButton* m_btnToggleWatch = nullptr;
    QPushButton* m_btnPlay = nullptr;
    QPushButton* m_btnOpenFolder = nullptr;
    bool m_isWatched = false;
};
