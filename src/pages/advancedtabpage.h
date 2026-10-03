#ifndef ADVANCEDTABPAGE_H
#define ADVANCEDTABPAGE_H

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class ToggleSwitch;

// 实例详情页「高级」二级 tab（即该实例的启动配置 Profile）：
// 自定义启动参数 + 内存上限(MB) + 进程优先级 + 临时禁用 PhysicsRangeExtender。
// 配置经 ConfigManager 持久化到 HKSPL.json（按实例 id）。
class AdvancedTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit AdvancedTabPage(QWidget *parent = nullptr);

    // 装载指定实例 id 的启动配置（写入各输入控件）。
    void loadLaunchArgs(const QString &instanceId);
    // 保存当前界面的启动配置（含高优先级说明提示）。
    void saveLaunchArgs();
    void refreshIcons(const QString &color);

private:
    // 按检测到的 KSP 版本重建图形后端可选项（1.8 前默认 DX9，1.8+ 默认 DX11 且多 DX12）
    void rebuildBackendOptions(bool dx11Era);
    // 临时禁用 PhysicsRangeExtender 开关：点击时立即改名/还原 dll（游戏退出后启动器会自动还原）
    void onDisablePreToggled(bool checked);

    QLineEdit*   m_launchArgsEdit = nullptr;
    QComboBox*   m_graphicsBackendCombo = nullptr; // 数据为附加参数：""=版本默认 / -force-opengl 等
    QSpinBox*    m_launchMemorySpin = nullptr;     // 内存上限 MB，0=不限制
    QComboBox*   m_launchPriorityCombo = nullptr;  // 0=低(不处理) 1=高
    QPushButton* m_saveLaunchArgsBtn = nullptr;
    QWidget*     m_disablePreRow = nullptr;        // 「临时禁用 PhysicsRangeExtender」整行（插件不存在时隐藏）
    QLabel*      m_disablePreHint = nullptr;       // 开关下方的说明文字（随行显隐）
    ToggleSwitch* m_disablePreToggle = nullptr;
    QString m_preDllPath;                          // PRE 插件 dll 完整路径（实例未含该插件时为空）
    bool m_preSyncing = false;                     // 程序性 setChecked 时屏蔽 toggled 处理
    QString m_instanceId;
};

#endif // ADVANCEDTABPAGE_H