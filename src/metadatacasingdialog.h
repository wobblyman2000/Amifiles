#ifndef METADATACASINGDIALOG_H
#define METADATACASINGDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QMap>

class QTableWidget;
class QCheckBox;
class QRadioButton;
class QProgressBar;
class QLabel;

class MetadataCasingDialog : public QDialog {
    Q_OBJECT
public:
    explicit MetadataCasingDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~MetadataCasingDialog() override = default;

private slots:
    void updatePreviews();
    void onApply();

private:
    void setupUI();
    QString computeCasedString(const QString& input, const QString& mode, bool cleanUnderscores, bool cleanUrlEscapes, bool cleanSpaces);

    QStringList m_filePaths;
    
    QCheckBox* m_chkFilename = nullptr;
    QCheckBox* m_chkTitle = nullptr;
    QCheckBox* m_chkArtist = nullptr;
    QCheckBox* m_chkAlbum = nullptr;

    QRadioButton* m_radTitleCase = nullptr;
    QRadioButton* m_radSentenceCase = nullptr;
    QRadioButton* m_radUppercase = nullptr;
    QRadioButton* m_radLowercase = nullptr;

    QCheckBox* m_chkUnderscores = nullptr;
    QCheckBox* m_chkUrlEscapes = nullptr;
    QCheckBox* m_chkCleanSpaces = nullptr;

    QTableWidget* m_tablePreview = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_lblStatus = nullptr;
    QCheckBox* m_chkDryRun = nullptr;
};

#endif // METADATACASINGDIALOG_H
