#include "savedetailpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include "../iconutils.h"

// 自定义委托：控制哪些列可编辑，bool/gender用下拉框，数值用浮点输入
class KerbalItemDelegate : public QStyledItemDelegate {
public:
    explicit KerbalItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.column() != 1) return nullptr;

        QString key = index.data(Qt::UserRole).toString();
        QString value = index.data(Qt::DisplayRole).toString();

        // 布尔类型：badS, veteran, hero
        if (key == "badS" || key == "veteran" || key == "hero") {
            QComboBox* combo = new QComboBox(parent);
            combo->addItem("True");
            combo->addItem("False");
            combo->setCurrentText(value);
            return combo;
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

    m_backButton = new QPushButton(IconUtils::tintedIcon(":/icons/back.svg", "#ffffff"), " 返回", topBar);
    m_backButton->setObjectName("backButton");
    m_backButton->setFixedHeight(40);
    m_backButton->setMinimumWidth(100);
    connect(m_backButton, &QPushButton::clicked, this, &SaveDetailPage::onBackClicked);

    m_homeButton = new QPushButton(IconUtils::tintedIcon(":/icons/home.svg", "#ffffff"), "", topBar);
    m_homeButton->setObjectName("backButton");
    m_homeButton->setFixedSize(40, 40);
    connect(m_homeButton, &QPushButton::clicked, this, &SaveDetailPage::onHomeClicked);

    m_titleLabel = new QLabel("存档详情", topBar);
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

    m_saveInfoBtn = new QPushButton(IconUtils::tintedIcon(":/icons/sliders.svg", "#ffffff"), "  存档信息", m_sidebar);
    m_saveInfoBtn->setObjectName("detailNavButton");
    m_saveInfoBtn->setCheckable(true);
    m_saveInfoBtn->setChecked(true);
    m_saveInfoBtn->setMinimumHeight(40);
    connect(m_saveInfoBtn, &QPushButton::clicked, this, &SaveDetailPage::onNavButtonClicked);

    m_kerbalsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/list.svg", "#ffffff"), "  管理Kerbals", m_sidebar);
    m_kerbalsBtn->setObjectName("detailNavButton");
    m_kerbalsBtn->setCheckable(true);
    m_kerbalsBtn->setMinimumHeight(40);
    connect(m_kerbalsBtn, &QPushButton::clicked, this, &SaveDetailPage::onNavButtonClicked);

    sidebarLayout->addWidget(m_saveInfoBtn);
    sidebarLayout->addWidget(m_kerbalsBtn);
    sidebarLayout->addStretch();

    m_contentStack = new QStackedWidget(this);
    setupSaveInfoTab();
    setupKerbalsTab();

    contentLayout->addWidget(m_sidebar);
    contentLayout->addWidget(m_contentStack, 1);
    mainLayout->addLayout(contentLayout, 1);
}

void SaveDetailPage::setupSaveInfoTab()
{
    QWidget* tab = new QWidget(m_contentStack);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(15, 10, 15, 15);

    QLabel* note = new QLabel("注：存档信息为只读显示", tab);
    note->setStyleSheet("color: #888; font-size: 9pt;");
    layout->addWidget(note);

    m_infoTree = new QTreeWidget(tab);
    m_infoTree->setColumnCount(2);
    m_infoTree->setHeaderLabels({"项目", "值"});
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

    QLabel* listNote = new QLabel("双击Kerbal可编辑其属性", listPage);
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

    m_backToKerbalListBtn = new QPushButton(IconUtils::tintedIcon(":/icons/back.svg", "#ffffff"), " 返回列表", detailTopBar);
    m_backToKerbalListBtn->setObjectName("backButton");
    m_backToKerbalListBtn->setMinimumHeight(36);
    connect(m_backToKerbalListBtn, &QPushButton::clicked, this, &SaveDetailPage::onBackToKerbalList);
    detailTopLayout->addWidget(m_backToKerbalListBtn);
    detailTopLayout->addStretch();
    detailLayout->addWidget(detailTopBar);

    m_kerbalDetailTree = new QTreeWidget(m_kerbalDetailWidget);
    m_kerbalDetailTree->setColumnCount(2);
    m_kerbalDetailTree->setHeaderLabels({"属性", "值"});
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

    m_saveKerbalsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), " 保存修改", btnBar);
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

