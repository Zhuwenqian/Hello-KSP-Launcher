#ifndef CKANMANAGER_H
#define CKANMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QFutureWatcher>
#include <QPair>
#include <QStorageInfo>
#include <atomic>

#include "ckan/ckan.h"

// 阶段一（下载全部 zip + 计算实际冲突）的后台返回结果
struct DownloadPhaseResult {
    bool        ok = false;
    QString     error;
    QStringList conflicts; // 与手动占用冲突的 GameData 顶层文件夹
};

// 启动器 <-> libckan 适配层单例。
// 负责：绑定当前实例、仓库索引异步刷新、mod 搜索/已安装查询、
//       安装/卸载/升级（后台线程执行）、全局下载缓存目录管理。
class CKanManager : public QObject
{
    Q_OBJECT
public:
    static CKanManager& instance();

    // ---- 实例绑定 ----
    void openInstance(const QString &gameDir, const QString &instanceName);
    void closeInstance();
    bool hasInstance() const { return m_ckan != nullptr; }
    QString gameDir() const;
    // 当前实例实际检测到的 KSP 版本（检测失败返回无效版本）
    ckan::GameVersion detectedVersion() const
    {
        return m_ckan ? m_ckan->detectedVersion() : ckan::GameVersion();
    }
    // 设置用户勾选的额外兼容区间（无效区间表示未启用）。
    // 安装/依赖解析时，候选兼容当前实例版本 或 兼容该区间（任一满足即可）。
    void setCompatRange(const ckan::GameVersionRange &r) { m_compatRange = r; }
    // 重新加载注册表：注册表文件被外部改动/删除后，刷新 libckan 内存中的已安装数据。
    void reloadRegistry();

    // ---- 缓存目录 ----
    QString cacheRoot() const;      // exe目录/ckan_cache
    QString downloadDir() const;    // 配置的下载缓存目录（默认为 cacheRoot/downloads）

    // 精确清理下载缓存：仅删除与已知模组（索引全部版本 + 已安装模组）对应的缓存 zip，
    // 目录中其他文件一律保留。返回删除的文件数。
    int cleanDownloadCache();

    // ---- 仓库索引 ----
    // force=true 时忽略本地缓存，强制重新下载（用户手动“刷新仓库”）。
    void refreshIndexAsync(bool force = false);
    bool indexReady() const;
    int  indexSize() const;
    QVector<ckan::CkanModule> search(const QString &query) const;
    QVector<ckan::CkanModule> versionsOf(const QString &identifier) const;
    ckan::CkanModule latestOf(const QString &identifier) const;
    // 某标识符的下载次数（来自仓库 download_counts.json）；无数据返回 -1
    int downloadCount(const QString &identifier) const;

    // ---- 已安装 ----
    QVector<ckan::InstalledModule> installedModules() const;
    QString installedVersion(const QString &identifier) const;
    bool isInstalled(const QString &identifier) const;
    // 仓库中存在更新版本（最新版 > 已装版）
    bool isUpgradable(const QString &identifier) const;

    // ---- 手动安装模组（DLL 扫描） ----
    // 后台线程扫描 GameData 下 .dll（排除官方目录），写入 registry.installedDlls 并保存。
    // 结果缓存：同一实例只扫描一次（后续进入无需重复全盘扫描）；同实例已扫描或在途则直接返回。
    void scanUnmanagedDllsAsync();
    // 当前实例的 DLL 扫描是否已完成（可用于显示"正在扫描已安装的 DLL"提示）
    bool unmanagedScanDone() const;
    // 该标识符是否被 DLL 扫描识别为手动安装模组
    bool isAutoDetected(const QString &identifier) const;

    // ---- 整合包导出 ----
    // 生成官方 CKAN 元包 JSON（depends 列出已安装模组，依赖优先）。
    // 无可导出模组时返回空并填充 error。
    QByteArray exportModpackCkan(QString *error = nullptr);

    // ---- 安装/卸载/升级（异步） ----
    void installAsync(const QString &identifier, bool autoRecommends);
    void uninstallAsync(const QString &identifier);
    void upgradeAsync(const QString &identifier);
    // 批量操作：内部按状态过滤（批量安装跳过已安装、批量升级仅可升级、批量卸载仅已安装）
    void installBatchAsync(const QStringList &identifiers);
    void upgradeBatchAsync(const QStringList &identifiers);
    void uninstallBatchAsync(const QStringList &identifiers);

