#ifndef SERVICES_INDEXSERVICE_H
#define SERVICES_INDEXSERVICE_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "ckan/ckan.h"
#include "ckan/ckanmodule.h"

namespace services {

// 仓库索引与模组检索服务：封装对 ckan::CKan 索引数据的只读查询。
// 不持后台线程，仅需在引用其 ckan::CKan 有效的前提下于 UI 线程调用；
// 所引用的 ckan::CKan 生命周期由 CKanManager 门面托管（setCkan/openInstance 时注入）。
class IndexService
{
public:
    void setCkan(ckan::CKan *ckan) { m_ckan = ckan; }

    bool indexReady() const { return m_ckan && m_ckan->indexReady(); }
    int indexSize() const { return m_ckan ? m_ckan->indexSize() : 0; }
    QVector<ckan::CkanModule> search(const QString &query) const
    {
        return m_ckan ? m_ckan->search(query) : QVector<ckan::CkanModule>();
    }
    QVector<ckan::CkanModule> versionsOf(const QString &identifier) const
    {
        return m_ckan ? m_ckan->versionsOf(identifier) : QVector<ckan::CkanModule>();
    }
    ckan::CkanModule latestOf(const QString &identifier) const
    {
        return m_ckan ? m_ckan->latestOf(identifier) : ckan::CkanModule();
    }
    int downloadCount(const QString &identifier) const
    {
        return m_ckan ? m_ckan->downloadCount(identifier) : -1;
    }
    QStringList allIdentifiers() const
    {
        return m_ckan ? m_ckan->allIdentifiers() : QStringList();
    }
    ckan::GameVersion detectedVersion() const
    {
        return m_ckan ? m_ckan->detectedVersion() : ckan::GameVersion();
    }

    // latest 是否严格大于 installed（版本段补齐比较，用于判断"存在更新"）。
    static bool isNewerVersion(const QString &latest, const QString &installed);
    // 仓库中存在比当前已装版本更新的版本（无索引 / 未安装 / 最新版无效均返回 false）。
    bool isUpgradable(const QString &identifier) const;

private:
    ckan::CKan *m_ckan = nullptr;
};

} // namespace services

#endif // SERVICES_INDEXSERVICE_H