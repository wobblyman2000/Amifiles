#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <QDateTime>
#include <atomic>

class AdvancedSearchWorker : public QObject {
    Q_OBJECT
public:
    AdvancedSearchWorker(const QString& query, const QString& path, bool caseSensitive, int matchType,
                         bool filterSize, qint64 minSize, qint64 maxSize,
                         bool filterDate, const QDateTime& startDate, const QDateTime& endDate,
                         bool searchInside);
    ~AdvancedSearchWorker() override = default;

    void cancel();

public slots:
    void doSearch();

signals:
    void resultFound(const QString& filePath, qint64 size, const QString& type, const QDateTime& modified);
    void finished();

private:
    QString m_query;
    QString m_path;
    bool m_caseSensitive;
    int m_matchType; // 0=Substring, 1=Wildcard, 2=Regex
    bool m_filterSize;
    qint64 m_minSize;
    qint64 m_maxSize;
    bool m_filterDate;
    QDateTime m_startDate;
    QDateTime m_endDate;
    bool m_searchInside;
    std::atomic<bool> m_cancel;
};

class SearchDialog : public QDialog {
    Q_OBJECT
public:
    explicit SearchDialog(const QString& startPath, QWidget* parent = nullptr);
    ~SearchDialog() override;

private slots:
    void onBrowsePath();
    void onStartSearch();
    void onStopSearch();
    void onResultFound(const QString& filePath, qint64 size, const QString& type, const QDateTime& modified);
    void onSearchFinished();
    void onDoubleClicked(int row, int column);
    void onTypeChanged(int index);

private:
    void setupUI();

    QString m_startPath;
    
    QLineEdit* m_txtQuery = nullptr;
    QLineEdit* m_txtLocation = nullptr;
    QPushButton* m_btnBrowse = nullptr;
    
    QComboBox* m_comboType = nullptr;
    QCheckBox* m_chkCaseSensitive = nullptr;
    
    // Size Filter
    QCheckBox* m_chkSize = nullptr;
    QSpinBox* m_spinMinSize = nullptr;
    QComboBox* m_comboMinUnit = nullptr;
    QSpinBox* m_spinMaxSize = nullptr;
    QComboBox* m_comboMaxUnit = nullptr;
    
    // Date Filter
    QCheckBox* m_chkDate = nullptr;
    QDateTimeEdit* m_dateStart = nullptr;
    QDateTimeEdit* m_dateEnd = nullptr;

    // Search Inside
    QCheckBox* m_chkInside = nullptr;

    QPushButton* m_btnSearch = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnClose = nullptr;
    
    QTableWidget* m_tableResults = nullptr;
    QLabel* m_lblStatus = nullptr;

    QThread* m_thread = nullptr;
    AdvancedSearchWorker* m_worker = nullptr;
};

#endif // SEARCHDIALOG_H
