#ifndef MODSTABPAGE_H
#define MODSTABPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QTableView>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QStringList>
#include <QSplitter>

class QShowEvent;

#include "ckan/ckanmodule.h"
#include "modscontroller.h"
#include "modtablemodel.h"
#include "../configmanager.h"
#include "../instancemanager.h"

class QTimer;

// 「文件」tab 后台构建的轻量清单节点树：与 UI 解耦，worker 线程内建好+排序，
// 主线程只把**可见层**转成 QTreeWidgetItem，展开目录时才物化下一层（可承载十几万条目）。
struct ModsContentsNode
{
    QString name;       // 本级名
    qint64 size = -1;   // 文件字节；目录为 -1
    bool isDir = false;
    QList<ModsContentsNode*> children;
    ~ModsContentsNode() { qDeleteAll(children); }
};

// 「文件」tab 后台探测/建树结果：root 空=压缩包未缓存且无已安装目录可展示；
// fromZip 区分来源（决定状态栏文案与 GameData 根节点是否默认展开）。
struct ModsContentsResult
{
    ModsContentsNode *root = nullptr;
    QString error;
    bool fromZip = false;
};

// 实例详情页的"模组管理"二级页：承载全部模组 UI
// （搜索/筛选/标签/表格/详情四 tab/下载进度/操作按钮/导入与安装历史）。
// 业务编排经内部的 ModsController（其持有真实 Qt 决策弹窗并转发操作与信号）；
// 只读查询直接走 CKanManager 单例。
class ModsTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit ModsTabPage(QWidget *parent = nullptr);
    ~ModsTabPage() override;

protected:
    // 首次显示时按真实可用高度精确还原分隔条持久化位置
    // （构造期页面未布局，setSizes 会被 Qt 按可用空间重新归一化导致还原失效）。
    void showEvent(QShowEvent *event) override;

public:
    // 绑定实例并准备模组数据（非阻塞：后台索引/DLL 扫描，就绪后自动填充模型）。
    void setInstance(const KSPInstance &inst, const QString &instanceId);
    // 前台/离开切换：进入时补"加载中"提示或刷新按钮态，并控制注册表锁轮询开关。
    void setTabActive(bool active);
    // .ckan 整合包导入后跳转到本页并带待装清单：索引就绪后自动批量安装。
    void queueCkanInstall(const QStringList &identifiers);
    // 重新准备模组数据（供整合包导入等清空 GameData 后刷新；沿用已绑定实例）。
    void prepareMods();
    // 索引与 DLL 扫描均就绪时填充模组模型并刷新按钮；未就绪则清空并给出加载提示。
    // （公开供整合包导入流程在重建 CKan 后强制刷新模型。）
    void maybePopulateMods();
    void refreshIcons(const QString &color);

private slots:
    // 顶栏搜索/筛选
    void onModSearchChanged(const QString &text);
    void onModFilterChanged(int index);
    void onTagFilterChanged(int index);
    void rebuildTagFilter();
    void onShowIncompatibleToggled(bool checked);
    void onCompatVersionsClicked();
    void onRefreshModsClicked();
    // 表格选择与勾选
    void onModSelectionChanged();
    void onModDoubleClicked(const QModelIndex &index);
    void onSelectAllClicked();
    // 操作按钮
    void onInstallModClicked();
    void onUninstallModClicked();
    void onUpgradeModClicked();
    void onCancelDownloadClicked();
    void onImportModClicked();
    void onShowHistoryClicked();
    // 适配层信号回调
    void onIndexRefreshed(CKanManager::IndexRefreshStatus status, const QString &error);
    void onUnmanagedScanFinished();
    void onModOperationFinished(bool ok, const QString &message);
    void onDownloadProgress(const QString &identifier, qint64 doneBytes,
                            qint64 totalBytes, qint64 speedBps);
    // 后台构建完整个 mod 列表后回主线程填充模型
    void onModsLoadFinished();
    // 搜索输入防抖：连续输入只触发一次过滤（150ms）
    void onSearchDebounceTimeout();
    // 模组详情四 tab
    void onSingleDownloadFinished(bool ok, const QString &identifier, const QString &error);
    void onContentsDownloadClicked();
    // 「文件」tab 目录展开时按需物化下一层子节点（懒加载）
    void onContentsItemExpanded(QTreeWidgetItem *item);
    void onReverseRelToggled(bool on);
    void onRelationItemExpanded(QTreeWidgetItem *item);
    void onVersionSelectionChanged();
    void onVersionInstallClicked();

