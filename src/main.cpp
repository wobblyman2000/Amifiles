#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set metadata for QSettings to use by default
    app.setApplicationName("Amifiles");
    app.setOrganizationName("Amifiles");
    app.setApplicationVersion("1.0");

    MainWindow w;
    w.show();

    return app.exec();
}
