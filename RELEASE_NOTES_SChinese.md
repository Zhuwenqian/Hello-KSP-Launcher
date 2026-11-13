# 发布说明

> **长期支持（LTS）公告：** Hello KSP Launcher 的功能已进入稳定、完整阶段。由于作者精力有限，**此项目将很长一段时间不再更新**。v1.4.3 为**长期支持版**，也是可预见未来内的最后一个计划版本，请保留本安装包。问题反馈仍然欢迎，但修复可能间隔很久。

## v1.4.3 — 长期支持版：更聪明的崩溃诊断、崩溃弹窗始终提示与图标换装（2026-11-13）

本次更新围绕崩溃诊断展开：启动器现在能识别游戏加载资源时的原生内存溢出，无法判断原因时也始终弹出崩溃报告，并修复了日志分析中的无限递归——此前遇到特定崩溃日志时启动器自己会栈溢出。此外还更新了若干操作图标，并整理了实例管理的内部代码结构。

### 崩溃诊断

- **崩溃报告始终弹窗**（`playerloganalyzer.{h,cpp}`、`mainwindow.{h,cpp}`、`pages/settingspage.cpp`）——此前只有游戏以非零退出码结束才弹分析窗口。现在只要退出码非零**或**日志含崩溃痕迹就会弹窗；即便分析不出明确原因也会弹出，标题为「游戏异常退出（未识别出明确原因）」，正文附退出码并展示**日志末尾 40 行**作为上下文。只有退出码为 0 且日志无崩溃痕迹时才静默返回。`Player.log` 缺失时同样给出提示与可能原因。原先单独的「游戏进程发生错误」弹窗已删除（启动失败仍由启动流程提示），「打开日志文件 / 保存日志」仅在日志确实存在时提供。
- **加载阶段的原生内存溢出识别**（`playerloganalyzer.{h,cpp}`、`mainwindow.cpp`、`pages/settingspage.cpp`）——新增崩溃类型 `NativeLoadOutOfMemory`，用于识别这类日志：日志在一条 `Load ...` 之后紧接着 Unity 原生崩溃转储（`Crash!!!` 后跟 `OUTPUTTING STACK TRACE` 退出），且之前没有托管异常——贴图/模型的解压与上传走引擎原生代码，不经 Mono 栈。弹窗会显示崩溃前的最后一条日志（如 `Load MBM texture: KFC/parts/...`），说明原生路径的成因，并分别给出「内存不足」与「模组资源损坏」两类建议。三项判定条件任一不满足时，回落到既有的内存溢出 / `Caught fatal signal` 判定。
- **修复日志分析的无限递归**（`playerloganalyzer.cpp`）——`analyzePlayerLog` 与 `extractPlayerLogContext` 相互递归调用，日志只要含 `Caught fatal signal` 或内存溢出标记就会栈溢出——恰好在最需要崩溃弹窗时把启动器自己搞崩。上下文提取改为经由接收已判定类型的内部辅助函数，公开接口签名不变。

### 界面

- **操作图标换装**（`gen_icons.py`、`resources.qrc`、`pages/instancedetailpage.cpp`、`pages/modstabpage.cpp`、`pages/savedetailpage.cpp`）——新增并注册 `export.svg`（右弯分享箭头）用于「导出整合包」，`kerbal.svg`（小绿人轮廓）用于「管理小绿人」；「导入整合包」「导入模组」改用下载图标，「存档信息」改用列表图标，兼容版本按钮仅保留文字。

### 内部重构

- **按域拆分 `InstanceManager`**（`src/game/`）——原先约 2300 行、包揽进程/存档/小绿人/飞船/备份/整合包/键位/游戏设置/DLC 的上帝类，拆分为 `GameProcessManager`、`GameSettingsManager`（含 `settingskeymap`）、`SaveManager`、`ShipManager`、`BackupManager`、`ModpackExporter`，并用共享的 `trash` 工具替换了两份重复的 Windows 回收站实现。行为零改动；删除了无人调用的 `listMods`。

### 技术栈

- **框架**：Qt 6.12（Widgets、Svg、Network、Concurrent）
- **语言**：C++17
- **构建**：CMake ≥ 3.16；MinGW（Qt 自带）；Inno Setup 7 安装包
- **测试**：`test_libckan` + `test_launcher`（全部通过）
