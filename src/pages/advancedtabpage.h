#ifndef ADVANCEDTABPAGE_H
#define ADVANCEDTABPAGE_H

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;

// 实例详情页「高级」二级 tab（即该实例的启动配置 Profile）：
// 自定义启动参数 + 内存上限(MB) + 进程优先级。
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
    QLineEdit*   m_launchArgsEdit = nullptr;
    QSpinBox*    m_launchMemorySpin = nullptr;     // 内存上限 MB，0=不限制
    QComboBox*   m_launchPriorityCombo = nullptr;  // 0=低(不处理) 1=高
    QPushButton* m_saveLaunchArgsBtn = nullptr;
    QString m_instanceId;
};

#endif // ADVANCEDTABPAGE_H