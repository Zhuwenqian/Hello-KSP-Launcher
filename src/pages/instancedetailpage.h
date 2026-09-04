#ifndef INSTANCEDETAILPAGE_H
#define INSTANCEDETAILPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMenu>
#include <QTableView>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QCheckBox>
#include <QTabWidget>
#include <QTreeWidgetItem>
#include <QFutureWatcher>
#include <QStringList>
#include "../configmanager.h"
#include "../instancemanager.h"
#include "modtablemodel.h"

class QTimer;

class InstanceDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstanceDetailPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadCurrentInstance();
    void refreshIcons(const QString& color);
    // 外部切换详情页的二级 tab：0=游戏设置 1=DLC 2=模组管理 3=高级（供存档等实例子页跳回）
    void showSection(int detailIndex);
    // 供存档等实例子页触发原属于详情页侧栏的动作
    void triggerExportModpack();
    void triggerImportModpack();
    void triggerBrowse();

signals:
    void backClicked();
    void savesManageRequested();

private slots:
    void onBackClicked();
    void onNavButtonClicked();
    void refreshData();
    void onSaveSettingsClicked();
    void onSaveLaunchArgsClicked();
    void onExportModpackClicked();
    void onImportModpackClicked();
    void onBrowseClicked();
    void onBrowseActionTriggered();
    // 设置搜索：按设置项显示名过滤设置树
    void onSettingsSearchChanged(const QString &text);
    // mod 管理
    void onModSearchChanged(const QString &text);
    void onModFilterChanged(int index);
    void onTagFilterChanged(int index);
    void rebuildTagFilter();
    void onShowIncompatibleToggled(bool checked);
    void onCompatVersionsClicked();
    void onRefreshModsClicked();
    void onModSelectionChanged();
    void onModDoubleClicked(const QModelIndex &index);
    void onInstallModClicked();
    void onUninstallModClicked();
    void onUpgradeModClicked();
    void onSelectAllClicked();
    void onIndexRefreshed(bool ok, const QString &error);
    void onUnmanagedScanFinished();
    void onModOperationFinished(bool ok, const QString &message);
    void onDownloadProgress(const QString &identifier, qint64 doneBytes,
                            qint64 totalBytes, qint64 speedBps);
    void onCancelDownloadClicked();
    void onImportModClicked();
    void onShowHistoryClicked();

    // 模组详情四 tab
    void onSingleDownloadFinished(bool ok, const QString &identifier, const QString &error);
    void onContentsDownloadClicked();
    void onReverseRelToggled(bool on);
    void onRelationItemExpanded(QTreeWidgetItem *item);
    void onVersionSelectionChanged();
    void onVersionInstallClicked();

