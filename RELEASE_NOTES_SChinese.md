# 发布说明

## v1.4.1 — 部件计数、预制件与小绿人管理（2026-10-02）

本次更新为飞船管理加入部件计数、搜索与玩家自制载具缩略图，为存档新增「预制件」页签，引入完整的小绿人管理（分类页签、搜索、删除/重命名、滑块），备份支持日期筛选，修复重复的更新弹窗，并改用 Inno Setup 安装包。

### 飞船管理

- **部件计数**（`instancemanager_ships.cpp`）——`loadCraftInfo` 通过统计 `.craft` 顶层 `PART { … }` 块得出 `ShipInfo::partCount`（解析器忽略嵌套键）。计数显示在每行列表（「游戏版本： X · 部件数： N」）与详情页。
- **玩家自制载具缩略图**（`instancemanager_ships.cpp`）——当 `Ships/@thumbs` 无原版图时，存档模式飞船管理会查找 `<游戏根目录>/thumbs/<存档名>_<VAB|SPH>_<载具>.png`（大小写不敏感、仅 `.png`）。实例模式仅读原版缩略图（飞船多存档共享，无单一存档名）；预制件始终回退火箭图标。
- **清晰的兜底图标**（`iconutils.{h,cpp}`）——新增 `tintedPixmap` 按目标像素尺寸直接栅格化着色 SVG；火箭兜底图按缩略图框实际尺寸渲染，不再是放大发糊的 96px 位图。
- **飞船搜索**（`shiptabpage.{h,cpp}`）——列表页搜索框实时过滤 VAB/SPH/预制件，按显示名不区分大小写包含匹配；带清除按钮，「（无匹配飞船）/（无匹配预制件）」灰色居中空态，过滤时隐藏占位行，删除/导入刷新后自动重放搜索词。
- **详情返回交给父页面**——去掉详情页内嵌「返回」按钮；存档/实例详情页顶部「返回」在飞船详情可见时先回列表（`isDetailVisible()` / `goBackToList()`）。

### 预制件（仅存档）

- **第三个「预制件」页签**（`shiptabpage.{h,cpp}`）——通过 `setSubassembliesEnabled(true)` 为存档详情页的飞船管理启用；列出 `<存档根>/Subassemblies/`（与 `Ships/` 同级）。实例管理保持 VAB/SPH，因为预制件属于单个存档。
- **功能完整接入**——导入（按钮文案跟随页签；首次导入自动建目录；禁止从自身目录导入）、拖放、删除进回收站、覆盖确认，以及「（未检测到预制件）」空态，均按预制件措辞。通过新的 `listCraftFilesIn(dir)` 辅助函数列出。

### 小绿人管理（存档详情）

- **应聘者/乘员页签**（`savedetailpage.{h,cpp}`）——小绿人列表拆成 `QTabWidget`（「应聘者」/「乘员」）；仅 `type == "Crew"` 进乘员页，其余全部归入应聘者页兜底，未识别类型按原文显示。`dark.qss` / `light.qss` 两套主题补充 pane 样式。
- **搜索框**——不区分大小写姓名匹配，或 `@jobs:Pilot` 按职业过滤（英文职业名，不区分大小写部分匹配）；刷新后搜索词保留，「未检测到小绿人」与「（无匹配应聘者）/（无匹配乘员）」空态区分。
- **删除与重命名**（`instancemanager_saves.cpp`）——每行「…」菜单提供删除（从 `persistent.sfs` 的 `ROSTER` 移除 `KERBAL` 块）与重命名（改写 `name =` 行）；写盘前后均校验括号配平，立即写盘。
- **勇敢度/愚蠢度滑块**——双击浮点编辑器替换为常驻滑块（0–1.0，步进 0.1）加实时数值标签；保存时从自定义 role 收集数值。两套主题均已配色。性别下拉显示「男/女」但保存仍为 `Male`/`Female`；类型/性别显示时翻译。

### 备份日期筛选

- **时间过滤**（`savedetailpage.cpp`）——备份工具栏搜索框把查询串按数字分词：单个数字与任一时间分量（年/月/日/时/分/秒）相等即命中，多个数字从年开始按顺序匹配（如 `2026-01-01 12:30`、`10-02`）。列表缓存于内存，按键不再重读磁盘；无匹配时显示「（没有匹配的备份）」空态。

### 修复与行为变更

- **更新弹窗**（`updateflow.cpp`）——更新器信号改为整个进程只连接一次（静态标记），不再每次检查都重复 connect；此前自动检查叠加多次手动检查会堆积处理函数并连发多个对话框，且后续弹窗无父窗口。
- **默认语言**——全新配置默认 `en_US`；镜像更新源更名为「镜像源（zwqbook.cn）」（`configmanager.cpp`、`settingspage.cpp`）。

### 打包与构建

- **Inno Setup 安装包**（`installer/helloksplauncher.iss`、`make-installer.ps1`，新增）——构建按用户安装包（`{userpf}`，不强制 UAC，允许管理员覆盖），带语言选择对话框、桌面图标任务与 lzma2/max 压缩。排除运行期写入的文件（`HKSPL.json`、`backups`、`ckan_cache`、`generic`、日志），卸载器一并删除；版本号自动从 `src/appversion.h` 同步。输出：`HelloKSPLauncher-<version>-setup.exe`。
- **Qt 6.12 工具链**（`build.ps1`）——从头 configure（MinGW Makefiles，显式编译器/前缀路径），运行 `windeployqt` 保持 `dist/` DLL 与构建 Qt 同步，并补拷 `libckan.dll` 所需的 `Qt6Concurrent.dll`。
- 其他：版本号升至 1.4.1；`.gitignore` 新增 `.vscode/` 与 `*-setup.exe`；`_gen_icons.py` 更名 `gen_icons.py`；全部新增字符串完成 en_US/zh_CN 翻译并重建 `.qm`。

### 技术栈

- **框架**：Qt 6.12（Widgets、Svg、Network、Concurrent）
- **语言**：C++17
- **构建**：CMake ≥ 3.16；MinGW（Qt 自带）；Inno Setup 7 安装包
- **测试**：`test_libckan` + `test_launcher`（全部通过）
