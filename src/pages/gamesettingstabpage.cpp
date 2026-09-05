// 实例详情页 - 游戏设置 tab
#include "gamesettingstabpage.h"
#include "../widgets/toggleswitch.h"
#include "../iconutils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QMap>
#include <QStyledItemDelegate>
#include <QFont>

// Custom delegate to control editing: only column 1 (value) editable
class SettingsItemDelegate : public QStyledItemDelegate
{
public:
    explicit SettingsItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override {
        if (index.column() != 1) return nullptr; // Only value column editable
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
};

GameSettingsTabPage::GameSettingsTabPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("gameSettingsTabPage");
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(10);

    m_settingsTree = new QTreeWidget(this);
    m_settingsTree->setColumnCount(2);
    m_settingsTree->setHeaderLabels({tr("设置项"), tr("值")});
    m_settingsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_settingsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_settingsTree->setAlternatingRowColors(true);
    m_settingsTree->setRootIsDecorated(true);
    m_settingsTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    m_settingsTree->setItemDelegate(new SettingsItemDelegate(m_settingsTree));

    // 设置搜索框：按设置项显示名实时过滤
    m_settingsSearchEdit = new QLineEdit(this);
    m_settingsSearchEdit->setPlaceholderText(tr("搜索设置项..."));
    m_settingsSearchEdit->setClearButtonEnabled(true);
    m_settingsSearchEdit->setMinimumHeight(30);
    connect(m_settingsSearchEdit, &QLineEdit::textChanged,
            this, &GameSettingsTabPage::onSettingsSearchChanged);

    QWidget* btnBar = new QWidget(this);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton* saveBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr(" 保存设置"), btnBar);
    saveBtn->setObjectName("primaryButton");
    saveBtn->setMinimumHeight(36);
    saveBtn->setMinimumWidth(140);
    connect(saveBtn, &QPushButton::clicked, this, &GameSettingsTabPage::onSaveSettingsClicked);
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);

    layout->addWidget(m_settingsSearchEdit);
    layout->addWidget(m_settingsTree, 1);
    layout->addWidget(btnBar);
}

void GameSettingsTabPage::loadGameSettings(const QString &gamePath)
{
    m_gamePath = gamePath;
    m_settingsTree->clear();
    m_currentSettings = InstanceManager::instance().loadGameSettings(gamePath);

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

void GameSettingsTabPage::onSettingsSearchChanged(const QString &text)
{
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

bool GameSettingsTabPage::saveGameSettings()
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
    if (m_gamePath.isEmpty())
        return false;
    bool ok = InstanceManager::instance().saveGameSettings(m_gamePath, updatedSettings);
    if (ok) {
        m_currentSettings = updatedSettings;
        QMessageBox::information(this, tr("保存成功"), tr("游戏设置已保存！"));
    } else {
        QMessageBox::warning(this, tr("保存失败"), tr("无法保存游戏设置，请检查文件权限。"));
    }
    return ok;
}

void GameSettingsTabPage::onSaveSettingsClicked()
{
    saveGameSettings();
}