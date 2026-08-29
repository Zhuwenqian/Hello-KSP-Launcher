#include "saveslistpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include "../instancemanager.h"
#include "../iconutils.h"

SavesListPage::SavesListPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("savesListPage");
    setupUI();
}

void SavesListPage::setupUI()
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
    connect(m_backButton, &QPushButton::clicked, this, &SavesListPage::onBackClicked);

    m_titleLabel = new QLabel(tr("存档管理"), topBar);
    m_titleLabel->setObjectName("pageTitle");

    topBarLayout->addWidget(m_backButton);
    topBarLayout->addWidget(m_titleLabel, 1);

    m_savesList = new QListWidget(this);
    m_savesList->setObjectName("savesListWidget");
    connect(m_savesList, &QListWidget::itemDoubleClicked, this, &SavesListPage::onSaveItemDoubleClicked);

    QWidget* rightWidget = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(topBar);
    rightLayout->addWidget(m_savesList, 1);

    // 左侧实例管理二级菜单（与详情页一致）：游戏设置 / DLC / 模组管理 / 存档管理 / 高级
    m_instanceSidebar = new QWidget(this);
    m_instanceSidebar->setFixedWidth(200);
    m_instanceSidebar->setObjectName("sidebarWidget");
    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_instanceSidebar);
    sidebarLayout->setContentsMargins(0, 10, 0, 10);
    sidebarLayout->setSpacing(0);

    m_navGameSettingsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/sliders.svg", "#ffffff"), tr("  游戏设置"), m_instanceSidebar);
    m_navGameSettingsBtn->setObjectName("detailNavButton");
    m_navGameSettingsBtn->setCheckable(true);
    m_navGameSettingsBtn->setMinimumHeight(40);
    connect(m_navGameSettingsBtn, &QPushButton::clicked, this, &SavesListPage::onInstanceNavClicked);

    m_navDlcBtn = new QPushButton(IconUtils::tintedIcon(":/icons/package.svg", "#ffffff"), tr("  DLC"), m_instanceSidebar);
    m_navDlcBtn->setObjectName("detailNavButton");
    m_navDlcBtn->setCheckable(true);
    m_navDlcBtn->setMinimumHeight(40);
    connect(m_navDlcBtn, &QPushButton::clicked, this, &SavesListPage::onInstanceNavClicked);

    m_navModsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/puzzle.svg", "#ffffff"), tr("  模组管理"), m_instanceSidebar);
    m_navModsBtn->setObjectName("detailNavButton");
    m_navModsBtn->setCheckable(true);
    m_navModsBtn->setMinimumHeight(40);
    connect(m_navModsBtn, &QPushButton::clicked, this, &SavesListPage::onInstanceNavClicked);

    m_navSavesBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr("  存档管理"), m_instanceSidebar);
    m_navSavesBtn->setObjectName("detailNavButton");
    m_navSavesBtn->setCheckable(true);
    m_navSavesBtn->setMinimumHeight(40);
    connect(m_navSavesBtn, &QPushButton::clicked, this, &SavesListPage::onInstanceNavClicked);

    m_navAdvancedBtn = new QPushButton(IconUtils::tintedIcon(":/icons/settings.svg", "#ffffff"), tr("  高级"), m_instanceSidebar);
    m_navAdvancedBtn->setObjectName("detailNavButton");
    m_navAdvancedBtn->setCheckable(true);
    m_navAdvancedBtn->setMinimumHeight(40);
    connect(m_navAdvancedBtn, &QPushButton::clicked, this, &SavesListPage::onInstanceNavClicked);

    m_navExportBtn = new QPushButton(IconUtils::tintedIcon(":/icons/database.svg", "#ffffff"), tr("  导出整合包"), m_instanceSidebar);
    m_navExportBtn->setObjectName("detailNavButton");
    m_navExportBtn->setCheckable(true);
    m_navExportBtn->setMinimumHeight(40);
    connect(m_navExportBtn, &QPushButton::clicked, this, [this]{
        m_navExportBtn->setChecked(false);
        emit modpackActionRequested(0);
    });

    m_navImportBtn = new QPushButton(IconUtils::tintedIcon(":/icons/folder-open.svg", "#ffffff"), tr("  导入整合包"), m_instanceSidebar);
    m_navImportBtn->setObjectName("detailNavButton");
    m_navImportBtn->setCheckable(true);
    m_navImportBtn->setMinimumHeight(40);
    connect(m_navImportBtn, &QPushButton::clicked, this, [this]{
        m_navImportBtn->setChecked(false);
        emit modpackActionRequested(1);
    });

    m_navBrowseBtn = new QPushButton(IconUtils::tintedIcon(":/icons/folder-open.svg", "#ffffff"), tr("  浏览"), m_instanceSidebar);
    m_navBrowseBtn->setObjectName("detailNavButton");
    m_navBrowseBtn->setCheckable(true);
    m_navBrowseBtn->setMinimumHeight(40);
    connect(m_navBrowseBtn, &QPushButton::clicked, this, [this]{
        m_navBrowseBtn->setChecked(false);
        emit modpackActionRequested(2);
    });

    sidebarLayout->addWidget(m_navGameSettingsBtn);
    sidebarLayout->addWidget(m_navDlcBtn);
    sidebarLayout->addWidget(m_navModsBtn);
    sidebarLayout->addWidget(m_navSavesBtn);
    sidebarLayout->addWidget(m_navAdvancedBtn);
    sidebarLayout->addWidget(m_navExportBtn);
    sidebarLayout->addWidget(m_navImportBtn);
    sidebarLayout->addWidget(m_navBrowseBtn);
    sidebarLayout->addStretch();

    m_navSavesBtn->setChecked(true);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_instanceSidebar);
    contentLayout->addWidget(rightWidget, 1);

    mainLayout->addLayout(contentLayout, 1);
}

