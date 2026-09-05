#include "ckanmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

#include "configmanager.h"

CKanManager::CKanManager(QObject *parent)
    : QObject(parent)
{
    // 安装/卸载流程交互决策（冲突/建议/提供者/磁盘/确认）：不再在此构造默认弹窗，
    // 改由 UI 层的 ModsController 在构造时经 setModDecisions() 注入（其同时下发给
    // InstallService），业务层不直接弹窗。ModsController 在模组页创建时即构造，
    // 早于任何安装/卸载流程，故钩子在使用前必已注入。
    // 索引镜像前缀：拼接在仓库自身 URL 前（仅 GitHub 托管的仓库适用；官方 GitHub 优先，镜像回退）
    m_indexMirrorPrefixes = {
        QStringLiteral("https://gh-proxy.com/"),
        QStringLiteral("https://ghfast.top/"),
    };
    // 模组下载镜像前缀：拼接在官方下载 URL 前（gh 代理，可代理任意 GitHub 资源）
    m_moduleMirrorPrefixes = {
        QStringLiteral("https://gh-proxy.com/"),
        QStringLiteral("https://ghfast.top/"),
    };
}

CKanManager::~CKanManager()
{
    discardCurrentInstance();
}

CKanManager& CKanManager::instance()
{
    static CKanManager mgr;
    return mgr;
}

void CKanManager::clearWatchers()
{
    if (m_indexWatcher) { m_indexWatcher->deleteLater(); m_indexWatcher = nullptr; }
    if (m_downloadWatcher) { m_downloadWatcher->deleteLater(); m_downloadWatcher = nullptr; }
    if (m_installWatcher) { m_installWatcher->deleteLater(); m_installWatcher = nullptr; }
    // 注意：不清理 m_scanWatcher。DLL 扫描是独立任务，若这里连带清掉，
    // refreshIndexAsync 等调用 clearWatchers 时会误删扫描 watcher，
    // 导致扫描完成后 unmanagedScanFinished 信号不再发出、模组页一直停在"正在扫描 DLL"。
    // 扫描 watcher 只在切换/关闭实例（openInstance/closeInstance）时清理。
}

// 放弃当前实例：先取消并等待在途后台任务（扫描/索引/下载/安装）全部结束，
// 再释放 m_ckan，避免工作线程在 m_ckan 被删除后继续访问（use-after-free）。
void CKanManager::discardCurrentInstance()
{
    // 1. 请求中止在途任务：索引/模组下载与安装均以 200ms 轮询取消标志，取消后会很快结束
    m_indexCancelRequested.store(true);
    if (m_ckan)
        m_ckan->cancelInstall();

    // 2. 等待全部在途 future 结束（仅在 future 有效时等待，避免 setFuture 未调用时卡死）。
    //    等待期间工作线程不依赖 UI 线程完成，因此不会死锁；取消后等待窗口很短。
    if (m_scanWatcher && m_scanWatcher->future().isValid())
        m_scanWatcher->future().waitForFinished();
    if (m_indexWatcher && m_indexWatcher->future().isValid())
        m_indexWatcher->future().waitForFinished();
    if (m_downloadWatcher && m_downloadWatcher->future().isValid())
        m_downloadWatcher->future().waitForFinished();
    if (m_installWatcher && m_installWatcher->future().isValid())
        m_installWatcher->future().waitForFinished();

    // 在途索引刷新已结束，复位防重入标志（其 finished 回调随后在事件循环中也会复位，幂等）
    m_indexRefreshing.store(false);

    // 3. 清理 watcher（含 DLL 扫描 watcher）与 m_ckan；先让服务脱离 m_ckan 再删除，避免悬垂指针
    clearWatchers();
    if (m_scanWatcher) { m_scanWatcher->deleteLater(); m_scanWatcher = nullptr; }
    m_index.setCkan(nullptr);
    m_scan.setCkan(nullptr);
    m_install.setCkan(nullptr);
    m_uninstall.setCkan(nullptr);
    m_modpack.setCkan(nullptr);
    m_cache.setCkan(nullptr);
    delete m_ckan;
    m_ckan = nullptr;
    m_instanceName.clear();
}

