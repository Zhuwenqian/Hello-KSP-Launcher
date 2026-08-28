#include "savedetailpage.h"
#include "../widgets/toggleswitch.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QProgressDialog>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include "../iconutils.h"

// 自定义委托：控制哪些列可编辑，bool用永久开关，gender用下拉框，数值用浮点输入
class KerbalItemDelegate : public QStyledItemDelegate {
public:
    explicit KerbalItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.column() != 1) return nullptr;

        QString key = index.data(Qt::UserRole).toString();
        QString value = index.data(Qt::DisplayRole).toString();

        // 布尔类型由永久ToggleSwitch控件处理，不需要编辑器
        if (key == "badS" || key == "veteran" || key == "hero") {
            return nullptr;
        }
        // 性别
        if (key == "gender") {
            QComboBox* combo = new QComboBox(parent);
            combo->addItem("Male");
            combo->addItem("Female");
            combo->setCurrentText(value);
            return combo;
        }
        // 浮点数值：brave, dumb (0.0-1.0)
        if (key == "brave" || key == "dumb") {
            QDoubleSpinBox* spin = new QDoubleSpinBox(parent);
            spin->setRange(0.0, 1.0);
            spin->setSingleStep(0.1);
            spin->setDecimals(1);
            spin->setValue(value.toDouble());
            return spin;
        }
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);
        QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(editor);
        QString value = index.data(Qt::DisplayRole).toString();

        if (combo) {
            combo->setCurrentText(value);
            return;
        }
        if (spin) {
            spin->setValue(value.toDouble());
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);
        QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(editor);

        if (combo) {
            model->setData(index, combo->currentText());
            return;
        }
        if (spin) {
            model->setData(index, QString::number(spin->value(), 'f', 1));
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
};

SaveDetailPage::SaveDetailPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void SaveDetailPage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部栏
    QWidget* topBar = new QWidget(this);
    QHBoxLayout* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(10, 5, 15, 5);

    m_backButton = new QPushButton(IconUtils::tintedIcon(":/icons/back.svg", "#ffffff"), tr(" 返回"), topBar);
    m_backButton->setObjectName("backButton");
    m_backButton->setFixedHeight(40);
    m_backButton->setMinimumWidth(100);
    connect(m_backButton, &QPushButton::clicked, this, &SaveDetailPage::onBackClicked);

    m_homeButton = new QPushButton(IconUtils::tintedIcon(":/icons/home.svg", "#ffffff"), "", topBar);
    m_homeButton->setObjectName("backButton");
    m_homeButton->setFixedSize(40, 40);
    connect(m_homeButton, &QPushButton::clicked, this, &SaveDetailPage::onHomeClicked);

    m_titleLabel = new QLabel(tr("存档详情"), topBar);
    m_titleLabel->setObjectName("pageTitle");

    topBarLayout->addWidget(m_backButton);
    topBarLayout->addWidget(m_homeButton);
    topBarLayout->addWidget(m_titleLabel, 1);
    mainLayout->addWidget(topBar);

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // 侧边栏
    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(200);
    m_sidebar->setObjectName("sidebarWidget");
    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(0, 10, 0, 10);
    sidebarLayout->setSpacing(0);

    m_saveInfoBtn = new QPushButton(IconUtils::tintedIcon(":/icons/sliders.svg", "#ffffff"), tr("  存档信息"), m_sidebar);
    m_saveInfoBtn->setObjectName("detailNavButton");
    m_saveInfoBtn->setCheckable(true);
    m_saveInfoBtn->setChecked(true);
    m_saveInfoBtn->setMinimumHeight(40);
    connect(m_saveInfoBtn, &QPushButton::clicked, this, &SaveDetailPage::onNavButtonClicked);

    m_kerbalsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/list.svg", "#ffffff"), tr("  管理小绿人"), m_sidebar);
    m_kerbalsBtn->setObjectName("detailNavButton");
    m_kerbalsBtn->setCheckable(true);
    m_kerbalsBtn->setMinimumHeight(40);
    connect(m_kerbalsBtn, &QPushButton::clicked, this, &SaveDetailPage::onNavButtonClicked);

    m_backupsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/package.svg", "#ffffff"), tr("  备份管理"), m_sidebar);
    m_backupsBtn->setObjectName("detailNavButton");
    m_backupsBtn->setCheckable(true);
    m_backupsBtn->setMinimumHeight(40);
    connect(m_backupsBtn, &QPushButton::clicked, this, &SaveDetailPage::onNavButtonClicked);

    sidebarLayout->addWidget(m_saveInfoBtn);
    sidebarLayout->addWidget(m_kerbalsBtn);
    sidebarLayout->addWidget(m_backupsBtn);
    sidebarLayout->addStretch();

    m_contentStack = new QStackedWidget(this);
    setupSaveInfoTab();
    setupKerbalsTab();
    setupBackupsTab();

    contentLayout->addWidget(m_sidebar);
    contentLayout->addWidget(m_contentStack, 1);
    mainLayout->addLayout(contentLayout, 1);
}

