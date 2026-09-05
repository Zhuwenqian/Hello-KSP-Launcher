// 实例详情页 - 模组管理二级页（承载全部模组 UI）
#include "modstabpage.h"
#include "../ckanmanager.h"
#include "../configmanager.h"
#include "../iconutils.h"
#include "ckan/version.h"
#include "ckan/moduleinstaller.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QSet>
#include <QItemSelectionModel>
#include <QTextEdit>
#include <QListWidget>
#include <QSplitter>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QAbstractScrollArea>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

namespace {
QString formatBytes(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("?");
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return QStringLiteral("%1 MB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
}

// 递归把 absDir 下所有内容加入 parent（子目录标记 dir 并可继续展开，文件显示大小）。
// 供「文件」tab 无缓存但已安装时，直接浏览已安装模组目录。
void addDirContentsToTree(QTreeWidgetItem *parent, const QString &absDir)
{
    QDir d(absDir);
    const QFileInfoList infos = d.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);
    for (const QFileInfo &fi : infos) {
        QTreeWidgetItem *item = new QTreeWidgetItem(
            parent, {fi.fileName(), fi.isFile() ? formatBytes(fi.size()) : QString()});
        if (fi.isDir()) {
            item->setData(0, Qt::UserRole, QStringLiteral("dir"));
            addDirContentsToTree(item, fi.absoluteFilePath());
        }
    }
}
} // namespace

ModsTabPage::ModsTabPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setTabActive(false);
}

