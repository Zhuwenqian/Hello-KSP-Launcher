#include "modtablemodel.h"

#include "../ckanmanager.h"

ModsTableModel::ModsTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ModsTableModel::setModules(const QVector<ckan::CkanModule> &modules)
{
    beginResetModel();
    m_modules = modules;
    endResetModel();
}

void ModsTableModel::clear()
{
    beginResetModel();
    m_modules.clear();
    endResetModel();
}

ckan::CkanModule ModsTableModel::moduleAt(int row) const
{
    if (row < 0 || row >= m_modules.size()) return ckan::CkanModule();
    return m_modules.at(row);
}

ModsTableModel::Status ModsTableModel::statusAt(int row) const
{
    const ckan::CkanModule mod = moduleAt(row);
    if (!mod.isValid()) return NotInstalled;
    CKanManager &mgr = CKanManager::instance();
    if (mgr.isUpgradable(mod.identifier)) return Upgradable;
    if (mgr.isInstalled(mod.identifier)) return Installed;
    if (mgr.isAutoDetected(mod.identifier)) return AutoDetected;
    return NotInstalled;
}

void ModsTableModel::refreshStatus()
{
    const int n = m_modules.size();
    if (n == 0) return;
    emit dataChanged(index(0, 0), index(n - 1, ColumnCount - 1));
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
    const int row = index.row();
    const ckan::CkanModule mod = moduleAt(row);
    if (!mod.isValid()) return QVariant();

    if (role == Qt::UserRole) {
        return static_cast<int>(statusAt(row));
    }

    if (index.column() == ColCheck && role == Qt::CheckStateRole) {
        // AD（手动安装）模组不可勾选
        if (statusAt(row) == AutoDetected) return QVariant();
        return m_checked.contains(mod.identifier) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColCheck: {
            // 手动安装模组：勾选列显示 AD 标记以替代复选框
            if (statusAt(row) == AutoDetected) return QStringLiteral("AD");
            return QVariant();
        }
        case ColName:       return mod.name;
        case ColIdentifier: return mod.identifier;
        case ColVersion:    return mod.version;
        case ColStatus: {
            switch (statusAt(row)) {
            case Upgradable:  return tr("可升级");
            case Installed:   return tr("已安装");
            case AutoDetected: return tr("AD");
            case NotInstalled: return tr("未安装");
            }
            return QVariant();
        }
        case ColSize: {
            const double mb = mod.downloadSize / 1024.0 / 1024.0;
            return mb >= 1.0 ? QString::number(mb, 'f', 1) + tr(" MB")
                             : QString::number(mod.downloadSize / 1024.0, 'f', 0) + tr(" KB");
        }
        }
    }

    if (role == Qt::ToolTipRole) {
        return mod.abstract;
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
    }
    return QVariant();
}

bool ModsTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.column() == ColCheck && role == Qt::CheckStateRole) {
        const ckan::CkanModule mod = moduleAt(index.row());
        if (!mod.isValid()) return false;
        // AD（手动安装）模组不可勾选
        if (statusAt(index.row()) == AutoDetected) return false;
        setChecked(mod.identifier, value.toInt() == Qt::Checked);
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
    const ckan::CkanModule mod = src->moduleAt(sourceRow);
    if (!mod.isValid()) return false;

    // 不兼容模组默认隐藏（除非开启显示）
    if (!m_showIncompatible && !mod.isCompatible(ckan::GameVersion()))
        return false;

    if (!m_search.isEmpty()) {
        const QString s = m_search.toLower();
        if (!mod.name.toLower().contains(s) && !mod.identifier.toLower().contains(s))
            return false;
    }
    if (m_statusFilter >= 0) {
        const ModsTableModel::Status s = src->statusAt(sourceRow);
        // AD（手动安装）模组归入「已安装」分类
        if (s == ModsTableModel::AutoDetected) {
            if (m_statusFilter != static_cast<int>(ModsTableModel::Installed)) return false;
        } else if (s != static_cast<ModsTableModel::Status>(m_statusFilter)) {
            return false;
        }
    }
    return true;
}