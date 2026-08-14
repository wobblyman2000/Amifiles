#ifndef METADATAINSPECTORSIDEBAR_H
#define METADATAINSPECTORSIDEBAR_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QScrollArea>

class MetadataInspectorSidebar : public QWidget {
    Q_OBJECT
public:
    explicit MetadataInspectorSidebar(QWidget* parent = nullptr);

public slots:
    void inspectFile(const QString& filePath);
    void clearInspection();

signals:
    void openFileRequested(const QString& filePath);
    void editTagsRequested(const QString& filePath);

private:
    void setupUI();

    QString m_currentFilePath;

    QLabel* m_lblThumbnail = nullptr;
    QLabel* m_lblName = nullptr;
    QLabel* m_lblPath = nullptr;
    QLabel* m_lblSize = nullptr;
    QLabel* m_lblDates = nullptr;
    QLabel* m_lblPermissions = nullptr;
    QLabel* m_lblDetailsHeader = nullptr;
    QLabel* m_lblDetailsContent = nullptr;

    QLabel* m_lblTags = nullptr;
    QPushButton* m_btnEditTags = nullptr;
    QPushButton* m_btnOpen = nullptr;
    QPushButton* m_btnTouch = nullptr;
};

#endif // METADATAINSPECTORSIDEBAR_H
