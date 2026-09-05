#include "instancedetailpage.h"
#include "modstabpage.h"
#include "gamesettingstabpage.h"
#include "dlctabpage.h"
#include "advancedtabpage.h"
#include "modpackcontroller.h"
#include "../ckanmanager.h"
#include "../iconutils.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

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
    // 各二级 tab 拆为独立页面类；栈下标按 showSection 约定：0=游戏设置 1=DLC 2=模组管理 3=高级
    m_gameSettingsTabPage = new GameSettingsTabPage(m_contentStack);
    m_contentStack->addWidget(m_gameSettingsTabPage);
    m_dlcTabPage = new DlcTabPage(m_contentStack);
    m_contentStack->addWidget(m_dlcTabPage);
    m_modsTabPage = new ModsTabPage(m_contentStack);
    m_contentStack->addWidget(m_modsTabPage);
    m_advancedTabPage = new AdvancedTabPage(m_contentStack);
    m_contentStack->addWidget(m_advancedTabPage);
    setupBrowseMenu();

    // 整合包导入/导出流程控制器（对话框以本页为父窗口）
    m_modpackController = new ModpackController(this, this);
    m_modpackController->setInstance(m_instance);
    connect(m_modpackController, &ModpackController::showSettingsRequested, this, &InstanceDetailPage::onModpackShowSettings);
    connect(m_modpackController, &ModpackController::modsReloadRequested,  this, &InstanceDetailPage::onModpackReloadMods);
    connect(m_modpackController, &ModpackController::modsInstallRequested, this, &InstanceDetailPage::onModpackModsInstall);

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
        // 切到存档页：先离开模组管理（停轮询），再回到游戏设置 tab 兜底
        m_modsTabPage->setTabActive(false);
        emit savesManageRequested();
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
    m_exportModpackBtn->setChecked(false);

    m_modsTabPage->setTabActive(detailIndex == 2);
    m_contentStack->setCurrentIndex(detailIndex);

    if (detailIndex == 3) {
        m_advancedTabPage->loadLaunchArgs(m_instanceId);
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
    m_gameSettingsTabPage->loadGameSettings(m_instance.path);
    m_dlcTabPage->loadDLCs(m_instance.path);
    m_advancedTabPage->loadLaunchArgs(m_instanceId);
    m_modpackController->setInstance(m_instance);
    m_modsTabPage->setInstance(m_instance, m_instanceId);
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
    m_advancedTabPage->refreshIcons(color);
    m_modsTabPage->refreshIcons(color);
}

// ---- 整合包导出/导入菜单入口（实际流程在 ModpackController） ----

void InstanceDetailPage::onExportModpackClicked()
{
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"), tr("实例路径为空，无法导出整合包。"));
        m_exportModpackBtn->setChecked(false);
        return;
    }

    // 弹出菜单让用户选择导出方式
    QMenu menu(this);
    QAction *zipAction = menu.addAction(tr("打包 GameData 为 ZIP"));
    QAction *ckanAction = menu.addAction(tr("导出为 CKAN 文件"));
    m_exportModpackBtn->setChecked(false);
    const QPoint pos = m_exportModpackBtn->mapToGlobal(QPoint(0, m_exportModpackBtn->height() + 4));
    QAction *selected = menu.exec(pos);
    if (selected == zipAction)
        m_modpackController->exportAsZip();
    else if (selected == ckanAction)
        m_modpackController->exportAsCkan();
}

void InstanceDetailPage::onImportModpackClicked()
{
    m_importModpackBtn->setChecked(false);
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(this, tr("导入失败"), tr("实例路径为空，无法导入整合包。"));
        return;
    }
    if (!QDir(m_instance.path + QStringLiteral("/GameData")).exists()) {
        QMessageBox::warning(this, tr("导入失败"), tr("GameData 目录不存在，无法导入整合包。"));
        return;
    }

    // 弹出菜单让用户选择导入方式
    QMenu menu(this);
    QAction *zipAction = menu.addAction(tr("从 ZIP 导入"));
    QAction *ckanAction = menu.addAction(tr("从 .ckan 文件导入"));
    const QPoint pos = m_importModpackBtn->mapToGlobal(QPoint(0, m_importModpackBtn->height() + 4));
    QAction *selected = menu.exec(pos);
    if (selected == zipAction)
        m_modpackController->importFromZip();
    else if (selected == ckanAction)
        m_modpackController->importFromCkan();
}

// ---- ModpackController 信号接线 ----

void InstanceDetailPage::onModpackShowSettings()
{
    m_gameSettingsBtn->setChecked(true);
    m_exportModpackBtn->setChecked(false);
    m_importModpackBtn->setChecked(false);
    m_contentStack->setCurrentIndex(0);
}

void InstanceDetailPage::onModpackReloadMods()
{
    // 重启 CKan（注册表/Filesystem 已变），然后重新准备并预填充模组模型
    CKanManager::instance().closeInstance();
    m_modsTabPage->prepareMods();
    m_modsTabPage->maybePopulateMods();
}

void InstanceDetailPage::onModpackModsInstall(const QStringList &identifiers)
{
    // 跳转到模组管理界面进行下载（showSection 会切栈、勾选侧栏并激活模组页；
    // queueCkanInstall 绑实例、确保数据就绪，索引未就绪则进入待装清单，就绪后自动安装）
    showSection(2);
    m_modsTabPage->queueCkanInstall(identifiers);
}

// ---- 浏览菜单 ----

void InstanceDetailPage::setupBrowseMenu()
{
    m_browseMenu = new QMenu(this);
    m_browseRootAction = m_browseMenu->addAction(tr("游戏根目录"));
    m_browseRootAction->setData("root");

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
    connect(m_browseRootAction, &QAction::triggered, this, &InstanceDetailPage::onBrowseActionTriggered);
    for (QAction* action : m_browseActions) {
        connect(action, &QAction::triggered, this, &InstanceDetailPage::onBrowseActionTriggered);
    }
}

void InstanceDetailPage::updateBrowseMenuState()
{
    if (!m_browseMenu) return;
    const QString& root = m_instance.path;
    const bool rootExists = !root.isEmpty() && QDir(root).exists();

    m_browseBtn->setVisible(rootExists);
    m_browseRootAction->setVisible(rootExists);
    m_browseRootAction->setEnabled(rootExists);
    m_browseRootAction->setData(root);
    if (!rootExists) return;

    const QStringList paths = {
        QDir(root).filePath("KSP.log"),
        QDir(root).filePath("Logs"),
        QDir(root).filePath("glog/Principia"),
        QDir(root).filePath("GameData"),
        QDir(root).filePath("Ships"),
        QDir(root).filePath("Ships/Script"),
        QDir(root).filePath("saves")
    };
    for (int i = 0; i < m_browseActions.size() && i < m_browsePaths.size(); ++i) {
        QString p = paths.at(i);
        if (i == 5 && !QFileInfo(p).exists()) {
            QString alt = QDir(root).filePath("Ships/Scripts");
            if (QFileInfo(alt).exists())
                p = alt;
        }
        m_browsePaths[i] = p;
        bool exists = QFileInfo(p).exists();
        m_browseActions[i]->setVisible(exists);
        m_browseActions[i]->setEnabled(exists);
        m_browseActions[i]->setData(p);
    }
}

void InstanceDetailPage::onBrowseClicked()
{
    if (!m_browseMenu) return;
    updateBrowseMenuState();
    m_browseBtn->setChecked(false);
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