#include "homepage.h"
#include <QVBoxLayout>

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

    QLabel* subtitleLabel = new QLabel(tr("选择实例并点击启动游戏开始"), this);
    subtitleLabel->setStyleSheet("font-size: 11pt; color: #888;");
    mainLayout->addWidget(subtitleLabel);

    mainLayout->addStretch(1);
}

void HomePage::refreshCurrentInstance()
{
    // 实例信息已移除此处不再显示，保留 API 兼容外部调用
}
