#ifndef BATCHTOUCHDIALOG_H
#define BATCHTOUCHDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

class BatchTouchDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchTouchDialog(const QStringList& filePaths, QWidget* parent = nullptr);

private slots:
    void onApplyClicked();

private:
    void setupUI();

    QStringList m_filePaths;

    QCheckBox* m_chkModified = nullptr;
    QDateTimeEdit* m_dtModified = nullptr;

    QCheckBox* m_chkAccessed = nullptr;
    QDateTimeEdit* m_dtAccessed = nullptr;

    QCheckBox* m_chkRecursive = nullptr;
    QComboBox* m_comboPreset = nullptr;

    QLabel* m_lblStatus = nullptr;
};

#endif // BATCHTOUCHDIALOG_H
