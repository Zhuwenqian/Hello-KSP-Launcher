#include "instancemanager.h"
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
}

bool InstanceManager::launchGame(const QString &exePath, const QString &args)
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        return false;
    }

    if (!m_gameProcess) {
        m_gameProcess = new QProcess(this);
        connect(m_gameProcess, &QProcess::started, this, &InstanceManager::gameStarted);
        connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &InstanceManager::gameFinished);
        connect(m_gameProcess, &QProcess::errorOccurred, this, &InstanceManager::gameError);
    }

    QFileInfo exeInfo(exePath);
    m_gameProcess->setWorkingDirectory(exeInfo.absolutePath());

    // Split args by spaces, handling simple quoting
    QStringList argList = QProcess::splitCommand(args);
    m_gameProcess->start(exePath, argList);
    return true;
}

void InstanceManager::stopGame()
{
    if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning)
        return;
    // 先发送关闭信号(WM_CLOSE)给游戏一个保存机会，超时未退出再强制结束
    m_gameProcess->terminate();
    if (!m_gameProcess->waitForFinished(3000))
        m_gameProcess->kill();
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
