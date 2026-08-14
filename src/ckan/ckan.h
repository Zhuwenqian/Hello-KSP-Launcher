#ifndef CKAN_CKAN_H
#define CKAN_CKAN_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <functional>
#include <atomic>

#include "ckan_export.h"
#include "ckanmodule.h"
#include "installedmodule.h"
#include "registry.h"
#include "repository.h"
#include "gameinstance.h"
#include "version.h"
#include "relationshipresolver.h"
#include "moduleinstaller.h"

namespace ckan {

// libckan 顶层门面：为启动器提供统一、简洁的接口。
// 封装仓库索引、注册表、依赖解析、安装/卸载。
class CKAN_API CKan
{
public:
    explicit CKan(const QString &gameDir, const QString &instanceName);

    // ---- 实例与注册表 ----
    GameInstance *instance() { return &m_instance; }
    const GameInstance *instance() const { return &m_instance; }
    Registry *registry() { return m_instance.registry(); }

    // ---- 仓库索引（mod 列表） ----
    // 从仓库下载并建立索引。mirrors 为镜像列表。
    // force=true 时忽略本地缓存，强制重新下载；否则使用缓存（见 RepoIndex::buildCached）。
    // onProgress 下载进度回调，cancelFlag 置真则中止索引下载。
    bool refreshIndex(const QStringList &mirrors = {}, QString *error = nullptr,
                      bool force = false,
                      const std::function<void(qint64, qint64)> &onProgress = {},
                      std::atomic_bool *cancelFlag = nullptr);
    QVector<CkanModule> search(const QString &query) const;    // 按名称/标识符搜索
    QVector<CkanModule> versionsOf(const QString &identifier) const;
    CkanModule latestOf(const QString &identifier) const;
    bool indexReady() const { return m_indexReady; }
    int  indexSize() const { return static_cast<int>(m_index.size()); }

    // ---- 安装相关 ----
    // 解析安装某模块所需的完整集合（含依赖）
    ResolutionResult resolveInstall(const CkanModule &mod, bool autoInstallRecommends = true);
    // 一次性解析多个模块的完整安装集（含相互依赖，用于批量安装）
    ResolutionResult resolveInstallMany(const QVector<CkanModule> &mods,
                                        bool autoInstallRecommends = true);
    // 执行安装（downloadDir 为 zip 缓存目录）
    InstallResult install(const QVector<CkanModule> &modules, const QString &downloadDir,
                          const QStringList &mirrors = {});
    InstallResult uninstall(const QString &identifier);

    // ---- 镜像/代理配置 ----
    static void setProxyUrl(const QString &proxyUrl);

    // 已安装模块列表
    QVector<InstalledModule> installedModules() const;

private:
    GameInstance m_instance;
    QMap<QString, QVector<CkanModule>> m_index;
    bool m_indexReady = false;
};

} // namespace ckan

#endif // CKAN_CKAN_H