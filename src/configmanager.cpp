#include "configmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    loadDefaults();
    load();
}

ConfigManager::~ConfigManager()
{
    save();
}

QString ConfigManager::getConfigPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir(appDir).filePath("HKSPL.json");
}

void ConfigManager::loadDefaults()
{
    m_config["language"] = "zh_CN";
    m_config["launchBehavior"] = static_cast<int>(Minimize);
    m_config["theme"] = "dark";
    m_instances.clear();
    m_currentInstanceId.clear();
}

bool ConfigManager::load()
{
    QFile file(getConfigPath());
    if (!file.exists()) {
        return save();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    m_config = doc.object();

    m_instances.clear();
    QJsonArray instArray = m_config["instances"].toArray();
    for (const QJsonValue& val : instArray) {
        QJsonObject obj = val.toObject();
        KSPInstance inst;
        inst.id = obj["id"].toString();
        inst.name = obj["name"].toString();
        inst.path = obj["path"].toString();
        inst.exePath = obj["exePath"].toString();
        inst.launchArgs = obj["launchArgs"].toString();
        m_instances.append(inst);
    }

    m_currentInstanceId = m_config["currentInstance"].toString();

    return true;
}

bool ConfigManager::save()
{
    m_config["language"] = language();
    m_config["launchBehavior"] = static_cast<int>(launchBehavior());
    m_config["theme"] = theme();

    QJsonArray instArray;
    for (const KSPInstance& inst : m_instances) {
        QJsonObject obj;
        obj["id"] = inst.id;
        obj["name"] = inst.name;
        obj["path"] = inst.path;
        obj["exePath"] = inst.exePath;
        obj["launchArgs"] = inst.launchArgs;
        instArray.append(obj);
    }
    m_config["instances"] = instArray;
    m_config["currentInstance"] = m_currentInstanceId;

    QFile file(getConfigPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonDocument doc(m_config);
    file.write(doc.toJson());
    file.close();
    return true;
}

QString ConfigManager::language() const
{
    return m_config["language"].toString("zh_CN");
}

void ConfigManager::setLanguage(const QString &lang)
{
    if (language() != lang) {
        m_config["language"] = lang;
        save();
        emit configChanged();
    }
}

ConfigManager::LaunchBehavior ConfigManager::launchBehavior() const
{
    return static_cast<LaunchBehavior>(m_config["launchBehavior"].toInt(static_cast<int>(Minimize)));
}

void ConfigManager::setLaunchBehavior(LaunchBehavior behavior)
{
    if (launchBehavior() != behavior) {
        m_config["launchBehavior"] = static_cast<int>(behavior);
        save();
        emit configChanged();
    }
}

QString ConfigManager::theme() const
{
    return m_config["theme"].toString("dark");
}

void ConfigManager::setTheme(const QString &theme)
{
    if (this->theme() != theme) {
        m_config["theme"] = theme;
        save();
        emit configChanged();
    }
}

QList<KSPInstance> ConfigManager::instances() const
{
    return m_instances;
}

void ConfigManager::addInstance(const KSPInstance &inst)
{
    KSPInstance newInst = inst;
    if (newInst.id.isEmpty()) {
        newInst.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    m_instances.append(newInst);
    if (m_instances.size() == 1) {
        m_currentInstanceId = newInst.id;
    }
    save();
    emit instancesChanged();
    if (m_currentInstanceId == newInst.id) {
        emit currentInstanceChanged();
    }
}

void ConfigManager::removeInstance(const QString &id)
{
    for (int i = 0; i < m_instances.size(); ++i) {
        if (m_instances[i].id == id) {
            m_instances.removeAt(i);
            if (m_currentInstanceId == id) {
                m_currentInstanceId = m_instances.isEmpty() ? QString() : m_instances.first().id;
                emit currentInstanceChanged();
            }
            save();
            emit instancesChanged();
            return;
        }
    }
}

void ConfigManager::renameInstance(const QString &id, const QString &newName)
{
    for (int i = 0; i < m_instances.size(); ++i) {
        if (m_instances[i].id == id) {
            m_instances[i].name = newName;
            save();
            emit instancesChanged();
            return;
        }
    }
}

void ConfigManager::updateInstanceLaunchArgs(const QString &id, const QString &args)
{
    for (int i = 0; i < m_instances.size(); ++i) {
        if (m_instances[i].id == id) {
            m_instances[i].launchArgs = args;
            save();
            emit instancesChanged();
            return;
        }
    }
}

KSPInstance ConfigManager::currentInstance() const
{
    return getInstance(m_currentInstanceId);
}

void ConfigManager::setCurrentInstance(const QString &id)
{
    if (m_currentInstanceId != id) {
        m_currentInstanceId = id;
        save();
        emit currentInstanceChanged();
    }
}

KSPInstance ConfigManager::getInstance(const QString &id) const
{
    for (const KSPInstance& inst : m_instances) {
        if (inst.id == id) {
            return inst;
        }
    }
    return KSPInstance();
}
