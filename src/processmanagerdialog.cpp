#include "processmanagerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

ProcessChartWidget::ProcessChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(100);
}

void ProcessChartWidget::addSample(double cpu, double mem) {
    m_cpuSamples.append(cpu);
    if (m_cpuSamples.size() > m_maxSamples) m_cpuSamples.removeFirst();

    m_memSamples.append(mem);
    if (m_memSamples.size() > m_maxSamples) m_memSamples.removeFirst();

    update();
}

void ProcessChartWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dark grid background
    painter.fillRect(rect(), QColor("#181825"));

    // Draw Grid Lines
    painter.setPen(QPen(QColor("#313244"), 1, Qt::DashLine));
    for (int i = 1; i < 4; ++i) {
        int y = (height() * i) / 4;
        painter.drawLine(0, y, width(), y);
    }

    if (m_cpuSamples.isEmpty()) return;

    double wStep = static_cast<double>(width()) / (m_maxSamples - 1);
    
    // Helper to draw a line chart path
    auto drawLinePath = [&](const QVector<double>& samples, const QColor& color, const QColor& fillColor) {
        QPainterPath path;
        QPainterPath fillPath;
        
        double firstY = height() - (samples[0] / 100.0) * height();
        path.moveTo(0, firstY);
        fillPath.moveTo(0, height());
        fillPath.lineTo(0, firstY);

        for (int i = 1; i < samples.size(); ++i) {
            double x = i * wStep;
            double y = height() - (samples[i] / 100.0) * height();
            path.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
        
        fillPath.lineTo((samples.size() - 1) * wStep, height());
        fillPath.closeSubpath();

        // Fill area
        painter.fillPath(fillPath, fillColor);

        // Draw Line
        painter.setPen(QPen(color, 2));
        painter.drawPath(path);
    };

    // Draw RAM (Blue)
    drawLinePath(m_memSamples, QColor("#89b4fa"), QColor(137, 180, 250, 40));
    // Draw CPU (Red/Peach)
    drawLinePath(m_cpuSamples, QColor("#f38ba8"), QColor(243, 139, 168, 40));
}

ProcessManagerDialog::ProcessManagerDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    refreshStats();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessManagerDialog::refreshStats);
    m_timer->start(1500); // refresh every 1.5 seconds
}

void ProcessManagerDialog::setupUI() {
    setWindowTitle("System Monitor & Process Manager");
    resize(640, 520);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
        "QTableWidget { background-color: #181825; color: #cdd6f4; gridline-color: #313244; border: 1px solid #313244; border-radius: 4px; }"
        "QHeaderView::section { background-color: #313244; color: #cdd6f4; padding: 5px; border: 1px solid #1e1e2e; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Chart Header
    QHBoxLayout* chartHeader = new QHBoxLayout();
    chartHeader->addWidget(new QLabel("<b>Live Performance Monitor</b> (Red: CPU, Blue: RAM)", this));
    mainLayout->addLayout(chartHeader);

    // Chart widget
    m_chart = new ProcessChartWidget(this);
    mainLayout->addWidget(m_chart);

    // Search and Summary
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_editSearch = new QLineEdit(this);
    m_editSearch->setPlaceholderText("Filter processes by name or PID...");
    connect(m_editSearch, &QLineEdit::textChanged, this, &ProcessManagerDialog::onSearchChanged);
    searchLayout->addWidget(m_editSearch, 1);

    m_lblSummary = new QLabel("System Statistics: CPU: 0% | Memory: 0%", this);
    m_lblSummary->setStyleSheet("font-weight: bold;");
    searchLayout->addWidget(m_lblSummary);
    mainLayout->addLayout(searchLayout);

    // Processes Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"PID", "User", "% CPU", "% MEM", "Process Name"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_table);

    // Buttons
    QHBoxLayout* bottom = new QHBoxLayout();
    m_btnKill = new QPushButton("Kill Process", this);
    m_btnKill->setStyleSheet(
        "QPushButton { background-color: #fab387; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #b4befe; }"
    );
    connect(m_btnKill, &QPushButton::clicked, this, &ProcessManagerDialog::onKillProcess);
    bottom->addWidget(m_btnKill);

    m_btnForceKill = new QPushButton("Force Terminate (Shred)", this);
    m_btnForceKill->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #11111b; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #e64553; }"
    );
    connect(m_btnForceKill, &QPushButton::clicked, this, &ProcessManagerDialog::onForceKillProcess);
    bottom->addWidget(m_btnForceKill);

    bottom->addStretch();
    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet(
        "QPushButton { background-color: #313244; color: #cdd6f4; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45475a; }"
    );
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(btnClose);

    mainLayout->addLayout(bottom);
}

