#ifndef METADATAHOVERCARD_H
#define METADATAHOVERCARD_H

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPainter>
#include <QPen>
#include "metadataextractor.h"
#include "tagmanager.h"

class MetadataHoverCard : public QFrame {
public:
    explicit MetadataHoverCard(QWidget* parent = nullptr) 
        : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        
        setStyleSheet(
            "QLabel { color: #cdd6f4; font-family: 'Outfit'; font-size: 13px; }"
            "QLabel#titleLabel { color: #89b4fa; font-size: 16px; font-weight: bold; }"
            "QLabel#ratingLabel { color: #f9e2af; font-size: 15px; }"
        );
        
        QVBoxLayout* frameLayout = new QVBoxLayout(this);
        frameLayout->setContentsMargins(12, 12, 12, 12);
        frameLayout->setSpacing(6);
        
        m_lblTitle = new QLabel(this);
        m_lblTitle->setObjectName("titleLabel");
        m_lblTitle->setWordWrap(true);
        frameLayout->addWidget(m_lblTitle);
        
        m_lblRating = new QLabel(this);
        m_lblRating->setObjectName("ratingLabel");
        frameLayout->addWidget(m_lblRating);
        
        m_infoLayout = new QFormLayout();
        m_infoLayout->setSpacing(6);
        m_infoLayout->setLabelAlignment(Qt::AlignRight);
        m_infoLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        frameLayout->addLayout(m_infoLayout);
    }
    
    void setMetadata(const FileMetadata& meta, const QString& filePath) {
        m_currentFilePath = filePath;
        m_lblTitle->setText(meta.name);
        
        int rating = TagManager::instance().getFileRating(filePath);
        if (rating > 0) {
            QString stars;
            for (int i = 1; i <= 5; ++i) {
                stars += (i <= rating) ? "★" : "☆";
            }
            m_lblRating->setText(stars);
            m_lblRating->setVisible(true);
        } else {
            m_lblRating->setVisible(false);
        }
        
        QLayoutItem* child;
        while ((child = m_infoLayout->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
        
        auto addInfoRow = [this](const QString& labelText, const QString& valueText) {
            if (valueText.isEmpty()) return;
            QLabel* lblName = new QLabel(labelText, this);
            lblName->setStyleSheet("color: #a6adc8; font-weight: bold;");
            QLabel* lblVal = new QLabel(valueText, this);
            lblVal->setWordWrap(true);
            m_infoLayout->addRow(lblName, lblVal);
        };
        
        QString format = meta.imageFormat;
        if (format.isEmpty()) format = meta.extension.toUpper();
        addInfoRow("Format:", format);
        
        if (meta.size > 0) {
            double mb = meta.size / (1024.0 * 1024.0);
            addInfoRow("Size:", QString("%1 MB").arg(mb, 0, 'f', 2));
        }
        
        if (meta.imageDimensions.isValid()) {
            addInfoRow("Resolution:", QString("%1 × %2").arg(meta.imageDimensions.width()).arg(meta.imageDimensions.height()));
        }
        
        if (!meta.artist.isEmpty()) addInfoRow("Artist:", meta.artist);
        if (!meta.album.isEmpty()) addInfoRow("Album:", meta.album);
        if (meta.durationMs > 0) addInfoRow("Duration:", meta.durationStr);
        if (meta.bitrate > 0) addInfoRow("Bitrate:", QString("%1 kbps").arg(meta.bitrate));
        
        if (!meta.dateTaken.isEmpty()) addInfoRow("Date Taken:", meta.dateTaken);
        if (!meta.cameraModel.isEmpty()) addInfoRow("Camera:", meta.cameraModel);
        if (!meta.modified.isEmpty()) addInfoRow("Modified:", meta.modified);
        
        QStringList tags = TagManager::instance().getFileTags(filePath);
        if (!tags.isEmpty()) {
            addInfoRow("Tags:", tags.join(", "));
        }
        
        adjustSize();
    }
    
    QString currentFilePath() const { return m_currentFilePath; }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Draw solid background
        painter.setBrush(QColor("#1e1e2e"));
        // Draw solid borders
        painter.setPen(QPen(QColor("#89b4fa"), 1));
        
        QRectF r = rect();
        r.adjust(0.5, 0.5, -0.5, -0.5);
        painter.drawRoundedRect(r, 8, 8);
    }

private:
    QString m_currentFilePath;
    QLabel* m_lblTitle = nullptr;
    QLabel* m_lblRating = nullptr;
    QFormLayout* m_infoLayout = nullptr;
};

#endif // METADATAHOVERCARD_H
