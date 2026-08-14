#include "repository.h"

namespace ckan {

QString Repository::defaultRepoUrl()
{
    return QStringLiteral("https://github.com/KSP-CKAN/CKAN-meta/archive/master.tar.gz");
}

QString Repository::repositoryListUrl()
{
    return QStringLiteral("https://raw.githubusercontent.com/KSP-CKAN/CKAN-meta/master/repositories.json");
}

Repository Repository::defaultKspRepo()
{
    Repository r;
    r.name = QStringLiteral("KSP-CKAN");
    r.uri  = defaultRepoUrl();
    return r;
}

} // namespace ckan