#ifndef PROCESSOPT_H
#define PROCESSOPT_H

#include <QStringList>
#include <QList>

// 跨平台的进程级启动选项工具（高优先级 / 内存限制）。
// 独立小模块便于单测；仅在真实启动路径（InstanceManager::launchGame）执行副作用操作。
namespace processopt {

// 一条结束浏览器进程的本地命令
struct KillCommand {
    QString program;   // 可执行程序（taskkill / pkill）
    QStringList args;  // 参数
};

// 结束常见浏览器进程（Edge/Chrome/Firefox）对应的命令列表（纯函数，按平台生成，便于单测）。
QList<KillCommand> browserKillCommands();

// 逐个执行 browserKillCommands()，结束浏览器进程。返回是否执行过命令。
bool killBrowsers();

// 将 pid 对应进程的 CPU 优先级设为“高”。
// 无效 pid / 平台不支持返回 false，失败不抛异常。
bool setProcessHighPriority(qint64 pid);

#if defined(_WIN32)
// Windows：把进程放入 Job Object 并限制该 Job 的内存上限。
// memoryBytes<=0 表示不限制（仍会创建 Job 绑定进程）。返回 Job 句柄(void*)，
// 调用方须在进程退出后 closeMemoryJob()。创建失败返回 nullptr。
void *openMemoryJob(qint64 pid, qint64 memoryBytes);
void closeMemoryJob(void *job);
#else
// POSIX(Linux/macOS)：在子进程 exec 前调用（QProcess::setChildProcessModifier 内），
// 限制其虚拟地址空间 RLIMIT_AS。memoryBytes<=0 表示不限制。
void setChildMemoryLimit(qint64 memoryBytes);
#endif

} // namespace processopt

#endif // PROCESSOPT_H