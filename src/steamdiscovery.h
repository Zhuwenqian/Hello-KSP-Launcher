#ifndef STEAMDISCOVERY_H
#define STEAMDISCOVERY_H

#include <QStringList>

// Steam 库发现（仅 Windows）：参照 CKAN-master 的 SteamLibrary 实现。
// 通过 Windows 注册表定位 Steam 安装目录，解析 config/libraryfolders.vdf
// 枚举所有 Steam 库，在 steamapps/common 下按标准目录名 "Kerbal Space Program"
// 查找 KSP，并以 settings.cfg + GameData 校验返回有效的游戏根目录。
// 供启动器启动时自动扫描、把发现的 KSP 直接加入实例列表。
class SteamDiscovery
{
public:
    // 扫描所有 Steam 库，返回有效的 KSP 游戏根目录列表（按库发现顺序）。
    // 非 Windows 平台直接返回空。
    static QStringList discoverKSPDirs();

    // 解析 libraryfolders.vdf，返回其中声明的所有 Steam 库根目录。
    // 单独暴露以便单元测试。
    static QStringList parseLibraryFolders(const QString &vdfPath);

private:
    // 从 Windows 注册表读取 Steam 安装目录；非 Windows 或不存在时返回空。
    static QString steamInstallPath();
};

#endif // STEAMDISCOVERY_H