private:
    void setupUi();
    // 获取注册表锁后真正执行装载（加载索引 + DLL 扫描）。
    void prepareModsLoading();
    // 注册表写锁被其他进程占用时的门控：弹窗 + 清空表格 + 禁用按钮 + 10s 轮询。
    void startRegistryLockWait();
    void stopRegistryLockWait();
    void onRegistryLockPollTick();
    // 将当前实例勾选的兼容版本区间应用到过滤代理与 CKanManager
    void applyCompatRange();
    void updateModActionButtons();
    void updateSelectAllButtonText();
    void setModButtonsEnabled(bool enabled);
    // 模组列表 UI 状态持久化（每实例，随改随存）
    void captureAndSaveListState();   // 立即抓取当前控件状态并落盘（切换实例前 flush、防抖超时回调）
    void queueStateSave();            // 防抖后落盘（搜索/筛选/标签/详情tab/排序/滚动/选中行）
    void restoreListState();          // setInstance 时把已存状态应用回控件（标签/滚动/选中行留待数据就绪）
    void restoreListStateAfterLoad(); // 列表数据就绪（标签下拉重建后）还原标签选中/选中行/滚动位置
    void showModDetails(const ckan::CkanModule &mod);
    void setDetailNote(const QString &text);
    void showMetaTab(const ckan::CkanModule &mod);
    // 「文件」tab 懒加载入口：清单已过期且当前模组有效时才真正构建（见 m_contentsStale）
    void ensureContentsLoaded();
    // 后台建轻量节点树（缓存探测/zip 解析/目录递归/排序全在 worker）+ 主线程仅建可见层，
    // 展开目录时才物化子级——大模组包（十几万条目）切 tab/展开都不卡 UI
    void startContentsScan(const ckan::CkanModule &mod, const QString &downloadDir,
                           const QString &gameDir, const QStringList &installedEntries);
    void cancelContentsWork();
    QTreeWidgetItem* createContentsItem(QTreeWidgetItem *parent, ModsContentsNode *node);
    void populateContentsItem(QTreeWidgetItem *item);
    void showContentsTab(const ckan::CkanModule &mod);
    void showRelationshipsTab(const ckan::CkanModule &mod, bool reverse);
    void addRelationChildren(QTreeWidgetItem *parent, const QString &identifier, int depth);
    void showVersionsTab(const ckan::CkanModule &mod);
    void showDownloadProgress();
    void hideDownloadProgress();
    void showUninstallProgress(const QString &label);
    // 依据真实卸载级联规则计算依赖数量，生成确认提示（无依赖返回空串）。
    QString uninstallCascadeHint(const QStringList &identifiers);

    ModsController m_controller;

    QString m_instanceId;
    KSPInstance m_instance;

    // 顶栏控件
    QLineEdit* m_modSearchEdit;
    QComboBox* m_modFilterCombo;
    QComboBox* m_tagFilterCombo; // 按仓库自带 tag 筛选
    QCheckBox* m_showIncompatCheck;
    QPushButton* m_compatBtn;    // 兼容版本设置按钮
    QPushButton* m_selectAllBtn;
    QPushButton* m_refreshModsBtn;
    // 表格
    ModsTableModel* m_modsModel;
    ModsFilterProxyModel* m_modsProxy;
    QTableView* m_modTable;
    // 下载进度
    QWidget*  m_modProgressWidget;
    QProgressBar* m_modProgressBar;
    QLabel*   m_modProgressLabel;
    QPushButton* m_cancelDownloadBtn;
    // 操作按钮
    QPushButton* m_installModBtn;
    QPushButton* m_uninstallModBtn;
    QPushButton* m_upgradeModBtn;
    QPushButton* m_importModBtn;  // 导入单模组文件（.zip/.ckan）
    QPushButton* m_historyBtn;    // 查看安装历史
    // 模组详情四 tab
    QTabWidget*  m_modDetailTabs;     // 元数据 / 文件 / 关系 / 版本
    // 模组列表 / 详情垂直分隔条（上方=列表段，下方=四tab）
    QSplitter*   m_modSplitter = nullptr;
    bool         m_splitterRestored = false; // 分隔条持久化高度是否已还原（仅首次显示）
    QTextEdit*   m_metaText;          // 元数据 tab
    QTreeWidget* m_contentsTree;      // 文件清单 tab
    QLabel*      m_contentsStatusLabel;
    QPushButton* m_contentsDownloadBtn;
    QTreeWidget* m_relTree;           // 关系 tab（懒加载树，可切反向）
    QCheckBox*   m_reverseRelCheck;
    QTreeWidget* m_versionsTree;      // 版本历史 tab
    QPushButton* m_versionsInstallBtn;

    QFutureWatcher<QStringList>* m_reverseWatcher = nullptr; // 反向关系扫描在途
    QFutureWatcher<QVector<ckan::CkanModule>>* m_modsLoadWatcher = nullptr;
    // 后台建树在途 + 「文件」tab 可见层物化状态
    QFutureWatcher<ModsContentsResult>* m_contentsScanWatcher = nullptr;
    qint64 m_contentsGeneration = 0; // 递增代数：选中切换/重入即作废在途扫描与节点树
    ModsContentsNode* m_contentsRoot = nullptr; // 后台建好的节点树根（展开物化的数据源）
    QString m_contentsDoneStatus;          // 清单就绪后的状态栏文案
    bool m_contentsExpandGameData = false; // 已安装目录模式：完成后展开 GameData 根节点
    QString m_currentModIdentifier;
    ckan::CkanModule m_currentMod; // 当前选中模组（供「文件」tab 懒加载时取用）
    bool m_contentsStale = true;   // 「文件」tab 清单是否待构建（懒加载：切到该 tab 才建树）
    bool m_modsReady = false;    // 索引与 DLL 扫描均就绪，模组模型已填充
    bool m_modsTabActive = false; // 当前是否正显示"模组管理"tab（用于加载提示/轮询）
    bool m_registryLockWaiting = false;
    QTimer* m_registryLockPollTimer = nullptr;
    // 待安装的 .ckan 导入标识符（索引就绪后自动触发批量安装）
    QStringList m_pendingCkanIdentifiers;
    // 搜索输入防抖定时器（150ms，singleShot）
    QTimer* m_searchDebounceTimer = nullptr;
    // 模组列表列宽持久化：拖动后防抖落盘
    QTimer* m_colWidthSaveTimer = nullptr;
    // 模组列表 UI 状态持久化（每实例，随改随存）：防抖定时器 + 待还原状态
    QTimer* m_stateSaveTimer = nullptr;
    QJsonObject m_pendingState; // 本次实例待还原状态（数据就绪后消费；空=无已存状态走默认）
    bool m_restorePending = false; // 是否处于"进入实例的首次加载"（区分刷新，避免刷新时误重置标签）
};

#endif // MODSTABPAGE_H