void ModsTabPage::setupUi()
{
    setObjectName("modsContentWidget");
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(8);

    // 顶栏：搜索 + 筛选 + 刷新仓库
    QWidget* topBar = new QWidget(this);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);

    m_modSearchEdit = new QLineEdit(topBar);
    m_modSearchEdit->setPlaceholderText(tr("搜索模组名称或标识符..."));
    m_modSearchEdit->setClearButtonEnabled(true);
    m_modSearchEdit->setMinimumHeight(34);
    connect(m_modSearchEdit, &QLineEdit::textChanged, this, &ModsTabPage::onModSearchChanged);

    m_modFilterCombo = new QComboBox(topBar);
    m_modFilterCombo->addItem(tr("全部"), -1);
    m_modFilterCombo->addItem(tr("已安装"), static_cast<int>(ModsTableModel::Installed));
    m_modFilterCombo->addItem(tr("可升级"), static_cast<int>(ModsTableModel::Upgradable));
    m_modFilterCombo->addItem(tr("未安装"), static_cast<int>(ModsTableModel::NotInstalled));
    m_modFilterCombo->setMinimumHeight(34);
    connect(m_modFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModsTabPage::onModFilterChanged);

    // 按仓库自带 tag 筛选
    m_tagFilterCombo = new QComboBox(topBar);
    m_tagFilterCombo->addItem(tr("全部标签"), QString());
    m_tagFilterCombo->setMinimumHeight(34);
    connect(m_tagFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModsTabPage::onTagFilterChanged);

    m_refreshModsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/refresh.svg", "#ffffff"),
                                       tr(" 刷新仓库"), topBar);
    m_refreshModsBtn->setObjectName("primaryButton");
    m_refreshModsBtn->setMinimumHeight(34);
    connect(m_refreshModsBtn, &QPushButton::clicked, this, &ModsTabPage::onRefreshModsClicked);

    m_selectAllBtn = new QPushButton(tr(" 全选"), topBar);
    m_selectAllBtn->setObjectName("primaryButton");
    m_selectAllBtn->setMinimumHeight(34);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &ModsTabPage::onSelectAllClicked);

    m_showIncompatCheck = new QCheckBox(tr("显示不兼容"), topBar);
    m_showIncompatCheck->setMinimumHeight(34);
    m_showIncompatCheck->setChecked(ConfigManager::instance().showIncompatibleMods());
    connect(m_showIncompatCheck, &QCheckBox::toggled, this, &ModsTabPage::onShowIncompatibleToggled);

    m_compatBtn = new QPushButton(IconUtils::tintedIcon(":/icons/rocket.svg", "#ffffff"),
                                  tr(" 兼容版本"), topBar);
    m_compatBtn->setMinimumHeight(34);
    connect(m_compatBtn, &QPushButton::clicked, this, &ModsTabPage::onCompatVersionsClicked);

    topLayout->addWidget(m_modSearchEdit, 1);
    topLayout->addWidget(m_modFilterCombo);
    topLayout->addWidget(m_tagFilterCombo);
    topLayout->addWidget(m_showIncompatCheck);
    topLayout->addWidget(m_compatBtn);
    topLayout->addWidget(m_selectAllBtn);
    topLayout->addWidget(m_refreshModsBtn);
    layout->addWidget(topBar);

    // 表格
    m_modsModel = new ModsTableModel(this);
    m_modsProxy = new ModsFilterProxyModel(this);
    m_modsProxy->setSourceModel(m_modsModel);

    m_modTable = new QTableView(this);
    m_modTable->setModel(m_modsProxy);
    m_modTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modTable->verticalHeader()->setVisible(false);
    // 列宽：除勾选列外均可拖动调整（Excel 式），宽度持久化到全局设置。
    QHeaderView* hHeader = m_modTable->horizontalHeader();
    hHeader->setStretchLastSection(true);
    const QVector<int> colWidths = ConfigManager::instance().modTableColumnWidths();
    for (int col = 0; col < ModsTableModel::ColumnCount; ++col) {
        if (col == ModsTableModel::ColCheck) {
            hHeader->setSectionResizeMode(col, QHeaderView::Fixed);
            m_modTable->setColumnWidth(col, colWidths.value(col, 40));
        } else {
            hHeader->setSectionResizeMode(col, QHeaderView::Interactive);
            m_modTable->setColumnWidth(col, colWidths.value(col));
        }
    }
    // 拖动结束后（防抖 250ms）一次性写入设置；末列由拉尾布局分配宽度，不据其持久化
    m_colWidthSaveTimer = new QTimer(this);
    m_colWidthSaveTimer->setSingleShot(true);
    m_colWidthSaveTimer->setInterval(250);
    connect(m_colWidthSaveTimer, &QTimer::timeout, this, [this]() {
        QVector<int> w(ModsTableModel::ColumnCount);
        for (int col = 0; col < ModsTableModel::ColumnCount; ++col)
            w[col] = m_modTable->columnWidth(col);
        ConfigManager::instance().setModTableColumnWidths(w);
    });
    connect(hHeader, &QHeaderView::sectionResized, this, [this](int logical) {
        if (logical == ModsTableModel::ColumnCount - 1) return; // 末列为拉尾列，布局会覆盖宽度
        m_colWidthSaveTimer->start();
    });
    // 双击某列表头：恢复该列内置默认宽度
    connect(hHeader, &QHeaderView::sectionHandleDoubleClicked, this, [this, hHeader](int logical) {
        const QVector<int> def = ConfigManager::defaultModTableColumnWidths();
        hHeader->resizeSection(logical, def.value(logical, m_modTable->columnWidth(logical)));
    });
    m_modTable->setSortingEnabled(true);
    m_modTable->sortByColumn(ModsTableModel::ColName, Qt::AscendingOrder);
    connect(m_modTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ModsTabPage::onModSelectionChanged);
    connect(m_modTable, &QTableView::doubleClicked, this, &ModsTabPage::onModDoubleClicked);
    connect(m_modsModel, &ModsTableModel::dataChanged, this, [this]() {
        updateSelectAllButtonText();
        updateModActionButtons();
    });
    layout->addWidget(m_modTable, 1);

    // 下载进度条（任务进行中显示）
    m_modProgressWidget = new QWidget(this);
    QHBoxLayout* progLayout = new QHBoxLayout(m_modProgressWidget);
    progLayout->setContentsMargins(0, 0, 0, 0);
    progLayout->setSpacing(8);
    m_modProgressBar = new QProgressBar(m_modProgressWidget);
    m_modProgressBar->setRange(0, 1000);
    m_modProgressBar->setValue(0);
    m_modProgressBar->setTextVisible(false);
    m_modProgressLabel = new QLabel(tr("就绪"), m_modProgressWidget);
    m_modProgressLabel->setMinimumWidth(220);
    m_cancelDownloadBtn = new QPushButton(IconUtils::tintedIcon(":/icons/x.svg", "#ffffff"),
                                          tr(" 取消"), m_modProgressWidget);
    m_cancelDownloadBtn->setObjectName("dangerButton");
    connect(m_cancelDownloadBtn, &QPushButton::clicked, this, &ModsTabPage::onCancelDownloadClicked);
    progLayout->addWidget(m_modProgressBar, 1);
    progLayout->addWidget(m_modProgressLabel);
    progLayout->addWidget(m_cancelDownloadBtn);
    m_modProgressWidget->setVisible(false);
    layout->addWidget(m_modProgressWidget);

    // 底部：详情（四 tab）+ 操作按钮
    m_modDetailTabs = new QTabWidget(this);
    m_modDetailTabs->setMinimumHeight(210);
    m_modDetailTabs->setObjectName("modDetailTabs");

    // ① 元数据 tab
    m_metaText = new QTextEdit(m_modDetailTabs);
    m_metaText->setReadOnly(true);
    m_metaText->setPlaceholderText(tr("选中一个模组查看详情"));
    m_modDetailTabs->addTab(m_metaText, tr("元数据"));

    // ② 文件（Contents）tab
    QWidget* contentsWrap = new QWidget(m_modDetailTabs);
    QVBoxLayout* contentsLay = new QVBoxLayout(contentsWrap);
    contentsLay->setContentsMargins(0, 0, 0, 0);
    contentsLay->setSpacing(4);
    QHBoxLayout* contentsBar = new QHBoxLayout;
    contentsBar->setContentsMargins(0, 0, 0, 0);
    m_contentsStatusLabel = new QLabel(tr("仅显示已缓存的压缩包内容。"), contentsWrap);
    m_contentsDownloadBtn = new QPushButton(
        IconUtils::tintedIcon(":/icons/download.svg", "#ffffff"),
        tr(" 下载压缩包"), contentsWrap);
    m_contentsDownloadBtn->setObjectName("primaryButton");
    m_contentsDownloadBtn->setMinimumHeight(28);
    connect(m_contentsDownloadBtn, &QPushButton::clicked,
            this, &ModsTabPage::onContentsDownloadClicked);
    contentsBar->addWidget(m_contentsStatusLabel, 1);
    contentsBar->addWidget(m_contentsDownloadBtn);
    contentsLay->addLayout(contentsBar);
    m_contentsTree = new QTreeWidget(contentsWrap);
    m_contentsTree->setHeaderLabels({tr("文件"), tr("大小")});
    m_contentsTree->setRootIsDecorated(false);
    m_contentsTree->header()->setStretchLastSection(false);
    m_contentsTree->setColumnWidth(0, 240);
    contentsLay->addWidget(m_contentsTree, 1);
    m_modDetailTabs->addTab(contentsWrap, tr("文件"));

    // ③ 关系（Relationships）tab：前向/反向树（懒加载）
    QWidget* relWrap = new QWidget(m_modDetailTabs);
    QVBoxLayout* relLay = new QVBoxLayout(relWrap);
    relLay->setContentsMargins(0, 0, 0, 0);
    relLay->setSpacing(4);
    m_reverseRelCheck = new QCheckBox(tr("显示反向关系（哪些模组依赖/引用当前模组）"), relWrap);
    connect(m_reverseRelCheck, &QCheckBox::toggled, this, &ModsTabPage::onReverseRelToggled);
    relLay->addWidget(m_reverseRelCheck);
    m_relTree = new QTreeWidget(relWrap);
    m_relTree->setHeaderLabels({tr("关系"), tr("模组")});
    m_relTree->header()->setStretchLastSection(true);
    m_relTree->setRootIsDecorated(true);
    m_relTree->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
    connect(m_relTree, &QTreeWidget::itemExpanded, this, &ModsTabPage::onRelationItemExpanded);
    relLay->addWidget(m_relTree, 1);
    m_modDetailTabs->addTab(relWrap, tr("关系"));

    // ④ 版本（Versions）tab：历史版本降序 + 一键安装
    QWidget* verWrap = new QWidget(m_modDetailTabs);
    QVBoxLayout* verLay = new QVBoxLayout(verWrap);
    verLay->setContentsMargins(0, 0, 0, 0);
    verLay->setSpacing(4);
    m_versionsTree = new QTreeWidget(verWrap);
    m_versionsTree->setHeaderLabels({tr("版本"), tr("发布日期"), tr("大小"), tr("状态")});
    m_versionsTree->header()->setStretchLastSection(true);
    m_versionsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_versionsTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_versionsTree, &QTreeWidget::itemSelectionChanged,
            this, &ModsTabPage::onVersionSelectionChanged);
    verLay->addWidget(m_versionsTree, 1);
    m_versionsInstallBtn = new QPushButton(
        IconUtils::tintedIcon(":/icons/download.svg", "#ffffff"),
        tr(" 安装此版本"), verWrap);
    m_versionsInstallBtn->setObjectName("primaryButton");
    m_versionsInstallBtn->setMinimumHeight(30);
    m_versionsInstallBtn->setEnabled(false);
    connect(m_versionsInstallBtn, &QPushButton::clicked,
            this, &ModsTabPage::onVersionInstallClicked);
    verLay->addWidget(m_versionsInstallBtn);
    m_modDetailTabs->addTab(verWrap, tr("版本"));

    layout->addWidget(m_modDetailTabs);

    QWidget* btnBar = new QWidget(this);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    // 文件操作（左侧）：导入单模组 / 查看安装历史
    m_importModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/folder-open.svg", "#ffffff"),
                                     tr(" 导入模组"), btnBar);
    m_importModBtn->setMinimumHeight(36);
    connect(m_importModBtn, &QPushButton::clicked, this, &ModsTabPage::onImportModClicked);

    m_historyBtn = new QPushButton(IconUtils::tintedIcon(":/icons/list.svg", "#ffffff"),
                                   tr(" 安装历史"), btnBar);
    m_historyBtn->setMinimumHeight(36);
    connect(m_historyBtn, &QPushButton::clicked, this, &ModsTabPage::onShowHistoryClicked);

    m_installModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/add.svg", "#ffffff"),
                                      tr(" 安装"), btnBar);
    m_installModBtn->setObjectName("primaryButton");
    m_installModBtn->setMinimumHeight(36);
    connect(m_installModBtn, &QPushButton::clicked, this, &ModsTabPage::onInstallModClicked);

    m_upgradeModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/check.svg", "#ffffff"),
                                      tr(" 升级"), btnBar);
    m_upgradeModBtn->setObjectName("primaryButton");
    m_upgradeModBtn->setMinimumHeight(36);
    connect(m_upgradeModBtn, &QPushButton::clicked, this, &ModsTabPage::onUpgradeModClicked);

    m_uninstallModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#ffffff"),
                                        tr(" 卸载"), btnBar);
    m_uninstallModBtn->setObjectName("dangerButton");
    m_uninstallModBtn->setMinimumHeight(36);
    connect(m_uninstallModBtn, &QPushButton::clicked, this, &ModsTabPage::onUninstallModClicked);

    btnLayout->addWidget(m_importModBtn);
    btnLayout->addWidget(m_historyBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_installModBtn);
    btnLayout->addWidget(m_upgradeModBtn);
    btnLayout->addWidget(m_uninstallModBtn);
    layout->addWidget(btnBar);

    // 适配层信号（经 ModsController 统一转发）
    connect(&m_controller, &ModsController::indexRefreshed,
            this, &ModsTabPage::onIndexRefreshed);
    connect(&m_controller, &ModsController::unmanagedScanFinished,
            this, &ModsTabPage::onUnmanagedScanFinished);
    connect(&m_controller, &ModsController::operationFinished,
            this, &ModsTabPage::onModOperationFinished);
    connect(&m_controller, &ModsController::installProgress,
            this, [this](const QString &id, int percent) {
        if (id == m_currentModIdentifier)
            setDetailNote(tr("正在处理 %1 ... %2%").arg(id).arg(percent));
        // 进入安装阶段：进度条切为不确定模式，文案改为“正在安装”
        if (!m_modProgressWidget->isVisible())
            m_modProgressWidget->setVisible(true);
        m_modProgressBar->setRange(0, 0);
        m_modProgressLabel->setText(tr("正在安装：%1").arg(id));
        m_cancelDownloadBtn->setEnabled(true);
    });
    connect(&m_controller, &ModsController::downloadProgress,
            this, &ModsTabPage::onDownloadProgress);
    connect(&m_controller, &ModsController::singleDownloadFinished,
            this, &ModsTabPage::onSingleDownloadFinished);

    updateModActionButtons();
}

