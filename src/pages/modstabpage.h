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

#include "ckan/ckanmodule.h"
#include "modscontroller.h"
#include "modtablemodel.h"
#include "../configmanager.h"
#include "../instancemanager.h"

class QTimer;

// 实例详情页的"模组管理"二级页：承载全部模组 UI
// （搜索/筛选/标签/表格/详情四 tab/下载进度/操作按钮/导入与安装历史）。
// 业务编排经内部的 ModsController（其持有真实 Qt 决策弹窗并转发操作与信号）；
// 只读查询直接走 CKanManager 单例。
class ModsTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit ModsTabPage(QWidget *parent = nullptr);

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
    // 模组详情四 tab
    void onSingleDownloadFinished(bool ok, const QString &identifier, const QString &error);
    void onContentsDownloadClicked();
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
    void showModDetails(const ckan::CkanModule &mod);
    void setDetailNote(const QString &text);
    void showMetaTab(const ckan::CkanModule &mod);
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
    QString m_currentModIdentifier;
    bool m_modsReady = false;    // 索引与 DLL 扫描均就绪，模组模型已填充
    bool m_modsTabActive = false; // 当前是否正显示"模组管理"tab（用于加载提示/轮询）
    bool m_registryLockWaiting = false;
    QTimer* m_registryLockPollTimer = nullptr;
    // 待安装的 .ckan 导入标识符（索引就绪后自动触发批量安装）
    QStringList m_pendingCkanIdentifiers;
    // 模组列表列宽持久化：拖动后防抖落盘
    QTimer* m_colWidthSaveTimer = nullptr;
};

#endif // MODSTABPAGE_H