void CKanManager::openInstance(const QString &gameDir, const QString &instanceName)
{
    if (m_ckan && m_ckan->gameDir() == gameDir)
        return; // 已是同一实例
    // 切换实例：先取消并等待在途后台任务结束，再释放旧实例，避免工作线程访问已删除的 m_ckan
    discardCurrentInstance();

    // 一次性把库运行配置传入 CKan 门面（缓存目录/镜像前缀/并发），
    // 替代原先的全局静态配置（RepoIndex::setCacheDir 等）与配置双向渗透。
    ckan::CKanConfig cfg;
    cfg.indexCacheDir = QDir(cacheRoot()).filePath(QStringLiteral("index"));
    cfg.indexMirrorPrefixes = m_indexMirrorPrefixes;
    cfg.moduleMirrorPrefixes = m_moduleMirrorPrefixes;
    cfg.downloadConcurrency = ConfigManager::instance().downloadConcurrency();
    cfg.downloadRateLimitBps = ConfigManager::instance().downloadRateLimitBytesPerSecond();
    m_ckan = new ckan::CKan(gameDir, instanceName, cfg);
    m_instanceName = instanceName;
    // 把新实例注入到业务服务（查询/扫描/安装决策/卸载/整合包/缓存均基于 m_ckan）
    m_index.setCkan(m_ckan);
    m_scan.setCkan(m_ckan);
    m_install.setCkan(m_ckan);
    m_uninstall.setCkan(m_ckan);
    m_modpack.setCkan(m_ckan);
    m_cache.setCkan(m_ckan);
}

void CKanManager::closeInstance()
{
    discardCurrentInstance();
}

QString CKanManager::gameDir() const
{
    return m_ckan ? m_ckan->gameDir() : QString();
}

bool CKanManager::tryAcquireRegistryLock()
{
    return m_ckan && m_ckan->tryAcquireRegistryLock();
}

void CKanManager::reloadRegistry()
{
    if (m_ckan)
        m_ckan->reloadRegistry();
}

QString CKanManager::cacheRoot() const
{
    return m_cache.cacheRoot();
}

QString CKanManager::downloadDir() const
{
    return m_cache.downloadDir();
}

int CKanManager::cleanDownloadCache()
{
    return m_cache.cleanDownloadCache();
}

void CKanManager::refreshIndexAsync(bool force)
{
    if (!m_ckan) { emit indexRefreshed(IndexRefreshStatus::Failed, tr("尚未绑定游戏实例")); return; }
    // 防重入：索引刷新已在途时忽略本次请求（在途刷新完成后会照常发出 indexRefreshed），
    // 避免多个后台线程同时写 CKan::m_index 造成数据竞争。
    if (m_indexRefreshing.exchange(true))
        return;
    clearWatchers();
    m_indexCancelRequested.store(false);

    // 从配置读取仓库列表、缓存有效期与镜像偏好（索引缓存目录已在 openInstance 传入 CKanConfig）
    const QVector<ckan::Repository> repos = ConfigManager::instance().repositories();
    const qint64 maxAgeSecs = ConfigManager::instance().indexRefreshIntervalSecs();
    const bool preferMirror =
        ConfigManager::instance().indexDownloadSource() == ConfigManager::MirrorFirst;

    auto watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    m_indexWatcher = watcher;
    auto future = QtConcurrent::run([this, repos, force, maxAgeSecs, preferMirror]() {
        QString err;
        const bool ok = m_ckan->refreshIndex(repos, &err, force, maxAgeSecs, preferMirror,
            [this](const QString &repoName, qint64 received, qint64 total) {
                emit downloadProgress(repoName, received, total, 0);
            },
            &m_indexCancelRequested);
        return qMakePair(ok, err);
    });
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher]() {
        // 刷新期间被切实例取代（discardCurrentInstance）：仅丢弃 watcher，不再发信号。
        if (m_indexWatcher != watcher) {
            watcher->deleteLater();
            m_indexRefreshing.store(false);
            return;
        }
        const QPair<bool, QString> r = watcher->result();
        // 用类型化状态取代 "已取消" 魔法串：取消与否看取消标志，错误文案按 status 分支消费。
        const IndexRefreshStatus st = r.first ? IndexRefreshStatus::Success
            : m_indexCancelRequested.load() ? IndexRefreshStatus::Cancelled
            : IndexRefreshStatus::Failed;
        emit indexRefreshed(st, r.second);
        watcher->deleteLater();
        if (m_indexWatcher == watcher) m_indexWatcher = nullptr;
        m_indexRefreshing.store(false);
    });
    watcher->setFuture(future);
}