void ModsTabPage::setInstance(const KSPInstance &inst, const QString &instanceId)
{
    m_instance = inst;
    m_instanceId = instanceId;
    prepareMods();
}

void ModsTabPage::prepareMods()
{
    if (m_instance.path.isEmpty()) return;

    // 绑定实例（同一实例时内部直接返回，不会重复构造/扫描）
    m_controller.openInstance(m_instance.path, m_instance.name);

    if (m_modsProxy) {
        m_modsProxy->setShowIncompatible(ConfigManager::instance().showIncompatibleMods());
        // 传入实际检测到的 KSP 版本，按真实游戏版本过滤不兼容模组
        m_modsProxy->setGameVersion(CKanManager::instance().detectedVersion());
    }
    // 应用用户勾选的兼容版本区间（过滤代理 + 安装/依赖解析共用）
    applyCompatRange();

    // 注册表写锁门控：被其他进程占用时不加载，等待其释放。
    if (!m_controller.tryAcquireRegistryLock()) {
        startRegistryLockWait();
        return;
    }
    stopRegistryLockWait();
    prepareModsLoading();
}

// 已持有注册表写锁后的真正装载：加载/刷新索引 + DLL 扫描。
void ModsTabPage::prepareModsLoading()
{
    CKanManager &mgr = CKanManager::instance();
    if (!mgr.indexReady()) {
        // 需要加载/刷新索引：加载索引的同时强制重扫一次 DLL，
        // 保持手动安装（AD）模组识别最新（新放入/移除的模组 DLL 即时反映）
        m_modsModel->clear();
        setDetailNote(tr("正在加载 CKAN 仓库索引，请稍候..."));
        showDownloadProgress();
        m_controller.requestRefreshIndex(false);
        m_controller.requestScanDlls(true);
    } else if (!mgr.unmanagedScanDone()) {
        // 索引已就绪但本实例 DLL 扫描尚未完成（如首次构造）→ 后台扫描
        m_modsModel->clear();
        m_controller.requestScanDlls(false);
    }

    maybePopulateMods();
}

// 前台/离开切换：进入模组页时补"加载中"提示或刷新按钮态，并控制注册表锁轮询开关。
void ModsTabPage::setTabActive(bool active)
{
    m_modsTabActive = active;
    // 注册表锁等待仅在该页前台时轮询
    if (active && m_registryLockWaiting && m_registryLockPollTimer
        && !m_registryLockPollTimer->isActive()) {
        m_registryLockPollTimer->start();
    } else if (!active && m_registryLockWaiting && m_registryLockPollTimer) {
        m_registryLockPollTimer->stop();
    }
    if (!active) return;
    // 数据未就绪时给出"加载中"提示（后台扫描/索引加载完成会自动填充）
    if (!m_modsReady) {
        CKanManager &mgr = CKanManager::instance();
        if (!mgr.indexReady())
            setDetailNote(tr("正在加载 CKAN 仓库索引，请稍候..."));
        else
            setDetailNote(tr("正在扫描已安装的 DLL，请稍候..."));
    } else {
        // 确保刷新按钮状态与选中态一致
        updateModActionButtons();
    }
}

// .ckan 整合包导入：跳转后带待装清单，索引就绪后自动批量安装。
void ModsTabPage::queueCkanInstall(const QStringList &identifiers)
{
    m_pendingCkanIdentifiers = identifiers;
    prepareMods();
    if (CKanManager::instance().indexReady()) {
        m_pendingCkanIdentifiers.clear();
        m_controller.requestInstallBatch(identifiers);
    } else {
        setDetailNote(tr("正在加载 CKAN 仓库索引，就绪后将自动开始安装所选模组..."));
    }
}

void ModsTabPage::refreshIcons(const QString &color)
{
    m_refreshModsBtn->setIcon(IconUtils::tintedIcon(":/icons/refresh.svg", color));
    m_installModBtn->setIcon(IconUtils::tintedIcon(":/icons/add.svg", color));
    m_uninstallModBtn->setIcon(IconUtils::tintedIcon(":/icons/trash-2.svg", color));
    m_upgradeModBtn->setIcon(IconUtils::tintedIcon(":/icons/check.svg", color));
    m_compatBtn->setIcon(IconUtils::tintedIcon(":/icons/rocket.svg", color));
}

// 注册表写锁被其他进程占用：弹窗告知，清空表格、禁用模组按钮，启动 10s 轮询等待。
void ModsTabPage::startRegistryLockWait()
{
    if (!m_registryLockPollTimer) {
        m_registryLockPollTimer = new QTimer(this);
        m_registryLockPollTimer->setInterval(10000);
        connect(m_registryLockPollTimer, &QTimer::timeout,
                this, &ModsTabPage::onRegistryLockPollTick);
    }

    if (m_registryLockWaiting) {
        // 已在等待：仅维持等待态（不重复弹窗），确保禁用按钮并持续轮询
        m_modsModel->clear();
        setDetailNote(tr("等待其他程序释放注册表锁..."));
        setModButtonsEnabled(false);
        if (m_modsTabActive && !m_registryLockPollTimer->isActive())
            m_registryLockPollTimer->start();
        return;
    }

    m_registryLockWaiting = true;
    // 首次进入：弹出说明，确认后开始轮询
    QMessageBox::information(this, tr("注册表已锁定"),
        tr("当前实例注册表已上锁，正在被其他程序（官方 CKAN 或另一个启动器）使用。\n\n"
           "请您关闭其他启动器实例或官方 CKAN 后，本启动器会自动继续加载模组列表。"));
    m_modsModel->clear();
    setDetailNote(tr("等待其他程序释放注册表锁..."));
    setModButtonsEnabled(false);
    if (m_modsTabActive)
        m_registryLockPollTimer->start();
}

void ModsTabPage::stopRegistryLockWait()
{
    if (m_registryLockPollTimer)
        m_registryLockPollTimer->stop();
    m_registryLockWaiting = false;
}

void ModsTabPage::onRegistryLockPollTick()
{
    // 决策：仅当"模组管理"tab 处于前台时才轮询，离开即停止
    if (!m_modsTabActive) {
        m_registryLockPollTimer->stop();
        return;
    }
    if (!m_controller.tryAcquireRegistryLock()) return; // 锁仍被占用
    // 其他程序已释放锁且本进程取得锁：结束等待，恢复按钮并重新装载
    stopRegistryLockWait();
    setModButtonsEnabled(true);
    prepareModsLoading();
}

