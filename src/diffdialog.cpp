#include "diffdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QTextBlock>
#include <QTextEdit>

VisualDiffDialog::VisualDiffDialog(const QString& fileLeft, const QString& fileRight, QWidget* parent)
    : QDialog(parent), m_fileLeft(fileLeft), m_fileRight(fileRight) {
    setWindowTitle("Visual File Comparison");
    resize(900, 600);
    setStyleSheet("QDialog { background-color: #1e1e2e; color: #cdd6f4; }"
                  "QLabel { color: #cdd6f4; }"
                  "QPlainTextEdit { background-color: #181825; border: 1px solid #313244; color: #cdd6f4; font-family: monospace; font-size: 12px; border-radius: 4px; }"
                  "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
                  "QPushButton:hover { background-color: #45475a; }");

    setupUI();
    computeDiff(m_fileLeft, m_fileRight);
}

void VisualDiffDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Header displaying file names
    m_lblHeader = new QLabel(this);
    m_lblHeader->setStyleSheet("font-size: 13px; font-weight: bold; color: #89b4fa;");
    m_lblHeader->setText(QString("Comparing:\nLeft:  %1\nRight: %2")
                         .arg(m_fileLeft).arg(m_fileRight));
    mainLayout->addWidget(m_lblHeader);

    // Edit columns layout
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(8);

    m_editLeft = new QPlainTextEdit(this);
    m_editLeft->setReadOnly(true);
    m_editLeft->setLineWrapMode(QPlainTextEdit::NoWrap);
    columnsLayout->addWidget(m_editLeft, 1);

    m_editRight = new QPlainTextEdit(this);
    m_editRight->setReadOnly(true);
    m_editRight->setLineWrapMode(QPlainTextEdit::NoWrap);
    columnsLayout->addWidget(m_editRight, 1);

    mainLayout->addLayout(columnsLayout, 1);

    // Sync scrollbars
    connect(m_editLeft->verticalScrollBar(), &QScrollBar::valueChanged,
            m_editRight->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_editRight->verticalScrollBar(), &QScrollBar::valueChanged,
            m_editLeft->verticalScrollBar(), &QScrollBar::setValue);
    
    connect(m_editLeft->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_editRight->horizontalScrollBar(), &QScrollBar::setValue);
    connect(m_editRight->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_editLeft->horizontalScrollBar(), &QScrollBar::setValue);

    // Bottom action layout
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    QPushButton* btnClose = new QPushButton("Close", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);
}

void VisualDiffDialog::computeDiff(const QString& pathLeft, const QString& pathRight) {
    QFile fileL(pathLeft);
    QFile fileR(pathRight);

    if (!fileL.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Diff Failed", "Could not open left file for reading.");
        return;
    }
    if (!fileR.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Diff Failed", "Could not open right file for reading.");
        return;
    }

    QTextStream streamL(&fileL);
    QTextStream streamR(&fileR);

    QStringList linesL, linesR;
    while (!streamL.atEnd()) linesL.append(streamL.readLine());
    while (!streamR.atEnd()) linesR.append(streamR.readLine());

    fileL.close();
    fileR.close();

    // Optimize: If files are massive, avoid full LCS table to prevent memory blowup
    int n = linesL.size();
    int m = linesR.size();
    if (n > 2500 || m > 2500) {
        // Fallback simple line-by-line comparison
        int maxLen = qMax(n, m);
        QStringList alignedL, alignedR;
        for (int i = 0; i < maxLen; ++i) {
            QString l = (i < n) ? linesL[i] : "";
            QString r = (i < m) ? linesR[i] : "";
            alignedL.append(l);
            alignedR.append(r);
            if (i >= n) {
                m_addedLines.append(i);
            } else if (i >= m) {
                m_deletedLines.append(i);
            } else if (l != r) {
                m_deletedLines.append(i);
                m_addedLines.append(i);
            }
        }
        m_editLeft->setPlainText(alignedL.join("\n"));
        m_editRight->setPlainText(alignedR.join("\n"));
        applyHighlighting();
        return;
    }

    // Standard LCS dynamic programming table
    QVector<QVector<int>> dp(n + 1, QVector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (linesL[i-1] == linesR[j-1]) {
                dp[i][j] = dp[i-1][j-1] + 1;
            } else {
                dp[i][j] = qMax(dp[i-1][j], dp[i][j-1]);
            }
        }
    }

    // Backtrack to reconstruct diff alignment
    int i = n, j = m;
    struct AlignedItem {
        QString lineL;
        QString lineR;
        bool isAdd = false;
        bool isDel = false;
    };
    QList<AlignedItem> aligned;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && linesL[i-1] == linesR[j-1]) {
            aligned.prepend({linesL[i-1], linesR[j-1], false, false});
            i--;
            j--;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            aligned.prepend({"", linesR[j-1], true, false});
            j--;
        } else {
            aligned.prepend({linesL[i-1], "", false, true});
            i--;
        }
    }

    QStringList leftOutput, rightOutput;
    m_addedLines.clear();
    m_deletedLines.clear();

    for (int idx = 0; idx < aligned.size(); ++idx) {
        const auto& item = aligned[idx];
        leftOutput.append(item.lineL);
        rightOutput.append(item.lineR);
        if (item.isDel) m_deletedLines.append(idx);
        if (item.isAdd) m_addedLines.append(idx);
    }

    m_editLeft->setPlainText(leftOutput.join("\n"));
    m_editRight->setPlainText(rightOutput.join("\n"));

    applyHighlighting();
}

void VisualDiffDialog::applyHighlighting() {
    // Apply RED highlighting for deletions on left pane
    QList<QTextEdit::ExtraSelection> leftSelections;
    QTextDocument* docL = m_editLeft->document();
    for (int lineNum : m_deletedLines) {
        QTextBlock block = docL->findBlockByLineNumber(lineNum);
        if (block.isValid()) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(QColor(63, 45, 45)); // Catppuccin dark red tint
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = QTextCursor(block);
            leftSelections.append(sel);
        }
    }
    m_editLeft->setExtraSelections(leftSelections);

    // Apply GREEN highlighting for additions on right pane
    QList<QTextEdit::ExtraSelection> rightSelections;
    QTextDocument* docR = m_editRight->document();
    for (int lineNum : m_addedLines) {
        QTextBlock block = docR->findBlockByLineNumber(lineNum);
        if (block.isValid()) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(QColor(45, 63, 45)); // Catppuccin dark green tint
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = QTextCursor(block);
            rightSelections.append(sel);
        }
    }
    m_editRight->setExtraSelections(rightSelections);
}