bool CKanManager::indexReady() const
{
    return m_index.indexReady();
}

QVector<ckan::CkanModule> CKanManager::search(const QString &query) const
{
    return m_index.search(query);
}

QVector<ckan::CkanModule> CKanManager::versionsOf(const QString &identifier) const
{
    return m_index.versionsOf(identifier);
}

ckan::CkanModule CKanManager::latestOf(const QString &identifier) const
{
    return m_index.latestOf(identifier);
}

int CKanManager::downloadCount(const QString &identifier) const
{
    return m_index.downloadCount(identifier);
}

QVector<ckan::InstalledModule> CKanManager::installedModules() const
{
    return m_ckan ? m_ckan->installedModules() : QVector<ckan::InstalledModule>();
}

QString CKanManager::installedVersion(const QString &identifier) const
{
    return m_ckan ? m_ckan->installedVersion(identifier) : QString();
}

bool CKanManager::isInstalled(const QString &identifier) const
{
    return m_ckan && m_ckan->isInstalled(identifier);
}

QStringList CKanManager::installedGameDataEntries(const QString &identifier) const
{
    return m_ckan ? m_ckan->installedGameDataEntries(identifier) : QStringList();
}

bool CKanManager::isUpgradable(const QString &identifier) const
{
    return m_index.isUpgradable(identifier);
}

void CKanManager::scanUnmanagedDllsAsync(bool force)
{
    if (!m_ckan || m_scanWatcher)
        return; // 未绑定或在途 → 无法启动新扫描
    if (!force && m_ckan->dllsScanned())
        return; // 非强制：已扫描（缓存命中）→ 无需重复全盘扫描

    auto watcher = new QFutureWatcher<void>(this);
    m_scanWatcher = watcher;
    auto future = QtConcurrent::run([this]() {
        m_scan.scanUnmanagedDlls();
    });
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        if (m_scanWatcher != watcher) { watcher->deleteLater(); return; } // 已切实例
        watcher->deleteLater();
        if (m_scanWatcher == watcher) m_scanWatcher = nullptr;
        emit unmanagedScanFinished();
    });
    watcher->setFuture(future);
}

bool CKanManager::unmanagedScanDone() const
{
    return m_scan.unmanagedScanDone();
}

bool CKanManager::isAutoDetected(const QString &identifier) const
{
    return m_scan.isAutoDetected(identifier);
}

QString CKanManager::autoDetectedVersion(const QString &identifier) const
{
    return m_scan.autoDetectedVersion(identifier);
}

QByteArray CKanManager::exportModpackCkan(QString *error)
{
    return m_modpack.exportCkan(error);
}

void CKanManager::writeHistorySnapshot()
{
    m_modpack.writeHistorySnapshot();
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

void CKanManager::installVersionAsync(const ckan::CkanModule &mod)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }
    if (!mod.isValid()) { emit operationFinished(false, tr("无效模组")); return; }
    // 与 installAsync 相同流程：依赖解析、预检、下载、冲突处理、安装。
    // 已安装旧版时 resolveAndInstall 会自动先卸载旧版再装指定版本（支持降级）。
    resolveAndInstall({mod}, true, tr("已切换版本：%1 %2").arg(mod.name, mod.version));
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
        if (m_installWatcher != watcher) { watcher->deleteLater(); return; } // 已被新操作/切实例取代
        const ckan::InstallResult r = watcher->result();
        m_ckan->reloadRegistry();
        emit installedChanged();
        if (r.cancelled) {
            // 用户取消：事务已回滚到卸载前状态，按“成功”完成处理（刷新并提示已恢复）。
            emit operationFinished(true, tr("已取消卸载，已恢复原状"));
        } else {
            const QString list = r.installedIdentifiers.join(QLatin1Char(','));
            emit operationFinished(r.ok, r.ok ? tr("已卸载：%1").arg(list.isEmpty() ? QString() : list)
                                              : r.error);
            if (r.ok) writeHistorySnapshot();
        }
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
    const QStringList toRemove = m_uninstall.filterInstalled(identifiers);
    if (toRemove.isEmpty()) {
        emit operationFinished(true, tr("没有可卸载的模组"));
        return;
    }
    clearWatchers();
    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, toRemove]() {
        // 整批在一个事务内完成：任一失败或用户 cancel() 都整体回滚到卸载前状态。
        return m_ckan->uninstallMany(toRemove);
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this, [this, watcher, toRemove]() {
        if (m_installWatcher != watcher) { watcher->deleteLater(); return; } // 已被新操作/切实例取代
        const ckan::InstallResult r = watcher->result();
        m_ckan->reloadRegistry();
        emit installedChanged();
        const QString msg = r.cancelled ? tr("已取消卸载，已恢复原状")
                          : (r.ok ? tr("批量卸载完成（%1 个）").arg(toRemove.size()) : r.error);
        // 取消回滚视为“成功”完成以便刷新；仅真正卸载成功才写历史。
        emit operationFinished(r.cancelled || r.ok, msg);
        if (r.ok && !r.cancelled) writeHistorySnapshot();
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
    });
    watcher->setFuture(future);
}

