#include "instancedetailpage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QMap>
#include <QIcon>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QLineEdit>

// Custom delegate to control editing: only column 1 (value) editable, bool values use combo box
class SettingsItemDelegate : public QStyledItemDelegate {
public:
    explicit SettingsItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.column() != 1) return nullptr; // Only value column editable

        QString value = index.data(Qt::DisplayRole).toString();
        if (value == "True" || value == "False" || value == "true" || value == "false") {
            QComboBox* combo = new QComboBox(parent);
            combo->addItem("True");
            combo->addItem("False");
            combo->setCurrentText(value);
            return combo;
        }
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);
        if (combo) {
            QString value = index.data(Qt::DisplayRole).toString();
            combo->setCurrentText(value);
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);
        if (combo) {
            model->setData(index, combo->currentText());
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
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

    m_advancedBtn = new QPushButton(QIcon(":/icons/settings.svg"), "  高级", m_detailSidebar);
    m_advancedBtn->setObjectName("detailNavButton");
    m_advancedBtn->setCheckable(true);
    m_advancedBtn->setMinimumHeight(40);
    connect(m_advancedBtn, &QPushButton::clicked, this, &InstanceDetailPage::onNavButtonClicked);

    sidebarLayout->addWidget(m_gameSettingsBtn);
    sidebarLayout->addWidget(m_dlcBtn);
    sidebarLayout->addWidget(m_modsBtn);
    sidebarLayout->addWidget(m_advancedBtn);
    sidebarLayout->addStretch();

    m_contentStack = new QStackedWidget(this);
    setupGameSettingsTab();
    setupDLCTab();
    setupModsTab();
    setupAdvancedTab();

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
    m_settingsTree->setRootIsDecorated(true);
    m_settingsTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    m_settingsTree->setItemDelegate(new SettingsItemDelegate(m_settingsTree));

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
    m_advancedBtn->setChecked(btn == m_advancedBtn);

    if (btn == m_gameSettingsBtn) {
        m_contentStack->setCurrentIndex(0);
    } else if (btn == m_dlcBtn) {
        m_contentStack->setCurrentIndex(1);
    } else if (btn == m_modsBtn) {
        m_contentStack->setCurrentIndex(2);
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
            item->setText(1, s.value);
            item->setData(0, Qt::UserRole, s.key);
            // ItemIsEditable flag needed, delegate controls which column actually edits
            item->setFlags(item->flags() | Qt::ItemIsEditable);
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
    m_modList->clear();
    QStringList mods = InstanceManager::instance().listMods(m_instance.path);
    for (const QString& mod : mods) {
        m_modList->addItem(mod);
    }
    if (mods.isEmpty()) {
        m_modList->addItem("（未检测到第三方模组）");
    }
}

void InstanceDetailPage::setupAdvancedTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(10);

    QLabel* titleLabel = new QLabel("启动参数", tab);
    titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold;");
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel("在这里输入附加启动参数，例如：-force-d3d11 -popupwindow", tab);
    descLabel->setStyleSheet("color: #888; font-size: 9pt;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    m_launchArgsEdit = new QLineEdit(tab);
    m_launchArgsEdit->setPlaceholderText("输入启动参数，多个参数用空格分隔");
    m_launchArgsEdit->setMinimumHeight(36);
    layout->addWidget(m_launchArgsEdit);

    QWidget* btnBar = new QWidget(tab);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_saveLaunchArgsBtn = new QPushButton(QIcon(":/icons/save.svg"), " 确认保存", btnBar);
    m_saveLaunchArgsBtn->setObjectName("primaryButton");
    m_saveLaunchArgsBtn->setMinimumHeight(36);
    m_saveLaunchArgsBtn->setMinimumWidth(140);
    connect(m_saveLaunchArgsBtn, &QPushButton::clicked, this, &InstanceDetailPage::onSaveLaunchArgsClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_saveLaunchArgsBtn);
    layout->addWidget(btnBar);
    layout->addStretch();

    m_contentStack->addWidget(tab);
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
    QMessageBox::information(this, "提示", "启动参数已保存");
}
