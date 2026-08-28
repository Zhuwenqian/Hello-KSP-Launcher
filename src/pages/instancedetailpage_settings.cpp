// 实例管理详情页 - 游戏设置 / DLC / 高级 / 浏览 功能实现
#include "instancedetailpage.h"
#include "../widgets/toggleswitch.h"
#include "../iconutils.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QMap>
#include <QStyledItemDelegate>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFrame>
#include <QFont>
#include <QAction>

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

    // 设置搜索框：按设置项显示名实时过滤
    m_settingsSearchEdit = new QLineEdit(tab);
    m_settingsSearchEdit->setPlaceholderText(tr("搜索设置项..."));
    m_settingsSearchEdit->setClearButtonEnabled(true);
    m_settingsSearchEdit->setMinimumHeight(30);
    connect(m_settingsSearchEdit, &QLineEdit::textChanged,
            this, &InstanceDetailPage::onSettingsSearchChanged);

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

    layout->addWidget(m_settingsSearchEdit);
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
    // 同时连接游戏根目录项，否则点击无响应（无法打开）
    connect(m_browseRootAction, &QAction::triggered,
            this, &InstanceDetailPage::onBrowseActionTriggered);
    for (QAction* action : m_browseActions) {
        connect(action, &QAction::triggered, this, &InstanceDetailPage::onBrowseActionTriggered);
    }
}

void InstanceDetailPage::updateBrowseMenuState()
{
    if (!m_browseMenu) return;

    // 以当前实例的路径实时检查各浏览目标是否存在；不存在的子项隐藏。
    // 每次打开浏览菜单（onBrowseClicked）都会调用这里，实例切换时也会随
    // refreshData() 重新执行，因此同一实例路径变化后能及时反映最新状态。
    const QString& root = m_instance.path;
    const bool rootExists = !root.isEmpty() && QDir(root).exists();

    // 游戏根目录缺失时整条「浏览」按钮隐藏（防御性；正常实例根目录必然存在）
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
        QDir(root).filePath("Ships/Script"),   // kOS 默认脚本归档为单数 Script
        QDir(root).filePath("saves")
    };
    for (int i = 0; i < m_browseActions.size() && i < m_browsePaths.size(); ++i) {
        QString p = paths.at(i);
        // kOS 默认脚本目录为单数 Script，个别实例用复数 Scripts，都尝试
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

    // 重新加载后按当前搜索词重新过滤。
    if (m_settingsSearchEdit) {
        onSettingsSearchChanged(m_settingsSearchEdit->text());
    }
}

void InstanceDetailPage::onSettingsSearchChanged(const QString &text)
{
    if (!m_settingsTree) return;
    const QString query = text.trimmed();
    for (int i = 0; i < m_settingsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* categoryItem = m_settingsTree->topLevelItem(i);
        int visible = 0;
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            QTreeWidgetItem* item = categoryItem->child(j);
            // 按设置项显示名（当前界面语言下的名字）匹配
            bool match = query.isEmpty()
                || item->text(0).contains(query, Qt::CaseInsensitive);
            item->setHidden(!match);
            if (match) ++visible;
        }
        // 分类节点：没有任何匹配子项时隐藏
        categoryItem->setHidden(visible == 0);
        if (visible > 0) categoryItem->setExpanded(true);
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
