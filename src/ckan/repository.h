#ifndef CKAN_REPOSITORY_H
#define CKAN_REPOSITORY_H

#include <QString>
#include "ckan_export.h"

namespace ckan {

// 仓库描述，对应 CKAN 的 Repository。
// 默认仓库为 KSP-CKAN/CKAN-meta 的 master tar.gz。
struct CKAN_API Repository {
    QString name;
    QString uri;
    int     priority = 0;
    bool    mirror   = false;
    QString comment;

    bool isValid() const { return !name.isEmpty() && !uri.isEmpty(); }

    static Repository defaultKspRepo();
    static QString defaultRepoUrl();
    static QString repositoryListUrl();
};

} // namespace ckan

#endif // CKAN_REPOSITORY_H