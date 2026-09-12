#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include "mainwindow.h"
#include "configmanager.h"
#include "debuglogger.h"
#include "ckanmanager.h"
#include "updatemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HelloKSPLauncher");
    a.setOrganizationName("HelloKSP");
    // 应用图标：覆盖窗口左上角与任务栏图标
    a.setWindowIcon(QIcon(QStringLiteral(":/appicon.ico")));

    // 按已持久化配置启用调试日志：是否写入由启动时的设置决定，
    // 因此“开启”的本会话不写、从下次启动起把日志写入 HKSPL.log。
    DebugLogger::instance().start();

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

    // 更新器组件更新（仅 Windows）：上一轮更新因 Release 提示"本 Release 含有更新器更新"
    // 而保留了 zip（updater.exe 受保留名单约束无法自我替换），由新版启动器在此静默替换。
    // 失败弹窗提供"重试 / 忽略"；正常情况无弹窗、不打断启动。
#if defined(_WIN32)
    {
        QString updErr;
        UpdaterManager::PendingUpdaterResult updRes =
            UpdaterManager::applyPendingUpdaterUpdate(&updErr);
        while (updRes == UpdaterManager::PendingUpdaterResult::Failed) {
            QMessageBox box;
            box.setWindowTitle(QObject::tr("更新器组件更新失败"));
            box.setIcon(QMessageBox::Warning);
            box.setText(QObject::tr("检测到待处理的更新器（updater.exe）更新，但替换失败：%1")
                            .arg(updErr));
            QPushButton *retry = box.addButton(QObject::tr("重试"), QMessageBox::ActionRole);
            box.addButton(QObject::tr("忽略"), QMessageBox::ActionRole);
            box.exec();
            if (box.clickedButton() == retry)
                updRes = UpdaterManager::applyPendingUpdaterUpdate(&updErr);
            else {
                UpdaterManager::cleanupPendingUpdaterUpdate();
                break;
            }
        }
    }
#endif

    MainWindow w;
    w.show();

    // 退出清理：aboutToQuit（QApplication 仍存活、事件循环已收尾）时取消并等待全部
    // 在途后台任务（索引下载/模组下载/安装/卸载/DLL 扫描），并释放当前实例的注册表锁。
    // 不能依赖 ~CKanManager 的静态析构做这件事：全局线程池的 waitForDone() 可能先于
    // 其执行，取消标志无人置位，下载线程会一直跑到自然结束——窗口关闭后进程长时间
    // 残留，且 registry.locked 锁文件随残留进程存活，新开的启动器进模组管理会被判占用。
    QObject::connect(&a, &QCoreApplication::aboutToQuit,
                     []() { CKanManager::instance().closeInstance(); });

    return a.exec();
}