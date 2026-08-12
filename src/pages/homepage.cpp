#include "homepage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include "../instancemanager.h"

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void HomePage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(20);

    QLabel* welcomeLabel = new QLabel("Hello KSP Launcher", this);
    welcomeLabel->setStyleSheet("font-size: 24pt; font-weight: bold;");
    mainLayout->addWidget(welcomeLabel);

    QLabel* subtitleLabel = new QLabel("选择实例并点击启动游戏开始", this);
    subtitleLabel->setStyleSheet("font-size: 11pt; color: #888;");
    mainLayout->addWidget(subtitleLabel);

    mainLayout->addStretch(1);

    // Instance info frame
    m_infoFrame = new QFrame(this);
    m_infoFrame->setObjectName("homeInfoFrame");
    m_infoFrame->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* infoLayout = new QVBoxLayout(m_infoFrame);
    infoLayout->setContentsMargins(25, 25, 25, 25);
    infoLayout->setSpacing(12);

    m_instanceNameLabel = new QLabel("未选择实例", m_infoFrame);
    m_instanceNameLabel->setStyleSheet("font-size: 18pt; font-weight: bold;");

    m_instancePathLabel = new QLabel("", m_infoFrame);
    m_instancePathLabel->setStyleSheet("color: #888; font-size: 10pt;");
    m_instancePathLabel->setWordWrap(true);

    m_statusLabel = new QLabel("", m_infoFrame);
    m_statusLabel->setStyleSheet("font-size: 11pt;");

    infoLayout->addWidget(m_instanceNameLabel);
    infoLayout->addWidget(m_instancePathLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(m_infoFrame, 2);
    mainLayout->addStretch(2);
}

void HomePage::refreshCurrentInstance()
{
    KSPInstance inst = ConfigManager::instance().currentInstance();
    if (inst.id.isEmpty()) {
        m_instanceNameLabel->setText("未选择实例");
        m_instancePathLabel->setText("请在左侧「实例列表」中添加或选择一个实例");
        m_statusLabel->setText("");
        m_infoFrame->setStyleSheet("");
        return;
    }

    m_instanceNameLabel->setText(inst.name);
    m_instancePathLabel->setText(inst.path);

    bool valid = InstanceManager::instance().isValidKSPPath(inst.path);
    QList<DLCDetection> dlcs = InstanceManager::instance().detectDLCs(inst.path);
    int installedDlcs = 0;
    for (const DLCDetection& d : dlcs) {
        if (d.installed) installedDlcs++;
    }
    QStringList mods = InstanceManager::instance().listMods(inst.path);

    QString statusText;
    if (valid) {
        statusText = QString("路径有效 | DLC: %1/%2 | 模组: %3")
                         .arg(installedDlcs).arg(dlcs.size()).arg(mods.size());
        m_infoFrame->setStyleSheet("#homeInfoFrame { background-color: rgba(0,120,215,0.08); border-radius: 8px; }");
    } else {
        statusText = "警告：路径无效，请检查实例配置";
        m_infoFrame->setStyleSheet("#homeInfoFrame { background-color: rgba(215,60,0,0.08); border-radius: 8px; }");
    }
    m_statusLabel->setText(statusText);
}
