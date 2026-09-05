#include "installservice.h"

#include <QObject>

namespace services {

InstallService::ResolveResult InstallService::resolveInstallSet(
    const QVector<ckan::CkanModule> &mods, bool autoRecommends, bool showSuggests) const
{
    ResolveResult rr;
    if (!m_ckan || mods.isEmpty()) {
        rr.ok = false;
        rr.error = QObject::tr("尚未绑定游戏实例");
        return rr;
    }

    ckan::ResolutionResult res = m_ckan->resolveInstallMany(mods, autoRecommends, showSuggests,
                                                            m_compatRange);

    // 多提供者选择：同一虚拟包被多个模组提供 → 弹窗让用户决定安装哪个。
    // 选择结果并入安装集后重新解析（循环直至无多提供者待选；guard 防死循环）。
    QVector<ckan::CkanModule> selectedProviders;
    for (int guard = 0; guard < 16 && !res.providerChoices.isEmpty(); ++guard) {
        bool cancelled = false;
        const QVector<ckan::CkanModule> picked = m_decisions.providers(res.providerChoices, &cancelled);
        if (cancelled) { rr.cancelled = true; return rr; }
        if (picked.isEmpty()) { rr.error = QObject::tr("未选择任何提供者"); return rr; }
        selectedProviders += picked;
        QVector<ckan::CkanModule> combined = mods;
        combined += selectedProviders;
        res = m_ckan->resolveInstallMany(combined, autoRecommends, showSuggests, m_compatRange);
        if (res.conflicted) { rr.error = res.conflicts.join(QLatin1Char('\n')); return rr; }
        if (res.missing) {
            rr.error = QObject::tr("缺少依赖：%1").arg(res.notFound.join(QLatin1Char(',')));
            return rr;
        }
    }
    if (res.conflicted) { rr.error = res.conflicts.join(QLatin1Char('\n')); return rr; }
    if (res.missing) {
        rr.error = QObject::tr("缺少依赖：%1").arg(res.notFound.join(QLatin1Char(',')));
        return rr;
    }

    QVector<ckan::CkanModule> modules = res.modulesToInstall;

    // 级联建议：弹窗让用户勾选可选模组；选中的并入安装集重新解析（连其依赖一起）
    if (showSuggests && !res.suggestedModules.isEmpty()) {
        bool cancelled = false;
        const QVector<ckan::CkanModule> selected = m_decisions.suggests(res.suggestedModules, &cancelled);
        if (cancelled) { rr.cancelled = true; return rr; }
        if (!selected.isEmpty()) {
            QVector<ckan::CkanModule> combined = mods;
            combined += selected;
            const ckan::ResolutionResult res2 = m_ckan->resolveInstallMany(combined, autoRecommends,
                                                                           false, m_compatRange);
            if (res2.conflicted) { rr.error = res2.conflicts.join(QLatin1Char('\n')); return rr; }
            if (res2.missing) {
                rr.error = QObject::tr("缺少依赖：%1").arg(res2.notFound.join(QLatin1Char(',')));
                return rr;
            }
            modules = res2.modulesToInstall;
        }
    }

    if (modules.isEmpty()) { rr.nothingToDo = true; return rr; }

    // 已安装但需更新的：安装前先卸载旧版本
    QStringList preUninstall;
    for (const ckan::CkanModule &m : mods)
        if (m_ckan->isInstalled(m.identifier))
            preUninstall.append(m.identifier);

    rr.ok = true;
    rr.modulesToInstall = modules;
    rr.preUninstall = preUninstall;
    return rr;
}

} // namespace services