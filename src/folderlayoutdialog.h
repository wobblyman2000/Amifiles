#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QList>
#include "mainwindow.h"

class FolderLayoutDialog : public QDialog {
    Q_OBJECT

public:
    FolderLayoutDialog(const QList<FolderLayoutRule>& existingRules, const QList<CustomButton>& availableButtons, QWidget* parent = nullptr);
    QList<FolderLayoutRule> rules() const { return m_rules; }

private slots:
    void onAddRule();
    void onDeleteRule();
    void onBrowsePath(int row);
    void onChooseButtons(int row);
    void onSave();

private:
    void setupUI();
    void populateTable();

    QTableWidget* m_table = nullptr;
    QList<FolderLayoutRule> m_rules;
    QList<CustomButton> m_availableButtons;
};
