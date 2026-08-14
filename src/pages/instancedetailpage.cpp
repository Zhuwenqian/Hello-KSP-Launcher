#include "instancedetailpage.h"
#include "../widgets/toggleswitch.h"
#include "../ckanmanager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QMap>
#include <QIcon>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QLineEdit>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include "../iconutils.h"

namespace {
// 将关系列表转为模块名列表（用于依赖/冲突展示）
QStringList relNames(const QVector<ckan::Relationship> &rels)
{
    QStringList out;
    for (const ckan::Relationship &r : rels)
        out << r.name;
    return out;
}
} // namespace

// Custom delegate to control editing: only column 1 (value) editable
class SettingsItemDelegate : public QStyledItemDelegate {
public:
    explicit SettingsItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.column() != 1) return nullptr; // Only value column editable

        // Bool values use permanent ToggleSwitch widget, no editor needed
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
};

InstanceDetailPage::InstanceDetailPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void InstanceDetailPage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget* topBar = new QWidget(this);
    QHBoxLayout* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(10, 5, 15, 5);

    m_backButton = new QPushButton(IconUtils::tintedIcon(":/icons/back.svg", "#ffffff"), tr(" 返回"), topBar);
    m_backButton->setObjectName("backButton");
    m_backButton->setFixedHeight(40);
    m_backButton->setMinimumWidth(100);
    connect(m_backButton, &QPushButton::clicked, this, &InstanceDetailPage::onBackClicked);

    m_titleLabel = new QLabel(tr("实例管理"), topBar);
    m_titleLabel->setObjectName("pageTitle");

    topBarLayout->addWidget(m_backButton);
    topBarLayout->addWidget(m_titleLabel, 1);
    mainLayout->addWidget(topBar);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_detailSidebar = new QWidget(this);
    m_detailSidebar->setFixedWidth(200);
    m_detailSidebar->setObjectName("sidebarWidget");
    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_detailSidebar);
    sidebarLayout->setContentsMargins(0, 10, 0, 10);
    sidebarLayout->setSpacing(0);

    m_gameSettingsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/sliders.svg", "#ffffff"), tr("  游戏设置"), m_detailSidebar);
    m_gameSettingsBtn->setObjectName("detailNavButton");
    m_gameSettingsBtn->setCheckable(true);
    m_gameSettingsBtn->setChecked(true);
    m_gameSettingsBtn->setMinimumHeight(40);
    connect(m_gameSettingsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_dlcBtn = new QPushButton(IconUtils::tintedIcon(":/icons/package.svg", "#ffffff"), tr("  DLC"), m_detailSidebar);
    m_dlcBtn->setObjectName("detailNavButton");
    m_dlcBtn->setCheckable(true);
    m_dlcBtn->setMinimumHeight(40);
    connect(m_dlcBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_modsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/puzzle.svg", "#ffffff"), tr("  模组管理"), m_detailSidebar);
    m_modsBtn->setObjectName("detailNavButton");
    m_modsBtn->setCheckable(true);
    m_modsBtn->setMinimumHeight(40);
    connect(m_modsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_savesBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr("  存档管理"), m_detailSidebar);
    m_savesBtn->setObjectName("detailNavButton");
    m_savesBtn->setCheckable(true);
    m_savesBtn->setMinimumHeight(40);
    connect(m_savesBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_advancedBtn = new QPushButton(IconUtils::tintedIcon(":/icons/settings.svg", "#ffffff"), tr("  高级"), m_detailSidebar);
    m_advancedBtn->setObjectName("detailNavButton");
    m_advancedBtn->setCheckable(true);
    m_advancedBtn->setMinimumHeight(40);
    connect(m_advancedBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_exportModpackBtn = new QPushButton(IconUtils::tintedIcon(":/icons/database.svg", "#ffffff"), tr("  导出整合包"), m_detailSidebar);
    m_exportModpackBtn->setObjectName("detailNavButton");
    m_exportModpackBtn->setCheckable(true);
    m_exportModpackBtn->setMinimumHeight(40);
    connect(m_exportModpackBtn, &QPushButton::clicked, this, &InstanceDetailPage::onExportModpackClicked);

    m_browseBtn = new QPushButton(IconUtils::tintedIcon(":/icons/folder-open.svg", "#ffffff"), tr("  浏览"), m_detailSidebar);
    m_browseBtn->setObjectName("detailNavButton");
    m_browseBtn->setCheckable(true);
    m_browseBtn->setMinimumHeight(40);
    connect(m_browseBtn, &QPushButton::clicked, this, &InstanceDetailPage::onBrowseClicked);

    sidebarLayout->addWidget(m_gameSettingsBtn);
    sidebarLayout->addWidget(m_dlcBtn);
    sidebarLayout->addWidget(m_modsBtn);
    sidebarLayout->addWidget(m_savesBtn);
    sidebarLayout->addWidget(m_advancedBtn);
    sidebarLayout->addWidget(m_exportModpackBtn);
    sidebarLayout->addWidget(m_browseBtn);
    sidebarLayout->addStretch();

    m_contentStack = new QStackedWidget(this);
    setupGameSettingsTab();
    setupDLCTab();
    setupModsTab();
    setupAdvancedTab();
    setupBrowseMenu();

    contentLayout->addWidget(m_detailSidebar);
    contentLayout->addWidget(m_contentStack, 1);
    mainLayout->addLayout(contentLayout, 1);
}

void InstanceDetailPage::setupGameSettingsTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(10);

    m_settingsTree = new QTreeWidget(tab);
    m_settingsTree->setColumnCount(2);
    m_settingsTree->setHeaderLabels({tr("设置项"), tr("值")});
    m_settingsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_settingsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_settingsTree->setAlternatingRowColors(true);
    m_settingsTree->setRootIsDecorated(true);
    m_settingsTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    m_settingsTree->setItemDelegate(new SettingsItemDelegate(m_settingsTree));

    QWidget* btnBar = new QWidget(tab);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton* saveBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr(" 保存设置"), btnBar);
    saveBtn->setObjectName("primaryButton");
    saveBtn->setMinimumHeight(36);
    saveBtn->setMinimumWidth(140);
    connect(saveBtn, &QPushButton::clicked, this, &InstanceDetailPage::onSaveSettingsClicked);
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);

    layout->addWidget(m_settingsTree, 1);
    layout->addWidget(btnBar);
    m_contentStack->addWidget(tab);
}

