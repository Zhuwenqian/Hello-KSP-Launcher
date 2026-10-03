#ifndef GAME_GAMEPROCESSMANAGER_H
#define GAME_GAMEPROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

// 游戏进程域管理器：启动/停止 KSP 进程、高优先级与内存限制、
// 插件临时禁用与自动还原。启动器中唯一持有游戏进程状态的 QObject 单例。
class GameProcessManager : public QObject
{
    Q_OBJECT
public:
    static GameProcessManager& instance();

    // KSP 安装目录定位与合法性校验（GameData + KSP 可执行文件）
    static QString detectGameRoot(const QString &exePath);
    static bool isValidKSPPath(const QString &path);

    bool launchGame(const QString& exePath, const QString& args = QString(),
                    int memoryLimitMB = 0, bool highPriority = false);
    void stopGame();

    // 临时禁用插件：把 dll 重命名为「dll.disabled」并记录原路径，游戏进程退出后自动还原。
    // dll 不存在但 .disabled 已存在时视为已处于禁用状态，仅接管记录（幂等）。
    // 重命名失败（如文件被占用）返回 false。
    bool disablePluginTemporarily(const QString &dllPath);
    // 还原临时禁用的插件（去掉 .disabled 后缀并清除记录）。
    // 无记录且 expectedDllPath 非空时按该路径兜底还原（启动器重启后记录丢失的场景）。
    bool restoreTempDisabledPlugin(const QString &expectedDllPath = QString());

signals:
    void gameStarted();
    void gameFinished(int exitCode, QProcess::ExitStatus status);
    void gameError(QProcess::ProcessError error);

private slots:
    // 进程真正启动（已拿到有效 pid）后应用高优先级与内存限制。
    // launchGame 中 start() 返回后 state()/processId() 未必有效，故推迟到 started 信号再读。
    void applyGameOptions();

private:
    explicit GameProcessManager(QObject *parent = nullptr);
    ~GameProcessManager();
    GameProcessManager(const GameProcessManager&) = delete;
    GameProcessManager& operator=(const GameProcessManager&) = delete;

    // 释放游戏进程内存限制所用的 Job Object（Windows）；其它平台为空实现
    void releaseMemoryJob();

    QProcess* m_gameProcess;
    QString m_tempDisabledDll;           // 临时禁用插件的原始 dll 路径（空=无），游戏退出后还原
    QTimer* m_stopKillTimer = nullptr;   // 优雅终止超时后强制 kill（定期器回调，避免主线程阻塞等待）
    bool m_pendingHighPriority = false;  // 待 started 后应用的高优先级标记
    int  m_pendingMemoryLimitMB = 0;     // 待 started 后应用的内存限制（MB，0=不限）
#if defined(_WIN32)
    void* m_memoryJob = nullptr; // 进程内存限制所用的 Job Object 句柄（须在游戏退出/析构时释放）
#endif
};

#endif // GAME_GAMEPROCESSMANAGER_H
