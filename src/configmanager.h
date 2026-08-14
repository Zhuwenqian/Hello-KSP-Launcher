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
