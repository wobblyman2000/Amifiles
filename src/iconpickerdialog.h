#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QMap>
#include <QSet>
#include <QLabel>

class IconPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit IconPickerDialog(QWidget* parent = nullptr);
    ~IconPickerDialog() override = default;

    QString selectedIconName() const;

private slots:
    void onSearchChanged(const QString& text);
    void onCategoryChanged();
    void onIconDoubleClicked(QListWidgetItem* item);
    void onIconSelectionChanged();

private:
    void setupUI();
    void loadIcons();
    void filterIcons();

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_categoryList = nullptr;
    QListWidget* m_iconGrid = nullptr;
    QLabel* m_lblStatus = nullptr;
    QLabel* m_lblCount = nullptr;

    QString m_selectedIcon;

    // Static cache to avoid scanning system folders multiple times
    static QMap<QString, QSet<QString>> s_iconCache;
    static bool s_cacheLoaded;
};
