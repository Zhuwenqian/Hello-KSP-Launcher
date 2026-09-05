// 实例详情页 - DLC tab
#include "dlctabpage.h"
#include "../instancemanager.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>

DlcTabPage::DlcTabPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("dlcTabPage");
    setAttribute(Qt::WA_StyledBackground, true); // 普通 QWidget 需此属性才按 QSS 绘制半透明背板
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 10, 15, 15);

    m_dlcList = new QListWidget(this);
    layout->addWidget(m_dlcList);
}

void DlcTabPage::loadDLCs(const QString &gamePath)
{
    m_dlcList->clear();
    QList<DLCDetection> dlcs = InstanceManager::instance().detectDLCs(gamePath);

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