void InstanceDetailPage::setupDLCTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);

    m_dlcList = new QListWidget(tab);
    layout->addWidget(m_dlcList);
    m_contentStack->addWidget(tab);
}

void InstanceDetailPage::setupModsTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(8);

    // 顶栏：搜索 + 筛选 + 刷新仓库
    QWidget* topBar = new QWidget(tab);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);

    m_modSearchEdit = new QLineEdit(topBar);
    m_modSearchEdit->setPlaceholderText(tr("搜索模组名称或标识符..."));
    m_modSearchEdit->setClearButtonEnabled(true);
    m_modSearchEdit->setMinimumHeight(34);
    connect(m_modSearchEdit, &QLineEdit::textChanged, this, &InstanceDetailPage::onModSearchChanged);

    m_modFilterCombo = new QComboBox(topBar);
    m_modFilterCombo->addItem(tr("全部"), -1);
    m_modFilterCombo->addItem(tr("已安装"), static_cast<int>(ModsTableModel::Installed));
    m_modFilterCombo->addItem(tr("可升级"), static_cast<int>(ModsTableModel::Upgradable));
    m_modFilterCombo->addItem(tr("未安装"), static_cast<int>(ModsTableModel::NotInstalled));
    m_modFilterCombo->setMinimumHeight(34);
    connect(m_modFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InstanceDetailPage::onModFilterChanged);

    m_refreshModsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/refresh.svg", "#ffffff"),
                                       tr(" 刷新仓库"), topBar);
    m_refreshModsBtn->setObjectName("primaryButton");
    m_refreshModsBtn->setMinimumHeight(34);
    connect(m_refreshModsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onRefreshModsClicked);

    m_selectAllBtn = new QPushButton(IconUtils::tintedIcon(":/icons/check.svg", "#ffffff"),
                                     tr(" 全选"), topBar);
    m_selectAllBtn->setMinimumHeight(34);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &InstanceDetailPage::onSelectAllClicked);

    topLayout->addWidget(m_modSearchEdit, 1);
    topLayout->addWidget(m_modFilterCombo);
    topLayout->addWidget(m_selectAllBtn);
    topLayout->addWidget(m_refreshModsBtn);
    layout->addWidget(topBar);

    // 表格
    m_modsModel = new ModsTableModel(this);
    m_modsProxy = new ModsFilterProxyModel(this);
    m_modsProxy->setSourceModel(m_modsModel);

    m_modTable = new QTableView(tab);
    m_modTable->setModel(m_modsProxy);
    m_modTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modTable->setAlternatingRowColors(true);
    m_modTable->verticalHeader()->setVisible(false);
    m_modTable->horizontalHeader()->setStretchLastSection(true);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColCheck, QHeaderView::Fixed);
    m_modTable->setColumnWidth(ModsTableModel::ColCheck, 40);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColName, QHeaderView::Stretch);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColIdentifier, QHeaderView::ResizeToContents);
    m_modTable->horizontalHeader()->setSectionResizeMode(ModsTableModel::ColStatus, QHeaderView::ResizeToContents);
    m_modTable->setSortingEnabled(true);
    m_modTable->sortByColumn(ModsTableModel::ColName, Qt::AscendingOrder);
    connect(m_modTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &InstanceDetailPage::onModSelectionChanged);
    connect(m_modTable, &QTableView::doubleClicked, this, &InstanceDetailPage::onModDoubleClicked);
    connect(m_modsModel, &ModsTableModel::dataChanged, this, [this]() {
        updateSelectAllButtonText();
    });
    layout->addWidget(m_modTable, 1);

    // 下载进度条（任务进行中显示）
    m_modProgressWidget = new QWidget(tab);
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
    connect(m_cancelDownloadBtn, &QPushButton::clicked,
            this, &InstanceDetailPage::onCancelDownloadClicked);
    progLayout->addWidget(m_modProgressBar, 1);
    progLayout->addWidget(m_modProgressLabel);
    progLayout->addWidget(m_cancelDownloadBtn);
    m_modProgressWidget->setVisible(false);
    layout->addWidget(m_modProgressWidget);

    // 底部：详情 + 操作按钮
    m_modDetailText = new QTextEdit(tab);
    m_modDetailText->setReadOnly(true);
    m_modDetailText->setMaximumHeight(150);
    m_modDetailText->setPlaceholderText(tr("选中一个模组查看详情，双击查看依赖信息"));
    layout->addWidget(m_modDetailText);

    QWidget* btnBar = new QWidget(tab);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    m_installModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/add.svg", "#ffffff"),
                                      tr(" 安装"), btnBar);
    m_installModBtn->setObjectName("primaryButton");
    m_installModBtn->setMinimumHeight(36);
    connect(m_installModBtn, &QPushButton::clicked, this, &InstanceDetailPage::onInstallModClicked);

    m_uninstallModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#ffffff"),
                                        tr(" 卸载"), btnBar);
    m_uninstallModBtn->setObjectName("dangerButton");
    m_uninstallModBtn->setMinimumHeight(36);
    connect(m_uninstallModBtn, &QPushButton::clicked, this, &InstanceDetailPage::onUninstallModClicked);

    m_upgradeModBtn = new QPushButton(IconUtils::tintedIcon(":/icons/check.svg", "#ffffff"),
                                      tr(" 升级"), btnBar);
    m_upgradeModBtn->setObjectName("primaryButton");
    m_upgradeModBtn->setMinimumHeight(36);
    connect(m_upgradeModBtn, &QPushButton::clicked, this, &InstanceDetailPage::onUpgradeModClicked);

    // 批量操作
    m_batchInstallBtn = new QPushButton(IconUtils::tintedIcon(":/icons/add.svg", "#ffffff"),
                                        tr(" 批量安装"), btnBar);
    m_batchInstallBtn->setObjectName("primaryButton");
    m_batchInstallBtn->setMinimumHeight(36);
    connect(m_batchInstallBtn, &QPushButton::clicked, this, &InstanceDetailPage::onBatchInstallClicked);

    m_batchUpgradeBtn = new QPushButton(IconUtils::tintedIcon(":/icons/check.svg", "#ffffff"),
                                        tr(" 批量升级"), btnBar);
    m_batchUpgradeBtn->setObjectName("primaryButton");
    m_batchUpgradeBtn->setMinimumHeight(36);
    connect(m_batchUpgradeBtn, &QPushButton::clicked, this, &InstanceDetailPage::onBatchUpgradeClicked);

    m_batchUninstallBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#ffffff"),
                                          tr(" 批量卸载"), btnBar);
    m_batchUninstallBtn->setObjectName("dangerButton");
    m_batchUninstallBtn->setMinimumHeight(36);
    connect(m_batchUninstallBtn, &QPushButton::clicked, this, &InstanceDetailPage::onBatchUninstallClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_batchInstallBtn);
    btnLayout->addWidget(m_batchUpgradeBtn);
    btnLayout->addWidget(m_batchUninstallBtn);
    btnLayout->addWidget(m_installModBtn);
    btnLayout->addWidget(m_upgradeModBtn);
    btnLayout->addWidget(m_uninstallModBtn);
    layout->addWidget(btnBar);

    // 适配层信号
    connect(&CKanManager::instance(), &CKanManager::indexRefreshed,
            this, &InstanceDetailPage::onIndexRefreshed);
    connect(&CKanManager::instance(), &CKanManager::operationFinished,
            this, &InstanceDetailPage::onModOperationFinished);
    connect(&CKanManager::instance(), &CKanManager::installProgress,
            this, [this](const QString &id, int percent) {
        if (id == m_currentModIdentifier)
            m_modDetailText->setPlainText(tr("正在处理 %1 ... %2%").arg(id).arg(percent));
    });
    connect(&CKanManager::instance(), &CKanManager::downloadProgress,
            this, &InstanceDetailPage::onDownloadProgress);

    updateModActionButtons();
    m_contentStack->addWidget(tab);
}

