#ifndef VAULTDIALOG_H
#define VAULTDIALOG_H

#include <QDialog>
#include <QThread>
#include <QLineEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

class VaultWorker : public QThread {
    Q_OBJECT
public:
    explicit VaultWorker(bool encrypt, const QString& sourcePath, const QString& password, QObject* parent = nullptr);
    ~VaultWorker() override = default;

signals:
    void progress(int percentage, const QString& statusText);
    void finished(bool success, const QString& resultMsg);

protected:
    void run() override;

private:
    bool runProcess(const QString& cmd, const QStringList& args, QByteArray* errOutput = nullptr);
    bool shredFile(const QString& filePath);

    bool m_encrypt;
    QString m_sourcePath;
    QString m_password;
};

class VaultDialog : public QDialog {
    Q_OBJECT
public:
    explicit VaultDialog(bool encrypt, const QString& sourcePath = QString(), QWidget* parent = nullptr);
    ~VaultDialog() override;

    QString password() const;
    QString decryptedPath() const;
    QString vaultPath() const;

private slots:
    void onBrowse();
    void onAction();
    void onWorkerProgress(int percentage, const QString& statusText);
    void onWorkerFinished(bool success, const QString& resultMsg);

private:
    void setupUI();

    bool m_encrypt;
    QString m_sourcePath;
    VaultWorker* m_worker = nullptr;

    QLabel* m_lblFile = nullptr;
    QPushButton* m_btnBrowse = nullptr;
    QLineEdit* m_editPassword = nullptr;
    QLineEdit* m_editConfirmPassword = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_lblStatus = nullptr;

    QPushButton* m_btnAction = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // VAULTDIALOG_H
