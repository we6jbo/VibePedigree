#include <QApplication>
#include <QCoreApplication>
#include "mainwindow.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("oneal");
    QCoreApplication::setApplicationName("VibePedigree");
    MainWindow window;
    window.show();
    return app.exec();
}