void InstanceDetailPage::setInstanceId(const QString &id)
{
    m_instanceId = id;
    m_instance = ConfigManager::instance().getInstance(id);
    m_titleLabel->setText("实例管理 - " + m_instance.name);
    refreshData();
}

void InstanceDetailPage::loadCurrentInstance()
{
    m_instance = ConfigManager::instance().currentInstance();
    m_instanceId = m_instance.id;
    m_titleLabel->setText("实例管理 - " + m_instance.name);
    refreshData();
}

void InstanceDetailPage::onBackClicked()
{
    emit backClicked();
}

void InstanceDetailPage::onNavButtonClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    m_gameSettingsBtn->setChecked(btn == m_gameSettingsBtn);
    m_dlcBtn->setChecked(btn == m_dlcBtn);
    m_modsBtn->setChecked(btn == m_modsBtn);
    m_savesBtn->setChecked(btn == m_savesBtn);
    m_advancedBtn->setChecked(btn == m_advancedBtn);
    m_browseBtn->setChecked(false);

    if (btn == m_gameSettingsBtn) {
        m_contentStack->setCurrentIndex(0);
    } else if (btn == m_dlcBtn) {
        m_contentStack->setCurrentIndex(1);
    } else if (btn == m_modsBtn) {
        m_contentStack->setCurrentIndex(2);
    } else if (btn == m_savesBtn) {
        emit savesManageRequested();
        // 回到游戏设置tab，避免下次进来还是选中存档管理
        m_gameSettingsBtn->setChecked(true);
        m_savesBtn->setChecked(false);
        m_contentStack->setCurrentIndex(0);
    } else if (btn == m_advancedBtn) {
        loadLaunchArgs();
        m_contentStack->setCurrentIndex(3);
    }
}

