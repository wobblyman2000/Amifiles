#include "hexeditorwidget.h"
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QFontDatabase>

HexEditorWidget::HexEditorWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setStyleSheet(
        "QTextBrowser { background-color: #11111b; color: #cdd6f4; border: 1px solid #313244; border-radius: 4px; padding: 6px; }"
    );

    // Load monospaced font
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_textBrowser->setFont(monoFont);

    layout->addWidget(m_textBrowser);
}

void HexEditorWidget::clear() {
    m_textBrowser->clear();
}

void HexEditorWidget::loadFile(const QString& filePath) {
    clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_textBrowser->setText("<span style='color:#f38ba8;'>Error: Unable to open file for binary reading.</span>");
        return;
    }

    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();
    
    // Read up to 64 KB
    qint64 readLimit = 64 * 1024;
    QByteArray data = file.read(readLimit);
    file.close();

    QString htmlText = QString("<p style='color:#a6adc8; font-weight:bold;'>Binary Hex View of: %1 (%2 bytes shown)</p>")
                       .arg(fileInfo.fileName())
                       .arg(data.size());
    htmlText += "<pre style='font-family:monospace; line-height: 1.2;'>";

    // Header line
    htmlText += "<span style='color:#f9e2af;'>Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  Decoded Text</span>\n";
    htmlText += "<span style='color:#313244;'>-------------------------------------------------------------------------</span>\n";

    for (int offset = 0; offset < data.size(); offset += 16) {
        // Offset
        QString offsetStr = QString("%1").arg(offset, 8, 16, QChar('0')).toUpper();
        htmlText += QString("<span style='color:#89b4fa;'>%1</span>  ").arg(offsetStr);

        // Hex bytes
        QString hexPart;
        QString asciiPart;
        for (int i = 0; i < 16; ++i) {
            if (offset + i < data.size()) {
                unsigned char c = static_cast<unsigned char>(data.at(offset + i));
                hexPart += QString("%1 ").arg(QString::number(c, 16).rightJustified(2, '0')).toUpper();

                // ASCII
                if (c >= 32 && c <= 126) {
                    // Escape HTML characters
                    if (c == '<') asciiPart += "&lt;";
                    else if (c == '>') asciiPart += "&gt;";
                    else if (c == '&') asciiPart += "&amp;";
                    else asciiPart += QChar(c);
                } else {
                    asciiPart += ".";
                }
            } else {
                hexPart += "   ";
                asciiPart += " ";
            }
            if (i == 7) {
                hexPart += " ";
            }
        }

        htmlText += QString("<span style='color:#cdd6f4;'>%1</span> ").arg(hexPart);
        htmlText += QString("<span style='color:#a6e3a1;'> %1</span>\n").arg(asciiPart);
    }

    if (fileSize > readLimit) {
        htmlText += QString("\n<span style='color:#f38ba8;'>... [Truncated: File size is %1 MB, showing first 64 KB]</span>").arg(QString::number(fileSize / (1024.0 * 1024.0), 'f', 2));
    }

    htmlText += "</pre>";
    m_textBrowser->setHtml(htmlText);
}
