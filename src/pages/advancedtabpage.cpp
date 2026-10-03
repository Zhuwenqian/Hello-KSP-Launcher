// 实例详情页 - 高级（启动配置）tab
#include "advancedtabpage.h"
#include "../configmanager.h"
#include "../iconutils.h"
#include "../game/gameprocessmanager.h"
#include "../ckan/gameinstance.h"
#include "../widgets/toggleswitch.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>

AdvancedTabPage::AdvancedTabPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("advancedTabPage");
    setAttribute(Qt::WA_StyledBackground, true); // 普通 QWidget 需此属性才按 QSS 绘制半透明背板
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

    QLabel* descLabel = new QLabel(tr("在这里配置该实例的启动方式：图形后端、附加启动参数、内存上限与进程优先级。"), panel);
    descLabel->setStyleSheet("color: #888; font-size: 9pt;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // 图形后端（按 KSP 版本提供可选项，对应参数由启动器自动附加）
    layout->addWidget(new QLabel(tr("图形后端（实验性）"), panel));
    m_graphicsBackendCombo = new QComboBox(panel);
    m_graphicsBackendCombo->setMinimumHeight(36);
    layout->addWidget(m_graphicsBackendCombo);
    QLabel* backendHint = new QLabel(tr("决定游戏使用的渲染 API；对应启动参数由启动器自动附加，无需在自定义启动参数中填写。"), panel);
    backendHint->setStyleSheet("color: #888; font-size: 9pt;");
    backendHint->setWordWrap(true);
    layout->addWidget(backendHint);

    // 启动参数
    layout->addWidget(new QLabel(tr("自定义启动参数"), panel));
    m_launchArgsEdit = new QLineEdit(panel);
    m_launchArgsEdit->setPlaceholderText(tr("输入启动参数，多个参数用空格分隔，例如：-popupwindow -screen-fullscreen 0"));
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

    // 临时禁用 PhysicsRangeExtender：仅当实例安装了该插件（dll 或其 .disabled 残留态存在）才显示，
    // 显隐与开关状态在 loadLaunchArgs 中按实例刷新
    m_disablePreRow = new QWidget(panel);
    QHBoxLayout* preLayout = new QHBoxLayout(m_disablePreRow);
    preLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* preLabel = new QLabel(tr("临时禁用 PhysicsRangeExtender"), m_disablePreRow);
    m_disablePreToggle = new ToggleSwitch(m_disablePreRow);
    m_disablePreToggle->setToolTip(tr("开启后立即把插件 dll 重命名为 .disabled 使其在游戏运行期间失效，游戏退出后自动还原"));
    preLayout->addWidget(preLabel);
    preLayout->addStretch();
    preLayout->addWidget(m_disablePreToggle);
    m_disablePreRow->setVisible(false);
    layout->addWidget(m_disablePreRow);
    m_disablePreHint = new QLabel(tr("开启后该插件在游戏运行期间失效（dll 重命名为 .disabled），游戏退出后由启动器自动还原。"), panel);
    m_disablePreHint->setStyleSheet("color: #888; font-size: 9pt;");
    m_disablePreHint->setWordWrap(true);
    m_disablePreHint->setVisible(false);
    layout->addWidget(m_disablePreHint);
    connect(m_disablePreToggle, &ToggleSwitch::toggled,
            this, &AdvancedTabPage::onDisablePreToggled);
    // 游戏退出后启动器自动还原了 dll，延迟一拍按磁盘实际状态同步开关显示
    connect(&GameProcessManager::instance(), &GameProcessManager::gameFinished, this, [this]() {
        if (m_preDllPath.isEmpty()) return;
        QTimer::singleShot(0, this, [this]() {
            const bool disabled = QFileInfo::exists(m_preDllPath + QStringLiteral(".disabled"));
            if (m_disablePreToggle->isChecked() != disabled) {
                m_preSyncing = true;
                m_disablePreToggle->setChecked(disabled);
                m_preSyncing = false;
            }
        });
    });

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

    // 按检测到的 KSP 版本重建图形后端可选项（1.8 前默认 DX9；1.8+ 默认 DX11 且多 DX12），
    // 再回填已保存的选择；版本不符的历史值（findData 找不到）回退默认项。
    const ckan::GameVersion ver = ckan::GameInstance::detectVersionFromDir(inst.path);
    rebuildBackendOptions(ConfigManager::isDx11Era(ver));
    const int bidx = m_graphicsBackendCombo->findData(inst.graphicsBackend);
    m_graphicsBackendCombo->setCurrentIndex(bidx >= 0 ? bidx : 0);

    // 展示即剔除保留的图形后端参数（旧配置/手改配置可能残留），实际落盘以保存为准
    m_launchArgsEdit->setText(ConfigManager::stripReservedGraphicsArgs(inst.launchArgs));
    m_launchMemorySpin->setValue(inst.launchMemoryMB);
    const int idx = m_launchPriorityCombo->findData(inst.launchHighPriority ? 1 : 0);
    m_launchPriorityCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    // 临时禁用 PhysicsRangeExtender：检测「GameData/PhysicsRangeExtender/Plugins/PhysicsRangeExtender.dll」，
    // 存在才显示该项；若只剩 .disabled 残留态（上次禁用后未还原）也显示并保持开启，便于手动还原。
    m_preDllPath = inst.path + QStringLiteral("/GameData/PhysicsRangeExtender/Plugins/PhysicsRangeExtender.dll");
    const bool dllExists = QFileInfo::exists(m_preDllPath);
    const bool disabledExists = QFileInfo::exists(m_preDllPath + QStringLiteral(".disabled"));
    const bool showPre = dllExists || disabledExists;
    m_disablePreRow->setVisible(showPre);
    m_disablePreHint->setVisible(showPre);
    m_preSyncing = true;
    m_disablePreToggle->setChecked(disabledExists);
    m_preSyncing = false;
}

