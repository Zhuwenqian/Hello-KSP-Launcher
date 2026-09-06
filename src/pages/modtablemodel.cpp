#include "modtablemodel.h"

#include <QRegularExpression>

#include "../ckanmanager.h"

ModsTableModel::ModsTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ModsTableModel::setModules(const QVector<ckan::CkanModule> &modules)
{
    beginResetModel();
    m_modules = modules;
    rebuildStaticCache();
    endResetModel();
}

void ModsTableModel::clear()
{
    beginResetModel();
    m_modules.clear();
    m_rowCache.clear();
    endResetModel();
}

ckan::CkanModule ModsTableModel::moduleAt(int row) const
{
    const ckan::CkanModule *mod = modulePtr(row);
    return mod ? *mod : ckan::CkanModule();
}

const ckan::CkanModule *ModsTableModel::modulePtr(int row) const
{
    if (row < 0 || row >= m_modules.size()) return nullptr;
    return &m_modules.at(row);
}

const ModsTableModel::RowCache *ModsTableModel::rowCacheAt(int row) const
{
    if (row < 0 || row >= m_rowCache.size()) return nullptr;
    return &m_rowCache.at(row);
}

// 行级预计算：合并小写搜索串 + 兼容判定 + 安装状态，一次构建、热路径查表。
// 在索引就绪/状态变化时调用；兼容字段依赖 m_gameVersion/m_compatRange（当前上下文）。
void ModsTableModel::rebuildStaticCache()
{
    const int n = m_modules.size();
    m_rowCache.resize(n);
    CKanManager &mgr = CKanManager::instance();
    for (int i = 0; i < n; ++i) {
        const ckan::CkanModule &mod = m_modules.at(i);
        RowCache &c = m_rowCache[i];
        // 普通关键词一次 contains 命中 name/identifier/abstract 任一字段
        c.lowerSearch = (mod.name + QLatin1Char('|') + mod.identifier + QLatin1Char('|') + mod.abstract)
                            .toLower();
        c.compatibleCurrent = mod.isCompatible(m_gameVersion);
        c.compatibleRange = (m_compatRange.lowerSet() || m_compatRange.upperSet())
                                ? mod.isCompatible(m_compatRange) : false;
        // 模型中的模块即各标识符的仓库最新版（由 CKan::search 填充）。
        // 直接与已安装版本比较即可判断是否可升级，避免每行每次渲染都重复执行
        // latestOf -> versionsOf -> 全量版本排序 的昂贵计算（大列表卡顿的根因）。
        const QString installed = mgr.installedVersion(mod.identifier);
        if (!installed.isEmpty()) {
            c.status = (ckan::ModuleVersion(mod.version) > ckan::ModuleVersion(installed))
                           ? Upgradable : Installed;
        } else {
            c.status = mgr.isAutoDetected(mod.identifier) ? AutoDetected : NotInstalled;
        }
    }
}

void ModsTableModel::rebuildCompatibilityCache()
{
    const int n = m_rowCache.size();
    if (n != m_modules.size()) {
        rebuildStaticCache();
        return;
    }
    for (int i = 0; i < n; ++i) {
        const ckan::CkanModule &mod = m_modules.at(i);
        RowCache &c = m_rowCache[i];
        c.compatibleCurrent = mod.isCompatible(m_gameVersion);
        c.compatibleRange = (m_compatRange.lowerSet() || m_compatRange.upperSet())
                                ? mod.isCompatible(m_compatRange) : false;
    }
}

void ModsTableModel::setCompatibilityContext(const ckan::GameVersion &v, const ckan::GameVersionRange &r)
{
    m_gameVersion = v;
    m_compatRange = r;
    rebuildCompatibilityCache();
}

ModsTableModel::Status ModsTableModel::statusAt(int row) const
{
    const RowCache *c = rowCacheAt(row);
    if (!c) return NotInstalled;
    return c->status;
}

void ModsTableModel::refreshStatus()
{
    if (m_modules.isEmpty()) return;
    rebuildStaticCache(); // 安装/卸载后状态可能变化，重建缓存（含已安装版本比较）
    emit dataChanged(index(0, 0), index(m_modules.size() - 1, ColumnCount - 1));
}

bool ModsTableModel::isChecked(const QString &identifier) const
{
    return m_checked.contains(identifier);
}

