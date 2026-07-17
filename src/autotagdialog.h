#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QList>

struct AutoTagRule {
    QString id;
    QString name;
    QString pattern;   // e.g. "*.mp3" or "*.pdf"
    qint64 minSize = -1; // -1 means disabled, in MB otherwise
    QString tag;
    QString color;     // "red", "orange", "yellow", "green", "blue", "purple"
    bool active = true;
};

class AutoTagDialog : public QDialog {
    Q_OBJECT
public:
    explicit AutoTagDialog(QWidget* parent = nullptr);
    ~AutoTagDialog() override = default;

private slots:
    void onAddRule();
    void onDeleteRule();
    void onSave();

private:
    void setupUI();
    void populateTable();

    QList<AutoTagRule> m_rules;
    QTableWidget* m_table = nullptr;
};