void SaveDetailPage::setupSaveInfoTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);

    QLabel* note = new QLabel(tr("注：存档信息为只读显示"), tab);
    note->setStyleSheet("color: #888; font-size: 9pt;");
    layout->addWidget(note);

    m_infoTree = new QTreeWidget(tab);
    m_infoTree->setColumnCount(2);
    m_infoTree->setHeaderLabels({tr("项目"), tr("值")});
    m_infoTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_infoTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_infoTree->setAlternatingRowColors(true);
    m_infoTree->setRootIsDecorated(false);
    m_infoTree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_infoTree, 1);
    m_contentStack->addWidget(tab);
}

void SaveDetailPage::setupKerbalsTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);

    m_kerbalsStack = new QStackedWidget(tab);

    // Kerbal列表页
    QWidget* listPage = new QWidget(m_kerbalsStack);
    QVBoxLayout* listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(15, 10, 15, 15);

    QLabel* listNote = new QLabel(tr("双击小绿人可编辑其属性"), listPage);
    listNote->setStyleSheet("color: #888; font-size: 9pt;");
    listLayout->addWidget(listNote);

    m_kerbalList = new QListWidget(listPage);
    connect(m_kerbalList, &QListWidget::itemClicked, this, &SaveDetailPage::onKerbalItemClicked);
    listLayout->addWidget(m_kerbalList, 1);

    m_kerbalsStack->addWidget(listPage);

    // Kerbal详情页
    m_kerbalDetailWidget = new QWidget(m_kerbalsStack);
    QVBoxLayout* detailLayout = new QVBoxLayout(m_kerbalDetailWidget);
    detailLayout->setContentsMargins(15, 10, 15, 15);
    detailLayout->setSpacing(10);

    QWidget* detailTopBar = new QWidget(m_kerbalDetailWidget);
    QHBoxLayout* detailTopLayout = new QHBoxLayout(detailTopBar);
    detailTopLayout->setContentsMargins(0, 0, 0, 0);

    m_backToKerbalListBtn = new QPushButton(IconUtils::tintedIcon(":/icons/back.svg", "#ffffff"), tr(" 返回列表"), detailTopBar);
    m_backToKerbalListBtn->setObjectName("backButton");
    m_backToKerbalListBtn->setMinimumHeight(36);
    connect(m_backToKerbalListBtn, &QPushButton::clicked, this, &SaveDetailPage::onBackToKerbalList);
    detailTopLayout->addWidget(m_backToKerbalListBtn);
    detailTopLayout->addStretch();
    detailLayout->addWidget(detailTopBar);

    m_kerbalDetailTree = new QTreeWidget(m_kerbalDetailWidget);
    m_kerbalDetailTree->setColumnCount(2);
    m_kerbalDetailTree->setHeaderLabels({tr("属性"), tr("值")});
    m_kerbalDetailTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_kerbalDetailTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_kerbalDetailTree->setAlternatingRowColors(true);
    m_kerbalDetailTree->setRootIsDecorated(false);
    m_kerbalDetailTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    m_kerbalDetailTree->setItemDelegate(new KerbalItemDelegate(m_kerbalDetailTree));

    detailLayout->addWidget(m_kerbalDetailTree, 1);

    QWidget* btnBar = new QWidget(m_kerbalDetailWidget);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_saveKerbalsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr(" 保存修改"), btnBar);
    m_saveKerbalsBtn->setObjectName("primaryButton");
    m_saveKerbalsBtn->setMinimumHeight(36);
    m_saveKerbalsBtn->setMinimumWidth(140);
    connect(m_saveKerbalsBtn, &QPushButton::clicked, this, &SaveDetailPage::onSaveKerbalsClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_saveKerbalsBtn);
    detailLayout->addWidget(btnBar);

    m_kerbalsStack->addWidget(m_kerbalDetailWidget);
    m_kerbalsStack->setCurrentIndex(0);

    layout->addWidget(m_kerbalsStack, 1);
    m_contentStack->addWidget(tab);
}