void ModsTableModel::setChecked(const QString &identifier, bool checked)
{
    if (checked) m_checked.insert(identifier);
    else m_checked.remove(identifier);
}

void ModsTableModel::setAllChecked(const QVector<ckan::CkanModule> &mods, bool checked)
{
    if (mods.isEmpty()) return;
    for (const ckan::CkanModule &m : mods) {
        if (checked) m_checked.insert(m.identifier);
        else m_checked.remove(m.identifier);
    }
    emit dataChanged(index(0, ColCheck), index(m_modules.size() - 1, ColCheck));
}

QStringList ModsTableModel::checkedIdentifiers() const
{
    QStringList out;
    for (const ckan::CkanModule &m : m_modules)
        if (m_checked.contains(m.identifier)) out << m.identifier;
    return out;
}

int ModsTableModel::checkedCount() const
{
    int n = 0;
    for (const ckan::CkanModule &m : m_modules)
        if (m_checked.contains(m.identifier)) ++n;
    return n;
}

void ModsTableModel::clearAllChecks()
{
    if (m_checked.isEmpty()) return;
    m_checked.clear();
    if (!m_modules.isEmpty())
        emit dataChanged(index(0, ColCheck), index(m_modules.size() - 1, ColCheck));
}

int ModsTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_modules.size();
}

int ModsTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ModsTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    const ckan::CkanModule *mod = modulePtr(index.row());
    if (!mod) return QVariant();

    if (role == Qt::UserRole) {
        return static_cast<int>(statusAt(index.row()));
    }

    if (index.column() == ColCheck && role == Qt::CheckStateRole) {
        // AD（手动安装）模组不可勾选
        if (statusAt(index.row()) == AutoDetected) return QVariant();
        return m_checked.contains(mod->identifier) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColCheck: {
            // 手动安装模组：勾选列显示 AD 标记以替代复选框
            if (statusAt(index.row()) == AutoDetected) return QStringLiteral("AD");
            return QVariant();
        }
        case ColName:       return mod->name;
        case ColIdentifier: return mod->identifier;
        case ColVersion:    return mod->version;
        case ColStatus: {
            switch (statusAt(index.row())) {
            case Upgradable:  return tr("可升级");
            case Installed:   return tr("已安装");
            case AutoDetected: return tr("AD");
            case NotInstalled: return tr("未安装");
            }
            return QVariant();
        }
        case ColSize: {
            const double mb = mod->downloadSize / 1024.0 / 1024.0;
            return mb >= 1.0 ? QString::number(mb, 'f', 1) + tr(" MB")
                             : QString::number(mod->downloadSize / 1024.0, 'f', 0) + tr(" KB");
        }
        case ColDownloads: {
            const int n = CKanManager::instance().downloadCount(mod->identifier);
            return n >= 0 ? QString::number(n) : QStringLiteral("-");
        }
        case ColTags: {
            return mod->tags.join(QStringLiteral(", "));
        }
        }
    }

    if (role == Qt::ToolTipRole) {
        return mod->abstract;
    }

    return QVariant();
}

QVariant ModsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation != Qt::Horizontal) return QVariant();
    switch (section) {
    case ColCheck:      return QString();
    case ColName:       return tr("名称");
    case ColIdentifier: return tr("标识符");
    case ColVersion:    return tr("版本");
    case ColStatus:     return tr("状态");
    case ColSize:       return tr("大小");
    case ColDownloads:  return tr("下载");
    case ColTags:       return tr("标签");
    }
    return QVariant();
}

bool ModsTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.column() == ColCheck && role == Qt::CheckStateRole) {
        const ckan::CkanModule *mod = modulePtr(index.row());
        if (!mod) return false;
        // AD（手动安装）模组不可勾选
        if (statusAt(index.row()) == AutoDetected) return false;
        setChecked(mod->identifier, value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }
    return QAbstractTableModel::setData(index, value, role);
}

Qt::ItemFlags ModsTableModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    // AD（手动安装）模组整体不可操作，禁用复选框
    if (index.column() == ColCheck && statusAt(index.row()) != AutoDetected)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

