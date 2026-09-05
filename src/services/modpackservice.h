#ifndef SERVICES_MODPACKSERVICE_H
#define SERVICES_MODPACKSERVICE_H

#include <QByteArray>
#include <QString>

namespace ckan { class CKan; }

namespace services {

// 整合包服务：官方 CKAN 元包导出与安装历史快照写入。
// 均为对 ckan::CKan 的薄封装，供门面/ModsController 复用；生命周期由门面托管。
class ModpackService
{
public:
    void setCkan(ckan::CKan *ckan) { m_ckan = ckan; }

    // 生成官方 CKAN 元包 JSON（depends 列出已安装模组，依赖优先）。
    // 无可导出模组时返回空并填充 error。
    QByteArray exportCkan(QString *error) const
    {
        return m_ckan ? m_ckan->exportModpackCkan(error) : QByteArray();
    }
    // 提交安装成功后生成安装历史快照（尽力而为，失败静默）
    void writeHistorySnapshot() const
    {
        if (m_ckan) m_ckan->writeHistorySnapshot();
    }

private:
    ckan::CKan *m_ckan = nullptr;
};

} // namespace services

#endif // SERVICES_MODPACKSERVICE_H