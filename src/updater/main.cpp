// updater：独立更新组件（跨平台）。
// 由主程序调用，负责：
//  1) 可选等待旧主程序进程退出（--wait-pid <pid>）；
//  2) 清理应用目录下除保留项（ckan_cache / HKSPL.json / backups / updater 自身 / .updater_update）
//     之外的所有文件与文件夹（保证新版本字节级一致，无旧文件残留）；
//  3) 从更新包解压新版本到暂存目录，再整体移入应用根目录；
//  4) 重启新版 HelloKSPLauncher.exe 后退出。
// 用法：updater --apply <新版本.zip> [--wait-pid <pid>]
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QDateTime>
#include <cstdio>

#include "miniz.h"

#if defined(_WIN32)
#include <windows.h>
#undef interface
#else
#include <unistd.h>
#include <signal.h>
#endif

namespace {

// 保留项：更新时绝不删除（用户数据 / 缓存 / 更新组件自身 / 暂存目录）
const QStringList kKeepNames = {
    QStringLiteral("ckan_cache"),
    QStringLiteral("HKSPL.json"),
    QStringLiteral("backups"),
    QStringLiteral("updater"),
    QStringLiteral("updater.exe"),
    QStringLiteral(".updater_update"),
    QStringLiteral(".updating") // "正在更新"标记：启动器据此识别本次更新窗口
};

QString g_installDir;
QString g_logPath;

void log(const QString &msg)
{
    QFile f(g_logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(QStringLiteral("[%1] %2\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate), msg).toUtf8());
        f.close();
    }
    fputs(msg.toUtf8().constData(), stderr);
    fputs("\n", stderr);
}

void fatal(const QString &msg)
{
    log(msg);
#if defined(_WIN32)
    MessageBoxW(nullptr, (const wchar_t*)msg.utf16(), L"更新失败", MB_ICONERROR | MB_OK);
#else
    fflush(stderr);
#endif
    QCoreApplication::exit(1);
}

// 等待指定 pid 的进程退出（进程已不存在视为已退出）
void waitForPidExit(int pid)
{
    if (pid <= 0) return;
#if defined(_WIN32)
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    if (!h) return; // 进程已不存在
    for (int i = 0; i < 600; ++i) { // 最多 60s
        if (WaitForSingleObject(h, 100) == WAIT_OBJECT_0) break;
    }
    CloseHandle(h);
#else
    for (int i = 0; i < 300; ++i) { // 最多 60s
        if (kill(pid, 0) != 0) break; // 进程已退出
        QThread::msleep(200);
    }
#endif
}

// 删除目录下除保留项外的所有文件和子目录
void clearAppDir()
{
    QDir dir(g_installDir);
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                                    | QDir::System | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        if (kKeepNames.contains(fi.fileName())) continue;
        if (fi.isDir())
            QDir(fi.absoluteFilePath()).removeRecursively();
        else
            QFile::remove(fi.absoluteFilePath());
        log(QStringLiteral("移除: %1").arg(fi.fileName()));
    }
}

// 把 zip 内条目路径映射到 base 下的本地路径；拒绝绝对路径 / 越级，防止 zip 路径穿越
QString safeTarget(const QString &base, const QString &zipName)
{
    if (zipName.isEmpty() || zipName.contains(QLatin1Char('\\'))) return QString();
    const QStringList segs = zipName.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segs.isEmpty()) return QString();
    QString cur = base;
    for (const QString &seg : segs) {
        if (seg == QStringLiteral("..") || seg == QLatin1Char('.') || seg.isEmpty())
            return QString();
        cur = QDir(cur).filePath(seg);
    }
    return cur;
}

// 解压 zip 到 stageDir（保留原始目录结构，供后续整体搬运）
bool extractZip(const QString &zipPath, const QString &stageDir)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof(z));
    if (!mz_zip_reader_init_file(&z, zipPath.toUtf8().constData(), 0)) {
        log(QStringLiteral("无法打开更新包: %1").arg(zipPath));
        return false;
    }
    QDir().mkpath(stageDir);
    const mz_uint count = mz_zip_reader_get_num_files(&z);

    // 第一遍：创建所有目录
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        if (st.m_is_directory) {
            const QString t = safeTarget(stageDir, QString::fromUtf8(st.m_filename));
            if (!t.isEmpty()) QDir().mkpath(t);
        }
    }
    // 第二遍：解压文件
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        if (st.m_is_directory) continue;
        const QString name = QString::fromUtf8(st.m_filename);
        const QString t = safeTarget(stageDir, name);
        if (t.isEmpty()) {
            log(QStringLiteral("跳过不安全路径: %1").arg(name));
            continue;
        }
        QDir().mkpath(QFileInfo(t).absolutePath());
        if (!mz_zip_reader_extract_to_file(&z, i, t.toUtf8().constData(), 0)) {
            log(QStringLiteral("解压失败: %1").arg(name));
            mz_zip_reader_end(&z);
            return false;
        }
    }
    mz_zip_reader_end(&z);
    return true;
}