QStringList CKanManager::uninstallPlan(const QStringList &identifiers)
{
    return m_uninstall.uninstallPlan(identifiers);
}

void CKanManager::importAsync(const QString &path)
{
    if (!m_ckan) { emit operationFinished(false, tr("尚未绑定游戏实例")); return; }

    bool isMeta = false;
    QString err;
    const ckan::CkanModule mod = m_ckan->importModuleFile(path, &isMeta, &err);
    if (!mod.isValid()) { emit operationFinished(false, tr("导入失败：%1").arg(err)); return; }

    // 情形 A：元包（仅列 depends，无 install 规则）→ 从仓库解析其中的依赖安装
    if (isMeta || (mod.install.isEmpty() && !mod.depends.isEmpty())) {
        QVector<ckan::CkanModule> toInstall;
        QStringList missing;
        for (const ckan::Relationship &rel : mod.depends) {
            const QString id = rel.name;
            if (isInstalled(id)) continue;
            const ckan::CkanModule latest = latestOf(id);
            if (latest.isValid()) toInstall.append(latest);
            else missing << id;
        }
        if (toInstall.isEmpty()) {
            emit operationFinished(true, missing.isEmpty()
                                         ? tr("元包内模组均已安装，无需操作")
                                         : tr("元包内模组均已安装（仓库无：%1）")
                                               .arg(missing.join(QLatin1Char(','))));
            return;
        }
        if (!missing.isEmpty()) {
            // 仓库缺失的依赖跳过并提示（参照官方：仍安装可解析的部分）
            if (!m_decisions.confirm(
                    tr("导入模组"),
                    tr("以下依赖在仓库中不存在，将跳过：\n%1\n\n是否继续安装可解析的模组？")
                        .arg(missing.join(QLatin1Char('\n'))))) {
                emit operationFinished(false, tr("已取消")); return;
            }
        }
        resolveAndInstall(toInstall, true, tr("导入安装完成"));
        return;
    }

    // 情形 B：仓库存在同标识符 → 提示后安装仓库版本（遵循仓库并给提示）
    const ckan::CkanModule repoMod = latestOf(mod.identifier);
    if (repoMod.isValid()) {
        const bool sameVersion = (repoMod.version == mod.version);
        if (!m_decisions.confirm(
                tr("导入模组"),
                tr("模组“%1”在仓库中已存在%2。\n是否改为安装仓库版本（%3）？"
                   "\n（选否则取消本次导入）")
                    .arg(mod.name,
                         sameVersion ? QString() : tr("（本地版本 %1）").arg(mod.version),
                         repoMod.version))) {
            emit operationFinished(false, tr("已取消")); return;
        }
        resolveAndInstall({repoMod}, true, tr("安装完成"));
        return;
    }

    // 情形 C：仓库无此模组 → 复制导入文件到缓存后直接安装
    clearWatchers();
    QDir().mkpath(downloadDir());
    const QString cachePath = m_ckan->importStoreCache(mod, path, downloadDir(), &err);
    if (cachePath.isEmpty()) { emit operationFinished(false, tr("导入失败：%1").arg(err)); return; }

    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, mod]() {
        return m_ckan->installFromCache(QVector<ckan::CkanModule>{mod}, downloadDir(),
                                        {}, {}, [this](const QString &id, int percent) {
            emit installProgress(id, percent);
        });
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this,
            [this, watcher, mod]() {
        if (m_installWatcher != watcher) { watcher->deleteLater(); return; } // 已被新操作/切实例取代
        const ckan::InstallResult r = watcher->result();
        m_ckan->reloadRegistry();
        emit installedChanged();
        emit operationFinished(r.ok, r.ok ? tr("导入安装完成：%1").arg(mod.name) : r.error);
        if (r.ok) writeHistorySnapshot();
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
        m_ckan->releaseInstaller();
    });
    watcher->setFuture(future);
}

