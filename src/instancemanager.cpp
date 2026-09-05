#include "instancemanager.h"
#include "processopt.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

InstanceManager& InstanceManager::instance()
{
    static InstanceManager inst;
    return inst;
}

InstanceManager::InstanceManager(QObject *parent)
    : QObject(parent), m_gameProcess(nullptr)
{
}

InstanceManager::~InstanceManager()
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        m_gameProcess->kill();
        m_gameProcess->waitForFinished(3000);
    }
    delete m_gameProcess;
#if defined(_WIN32)
    releaseMemoryJob();
#endif
}

bool InstanceManager::launchGame(const QString &exePath, const QString &args,
                                 int memoryLimitMB, bool highPriority)
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        return false;
    }

    if (!m_gameProcess) {
        m_gameProcess = new QProcess(this);
        connect(m_gameProcess, &QProcess::started, this, &InstanceManager::applyGameOptions);
        connect(m_gameProcess, &QProcess::started, this, &InstanceManager::gameStarted);
        connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &InstanceManager::gameFinished);
        connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int, QProcess::ExitStatus) {
            // 进程退出：停止优雅终止的强制 kill 定时器，并释放内存限制 Job
            if (m_stopKillTimer) m_stopKillTimer->stop();
            releaseMemoryJob();
        });
        connect(m_gameProcess, &QProcess::errorOccurred, this, &InstanceManager::gameError);
    }

    QFileInfo exeInfo(exePath);
    m_gameProcess->setWorkingDirectory(exeInfo.absolutePath());

    // 高优先级：先结束浏览器进程为游戏让资源
    if (highPriority) {
        processopt::killBrowsers();
    }

#if defined(_WIN32)
    // Windows 内存限制通过 Job Object 在进程启动后绑定，无需子进程钩子
#else
    // POSIX：在子进程 exec 前限制其地址空间（RLIMIT_AS）
    if (memoryLimitMB > 0) {
        const qint64 bytes = static_cast<qint64>(memoryLimitMB) * 1024 * 1024;
        m_gameProcess->setChildProcessModifier([bytes]() {
            processopt::setChildMemoryLimit(bytes);
        });
    }
#endif

    // Split args by spaces, handling simple quoting
    QStringList argList = QProcess::splitCommand(args);

    // 高优先级与内存限制推迟到 started 信号（进程真正启动、pid 有效）后再应用。
    // start() 返回后 state()/processId() 可能仍在 Starting/无效，立即读取会拿不到 pid。
    m_pendingHighPriority = highPriority;
    m_pendingMemoryLimitMB = memoryLimitMB;

    m_gameProcess->start(exePath, argList);
    return true;
}

void InstanceManager::applyGameOptions()
{
    if (!m_gameProcess) return;
    const qint64 pid = m_gameProcess->processId();
    if (m_pendingHighPriority) {
        processopt::setProcessHighPriority(pid);
    }
#if defined(_WIN32)
    if (m_pendingMemoryLimitMB > 0) {
        const qint64 bytes = static_cast<qint64>(m_pendingMemoryLimitMB) * 1024 * 1024;
        m_memoryJob = processopt::openMemoryJob(pid, bytes);
    }
#endif
    // 一次性应用，复位避免作用于后续启动
    m_pendingHighPriority = false;
    m_pendingMemoryLimitMB = 0;
}

void InstanceManager::releaseMemoryJob()
{
#if defined(_WIN32)
    if (m_memoryJob) {
        processopt::closeMemoryJob(m_memoryJob);
        m_memoryJob = nullptr;
    }
#endif
}

void InstanceManager::stopGame()
{
    if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning)
        return;
    // 先发关闭信号(WM_CLOSE)给游戏一个保存机会。剩余动作全部异步：
    // 超时强 kill 由定时器回调执行、内存 Job 释放与 UI 状态复位由 finished 信号完成，
    // 不再在主线程 waitForFinished 阻塞等待（避免停止游戏时冻结界面）。
    m_gameProcess->terminate();
    if (!m_stopKillTimer) {
        m_stopKillTimer = new QTimer(this);
        m_stopKillTimer->setSingleShot(true);
        m_stopKillTimer->setInterval(3000);
        connect(m_stopKillTimer, &QTimer::timeout, this, [this]() {
            // 优雅关闭超时：进程仍运行则强制结束
            if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning)
                m_gameProcess->kill();
        });
    }
    m_stopKillTimer->start();
}

QString InstanceManager::detectGameRoot(const QString &exePath) const
{
    QFileInfo fi(exePath);
    return fi.absolutePath();
}

bool InstanceManager::isValidKSPPath(const QString &path) const
{
    // settings.cfg 由游戏首次启动后生成，全新未运行过的安装没有该文件，因此不作为合法性必要条件。
    // 只需存在 KSP 可执行文件与 GameData 目录即可判定为有效的 KSP 游戏目录。
    QDir dir(path);
    if (!dir.exists("GameData")) return false;
    return QFileInfo::exists(dir.filePath(QStringLiteral("KSP_x64.exe")))
        || QFileInfo::exists(dir.filePath(QStringLiteral("KSP.exe")))
        || QFileInfo::exists(dir.filePath(QStringLiteral("KSP.x86_64")));
}
