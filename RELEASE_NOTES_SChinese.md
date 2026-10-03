# 发布说明
> **重要提示 / Important Note:** 本 Release 含有更新器（`updater.exe`）的更新。如果您正在使用早期的 v1.2.0 系列版本，由于其自带的老版更新器无法获取到自身的新二进制，自动更新可能无法替换本组件——届时请手动替换 `updater.exe`。
>
> This release ships an updated built-in updater (`updater.exe`). If you are still on the older v1.2.0 series, its built-in updater cannot fetch its own new binary, so the auto-update may fail to replace this component — please replace `updater.exe` manually in that case.

## v1.4.2 — 图形后端选择、PRE 临时禁用、音量拖动条与模组操作锁定（2026-10-03）

本次更新为高级页新增按实例的图形后端选择与 PhysicsRangeExtender 临时禁用开关，游戏设置音量项改为拖动条，模组操作全程锁定按钮防止并发写操作，并修复浏览大体积缓存包时的界面卡顿、更新器失败流程可能清空安装目录的问题、下载续传错位与背景图缩放掉帧。

### 启动配置

- **图形后端选择（实验性）**（`configmanager.{h,cpp}`、`advancedtabpage.{h,cpp}`、`mainwindow.cpp`）——自定义启动参数框上方新增按实例的下拉框。选项跟随检测到的游戏版本：1.8 前默认 DirectX 9（无参数），可选 OpenGL（`-force-opengl`）、DirectX 11（`-force-d3d11`）；1.8 及以上默认 DirectX 11，可选 OpenGL、DirectX 12（`-force-d3d12`）。6 个保留 `-force-*` 渲染参数在保存与启动时都会从自定义启动参数中剔除，旧配置残留也不会传给游戏。选择按实例持久化（`HKSPL.json` 的 `graphicsBackend`），不经高级页直接启动同样生效；与版本不符的选择静默回退默认渲染器。当下拉框明确选择过后端时，崩溃分析弹窗首行显示「本次启动使用的图形后端：OpenGL（-force-opengl）」，便于直接反馈给模组作者。
- **临时禁用 PhysicsRangeExtender**（`instancemanager.{h,cpp}`、`advancedtabpage.{h,cpp}`）——进程优先级下方新增开关（仅当 `GameData\PhysicsRangeExtender\Plugins\PhysicsRangeExtender.dll` 存在时显示），点击立即把 DLL 改名为 `.disabled`（无需「确认保存」），游戏退出后自动还原——含强杀路径与启动器关闭。开关反映磁盘实际状态：上次运行残留的 `.disabled` 显示为开启状态，可手动还原；改名失败（DLL 被占用）时开关回弹并弹警告。

### 游戏设置

- **音量拖动条**（`instancemanager_keymap.{h,cpp}`、`instancemanager_settings.cpp`、`gamesettingstabpage.cpp`、`dark.qss` / `light.qss`）——主音量/飞船/环境/音乐/界面/语音音量项改为拖动条（0–1，步进 0.01），右侧两位小数数字显示随拖动实时更新。slider 标记随 keymap 数据透传而非按键名硬编码；解析失败的值仍回退普通文本编辑，保存走既有路径（数值从 `Qt::UserRole` 读取，不会透过透明行底透出重影）。

### 模组管理

- **操作锁定**（`modstabpage.{h,cpp}`）——安装/升级/卸载（含导入）按钮在任一模组操作全程保持禁用，直至 `operationFinished`（成功/失败/取消均统一走该信号）才解锁。此前操作进行中时切换选中行、勾选/取消勾选、全选，或后台索引/DLL 扫描完成回填，都会重算按钮状态把已禁用的按钮重新点亮，允许并发触发第二次写操作。所有入口均已覆盖——包括版本历史安装与 `.ckan` 整合包自动批量安装（这两条路径原先完全不锁）。取消按钮保持可用，并经同一流程解锁。
- **「文件」tab 浏览大包不再卡顿**（`modstabpage.{h,cpp}`、`moduleinstaller.{h,cpp}`）——选中缓存中的 1 GB 模组包曾让界面卡住数秒。修复两层根因：详情加载不再抢先构建文件树（未切到「文件」tab 前仅显示提示；已停在「文件」tab 时切换选中仍立即重建），文件树改由后台线程构建（`QtConcurrent::run`）。新增 `findCacheZipFast()` 仅读 zip central directory（毫秒级）识别缓存包，不再把整个文件读进内存算哈希；实际安装前的完整性校验仍用完整 SHA256。子节点按需懒物化（UI 开销与展开过的目录成正比，与包内总条目数无关），目录排序移至后台线程，快速连续切换选中经代数计数取消过期扫描。

### 修复

- **更新器失败流程**（`updater/main.cpp`）——`fatal()` 现在真正终止进程。此前更新包缺失或损坏时仅记录错误后**继续执行**，清空应用目录并搬运空 stage——最坏情况启动器被清空且新版未就位。`fatal()` 同时清除 `.updating` 标记，更新失败后旧版仍可正常启动。
- **下载健壮性**（`ckan/downloader.cpp`）——同步下载补传输超时（服务器停滞不再永久挂起事件循环）；HTTP ≥ 400 状态响应不再把错误页 body 追加进 partial 文件并从错误偏移续传——改为放弃当前镜像换下一个；仅真正的传输中断才同址续传。
- **Downloader 异步处理器叠加**（`ckan/downloader.{h,cpp}`）——异步完成处理器改为构造函数中连接一次，不再每次 `downloadAsync()` 都重复 connect（此前每次调用叠加一个处理器，一次 reply 触发多次完成路径）。
- **背景缩放性能**（`mainwindow.{h,cpp}`）——窗口缩放不再每个事件都做一次平滑重缩放；拖拽期间由 `scaledContents` 即时拉伸现有位图兜底，停止缩放 150 ms 后由空闲定时器触发一次精确重缩放消除模糊。
- **小绿人搜索防抖**（`savedetailpage.{h,cpp}`）——小绿人搜索框输入防抖 250 ms，不再每敲一键就清空重建两个列表；程序化刷新仍立即填充并保留搜索词。

### 技术栈

- **框架**：Qt 6.12（Widgets、Svg、Network、Concurrent）
- **语言**：C++17
- **构建**：CMake ≥ 3.16；MinGW（Qt 自带）；Inno Setup 7 安装包
- **测试**：`test_libckan` + `test_launcher`（全部通过）
