#ifndef FILEDIFFDIALOG_H
#define FILEDIFFDIALOG_H

#include <QDialog>
#include <QString>

class QTextEdit;
class QLabel;

class FileDiffDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileDiffDialog(const QString& file1Path, const QString& file2Path, QWidget* parent = nullptr);
    ~FileDiffDialog() override = default;

private:
    void setupUI();
    void computeAndDisplayDiff();

    QString m_file1Path;
    QString m_file2Path;

    QLabel* m_lblFile1Header = nullptr;
    QLabel* m_lblFile2Header = nullptr;
    QTextEdit* m_txtLeft = nullptr;
    QTextEdit* m_txtRight = nullptr;
    QLabel* m_lblDiffStats = nullptr;
};

#endif // FILEDIFFDIALOG_H
