#ifndef FILEASSOCIATIONSDIALOG_H
#define FILEASSOCIATIONSDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSettings>

class FileAssociationsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileAssociationsDialog(QWidget* parent = nullptr);
    ~FileAssociationsDialog() override = default;

    static QString getCustomHandler(const QString& extension);
    static bool launchFile(const QString& filePath);

private slots:
    void onAddAssociation();
    void onDeleteSelected();
    void onSave();

private:
    void setupUI();
    void loadAssociations();

    QTableWidget* m_table = nullptr;
    QLineEdit* m_txtExt = nullptr;
    QLineEdit* m_txtCmd = nullptr;
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QPushButton* m_btnSave = nullptr;
};

#endif // FILEASSOCIATIONSDIALOG_H
