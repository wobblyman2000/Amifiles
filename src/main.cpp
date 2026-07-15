#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

static QIcon createApplicationIcon() {
    QPixmap pixmap(128, 128);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw dark rounded background
    QLinearGradient bgGrad(0, 0, 0, 128);
    bgGrad.setColorAt(0, QColor(30, 30, 46));
    bgGrad.setColorAt(1, QColor(17, 17, 27));
    painter.setBrush(bgGrad);
    painter.setPen(QPen(QColor(69, 71, 90), 2));
    painter.drawRoundedRect(4, 4, 120, 120, 24, 24);

    // Draw stylized folder shape
    QPainterPath folderPath;
    folderPath.moveTo(24, 40);
    folderPath.lineTo(54, 40);
    folderPath.lineTo(64, 50);
    folderPath.lineTo(104, 50);
    folderPath.lineTo(104, 96);
    folderPath.lineTo(24, 96);
    folderPath.closeSubpath();

    QLinearGradient folderGrad(24, 40, 104, 96);
    folderGrad.setColorAt(0, QColor(137, 180, 250)); // Light blue
    folderGrad.setColorAt(1, QColor(203, 166, 247)); // Purple
    painter.setBrush(folderGrad);
    painter.setPen(Qt::NoPen);
    painter.drawPath(folderPath);

    // Draw glowing document sheet sticking out of the folder
    QPainterPath docPath;
    docPath.moveTo(40, 30);
    docPath.lineTo(88, 30);
    docPath.lineTo(88, 70);
    docPath.lineTo(40, 70);
    docPath.closeSubpath();
    
    QLinearGradient docGrad(40, 30, 88, 70);
    docGrad.setColorAt(0, QColor(245, 224, 220, 220)); // Rosewater translucent
    docGrad.setColorAt(1, QColor(254, 228, 208, 180));
    painter.setBrush(docGrad);
    painter.drawPath(docPath);
    
    // Redraw folder front cover to overlap the document sheet
    QPainterPath frontPath;
    frontPath.moveTo(24, 54);
    frontPath.lineTo(104, 54);
    frontPath.lineTo(104, 96);
    frontPath.lineTo(24, 96);
    frontPath.closeSubpath();
    
    QLinearGradient frontGrad(24, 54, 104, 96);
    frontGrad.setColorAt(0, QColor(137, 180, 250, 230)); // Blue overlay
    frontGrad.setColorAt(1, QColor(116, 199, 236, 200)); // Sapphire
    painter.setBrush(frontGrad);
    painter.drawPath(frontPath);

    // Draw letter "A" highlight in the center of the folder
    painter.setPen(QPen(QColor(30, 30, 46), 3));
    QFont font("Outfit", 26, QFont::Bold);
    painter.setFont(font);
    painter.drawText(QRect(24, 54, 80, 42), Qt::AlignCenter, "A");

    return QIcon(pixmap);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set metadata for QSettings to use by default
    app.setApplicationName("Amifiles");
    app.setOrganizationName("Amifiles");
    app.setApplicationVersion("1.0");

    app.setWindowIcon(createApplicationIcon());

    MainWindow w;
    w.showMaximized();

    return app.exec();
}
