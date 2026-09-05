#ifndef SERVICES_SCANSERVICE_H
#define SERVICES_SCANSERVICE_H

#include <QString>

#include "ckan/ckan.h"

namespace services {

// 手动安装模组（DLL 扫描，AD）服务：封装 ckan::CKan 的扫描执行与其结果查询。
// 扫描本体在后台线程执行（由 CKanManager 门面用 QtConcurrent 调度），
// 本服务仅提供同步的底层操作与只读查询；引用的 ckan::CKan 生命周期由门面托管。
class ScanService
{
public:
    void setCkan(ckan::CKan *ckan) { m_ckan = ckan; }

    // 执行一次 DLL 全盘扫描并写入注册表；结果缓存于 ckan::CKan 内。
    void scanUnmanagedDlls() const { if (m_ckan) m_ckan->scanUnmanagedDlls(); }
    // 当前实例的 DLL 扫描是否已完成（用于页面提示"正在扫描/已就绪"）
    bool unmanagedScanDone() const { return m_ckan && m_ckan->dllsScanned(); }
    // 该标识符是否被 DLL 扫描识别为手动安装模组
    bool isAutoDetected(const QString &identifier) const
    {
        return m_ckan && m_ckan->isAutoDetected(identifier);
    }
    // 手动安装（AD）模组的已装版本（尽力推导，可能为空）
    QString autoDetectedVersion(const QString &identifier) const
    {
        return m_ckan ? m_ckan->autoDetectedVersion(identifier) : QString();
    }

private:
    ckan::CKan *m_ckan = nullptr;
};

} // namespace services

#endif // SERVICES_SCANSERVICE_H