void SaveDetailPage::setSavePath(const QString &saveFolderPath)
{
    m_saveFolderPath = saveFolderPath;
    QDir dir(saveFolderPath);
    m_titleLabel->setText("存档 - " + dir.dirName());
    loadSaveData();
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
    if (modeDisplay == "SANDBOX") modeDisplay = "沙盒模式";
    else if (modeDisplay == "CAREER") modeDisplay = "生涯模式";
    else if (modeDisplay == "SCIENCE_SANDBOX") modeDisplay = "科学模式";

    addInfoItem("存档标题", m_saveInfo.title);
    addInfoItem("游戏版本", m_saveInfo.version);
    addInfoItem("游戏模式", modeDisplay);
    addInfoItem("种子", m_saveInfo.seed);
    addInfoItem("有模组", m_saveInfo.modded ? "是" : "否");
    addInfoItem("游戏完整版本号", m_saveInfo.versionFull);
    addInfoItem("创建存档版本", m_saveInfo.versionCreated);
    addInfoItem("时间戳", m_saveInfo.persistentTimestamp);
    addInfoItem("环境信息", m_saveInfo.envInfo);

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
        if (traitDisplay == "Pilot") traitDisplay = "飞行员";
        else if (traitDisplay == "Engineer") traitDisplay = "工程师";
        else if (traitDisplay == "Scientist") traitDisplay = "科学家";
        QString genderDisplay = (k.gender == "Male") ? "男" : "女";
        QString statusText = QString("%1 | %2 | %3").arg(genderDisplay, k.type, traitDisplay);
        if (k.veteran) statusText += " | 老兵";
        if (k.hero) statusText += " | 英雄";
        if (k.badS) statusText += " | 坏蛋";

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
        m_kerbalList->addItem("（未检测到Kerbal）");
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

    QString genderDisplay = (kerbal.gender == "Male") ? "Male" : "Female";
    QString traitDisplay = kerbal.trait;
    // trait保留原始值，不翻译因为需要保存回SFS

    addEditItem("姓名", "name", kerbal.name, true); // 姓名可编辑
    addEditItem("性别", "gender", kerbal.gender);
    addEditItem("类型", "type", kerbal.type, false); // type不可编辑
    addEditItem("职业", "trait", kerbal.trait);
    addEditItem("勇敢度", "brave", QString::number(kerbal.brave, 'f', 1));
    addEditItem("愚蠢度", "dumb", QString::number(kerbal.dumb, 'f', 1));
    addEditItem("坏蛋", "badS", kerbal.badS ? "True" : "False");
    addEditItem("老兵", "veteran", kerbal.veteran ? "True" : "False");
    addEditItem("英雄", "hero", kerbal.hero ? "True" : "False");

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
                        QMessageBox::warning(this, "输入错误", "性别必须是 Male 或 Female");
                        return false;
                    }
                    k.gender = value;
                } else if (key == "trait") {
                    k.trait = value;
                } else if (key == "brave") {
                    bool ok;
                    double v = value.toDouble(&ok);
                    if (!ok || v < 0.0 || v > 1.0) {
                        QMessageBox::warning(this, "输入错误", "勇敢度必须是0.0-1.0之间的数值");
                        return false;
                    }
                    k.brave = v;
                } else if (key == "dumb") {
                    bool ok;
                    double v = value.toDouble(&ok);
                    if (!ok || v < 0.0 || v > 1.0) {
                        QMessageBox::warning(this, "输入错误", "愚蠢度必须是0.0-1.0之间的数值");
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

    if (btn == m_saveInfoBtn) {
        m_contentStack->setCurrentIndex(0);
    } else if (btn == m_kerbalsBtn) {
        m_contentStack->setCurrentIndex(1);
        m_kerbalsStack->setCurrentIndex(0);
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
        QMessageBox::information(this, "保存成功", "Kerbal数据已保存！");
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
        QMessageBox::warning(this, "保存失败", "无法保存Kerbal数据，请检查文件权限或文件格式是否损坏。");
    }
}

void SaveDetailPage::onBackToKerbalList()
{
    // 检查是否有未保存修改（简单提示）
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认",
        "返回列表将放弃未保存的修改，确定吗？",
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
    m_backToKerbalListBtn->setIcon(IconUtils::tintedIcon(":/icons/back.svg", color));
    m_saveKerbalsBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
}
