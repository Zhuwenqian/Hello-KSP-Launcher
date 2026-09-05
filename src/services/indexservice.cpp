#include "indexservice.h"

#include "ckan/version.h"

namespace services {

bool IndexService::isNewerVersion(const QString &latest, const QString &installed)
{
    return ckan::ModuleVersion(latest) > ckan::ModuleVersion(installed);
}

bool IndexService::isUpgradable(const QString &identifier) const
{
    if (!m_ckan || !m_ckan->indexReady()) return false;
    const QString installed = m_ckan->installedVersion(identifier);
    if (installed.isEmpty()) return false;
    const ckan::CkanModule latest = m_ckan->latestOf(identifier);
    if (!latest.isValid()) return false;
    return isNewerVersion(latest.version, installed);
}

} // namespace services