void SaveDetailPage::setupBackupsTab()
{
    m_backupsTab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(m_backupsTab);
    layout->setContentsMargins(0, 0, 0, 0);

    // 顶部工具栏
    QWidget* toolBar = new QWidget(m_backupsTab);
    QHBoxLayout* toolBarLayout = new QHBoxLayout(toolBar);
    toolBarLayout->setContentsMargins(15, 10, 15, 10);

    m_refreshBackupsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/refresh.svg", "#ffffff"), tr(" 刷新"), toolBar);
    m_refreshBackupsBtn->setObjectName("backButton");
    m_refreshBackupsBtn->setMinimumHeight(36);
    connect(m_refreshBackupsBtn, &QPushButton::clicked, this, &SaveDetailPage::onRefreshBackupsClicked);

    m_createBackupBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr(" 创建新备份"), toolBar);
    m_createBackupBtn->setObjectName("primaryButton");
    m_createBackupBtn->setMinimumHeight(36);
    m_createBackupBtn->setMinimumWidth(150);
    connect(m_createBackupBtn, &QPushButton::clicked, this, &SaveDetailPage::onCreateBackupClicked);

    toolBarLayout->addWidget(m_refreshBackupsBtn);
    toolBarLayout->addStretch();
    toolBarLayout->addWidget(m_createBackupBtn);
    layout->addWidget(toolBar);

    // 备份列表
    m_backupList = new QListWidget(m_backupsTab);
    m_backupList->setObjectName("backupListWidget");
    m_backupList->setSpacing(2);
    layout->addWidget(m_backupList, 1);

    m_contentStack->addWidget(m_backupsTab);
}

void SaveDetailPage::setSavePath(const QString &saveFolderPath, const QString &instanceName)
{
    m_saveFolderPath = saveFolderPath;
    m_instanceName = instanceName;
    QDir dir(saveFolderPath);
    m_saveName = dir.dirName();
    m_titleLabel->setText("存档 - " + m_saveName);
    loadSaveData();
    refreshBackupList();
}

void SaveDetailPage::loadSaveData()
{
    m_saveInfo = InstanceManager::instance().loadSaveInfo(m_saveFolderPath);
    m_kerbals = InstanceManager::instance().loadKerbals(m_saveFolderPath);

    // 填充存档信息
    m_infoTree->clear();
    auto addInfoItem = [this](const QString& key, const QString& value) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_infoTree);
        item->setText(0, key);
        item->setText(1, value);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    };

    QString modeDisplay = m_saveInfo.mode;
    if (modeDisplay == "SANDBOX") modeDisplay = tr("沙盒模式");
    else if (modeDisplay == "CAREER") modeDisplay = tr("生涯模式");
    else if (modeDisplay == "SCIENCE_SANDBOX") modeDisplay = tr("科学模式");

    addInfoItem(tr("存档标题"), m_saveInfo.title);
    addInfoItem(tr("游戏版本"), m_saveInfo.version);
    addInfoItem(tr("游戏模式"), modeDisplay);
    addInfoItem(tr("种子"), m_saveInfo.seed);
    addInfoItem(tr("有模组"), m_saveInfo.modded ? tr("是") : tr("否"));
    addInfoItem(tr("游戏完整版本号"), m_saveInfo.versionFull);
    addInfoItem(tr("创建存档版本"), m_saveInfo.versionCreated);
    addInfoItem(tr("时间戳"), m_saveInfo.persistentTimestamp);
    addInfoItem(tr("环境信息"), m_saveInfo.envInfo);

    // 填充Kerbal列表
    m_kerbalList->clear();
    for (const KerbalInfo& k : m_kerbals) {
        QWidget* itemWidget = new QWidget(m_kerbalList);
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(20, 14, 20, 14);

        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(k.name, itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 11pt;");

        QString traitDisplay = k.trait;
        if (traitDisplay == "Pilot") traitDisplay = tr("飞行员");
        else if (traitDisplay == "Engineer") traitDisplay = tr("工程师");
        else if (traitDisplay == "Scientist") traitDisplay = tr("科学家");
        QString genderDisplay = (k.gender == "Male") ? tr("男") : tr("女");
        QString statusText = QString("%1 | %2 | %3").arg(genderDisplay, k.type, traitDisplay);
        if (k.veteran) statusText += tr(" | 老兵");
        if (k.hero) statusText += tr(" | 英雄");
        if (k.badS) statusText += tr(" | 坏蛋");

        QLabel* statusLabel = new QLabel(statusText, itemWidget);
        statusLabel->setStyleSheet("color: #888; font-size: 9pt;");

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(statusLabel);
        layout->addLayout(textLayout, 1);

        QListWidgetItem* item = new QListWidgetItem(m_kerbalList);
        item->setSizeHint(QSize(0, 70));
        item->setData(Qt::UserRole, k.name);
        m_kerbalList->addItem(item);
        m_kerbalList->setItemWidget(item, itemWidget);
    }

    if (m_kerbals.isEmpty()) {
        m_kerbalList->addItem(tr("（未检测到小绿人）"));
    }

    m_kerbalsStack->setCurrentIndex(0);
}