bool ModsFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto *src = static_cast<ModsTableModel *>(sourceModel());
    if (!src) return true;
    const ckan::CkanModule *mod = src->modulePtr(sourceRow);
    const ModsTableModel::RowCache *cache = src->rowCacheAt(sourceRow);
    if (!mod || !cache) return false;

    // 不兼容模组默认隐藏（除非开启显示）。
    // 兼容判定：兼容当前实例实际版本 或 兼容用户勾选的额外区间（任一满足即可）。
    // 兼容结果已在模型预计算，此处 O(1) 查表。
    bool compatible = cache->compatibleCurrent;
    if (!compatible && (m_compatRange.lowerSet() || m_compatRange.upperSet()))
        compatible = cache->compatibleRange;
    if (!m_showIncompatible && !compatible)
        return false;

    if (!m_searchTokens.isEmpty() && !matchesSearch(*cache, *mod))
        return false;
    // 按 tag 过滤（空串不过滤）；大小写不敏感，模组含任一匹配 tag 即通过
    if (!m_tagFilter.isEmpty()) {
        bool matched = false;
        for (const QString &t : mod->tags)
            if (t.compare(m_tagFilter, Qt::CaseInsensitive) == 0) { matched = true; break; }
        if (!matched) return false;
    }
    if (m_statusFilter >= 0) {
        const ModsTableModel::Status s = cache->status;
        // AD（手动安装）模组归入「已安装」分类
        if (s == ModsTableModel::AutoDetected) {
            if (m_statusFilter != static_cast<int>(ModsTableModel::Installed)) return false;
        } else if (s != static_cast<ModsTableModel::Status>(m_statusFilter)) {
            return false;
        }
    }
    return true;
}

QVector<ModsFilterProxyModel::SearchToken> ModsFilterProxyModel::parseSearchTokens(const QString &text)
{
    QVector<SearchToken> tokens;
    const QStringList rawTokens = text.split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &raw : rawTokens) {
        const QString tok = raw.toLower();
        SearchToken t;
        if (tok.startsWith(QLatin1Char('@'))) {
            const int colon = tok.indexOf(QLatin1Char(':'));
            // 无 `:` 的字段 token（或空值）忽略，不算命中/否决。
            if (colon <= 1) continue;
            const QString field = tok.mid(1, colon - 1);
            const QString val = tok.mid(colon + 1);
            if (val.isEmpty()) continue;

            if (field == QLatin1String("author")) {
                t.field = FieldAuthor;
            } else if (field == QLatin1String("desc") || field == QLatin1String("description")) {
                t.field = FieldDesc;
            } else if (field == QLatin1String("license")) {
                t.field = FieldLicense;
            } else if (field == QLatin1String("depend") || field == QLatin1String("depends")) {
                t.field = FieldDepends;
            } else if (field == QLatin1String("provides")) {
                t.field = FieldProvides;
            } else if (field == QLatin1String("tag") || field == QLatin1String("tags")) {
                t.field = FieldTag;
            } else {
                continue; // 未知字段忽略该 token，不否决任何行
            }
            t.value = val;
        } else {
            t.field = FieldGeneral;
            t.value = tok;
        }
        tokens.append(t);
    }
    return tokens;
}

bool ModsFilterProxyModel::matchesSearch(const ModsTableModel::RowCache &cache,
                                         const ckan::CkanModule &mod) const
{
    // 任意一个字段值（大小写不敏感）包含目标串即命中。
    const auto anyContains = [](const QStringList &values, const QString &needle) {
        for (const QString &v : values)
            if (v.toLower().contains(needle)) return true;
        return false;
    };

    for (const SearchToken &t : m_searchTokens) {
        bool ok;
        switch (t.field) {
        case FieldGeneral:
            // 普通关键词：命中名称/标识符/摘要（合并小写串一次 contains）
            ok = cache.lowerSearch.contains(t.value);
            break;
        case FieldAuthor:
            ok = anyContains(mod.author, t.value);
            break;
        case FieldDesc:
            ok = mod.description.toLower().contains(t.value);
            break;
        case FieldLicense:
            ok = anyContains(mod.license, t.value);
            break;
        case FieldDepends:
            ok = false;
            for (const ckan::Relationship &rel : mod.depends)
                if (rel.name.toLower().contains(t.value)) { ok = true; break; }
            break;
        case FieldProvides:
            ok = anyContains(mod.providesList(), t.value);
            break;
        case FieldTag:
            ok = anyContains(mod.tags, t.value);
            break;
        default:
            ok = true; // 未知字段忽略，不否决
            break;
        }
        if (!ok) return false;
    }
    return true;
}