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

    QList<KSPInstance> instances() const;
    void addInstance(const KSPInstance& inst);
    void removeInstance(const QString& id);
    void renameInstance(const QString& id, const QString& newName);
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