void InstanceDetailPage::refreshData()
{
    if (m_instance.path.isEmpty()) return;
    loadGameSettings();
    loadDLCs();
    loadMods();
    loadLaunchArgs();
}

void InstanceDetailPage::loadGameSettings()
{
    m_settingsTree->clear();
    m_currentSettings = InstanceManager::instance().loadGameSettings(m_instance.path);

    // Group settings by category
    QMap<QString, QList<int>> categoryMap;
    for (int i = 0; i < m_currentSettings.size(); ++i) {
        categoryMap[m_currentSettings[i].category].append(i);
    }

    // Create category parent nodes
    for (auto it = categoryMap.begin(); it != categoryMap.end(); ++it) {
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem(m_settingsTree);
        categoryItem->setText(0, it.key());
        categoryItem->setText(1, QString("(%1项)").arg(it.value().size()));
        QFont catFont = categoryItem->font(0);
        catFont.setBold(true);
        categoryItem->setFont(0, catFont);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsEditable);
        categoryItem->setExpanded(true);

        for (int idx : it.value()) {
            const GameSetting& s = m_currentSettings[idx];
            QTreeWidgetItem* item = new QTreeWidgetItem(categoryItem);
            item->setText(0, s.displayName);
            item->setData(0, Qt::UserRole, s.key);
            // ItemIsEditable flag needed, delegate controls which column actually edits
            item->setFlags(item->flags() | Qt::ItemIsEditable);

            QString valLower = s.value.toLower();
            if (valLower == "true" || valLower == "false") {
                // Bool values: permanently show ToggleSwitch instead of text
                item->setText(1, s.value);
                ToggleSwitch* toggle = new ToggleSwitch(m_settingsTree);
                toggle->setChecked(valLower == "true");
                connect(toggle, &ToggleSwitch::toggled, this, [item](bool checked) {
                    item->setText(1, checked ? "True" : "False");
                });
                m_settingsTree->setItemWidget(item, 1, toggle);
            } else {
                item->setText(1, s.value);
            }
        }
    }
}

bool InstanceDetailPage::saveGameSettings()
{
    QList<GameSetting> updatedSettings = m_currentSettings;
    for (int i = 0; i < m_settingsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* categoryItem = m_settingsTree->topLevelItem(i);
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            QTreeWidgetItem* item = categoryItem->child(j);
            QString originalKey = item->data(0, Qt::UserRole).toString();
            if (originalKey.isEmpty()) continue;
            QString newValue = item->text(1);
            for (GameSetting& s : updatedSettings) {
                if (s.key == originalKey) {
                    s.value = newValue;
                    break;
                }
            }
        }
    }
    bool ok = InstanceManager::instance().saveGameSettings(m_instance.path, updatedSettings);
    if (ok) {
        m_currentSettings = updatedSettings;
        QMessageBox::information(this, tr("保存成功"), tr("游戏设置已保存！"));
    } else {
        QMessageBox::warning(this, tr("保存失败"), tr("无法保存游戏设置，请检查文件权限。"));
    }
    return ok;
}

void InstanceDetailPage::onSaveSettingsClicked()
{
    saveGameSettings();
}

