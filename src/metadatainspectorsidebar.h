#ifndef METADATAINSPECTORSIDEBAR_H
#define METADATAINSPECTORSIDEBAR_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
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
    void filePermissionsChanged(const QString& filePath);

private slots:
    void onApplyPermissions();
    void updateOctalDisplay();

private:
    void setupUI();

    QString m_currentFilePath;

    QLabel* m_lblThumbnail = nullptr;
    QLabel* m_lblName = nullptr;
    QLabel* m_lblPath = nullptr;
    QLabel* m_lblSize = nullptr;
    QLabel* m_lblDates = nullptr;
    QLabel* m_lblOctalPerms = nullptr;

    // Interactive POSIX Permission Matrix
    QCheckBox* m_chkOwnerR = nullptr;
    QCheckBox* m_chkOwnerW = nullptr;
    QCheckBox* m_chkOwnerX = nullptr;

    QCheckBox* m_chkGroupR = nullptr;
    QCheckBox* m_chkGroupW = nullptr;
    QCheckBox* m_chkGroupX = nullptr;

    QCheckBox* m_chkOtherR = nullptr;
    QCheckBox* m_chkOtherW = nullptr;
    QCheckBox* m_chkOtherX = nullptr;

    QPushButton* m_btnApplyPerms = nullptr;

    QLabel* m_lblDetailsHeader = nullptr;
    QLabel* m_lblDetailsContent = nullptr;

    QLabel* m_lblTags = nullptr;
    QPushButton* m_btnEditTags = nullptr;
    QPushButton* m_btnOpen = nullptr;
};

#endif // METADATAINSPECTORSIDEBAR_H
