#ifndef FILESPLITTERDIALOG_H
#define FILESPLITTERDIALOG_H

#include <QDialog>
#include <QString>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

class FileSplitterDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { Split, Join };

    explicit FileSplitterDialog(const QString& targetPath, Mode mode = Split, QWidget* parent = nullptr);

private slots:
    void onExecuteClicked();

private:
    void setupUI();
    void executeSplit();
    void executeJoin();

    QString m_targetPath;
    Mode m_mode;

    QComboBox* m_comboChunkSize = nullptr;
    QSpinBox* m_spinCustomMb = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_lblStatus = nullptr;
    QPushButton* m_btnExecute = nullptr;
};

#endif // FILESPLITTERDIALOG_H
