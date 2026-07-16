#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QList>
#include "favoritesmanager.h"

class DynamicFavoritesDialog : public QDialog {
    Q_OBJECT
public:
    explicit DynamicFavoritesDialog(QWidget* parent = nullptr);
    ~DynamicFavoritesDialog() override = default;

private slots:
    void onAddRule();
    void onDeleteRule();
    void onSave();

private:
    void setupUI();
    void populateTable();

    QTableWidget* m_table = nullptr;
    QList<DynamicFavoriteRule> m_rules;
};