void InstanceDetailPage::loadDLCs()
{
    m_dlcList->clear();
    QList<DLCDetection> dlcs = InstanceManager::instance().detectDLCs(m_instance.path);

    for (const DLCDetection& dlc : dlcs) {
        QWidget* itemWidget = new QWidget(m_dlcList);
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(15, 12, 15, 12);

        QLabel* nameLabel = new QLabel(dlc.displayName, itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");

        QLabel* statusLabel = new QLabel(itemWidget);
        if (dlc.installed) {
            statusLabel->setText("● Installed");
            statusLabel->setObjectName("dlcInstalled");
        } else {
            statusLabel->setText("○ Not Installed");
            statusLabel->setObjectName("dlcNotInstalled");
        }
        statusLabel->setMinimumWidth(100);

        layout->addWidget(nameLabel);
        layout->addStretch();
        layout->addWidget(statusLabel);

        QListWidgetItem* item = new QListWidgetItem(m_dlcList);
        item->setSizeHint(QSize(0, 50));
        m_dlcList->addItem(item);
        m_dlcList->setItemWidget(item, itemWidget);
    }
}

void InstanceDetailPage::loadMods()
{
    if (m_instance.path.isEmpty()) return;

    CKanManager &mgr = CKanManager::instance();
    mgr.openInstance(m_instance.path, m_instance.name);

    if (!mgr.indexReady()) {
        // 首次进入：自动加载仓库索引（优先使用本地缓存）
        m_modsModel->clear();
        m_modDetailText->setPlainText(tr("正在加载 CKAN 仓库索引，请稍候..."));
        showDownloadProgress();
        mgr.refreshIndexAsync();
        return;
    }

    m_modsModel->setModules(mgr.search(QString()));
    updateModActionButtons();
}

void InstanceDetailPage::onModSearchChanged(const QString &text)
{
    if (m_modsProxy) m_modsProxy->setSearchText(text);
}

void InstanceDetailPage::onModFilterChanged(int index)
{
    if (!m_modsProxy || !m_modFilterCombo) return;
    m_modsProxy->setStatusFilter(m_modFilterCombo->itemData(index).toInt());
}

void InstanceDetailPage::onRefreshModsClicked()
{
    if (m_instance.path.isEmpty()) return;
    CKanManager::instance().openInstance(m_instance.path, m_instance.name);
    m_modDetailText->setPlainText(tr("正在刷新仓库索引..."));
    showDownloadProgress();
    CKanManager::instance().refreshIndexAsync(true); // 手动刷新：强制重新下载
}

void InstanceDetailPage::onIndexRefreshed(bool ok, const QString &error)
{
    hideDownloadProgress();
    if (error == QStringLiteral("已取消")) {
        m_modDetailText->setPlainText(tr("已取消仓库索引加载。"));
        return;
    }
    if (!ok) {
        m_modsModel->clear();
        m_modDetailText->setPlainText(tr("仓库索引刷新失败：%1").arg(error));
        QMessageBox::warning(this, tr("刷新失败"), tr("无法获取仓库索引：\n%1").arg(error));
        return;
    }
    m_modsModel->setModules(CKanManager::instance().search(QString()));
    const int n = m_modsModel->rowCount();
    m_modDetailText->setPlainText(tr("仓库索引已就绪，共 %1 个模组。").arg(n));
    updateModActionButtons();
}

void InstanceDetailPage::onModSelectionChanged()
{
    if (!m_modsProxy || !m_modTable) return;
    const QModelIndex idx = m_modTable->currentIndex();
    if (!idx.isValid()) {
        m_currentModIdentifier.clear();
        m_modDetailText->clear();
        updateModActionButtons();
        return;
    }
    const QModelIndex src = m_modsProxy->mapToSource(idx);
    const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
    m_currentModIdentifier = mod.identifier;
    showModDetails(mod);
    updateModActionButtons();
}

void InstanceDetailPage::onModDoubleClicked(const QModelIndex &index)
{
    if (!m_modsProxy || !index.isValid()) return;
    const QModelIndex src = m_modsProxy->mapToSource(index);
    const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
    if (!mod.isValid()) return;
    // 双击：显示依赖与冲突详情
    QString text = mod.name + "  " + mod.version + "\n\n";
    text += tr("依赖：") + (mod.depends.isEmpty() ? tr("（无）")
           : relNames(mod.depends).join(QStringLiteral(", "))) + "\n";
    text += tr("推荐：") + (mod.recommends.isEmpty() ? tr("（无）")
           : relNames(mod.recommends).join(QStringLiteral(", "))) + "\n";
    text += tr("冲突：") + (mod.conflicts.isEmpty() ? tr("（无）")
           : relNames(mod.conflicts).join(QStringLiteral(", ")));
    m_modDetailText->setPlainText(text);
}

void InstanceDetailPage::updateModActionButtons()
{
    if (!m_installModBtn || !m_uninstallModBtn || !m_upgradeModBtn) return;
    const QString id = m_currentModIdentifier;
    const bool hasSel = !id.isEmpty();
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    if (!hasSel) return;

    CKanManager &mgr = CKanManager::instance();
    const bool installed = mgr.isInstalled(id);
    const bool upgradable = mgr.isUpgradable(id);
    m_installModBtn->setEnabled(!installed || upgradable);
    m_uninstallModBtn->setEnabled(installed && !mgr.indexReady() ? true : installed);
    m_upgradeModBtn->setEnabled(upgradable);
}

void InstanceDetailPage::showModDetails(const ckan::CkanModule &mod)
{
    if (!mod.isValid()) return;
    QString text;
    text += "<b>" + mod.name.toHtmlEscaped() + "  " + mod.version.toHtmlEscaped() + "</b>\n\n";
    if (!mod.abstract.isEmpty()) text += mod.abstract.toHtmlEscaped() + "\n\n";
    if (!mod.author.isEmpty()) text += tr("作者：%1\n").arg(mod.author.join(QStringLiteral(", ")));
    if (!mod.license.isEmpty()) text += tr("许可：%1\n").arg(mod.license.join(QStringLiteral(", ")));
    if (!mod.kspVersion.isEmpty()) text += tr("KSP 版本：%1\n").arg(mod.kspVersion);
    if (mod.downloadSize > 0)
        text += tr("下载大小：%1 MB\n").arg(QString::number(mod.downloadSize / 1024.0 / 1024.0, 'f', 1));
    if (!mod.depends.isEmpty()) text += tr("\n依赖：%1").arg(relNames(mod.depends).join(QStringLiteral(", ")));
    if (!mod.conflicts.isEmpty()) text += tr("\n冲突：%1").arg(relNames(mod.conflicts).join(QStringLiteral(", ")));
    m_modDetailText->setHtml(text);
}

void InstanceDetailPage::onInstallModClicked()
{
    if (m_currentModIdentifier.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个模组。"));
        return;
    }
    m_refreshModsBtn->setEnabled(false);
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    showDownloadProgress();
    CKanManager::instance().installAsync(m_currentModIdentifier, true);
}

void InstanceDetailPage::onUninstallModClicked()
{
    if (m_currentModIdentifier.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个模组。"));
        return;
    }
    const QString id = m_currentModIdentifier;
    QString name = id;
    const QModelIndex cur = m_modTable->currentIndex();
    if (cur.isValid()) {
        const QModelIndex src = m_modsProxy->mapToSource(cur);
        const ckan::CkanModule mod = m_modsModel->moduleAt(src.row());
        if (mod.isValid()) name = mod.name;
    }
    if (QMessageBox::question(this, tr("确认卸载"),
            tr("确定要卸载模组 %1 吗？").arg(name.isEmpty() ? id : name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    m_refreshModsBtn->setEnabled(false);
    m_installModBtn->setEnabled(false);
    m_uninstallModBtn->setEnabled(false);
    m_upgradeModBtn->setEnabled(false);
    CKanManager::instance().uninstallAsync(id);
}

void InstanceDetailPage::onUpgradeModClicked()
{
    if (m_currentModIdentifier.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个模组。"));
        return;
    }
    setModButtonsEnabled(false);
    showDownloadProgress();
    CKanManager::instance().upgradeAsync(m_currentModIdentifier);
}

void InstanceDetailPage::setModButtonsEnabled(bool enabled)
{
    m_refreshModsBtn->setEnabled(enabled);
    m_installModBtn->setEnabled(enabled);
    m_uninstallModBtn->setEnabled(enabled);
    m_upgradeModBtn->setEnabled(enabled);
    m_batchInstallBtn->setEnabled(enabled);
    m_batchUpgradeBtn->setEnabled(enabled);
    m_batchUninstallBtn->setEnabled(enabled);
    if (!enabled) return;
    updateModActionButtons();
    updateSelectAllButtonText();
}

void InstanceDetailPage::updateSelectAllButtonText()
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

void InstanceDetailPage::onSelectAllClicked()
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
}

void InstanceDetailPage::onBatchInstallClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先勾选要安装的模组。"));
        return;
    }
    setModButtonsEnabled(false);
    showDownloadProgress();
    CKanManager::instance().installBatchAsync(ids);
}

void InstanceDetailPage::onBatchUpgradeClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先勾选要升级的模组。"));
        return;
    }
    setModButtonsEnabled(false);
    showDownloadProgress();
    CKanManager::instance().upgradeBatchAsync(ids);
}

