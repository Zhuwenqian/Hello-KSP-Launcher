#include "installservice.h"

#include <QObject>
#include <QSet>
#include <QList>
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

    // 需要弹窗勾选 recommends → 解析时关闭 recommends 自动安装（推荐改由下方渐进式弹窗收集），
    // 避免解析阶段把推荐自动装掉。Suggests 不参与依赖集合，统一在渐进式弹窗中处理。
    const bool autoRec = showRecommends ? false : autoRecommends;

    ckan::ResolutionResult res = m_ckan->resolveInstallMany(mods, autoRec, false,
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
        res = m_ckan->resolveInstallMany(combined, autoRec, false, m_compatRange);
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

    // --- 渐进式收集：先逐个弹推荐(Recommends)，再逐个弹建议(Suggests) ---
    // 只对"明确安装"（用户勾选/点击）的模组收集；每个模组的推荐/建议单独一个框弹窗，
    // 勾选结果并入安装集后，其新产生的模组再进队继续收集（动态循环直至不再新增）。
    QVector<ckan::CkanModule> currentSet = res.modulesToInstall; // 当前完整安装集（含依赖）
    QMap<QString, ckan::CkanModule> explicitSelections;          // 用户明确选择安装的模组（去重只装一次）
    QSet<QString> explicitIds;                                   // 用户明确选择的标识符集合
    for (const ckan::CkanModule &m : mods) {
        explicitSelections[m.identifier] = m;
        explicitIds.insert(m.identifier);
    }
    for (const ckan::CkanModule &m : selectedProviders) {
        explicitSelections[m.identifier] = m;
        explicitIds.insert(m.identifier);
    }

    auto findInSet = [&](const QString &id) -> const ckan::CkanModule * {
        for (const ckan::CkanModule &m : currentSet)
            if (m.identifier == id) return &m;
        return nullptr;
    };

    // 合并一次勾选结果并重新解析；新增有选则更新安装集。返回是否成功（失败已填 rr）。
    auto mergeAndResolve = [&](const QVector<ckan::CkanModule> &selected) -> bool {
        for (const ckan::CkanModule &m : selected) {
            if (m.identifier.isEmpty()) continue;
            if (explicitSelections.contains(m.identifier)) continue; // 只装一次
            explicitSelections[m.identifier] = m;
            explicitIds.insert(m.identifier);
        }
        if (selected.isEmpty()) return true; // 未新增，无需重解析
        QVector<ckan::CkanModule> combined;
        for (auto it = explicitSelections.constBegin(); it != explicitSelections.constEnd(); ++it)
            combined.append(it.value());
        const ckan::ResolutionResult res2 = m_ckan->resolveInstallMany(combined, false, false,
                                                                       m_compatRange);
        if (res2.conflicted) { rr.error = res2.conflicts.join(QLatin1Char('\n')); return false; }
        if (res2.missing) {
            rr.error = QObject::tr("缺少依赖：%1").arg(res2.notFound.join(QLatin1Char(',')));
            return false;
        }
        currentSet = res2.modulesToInstall;
        return true;
    };

    QSet<QString> recShown; // 已弹过推荐框的标识符（避免重复/跨阶段重复弹）

    // 处理一条身份标识的推荐或建议：收集候选 → 弹窗 → 并入安装集 → 新模组入队。
    // 返回是否继续；失败（含用户取消）填 rr 返回 false。
    // 注意：mergeAndResolve 会重建 currentSet，避免跨该调用保存 parent 指针（会悬垂）。
    auto processOptional = [&](const QString &id, bool wantRecommends, QList<QString> &queue,
                               QSet<QString> &enqueued) -> bool {
        const ckan::CkanModule *parent = findInSet(id);
        if (!parent) return true; // 集合中找不到（理论不发生）→ 跳过
        const QString pname = parent->name;
        // 建议阶段补弹：该模组推荐还从未弹过 → 先弹它的推荐框
        if (!wantRecommends && !recShown.contains(id)) {
            recShown.insert(id);
            parent = findInSet(id); // 重新定位（集合可能已因前序处理变化）
            const QVector<ckan::CkanModule> recCands =
                m_ckan->collectOptionalFor(*parent, currentSet, true, m_compatRange);
            if (!recCands.isEmpty()) {
                bool cancelled = false;
                const QVector<ckan::CkanModule> sel = m_decisions.recommends(recCands, &cancelled, pname);
                if (cancelled) { rr.cancelled = true; return false; }
                if (!mergeAndResolve(sel)) return false;
                for (const ckan::CkanModule &m : sel)
                    if (!enqueued.contains(m.identifier)) { queue.append(m.identifier); enqueued.insert(m.identifier); }
            }
        }
        if (wantRecommends) recShown.insert(id);
        parent = findInSet(id); // mergeAndResolve 后可覆盖强指针重新定位
        const QVector<ckan::CkanModule> cands =
            m_ckan->collectOptionalFor(*parent, currentSet, wantRecommends, m_compatRange);
        if (cands.isEmpty()) return true; // 无可推荐/建议 → 静默跳过
        bool cancelled = false;
        const QVector<ckan::CkanModule> sel = wantRecommends
            ? m_decisions.recommends(cands, &cancelled, pname)
            : m_decisions.suggests(cands, &cancelled, pname);
        if (cancelled) { rr.cancelled = true; return false; }
        if (!mergeAndResolve(sel)) return false;
        for (const ckan::CkanModule &m : sel)
            if (!enqueued.contains(m.identifier)) { queue.append(m.identifier); enqueued.insert(m.identifier); }
        return true;
    };

    // 阶段一：推荐（Recommends）——逐个弹推荐框（新勾选的模组也进队继续收集其推荐）
    if (showRecommends) {
        QList<QString> queue;
        QSet<QString> enqueued;
        for (const QString &id : explicitIds)
            if (!enqueued.contains(id)) { queue.append(id); enqueued.insert(id); }
        while (!queue.isEmpty()) {
            const QString id = queue.takeFirst();
            if (recShown.contains(id)) continue; // 已弹过，跳过
            if (!processOptional(id, true, queue, enqueued)) return rr;
        }
    }

    // 阶段二：建议（Suggests）——逐个弹建议框（含对所有已明确模组；新勾选者先补弹其推荐）
    if (showSuggests) {
        QList<QString> queue;
        QSet<QString> enqueued;
        for (auto it = explicitIds.constBegin(); it != explicitIds.constEnd(); ++it)
            if (!enqueued.contains(*it)) { queue.append(*it); enqueued.insert(*it); }
        while (!queue.isEmpty()) {
            const QString id = queue.takeFirst();
            if (!processOptional(id, false, queue, enqueued)) return rr;
        }
    }

    const QVector<ckan::CkanModule> modules = currentSet;

    if (modules.isEmpty()) { rr.nothingToDo = true; return rr; }

    // 已安装但需更新的：安装前先卸载旧版本（覆盖含渐进勾选的推荐/建议）
    QStringList preUninstall;
    for (const ckan::CkanModule &m : modules)
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