// 索引与 DLL 扫描均就绪时异步填充模组模型；否则保持清空并显示"加载中"提示。
void ModsTabPage::maybePopulateMods()
{
    CKanManager &mgr = CKanManager::instance();
    m_modsReady = mgr.indexReady() && mgr.unmanagedScanDone();
    if (m_modsReady) {
        // 整张 mod 列表的构建（search：遍历索引、逐标识符求最新版）在大索引下较耗时。
        // 放到后台线程执行，完成后由 onModsLoadFinished 在主线程填充模型，避免阻塞 UI。
        if (!m_modsLoadWatcher) {
            m_modsLoadWatcher = new QFutureWatcher<QVector<ckan::CkanModule>>(this);
            connect(m_modsLoadWatcher, &QFutureWatcher<QVector<ckan::CkanModule>>::finished,
                    this, &ModsTabPage::onModsLoadFinished);
        }
        // 连发多次时 setFuture 只保留最新一次；旧任务虽然继续跑，但其结果不再派发到这里
        m_modsLoadWatcher->setFuture(QtConcurrent::run([&mgr]() {
            return mgr.search(QString());
        }));
        return;
    }
    // 数据尚未就绪：若用户已切到模组页，给出明确的"加载中"提示
    m_modsModel->clear();
    if (m_modsTabActive) {
        if (!mgr.indexReady())
            setDetailNote(tr("正在加载 CKAN 仓库索引，请稍候..."));
        else
            setDetailNote(tr("正在扫描已安装的 DLL，请稍候..."));
    }
}

// 后台 mod 列表构建完成，回主线程填充模型并重算依赖 UI。
void ModsTabPage::onModsLoadFinished()
{
    if (!m_modsLoadWatcher || !m_modsReady) return;
    m_modsModel->setModules(m_modsLoadWatcher->result());
    rebuildTagFilter();
    updateModActionButtons();
    const int n = m_modsModel->rowCount();
    setDetailNote(tr("仓库索引已就绪，共 %1 个模组。").arg(n));
}

void ModsTabPage::onUnmanagedScanFinished()
{
    // 后台 DLL 扫描完成：若索引也已就绪则填充模型（含此前切到模组页的场景）
    maybePopulateMods();
    updateModActionButtons();
}

void ModsTabPage::onModSearchChanged(const QString &text)
{
    if (m_modsProxy) m_modsProxy->setSearchText(text);
}

void ModsTabPage::onModFilterChanged(int index)
{
    if (!m_modsProxy || !m_modFilterCombo) return;
    m_modsProxy->setStatusFilter(m_modFilterCombo->itemData(index).toInt());
}

void ModsTabPage::onTagFilterChanged(int index)
{
    if (!m_modsProxy || !m_tagFilterCombo) return;
    m_modsProxy->setTagFilter(m_tagFilterCombo->itemData(index).toString());
}

// 从模型当前模块重刷「标签」下拉（保留当前选中项），并清空多余项
void ModsTabPage::rebuildTagFilter()
{
    if (!m_tagFilterCombo || !m_modsModel) return;
    const QString current = m_tagFilterCombo->currentData().toString();

    QSet<QString> tags;
    const int rows = m_modsModel->rowCount();
    for (int r = 0; r < rows; ++r) {
        const ckan::CkanModule mod = m_modsModel->moduleAt(r);
        for (const QString &t : mod.tags)
            if (!t.trimmed().isEmpty()) tags.insert(t);
    }
    QStringList sorted = tags.values();
    sorted.sort(Qt::CaseInsensitive);

    m_tagFilterCombo->blockSignals(true);
    m_tagFilterCombo->clear();
    m_tagFilterCombo->addItem(tr("全部标签"), QString());
    for (const QString &t : sorted)
        m_tagFilterCombo->addItem(t, t);
    // 恢复当前选中项；若已失效（如刷新后该标签消失）则归位到「全部标签」
    int idx = m_tagFilterCombo->findData(current);
    m_tagFilterCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_tagFilterCombo->blockSignals(false);

    // 数据变了，按当前选中项重新过滤（findData 用精确字符串，tag 大小写敏感）
    m_modsProxy->setTagFilter(m_tagFilterCombo->currentData().toString());
}

void ModsTabPage::onShowIncompatibleToggled(bool checked)
{
    ConfigManager::instance().setShowIncompatibleMods(checked);
    if (m_modsProxy) m_modsProxy->setShowIncompatible(checked);
}

// 将当前实例勾选的兼容版本区间应用到过滤代理与 CKanManager（供安装/依赖解析使用）
void ModsTabPage::applyCompatRange()
{
    if (m_instanceId.isEmpty()) return;
    // 未配置时按检测到的游戏版本动态推导默认勾选（1.9~1.12 区间内回退到 1.9）
    const ckan::GameVersion detected = CKanManager::instance().detectedVersion();
    const QStringList lines = ConfigManager::instance().compatibleVersions(m_instanceId, detected);
    const ckan::GameVersionRange range = ckan::versionLinesToRange(lines);
    if (m_modsProxy)
        m_modsProxy->setCompatRange(range);
    CKanManager::instance().setCompatRange(range);
    if (m_compatBtn) {
        // 按钮文案显示当前已勾选的版本数，空勾选则仅按实例实际版本判断
        if (lines.isEmpty()) {
            m_compatBtn->setText(tr(" 兼容版本"));
            m_compatBtn->setToolTip(tr("仅按当前实例实际版本判断兼容性"));
        } else {
            m_compatBtn->setText(tr(" 兼容版本(%1)").arg(lines.size()));
            m_compatBtn->setToolTip(tr("已勾选：%1").arg(lines.join(QStringLiteral("、"))));
        }
    }
}

// 兼容版本设置弹窗：勾选 1.0~1.12 全部版本线（未配置时按游戏版本动态推导默认，
// 1.9~1.12 区间内回退到 1.9，低于 1.9 仅勾选自身版本线），
// 勾选后 KSP 版本落在所选连续区间内的模组均视为兼容；全部取消则仅按实例实际版本判断。
void ModsTabPage::onCompatVersionsClicked()
{
    if (m_instanceId.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("兼容版本设置"));
    dlg.setMinimumWidth(360);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QLabel *info = new QLabel(tr("勾选需要兼容的 KSP 版本（1.0 ~ 1.12）。\n"
                                 "勾选后，KSP 版本落在所选区间内的模组均视为兼容。\n"
                                 "全部取消勾选则仅按当前实例实际版本判断。"), &dlg);
    info->setWordWrap(true);
    lay->addWidget(info);

    // 当前实例已保存的勾选（未配置时按检测到的游戏版本动态推导默认勾选）
    const QStringList saved = ConfigManager::instance().compatibleVersions(
        m_instanceId, CKanManager::instance().detectedVersion());
    const QSet<QString> savedSet(saved.begin(), saved.end());

    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    QWidget *host = new QWidget(scroll);
    QVBoxLayout *hostLay = new QVBoxLayout(host);
    QVector<QCheckBox*> boxes;
    // 全部版本：1.0 ~ 1.12
    for (int minor = 0; minor <= 12; ++minor) {
        const QString line = QStringLiteral("1.%1").arg(minor);
        QCheckBox *cb = new QCheckBox(tr("KSP %1").arg(line), host);
        cb->setChecked(savedSet.contains(line));
        boxes.append(cb);
        hostLay->addWidget(cb);
    }
    hostLay->addStretch();
    scroll->setWidget(host);
    lay->addWidget(scroll, 1);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) return;

    QStringList selected;
    for (int minor = 0; minor <= 12; ++minor)
        if (boxes.at(minor)->isChecked())
            selected << QStringLiteral("1.%1").arg(minor);

    ConfigManager::instance().setCompatibleVersions(m_instanceId, selected);
    applyCompatRange();
}