void InstanceDetailPage::onBatchUninstallClicked()
{
    const QStringList ids = m_modsModel->checkedIdentifiers();
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先勾选要卸载的模组。"));
        return;
    }
    if (QMessageBox::question(this, tr("确认批量卸载"),
            tr("确定要批量卸载已勾选的 %1 个模组吗？").arg(ids.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    setModButtonsEnabled(false);
    CKanManager::instance().uninstallBatchAsync(ids);
}

void InstanceDetailPage::onModOperationFinished(bool ok, const QString &message)
{
    setModButtonsEnabled(true);
    hideDownloadProgress();
    if (ok) {
        m_modsModel->refreshStatus();
        m_modsModel->clearAllChecks();
        updateSelectAllButtonText();
        updateModActionButtons();
        QMessageBox::information(this, tr("完成"), message);
    } else {
        updateModActionButtons();
        QMessageBox::warning(this, tr("操作失败"), message);
    }
}

void InstanceDetailPage::showDownloadProgress()
{
    m_modProgressBar->setRange(0, 1000);
    m_modProgressBar->setValue(0);
    m_modProgressLabel->setText(tr("准备下载..."));
    m_cancelDownloadBtn->setEnabled(true);
    m_modProgressWidget->setVisible(true);
}

void InstanceDetailPage::hideDownloadProgress()
{
    m_modProgressWidget->setVisible(false);
}

namespace {
QString formatBytes(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("?");
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return QStringLiteral("%1 MB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
}
} // namespace

void InstanceDetailPage::onDownloadProgress(const QString &identifier, qint64 doneBytes,
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

void InstanceDetailPage::onCancelDownloadClicked()
{
    CKanManager::instance().cancelCurrentOperation();
    m_modProgressLabel->setText(tr("正在取消..."));
    m_cancelDownloadBtn->setEnabled(false);
}

void InstanceDetailPage::setupAdvancedTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* outerLayout = new QVBoxLayout(tab);
    outerLayout->setContentsMargins(15, 10, 15, 15);

    QFrame* panel = new QFrame(tab);
    panel->setObjectName("pagePanel");
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);

    QLabel* titleLabel = new QLabel(tr("启动参数"), panel);
    titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold;");
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel(tr("在这里输入附加启动参数，例如：-force-d3d11 -popupwindow"), panel);
    descLabel->setStyleSheet("color: #888; font-size: 9pt;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    m_launchArgsEdit = new QLineEdit(panel);
    m_launchArgsEdit->setPlaceholderText(tr("输入启动参数，多个参数用空格分隔"));
    m_launchArgsEdit->setMinimumHeight(36);
    layout->addWidget(m_launchArgsEdit);

    QWidget* btnBar = new QWidget(panel);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_saveLaunchArgsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr(" 确认保存"), btnBar);
    m_saveLaunchArgsBtn->setObjectName("primaryButton");
    m_saveLaunchArgsBtn->setMinimumHeight(36);
    m_saveLaunchArgsBtn->setMinimumWidth(140);
    connect(m_saveLaunchArgsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onSaveLaunchArgsClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_saveLaunchArgsBtn);
    layout->addWidget(btnBar);
    layout->addStretch();

    outerLayout->addWidget(panel);
    m_contentStack->addWidget(tab);
}

void InstanceDetailPage::setupBrowseMenu()
{
    m_browseMenu = new QMenu(this);

    // 游戏根目录
    m_browseRootAction = m_browseMenu->addAction(tr("游戏根目录"));
    m_browseRootAction->setData("root");

    // 其余浏览目标
    struct BrowseEntry {
        const char* data;
        QString label;
    };
    const QList<BrowseEntry> entries = {
        {"ksp_log",      tr("KSP.log")},
        {"logs",         tr("模块日志")},
        {"principia",    tr("Principia日志文件夹")},
        {"gamedata",     tr("模组文件夹")},
        {"ships",        tr("飞船文件夹")},
        {"kos_scripts",  tr("kOS代码文件夹")},
        {"saves",        tr("存档文件夹")}
    };
    m_browseActions.clear();
    m_browsePaths.clear();
    for (const BrowseEntry& e : entries) {
        QAction* action = m_browseMenu->addAction(e.label);
        action->setData(QString::fromLatin1(e.data));
        m_browseActions.append(action);
        m_browsePaths.append(QString());
    }
    for (QAction* action : m_browseActions) {
        connect(action, &QAction::triggered, this, &InstanceDetailPage::onBrowseActionTriggered);
    }
}

void InstanceDetailPage::updateBrowseMenuState()
{
    if (!m_browseMenu) return;
    const QString& root = m_instance.path;
    m_browseRootAction->setEnabled(!root.isEmpty() && QDir(root).exists());
    m_browseRootAction->setData(root);

    const QStringList paths = {
        QDir(root).filePath("KSP.log"),
        QDir(root).filePath("Logs"),
        QDir(root).filePath("glog/Principia"),
        QDir(root).filePath("GameData"),
        QDir(root).filePath("Ships"),
        QDir(root).filePath("Ships/Scripts"),
        QDir(root).filePath("saves")
    };
    for (int i = 0; i < m_browseActions.size() && i < m_browsePaths.size(); ++i) {
        QString p = paths.at(i);
        m_browsePaths[i] = p;
        bool exists = QFileInfo(p).exists();
        m_browseActions[i]->setEnabled(exists);
        m_browseActions[i]->setData(p);
    }
}

void InstanceDetailPage::onBrowseClicked()
{
    if (!m_browseMenu) return;
    updateBrowseMenuState();
    // 弹出菜单时取消按钮选中态，避免状态残留
    m_browseBtn->setChecked(false);
    // 从按钮下方弹出
    QPoint pos = m_browseBtn->mapToGlobal(QPoint(0, m_browseBtn->height() + 4));
    m_browseMenu->exec(pos);
}

void InstanceDetailPage::onBrowseActionTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) return;
    QString path = action->data().toString();
    if (path.isEmpty()) return;
    openBrowseTarget(path);
}

