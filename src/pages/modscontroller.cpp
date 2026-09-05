#include "modscontroller.h"

#include "ckanmanager.h"

ModsController::ModsController(QObject *parent)
    : QObject(parent)
{
    // 由本控制器持有并注入真实 Qt 弹窗决策钩子（冲突/建议/提供者/磁盘/确认）。
    // setModDecisions 还会把同一套钩子下发给 InstallService（安装前置决策）。
    m_decisions = moddecision::makeDefaultModDecisions();
    CKanManager::instance().setModDecisions(m_decisions);

    // 继发 CKanManager 后台进度与结果信号，供模组页统一消费（信号对信号直连）。
    CKanManager &mgr = CKanManager::instance();
    connect(&mgr, &CKanManager::indexRefreshed,        this, &ModsController::indexRefreshed);
    connect(&mgr, &CKanManager::operationFinished,     this, &ModsController::operationFinished);
    connect(&mgr, &CKanManager::unmanagedScanFinished, this, &ModsController::unmanagedScanFinished);
    connect(&mgr, &CKanManager::singleDownloadFinished,this, &ModsController::singleDownloadFinished);
    connect(&mgr, &CKanManager::installProgress,       this, &ModsController::installProgress);
    connect(&mgr, &CKanManager::downloadProgress,      this, &ModsController::downloadProgress);
}

void ModsController::openInstance(const QString &gameDir, const QString &instanceName)
{
    CKanManager::instance().openInstance(gameDir, instanceName);
}

bool ModsController::tryAcquireRegistryLock()
{
    return CKanManager::instance().tryAcquireRegistryLock();
}

void ModsController::requestRefreshIndex(bool force)
{
    CKanManager::instance().refreshIndexAsync(force);
}

void ModsController::requestScanDlls(bool force)
{
    CKanManager::instance().scanUnmanagedDllsAsync(force);
}

void ModsController::requestInstall(const QString &identifier)
{
    CKanManager::instance().installAsync(identifier, true);
}

void ModsController::requestInstallVersion(const ckan::CkanModule &mod)
{
    CKanManager::instance().installVersionAsync(mod);
}

void ModsController::requestInstallBatch(const QStringList &identifiers)
{
    CKanManager::instance().installBatchAsync(identifiers);
}

void ModsController::requestUpgrade(const QString &identifier)
{
    CKanManager::instance().upgradeAsync(identifier);
}

void ModsController::requestUpgradeBatch(const QStringList &identifiers)
{
    CKanManager::instance().upgradeBatchAsync(identifiers);
}

void ModsController::requestUninstall(const QString &identifier)
{
    CKanManager::instance().uninstallAsync(identifier);
}

void ModsController::requestUninstallBatch(const QStringList &identifiers)
{
    CKanManager::instance().uninstallBatchAsync(identifiers);
}

QStringList ModsController::uninstallPlan(const QStringList &identifiers) const
{
    return CKanManager::instance().uninstallPlan(identifiers);
}

void ModsController::requestImport(const QString &path)
{
    CKanManager::instance().importAsync(path);
}

void ModsController::requestDownloadSingle(const ckan::CkanModule &mod)
{
    CKanManager::instance().downloadSingleToCacheAsync(mod);
}

void ModsController::requestCancel()
{
    CKanManager::instance().cancelCurrentOperation();
}