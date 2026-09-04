#ifndef UPDATEFLOW_H
#define UPDATEFLOW_H

class QWidget;

// 更新流程的弹窗编排层（仅主程序使用，含 GUI）。
// 把 UpdaterManager 的查询 / 下载 / 应用串成一致的用户交互：
// 发现新版本 → 弹窗询问 → 进度条下载 → 启动更新器并退出主程序。
namespace updateflow {

// 启动时自动检查（静默)：失败不弹窗；有新版时才弹提示并询问是否更新。
// 是否检查由设置 autoCheckUpdate 控制。
void checkSilent();

// 设置页手动检查：无论有无更新都给用户明确反馈。
void checkManual(QWidget *parent);

}

#endif // UPDATEFLOW_H