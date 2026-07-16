#ifndef AGESTYLINGDIALOG_H
#define AGESTYLINGDIALOG_H

#include <QDialog>
#include "filepanel.h" // For AgeColorRule struct

class QTableWidget;
class QPushButton;

class AgeStylingDialog : public QDialog {
    Q_OBJECT
public:
    explicit AgeStylingDialog(QWidget* parent = nullptr);
    ~AgeStylingDialog() override = default;

private slots:
    void onAddRule();
    void onDeleteRule();
    void onColorCellClicked(int row, int col);
    void onResetDefaults();
    void onSave();

private:
    void setupUI();
    void loadRules();
    void populateTable();

    QList<AgeColorRule> m_rules;
    QTableWidget* m_table = nullptr;
};

#endif // AGESTYLINGDIALOG_H