void CKanManager::downloadSingleToCacheAsync(const ckan::CkanModule &mod)
{
    if (!m_ckan) { emit singleDownloadFinished(false, mod.identifier, tr("尚未绑定游戏实例")); return; }
    if (!mod.isValid()) { emit singleDownloadFinished(false, mod.identifier, tr("无效模组")); return; }
    // 已存在有效缓存则直接报告成功，无需重新下载
    if (!ckan::ModuleInstaller::findCacheZip(downloadDir(), mod).isEmpty()) {
        emit singleDownloadFinished(true, mod.identifier, QString());
        return;
    }
    clearWatchers();
    QDir().mkpath(downloadDir());
    const bool preferModeModuleMirrors =
        ConfigManager::instance().moduleDownloadSource() == ConfigManager::MirrorFirst;

    auto watcher = new QFutureWatcher<DownloadPhaseResult>(this);
    m_downloadWatcher = watcher;
    auto future = QtConcurrent::run([this, mod, preferModeModuleMirrors]() {
        return DownloadPhaseResult{ m_ckan->downloadModules(
                QVector<ckan::CkanModule>{mod}, downloadDir(),
                preferModeModuleMirrors, 1, nullptr, nullptr) };
    });
    connect(watcher, &QFutureWatcher<DownloadPhaseResult>::finished, this,
            [this, watcher, mod]() {
        if (m_downloadWatcher != watcher) { watcher->deleteLater(); return; }
        const DownloadPhaseResult r = watcher->result();
        watcher->deleteLater();
        if (m_downloadWatcher == watcher) m_downloadWatcher = nullptr;
        m_ckan->releaseInstaller();
        emit singleDownloadFinished(r.ok, mod.identifier, r.error);
    });
    watcher->setFuture(future);
}

void CKanManager::cancelCurrentOperation()
{
    m_indexCancelRequested.store(true);
    if (m_ckan)
        m_ckan->cancelInstall();
}

