#include "filediffdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QScrollBar>

FileDiffDialog::FileDiffDialog(const QString& file1Path, const QString& file2Path, QWidget* parent)
    : QDialog(parent), m_file1Path(file1Path), m_file2Path(file2Path) {
    setupUI();
    computeAndDisplayDiff();
}

void FileDiffDialog::setupUI() {
    setWindowTitle("Side-by-Side File Comparison");
    resize(1000, 650);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-family: 'Outfit'; }"
        "QTextEdit { background-color: #181825; color: #cdd6f4; border: 1px solid #313244; font-family: monospace; font-size: 13px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    m_lblFile1Header = new QLabel(QString("<b>Left File:</b> %1").arg(QFileInfo(m_file1Path).fileName()), this);
    m_lblFile2Header = new QLabel(QString("<b>Right File:</b> %1").arg(QFileInfo(m_file2Path).fileName()), this);
    headerLayout->addWidget(m_lblFile1Header, 1);
    headerLayout->addWidget(m_lblFile2Header, 1);
    mainLayout->addLayout(headerLayout);

    QHBoxLayout* textLayout = new QHBoxLayout();
    m_txtLeft = new QTextEdit(this);
    m_txtLeft->setReadOnly(true);
    m_txtRight = new QTextEdit(this);
    m_txtRight->setReadOnly(true);

    connect(m_txtLeft->verticalScrollBar(), &QScrollBar::valueChanged, m_txtRight->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_txtRight->verticalScrollBar(), &QScrollBar::valueChanged, m_txtLeft->verticalScrollBar(), &QScrollBar::setValue);

    textLayout->addWidget(m_txtLeft, 1);
    textLayout->addWidget(m_txtRight, 1);
    mainLayout->addLayout(textLayout, 1);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_lblDiffStats = new QLabel("Analyzing differences...", this);
    m_lblDiffStats->setStyleSheet("color: #89b4fa; font-style: italic;");
    bottomLayout->addWidget(m_lblDiffStats, 1);

    QPushButton* btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet("QPushButton { background-color: #313244; color: #cdd6f4; font-weight: bold; border-radius: 4px; padding: 6px 20px; } QPushButton:hover { background-color: #45475a; }");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(btnClose);

    mainLayout->addLayout(bottomLayout);
}

void FileDiffDialog::computeAndDisplayDiff() {
    QFile f1(m_file1Path);
    QFile f2(m_file2Path);

    if (!f1.open(QIODevice::ReadOnly | QIODevice::Text) || !f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lblDiffStats->setText("Error opening files for comparison.");
        return;
    }

    QStringList lines1, lines2;
    QTextStream in1(&f1), in2(&f2);
    while (!in1.atEnd()) lines1.append(in1.readLine());
    while (!in2.atEnd()) lines2.append(in2.readLine());

    QString leftHtml, rightHtml;
    int maxLines = qMax(lines1.size(), lines2.size());
    int diffCount = 0;

    for (int i = 0; i < maxLines; ++i) {
        QString l1 = (i < lines1.size()) ? lines1[i].toHtmlEscaped() : "";
        QString l2 = (i < lines2.size()) ? lines2[i].toHtmlEscaped() : "";

        if (i < lines1.size() && i < lines2.size()) {
            if (lines1[i] == lines2[i]) {
                leftHtml += QString("<div style='white-space: pre;'><span style='color: #6c7086;'>%1  </span>%2</div>").arg(i + 1, 4).arg(l1);
                rightHtml += QString("<div style='white-space: pre;'><span style='color: #6c7086;'>%1  </span>%2</div>").arg(i + 1, 4).arg(l2);
            } else {
                diffCount++;
                leftHtml += QString("<div style='background-color: #4e2e38; white-space: pre;'><span style='color: #f38ba8;'>%1 -</span>%2</div>").arg(i + 1, 4).arg(l1);
                rightHtml += QString("<div style='background-color: #2e4638; white-space: pre;'><span style='color: #a6e3a1;'>%1 +</span>%2</div>").arg(i + 1, 4).arg(l2);
            }
        } else if (i < lines1.size()) {
            diffCount++;
            leftHtml += QString("<div style='background-color: #4e2e38; white-space: pre;'><span style='color: #f38ba8;'>%1 -</span>%2</div>").arg(i + 1, 4).arg(l1);
            rightHtml += QString("<div style='background-color: #181825; white-space: pre;'><span style='color: #6c7086;'>%1  </span></div>").arg(i + 1, 4);
        } else {
            diffCount++;
            leftHtml += QString("<div style='background-color: #181825; white-space: pre;'><span style='color: #6c7086;'>%1  </span></div>").arg(i + 1, 4);
            rightHtml += QString("<div style='background-color: #2e4638; white-space: pre;'><span style='color: #a6e3a1;'>%1 +</span>%2</div>").arg(i + 1, 4).arg(l2);
        }
    }

    m_txtLeft->setHtml(leftHtml);
    m_txtRight->setHtml(rightHtml);
    m_lblDiffStats->setText(QString("Comparison complete: %1 differing line(s) out of %2 max lines.").arg(diffCount).arg(maxLines));
}
