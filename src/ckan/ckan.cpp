#include "ckan.h"

#include "repoindex.h"
#include "relationshipresolver.h"
#include "moduleinstaller.h"
#include "downloader.h"

namespace ckan {

CKan::CKan(const QString &gameDir, const QString &instanceName)
    : m_instance(gameDir, instanceName)
{
}

bool CKan::refreshIndex(const QStringList &mirrors, QString *error, bool force,
                        qint64 maxAgeSecs, bool preferMirror,
                        const std::function<void(qint64, qint64)> &onProgress,
                        std::atomic_bool *cancelFlag)
{
    const Repository repo = Repository::defaultKspRepo();
    m_indexReady = RepoIndex::buildCached(repo, mirrors, &m_index, error, force,
                                          maxAgeSecs, onProgress, cancelFlag, preferMirror);
    return m_indexReady;
}

QVector<CkanModule> CKan::search(const QString &query) const
{
    QVector<CkanModule> out;
    const QString q = query.trimmed().toLower();
    for (auto it = m_index.constBegin(); it != m_index.constEnd(); ++it) {
        // 必须按版本排序取最新，不能取 m_index 中首个（tar 字母序）版本
        const CkanModule latest = RepoIndex::latestFor(m_index, it.key());
        if (!latest.isValid()) continue;
        if (q.isEmpty()
            || latest.identifier.toLower().contains(q)
            || latest.name.toLower().contains(q)
            || latest.abstract.toLower().contains(q)) {
            out.append(latest);
        }
    }
    // 按名称排序
    std::sort(out.begin(), out.end(), [](const CkanModule &a, const CkanModule &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return out;
}

QVector<CkanModule> CKan::versionsOf(const QString &identifier) const
{
    return RepoIndex::versionsFor(m_index, identifier);
}

CkanModule CKan::latestOf(const QString &identifier) const
{
    return RepoIndex::latestFor(m_index, identifier);
}

QStringList CKan::allIdentifiers() const
{
    return m_index.keys();
}

ResolutionResult CKan::resolveInstall(const CkanModule &mod, bool autoInstallRecommends)
{
    QVector<CkanModule> toInstall;
    toInstall.append(mod);
    return resolveInstallMany(toInstall, autoInstallRecommends);
}

ResolutionResult CKan::resolveInstallMany(const QVector<CkanModule> &mods,
                                          bool autoInstallRecommends)
{
    RelationshipResolver resolver(m_index);
    return resolver.resolve(mods, *registry(), autoInstallRecommends);
}

InstallResult CKan::install(const QVector<CkanModule> &modules, const QString &downloadDir,
                            const QStringList &mirrorPrefixes, bool preferModuleMirrors)
{
    ModuleInstaller installer(&m_instance);
    return installer.install(modules, downloadDir, {}, mirrorPrefixes, preferModuleMirrors);
}

InstallResult CKan::uninstall(const QString &identifier)
{
    ModuleInstaller installer(&m_instance);
    return installer.uninstall(identifier);
}

void CKan::setProxyUrl(const QString &proxyUrl)
{
    Downloader::setProxyUrl(proxyUrl);
}

QVector<InstalledModule> CKan::installedModules() const
{
    QVector<InstalledModule> out;
    const Registry *reg = m_instance.registry();
    for (auto it = reg->installedModules.constBegin();
         it != reg->installedModules.constEnd(); ++it)
        out.append(it.value());
    return out;
}

} // namespace ckan