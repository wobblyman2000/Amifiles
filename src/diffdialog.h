#ifndef DIFFDIALOG_H
#define DIFFDIALOG_H

#include <QDialog>
#include <QPlainTextEdit>
#include <QLabel>
#include <QScrollBar>

class VisualDiffDialog : public QDialog {
    Q_OBJECT
public:
    VisualDiffDialog(const QString& fileLeft, const QString& fileRight, QWidget* parent = nullptr);
    ~VisualDiffDialog() override = default;

private:
    void setupUI();
    void computeDiff(const QString& pathLeft, const QString& pathRight);
    void applyHighlighting();

    QString m_fileLeft;
    QString m_fileRight;

    QLabel* m_lblHeader = nullptr;
    QPlainTextEdit* m_editLeft = nullptr;
    QPlainTextEdit* m_editRight = nullptr;

    // Track which lines need red/green highlighting
    QList<int> m_deletedLines; // 0-indexed lines on left to highlight red
    QList<int> m_addedLines;   // 0-indexed lines on right to highlight green
};

#endif // DIFFDIALOG_H
