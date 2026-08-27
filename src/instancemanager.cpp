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

QString InstanceManager::detectGameRoot(const QString &exePath) const
{
    QFileInfo fi(exePath);
    return fi.absolutePath();
}

bool InstanceManager::isValidKSPPath(const QString &path) const
{
    QDir dir(path);
    return dir.exists("settings.cfg") && dir.exists("GameData");
}
