#include "ckanmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QtConcurrent/QtConcurrent>

#include "ckan/repoindex.h"

CKanManager::CKanManager(QObject *parent)
    : QObject(parent)
{
    // GitHub 镜像，用于 CKAN-meta 仓库下载回退（主 URL 优先）
    m_mirrors = {
        QStringLiteral("https://gh-proxy.com/https://github.com/KSP-CKAN/CKAN-meta/archive/master.tar.gz"),
        QStringLiteral("https://ghfast.top/https://github.com/KSP-CKAN/CKAN-meta/archive/master.tar.gz"),
    };
}

CKanManager::~CKanManager()
{
    clearWatchers();
    delete m_ckan;
    m_ckan = nullptr;
}

CKanManager& CKanManager::instance()
{
    static CKanManager mgr;
    return mgr;
}

void CKanManager::clearWatchers()
{
    if (m_indexWatcher) { m_indexWatcher->deleteLater(); m_indexWatcher = nullptr; }
    if (m_installWatcher) { m_installWatcher->deleteLater(); m_installWatcher = nullptr; }
    if (m_installer) { m_installer->deleteLater(); m_installer = nullptr; }
}

void CKanManager::openInstance(const QString &gameDir, const QString &instanceName)
{
    if (m_ckan && m_ckan->instance()->gameDir() == gameDir)
        return; // 已是同一实例
    clearWatchers();
    delete m_ckan;
    m_ckan = new ckan::CKan(gameDir, instanceName);
    m_instanceName = instanceName;
}

void CKanManager::closeInstance()
{
    clearWatchers();
    delete m_ckan;
    m_ckan = nullptr;
    m_instanceName.clear();
}

QString CKanManager::gameDir() const
{
    return m_ckan ? m_ckan->instance()->gameDir() : QString();
}

QString CKanManager::cacheRoot() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ckan_cache"));
}

QString CKanManager::downloadDir() const
{
    return QDir(cacheRoot()).filePath(QStringLiteral("downloads"));
}

void CKanManager::refreshIndexAsync(bool force)
{
    if (!m_ckan) { emit indexRefreshed(false, tr("尚未绑定游戏实例")); return; }
    clearWatchers();

    // 索引缓存目录：exe目录/ckan_cache/index
    ckan::RepoIndex::setCacheDir(QDir(cacheRoot()).filePath(QStringLiteral("index")));
    m_indexCancelRequested.store(false);

    auto watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    m_indexWatcher = watcher;
    auto future = QtConcurrent::run([this, force]() {
        QString err;
        const bool ok = m_ckan->refreshIndex(m_mirrors, &err, force,
            [this](qint64 received, qint64 total) {
                emit downloadProgress(QStringLiteral("仓库索引"), received, total, 0);
            },
            &m_indexCancelRequested);
        if (!ok && m_indexCancelRequested.load())
            err = QStringLiteral("已取消");
        return qMakePair(ok, err);
    });
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher]() {
        const QPair<bool, QString> r = watcher->result();
        emit indexRefreshed(r.first, r.second);
        watcher->deleteLater();
        if (m_indexWatcher == watcher) m_indexWatcher = nullptr;
    });
    watcher->setFuture(future);
}

bool CKanManager::indexReady() const
{
    return m_ckan && m_ckan->indexReady();
}

int CKanManager::indexSize() const
{
    return m_ckan ? m_ckan->indexSize() : 0;
}

QVector<ckan::CkanModule> CKanManager::search(const QString &query) const
{
    return m_ckan ? m_ckan->search(query) : QVector<ckan::CkanModule>();
}

QVector<ckan::CkanModule> CKanManager::versionsOf(const QString &identifier) const
{
    return m_ckan ? m_ckan->versionsOf(identifier) : QVector<ckan::CkanModule>();
}

ckan::CkanModule CKanManager::latestOf(const QString &identifier) const
{
    return m_ckan ? m_ckan->latestOf(identifier) : ckan::CkanModule();
}

QVector<ckan::InstalledModule> CKanManager::installedModules() const
{
    return m_ckan ? m_ckan->installedModules() : QVector<ckan::InstalledModule>();
}

QString CKanManager::installedVersion(const QString &identifier) const
{
    return m_ckan ? m_ckan->registry()->installedVersion(identifier) : QString();
}

bool CKanManager::isInstalled(const QString &identifier) const
{
    return m_ckan && m_ckan->registry()->isInstalled(identifier);
}

bool CKanManager::isUpgradable(const QString &identifier) const
{
    if (!m_ckan || !m_ckan->indexReady()) return false;
    const QString installed = installedVersion(identifier);
    if (installed.isEmpty()) return false;
    const ckan::CkanModule latest = latestOf(identifier);
    if (!latest.isValid()) return false;
    return ckan::ModuleVersion(latest.version) > ckan::ModuleVersion(installed);
}

void CKanManager::installAsync(const QString &identifier, bool autoRecommends)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    const ckan::CkanModule mod = latestOf(identifier);
    if (!mod.isValid()) { emit operationFinished(false, tr("仓库中未找到：%1").arg(identifier)); return; }

    // 若已安装且无更新，提示无需操作
    if (isInstalled(identifier) && !isUpgradable(identifier)) {
        emit operationFinished(true, tr("该模组已是最新版本"));
        return;
    }
    resolveAndInstall({mod}, autoRecommends, tr("安装完成"));
}

