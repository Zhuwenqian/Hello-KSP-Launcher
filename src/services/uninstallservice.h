#ifndef SERVICES_UNINSTALLSERVICE_H
#define SERVICES_UNINSTALLSERVICE_H

#include <QString>
#include <QStringList>

namespace ckan { class CKan; }

namespace services {

// 卸载服务：卸载计划的级联顺序与"已安装"过滤等只读决策。
// 卸载本体（后台、单事务、可取消回滚）由门面经 QtConcurrent 调度；本服务仅同步辅助。
class UninstallService
{
public:
    void setCkan(ckan::CKan *ckan) { m_ckan = ckan; }

    // 只读：一次性卸载 identifiers 的级联顺序（含目标，依赖者在前）；任一未安装返回空。
    QStringList uninstallPlan(const QStringList &identifiers) const
    {
        return m_ckan ? m_ckan->uninstallPlan(identifiers) : QStringList();
    }
    // 过滤出其中已安装的标识符（批量卸载仅处理已安装）
    QStringList filterInstalled(const QStringList &identifiers) const
    {
        QStringList out;
        if (!m_ckan) return out;
        for (const QString &id : identifiers)
            if (m_ckan->isInstalled(id)) out << id;
        return out;
    }

private:
    ckan::CKan *m_ckan = nullptr;
};

} // namespace services

#endif // SERVICES_UNINSTALLSERVICE_H