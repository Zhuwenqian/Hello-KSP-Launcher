#include "ckanmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QSet>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

#include "ckan/repoindex.h"
#include "ckan/moduleinstaller.h"
#include "configmanager.h"

CKanManager::CKanManager(QObject *parent)
    : QObject(parent)
{
    // 索引镜像：完整 CKAN-meta 仓库 URL（官方 GitHub 优先，镜像回退）
    m_indexMirrors = {
        QStringLiteral("https://gh-proxy.com/https://github.com/KSP-CKAN/CKAN-meta/archive/master.tar.gz"),
        QStringLiteral("https://ghfast.top/https://github.com/KSP-CKAN/CKAN-meta/archive/master.tar.gz"),
    };
    // 模组下载镜像前缀：拼接在官方下载 URL 前（gh 代理，可代理任意 GitHub 资源）
    m_moduleMirrorPrefixes = {
        QStringLiteral("https://gh-proxy.com/"),
        QStringLiteral("https://ghfast.top/"),
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
    if (m_downloadWatcher) { m_downloadWatcher->deleteLater(); m_downloadWatcher = nullptr; }
    if (m_installWatcher) { m_installWatcher->deleteLater(); m_installWatcher = nullptr; }
    if (m_installer) { m_installer->deleteLater(); m_installer = nullptr; }
}

// 安装任务结束后的统一清理（保留索引 watcher）
void CKanManager::cleanupInstaller()
{
    if (m_downloadWatcher) { m_downloadWatcher->deleteLater(); m_downloadWatcher = nullptr; }
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
    const QString cfg = ConfigManager::instance().downloadCacheDir().trimmed();
    if (!cfg.isEmpty())
        return cfg;
    return QDir(cacheRoot()).filePath(QStringLiteral("downloads"));
}

int CKanManager::cleanDownloadCache()
{
    const QString dir = downloadDir();
    QDir d(dir);
    if (!d.exists())
        return 0;

    // 收集所有已知模组缓存文件名（精确匹配，绝不误删其他文件）
    // 缓存名格式：<identifier>_<safeVersion>.zip
    QSet<QString> knownFiles;
    auto addModule = [&knownFiles](const QString &id, const QString &version) {
        if (id.isEmpty() || version.isEmpty()) return;
        knownFiles.insert(QStringLiteral("%1_%2.zip")
                              .arg(id, ckan::ModuleInstaller::safeCacheFileName(version)));
    };

    if (m_ckan) {
        if (m_ckan->indexReady()) {
            const QStringList ids = m_ckan->allIdentifiers();
            for (const QString &id : ids) {
                const auto versions = m_ckan->versionsOf(id);
                for (const ckan::CkanModule &m : versions)
                    addModule(m.identifier, m.version);
            }
        }
        const auto inst = m_ckan->installedModules();
        for (const ckan::InstalledModule &im : inst)
            addModule(im.identifier, im.module.version);
    }

    if (knownFiles.isEmpty())
        return 0; // 无已知模组，无法精确识别，不删除任何文件

    int removed = 0;
    const QStringList entries = d.entryList(QDir::Files, QDir::Name);
    for (const QString &name : entries) {
        if (knownFiles.contains(name) && QFile::remove(d.filePath(name)))
            ++removed;
    }
    return removed;
}

void CKanManager::refreshIndexAsync(bool force)
{
    if (!m_ckan) { emit indexRefreshed(false, tr("尚未绑定游戏实例")); return; }
    clearWatchers();

    // 索引缓存目录：exe目录/ckan_cache/index
    ckan::RepoIndex::setCacheDir(QDir(cacheRoot()).filePath(QStringLiteral("index")));
    m_indexCancelRequested.store(false);

    // 从配置读取缓存有效期与镜像偏好
    const qint64 maxAgeSecs = ConfigManager::instance().indexRefreshIntervalSecs();
    const bool preferMirror =
        ConfigManager::instance().indexDownloadSource() == ConfigManager::MirrorFirst;

    auto watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    m_indexWatcher = watcher;
    auto future = QtConcurrent::run([this, force, maxAgeSecs, preferMirror]() {
        QString err;
        const bool ok = m_ckan->refreshIndex(m_indexMirrors, &err, force, maxAgeSecs,
            preferMirror,
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

void CKanManager::scanUnmanagedDlls()
{
    if (!m_ckan) return;
    ckan::GameInstance *inst = m_ckan->instance();
    inst->registry()->installedDlls = inst->scanUnmanagedDlls();
    inst->saveRegistry();
}

bool CKanManager::isAutoDetected(const QString &identifier) const
{
    return m_ckan && m_ckan->instance()->registry()->installedDlls.contains(identifier);
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
        const QString list = r.installedIdentifiers.join(QLatin1Char(','));
        emit operationFinished(r.ok, r.ok ? tr("已卸载：%1").arg(list.isEmpty() ? QString() : list)
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

    const QVector<ckan::CkanModule> modules = res.modulesToInstall;
    if (modules.isEmpty()) { emit operationFinished(true, tr("无需操作")); return; }

    // 已安装但需更新的：安装前先卸载旧版本
    QStringList preUninstall;
    for (const ckan::CkanModule &m : mods)
        if (isInstalled(m.identifier)) preUninstall.append(m.identifier);

    clearWatchers();
    QDir().mkpath(downloadDir());
    m_installer = new ckan::ModuleInstaller(m_ckan->instance(), this);
    connect(m_installer, &ckan::ModuleInstaller::installProgress,
            this, &CKanManager::installProgress);
    connect(m_installer, &ckan::ModuleInstaller::byteProgress,
            this, &CKanManager::downloadProgress);

    // 阶段一（后台）：下载全部 zip 到缓存，并读取 zip 实际内容计算与手动占用的冲突
    const bool preferModuleMirrors =
        ConfigManager::instance().moduleDownloadSource() == ConfigManager::MirrorFirst;
    auto watcher = new QFutureWatcher<DownloadPhaseResult>(this);
    m_downloadWatcher = watcher;
    auto future = QtConcurrent::run([this, modules, preferModuleMirrors]() {
        DownloadPhaseResult r;
        r.ok = m_installer->downloadModules(modules, downloadDir(),
                                            m_moduleMirrorPrefixes, preferModuleMirrors, &r.error);
        if (r.ok)
            r.conflicts = computeActualFolderConflicts(modules, downloadDir());
        return r;
    });
    connect(watcher, &QFutureWatcher<DownloadPhaseResult>::finished, this,
            [this, watcher, modules, doneMessage, preUninstall]() {
        const DownloadPhaseResult r = watcher->result();
        watcher->deleteLater();
        if (m_downloadWatcher == watcher) m_downloadWatcher = nullptr;
        if (!r.ok) {
            cleanupInstaller();
            emit operationFinished(false, r.error);
            return;
        }
        // 阶段二前（UI 线程）：弹窗让用户选择冲突处理方式
        const QStringList foldersToDelete = askFolderConflicts(r.conflicts);
        if (foldersToDelete.size() == 1 && foldersToDelete.at(0) == QStringLiteral("__CANCEL__")) {
            cleanupInstaller();
            emit operationFinished(false, tr("已取消"));
            return;
        }
        startInstallPhase(modules, foldersToDelete, doneMessage, preUninstall);
    });
    watcher->setFuture(future);
}

// 手动占用的 GameData 顶层文件夹（相对 GameDir，如 "GameData/SomeMod"）
QStringList CKanManager::currentManualGameDataFolders() const
{
    return m_ckan ? m_ckan->instance()->manualGameDataFolders() : QStringList();
}

// 以 zip 实际内容为准，与手动占用文件夹比对，返回冲突的顶层文件夹（已排序去重）
QStringList CKanManager::computeActualFolderConflicts(const QVector<ckan::CkanModule> &modules,
                                                      const QString &downloadDir) const
{
    const QStringList manual = currentManualGameDataFolders();
    if (manual.isEmpty()) return QStringList();
    const QSet<QString> manualSet = QSet<QString>(manual.begin(), manual.end());

    QSet<QString> conflictSet;
    QStringList conflicts;
    for (const ckan::CkanModule &m : modules) {
        if (m.isMetapackage()) continue;
        const QString zipPath = downloadDir + QLatin1Char('/') + m.identifier + QLatin1Char('_')
                              + ckan::ModuleInstaller::safeCacheFileName(m.version) + QStringLiteral(".zip");
        QString err;
        const QStringList fols = ckan::ModuleInstaller::actualGameDataFolders(zipPath, m, &err);
        for (const QString &f : fols) {
            if (manualSet.contains(f) && !conflictSet.contains(f)) {
                conflictSet.insert(f);
                conflicts << f;
            }
        }
    }
    std::sort(conflicts.begin(), conflicts.end());
    return conflicts;
}

// 冲突弹窗（3 选项）：全部覆盖 / 全部删除旧的保留新的 / 取消。
// 返回待删除文件夹；用户取消返回占位 "__CANCEL__"；无冲突返回空。
QStringList CKanManager::askFolderConflicts(const QStringList &conflicts)
{
    if (conflicts.isEmpty()) return QStringList();

    QString list;
    for (const QString &c : conflicts) list += QStringLiteral("GameData/") + c + QLatin1Char('\n');
    QMessageBox box;
    box.setWindowTitle(tr("发现文件夹冲突"));
    box.setText(tr("下载完成后检查到以下文件夹已被手动安装的模组占用：\n\n%1\n\n请选择处理方式：").arg(list.trimmed()));
    QAbstractButton *allCover  = box.addButton(tr("全部覆盖（保留额外文件）"), QMessageBox::AcceptRole);
    QAbstractButton *allDelete = box.addButton(tr("全部删除旧的保留新的"), QMessageBox::DestructiveRole);
    QAbstractButton *cancel    = box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.exec();
    QAbstractButton *clicked = box.clickedButton();
    if (clicked == cancel)    return QStringList{QStringLiteral("__CANCEL__")};
    if (clicked == allDelete) return conflicts; // 全部删除旧的保留新的
    return QStringList();                       // 全部覆盖：不删除任何文件夹
}

void CKanManager::startInstallPhase(const QVector<ckan::CkanModule> &modules,
                                    const QStringList &foldersToDelete, const QString &doneMessage,
                                    const QStringList &preUninstall)
{
    if (!m_ckan || modules.isEmpty()) return;

    auto watcher = new QFutureWatcher<ckan::InstallResult>(this);
    m_installWatcher = watcher;
    auto future = QtConcurrent::run([this, modules, foldersToDelete, preUninstall]() {
        for (const QString &id : preUninstall)
            m_ckan->uninstall(id);
        return m_installer->installFromCache(modules, downloadDir(), foldersToDelete);
    });
    connect(watcher, &QFutureWatcher<ckan::InstallResult>::finished, this,
            [this, watcher, doneMessage]() {
        const ckan::InstallResult r = watcher->result();
        m_ckan->instance()->loadRegistry(); // 刷新已安装数据
        emit installedChanged();
        emit operationFinished(r.ok, r.ok ? doneMessage : r.error);
        watcher->deleteLater();
        if (m_installWatcher == watcher) m_installWatcher = nullptr;
        cleanupInstaller();
    });
    watcher->setFuture(future);
}