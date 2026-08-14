#ifndef CHECKSUMDIALOG_H
#define CHECKSUMDIALOG_H

#include <QDialog>
#include <QThread>
#include <QLineEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

class HashWorker : public QThread {
    Q_OBJECT
public:
    explicit HashWorker(const QString& filePath, QObject* parent = nullptr);
    ~HashWorker() override = default;

signals:
    void progress(int percentage);
    void finished(const QString& md5, const QString& sha1, const QString& sha256);
    void errorOccurred(const QString& errorMsg);

protected:
    void run() override;

private:
    QString m_filePath;
};

class ChecksumDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChecksumDialog(const QString& filePath = QString(), QWidget* parent = nullptr);
    ~ChecksumDialog() override;

private slots:
    void onSelectFile();
    void onHashProgress(int percentage);
    void onHashFinished(const QString& md5, const QString& sha1, const QString& sha256);
    void onHashError(const QString& errorMsg);
    void onVerifyTextChanged(const QString& text);
    void onCopyMd5();
    void onCopySha1();
    void onCopySha256();
    void onBrowseGenFolder();
    void onBrowseGenOutput();
    void onGenerateManifest();
    void onBrowseVerifyManifest();
    void onBrowseVerifyFolder();
    void onVerifyManifest();

private:
    void setupUI();
    void startHashing(const QString& path);

    QString m_filePath;
    HashWorker* m_worker = nullptr;

    class QTabWidget* m_tabWidget = nullptr;

    QLabel* m_lblFile = nullptr;
    QPushButton* m_btnSelectFile = nullptr;
    QProgressBar* m_progress = nullptr;

    QLineEdit* m_editMd5 = nullptr;
    QLineEdit* m_editSha1 = nullptr;
    QLineEdit* m_editSha256 = nullptr;
    QLineEdit* m_editVerify = nullptr;

    QPushButton* m_btnCopyMd5 = nullptr;
    QPushButton* m_btnCopySha1 = nullptr;
    QPushButton* m_btnCopySha256 = nullptr;

    QString m_hashMd5;
    QString m_hashSha1;
    QString m_hashSha256;

    // Manifest Generator UI
    QLineEdit* m_editGenFolder = nullptr;
    QLineEdit* m_editGenOutput = nullptr;
    class QComboBox* m_comboGenFormat = nullptr;

    // Manifest Verifier UI
    QLineEdit* m_editVerifyManifest = nullptr;
    QLineEdit* m_editVerifyFolder = nullptr;
    class QTreeWidget* m_treeVerify = nullptr;
    QLabel* m_lblVerifyStats = nullptr;
};

#endif // CHECKSUMDIALOG_H
