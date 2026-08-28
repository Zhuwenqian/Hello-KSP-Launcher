#include "saveslistpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include "../instancemanager.h"
#include "../iconutils.h"

SavesListPage::SavesListPage(QWidget *parent)
    : QWidget(parent)
{
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
    mainLayout->addWidget(topBar);

    m_savesList = new QListWidget(this);
    m_savesList->setObjectName("savesListWidget");
    connect(m_savesList, &QListWidget::itemDoubleClicked, this, &SavesListPage::onSaveItemDoubleClicked);
    mainLayout->addWidget(m_savesList, 1);
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
}
