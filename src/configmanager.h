#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QMap>
#include <QVector>

#include "ckan/repository.h"
#include "ckan/version.h"

struct KSPInstance {
    QString id;
    QString name;
    QString path;
    QString exePath;
    QString launchArgs;
    // 用户勾选的兼容 KSP 版本线（如 {"1.9","1.10","1.11","1.12"}），按实例记忆。
    // 空列表表示未勾选任何版本（兼容判定仅以当前实例实际版本为准）。
    // compatVersionsSet==false（新实例/旧配置缺字段）表示未显式配置，
    // 由 ConfigManager 按检测到的游戏版本动态推导默认勾选。
    QStringList compatibleVersions;
    bool compatVersionsSet = false; // 用户是否显式配置过（含显式清空）
};

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    static ConfigManager& instance();

    bool load();
    bool save();

    QString language() const;
    void setLanguage(const QString& lang);

    enum LaunchBehavior {
        KeepOpen,
        Minimize,
        Close
    };
    LaunchBehavior launchBehavior() const;
    void setLaunchBehavior(LaunchBehavior behavior);

    QString theme() const;
    void setTheme(const QString& theme);

    // 背景图设置
    // - 空字符串 / "default": 使用资源中默认背景
    // - 其他: 用户背景文件的绝对路径(由 BackgroundManager 复制到启动器目录下)
    QString backgroundPath() const;
    void setBackgroundPath(const QString& path);

    // 模组列表是否显示兼容性不满足的模组（默认隐藏）
    bool showIncompatibleMods() const;
    void setShowIncompatibleMods(bool show);

    // 某实例勾选的兼容 KSP 版本线。
    // 已显式配置（含空=未勾选任何版本）时返回持久化值；
    // 未配置（新实例/旧配置缺字段）时按 detectedVersion 动态推导默认勾选。
    QStringList compatibleVersions(const QString &instanceId,
                                   const ckan::GameVersion &detectedVersion) const;
    void setCompatibleVersions(const QString &instanceId, const QStringList &versionLines);
    // 按检测到的游戏版本推导默认勾选：
    // 版本位于 [1.9, 1.12] 时勾选「检测版本线 ~ 1.9」全部版本线（如 1.11.x → 1.11/1.10/1.9）；
    // 低于 1.9（或高于 1.12）时仅勾选检测版本所在版本线；检测失败回退静态 1.9~1.12。
    static QStringList defaultCompatibleVersions(const ckan::GameVersion &detectedVersion);

    // 安装时是否显示级联建议模组的勾选弹窗（默认开启）
    bool installSuggests() const;
    void setInstallSuggests(bool enable);

    // 下载/安装前是否做磁盘空间预检（默认开启）；不足时弹窗，用户可选择忽略继续或取消
    bool diskSpaceCheck() const;
    void setDiskSpaceCheck(bool enable);

    // ---- 模组管理设置 ----
    // 下载源偏好：官方优先（默认）或镜像优先
    enum DownloadSource { OfficialFirst, MirrorFirst };
    // 仓库索引缓存有效期（秒），默认 6 小时
    int indexRefreshIntervalSecs() const;
    void setIndexRefreshIntervalSecs(int secs);
    // 索引下载源
    DownloadSource indexDownloadSource() const;
    void setIndexDownloadSource(DownloadSource source);
    // 模组下载源
    DownloadSource moduleDownloadSource() const;
    void setModuleDownloadSource(DownloadSource source);
    // 下载缓存文件夹；空字符串表示使用默认位置（exe目录/ckan_cache/downloads）
    QString downloadCacheDir() const;
    void setDownloadCacheDir(const QString& dir);
    // 模组下载并发数（1~8），默认 3
    int downloadConcurrency() const;
    void setDownloadConcurrency(int count);
    // 单链接下载限速（字节/秒，0=不限速，默认 0）。负数按 0（不限速）处理。
    // 持久化到 HKSPL.json 的 "downloadRateLimitBytesPerSecond"。
    qint64 downloadRateLimitBytesPerSecond() const;
    void setDownloadRateLimitBytesPerSecond(qint64 bps);

    // 仓库列表（多仓库）：数组顺序即优先级（首位优先级最高）。
    // 默认仅含 KSP-CKAN 官方仓库；可自由增删。
    QVector<ckan::Repository> repositories() const;
    void setRepositories(const QVector<ckan::Repository> &repos);

    QList<KSPInstance> instances() const;
    void addInstance(const KSPInstance& inst);
    void removeInstance(const QString& id);
    void renameInstance(const QString& id, const QString& newName);
    void updateInstanceLaunchArgs(const QString& id, const QString& args);
    KSPInstance currentInstance() const;
    void setCurrentInstance(const QString& id);
    KSPInstance getInstance(const QString& id) const;

signals:
    void configChanged();
    void instancesChanged();
    void currentInstanceChanged();

private:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString getConfigPath() const;
    void loadDefaults();

    QJsonObject m_config;
    QList<KSPInstance> m_instances;
    QString m_currentInstanceId;
};

#endif // CONFIGMANAGER_H
