#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HelloKSPLauncher");
    a.setOrganizationName("HelloKSP");

    MainWindow w;
    w.show();

    return a.exec();
}