void ModsTabPage::onRefreshModsClicked()
{
    if (m_instance.path.isEmpty()) return;
    m_controller.openInstance(m_instance.path, m_instance.name);
    setDetailNote(tr("正在刷新仓库索引..."));
    showDownloadProgress();
    m_controller.requestRefreshIndex(true); // 手动刷新：强制重新下载
    // 顺带强制重新扫描一次 DLL，更新手动安装（AD）模组识别（如新放入/移除的模组 DLL）
    m_controller.requestScanDlls(true);
}

void ModsTabPage::onIndexRefreshed(CKanManager::IndexRefreshStatus status,
                                   const QString &error)
{
    hideDownloadProgress();
    // 取消：不当作失败处理，仅提示（用类型化状态取代 "已取消" 魔法串判断）
    if (status == CKanManager::IndexRefreshStatus::Cancelled) {
        setDetailNote(tr("已取消仓库索引加载。"));
        return;
    }
    if (status == CKanManager::IndexRefreshStatus::Failed) {
        m_modsModel->clear();
        setDetailNote(tr("仓库索引刷新失败：%1").arg(error));
        QMessageBox::warning(this, tr("刷新失败"), tr("无法获取仓库索引：\n%1").arg(error));
        return;
    }
    // 索引已就绪：若 DLL 扫描也完成则填充模型并显示数量；否则保留"加载中"提示，
    // 待后台扫描完成（unmanagedScanFinished）后再填充。
    maybePopulateMods();
    updateModActionButtons();
    // 索引就绪后，若存在 .ckan 导入待装清单，自动开始批量安装
    if (!m_pendingCkanIdentifiers.isEmpty()) {
        const QStringList pending = m_pendingCkanIdentifiers;
        m_pendingCkanIdentifiers.clear();
        m_controller.requestInstallBatch(pending);
    }
    // 部分仓库获取失败：提示用户（避免"静默缺失"）；页面不可见（如在设置页）时不弹窗
    if (!error.isEmpty() && isVisible())
        QMessageBox::warning(this, tr("仓库刷新"),
                             tr("部分仓库获取失败，已用其他仓库/旧缓存：\n%1").arg(error));
}

void ModsTabPage::onModSelectionChanged()
{
    if (!m_modsProxy || !m_modTable) return;
    const QModelIndex idx = m_modTable->currentIndex();
    if (!idx.isValid()) {
        m_currentModIdentifier.clear();
        m_metaText->clear();
        m_contentsTree->clear();
        m_relTree->clear();
        m_versionsTree->clear();
        m_versionsInstallBtn->setEnabled(false);
        updateModActionButtons();
        return;
    }
    const QModelIndex src = m_modsProxy->mapToSource(idx);
    const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
    m_currentModIdentifier = mod.identifier;
    showModDetails(mod);
    updateModActionButtons();
}

void ModsTabPage::onModDoubleClicked(const QModelIndex &index)
{
    if (!m_modsProxy || !index.isValid()) return;
    const QModelIndex src = m_modsProxy->mapToSource(index);
    const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
    if (!mod.isValid()) return;
    // 双击：切换到"关系"tab，聚焦依赖与冲突树
    m_currentModIdentifier = mod.identifier;
    showRelationshipsTab(mod, m_reverseRelCheck->isChecked());
    m_modDetailTabs->setCurrentIndex(2);
}

void ModsTabPage::updateModActionButtons()
{
    if (!m_installModBtn || !m_uninstallModBtn || !m_upgradeModBtn) return;
    CKanManager &mgr = CKanManager::instance();
    const QStringList checked = m_modsModel->checkedIdentifiers();
    const bool batch = checked.size() >= 2;

    // 按钮文案：批量时显示数量
    m_installModBtn->setText(batch ? tr(" 安装 (%1)").arg(checked.size()) : tr(" 安装"));
    m_upgradeModBtn->setText(batch ? tr(" 升级 (%1)").arg(checked.size()) : tr(" 升级"));
    m_uninstallModBtn->setText(batch ? tr(" 卸载 (%1)").arg(checked.size()) : tr(" 卸载"));

    m_installModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);

    if (batch) {
        bool anyInstall = false, anyUpgrade = false, anyUninstall = false;
        for (const QString &id : checked) {
            if (mgr.isAutoDetected(id)) continue;
            const bool installed = mgr.isInstalled(id);
            const bool upgradable = mgr.isUpgradable(id);
            if (!installed || upgradable) anyInstall = true;
            if (upgradable) anyUpgrade = true;
            if (installed) anyUninstall = true;
        }
        m_installModBtn->setEnabled(anyInstall);
        m_upgradeModBtn->setEnabled(anyUpgrade);
        m_uninstallModBtn->setEnabled(anyUninstall);
        return;
    }

    // 单个：勾选 1 个时用勾选的模组，否则用当前选中行
    const QString target = checked.size() == 1 ? checked.first() : m_currentModIdentifier;
    if (target.isEmpty()) return;
    if (mgr.isAutoDetected(target)) return;
    const bool installed = mgr.isInstalled(target);
    const bool upgradable = mgr.isUpgradable(target);
    m_installModBtn->setEnabled(!installed || upgradable);
    m_uninstallModBtn->setEnabled(installed && !mgr.indexReady() ? true : installed);
    m_upgradeModBtn->setEnabled(upgradable);
}

void ModsTabPage::showModDetails(const ckan::CkanModule &mod)
{
    if (!mod.isValid()) return;
    showMetaTab(mod);
    showContentsTab(mod);
    showRelationshipsTab(mod, m_reverseRelCheck->isChecked());
    showVersionsTab(mod);
}

// 状态提示写入元数据 tab（用于加载/刷新等过程性信息）
void ModsTabPage::setDetailNote(const QString &text)
{
    if (!m_metaText) return;
    if (text.isEmpty()) { m_metaText->clear(); return; }
    m_metaText->setHtml("<i>" + text.toHtmlEscaped() + "</i>");
}

void ModsTabPage::showMetaTab(const ckan::CkanModule &mod)
{
    QString s;
    s += "<b>" + mod.name.toHtmlEscaped() + "  " + mod.version.toHtmlEscaped() + "</b>";
    if (mod.identifier != mod.name)
        s += "<br/>" + tr("标识符：%1").arg(mod.identifier.toHtmlEscaped());
    if (!mod.abstract.isEmpty())
        s += "<br/>" + tr("描述：%1").arg(mod.abstract.toHtmlEscaped());
    if (!mod.author.isEmpty())
        s += "<br/>" + tr("作者：%1").arg(mod.author.join(QStringLiteral(", ")).toHtmlEscaped());
    if (!mod.license.isEmpty())
        s += "<br/>" + tr("许可：%1").arg(mod.license.join(QStringLiteral(", ")).toHtmlEscaped());
    // 部分模组只声明 ksp_version_min/max 区间，不填 ksp_version；回退显示区间端点（max 优先）。
    QString kspSrc = mod.kspVersion;
    if (kspSrc.isEmpty() && !mod.kspVersionMax.isEmpty()) kspSrc = mod.kspVersionMax;
    if (kspSrc.isEmpty() && !mod.kspVersionMin.isEmpty()) kspSrc = mod.kspVersionMin;
    if (!kspSrc.isEmpty())
        s += "<br/>" + tr("KSP 版本：%1").arg(ckan::ModuleVersion(kspSrc).toString());
    if (!mod.releaseDate.isEmpty())
        s += "<br/>" + tr("发布日期：%1").arg(mod.releaseDate.toHtmlEscaped());
    if (mod.downloadSize > 0)
        s += "<br/>" + tr("下载大小：%1").arg(formatBytes(mod.downloadSize));
    if (mod.installSize > 0)
        s += "<br/>" + tr("安装大小：%1").arg(formatBytes(mod.installSize));
    // （QTextEdit 的 HTML 会折叠裸换行，这里只用一个 <br/> 前缀即可）
    m_metaText->setHtml(s);
}

