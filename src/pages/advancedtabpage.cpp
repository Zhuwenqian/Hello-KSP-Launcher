// 实例详情页 - 高级（启动配置）tab
#include "advancedtabpage.h"
#include "../configmanager.h"
#include "../iconutils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>

AdvancedTabPage::AdvancedTabPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("advancedTabPage");
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(15, 10, 15, 15);

    QFrame* panel = new QFrame(this);
    panel->setObjectName("pagePanel");
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);

    QLabel* titleLabel = new QLabel(tr("启动配置"), panel);
    titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold;");
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel(tr("在这里配置该实例的启动方式：附加启动参数、内存上限与进程优先级。"), panel);
    descLabel->setStyleSheet("color: #888; font-size: 9pt;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // 启动参数
    layout->addWidget(new QLabel(tr("自定义启动参数"), panel));
    m_launchArgsEdit = new QLineEdit(panel);
    m_launchArgsEdit->setPlaceholderText(tr("输入启动参数，多个参数用空格分隔，例如：-force-d3d11 -popupwindow"));
    m_launchArgsEdit->setMinimumHeight(36);
    layout->addWidget(m_launchArgsEdit);

    // 内存限制
    layout->addWidget(new QLabel(tr("内存限制（MB）"), panel));
    m_launchMemorySpin = new QSpinBox(panel);
    m_launchMemorySpin->setRange(0, 65536);
    m_launchMemorySpin->setSingleStep(512);
    m_launchMemorySpin->setSpecialValueText(tr("不限制"));
    m_launchMemorySpin->setSuffix(QStringLiteral(" MB"));
    // 只在值 >0 时显示 MB 后缀；值为 0 时显示“不限制”
    connect(m_launchMemorySpin, &QSpinBox::valueChanged, this, [this](int v) {
        m_launchMemorySpin->setSuffix(v > 0 ? QStringLiteral(" MB") : QString());
    });
    m_launchMemorySpin->setMinimumHeight(36);
    layout->addWidget(m_launchMemorySpin);
    QLabel* memHint = new QLabel(tr("此处为系统级进程内存上限，0 表示不限制。"), panel);
    memHint->setStyleSheet("color: #888; font-size: 9pt;");
    memHint->setWordWrap(true);
    layout->addWidget(memHint);

    // 进程优先级
    layout->addWidget(new QLabel(tr("进程优先级"), panel));
    m_launchPriorityCombo = new QComboBox(panel);
    m_launchPriorityCombo->addItem(tr("低"), 0);
    m_launchPriorityCombo->addItem(tr("高"), 1);
    m_launchPriorityCombo->setMinimumHeight(36);
    layout->addWidget(m_launchPriorityCombo);

    QWidget* btnBar = new QWidget(panel);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_saveLaunchArgsBtn = new QPushButton(IconUtils::tintedIcon(":/icons/save.svg", "#ffffff"), tr(" 确认保存"), btnBar);
    m_saveLaunchArgsBtn->setObjectName("primaryButton");
    m_saveLaunchArgsBtn->setMinimumHeight(36);
    m_saveLaunchArgsBtn->setMinimumWidth(140);
    connect(m_saveLaunchArgsBtn, &QPushButton::clicked, this, &AdvancedTabPage::saveLaunchArgs);

    btnLayout->addStretch();
    btnLayout->addWidget(m_saveLaunchArgsBtn);
    layout->addWidget(btnBar);
    layout->addStretch();

    outerLayout->addWidget(panel);
}

void AdvancedTabPage::loadLaunchArgs(const QString &instanceId)
{
    m_instanceId = instanceId;
    if (m_instanceId.isEmpty()) return;
    const KSPInstance inst = ConfigManager::instance().getInstance(m_instanceId);
    m_launchArgsEdit->setText(inst.launchArgs);
    m_launchMemorySpin->setValue(inst.launchMemoryMB);
    const int idx = m_launchPriorityCombo->findData(inst.launchHighPriority ? 1 : 0);
    m_launchPriorityCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void AdvancedTabPage::saveLaunchArgs()
{
    QString args = m_launchArgsEdit->text().trimmed();
    ConfigManager &cfg = ConfigManager::instance();
    cfg.updateInstanceLaunchArgs(m_instanceId, args);
    cfg.setInstanceLaunchMemoryMB(m_instanceId, m_launchMemorySpin->value());
    const bool high = m_launchPriorityCombo->currentData().toInt() == 1;
    cfg.setInstanceLaunchHighPriority(m_instanceId, high);
    // 高优先级的“结束浏览器”说明仅在保存时提示一次
    QMessageBox::information(this, tr("提示"),
        high ? tr("已保存。高优先级将在启动时结束 Edge/Chrome/Firefox 的所有进程，并提升游戏进程优先级。")
             : tr("启动配置已保存"));
}

void AdvancedTabPage::refreshIcons(const QString &color)
{
    m_saveLaunchArgsBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
}