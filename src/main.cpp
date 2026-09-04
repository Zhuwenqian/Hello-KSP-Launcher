#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
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

    // 更新窗口守卫：应用目录存在".updating"标记时，说明 updater 正在替换文件
    // （用户手动双击启动器所致）。此时不加载窗口，提示后退出，等待 updater
    // 替换完成并自动重启新版，避免运行到被占用或半替换的程序。
    if (QFile::exists(QCoreApplication::applicationDirPath()
                      + QStringLiteral("/.updating"))) {
        QMessageBox::information(nullptr, QObject::tr("正在更新"),
            QObject::tr("检测到启动器正在更新，请稍候，更新完成后将自动启动。"));
        return 0;
    }

    MainWindow w;
    w.show();

    return a.exec();
}