#include "cacheservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>

#include "ckan/ckan.h"
#include "configmanager.h"

namespace services {

QString CacheService::cacheRoot() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ckan_cache"));
}

QString CacheService::downloadDir() const
{
    const QString cfg = ConfigManager::instance().downloadCacheDir().trimmed();
    if (!cfg.isEmpty())
        return cfg;
    return QDir(cacheRoot()).filePath(QStringLiteral("downloads"));
}

QStringList CacheService::knownCacheFileNames(const QString &id, const QString &version,
                                              const QString &url)
{
    QStringList names;
    if (id.isEmpty() || version.isEmpty())
        return names;
    // 兼容三种命名：官方 CKAN <hash8>-<identifier>-<version>.zip、
    // 手动下载 <identifier>-<version>.zip 与本启动器 <identifier>_<safeVersion>.zip
    names << QStringLiteral("%1_%2.zip").arg(id, ckan::CKan::safeCacheFileName(version));
    names << ckan::CKan::officialCacheFileName(id, version, url);
    names << ckan::CKan::officialCacheFileName(id, version);
    return names;
}

int CacheService::cleanDownloadCache() const
{
    const QString dir = downloadDir();
    QDir d(dir);
    if (!d.exists())
        return 0;

    // 收集所有已知模组缓存文件名（精确匹配，绝不误删其他文件）
    QSet<QString> knownFiles;
    auto addModule = [&knownFiles](const QString &id, const QString &version, const QString &url) {
        for (const QString &name : knownCacheFileNames(id, version, url))
            knownFiles.insert(name);
    };

    if (m_ckan) {
        if (m_ckan->indexReady()) {
            const QStringList ids = m_ckan->allIdentifiers();
            for (const QString &id : ids) {
                const auto versions = m_ckan->versionsOf(id);
                for (const ckan::CkanModule &m : versions) {
                    const QString url = m.downloadUrls.isEmpty() ? QString() : m.downloadUrls.first();
                    addModule(m.identifier, m.version, url);
                }
            }
        }
        const auto inst = m_ckan->installedModules();
        for (const ckan::InstalledModule &im : inst) {
            const QString url = im.module.downloadUrls.isEmpty() ? QString() : im.module.downloadUrls.first();
            addModule(im.identifier, im.module.version, url);
        }
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

} // namespace services