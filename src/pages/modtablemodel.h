#ifndef MODSTABLEMODEL_H
#define MODSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QVector>
#include <QString>
#include <QSet>

#include "ckan/ckanmodule.h"

// mod 列表表格模型：展示仓库搜索结果的 mod（最新版本），并标记已安装/可升级状态。
// 首列为复选框，勾选状态按标识符记忆（跨搜索/筛选保留），用于批量安装/升级/卸载。
class ModsTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColCheck = 0,
        ColName,
        ColIdentifier,
        ColVersion,
        ColStatus,
        ColSize,
        ColDownloads,
        ColTags,
        ColumnCount
    };

    enum Status {
        NotInstalled = 0,
        Installed,
        Upgradable,
        AutoDetected, // 手动安装模组（DLL 扫描识别，AD），不可勾选/操作
        StatusCount
    };

    explicit ModsTableModel(QObject *parent = nullptr);

    void setModules(const QVector<ckan::CkanModule> &modules);
    void clear();
    ckan::CkanModule moduleAt(int row) const;
    // 热路径访问：返回行对应模块的指针，越界返回 nullptr，避免 data()/statusAt()/
    // 过滤代理逐格整份深拷贝大对象 CkanModule。
    const ckan::CkanModule *modulePtr(int row) const;
    Status statusAt(int row) const;
    // 安装/卸载后刷新状态列
    void refreshStatus();

    // ---- 行级预计算缓存（过滤/渲染热路径直接查表）----
    // 大索引下 filterAcceptsRow/data() 每行每次重复执行 toLower、版本区间解析、
    // 安装状态查询是列表卡顿根因；这里在索引/上下文变化时一次性预计算，热路径 O(1) 查表。
    struct RowCache {
        QString lowerSearch;        // name|identifier|abstract 小写合并串（普通关键词一次 contains）
        bool compatibleCurrent = false; // 兼容当前实例实际版本（无效版本按兼容=true）
        bool compatibleRange = false;   // 兼容用户额外勾选区间（区间未启用=false）
        Status status = NotInstalled;
    };
    // 返回行缓存指针，越界返回 nullptr（与 modulePtr 相同的越界语义）
    const RowCache *rowCacheAt(int row) const;
    // 兼容上下文变化（当前游戏版本/额外区间）时由过滤代理调用，只重建兼容字段
    void setCompatibilityContext(const ckan::GameVersion &v, const ckan::GameVersionRange &r);

    // ---- 勾选状态 ----
    bool isChecked(const QString &identifier) const;
    void setChecked(const QString &identifier, bool checked);
    // 批量设置本批模组的勾选状态
    void setAllChecked(const QVector<ckan::CkanModule> &mods, bool checked);
    // 当前已勾选的标识符（与 m_modules 顺序一致）
    QStringList checkedIdentifiers() const;
    int checkedCount() const;
    // 清空所有勾选
    void clearAllChecks();

    // QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QVector<ckan::CkanModule> m_modules;
    QSet<QString> m_checked; // 已勾选的标识符
    QVector<RowCache> m_rowCache; // 行级预计算缓存（与 m_modules 等长）
    ckan::GameVersion m_gameVersion;        // 当前实例实际 KSP 版本（用于兼容缓存）
    ckan::GameVersionRange m_compatRange;   // 用户额外兼容区间（用于兼容缓存）

    void rebuildStaticCache();      // 索引/状态变化时重建整表缓存（lowerSearch+status）
    void rebuildCompatibilityCache(); // 兼容上下文变化时只重建 compatible 字段
};

