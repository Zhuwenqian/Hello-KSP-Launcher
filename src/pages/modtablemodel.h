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
    Status statusAt(int row) const;
    // 安装/卸载后刷新状态列
    void refreshStatus();

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
        beginFilterChange();
        endFilterChange();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    int m_statusFilter = -1;
    bool m_showIncompatible = false;
    QString m_search;
    ckan::GameVersion m_gameVersion; // 当前实例实际 KSP 版本（无效表示未检测到，按兼容处理）
};

#endif // MODSTABLEMODEL_H