void ModsTabPage::showContentsTab(const ckan::CkanModule &mod)
{
    m_contentsTree->clear();
    if (!m_modDetailTabs) return;
    // 无 install 规则的元包/虚拟包 → 没有实际文件
    if (mod.isMetapackage() || (mod.install.isEmpty() && !mod.depends.isEmpty())) {
        m_contentsStatusLabel->setText(tr("元数据包，不包含文件。"));
        m_contentsDownloadBtn->setVisible(false);
        return;
    }
    m_contentsDownloadBtn->setVisible(true);
    const QString zipPath =
        ckan::ModuleInstaller::findCacheZip(CKanManager::instance().downloadDir(), mod);
    if (zipPath.isEmpty()) {
        // 压缩包未缓存：若该模组已安装（含手动安装 AD），直接浏览已安装目录；
        // 第一层仍为 GameData，第二层才是模组文件夹。
        const QStringList installedEntries =
            CKanManager::instance().installedGameDataEntries(mod.identifier);
        if (!installedEntries.isEmpty()) {
            m_contentsStatusLabel->setText(tr("压缩包尚未缓存，以下为已安装目录："));
            CKanManager &mgr = CKanManager::instance();
            const QString gameDir = mgr.gameDir();
            const QString prefix = QStringLiteral("GameData/");
            QTreeWidgetItem *gd = new QTreeWidgetItem(m_contentsTree, {QStringLiteral("GameData"), QString()});
            gd->setData(0, Qt::UserRole, QStringLiteral("dir"));
            // 根层无展开指示符（rootIsDecorated(false)），默认展开以露出第二层模组文件夹
            gd->setExpanded(true);
            for (const QString &entry : installedEntries) {
                if (!entry.startsWith(prefix, Qt::CaseInsensitive)) continue;
                const QString abs = QDir(gameDir).filePath(entry);
                const QFileInfo fi(abs);
                QTreeWidgetItem *item = new QTreeWidgetItem(
                    gd, {fi.fileName(), fi.isFile() ? formatBytes(fi.size()) : QString()});
                if (fi.isDir()) {
                    item->setData(0, Qt::UserRole, QStringLiteral("dir"));
                    addDirContentsToTree(item, abs);
                }
            }
            return;
        }
        m_contentsStatusLabel->setText(tr("压缩包尚未缓存。下载后才能查看文件清单。"));
        return;
    }
    m_contentsStatusLabel->setText(tr("压缩包已缓存，列出内部文件："));
    QStringList entries;
    QString err;
    if (ckan::ModuleInstaller::listZipEntries(zipPath, &entries, &err)) {
        // 结构化为目录树（以 '/' 分割）
        for (const QString &e : entries) {
            if (e.isEmpty()) continue;
            QStringList segs = e.split('/');
            QTreeWidgetItem *parent = m_contentsTree->invisibleRootItem();
            const QString name = segs.takeLast();
            for (const QString &dir : segs) {
                QTreeWidgetItem *child = nullptr;
                for (int i = 0; i < parent->childCount(); ++i) {
                    if (parent->child(i)->text(0) == dir
                        && parent->child(i)->data(0, Qt::UserRole).toString() == QStringLiteral("dir")) {
                        child = parent->child(i); break;
                    }
                }
                if (!child) {
                    child = new QTreeWidgetItem(parent, {dir});
                    child->setData(0, Qt::UserRole, QStringLiteral("dir"));
                }
                parent = child;
            }
            new QTreeWidgetItem(parent, {name, QString()});
        }
        m_contentsTree->sortItems(0, Qt::AscendingOrder);
    } else {
        m_contentsStatusLabel->setText(tr("无法读取压缩包：%1").arg(err));
    }
}

void ModsTabPage::showRelationshipsTab(const ckan::CkanModule &mod, bool reverse)
{
    m_relTree->clear();
    if (!m_relTree) return;
    if (reverse) {
        // 反向关系在后台全索引扫描，避免阻塞 UI
        if (m_reverseWatcher) return; // 已有扫描在途
        m_relTree->addTopLevelItem(new QTreeWidgetItem({tr("扫描中..."), QString()}));
        const QString target = mod.identifier;
        auto watcher = new QFutureWatcher<QStringList>(this);
        m_reverseWatcher = watcher;
        connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, mod]() {
            if (m_reverseWatcher != watcher) { watcher->deleteLater(); return; }
            const QStringList refs = watcher->result();
            watcher->deleteLater();
            m_reverseWatcher = nullptr;
            if (mod.identifier != m_currentModIdentifier) return; // 已切换模组
            if (!m_reverseRelCheck->isChecked()) return;          // 用户已切回前向
            m_relTree->clear();
            if (refs.isEmpty()) {
                m_relTree->addTopLevelItem(new QTreeWidgetItem({tr("没有模组依赖或引用此模组。"), QString()}));
                return;
            }
            CKanManager &mgr = CKanManager::instance();
            for (const QString &id : refs) {
                const ckan::CkanModule dep = mgr.latestOf(id);
                const QString label = dep.isValid() ? dep.name : id;
                QTreeWidgetItem *item = new QTreeWidgetItem({tr("引用/依赖"), label});
                item->setData(0, Qt::UserRole, id);
                addRelationChildren(item, id, 1); // 预载一层，供继续展开
                m_relTree->addTopLevelItem(item);
            }
        });
        watcher->setFuture(QtConcurrent::run([this, target]() {
            QStringList refs;
            const auto all = CKanManager::instance().search(QString());
            for (const auto &m : all) {
                bool hit = false;
                for (const auto &r : m.depends)    if (r.name == target) { hit = true; break; }
                if (!hit) for (const auto &r : m.recommends) if (r.name == target) { hit = true; break; }
                if (!hit) for (const auto &r : m.suggests)   if (r.name == target) { hit = true; break; }
                if (!hit) for (const auto &r : m.conflicts)  if (r.name == target) { hit = true; break; }
                if (hit) refs << m.identifier;
            }
            refs.removeDuplicates();
            return refs;
        }));
        return;
    }

    // 前向：depends / recommends / suggests / conflicts
    bool any = false;
    const auto addGroup = [&](const QVector<ckan::Relationship> &rels) {
        for (const ckan::Relationship &rel : rels) {
            any = true;
            const QString type = rel.type == ckan::Relationship::Type::Conflicts ? tr("冲突")
                              : tr("依赖");
            QTreeWidgetItem *item = new QTreeWidgetItem({type, rel.name});
            // 预载子项占位，展开时懒加载其自身关系
            item->setData(0, Qt::UserRole, rel.name);
            item->addChild(new QTreeWidgetItem({tr("..."), QString()}));
            m_relTree->addTopLevelItem(item);
        }
    };
    addGroup(mod.depends);
    addGroup(mod.recommends);
    addGroup(mod.suggests);
    addGroup(mod.conflicts);
    if (!any)
        m_relTree->addTopLevelItem(new QTreeWidgetItem({tr("此模组没有依赖、推荐、建议或冲突关系。"), QString()}));
}