    // 请求中止当前下载/安装任务（线程安全）。
    void cancelCurrentOperation();
    // 当前是否有进行中的下载/安装任务（用于显示/隐藏取消按钮）
    bool isInstalling() const { return m_installWatcher != nullptr || m_downloadWatcher != nullptr; }

signals:
    void indexRefreshed(bool ok, const QString &error);
    void installedChanged();
    void installProgress(const QString &identifier, int percent);
    // 下载字节进度：当前模组、已完成字节数、批量总字节数、实时速度(B/s)
    void downloadProgress(const QString &identifier, qint64 doneBytes,
                          qint64 totalBytes, qint64 speedBps);
    // 后台 DLL 扫描完成（仅当同一实例尚未扫描时才会发出）
    void unmanagedScanFinished();
    void operationFinished(bool ok, const QString &message);

private:
    explicit CKanManager(QObject *parent = nullptr);
    ~CKanManager();
    CKanManager(const CKanManager&) = delete;
    CKanManager& operator=(const CKanManager&) = delete;

    void resolveAndInstall(const QVector<ckan::CkanModule> &mods, bool autoRecommends,
                           const QString &doneMessage);
    // 冲突弹窗（3 选项），返回待删除文件夹；用户取消返回占位 "__CANCEL__"
    QStringList askFolderConflicts(const QStringList &conflicts);
    // 级联建议勾选弹窗；cancelled 输出用户是否取消（区别于"全都不选"）
    QVector<ckan::CkanModule> askSuggests(const QVector<ckan::CkanModule> &suggests,
                                          bool *cancelled);
    // 多提供者选择弹窗：每个虚拟包从候选提供者中选一个；
    // 返回所选提供者模块；取消时 cancelled=true 并返回空
    QVector<ckan::CkanModule> askProviders(const QVector<ckan::ProviderChoice> &choices,
                                           bool *cancelled);
    // 安装阶段：前置卸载 + 从缓存安装（单事务，全部经 CKan 门面）
    void startInstallPhase(const QVector<ckan::CkanModule> &modules,
                           const QStringList &foldersToDelete, const QString &doneMessage,
                           const QStringList &preUninstall);
    // 磁盘空间不足警告弹窗：显示所需/可用空间，用户可选择"忽略并继续"或"取消"。
    // forDownload=true 表示检查的是下载缓存盘，false 表示游戏盘。返回 true 表示忽略继续。
    bool askDiskSpaceWarning(const QStorageInfo &storage, qint64 required,
                             const QString &path, bool forDownload);
    void clearWatchers();
    // 放弃当前实例：先取消并等待在途后台任务（扫描/索引/下载/安装）全部结束，
    // 再释放 m_ckan，避免工作线程在 m_ckan 被删除后继续访问（use-after-free）。
    void discardCurrentInstance();

    ckan::CKan *m_ckan = nullptr;
    QString m_instanceName;
    ckan::GameVersionRange m_compatRange; // 用户勾选的额外兼容区间（无效表示未启用）
    QStringList m_indexMirrorPrefixes;  // 索引下载镜像前缀（拼接在仓库自身 GitHub URL 前）
    QStringList m_moduleMirrorPrefixes; // 模组下载镜像前缀（拼接在官方下载 URL 前）

    QFutureWatcher<QPair<bool, QString>>  *m_indexWatcher = nullptr;
    QFutureWatcher<DownloadPhaseResult>   *m_downloadWatcher = nullptr;
    QFutureWatcher<ckan::InstallResult>   *m_installWatcher = nullptr;
    QFutureWatcher<void>                  *m_scanWatcher = nullptr; // DLL 扫描在途
    std::atomic_bool m_indexCancelRequested{false};
    // 索引刷新是否在途（防重入：刷新进行中时忽略重复的刷新请求，
    // 避免多个后台线程同时写 CKan::m_index 造成数据竞争）。
    std::atomic_bool m_indexRefreshing{false};
};

#endif // CKANMANAGER_H