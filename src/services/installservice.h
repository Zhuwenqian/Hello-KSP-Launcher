#ifndef SERVICES_INSTALLSERVICE_H
#define SERVICES_INSTALLSERVICE_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "ckan/ckan.h"
#include "moddecision.h"

namespace services {

// 安装前置决策服务：依赖解析、多提供者/级联建议选择、预卸载计算。
// 不含后台线程与 QObject 信号；进度由门面经回调转发，
// 弹窗经注入的 moddecision::Hooks 解耦（业务层不直接弹窗）。可脱离 UI 单测。
class InstallService
{
public:
    void setCkan(ckan::CKan *ckan) { m_ckan = ckan; }
    void setDecisions(const moddecision::Hooks &hooks) { m_decisions = hooks; }
    void setCompatRange(const ckan::GameVersionRange &range) { m_compatRange = range; }

    // 同步解析"本次要安装的模块集"。
    // ok=true 后可继续下载/安装；ok=false 时给定原因（cancelled=用户取消）。
    struct ResolveResult {
        bool ok = false;
        bool cancelled = false;
        bool nothingToDo = false; // 无待装模块（属正常完成，非错误）
        QString error;
        QVector<ckan::CkanModule> modulesToInstall;
        QStringList preUninstall; // 已装旧版、须先卸载的标识符
    };

    // 解析安装集：内部处理多提供者选择循环、级联建议勾选与预卸载计算。
    // 未绑定实例或 mods 为空 → 返回失败（error 描述原因）。
    ResolveResult resolveInstallSet(const QVector<ckan::CkanModule> &mods,
                                    bool autoRecommends, bool showSuggests) const;

private:
    ckan::CKan *m_ckan = nullptr;
    moddecision::Hooks m_decisions;
    ckan::GameVersionRange m_compatRange; // 用户勾选的额外兼容区间（无效表示未启用）
};

} // namespace services

#endif // SERVICES_INSTALLSERVICE_H