void SaveDetailPage::showKerbalDetail(const KerbalInfo &kerbal)
{
    m_currentKerbalName = kerbal.name;
    m_kerbalDetailTree->clear();

    auto addEditItem = [this](const QString& displayName, const QString& key, const QString& value, bool editable = true) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_kerbalDetailTree);
        item->setText(0, displayName);
        item->setText(1, value);
        item->setData(0, Qt::UserRole, key);
        item->setData(1, Qt::UserRole, key);
        if (editable) {
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        } else {
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        }
    };

    // For bool keys: permanently show ToggleSwitch widget
    auto addBoolItem = [this](const QString& displayName, const QString& key, bool value) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_kerbalDetailTree);
        item->setText(0, displayName);
        item->setText(1, value ? "True" : "False");
        item->setData(0, Qt::UserRole, key);
        item->setData(1, Qt::UserRole, key);
        item->setFlags(item->flags() | Qt::ItemIsEditable);

        ToggleSwitch* toggle = new ToggleSwitch(m_kerbalDetailTree);
        toggle->setChecked(value);
        connect(toggle, &ToggleSwitch::toggled, this, [item](bool checked) {
            item->setText(1, checked ? "True" : "False");
        });
        m_kerbalDetailTree->setItemWidget(item, 1, toggle);
    };

    QString genderDisplay = (kerbal.gender == "Male") ? "Male" : "Female";
    QString traitDisplay = kerbal.trait;
    // trait保留原始值，不翻译因为需要保存回SFS

    addEditItem(tr("姓名"), "name", kerbal.name, true); // 姓名可编辑
    addEditItem(tr("性别"), "gender", kerbal.gender);
    addEditItem(tr("类型"), "type", kerbal.type, false); // type不可编辑
    addEditItem(tr("职业"), "trait", kerbal.trait);
    addEditItem(tr("勇敢度"), "brave", QString::number(kerbal.brave, 'f', 1));
    addEditItem(tr("愚蠢度"), "dumb", QString::number(kerbal.dumb, 'f', 1));
    addBoolItem(tr("坏蛋"), "badS", kerbal.badS);
    addBoolItem(tr("老兵"), "veteran", kerbal.veteran);
    addBoolItem(tr("英雄"), "hero", kerbal.hero);

    m_kerbalsStack->setCurrentIndex(1);
}

