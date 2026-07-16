#ifndef CLOUDMOUNTDIALOG_H
#define CLOUDMOUNTDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QProcess>

class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;

class CloudMountDialog : public QDialog {
    Q_OBJECT
public:
    explicit CloudMountDialog(QWidget* parent = nullptr);
    ~CloudMountDialog() override = default;

private slots:
    void refreshRemotes();
    void onCreateRemote();
    void onMountRemote();
    void onUnmountRemote();

private:
    void setupUI();
    bool checkRcloneInstalled();

    QListWidget* m_listRemotes = nullptr;
    QLineEdit* m_txtName = nullptr;
    QComboBox* m_comboType = nullptr;
    QLineEdit* m_txtMountPath = nullptr;
    QLabel* m_lblStatus = nullptr;
};

#endif // CLOUDMOUNTDIALOG_H