// 过滤代理：按搜索文本（名称/标识符）与状态（未安装/已安装/可升级）过滤。
class ModsFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit ModsFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    // statusFilter: -1 表示不过滤；否则按 ModsTableModel::Status 过滤
    void setStatusFilter(int statusFilter)
    {
        if (m_statusFilter == statusFilter) return;
        m_statusFilter = statusFilter;
        beginFilterChange();
        endFilterChange();
    }
    void setSearchText(const QString &text)
    {
        const QString t = text.trimmed();
        if (m_search == t) return;
        m_search = t;
        // 搜索词只解析一次（split + 小写 + 字段归类），过滤时逐行复用，避免每行每词重复解析
        m_searchTokens = parseSearchTokens(t);
        beginFilterChange();
        endFilterChange();
    }
    // 默认隐藏不兼容模组；设置为 true 时显示不兼容模组
    void setShowIncompatible(bool show)
    {
        if (m_showIncompatible == show) return;
        m_showIncompatible = show;
        beginFilterChange();
        endFilterChange();
    }
    // 设置当前实例实际检测到的 KSP 版本，用于按真实游戏版本过滤模组兼容性
    void setGameVersion(const ckan::GameVersion &v)
    {
        if (m_gameVersion == v) return;
        m_gameVersion = v;
        // 同步重建源模型的行级兼容缓存，过滤查表即可
        if (auto *src = qobject_cast<ModsTableModel *>(sourceModel()))
            src->setCompatibilityContext(m_gameVersion, m_compatRange);
        beginFilterChange();
        endFilterChange();
    }
    // 按仓库自带 tag 过滤（空串=不过滤）。大小写不敏感，模组含任一匹配 tag 即通过。
    void setTagFilter(const QString &tag)
    {
        const QString t = tag.trimmed();
        if (m_tagFilter.compare(t, Qt::CaseInsensitive) == 0) return;
        m_tagFilter = t.toLower();
        beginFilterChange();
        endFilterChange();
    }
    // 设置用户勾选的额外兼容区间（无效区间表示未启用）。
    // 过滤判定 = 模组兼容当前实例版本 或 兼容该区间（任一满足即可）。
    void setCompatRange(const ckan::GameVersionRange &r)
    {
        const bool same = (m_compatRange.lowerSet() == r.lowerSet()
                           && m_compatRange.upperSet() == r.upperSet()
                           && m_compatRange.lowerInclusive() == r.lowerInclusive()
                           && m_compatRange.upperInclusive() == r.upperInclusive()
                           && (!r.lowerSet() || m_compatRange.lower() == r.lower())
                           && (!r.upperSet() || m_compatRange.upper() == r.upper()));
        if (same) return;
        m_compatRange = r;
        if (auto *src = qobject_cast<ModsTableModel *>(sourceModel()))
            src->setCompatibilityContext(m_gameVersion, m_compatRange);
        beginFilterChange();
        endFilterChange();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    // 预解析后的搜索词：field 指明匹配目标，value 已小写；过滤时逐行直接匹配。
    enum SearchField { FieldGeneral, FieldAuthor, FieldDesc, FieldLicense,
                       FieldDepends, FieldProvides, FieldTag };
    struct SearchToken {
        SearchField field = FieldGeneral;
        QString value;
    };
    // 解析搜索文本为 token 列表（普通关键词=名称/标识符/摘要；@字段:值 走对应字段）。
    // 保持 matchesSearch 的既有语义：未知 @字段 不否决；空值/缺 `:` 的 token 忽略。
    static QVector<SearchToken> parseSearchTokens(const QString &text);
    // 增强搜索匹配：返回该模组是否通过搜索条件（读取行级缓存 + 预解析 token）。
    bool matchesSearch(const ModsTableModel::RowCache &cache, const ckan::CkanModule &mod) const;

    int m_statusFilter = -1;
    bool m_showIncompatible = false;
    QString m_search;
    QVector<SearchToken> m_searchTokens; // 预解析的搜索词（setSearchText 时构建一次）
    QString m_tagFilter; // 按 tag 过滤（空串=不过滤），已转小写
    ckan::GameVersion m_gameVersion; // 当前实例实际 KSP 版本（无效表示未检测到，按兼容处理）
    ckan::GameVersionRange m_compatRange; // 用户勾选的额外兼容区间（无效表示未启用）
};

#endif // MODSTABLEMODEL_H