void InstanceDetailPage::openBrowseTarget(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        QMessageBox::information(this, tr("浏览"), tr("目标不存在：\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    QUrl url = QUrl::fromLocalFile(path);
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, tr("浏览"), tr("无法打开：\n%1").arg(QDir::toNativeSeparators(path)));
    }
}

void InstanceDetailPage::loadLaunchArgs()
{
    if (m_instanceId.isEmpty()) return;
    KSPInstance inst = ConfigManager::instance().getInstance(m_instanceId);
    m_launchArgsEdit->setText(inst.launchArgs);
}

void InstanceDetailPage::onSaveLaunchArgsClicked()
{
    QString args = m_launchArgsEdit->text().trimmed();
    ConfigManager::instance().updateInstanceLaunchArgs(m_instanceId, args);
    QMessageBox::information(this, tr("提示"), tr("启动参数已保存"));
}

void InstanceDetailPage::onExportModpackClicked()
{
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"), tr("实例路径为空，无法导出整合包。"));
        return;
    }

    QString gameDataPath = QDir(m_instance.path).filePath("GameData");
    if (!QDir(gameDataPath).exists()) {
        QMessageBox::warning(this, tr("导出失败"), tr("GameData 目录不存在，无法导出整合包。"));
        return;
    }

    // 默认文件名：实例名.zip，保存在启动器根目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString defaultFileName = m_instance.name + ".zip";
    QString defaultFilePath = QDir(appDir).filePath(defaultFileName);

    QString zipFilePath = QFileDialog::getSaveFileName(
        this,
        tr("导出整合包 - 选择保存位置"),
        defaultFilePath,
        tr("ZIP 文件 (*.zip)")
    );

    if (zipFilePath.isEmpty()) {
        return; // 用户取消
    }

    // 确保以 .zip 结尾
    if (!zipFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
        zipFilePath += ".zip";
    }

    // 选择完成后，取消所有按钮选中状态，回到游戏设置
    m_gameSettingsBtn->setChecked(true);
    m_exportModpackBtn->setChecked(false);
    m_contentStack->setCurrentIndex(0);

    // 创建进度对话框
    QProgressDialog progressDialog(tr("正在导出整合包..."), "取消", 0, 100, this);
    progressDialog.setWindowTitle(tr("导出整合包"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setValue(0);
    progressDialog.show();

    // 使用 QCoreApplication::processEvents 确保进度对话框显示
    QCoreApplication::processEvents();

    bool cancelled = false;
    bool success = InstanceManager::instance().exportModpack(
        m_instance.path,
        zipFilePath,
        [&](int progress) {
            // 在 UI 线程中更新进度
            QMetaObject::invokeMethod(&progressDialog, [&progressDialog, &cancelled, progress]() {
                if (progressDialog.wasCanceled()) {
                    cancelled = true;
                    return;
                }
                progressDialog.setValue(progress);
            }, Qt::QueuedConnection);
            // 处理事件以保持 UI 响应
            QCoreApplication::processEvents();
        }
    );

    progressDialog.close();

    if (cancelled) {
        QFile::remove(zipFilePath);
        QMessageBox::information(this, "提示", tr("导出已取消。"));
    } else if (success) {
        QMessageBox::information(this, tr("导出成功"),
            tr("整合包已成功导出到：\n%1").arg(QDir::toNativeSeparators(zipFilePath)));
    } else {
        QMessageBox::warning(this, tr("导出失败"), tr("导出整合包时发生错误，请检查磁盘空间和权限。"));
    }
}

void InstanceDetailPage::refreshIcons(const QString &color)
{
    m_backButton->setIcon(IconUtils::tintedIcon(":/icons/back.svg", color));
    m_gameSettingsBtn->setIcon(IconUtils::tintedIcon(":/icons/sliders.svg", color));
    m_dlcBtn->setIcon(IconUtils::tintedIcon(":/icons/package.svg", color));
    m_modsBtn->setIcon(IconUtils::tintedIcon(":/icons/puzzle.svg", color));
    m_savesBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
    m_advancedBtn->setIcon(IconUtils::tintedIcon(":/icons/settings.svg", color));
    m_exportModpackBtn->setIcon(IconUtils::tintedIcon(":/icons/database.svg", color));
    m_browseBtn->setIcon(IconUtils::tintedIcon(":/icons/folder-open.svg", color));
    m_saveLaunchArgsBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
    m_refreshModsBtn->setIcon(IconUtils::tintedIcon(":/icons/refresh.svg", color));
    m_installModBtn->setIcon(IconUtils::tintedIcon(":/icons/add.svg", color));
    m_uninstallModBtn->setIcon(IconUtils::tintedIcon(":/icons/trash-2.svg", color));
    m_upgradeModBtn->setIcon(IconUtils::tintedIcon(":/icons/check.svg", color));
}
