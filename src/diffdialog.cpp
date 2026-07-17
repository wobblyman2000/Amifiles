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
    m_lblHeader->setText(QString("Comparing & Merging:\nLeft:  %1\nRight: %2")
                         .arg(m_fileLeft).arg(m_fileRight));
    mainLayout->addWidget(m_lblHeader);

    // Edit columns layout
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(0);

    m_leftLineNums = new QPlainTextEdit(this);
    m_leftLineNums->setReadOnly(true);
    m_leftLineNums->setFixedWidth(40);
    m_leftLineNums->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_leftLineNums->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_leftLineNums->setStyleSheet("QPlainTextEdit { background-color: #11111b; color: #585b70; border: 1px solid #313244; text-align: right; font-family: monospace; font-size: 12px; }");
    columnsLayout->addWidget(m_leftLineNums);

    m_editLeft = new QPlainTextEdit(this);
    m_editLeft->setReadOnly(false);
    m_editLeft->setLineWrapMode(QPlainTextEdit::NoWrap);
    columnsLayout->addWidget(m_editLeft, 1);

    m_rightLineNums = new QPlainTextEdit(this);
    m_rightLineNums->setReadOnly(true);
    m_rightLineNums->setFixedWidth(40);
    m_rightLineNums->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rightLineNums->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rightLineNums->setStyleSheet("QPlainTextEdit { background-color: #11111b; color: #585b70; border: 1px solid #313244; text-align: right; font-family: monospace; font-size: 12px; }");
    columnsLayout->addWidget(m_rightLineNums);

    m_editRight = new QPlainTextEdit(this);
    m_editRight->setReadOnly(false);
    m_editRight->setLineWrapMode(QPlainTextEdit::NoWrap);
    columnsLayout->addWidget(m_editRight, 1);

    mainLayout->addLayout(columnsLayout, 1);

    // Sync scrollbars across all 4 text edits
    auto syncScrolls = [](QScrollBar* src, QScrollBar* dest) {
        connect(src, &QScrollBar::valueChanged, dest, &QScrollBar::setValue);
    };

    syncScrolls(m_editLeft->verticalScrollBar(), m_leftLineNums->verticalScrollBar());
    syncScrolls(m_editLeft->verticalScrollBar(), m_editRight->verticalScrollBar());
    syncScrolls(m_editLeft->verticalScrollBar(), m_rightLineNums->verticalScrollBar());

    syncScrolls(m_editRight->verticalScrollBar(), m_leftLineNums->verticalScrollBar());
    syncScrolls(m_editRight->verticalScrollBar(), m_editLeft->verticalScrollBar());
    syncScrolls(m_editRight->verticalScrollBar(), m_rightLineNums->verticalScrollBar());

    syncScrolls(m_editLeft->horizontalScrollBar(), m_editRight->horizontalScrollBar());
    syncScrolls(m_editRight->horizontalScrollBar(), m_editLeft->horizontalScrollBar());

    // Bottom action layout
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    QPushButton* btnMergeLtoR = new QPushButton("◀ Merge Left to Right", this);
    QPushButton* btnMergeRtoL = new QPushButton("Merge Right to Left ▶", this);
    QPushButton* btnSave = new QPushButton("💾 Save Changes", this);
    QPushButton* btnClose = new QPushButton("Close", this);

    btnMergeLtoR->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnMergeRtoL->setStyleSheet("QPushButton { background-color: #313244; color: #a6e3a1; } QPushButton:hover { background-color: #a6e3a1; color: #11111b; }");
    btnSave->setStyleSheet("QPushButton { background-color: #fab387; color: #11111b; } QPushButton:hover { background-color: #f9e2af; }");
    btnMergeLtoR->setToolTip("Overwrite right file with left file contents");
    btnMergeRtoL->setToolTip("Overwrite left file with right file contents");

    connect(btnMergeLtoR, &QPushButton::clicked, this, &VisualDiffDialog::onMergeLeftToRight);
    connect(btnMergeRtoL, &QPushButton::clicked, this, &VisualDiffDialog::onMergeRightToLeft);
    connect(btnSave, &QPushButton::clicked, this, &VisualDiffDialog::onSave);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(btnMergeLtoR);
    btnLayout->addWidget(btnMergeRtoL);
    btnLayout->addStretch(1);
    btnLayout->addWidget(btnSave);
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
        QStringList leftNums, rightNums;
        for (int i = 0; i < maxLen; ++i) {
            QString l = (i < n) ? linesL[i] : "";
            QString r = (i < m) ? linesR[i] : "";
            alignedL.append(l);
            alignedR.append(r);
            leftNums.append(i < n ? QString::number(i + 1) : "~");
            rightNums.append(i < m ? QString::number(i + 1) : "~");
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
        m_leftLineNums->setPlainText(leftNums.join("\n"));
        m_rightLineNums->setPlainText(rightNums.join("\n"));
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
        QString numL;
        QString numR;
        bool isAdd = false;
        bool isDel = false;
    };
    QList<AlignedItem> aligned;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && linesL[i-1] == linesR[j-1]) {
            aligned.prepend({linesL[i-1], linesR[j-1], QString::number(i), QString::number(j), false, false});
            i--;
            j--;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            aligned.prepend({"", linesR[j-1], "~", QString::number(j), true, false});
            j--;
        } else {
            aligned.prepend({linesL[i-1], "", QString::number(i), "~", false, true});
            i--;
        }
    }

    QStringList leftOutput, rightOutput;
    QStringList leftNums, rightNums;
    m_addedLines.clear();
    m_deletedLines.clear();

    for (int idx = 0; idx < aligned.size(); ++idx) {
        const auto& item = aligned[idx];
        leftOutput.append(item.lineL);
        rightOutput.append(item.lineR);
        leftNums.append(item.numL);
        rightNums.append(item.numR);
        if (item.isDel) m_deletedLines.append(idx);
        if (item.isAdd) m_addedLines.append(idx);
    }

    m_editLeft->setPlainText(leftOutput.join("\n"));
    m_editRight->setPlainText(rightOutput.join("\n"));
    m_leftLineNums->setPlainText(leftNums.join("\n"));
    m_rightLineNums->setPlainText(rightNums.join("\n"));

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