void AdvancedTabPage::rebuildBackendOptions(bool dx11Era)
{
    m_graphicsBackendCombo->clear();
    if (dx11Era) {
        m_graphicsBackendCombo->addItem(tr("DirectX 11（默认）"), QString());
        m_graphicsBackendCombo->addItem(QStringLiteral("OpenGL"), QStringLiteral("-force-opengl"));
        m_graphicsBackendCombo->addItem(QStringLiteral("DirectX 12"), QStringLiteral("-force-d3d12"));
    } else {
        m_graphicsBackendCombo->addItem(tr("DirectX 9（默认）"), QString());
        m_graphicsBackendCombo->addItem(QStringLiteral("OpenGL"), QStringLiteral("-force-opengl"));
        m_graphicsBackendCombo->addItem(QStringLiteral("DirectX 11"), QStringLiteral("-force-d3d11"));
    }
}

void AdvancedTabPage::saveLaunchArgs()
{
    // 保留图形后端参数（-force-d3d9/10/11/12、-force-opengl）由下拉框统一管理，
    // 自定义启动参数中不允许出现，保存前剔除
    bool removedReserved = false;
    QString args = ConfigManager::stripReservedGraphicsArgs(m_launchArgsEdit->text(), &removedReserved);
    m_launchArgsEdit->setText(args);
    ConfigManager &cfg = ConfigManager::instance();
    cfg.updateInstanceLaunchArgs(m_instanceId, args);
    cfg.setInstanceGraphicsBackend(m_instanceId, m_graphicsBackendCombo->currentData().toString());
    cfg.setInstanceLaunchMemoryMB(m_instanceId, m_launchMemorySpin->value());
    const bool high = m_launchPriorityCombo->currentData().toInt() == 1;
    cfg.setInstanceLaunchHighPriority(m_instanceId, high);
    // 高优先级的“结束浏览器”说明仅在保存时提示一次；剔除保留参数时附带说明
    QString msg;
    if (high)
        msg = tr("已保存。高优先级将在启动时结束 Edge/Chrome/Firefox 的所有进程，并提升游戏进程优先级。");
    else if (removedReserved)
        msg = tr("已保存。启动参数中的图形后端参数（-force-d3d9/10/11/12、-force-opengl、-force-vulkan）已移除，请改用图形后端下拉框。");
    else
        msg = tr("启动配置已保存");
    QMessageBox::information(this, tr("提示"), msg);
}

void AdvancedTabPage::refreshIcons(const QString &color)
{
    m_saveLaunchArgsBtn->setIcon(IconUtils::tintedIcon(":/icons/save.svg", color));
}

void AdvancedTabPage::onDisablePreToggled(bool checked)
{
    if (m_preSyncing || m_preDllPath.isEmpty()) return;
    GameProcessManager &im = GameProcessManager::instance();
    const bool ok = checked ? im.disablePluginTemporarily(m_preDllPath)
                            : im.restoreTempDisabledPlugin(m_preDllPath);
    // 以磁盘实际状态为准：改名失败（如 dll 被占用）时回弹开关并提示
    const bool disabledNow = QFileInfo::exists(m_preDllPath + QStringLiteral(".disabled"));
    if (ok && disabledNow == checked) return;
    m_preSyncing = true;
    m_disablePreToggle->setChecked(!checked);
    m_preSyncing = false;
    QMessageBox::warning(this, tr("提示"),
        checked
            ? tr("无法禁用 PhysicsRangeExtender：文件可能被其他程序占用，请稍后重试。")
            : tr("无法还原 PhysicsRangeExtender：文件可能被其他程序占用，请稍后重试。"));
}