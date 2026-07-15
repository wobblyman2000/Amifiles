#ifndef BULKRENAME_H
#define BULKRENAME_H

#include <QDialog>
#include <QStringList>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QHash>
#include "metadataextractor.h"

class BulkRenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit BulkRenameDialog(const QStringList& filePaths, QWidget* parent = nullptr);
    ~BulkRenameDialog() override = default;

private slots:
    void onUpdatePreview();
    void onApplyRename();
    void onSavePreset();
    void onLoadPreset();
    void onDeletePreset();

private:
    void setupUI();
    void loadPresetNames();
    QString computeNewName(const QString& filePath, int index) const;
    QString toTitleCase(const QString& str) const;
    QString toSentenceCase(const QString& str) const;

    QStringList m_filePaths;
    QHash<QString, FileMetadata> m_metadataCache;

    // UI Configuration Controls
    QCheckBox* m_chkUsePattern = nullptr;
    QLineEdit* m_txtPattern = nullptr;
    QComboBox* m_comboCase = nullptr;
    QLineEdit* m_txtFind = nullptr;
    QLineEdit* m_txtReplace = nullptr;
    QCheckBox* m_chkRegex = nullptr;
    QCheckBox* m_chkCaseSensitive = nullptr;
    QCheckBox* m_chkApplyToExt = nullptr;

    // Numbering controls
    QCheckBox* m_chkNumbering = nullptr;
    QComboBox* m_comboNumPos = nullptr;
    QSpinBox* m_spinNumStart = nullptr;
    QSpinBox* m_spinNumStep = nullptr;
    QSpinBox* m_spinNumPadding = nullptr;

    // Preset Controls
    QComboBox* m_comboPresets = nullptr;
    QPushButton* m_btnSavePreset = nullptr;
    QPushButton* m_btnDeletePreset = nullptr;

    // Preview list
    QTableWidget* m_tablePreview = nullptr;
};

#endif // BULKRENAME_H
