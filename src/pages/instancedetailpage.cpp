#include "instancedetailpage.h"
#include "../ckanmanager.h"
#include "../iconutils.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QTimer>

InstanceDetailPage::InstanceDetailPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("instanceDetailPage");
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

    m_importModpackBtn = new QPushButton(IconUtils::tintedIcon(":/icons/folder-open.svg", "#ffffff"), tr("  导入整合包"), m_detailSidebar);
    m_importModpackBtn->setObjectName("detailNavButton");
    m_importModpackBtn->setCheckable(true);
    m_importModpackBtn->setMinimumHeight(40);
    connect(m_importModpackBtn, &QPushButton::clicked, this, &InstanceDetailPage::onImportModpackClicked);

    sidebarLayout->addWidget(m_gameSettingsBtn);
    sidebarLayout->addWidget(m_dlcBtn);
    sidebarLayout->addWidget(m_modsBtn);
    sidebarLayout->addWidget(m_savesBtn);
    sidebarLayout->addWidget(m_advancedBtn);
    sidebarLayout->addWidget(m_exportModpackBtn);
    sidebarLayout->addWidget(m_importModpackBtn);
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

    if (btn == m_savesBtn) {
        m_modsTabActive = false;
        emit savesManageRequested();
        // 回到游戏设置tab，避免下次进来还是选中存档管理
        showSection(0);
        return;
    }

    int idx = -1;
    if (btn == m_gameSettingsBtn) idx = 0;
    else if (btn == m_dlcBtn) idx = 1;
    else if (btn == m_modsBtn) idx = 2;
    else if (btn == m_advancedBtn) idx = 3;
    if (idx >= 0)
        showSection(idx);
}

void InstanceDetailPage::showSection(int detailIndex)
{
    m_gameSettingsBtn->setChecked(detailIndex == 0);
    m_dlcBtn->setChecked(detailIndex == 1);
    m_modsBtn->setChecked(detailIndex == 2);
    m_advancedBtn->setChecked(detailIndex == 3);
    m_browseBtn->setChecked(false);
    m_importModpackBtn->setChecked(false);

    m_modsTabActive = (detailIndex == 2);
    m_contentStack->setCurrentIndex(detailIndex);

    // 注册表锁等待仅在该页前台时轮询：进入模组页则恢复轮询，离开则停止
    if (detailIndex == 2) {
        if (m_registryLockWaiting && m_registryLockPollTimer
            && !m_registryLockPollTimer->isActive())
            m_registryLockPollTimer->start();
    } else if (m_registryLockWaiting && m_registryLockPollTimer) {
        m_registryLockPollTimer->stop();
    }

    if (detailIndex == 2) {
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
    } else if (detailIndex == 3) {
        loadLaunchArgs();
    }
}

void InstanceDetailPage::triggerExportModpack()
{
    if (m_instance.path.isEmpty()) return;
    onExportModpackClicked();
}

void InstanceDetailPage::triggerImportModpack()
{
    if (m_instance.path.isEmpty()) return;
    onImportModpackClicked();
}

void InstanceDetailPage::triggerBrowse()
{
    if (m_instance.path.isEmpty()) return;
    onBrowseClicked();
}

void InstanceDetailPage::refreshData()
{
    if (m_instance.path.isEmpty()) return;
    updateBrowseMenuState();
    loadGameSettings();
    loadDLCs();
    loadLaunchArgs();
    prepareMods();
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
    m_importModpackBtn->setIcon(IconUtils::tintedIcon(":/icons/folder-open.svg", color));
    m_browseBtn->setIcon(IconUtils::tintedIcon(":/icons/folder-open.svg", color));
    m_saveLaunchArgsBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
    m_refreshModsBtn->setIcon(IconUtils::tintedIcon(":/icons/refresh.svg", color));
    m_installModBtn->setIcon(IconUtils::tintedIcon(":/icons/add.svg", color));
    m_uninstallModBtn->setIcon(IconUtils::tintedIcon(":/icons/trash-2.svg", color));
    m_upgradeModBtn->setIcon(IconUtils::tintedIcon(":/icons/check.svg", color));
    m_compatBtn->setIcon(IconUtils::tintedIcon(":/icons/rocket.svg", color));
}