// 为一个关系条目懒加载其下一层关系
void ModsTabPage::addRelationChildren(QTreeWidgetItem *parent, const QString &identifier, int depth)
{
    if (!parent) return;
    if (depth > 5) return; // 深度上限，防极端引用环
    // 清掉占位子项（若已加载则跳过）
    if (parent->childCount() == 1 && parent->child(0)->text(0) == QStringLiteral("...")) {
        delete parent->takeChild(0);
    } else if (parent->childCount() > 0) {
        return; // 已加载
    }
    const ckan::CkanModule mod = CKanManager::instance().latestOf(identifier);
    if (!mod.isValid()) { parent->addChild(new QTreeWidgetItem({tr("（仓库无此模组/虚拟包：%1）").arg(identifier), QString()})); return; }
    if (mod.depends.isEmpty() && mod.conflicts.isEmpty()) {
        parent->addChild(new QTreeWidgetItem({tr("（无依赖或冲突）"), QString()}));
        return;
    }
    for (const auto &r : mod.depends) {
        QTreeWidgetItem *c = new QTreeWidgetItem({tr("依赖"), r.name});
        c->setData(0, Qt::UserRole, r.name);
        c->addChild(new QTreeWidgetItem({QStringLiteral("..."), QString()}));
        parent->addChild(c);
    }
    for (const auto &r : mod.conflicts) {
        QTreeWidgetItem *c = new QTreeWidgetItem({tr("冲突"), r.name});
        c->setData(0, Qt::UserRole, r.name);
        c->addChild(new QTreeWidgetItem({QStringLiteral("..."), QString()}));
        parent->addChild(c);
    }
}

void ModsTabPage::showVersionsTab(const ckan::CkanModule &mod)
{
    m_versionsTree->clear();
    m_versionsInstallBtn->setEnabled(false);
    if (!m_versionsTree) return;
    const QVector<ckan::CkanModule> versions =
        CKanManager::instance().versionsOf(mod.identifier);
    if (versions.isEmpty()) {
        m_versionsTree->addTopLevelItem(new QTreeWidgetItem({tr("仓库无其它版本记录。"), QString(), QString(), QString()}));
        return;
    }
    // 降序排列（当前最新在前）
    QVector<ckan::CkanModule> vs = versions;
    std::sort(vs.begin(), vs.end(), [](const ckan::CkanModule &a, const ckan::CkanModule &b) {
        return ckan::ModuleVersion(a.version) > ckan::ModuleVersion(b.version);
    });
    CKanManager &mgr = CKanManager::instance();
    QString installed = mgr.installedVersion(mod.identifier);
    bool ad = false;
    // 手动安装（AD）模组注册表无版本号：从 DLL 文件名/内部版本推导，失败则回退标记最新版
    if (installed.isEmpty() && mgr.isAutoDetected(mod.identifier)) {
        ad = true;
        installed = mgr.autoDetectedVersion(mod.identifier);
    }
    const ckan::ModuleVersion installedVer(installed);
    bool marked = false; // AD 推导出的版本是否已命中某条目
    for (const ckan::CkanModule &v : vs) {
        QString status;
        if (!installed.isEmpty()) {
            // 先精确字符串匹配（沿用既有行为），再退化为数值比较（兼容 AD 推导的 .0 后缀等）
            const bool match = v.version == installed
                || (installedVer.isValid() && installedVer == ckan::ModuleVersion(v.version));
            if (match) {
                status = ad ? tr("已安装(AD)") : tr("已安装");
                if (ad) marked = true;
            }
        } else {
            status = tr("未安装");
        }
        QTreeWidgetItem *item = new QTreeWidgetItem({
            v.version, v.releaseDate, formatBytes(v.downloadSize), status});
        item->setData(0, Qt::UserRole, v.version);
        item->setData(1, Qt::UserRole, v.toJson()); // 存该版本完整元数据供安装
        m_versionsTree->addTopLevelItem(item);
    }
    // AD 版本未能命中任何条目：把最新版标记为“已安装(AD)”
    if (ad && !marked && !vs.isEmpty()) {
        QTreeWidgetItem *top = m_versionsTree->topLevelItem(0);
        if (top) top->setText(3, tr("已安装(AD)"));
    }
}

void ModsTabPage::onContentsDownloadClicked()
{
    if (m_currentModIdentifier.isEmpty()) return;
    const ckan::CkanModule mod =
        CKanManager::instance().latestOf(m_currentModIdentifier);
    if (!mod.isValid()) return;
    m_contentsDownloadBtn->setEnabled(false);
    m_contentsStatusLabel->setText(tr("正在下载压缩包..."));
    m_controller.requestDownloadSingle(mod);
}

void ModsTabPage::onSingleDownloadFinished(bool ok, const QString &identifier,
                                           const QString &error)
{
    if (identifier != m_currentModIdentifier) return;
    m_contentsDownloadBtn->setEnabled(true);
    if (!ok) {
        m_contentsStatusLabel->setText(tr("下载失败：%1").arg(error));
        return;
    }
    const ckan::CkanModule mod = CKanManager::instance().latestOf(identifier);
    if (mod.isValid()) showContentsTab(mod);
}

void ModsTabPage::onReverseRelToggled(bool on)
{
    const ckan::CkanModule mod =
        CKanManager::instance().latestOf(m_currentModIdentifier);
    if (mod.isValid()) showRelationshipsTab(mod, on);
}

void ModsTabPage::onRelationItemExpanded(QTreeWidgetItem *item)
{
    if (!item) return;
    const QString id = item->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) return;
    addRelationChildren(item, id, 1);
}

void ModsTabPage::onVersionSelectionChanged()
{
    m_versionsInstallBtn->setEnabled(m_versionsTree->currentItem() != nullptr);
}

void ModsTabPage::onVersionInstallClicked()
{
    QTreeWidgetItem *item = m_versionsTree->currentItem();
    if (!item) return;
    const QString version = item->data(0, Qt::UserRole).toString();
    const ckan::CkanModule mod = ckan::CkanModule::fromJsonObject(
        QJsonDocument::fromJson(item->data(1, Qt::UserRole).toByteArray()).object());
    if (!mod.isValid()) return;

    const QString installed = CKanManager::instance().installedVersion(mod.identifier);
    const bool isDowngrade = !installed.isEmpty()
        && ckan::ModuleVersion(installed) > ckan::ModuleVersion(version);
    if (isDowngrade) {
        const QMessageBox::StandardButton go = QMessageBox::question(
            this, tr("切换版本"),
            tr("当前已安装 %1，即将降级到 %2。\n是否继续？")
                .arg(installed, version),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (go != QMessageBox::Yes) return;
    }
    m_versionsInstallBtn->setEnabled(false);
    showDownloadProgress();
    m_controller.requestInstallVersion(mod);
}

void ModsTabPage::onInstallModClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.size() >= 2) {
        setModButtonsEnabled(false);
        showDownloadProgress();
        m_controller.requestInstallBatch(ids);
        return;
    }
    const QString target = ids.size() == 1 ? ids.first() : m_currentModIdentifier;
    if (target.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择或勾选一个模组。"));
        return;
    }
    m_refreshModsBtn->setEnabled(false);
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    showDownloadProgress();
    m_controller.requestInstall(target);
}

