#ifndef SERVICES_CACHESERVICE_H
#define SERVICES_CACHESERVICE_H

#include <QString>
#include <QStringList>

namespace ckan { class CKan; }

namespace services {

// 下载缓存服务：下载/索引缓存目录的定位与精确清理。
// 不持后台线程，仅需在引用其 ckan::CKan 有效的前提于 UI 线程调用；
// ckan::CKan 生命周期由 CKanManager 门面托管（setCkan 注入/置空）。
class CacheService
{
public:
    void setCkan(ckan::CKan *ckan) { m_ckan = ckan; }

    // 缓存根目录（exe 目录/ckan_cache）
    QString cacheRoot() const;
    // 下载缓存目录（用户配置优先，否则为 cacheRoot/downloads）
    QString downloadDir() const;

    // 精确清理下载缓存：仅删除与已知模组（索引全部版本 + 已安装模组）对应的缓存 zip，
    // 目录中其他文件一律保留。返回删除的文件数。
    int cleanDownloadCache() const;

    // 单个模组在缓存中可能存在的全部 zip 文件名（官方三种命名兼容）。
    // id/version 为空时返回空列表（不作为已知文件参与清理）。
    static QStringList knownCacheFileNames(const QString &id, const QString &version,
                                           const QString &url);

private:
    ckan::CKan *m_ckan = nullptr;
};

} // namespace services

#endif // SERVICES_CACHESERVICE_H