void VisualDiffDialog::onSave() {
    QFile fileL(m_fileLeft);
    QFile fileR(m_fileRight);

    if (!fileL.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save Failed", "Could not open left file for writing.");
        return;
    }
    if (!fileR.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save Failed", "Could not open right file for writing.");
        fileL.close();
        return;
    }

    QTextStream streamL(&fileL);
    QTextStream streamR(&fileR);

    // Strip padding lines when writing back to files
    QStringList linesL = m_editLeft->toPlainText().split('\n');
    QStringList numsL = m_leftLineNums->toPlainText().split('\n');
    for (int idx = 0; idx < qMin(linesL.size(), numsL.size()); ++idx) {
        if (numsL[idx] != "~") {
            streamL << linesL[idx] << "\n";
        }
    }

    QStringList linesR = m_editRight->toPlainText().split('\n');
    QStringList numsR = m_rightLineNums->toPlainText().split('\n');
    for (int idx = 0; idx < qMin(linesR.size(), numsR.size()); ++idx) {
        if (numsR[idx] != "~") {
            streamR << linesR[idx] << "\n";
        }
    }

    fileL.close();
    fileR.close();

    QMessageBox::information(this, "Changes Saved", "Both files have been updated successfully on disk.");
    computeDiff(m_fileLeft, m_fileRight);
}

void VisualDiffDialog::onMergeLeftToRight() {
    if (QMessageBox::question(this, "Confirm Merge", "Overwrite the Right file with Left file contents?") != QMessageBox::Yes) {
        return;
    }
    QFile::remove(m_fileRight);
    if (QFile::copy(m_fileLeft, m_fileRight)) {
        computeDiff(m_fileLeft, m_fileRight);
        QMessageBox::information(this, "Merge Complete", "Right file has been successfully merged.");
    } else {
        QMessageBox::warning(this, "Merge Failed", "Failed to overwrite Right file.");
    }
}

void VisualDiffDialog::onMergeRightToLeft() {
    if (QMessageBox::question(this, "Confirm Merge", "Overwrite the Left file with Right file contents?") != QMessageBox::Yes) {
        return;
    }
    QFile::remove(m_fileLeft);
    if (QFile::copy(m_fileRight, m_fileLeft)) {
        computeDiff(m_fileLeft, m_fileRight);
        QMessageBox::information(this, "Merge Complete", "Left file has been successfully merged.");
    } else {
        QMessageBox::warning(this, "Merge Failed", "Failed to overwrite Left file.");
    }
}