void CKanManager::resolveAndInstall(const QVector<ckan::CkanModule> &mods, bool autoRecommends,
                                    const QString &doneMessage)
{
    if (!m_ckan || mods.isEmpty()) return;
    // 安装前置决策（依赖解析 / 多提供者 / 级联建议 / 预卸载）下沉到 InstallService，
    // 门面只负责异步下载/安装编排与信号发布。
    m_install.setCompatRange(m_compatRange);
    const services::InstallService::ResolveResult res = m_install.resolveInstallSet(
        mods, autoRecommends, ConfigManager::instance().installSuggests());
    if (!res.ok) {
        emit operationFinished(false, res.cancelled ? tr("已取消") : res.error);
        return;
    }
    if (res.nothingToDo) { emit operationFinished(true, tr("无需操作")); return; }
    const QVector<ckan::CkanModule> modules = res.modulesToInstall;
    const QStringList preUninstall = res.preUninstall;

    clearWatchers();
    QDir().mkpath(downloadDir());

    // 磁盘空间预检（下载缓存盘）：按 downloadSize×1.15 估算所需字节数，
    // 不足时弹窗提示，用户可选择"忽略并继续"或"取消"。
    if (ConfigManager::instance().diskSpaceCheck() && !modules.isEmpty()) {
        const qint64 required = m_ckan->estimateRequiredBytes(modules);
        const QString path = downloadDir();
        const QStorageInfo storage(path);
        if (storage.isValid() && storage.bytesAvailable() >= 0
            && storage.bytesAvailable() < required) {
            const moddecision::DiskSpacePrompt prompt{
                true, path, storage.rootPath(), required, storage.bytesAvailable()};
            if (!m_decisions.diskSpace(prompt)) {
                emit operationFinished(false, tr("磁盘空间不足，已取消"));
                return;
            }
        }
    }

    // 阶段一（后台）：经 CKan 门面下载全部 zip 到缓存，
    // 门面内部同时按 zip 实际内容计算与手动占用的顶层文件夹冲突。
    const bool preferModuleMirrors =
        ConfigManager::instance().moduleDownloadSource() == ConfigManager::MirrorFirst;
    const int concurrency = ConfigManager::instance().downloadConcurrency();
    auto watcher = new QFutureWatcher<DownloadPhaseResult>(this);
    m_downloadWatcher = watcher;
    auto future = QtConcurrent::run([this, modules, preferModuleMirrors, concurrency]() {
        DownloadPhaseResult r;
        r.ok = m_ckan->downloadModules(modules, downloadDir(), preferModuleMirrors, concurrency,
                                       &r.conflicts, &r.error,
                                       [this](const QString &id, qint64 done, qint64 total, qint64 speed) {
                                           emit downloadProgress(id, done, total, speed);
                                       });
        return r;
    });
    connect(watcher, &QFutureWatcher<DownloadPhaseResult>::finished, this,
            [this, watcher, modules, doneMessage, preUninstall]() {
        if (m_downloadWatcher != watcher) { watcher->deleteLater(); return; } // 已被新操作/切实例取代
        const DownloadPhaseResult r = watcher->result();
        watcher->deleteLater();
        if (m_downloadWatcher == watcher) m_downloadWatcher = nullptr;
        if (!r.ok) {
            m_ckan->releaseInstaller();
            emit operationFinished(false, r.error);
            return;
        }
        // 阶段二前（UI 线程）：弹窗让用户选择冲突处理方式
        const moddecision::ConflictChoice choice = m_decisions.conflict(r.conflicts);
        if (choice.action == moddecision::ConflictAction::Cancel) {
            m_ckan->releaseInstaller();
            emit operationFinished(false, tr("已取消"));
            return;
        }
        // 磁盘空间预检（游戏盘）：下载完成后、安装写入前检查
        if (ConfigManager::instance().diskSpaceCheck() && !modules.isEmpty()) {
            const qint64 required = m_ckan->estimateRequiredBytes(modules);
            const QString path = m_ckan->gameDir();
            const QStorageInfo storage(path);
            if (storage.isValid() && storage.bytesAvailable() >= 0
                && storage.bytesAvailable() < required) {
                const moddecision::DiskSpacePrompt prompt{
                    false, path, storage.rootPath(), required, storage.bytesAvailable()};
                if (!m_decisions.diskSpace(prompt)) {
                    m_ckan->releaseInstaller();
                    emit operationFinished(false, tr("磁盘空间不足，已取消"));
                    return;
                }
            }
        }
        startInstallPhase(modules, choice.foldersToDelete, doneMessage, preUninstall);
    });
    watcher->setFuture(future);
}

void CKanManager::startInstallPhase(const QVector<ckan::CkanModule> &modules,
                                    const QStringList &foldersToDelete, const QString &doneMessage,
                                    const QStringList &preUninstall)
{
    if (!m_ckan || modules.isEmpty()) return;

    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, modules, foldersToDelete, preUninstall]() {
        // 单事务安装经 CKan 门面：先卸载旧版再安装新版，作为一个原子操作。
        // 任一步失败（含用户取消）整体回滚——恢复被删除的旧版文件、删除已写入的新文件、还原注册表。
        return m_ckan->installFromCache(modules, downloadDir(), foldersToDelete, preUninstall,
                                        [this](const QString &id, int percent) {
                                            emit installProgress(id, percent);
                                        });
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this,
            [this, watcher, doneMessage]() {
        if (m_installWatcher != watcher) { watcher->deleteLater(); return; } // 已被新操作/切实例取代
        const ckan::InstallResult r = watcher->result();
        m_ckan->reloadRegistry(); // 刷新已安装数据
        emit installedChanged();
        emit operationFinished(r.ok, r.ok ? doneMessage : r.error);
        if (r.ok) writeHistorySnapshot();
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
        m_ckan->releaseInstaller();
    });
    watcher->setFuture(future);
}