bool SaveDetailPage::collectKerbalData(QList<KerbalInfo> &kerbals)
{
    // 从列表收集所有Kerbal，然后更新当前显示的Kerbal
    kerbals = m_kerbals;

    if (m_currentKerbalName.isEmpty()) return true;

    // 从详情树收集当前编辑的Kerbal数据
    for (int i = 0; i < m_kerbalDetailTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_kerbalDetailTree->topLevelItem(i);
        QString key = item->data(0, Qt::UserRole).toString();
        QString value = item->text(1);

        for (KerbalInfo& k : kerbals) {
            if (k.name == m_currentKerbalName) {
                if (key == "name") {
                    // 姓名需要特殊处理，保存时用旧名称匹配
                    k.name = value;
                } else if (key == "gender") {
                    if (value != "Male" && value != "Female") {
                        QMessageBox::warning(this, tr("输入错误"), tr("性别必须是 Male 或 Female"));
                        return false;
                    }
                    k.gender = value;
                } else if (key == "trait") {
                    k.trait = value;
                } else if (key == "brave") {
                    bool ok;
                    double v = value.toDouble(&ok);
                    if (!ok || v < 0.0 || v > 1.0) {
                        QMessageBox::warning(this, tr("输入错误"), tr("勇敢度必须是0.0-1.0之间的数值"));
                        return false;
                    }
                    k.brave = v;
                } else if (key == "dumb") {
                    bool ok;
                    double v = value.toDouble(&ok);
                    if (!ok || v < 0.0 || v > 1.0) {
                        QMessageBox::warning(this, tr("输入错误"), tr("愚蠢度必须是0.0-1.0之间的数值"));
                        return false;
                    }
                    k.dumb = v;
                } else if (key == "badS") {
                    k.badS = (value.toLower() == "true");
                } else if (key == "veteran") {
                    k.veteran = (value.toLower() == "true");
                } else if (key == "hero") {
                    k.hero = (value.toLower() == "true");
                }
                break;
            }
        }
    }
    return true;
}

void SaveDetailPage::onBackClicked()
{
    emit backClicked();
}

void SaveDetailPage::onHomeClicked()
{
    emit homeClicked();
}

void SaveDetailPage::onNavButtonClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    m_saveInfoBtn->setChecked(btn == m_saveInfoBtn);
    m_kerbalsBtn->setChecked(btn == m_kerbalsBtn);
    m_backupsBtn->setChecked(btn == m_backupsBtn);

    if (btn == m_saveInfoBtn) {
        m_contentStack->setCurrentIndex(0);
    } else if (btn == m_kerbalsBtn) {
        m_contentStack->setCurrentIndex(1);
        m_kerbalsStack->setCurrentIndex(0);
    } else if (btn == m_backupsBtn) {
        m_contentStack->setCurrentIndex(2);
        refreshBackupList();
    }
}

void SaveDetailPage::onKerbalItemClicked(QListWidgetItem *item)
{
    QString name = item->data(Qt::UserRole).toString();
    for (const KerbalInfo& k : m_kerbals) {
        if (k.name == name) {
            showKerbalDetail(k);
            break;
        }
    }
}

void SaveDetailPage::onSaveKerbalsClicked()
{
    QList<KerbalInfo> updatedKerbals;
    if (!collectKerbalData(updatedKerbals)) {
        return;
    }

    // 找到当前编辑的Kerbal的新名称
    QString newName = m_currentKerbalName;
    for (const KerbalInfo& k : updatedKerbals) {
        if (k.originalName == m_currentKerbalName) {
            newName = k.name;
            break;
        }
    }

    bool ok = InstanceManager::instance().saveKerbals(m_saveFolderPath, updatedKerbals);
    if (ok) {
        m_kerbals = updatedKerbals;
        // 更新originalName为新名称，以便下次保存时正确匹配
        for (KerbalInfo& k : m_kerbals) {
            k.originalName = k.name;
        }
        m_currentKerbalName = newName;
        QMessageBox::information(this, tr("保存成功"), tr("小绿人数据已保存！"));
        // 刷新列表显示
        loadSaveData();
        m_kerbalsStack->setCurrentIndex(1);
        // 重新显示当前Kerbal详情
        for (const KerbalInfo& k : m_kerbals) {
            if (k.name == m_currentKerbalName) {
                showKerbalDetail(k);
                break;
            }
        }
    } else {
        QMessageBox::warning(this, tr("保存失败"), tr("无法保存小绿人数据，请检查文件权限或文件格式是否损坏。"));
    }
}

void SaveDetailPage::onBackToKerbalList()
{
    // 检查是否有未保存修改（简单提示）
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("确认"),
        tr("返回列表将放弃未保存的修改，确定吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_currentKerbalName.clear();
        m_kerbalsStack->setCurrentIndex(0);
    }
}