void CKanManager::uninstallAsync(const QString &identifier)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    clearWatchers();

    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, identifier]() {
        return m_ckan->uninstall(identifier);
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this, [this, watcher]() {
        const ckan::InstallResult r = watcher->result();
        emit installedChanged();
        emit operationFinished(r.ok, r.ok ? tr("已卸载：%1").arg(
            r.installedIdentifiers.isEmpty() ? QString() : r.installedIdentifiers.first())
                                          : r.error);
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
    });
    watcher->setFuture(future);
}

void CKanManager::upgradeAsync(const QString &identifier)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    const ckan::CkanModule mod = latestOf(identifier);
    if (!mod.isValid()) { emit operationFinished(false, tr("仓库中未找到：%1").arg(identifier)); return; }
    if (!isInstalled(identifier)) { emit operationFinished(false, tr("该模组尚未安装")); return; }
    resolveAndInstall({mod}, false, tr("升级完成"));
}

void CKanManager::installBatchAsync(const QStringList &identifiers)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    QVector<ckan::CkanModule> mods;
    bool hasInstalled = false, hasMissing = false;
    for (const QString &id : identifiers) {
        if (isInstalled(id)) { hasInstalled = true; continue; } // 批量安装跳过已安装
        const ckan::CkanModule mod = latestOf(id);
        if (mod.isValid()) mods.append(mod);
        else hasMissing = true;
    }
    if (mods.isEmpty()) {
        emit operationFinished(true, hasInstalled ? tr("所选模组均已安装，无需安装")
                                                  : tr("没有可安装的模组"));
        return;
    }
    resolveAndInstall(mods, true, tr("批量安装完成（%1 个）").arg(mods.size()));
}

void CKanManager::upgradeBatchAsync(const QStringList &identifiers)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    QVector<ckan::CkanModule> mods;
    for (const QString &id : identifiers) {
        if (!isUpgradable(id)) continue;
        const ckan::CkanModule mod = latestOf(id);
        if (mod.isValid()) mods.append(mod);
    }
    if (mods.isEmpty()) {
        emit operationFinished(true, tr("没有可升级的模组"));
        return;
    }
    resolveAndInstall(mods, false, tr("批量升级完成（%1 个）").arg(mods.size()));
}

void CKanManager::uninstallBatchAsync(const QStringList &identifiers)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    QStringList toRemove;
    for (const QString &id : identifiers)
        if (isInstalled(id)) toRemove << id;
    if (toRemove.isEmpty()) {
        emit operationFinished(true, tr("没有可卸载的模组"));
        return;
    }
    clearWatchers();
    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, toRemove]() {
        ckan::InstallResult r;
        r.ok = true;
        for (const QString &id : toRemove) {
            const ckan::InstallResult rr = m_ckan->uninstall(id);
            if (!rr.ok) { r.ok = false; r.error = rr.error; break; }
        }
        return r;
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this, [this, watcher, toRemove]() {
        const ckan::InstallResult r = watcher->result();
        m_ckan->instance()->loadRegistry();
        emit installedChanged();
        emit operationFinished(r.ok, r.ok ? tr("批量卸载完成（%1 个）").arg(toRemove.size()) : r.error);
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
        if (m_installer) { m_installer->deleteLater(); m_installer = nullptr; }
    });
    watcher->setFuture(future);
}

void CKanManager::cancelCurrentOperation()
{
    m_indexCancelRequested.store(true);
    if (m_installer)
        m_installer->cancel();
}

void CKanManager::resolveAndInstall(const QVector<ckan::CkanModule> &mods, bool autoRecommends,
                                    const QString &doneMessage)
{
    if (!m_ckan || mods.isEmpty()) return;
    const ckan::ResolutionResult res = m_ckan->resolveInstallMany(mods, autoRecommends);
    if (res.conflicted) { emit operationFinished(false, res.conflicts.join(QLatin1Char('\n'))); return; }
    if (res.missing)    { emit operationFinished(false, tr("缺少依赖：%1").arg(res.notFound.join(QLatin1Char(',')))); return; }

    // 已安装但需更新的：先卸载旧版本
    QStringList preUninstall;
    for (const ckan::CkanModule &m : mods)
        if (isInstalled(m.identifier)) preUninstall.append(m.identifier);
    runInstall(res.modulesToInstall, doneMessage, preUninstall);
}

void CKanManager::runInstall(const QVector<ckan::CkanModule> &modules,
                             const QString &doneMessage, const QStringList &preUninstall)
{
    if (!m_ckan || modules.isEmpty()) return;
    clearWatchers();
    QDir().mkpath(downloadDir());

    // 在 UI 线程创建并连接进度信号，后台线程调用 install
    m_installer = new ckan::ModuleInstaller(m_ckan->instance(), this);
    connect(m_installer, &ckan::ModuleInstaller::installProgress,
            this, &CKanManager::installProgress);
    connect(m_installer, &ckan::ModuleInstaller::byteProgress,
            this, &CKanManager::downloadProgress);

    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, modules, preUninstall]() {
        for (const QString &id : preUninstall)
            m_ckan->uninstall(id);
        return m_installer->install(modules, downloadDir(), m_mirrors);
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this, [this, watcher, doneMessage]() {
        const ckan::InstallResult r = watcher->result();
        m_ckan->instance()->loadRegistry(); // 刷新已安装数据
        emit installedChanged();
        emit operationFinished(r.ok, r.ok ? doneMessage : r.error);
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
        if (m_installer) { m_installer->deleteLater(); m_installer = nullptr; }
    });
    watcher->setFuture(future);
}