double ProcessManagerDialog::getSystemCpuUsage() {
    QFile file("/proc/stat");
    if (!file.open(QIODevice::ReadOnly)) return 0.0;

    QTextStream in(&file);
    QString line = in.readLine();
    file.close();

    QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (tokens.size() < 5) return 0.0;

    unsigned long long user = tokens[1].toULongLong();
    unsigned long long userLow = tokens[2].toULongLong();
    unsigned long long sys = tokens[3].toULongLong();
    unsigned long long idle = tokens[4].toULongLong();

    unsigned long long active = user + userLow + sys;
    unsigned long long total = active + idle;

    unsigned long long deltaActive = active - m_lastUser - m_lastUserLow - m_lastSys;
    unsigned long long deltaTotal = total - m_lastUser - m_lastUserLow - m_lastSys - m_lastIdle;

    m_lastUser = user;
    m_lastUserLow = userLow;
    m_lastSys = sys;
    m_lastIdle = idle;

    if (deltaTotal == 0) return 0.0;
    return (static_cast<double>(deltaActive) / deltaTotal) * 100.0;
}

double ProcessManagerDialog::getSystemMemUsage() {
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly)) return 0.0;

    QTextStream in(&file);
    qint64 total = 0;
    qint64 available = 0;

    QString line;
    while (in.readLineInto(&line)) {
        if (line.startsWith("MemTotal:")) {
            total = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)[1].toLongLong();
        } else if (line.startsWith("MemAvailable:")) {
            available = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)[1].toLongLong();
        }
    }
    file.close();

    if (total == 0) return 0.0;
    return (static_cast<double>(total - available) / total) * 100.0;
}

void ProcessManagerDialog::refreshStats() {
    double cpu = getSystemCpuUsage();
    double mem = getSystemMemUsage();

    m_chart->addSample(cpu, mem);
    m_lblSummary->setText(QString("System Statistics: CPU: %1% | Memory: %2%").arg(qRound(cpu)).arg(qRound(mem)));

    loadProcesses();
}

void ProcessManagerDialog::loadProcesses() {
    QProcess proc;
    proc.start("ps", {"-eo", "pid,user,%cpu,%mem,comm"});
    if (!proc.waitForFinished()) return;

    QString output = proc.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) return;

    // Filter selection matching state
    int selectedPid = -1;
    int curRow = m_table->currentRow();
    if (curRow >= 0) {
        selectedPid = m_table->item(curRow, 0)->text().toInt();
    }

    m_table->setRowCount(0);
    QString filterText = m_editSearch->text().trimmed().toLower();

    int rowIdx = 0;
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QStringList cols = lines[i].split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (cols.size() < 5) continue;

        QString pid = cols[0];
        QString user = cols[1];
        QString cpu = cols[2];
        QString mem = cols[3];
        
        // Reassemble process name if it has spaces
        QString name;
        for (int c = 4; c < cols.size(); ++c) {
            name += cols[c] + " ";
        }
        name = name.trimmed();

        // Apply filters
        if (!filterText.isEmpty()) {
            if (!name.toLower().contains(filterText) && !pid.contains(filterText)) {
                continue;
            }
        }

        m_table->insertRow(rowIdx);
        m_table->setItem(rowIdx, 0, new QTableWidgetItem(pid));
        m_table->setItem(rowIdx, 1, new QTableWidgetItem(user));
        m_table->setItem(rowIdx, 2, new QTableWidgetItem(cpu));
        m_table->setItem(rowIdx, 3, new QTableWidgetItem(mem));
        m_table->setItem(rowIdx, 4, new QTableWidgetItem(name));

        if (pid.toInt() == selectedPid) {
            m_table->selectRow(rowIdx);
        }
        rowIdx++;
    }
}

void ProcessManagerDialog::onSearchChanged(const QString&) {
    loadProcesses();
}

void ProcessManagerDialog::onKillProcess() {
    int curRow = m_table->currentRow();
    if (curRow < 0) return;

    QString pid = m_table->item(curRow, 0)->text();
    QString name = m_table->item(curRow, 4)->text();

    if (QMessageBox::question(this, "Kill Process", QString("Are you sure you want to terminate process %1 (PID: %2)?").arg(name).arg(pid)) == QMessageBox::Yes) {
        QProcess::startDetached("kill", {pid});
        loadProcesses();
    }
}

void ProcessManagerDialog::onForceKillProcess() {
    int curRow = m_table->currentRow();
    if (curRow < 0) return;

    QString pid = m_table->item(curRow, 0)->text();
    QString name = m_table->item(curRow, 4)->text();

    if (QMessageBox::warning(this, "Force Kill Process", QString("Are you absolutely sure you want to forcefully terminate process %1 (PID: %2)? This could result in loss of unsaved data.").arg(name).arg(pid), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        QProcess::startDetached("kill", {"-9", pid});
        loadProcesses();
    }
}
