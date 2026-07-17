#pragma once

#include <QDialog>
#include <QList>
#include <QTableWidget>
#include <QLineEdit>

struct AutoOrganizerRule {
    QString name;
    QString sourcePath;
    QString pattern;      // Comma-separated wildcards (e.g. *.jpg, *.png)
    QString targetPath;
    bool active = true;
};

class AutoOrganizerDialog : public QDialog {
    Q_OBJECT
public:
    explicit AutoOrganizerDialog(QWidget* parent = nullptr);
    ~AutoOrganizerDialog() override = default;

    static QList<AutoOrganizerRule> loadRules();
    static void saveRules(const QList<AutoOrganizerRule>& rules);
    static void executeRules();

private slots:
    void onAddRule();
    void onDeleteRule();
    void onBrowseSource();
    void onBrowseTarget();
    void onSaveClicked();

private:
    void setupUI();
    void refreshTable();

    QTableWidget* m_rulesTable = nullptr;
    QLineEdit* m_ruleNameEdit = nullptr;
    QLineEdit* m_sourceEdit = nullptr;
    QLineEdit* m_patternEdit = nullptr;
    QLineEdit* m_targetEdit = nullptr;
    QList<AutoOrganizerRule> m_rules;
};