void SavesListPage::onInstanceNavClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int idx = -1;
    if (btn == m_navGameSettingsBtn) idx = 0;
    else if (btn == m_navDlcBtn) idx = 1;
    else if (btn == m_navModsBtn) idx = 2;
    else if (btn == m_navAdvancedBtn) idx = 3;

    if (btn == m_navSavesBtn) {
        // 本身就在存档管理，无需跳转，仅保持高亮
        Q_UNUSED(idx);
        return;
    }

    if (idx >= 0) {
        emit navToDetail(idx);
    }
}

void SavesListPage::setInstanceId(const QString &id)
{
    m_instanceId = id;
    m_instance = ConfigManager::instance().getInstance(id);
    m_titleLabel->setText("存档管理 - " + m_instance.name);
    loadSaves();
}

void SavesListPage::loadSaves()
{
    m_savesList->clear();
    if (m_instance.path.isEmpty()) return;

    QStringList saveNames = InstanceManager::instance().listSaves(m_instance.path);
    QString savesDir = InstanceManager::instance().getSavesDir(m_instance.path);

    for (const QString& saveName : saveNames) {
        QString savePath = QDir(savesDir).filePath(saveName);
        SaveInfo info = InstanceManager::instance().loadSaveInfo(savePath);

        QWidget* itemWidget = new QWidget(m_savesList);
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(20, 15, 20, 15);

        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(saveName, itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 11pt;");

        QLabel* infoLabel = new QLabel(itemWidget);
        QString modeDisplay = info.mode;
        if (modeDisplay == "SANDBOX") modeDisplay = tr("沙盒模式");
        else if (modeDisplay == "CAREER") modeDisplay = tr("生涯模式");
        else if (modeDisplay == "SCIENCE_SANDBOX") modeDisplay = tr("科学模式");
        QString infoText = tr("版本: %1 | 模式: %2").arg(info.version, modeDisplay);
        if (info.modded) {
            infoText += tr(" | 模组");
        }
        infoLabel->setText(infoText);
        infoLabel->setStyleSheet("color: #888; font-size: 9pt;");

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(infoLabel);

        QLabel* arrowLabel = new QLabel("→", itemWidget);
        arrowLabel->setStyleSheet("color: #888; font-size: 14pt;");

        layout->addLayout(textLayout, 1);
        layout->addWidget(arrowLabel);

        QListWidgetItem* item = new QListWidgetItem(m_savesList);
        item->setSizeHint(QSize(0, 80));
        item->setData(Qt::UserRole, savePath);
        m_savesList->addItem(item);
        m_savesList->setItemWidget(item, itemWidget);
    }

    if (saveNames.isEmpty()) {
        m_savesList->addItem(tr("（未检测到存档）"));
    }
}

void SavesListPage::onBackClicked()
{
    emit backClicked();
}

void SavesListPage::onSaveItemDoubleClicked(QListWidgetItem *item)
{
    QString savePath = item->data(Qt::UserRole).toString();
    if (!savePath.isEmpty()) {
        emit saveSelected(savePath, m_instance.name);
    }
}

void SavesListPage::refreshIcons(const QString &color)
{
    m_backButton->setIcon(IconUtils::tintedIcon(":/icons/back.svg", color));
    m_navGameSettingsBtn->setIcon(IconUtils::tintedIcon(":/icons/sliders.svg", color));
    m_navDlcBtn->setIcon(IconUtils::tintedIcon(":/icons/package.svg", color));
    m_navModsBtn->setIcon(IconUtils::tintedIcon(":/icons/puzzle.svg", color));
    m_navSavesBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
    m_navAdvancedBtn->setIcon(IconUtils::tintedIcon(":/icons/settings.svg", color));
    m_navExportBtn->setIcon(IconUtils::tintedIcon(":/icons/database.svg", color));
    m_navImportBtn->setIcon(IconUtils::tintedIcon(":/icons/folder-open.svg", color));
    m_navBrowseBtn->setIcon(IconUtils::tintedIcon(":/icons/folder-open.svg", color));
}