void SaveDetailPage::refreshIcons(const QString &color)
{
    m_backButton->setIcon(IconUtils::tintedIcon(":/icons/back.svg", color));
    m_homeButton->setIcon(IconUtils::tintedIcon(":/icons/home.svg", color));
    m_saveInfoBtn->setIcon(IconUtils::tintedIcon(":/icons/sliders.svg", color));
    m_kerbalsBtn->setIcon(IconUtils::tintedIcon(":/icons/list.svg", color));
    m_backupsBtn->setIcon(IconUtils::tintedIcon(":/icons/package.svg", color));
    m_backToKerbalListBtn->setIcon(IconUtils::tintedIcon(":/icons/back.svg", color));
    m_saveKerbalsBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
    m_refreshBackupsBtn->setIcon(IconUtils::tintedIcon(":/icons/refresh.svg", color));
    m_createBackupBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
}

void SaveDetailPage::refreshBackupList()
{
    m_backupList->clear();
    QList<BackupInfo> backups = InstanceManager::instance().listBackups(m_instanceName, m_saveName);

    for (const BackupInfo& backup : backups) {
        QWidget* itemWidget = new QWidget(m_backupList);
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(20, 14, 10, 14);

        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(backup.saveName, itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 11pt;");

        QString timeStr = backup.timestamp.toString("yyyy年MM月dd日 HH:mm:ss");
        QLabel* timeLabel = new QLabel(timeStr, itemWidget);
        timeLabel->setStyleSheet("color: #888; font-size: 9pt;");
        if (!backup.note.isEmpty()) {
            nameLabel->setText(backup.saveName + tr("（") + backup.note + tr("）"));
            timeLabel->setToolTip(backup.note);
        }

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(timeLabel);
        layout->addLayout(textLayout, 1);

        // 操作按钮：恢复在最前，然后是显示、删除
        QString filePath = backup.filePath;

        QPushButton* restoreBtn = new QPushButton(IconUtils::tintedIcon(":/icons/refresh.svg", "#ffffff"), "", itemWidget);
        restoreBtn->setObjectName("iconButton");
        restoreBtn->setFixedSize(36, 36);
        restoreBtn->setToolTip(tr("从备份恢复"));
        connect(restoreBtn, &QPushButton::clicked, this, [this, filePath]() {
            onRestoreBackupClicked(filePath);
        });

        QPushButton* revealBtn = new QPushButton(IconUtils::tintedIcon(":/icons/folder-open.svg", "#ffffff"), "", itemWidget);
        revealBtn->setObjectName("iconButton");
        revealBtn->setFixedSize(36, 36);
        revealBtn->setToolTip(tr("在文件资源管理器中显示"));
        connect(revealBtn, &QPushButton::clicked, this, [this, filePath]() {
            onRevealBackupClicked(filePath);
        });

        QPushButton* deleteBtn = new QPushButton(IconUtils::tintedIcon(":/icons/trash-2.svg", "#ffffff"), "", itemWidget);
        deleteBtn->setObjectName("iconButton");
        deleteBtn->setFixedSize(36, 36);
        deleteBtn->setToolTip(tr("删除备份"));
        connect(deleteBtn, &QPushButton::clicked, this, [this, filePath]() {
            onDeleteBackupClicked(filePath);
        });

        layout->addWidget(restoreBtn);
        layout->addWidget(revealBtn);
        layout->addWidget(deleteBtn);

        QListWidgetItem* item = new QListWidgetItem(m_backupList);
        item->setSizeHint(QSize(0, 70));
        item->setData(Qt::UserRole, backup.filePath);
        m_backupList->addItem(item);
        m_backupList->setItemWidget(item, itemWidget);
    }

    if (backups.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem(tr("（暂无备份）"), m_backupList);
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setForeground(QColor("#888888"));
    }
}

void SaveDetailPage::onCreateBackupClicked()
{
    // 检查存档目录
    QDir saveDir(m_saveFolderPath);
    if (!saveDir.exists()) {
        QMessageBox::warning(this, tr("备份失败"), tr("存档文件夹不存在！"));
        return;
    }

    QFileInfoList entries = saveDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        QMessageBox::warning(this, tr("备份失败"), tr("存档文件夹为空，无法备份！"));
        return;
    }

    QProgressDialog progress(tr("正在创建备份..."), "取消", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    bool success = false;
    QString errorMsg;

    // 使用QtConcurrent在后台线程执行备份
    QFutureWatcher<bool> watcher;
    QEventLoop loop;
    int lastProgress = 0;

    connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);

    QFuture<bool> future = QtConcurrent::run([&]() {
        return InstanceManager::instance().createBackup(m_saveFolderPath, m_instanceName, m_saveName,
            QString(), [&](int p) {
                QMetaObject::invokeMethod(&progress, [&, p]() {
                    if (p > lastProgress) {
                        lastProgress = p;
                        progress.setValue(p);
                    }
                }, Qt::QueuedConnection);
            });
    });

    watcher.setFuture(future);

    // 显示进度对话框
    while (progress.isVisible() && !watcher.isFinished()) {
        loop.processEvents(QEventLoop::AllEvents, 100);
        if (progress.wasCanceled()) {
            // tar压缩过程不方便中途取消，仅关闭进度提示，等待任务自然完成
            break;
        }
    }

    if (watcher.isFinished()) {
        success = future.result();
    } else {
        // 用户关闭了进度框，等待任务完成
        watcher.waitForFinished();
        success = future.result();
    }

    progress.close();

    if (success) {
        QMessageBox::information(this, tr("备份成功"), tr("存档备份已创建！"));
        refreshBackupList();
    } else {
        QMessageBox::warning(this, tr("备份失败"), tr("创建备份时发生错误，请检查磁盘空间或文件权限。"));
    }
}

