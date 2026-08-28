#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QIcon>
#include "mainwindow.h"
#include "configmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HelloKSPLauncher");
    a.setOrganizationName("HelloKSP");
    // 应用图标：覆盖窗口左上角与任务栏图标
    a.setWindowIcon(QIcon(QStringLiteral(":/appicon.ico")));

    // Load translation based on saved language setting
    QTranslator translator;
    QString lang = ConfigManager::instance().language();
    if (translator.load("hello_ksp_launcher_" + lang,
                         QCoreApplication::applicationDirPath() + "/translations")) {
        a.installTranslator(&translator);
    }

    MainWindow w;
    w.show();

    return a.exec();
}