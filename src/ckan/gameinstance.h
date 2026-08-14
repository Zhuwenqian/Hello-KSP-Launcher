#ifndef CKAN_GAMEINSTANCE_H
#define CKAN_GAMEINSTANCE_H

#include <QString>
#include <QDir>
#include <QMap>

#include "ckan_export.h"
#include "registry.h"
#include "version.h"

namespace ckan {

// 游戏实例，对应 CKAN 的 GameInstance。
// 负责管理单个 KSP 游戏目录下的 CKAN 数据目录。
class CKAN_API GameInstance
{
public:
    GameInstance() = default;
    GameInstance(const QString &gameDir, const QString &name);

    // 初始化：确保 CKAN 目录结构存在
    void setupCkanDirectories();

    // 路径
    QString gameDir() const { return m_gameDir; }
    QString ckanDir() const { return m_gameDir + QStringLiteral("/CKAN"); }
    QString downloadDir() const { return m_gameDir + QStringLiteral("/CKAN/downloads"); }
    QString historyDir() const { return m_gameDir + QStringLiteral("/CKAN/history"); }
    QString registryPath() const { return ckanDir() + QStringLiteral("/registry.json"); }
    QString compatibleVersionsPath() const { return ckanDir() + QStringLiteral("/compatible_ksp_versions.json"); }

    // 相对路径转换
    QString toRelativeGameDir(const QString &abs) const;
    QString toAbsoluteGameDir(const QString &rel) const;

    // 检测已安装的 KSP 版本（优先 buildID，其次 readme）
    GameVersion detectVersion() const;

    // 扫描 GameData 下所有 .dll（排除 KSP 官方目录），推导手动安装模组的标识符。
    // 返回 identifier -> 相对 GameDir 路径，对应官方 ScanUnmanagedFiles/DllPathToIdentifier。
    QMap<QString, QString> scanUnmanagedDlls() const;

    // 注册表读写（自动加载/保存 registry.json）
    Registry *registry();
    const Registry *registry() const { return &m_registry; }
    void loadRegistry();
    void saveRegistry() const;

    // 报告是否有效（存在游戏文件）
    bool isValid() const;

    // 游戏目录（默认下载缓存目录：启动器 downloads，区别于 CKAN/downloads）
    // 外部可覆盖
    void setCustomDownloadDir(const QString &dir) { m_customDownloadDir = dir; }
    QString customDownloadDir() const { return m_customDownloadDir; }

private:
    QString m_gameDir;
    QString m_name;
    QString m_customDownloadDir;
    Registry m_registry;
    bool m_registryLoaded = false;
};

} // namespace ckan

#endif // CKAN_GAMEINSTANCE_H