void SaveDetailPage::onRefreshBackupsClicked()
{
    refreshBackupList();
}

void SaveDetailPage::onDeleteBackupClicked(const QString &filePath)
{
    QFileInfo fi(filePath);
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除备份 '%1' 吗？\n此操作不可撤销。").arg(fi.fileName()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (InstanceManager::instance().deleteBackup(filePath)) {
            refreshBackupList();
        } else {
            QMessageBox::warning(this, tr("删除失败"), tr("无法删除备份文件，请检查文件是否被占用。"));
        }
    }
}

void SaveDetailPage::onRevealBackupClicked(const QString &filePath)
{
    if (!InstanceManager::instance().revealBackupInExplorer(filePath)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件资源管理器。"));
    }
}

void SaveDetailPage::onRestoreBackupClicked(const QString &filePath)
{
    QFileInfo fi(filePath);

    // 恢复前提示：将删除目标存档全部内容，但会自动备份当前状态
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("确认恢复"),
        tr("将从备份 '%1' 恢复存档 '%2'。\n"
           "恢复前会自动创建一份当前存档的备份（标注为“恢复前备份”），\n"
           "然后删除当前存档中的全部文件，并用备份内容替换。\n\n"
           "是否继续？").arg(fi.fileName(), m_saveName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    bool success = false;

    QProgressDialog progress(tr("正在恢复存档..."), "取消", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    QFutureWatcher<bool> watcher;
    QEventLoop loop;
    int lastProgress = 0;

    connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);

    QFuture<bool> future = QtConcurrent::run([&]() {
        return InstanceManager::instance().restoreBackup(filePath, m_saveFolderPath,
            m_instanceName, m_saveName, [&](int p) {
                QMetaObject::invokeMethod(&progress, [&, p]() {
                    if (p > lastProgress) {
                        lastProgress = p;
                        progress.setValue(p);
                    }
                }, Qt::QueuedConnection);
            });
    });

    watcher.setFuture(future);

    while (progress.isVisible() && !watcher.isFinished()) {
        loop.processEvents(QEventLoop::AllEvents, 100);
        if (progress.wasCanceled()) {
            break;
        }
    }

    watcher.waitForFinished();
    success = future.result();
    progress.close();

    if (success) {
        QMessageBox::information(this, tr("恢复成功"), tr("存档已从备份恢复！"));
    } else {
        QMessageBox::warning(this, tr("恢复失败"),
            tr("存档恢复失败，可能存在文件占用或备份损坏。\n"
               "恢复前的存档已自动备份，可在备份列表中找回。"));
    }
    refreshBackupList();
}
