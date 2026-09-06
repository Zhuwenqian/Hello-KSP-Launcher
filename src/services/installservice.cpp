#include "installservice.h"

#include <QObject>
#include <QDebug>

namespace services {

InstallService::ResolveResult InstallService::resolveInstallSet(
    const QVector<ckan::CkanModule> &mods, bool autoRecommends, bool showSuggests,
    bool showRecommends) const
{
    ResolveResult rr;
    if (!m_ckan || mods.isEmpty()) {
        rr.ok = false;
        rr.error = QObject::tr("尚未绑定游戏实例");
        return rr;
    }

    // 需要弹窗勾选 recommends → 解析时只收集（不自动安装），供下方弹窗后按勾选结果重新解析
    const bool collectRecommends = showRecommends;
    const bool autoRec = collectRecommends ? false : autoRecommends;

    ckan::ResolutionResult res = m_ckan->resolveInstallMany(mods, autoRec, showSuggests,
                                                            m_compatRange, collectRecommends);
    if (collectRecommends && !res.recommendedModules.isEmpty())
        qInfo() << "[install] 待安装模组解析完成，收集到推荐模组" << res.recommendedModules.size()
                << "个，建议模组" << res.suggestedModules.size() << "个";

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
        res = m_ckan->resolveInstallMany(combined, autoRec, showSuggests, m_compatRange,
                                         collectRecommends);
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

    // 推荐模组（Recommends）弹窗：让用户勾选要安装的推荐（默认全选）。
    // 勾选结果并入安装集重新解析（连其依赖一起）；在级联建议弹窗之前处理，
    // 使基于最新安装集收集的建议也能一并展示。
    if (showRecommends && !res.recommendedModules.isEmpty()) {
        bool cancelled = false;
        const QVector<ckan::CkanModule> selected = m_decisions.recommends(res.recommendedModules, &cancelled);
        if (cancelled) { rr.cancelled = true; return rr; }
        if (!selected.isEmpty()) {
            QVector<ckan::CkanModule> combined = mods;
            combined += selected;
            // 二次解析：勾选的推荐已作为显式请求进入安装集，不再收集/自动装 recommends
            res = m_ckan->resolveInstallMany(combined, false, showSuggests, m_compatRange);
            if (res.conflicted) { rr.error = res.conflicts.join(QLatin1Char('\n')); return rr; }
            if (res.missing) {
                rr.error = QObject::tr("缺少依赖：%1").arg(res.notFound.join(QLatin1Char(',')));
                return rr;
            }
        }
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

    qInfo() << "[install] 安装决策完成：本次安装" << modules.size()
            << "个模组，需先卸载旧版本" << preUninstall.size() << "个";
    rr.ok = true;
    rr.modulesToInstall = modules;
    rr.preUninstall = preUninstall;
    return rr;
}

} // namespace services