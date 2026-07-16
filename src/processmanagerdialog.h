#ifndef PROCESSMANAGERDIALOG_H
#define PROCESSMANAGERDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QVector>

class ProcessChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProcessChartWidget(QWidget* parent = nullptr);
    void addSample(double cpu, double mem);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> m_cpuSamples;
    QVector<double> m_memSamples;
    const int m_maxSamples = 60;
};

class ProcessManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProcessManagerDialog(QWidget* parent = nullptr);
    ~ProcessManagerDialog() override = default;

private slots:
    void refreshStats();
    void onSearchChanged(const QString& text);
    void onKillProcess();
    void onForceKillProcess();

private:
    void setupUI();
    void loadProcesses();
    double getSystemCpuUsage();
    double getSystemMemUsage();

    QTableWidget* m_table = nullptr;
    QLineEdit* m_editSearch = nullptr;
    ProcessChartWidget* m_chart = nullptr;
    QTimer* m_timer = nullptr;

    QLabel* m_lblSummary = nullptr;
    QPushButton* m_btnKill = nullptr;
    QPushButton* m_btnForceKill = nullptr;

    // CPU calculations helper variables
    unsigned long long m_lastUser = 0;
    unsigned long long m_lastUserLow = 0;
    unsigned long long m_lastSys = 0;
    unsigned long long m_lastIdle = 0;
};

#endif // PROCESSMANAGERDIALOG_H
