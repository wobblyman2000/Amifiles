#ifndef KEYBINDINGSEDITOR_H
#define KEYBINDINGSEDITOR_H

#include <QDialog>
#include <QTableWidget>
#include <QKeySequence>
#include <QMap>

class KeybindingsEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeybindingsEditorDialog(const QMap<QString, QKeySequence>& currentBindings, QWidget* parent = nullptr);
    ~KeybindingsEditorDialog() override = default;

    QMap<QString, QKeySequence> getNewBindings() const { return m_bindings; }

private slots:
    void onCellDoubleClicked(int row, int column);
    void onResetAll();
    void onAccept();

private:
    void setupUI();
    void populateTable();

    QTableWidget* m_table = nullptr;
    QMap<QString, QKeySequence> m_bindings;
    QMap<QString, QKeySequence> m_defaults;
    QMap<QString, QString> m_actionLabels;
};

#endif // KEYBINDINGSEDITOR_H
