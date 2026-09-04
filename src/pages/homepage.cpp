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

    // 主标题已移除：应用标题栏已显示应用名

    mainLayout->addStretch(1);
}

void HomePage::refreshCurrentInstance()
{
    // 实例信息已移除此处不再显示，保留 API 兼容外部调用
}
