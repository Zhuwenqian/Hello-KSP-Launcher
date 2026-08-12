#include "instancelistpage.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QIcon>
#include <QHBoxLayout>
#include "../instancemanager.h"

InstanceListPage::InstanceListPage(QWidget *parent)
    : QWidget(parent), m_refreshPending(false)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top bar with back button
    QWidget* topBar = new QWidget(this);
    topBar->setObjectName("pageTopBar");
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 10, 15, 10);

    m_backButton = new QPushButton(QIcon(":/icons/arrow-left.svg"), " 返回", topBar);
    m_backButton->setObjectName("backButton");
    connect(m_backButton, &QPushButton::clicked, this, &InstanceListPage::onBackClicked);

    m_titleLabel = new QLabel("实例列表", topBar);
    m_titleLabel->setObjectName("pageTitle");

    topLayout->addWidget(m_backButton);
    topLayout->addSpacing(10);
    topLayout->addWidget(m_titleLabel);
    topLayout->addStretch();

    mainLayout->addWidget(topBar);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_listContainer = new QWidget(m_scrollArea);
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(15, 5, 15, 10);
    m_listLayout->setSpacing(0);
    m_listLayout->addStretch();

    m_addButton = new QPushButton(QIcon(":/icons/add.svg"), " 添加实例", m_listContainer);
    m_addButton->setObjectName("addInstanceButton");
    m_addButton->setMinimumHeight(50);
    m_addButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_addButton, &QPushButton::clicked, this, &InstanceListPage::onAddInstanceClicked);

    m_listLayout->insertWidget(0, m_addButton);
    m_scrollArea->setWidget(m_listContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    m_contextMenu = new QMenu(this);
    QAction* renameAction = m_contextMenu->addAction("重命名实例");
    QAction* deleteAction = m_contextMenu->addAction("删除实例");
    connect(renameAction, &QAction::triggered, this, &InstanceListPage::onRenameInstance);
    connect(deleteAction, &QAction::triggered, this, &InstanceListPage::onDeleteInstance);

    connect(&ConfigManager::instance(), &ConfigManager::instancesChanged, this, &InstanceListPage::refresh);
    connect(&ConfigManager::instance(), &ConfigManager::currentInstanceChanged, this, &InstanceListPage::refresh);

    doRefresh();
}

void InstanceListPage::refresh()
{
    if (!m_refreshPending) {
        m_refreshPending = true;
        QTimer::singleShot(0, this, &InstanceListPage::doRefresh);
    }
}

void InstanceListPage::doRefresh()
{
    m_refreshPending = false;
    m_selectedInstanceId = ConfigManager::instance().currentInstance().id;

    for (InstanceItemWidget* item : m_items) {
        item->deleteLater();
    }
    m_items.clear();

    QList<KSPInstance> instances = ConfigManager::instance().instances();
    for (int i = instances.size() - 1; i >= 0; --i) {
        InstanceItemWidget* item = new InstanceItemWidget(instances[i], instances[i].id == m_selectedInstanceId, m_listContainer);
        connect(item, &InstanceItemWidget::clicked, this, &InstanceListPage::onInstanceClicked);
        connect(item, &InstanceItemWidget::checkboxToggled, this, &InstanceListPage::onInstanceCheckboxToggled);
        connect(item, &InstanceItemWidget::menuRequested, this, &InstanceListPage::onInstanceMenuRequested);
        m_listLayout->insertWidget(1, item);
        m_items.append(item);
    }
}

void InstanceListPage::onAddInstanceClicked()
{
    emit addInstanceRequested();
}

void InstanceListPage::onBackClicked()
{
    emit backClicked();
}

void InstanceListPage::onInstanceClicked(const QString &id)
{
    KSPInstance inst = ConfigManager::instance().getInstance(id);
    if (!inst.id.isEmpty()) {
        // Update selection UI immediately without waiting for refresh
        for (InstanceItemWidget* item : m_items) {
            item->setSelected(item->instance().id == id);
        }
        emit instanceEntered(id);
        // Update current instance after entering detail page
        ConfigManager::instance().setCurrentInstance(id);
    }
}

void InstanceListPage::onInstanceCheckboxToggled(const QString &id, bool checked)
{
    if (checked) {
        for (InstanceItemWidget* item : m_items) {
            if (item->instance().id != id && item->isSelected()) {
                item->setSelected(false);
            }
        }
        ConfigManager::instance().setCurrentInstance(id);
        emit instanceSelected(id);
    }
}

void InstanceListPage::onInstanceMenuRequested(const QString &id, QPoint pos)
{
    m_menuInstanceId = id;
    m_contextMenu->exec(pos);
}

void InstanceListPage::onRenameInstance()
{
    KSPInstance inst = ConfigManager::instance().getInstance(m_menuInstanceId);
    if (inst.id.isEmpty()) return;

    bool ok;
    QString newName = QInputDialog::getText(this, "重命名实例", "请输入新的实例名称：",
                                             QLineEdit::Normal, inst.name, &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        ConfigManager::instance().renameInstance(m_menuInstanceId, newName.trimmed());
    }
}

void InstanceListPage::onDeleteInstance()
{
    KSPInstance inst = ConfigManager::instance().getInstance(m_menuInstanceId);
    if (inst.id.isEmpty()) return;

    QMessageBox::StandardButton ret = QMessageBox::question(this, "删除实例",
        QString("确定要从启动器列表中移除实例 \"%1\" 吗？\n游戏文件不会被删除。").arg(inst.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        ConfigManager::instance().removeInstance(m_menuInstanceId);
        emit currentInstanceChanged();
    }
}