private:
    void setupUI();
    void setupGameSettingsTab();
    void setupDLCTab();
    void setupModsTab();
    void setupAdvancedTab();
    void setupBrowseMenu();
    void updateBrowseMenuState();
    void openBrowseTarget(const QString& path);
    void loadGameSettings();
    bool saveGameSettings();
    void loadDLCs();
    // 非阻塞准备模组数据：绑定实例、后台 DLL 扫描、必要时异步加载索引；
    // 索引与扫描都就绪后自动填充模型（maybePopulateMods）。
    void prepareMods();
    // 索引与 DLL 扫描均就绪时填充模组模型并刷新按钮；未就绪则清空并给出加载提示。
    void maybePopulateMods();
    void loadLaunchArgs();
    // 整合包导出：打包 GameData 为 ZIP / 导出为 CKAN 元包
    void exportAsZip();
    void exportAsCkan();
    // 整合包导入：从 zip 解压到 GameData / 解析 .ckan 后在模组管理界面安装
    void importFromZip();
    void importFromCkan();
    // 将当前实例勾选的兼容版本区间应用到过滤代理与 CKanManager（供安装/依赖解析使用）
    void applyCompatRange();
    void updateModActionButtons();
    void updateSelectAllButtonText();
    void setModButtonsEnabled(bool enabled);
    void showModDetails(const ckan::CkanModule &mod);
    void setDetailNote(const QString &text);                 // 状态提示写入元数据 tab
    void showMetaTab(const ckan::CkanModule &mod);
    void showContentsTab(const ckan::CkanModule &mod);
    void showRelationshipsTab(const ckan::CkanModule &mod, bool reverse);
    void addRelationChildren(QTreeWidgetItem *parent, const QString &identifier, int depth);
    void showVersionsTab(const ckan::CkanModule &mod);
    void showDownloadProgress();
    void hideDownloadProgress();

    QList<GameSetting> m_currentSettings;

    QString m_instanceId;
    KSPInstance m_instance;

    QPushButton* m_backButton;
    QLabel* m_titleLabel;
    QWidget* m_detailSidebar;
    QStackedWidget* m_contentStack;

    QPushButton* m_gameSettingsBtn;
    QPushButton* m_dlcBtn;
    QPushButton* m_modsBtn;
    QPushButton* m_savesBtn;
    QPushButton* m_advancedBtn;
    QPushButton* m_exportModpackBtn;
    QPushButton* m_importModpackBtn;

    // 浏览菜单
    QPushButton* m_browseBtn;
    QMenu* m_browseMenu;
    QAction* m_browseRootAction;
    QList<QAction*> m_browseActions;
    QStringList m_browsePaths;

    // Game Settings tab
    QTreeWidget* m_settingsTree;
    QLineEdit* m_settingsSearchEdit;

    // DLC tab
    QListWidget* m_dlcList;

    // Mods tab
    ModsTableModel* m_modsModel;
    ModsFilterProxyModel* m_modsProxy;
    QTableView* m_modTable;
    QLineEdit* m_modSearchEdit;
    QComboBox* m_modFilterCombo;
    QComboBox* m_tagFilterCombo; // 按仓库自带 tag 筛选
    QPushButton* m_refreshModsBtn;
    QPushButton* m_selectAllBtn;
    QCheckBox*   m_showIncompatCheck;
    QPushButton* m_compatBtn; // 兼容版本设置按钮
    QPushButton* m_installModBtn;
    QPushButton* m_uninstallModBtn;
    QPushButton* m_upgradeModBtn;
    QPushButton* m_importModBtn;  // 导入单模组文件（.zip/.ckan）
    QPushButton* m_historyBtn;    // 查看安装历史
    // 模组详情四 tab
    QTabWidget*  m_modDetailTabs;      // 元数据 / 文件 / 关系 / 版本
    QTextEdit*   m_metaText;           // 元数据 tab
    QTreeWidget* m_contentsTree;       // 文件清单 tab
    QLabel*      m_contentsStatusLabel;
    QPushButton* m_contentsDownloadBtn;
    QTreeWidget* m_relTree;            // 关系 tab（懒加载树，可切反向）
    QCheckBox*   m_reverseRelCheck;
    QTreeWidget* m_versionsTree;       // 版本历史 tab
    QPushButton* m_versionsInstallBtn;
    QFutureWatcher<QStringList>* m_reverseWatcher = nullptr; // 反向关系扫描在途
    QString m_currentModIdentifier;
    bool m_modsReady = false;   // 索引与 DLL 扫描均就绪，模组模型已填充
    bool m_modsTabActive = false; // 当前是否正显示"模组管理"tab（用于加载提示）
    // 待安装的 .ckan 导入标识符（索引就绪后自动触发批量安装）
    QStringList m_pendingCkanIdentifiers;
    // 下载进度条与取消
    QWidget*  m_modProgressWidget;
    QProgressBar* m_modProgressBar;
    QLabel*   m_modProgressLabel;
    QPushButton* m_cancelDownloadBtn;
    // 模组列表列宽持久化：拖动后防抖落盘
    QTimer*   m_colWidthSaveTimer = nullptr;

    // Advanced tab（高级页即启动配置 Profile）
    QLineEdit* m_launchArgsEdit;
    QSpinBox* m_launchMemorySpin;     // 内存上限 MB，0=不限制
    QComboBox* m_launchPriorityCombo; // 0=低(不处理) 1=高
    QPushButton* m_saveLaunchArgsBtn;
};

#endif // INSTANCEDETAILPAGE_H
