#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>

class CommandPalette : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette() override = default;

signals:
    void commandTriggered(const QString& command);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void populateItems();

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_listWidget = nullptr;
    struct PaletteItem {
        QString text;
        QString action;
    };
    QList<PaletteItem> m_allItems;
};
