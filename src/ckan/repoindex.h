#ifndef CKAN_REPOINDEX_H
#define CKAN_REPOINDEX_H

#include <QByteArray>
#include <QString>
#include <QMap>
#include <QVector>
#include <functional>
#include <atomic>

#include "ckan_export.h"
#include "ckanmodule.h"
#include "repository.h"

namespace ckan {

// 仓库索引：下载 CKAN-meta tar.gz，解压并建立 identifier -> 模块列表 索引。
// 支持将下载的 tar.gz 落盘缓存，避免每次启动都重新下载。
class CKAN_API RepoIndex
{
public:
    // 索引缓存默认有效期（秒）
    static constexpr qint64 kDefaultCacheAgeSecs = 6 * 60 * 60;

    // 设置索引缓存的根目录（下载的 tar.gz 会保存到 该目录/<repo名>.tar.gz）。
    // 留空则不做落盘缓存。
    static void setCacheDir(const QString &dir);
    static QString cacheDir();

    // 从 tar.gz 内存数据解析所有 .ckan 文件
    static bool parseTarGz(const QByteArray &tarGz, QMap<QString, QVector<CkanModule>> *index,
                           QString *error = nullptr);

    // 从仓库下载并建立索引（每次都会下载）。
    // onProgress：下载进度回调(received, total)，cancelFlag 置真则中止并返回失败。
    static bool build(const Repository &repo, const QStringList &mirrors,
                      QMap<QString, QVector<CkanModule>> *index, QString *error = nullptr,
                      const std::function<void(qint64, qint64)> &onProgress = {},
                      std::atomic_bool *cancelFlag = nullptr);

    // 带缓存的构建：优先使用缓存（fresh 且未强制刷新时），否则下载并写入缓存。
    // maxAgeSecs 为缓存有效期（秒），默认 6 小时。
    static bool buildCached(const Repository &repo, const QStringList &mirrors,
                            QMap<QString, QVector<CkanModule>> *index, QString *error = nullptr,
                            bool forceRefresh = false, qint64 maxAgeSecs = kDefaultCacheAgeSecs,
                            const std::function<void(qint64, qint64)> &onProgress = {},
                            std::atomic_bool *cancelFlag = nullptr);

    // 便捷：取某 identifier 的全部版本（按版本降序）
    static QVector<CkanModule> versionsFor(const QMap<QString, QVector<CkanModule>> &index,
                                           const QString &identifier);
    // 取某 identifier 的最新版本
    static CkanModule latestFor(const QMap<QString, QVector<CkanModule>> &index,
                                const QString &identifier);
};

} // namespace ckan

#endif // CKAN_REPOINDEX_H