// 把 srcDir 第一层的子项逐一移入 dstDir（目标已存在则先删除），成功返回 true
bool moveTopLevel(const QString &srcDir, const QString &dstDir)
{
    QDir src(srcDir);
    const QFileInfoList entries = src.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                                    | QDir::System | QDir::Hidden);
    bool ok = true;
    for (const QFileInfo &fi : entries) {
        // 保留项不搬运（目标中的应用数据已在保留项中）
        if (kKeepNames.contains(fi.fileName())) continue;
        const QString dst = QDir(dstDir).filePath(fi.fileName());
        if (QFileInfo::exists(dst)) {
            if (fi.isDir())
                QDir(dst).removeRecursively();
            else
                QFile::remove(dst);
        }
        if (!QFile::rename(fi.absoluteFilePath(), dst)) {
            log(QStringLiteral("移动失败: %1").arg(fi.absoluteFilePath()));
            ok = false;
        }
    }
    return ok;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString zipPath, waitPid, installDir;
    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--apply") && i + 1 < args.size()) zipPath = args[++i];
        else if (args[i] == QLatin1String("--wait-pid") && i + 1 < args.size()) waitPid = args[++i];
        else if (args[i] == QLatin1String("--dir") && i + 1 < args.size()) installDir = args[++i];
    }
    if (zipPath.isEmpty() || installDir.isEmpty()) {
        fputs("usage: updater --dir <installDir> --apply <zip> [--wait-pid <pid>]\n", stderr);
        return 2;
    }

    g_installDir = QDir(installDir).absolutePath();
    g_logPath = QDir(g_installDir).filePath(QStringLiteral("updater.log"));

    // 写"正在更新"标记：更新窗口内（等待旧进程退出 / 清理 / 解压替换）用户手动
    // 双击启动器时，由启动器据此识别并直接退出，避免运行到被占用或半替换的程序。
    {
        QFile mk(QDir(g_installDir).filePath(QStringLiteral(".updating")));
        if (mk.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            mk.write("updating");
            mk.close();
        }
    }

    log(QStringLiteral("=== 开始更新: 包=%1 ===").arg(zipPath));
    if (!waitPid.isEmpty()) {
        waitForPidExit(waitPid.toInt());
        log(QStringLiteral("旧主程序已退出"));
    }

    if (!QFile::exists(zipPath))
        fatal(QStringLiteral("更新包不存在: %1").arg(zipPath));

    clearAppDir();

    const QString updDir = QDir(g_installDir).filePath(QStringLiteral(".updater_update"));
    const QString stageDir = QDir(updDir).filePath(QStringLiteral("stage"));
    if (!extractZip(zipPath, stageDir))
        fatal(QStringLiteral("解压更新包失败"));

    // 新版本根目录：zip 通常含单一顶层目录（发布文件夹名），取其内容为新根；否则 stage 即新根
    QString newRoot = stageDir;
    {
        QDir stage(stageDir);
        const QStringList firsts = stage.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (firsts.size() == 1) {
            const QString only = firsts.first();
            if (QFileInfo(stage.filePath(only)).isDir())
                newRoot = stage.filePath(only);
        }
    }
    if (!moveTopLevel(newRoot, g_installDir))
        fatal(QStringLiteral("应用新版本时出现错误（部分文件可能未覆盖）"));

    QDir(updDir).removeRecursively();
    // 替换完成、即将重启新版前，清除"正在更新"标记，让新启动器正常启动。
    QFile::remove(QDir(g_installDir).filePath(QStringLiteral(".updating")));

    // 重启新版主程序
    const QString exe = QDir(g_installDir).filePath(
#if defined(_WIN32)
        QStringLiteral("HelloKSPLauncher.exe"));
#else
        QStringLiteral("HelloKSPLauncher"));
#endif
    if (!QFile::exists(exe))
        fatal(QStringLiteral("未找到新版主程序: %1").arg(exe));
    log(QStringLiteral("启动新版: %1").arg(exe));
    QProcess::startDetached(exe, QStringList());

    log(QStringLiteral("=== 更新完成 ==="));
    return 0;
}