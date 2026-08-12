#include "instancedetailpage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QMap>
#include <QIcon>

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

    m_backButton = new QPushButton(QIcon(":/icons/back.svg"), " 返回", topBar);
    m_backButton->setObjectName("backButton");
    m_backButton->setFixedHeight(40);
    m_backButton->setMinimumWidth(100);
    connect(m_backButton, &QPushButton::clicked, this, &InstanceDetailPage::onBackClicked);

    m_titleLabel = new QLabel("实例管理", topBar);
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

    m_gameSettingsBtn = new QPushButton(QIcon(":/icons/sliders.svg"), "  游戏设置", m_detailSidebar);
    m_gameSettingsBtn->setObjectName("detailNavButton");
    m_gameSettingsBtn->setCheckable(true);
    m_gameSettingsBtn->setChecked(true);
    m_gameSettingsBtn->setMinimumHeight(40);
    connect(m_gameSettingsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_dlcBtn = new QPushButton(QIcon(":/icons/package.svg"), "  DLC", m_detailSidebar);
    m_dlcBtn->setObjectName("detailNavButton");
    m_dlcBtn->setCheckable(true);
    m_dlcBtn->setMinimumHeight(40);
    connect(m_dlcBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    m_modsBtn = new QPushButton(QIcon(":/icons/puzzle.svg"), "  模组管理", m_detailSidebar);
    m_modsBtn->setObjectName("detailNavButton");
    m_modsBtn->setCheckable(true);
    m_modsBtn->setMinimumHeight(40);
    connect(m_modsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    sidebarLayout->addWidget(m_gameSettingsBtn);
    sidebarLayout->addWidget(m_dlcBtn);
    sidebarLayout->addWidget(m_modsBtn);
    sidebarLayout->addStretch();

    m_contentStack = new QStackedWidget(this);
    setupGameSettingsTab();
    setupDLCTab();
    setupModsTab();

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
    m_settingsTree->setHeaderLabels({"设置项", "值"});
    m_settingsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_settingsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_settingsTree->setAlternatingRowColors(true);
    m_settingsTree->setRootIsDecorated(false);

    QWidget* btnBar = new QWidget(tab);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton* saveBtn = new QPushButton(QIcon(":/icons/save.svg"), " 保存设置", btnBar);
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

    QLabel* note = new QLabel("注：模组列表仅显示GameData目录下的文件夹", tab);
    note->setStyleSheet("color: #888; font-size: 9pt;");

    m_modList = new QListWidget(tab);
    layout->addWidget(note);
    layout->addWidget(m_modList, 1);
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

    if (btn == m_gameSettingsBtn) {
        m_contentStack->setCurrentIndex(0);
    } else if (btn == m_dlcBtn) {
        m_contentStack->setCurrentIndex(1);
    } else if (btn == m_modsBtn) {
        m_contentStack->setCurrentIndex(2);
    }
}

void InstanceDetailPage::refreshData()
{
    if (m_instance.path.isEmpty()) return;
    loadGameSettings();
    loadDLCs();
    loadMods();
}

void InstanceDetailPage::loadGameSettings()
{
    m_settingsTree->clear();
    m_currentSettings = InstanceManager::instance().loadGameSettings(m_instance.path);
    for (const GameSetting& s : m_currentSettings) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_settingsTree);
        item->setText(0, s.displayName);
        item->setText(1, s.value);
        item->setData(0, Qt::UserRole, s.key); // Store original key
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
}

bool InstanceDetailPage::saveGameSettings()
{
    QList<GameSetting> updatedSettings = m_currentSettings;
    for (int i = 0; i < m_settingsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_settingsTree->topLevelItem(i);
        QString originalKey = item->data(0, Qt::UserRole).toString();
        QString newValue = item->text(1);
        for (GameSetting& s : updatedSettings) {
            if (s.key == originalKey) {
                s.value = newValue;
                break;
            }
        }
    }
    bool ok = InstanceManager::instance().saveGameSettings(m_instance.path, updatedSettings);
    if (ok) {
        m_currentSettings = updatedSettings;
        QMessageBox::information(this, "保存成功", "游戏设置已保存！");
    } else {
        QMessageBox::warning(this, "保存失败", "无法保存游戏设置，请检查文件权限。");
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

    // Chinese DLC names
    QMap<QString, QString> dlcNames;
    dlcNames["MakingHistory"] = "历史扩展包 (Making History)";
    dlcNames["Serenity"] = "地面扩展包 (Breaking Ground)";

    for (const DLCDetection& dlc : dlcs) {
        QWidget* itemWidget = new QWidget(m_dlcList);
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(15, 12, 15, 12);

        QLabel* nameLabel = new QLabel(dlcNames.value(dlc.id, dlc.displayName), itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");

        QLabel* statusLabel = new QLabel(itemWidget);
        if (dlc.installed) {
            statusLabel->setText("● 已安装");
            statusLabel->setObjectName("dlcInstalled");
        } else {
            statusLabel->setText("○ 未安装");
            statusLabel->setObjectName("dlcNotInstalled");
        }
        statusLabel->setMinimumWidth(80);

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
    m_modList->clear();
    QStringList mods = InstanceManager::instance().listMods(m_instance.path);
    for (const QString& mod : mods) {
        m_modList->addItem(mod);
    }
    if (mods.isEmpty()) {
        m_modList->addItem("（未检测到第三方模组）");
    }
}
