#include "processopt.h"

#include <QProcess>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/wait.h>
#endif
#endif

namespace processopt {

QList<KillCommand> browserKillCommands()
{
    QList<KillCommand> cmds;
#if defined(_WIN32)
    // Windows：taskkill /IM <进程名> /F 按映像名结束
    for (const QString &exe : { QStringLiteral("msedge.exe"),
                                QStringLiteral("chrome.exe"),
                                QStringLiteral("firefox.exe") }) {
        cmds.append({ QStringLiteral("taskkill"), { QStringLiteral("/IM"), exe, QStringLiteral("/F") } });
    }
#else
    // POSIX：pkill -9 -f <进程名> 匹配进程名（不含 launcher 自身路径）
    for (const QString &name : { QStringLiteral("msedge"), QStringLiteral("firefox"),
                                 QStringLiteral("chrome") }) {
        cmds.append({ QStringLiteral("pkill"), { QStringLiteral("-9"), QStringLiteral("-f"), name } });
    }
#endif
    return cmds;
}

bool killBrowsers()
{
    const QList<KillCommand> cmds = browserKillCommands();
    for (const KillCommand &cmd : cmds)
        QProcess::execute(cmd.program, cmd.args);
    return !cmds.isEmpty();
}

bool setProcessHighPriority(qint64 pid)
{
    if (pid <= 0) return false;
#if defined(_WIN32)
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    const bool ok = SetPriorityClass(h, HIGH_PRIORITY_CLASS) != 0;
    CloseHandle(h);
    return ok;
#else
    return setpriority(PRIO_PROCESS, static_cast<id_t>(pid), -5) == 0;
#endif
}

#if defined(_WIN32)
void *openMemoryJob(qint64 pid, qint64 memoryBytes)
{
    if (pid <= 0) return nullptr;
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return nullptr;

    if (memoryBytes > 0) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
        // Windows 的 MemoryLimit 单位是字节
        info.JobMemoryLimit = static_cast<SIZE_T>(memoryBytes);
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                     &info, sizeof(info))) {
            CloseHandle(job);
            return nullptr;
        }
    }

    HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!proc) {
        CloseHandle(job);
        return nullptr;
    }
    const bool assigned = AssignProcessToJobObject(job, proc) != 0;
    CloseHandle(proc);
    if (!assigned) {
        CloseHandle(job);
        return nullptr;
    }
    return static_cast<void *>(job);
}

void closeMemoryJob(void *job)
{
    if (job) {
        CloseHandle(static_cast<HANDLE>(job));
    }
}
#else
void setChildMemoryLimit(qint64 memoryBytes)
{
    if (memoryBytes <= 0) return;
    struct rlimit rl;
    rl.rlim_cur = static_cast<rlim_t>(memoryBytes);
    rl.rlim_max = static_cast<rlim_t>(memoryBytes);
    setrlimit(RLIMIT_AS, &rl);
}
#endif

} // namespace processopt