void ModsTabPage::onUninstallModClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.size() >= 2) {
        if (QMessageBox::question(this, tr("确认批量卸载"),
                tr("确定要卸载已勾选的 %1 个模组吗？").arg(ids.size())
                    + uninstallCascadeHint(ids),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        setModButtonsEnabled(false);
        showUninstallProgress(tr("正在卸载 %1 个模组...").arg(ids.size()));
        m_controller.requestUninstallBatch(ids);
        return;
    }
    const QString target = ids.size() == 1 ? ids.first() : m_currentModIdentifier;
    if (target.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择或勾选一个模组。"));
        return;
    }
    QString name = target;
    for (int r = 0; r < m_modsProxy->rowCount(); ++r) {
        const QModelIndex src = m_modsProxy->mapToSource(m_modsProxy->index(r, ModsTableModel::ColCheck));
        const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
        if (mod.isValid() && mod.identifier == target) { name = mod.name; break; }
    }
    if (QMessageBox::question(this, tr("确认卸载"),
            tr("确定要卸载模组 %1 吗？").arg(name.isEmpty() ? target : name)
                + uninstallCascadeHint({target}),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    m_refreshModsBtn->setEnabled(false);
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    showUninstallProgress(tr("正在卸载：%1").arg(name.isEmpty() ? target : name));
    m_controller.requestUninstall(target);
}

void ModsTabPage::onUpgradeModClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.size() >= 2) {
        setModButtonsEnabled(false);
        showDownloadProgress();
        m_controller.requestUpgradeBatch(ids);
        return;
    }
    const QString target = ids.size() == 1 ? ids.first() : m_currentModIdentifier;
    if (target.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择或勾选一个模组。"));
        return;
    }
    setModButtonsEnabled(false);
    showDownloadProgress();
    m_controller.requestUpgrade(target);
}

void ModsTabPage::setModButtonsEnabled(bool enabled)
{
    m_refreshModsBtn->setEnabled(enabled);
    m_installModBtn->setEnabled(enabled);
    m_uninstallModBtn->setEnabled(enabled);
    m_upgradeModBtn->setEnabled(enabled);
    if (!enabled) return;
    updateModActionButtons();
    updateSelectAllButtonText();
}

void ModsTabPage::updateSelectAllButtonText()
{
    if (!m_selectAllBtn) return;
    bool any = false, all = true;
    for (int r = 0; r < m_modsProxy->rowCount(); ++r) {
        const QModelIndex src = m_modsProxy->mapToSource(m_modsProxy->index(r, ModsTableModel::ColCheck));
        const ckan::CkanModule m = m_modsModel->moduleAt(src.row());
        if (!m.isValid()) continue;
        any = true;
        if (!m_modsModel->isChecked(m.identifier)) { all = false; break; }
    }
    m_selectAllBtn->setText(any && all ? tr(" 清空") : tr(" 全选"));
}

void ModsTabPage::onSelectAllClicked()
{
    QVector<ckan::CkanModule> visible;
    for (int r = 0; r < m_modsProxy->rowCount(); ++r) {
        const QModelIndex src = m_modsProxy->mapToSource(m_modsProxy->index(r, ModsTableModel::ColCheck));
        const ckan::CkanModule m = m_modsModel->moduleAt(src.row());
        if (m.isValid()) visible.append(m);
    }
    if (visible.isEmpty()) return;
    bool allChecked = true;
    for (const ckan::CkanModule &m : visible)
        if (!m_modsModel->isChecked(m.identifier)) { allChecked = false; break; }
    m_modsModel->setAllChecked(visible, !allChecked);
    updateSelectAllButtonText();
    updateModActionButtons();
}

void ModsTabPage::onModOperationFinished(bool ok, const QString &message)
{
    setModButtonsEnabled(true);
    hideDownloadProgress();
    if (ok) {
        m_modsModel->refreshStatus();
        m_modsModel->clearAllChecks();
        updateSelectAllButtonText();
        updateModActionButtons();
        // 重新显示当前选中模组详情，覆盖安装期间残留的“正在处理 ... %”进度文案
        onModSelectionChanged();
        QMessageBox::information(this, tr("完成"), message);
    } else {
        updateModActionButtons();
        QMessageBox::warning(this, tr("操作失败"), message);
    }
}

void ModsTabPage::showDownloadProgress()
{
    m_modProgressBar->setRange(0, 1000);
    m_modProgressBar->setValue(0);
    m_modProgressLabel->setText(tr("准备下载..."));
    m_cancelDownloadBtn->setEnabled(true);
    m_modProgressWidget->setVisible(true);
}

void ModsTabPage::showUninstallProgress(const QString &label)
{
    m_modProgressBar->setRange(0, 0); // 卸载是本地删文件，无字节可计，用不确定进度条
    m_modProgressLabel->setText(label);
    m_cancelDownloadBtn->setEnabled(true);
    m_modProgressWidget->setVisible(true);
}

QString ModsTabPage::uninstallCascadeHint(const QStringList &identifiers)
{
    const QStringList plan = m_controller.uninstallPlan(identifiers);
    const int dependents = plan.isEmpty() ? 0 : qMax(0, plan.size() - identifiers.size());
    if (dependents <= 0) return QString();
    return tr("\n\n将连同 %1 个依赖模组一并卸载；取消时这些依赖与目标一起恢复。").arg(dependents);
}

void ModsTabPage::hideDownloadProgress()
{
    m_modProgressWidget->setVisible(false);
}

void ModsTabPage::onDownloadProgress(const QString &identifier, qint64 doneBytes,
                                     qint64 totalBytes, qint64 speedBps)
{
    if (!m_modProgressWidget->isVisible())
        m_modProgressWidget->setVisible(true);
    if (totalBytes > 0) {
        // 已知总量：按字节计算百分比
        if (m_modProgressBar->maximum() == 0)
            m_modProgressBar->setRange(0, 1000);
        const int v = static_cast<int>(doneBytes * 1000 / totalBytes);
        m_modProgressBar->setValue(qBound(0, v, 1000));
    } else if (m_modProgressBar->maximum() != 0) {
        // 总量未知（如索引 tar.gz 分块下载）：不确定进度条
        m_modProgressBar->setRange(0, 0);
    }
    QString text = tr("正在下载：%1  %2 / %3")
        .arg(identifier, formatBytes(doneBytes), formatBytes(totalBytes));
    if (speedBps > 0)
        text += QStringLiteral("  (%1/s)").arg(formatBytes(speedBps));
    m_modProgressLabel->setText(text);
}

void ModsTabPage::onCancelDownloadClicked()
{
    m_controller.requestCancel();
    m_modProgressLabel->setText(tr("正在取消..."));
    m_cancelDownloadBtn->setEnabled(false);
}

void ModsTabPage::onImportModClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("导入模组"), QString(),
        tr("模组文件 (*.zip *.ckan)"));
    if (path.isEmpty()) return; // 用户取消

    setModButtonsEnabled(false);
    showDownloadProgress();
    m_controller.requestImport(path);
}

void ModsTabPage::onShowHistoryClicked()
{
    const QString dir = CKanManager::instance().historyDir();
    if (dir.isEmpty()) {
        QMessageBox::information(this, tr("安装历史"), tr("尚未绑定游戏实例。"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("安装历史"));
    dlg.resize(680, 460);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QSplitter *splitter = new QSplitter(&dlg);
    QListWidget *list = new QListWidget(splitter);
    list->setMinimumWidth(240);
    QTextEdit *text = new QTextEdit(splitter);
    text->setReadOnly(true);
    text->setPlaceholderText(tr("选择左侧快照查看其安装内容"));
    splitter->addWidget(list);
    splitter->addWidget(text);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    lay->addWidget(splitter, 1);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    btnBox->button(QDialogButtonBox::Close)->setText(tr("关闭"));
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    // 快照文件名即为时间戳（零填充、可字典序排序），倒序取最新在前
    QDir d(dir);
    const QStringList files = d.entryList({QStringLiteral("*.ckan")},
                                          QDir::Files, QDir::Name | QDir::Reversed);
    if (files.isEmpty()) {
        text->setPlainText(tr("暂无安装历史。完成一次安装/卸载/升级后会自动生成快照。"));
    }
    for (const QString &f : files)
        list->addItem(f);

    connect(list, &QListWidget::currentRowChanged, &dlg, [&](int row) {
        if (row < 0 || row >= files.size()) { text->clear(); return; }
        QFile f(d.filePath(files.at(row)));
        if (f.open(QIODevice::ReadOnly))
            text->setPlainText(QString::fromUtf8(f.readAll()));
        else
            text->setPlainText(tr("无法读取：%1").arg(files.at(row)));
    });
    if (!files.isEmpty())
        list->setCurrentRow(0);

    dlg.exec();
}