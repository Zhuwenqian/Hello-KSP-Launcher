#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QMap>

struct KSPInstance {
    QString id;
    QString name;
    QString path;
    QString exePath;
    QString launchArgs;
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
