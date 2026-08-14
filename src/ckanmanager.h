#ifndef CKANMANAGER_H
#define CKANMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QFutureWatcher>
#include <QPair>
#include <atomic>

#include "ckan/ckan.h"

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

    // ---- 缓存目录 ----
    QString cacheRoot() const;      // exe目录/ckan_cache
    QString downloadDir() const;    // cacheRoot/downloads

    // ---- 仓库索引 ----
    // force=true 时忽略本地缓存，强制重新下载（用户手动“刷新仓库”）。
    void refreshIndexAsync(bool force = false);
    bool indexReady() const;
    int  indexSize() const;
    QVector<ckan::CkanModule> search(const QString &query) const;
    QVector<ckan::CkanModule> versionsOf(const QString &identifier) const;
    ckan::CkanModule latestOf(const QString &identifier) const;

    // ---- 已安装 ----
    QVector<ckan::InstalledModule> installedModules() const;
    QString installedVersion(const QString &identifier) const;
    bool isInstalled(const QString &identifier) const;
    // 仓库中存在更新版本（最新版 > 已装版）
    bool isUpgradable(const QString &identifier) const;

    // ---- 手动安装模组（DLL 扫描） ----
    // 扫描 GameData 下 .dll（排除官方目录），写入 registry.installedDlls 并保存
    void scanUnmanagedDlls();
    // 该标识符是否被 DLL 扫描识别为手动安装模组
    bool isAutoDetected(const QString &identifier) const;

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
    // 当前是否有进行中的安装任务（用于显示/隐藏取消按钮）
    bool isInstalling() const { return m_installWatcher != nullptr; }

signals:
    void indexRefreshed(bool ok, const QString &error);
    void installedChanged();
    void installProgress(const QString &identifier, int percent);
    // 下载字节进度：当前模组、已完成字节数、批量总字节数、实时速度(B/s)
    void downloadProgress(const QString &identifier, qint64 doneBytes,
                          qint64 totalBytes, qint64 speedBps);
    void operationFinished(bool ok, const QString &message);

private:
    explicit CKanManager(QObject *parent = nullptr);
    ~CKanManager();
    CKanManager(const CKanManager&) = delete;
    CKanManager& operator=(const CKanManager&) = delete;

    void runInstall(const QVector<ckan::CkanModule> &modules, const QString &doneMessage,
                const QStringList &preUninstall = {});
    void resolveAndInstall(const QVector<ckan::CkanModule> &mods, bool autoRecommends,
                           const QString &doneMessage);
    void clearWatchers();

    ckan::CKan *m_ckan = nullptr;
    QString m_instanceName;
    QStringList m_mirrors;

    ckan::ModuleInstaller *m_installer = nullptr;
    QFutureWatcher<QPair<bool, QString>>  *m_indexWatcher = nullptr;
    QFutureWatcher<ckan::InstallResult>   *m_installWatcher = nullptr;
    std::atomic_bool m_indexCancelRequested{false};
};

#endif // CKANMANAGER_H