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
    m_config["indexRefreshIntervalSecs"] = 6 * 60 * 60;
    m_config["indexDownloadSource"] = static_cast<int>(OfficialFirst);
    m_config["moduleDownloadSource"] = static_cast<int>(OfficialFirst);
    m_config["downloadCacheDir"] = QString();
    m_config["downloadConcurrency"] = 3;
    m_config["diskSpaceCheck"] = true;
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

    // 确保新字段有默认值(旧版配置文件可能没有)
    if (!m_config.contains("backgroundPath")) {
        m_config["backgroundPath"] = QString();
    }

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
        // 兼容版本勾选：字段存在（可能为空数组）按持久化值，空即"未勾选任何版本"；
        // 字段缺失（旧配置/新实例）保持未配置，由 compatibleVersions() 按检测到的
        // 游戏版本动态推导默认勾选（1.9~1.12 区间内回退到 1.9，低于 1.9 仅勾选自身版本线）。
        if (obj.contains("compatibleVersions")) {
            const QJsonArray arr = obj["compatibleVersions"].toArray();
            for (const QJsonValue &v : arr)
                inst.compatibleVersions.append(v.toString());
            inst.compatVersionsSet = true;
        }
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
        // 仅持久化用户显式配置的兼容版本；未配置时省略该字段，
        // 下次加载仍保持"未配置"，由游戏版本动态推导默认勾选。
        if (inst.compatVersionsSet)
            obj["compatibleVersions"] = QJsonArray::fromStringList(inst.compatibleVersions);
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

QString ConfigManager::backgroundPath() const
{
    return m_config["backgroundPath"].toString();
}

void ConfigManager::setBackgroundPath(const QString &path)
{
    if (backgroundPath() != path) {
        m_config["backgroundPath"] = path;
        save();
        emit configChanged();
    }
}

bool ConfigManager::showIncompatibleMods() const
{
    return m_config["showIncompatibleMods"].toBool(false);
}

void ConfigManager::setShowIncompatibleMods(bool show)
{
    if (showIncompatibleMods() != show) {
        m_config["showIncompatibleMods"] = show;
        save();
        emit configChanged();
    }
}

QStringList ConfigManager::defaultCompatibleVersions(const ckan::GameVersion &detectedVersion)
{
    QStringList lines;
    if (detectedVersion.isValid() && detectedVersion.major() == 1) {
        const int minor = detectedVersion.minor();
        if (minor >= 9 && minor <= 12) {
            // 位于 [1.9, 1.12]：勾选「检测版本线 ~ 1.9」全部版本线（如 1.11.x → 1.11/1.10/1.9）
            for (int m = minor; m >= 9; --m)
                lines << QStringLiteral("1.%1").arg(m);
        } else if (minor >= 0) {
            // 低于 1.9（或高于 1.12）：仅勾选检测版本所在版本线
            lines << QStringLiteral("1.%1").arg(minor);
        }
    }
    if (lines.isEmpty()) {
        // 版本检测失败或结构异常时回退静态默认 1.9~1.12
        lines = { QStringLiteral("1.12"), QStringLiteral("1.11"),
                  QStringLiteral("1.10"), QStringLiteral("1.9") };
    }
    return lines;
}

QStringList ConfigManager::compatibleVersions(const QString &instanceId,
                                              const ckan::GameVersion &detectedVersion) const
{
    for (const KSPInstance &inst : m_instances) {
        if (inst.id == instanceId) {
            if (inst.compatVersionsSet)
                return inst.compatibleVersions;
            return defaultCompatibleVersions(detectedVersion);
        }
    }
    return defaultCompatibleVersions(detectedVersion);
}

void ConfigManager::setCompatibleVersions(const QString &instanceId, const QStringList &versionLines)
{
    for (KSPInstance &inst : m_instances) {
        if (inst.id == instanceId) {
            if (inst.compatVersionsSet && inst.compatibleVersions == versionLines) return;
            inst.compatVersionsSet = true;
            inst.compatibleVersions = versionLines;
            save();
            emit instancesChanged();
            return;
        }
    }
}

bool ConfigManager::installSuggests() const
{
    return m_config["installSuggests"].toBool(true);
}

void ConfigManager::setInstallSuggests(bool enable)
{
    if (installSuggests() != enable) {
        m_config["installSuggests"] = enable;
        save();
        emit configChanged();
    }
}

bool ConfigManager::diskSpaceCheck() const
{
    return m_config["diskSpaceCheck"].toBool(true);
}

void ConfigManager::setDiskSpaceCheck(bool enable)
{
    if (diskSpaceCheck() != enable) {
        m_config["diskSpaceCheck"] = enable;
        save();
        emit configChanged();
    }
}

int ConfigManager::indexRefreshIntervalSecs() const
{
    return m_config["indexRefreshIntervalSecs"].toInt(6 * 60 * 60);
}

void ConfigManager::setIndexRefreshIntervalSecs(int secs)
{
    if (indexRefreshIntervalSecs() != secs) {
        m_config["indexRefreshIntervalSecs"] = secs;
        save();
        emit configChanged();
    }
}

ConfigManager::DownloadSource ConfigManager::indexDownloadSource() const
{
    return static_cast<DownloadSource>(
        m_config["indexDownloadSource"].toInt(static_cast<int>(OfficialFirst)));
}

void ConfigManager::setIndexDownloadSource(DownloadSource source)
{
    if (indexDownloadSource() != source) {
        m_config["indexDownloadSource"] = static_cast<int>(source);
        save();
        emit configChanged();
    }
}

ConfigManager::DownloadSource ConfigManager::moduleDownloadSource() const
{
    return static_cast<DownloadSource>(
        m_config["moduleDownloadSource"].toInt(static_cast<int>(OfficialFirst)));
}

void ConfigManager::setModuleDownloadSource(DownloadSource source)
{
    if (moduleDownloadSource() != source) {
        m_config["moduleDownloadSource"] = static_cast<int>(source);
        save();
        emit configChanged();
    }
}

QString ConfigManager::downloadCacheDir() const
{
    return m_config["downloadCacheDir"].toString();
}

void ConfigManager::setDownloadCacheDir(const QString &dir)
{
    if (downloadCacheDir() != dir) {
        m_config["downloadCacheDir"] = dir;
        save();
        emit configChanged();
    }
}

int ConfigManager::downloadConcurrency() const
{
    const int v = m_config["downloadConcurrency"].toInt(3);
    return qBound(1, v, 8);
}

void ConfigManager::setDownloadConcurrency(int count)
{
    const int clamped = qBound(1, count, 8);
    if (downloadConcurrency() != clamped) {
        m_config["downloadConcurrency"] = clamped;
        save();
        emit configChanged();
    }
}

qint64 ConfigManager::downloadRateLimitBytesPerSecond() const
{
    const qint64 v = m_config["downloadRateLimitBytesPerSecond"].toDouble(0);
    return v > 0 ? v : 0;
}

void ConfigManager::setDownloadRateLimitBytesPerSecond(qint64 bps)
{
    const qint64 valid = bps > 0 ? bps : 0; // 负数视为不限速（按 0）
    if (downloadRateLimitBytesPerSecond() != valid) {
        m_config["downloadRateLimitBytesPerSecond"] = static_cast<double>(valid);
        save();
        emit configChanged();
    }
}

QVector<int> ConfigManager::defaultModTableColumnWidths()
{
    // 顺序与 ModsTableModel::Column 一致：勾选/名称/标识符/版本/状态/大小/下载/标签
    return { 40, 220, 180, 90, 80, 90, 70, 140 };
}

QVector<int> ConfigManager::modTableColumnWidths() const
{
    const QVector<int> def = defaultModTableColumnWidths();
    const QJsonArray arr = m_config["modTableColumnWidths"].toArray();
    QVector<int> widths = def;
    for (int i = 0; i < arr.size() && i < widths.size(); ++i) {
        const int w = arr.at(i).toInt(0);
        if (w > 0) widths[i] = w;
    }
    return widths;
}

void ConfigManager::setModTableColumnWidths(const QVector<int> &widths)
{
    QVector<int> cur = modTableColumnWidths();
    if (cur.size() != widths.size() || cur != widths) {
        QJsonArray arr;
        for (int w : widths) arr.append(w);
        m_config["modTableColumnWidths"] = arr;
        save();
        emit configChanged();
    }
}

QVector<ckan::Repository> ConfigManager::repositories() const
{
    QVector<ckan::Repository> repos;
    const QJsonArray arr = m_config["repositories"].toArray();
    if (arr.isEmpty()) {
        repos.append(ckan::Repository::defaultKspRepo());
        return repos;
    }
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject obj = arr.at(i).toObject();
        ckan::Repository r;
        r.name = obj["name"].toString();
        r.uri  = obj["uri"].toString();
        r.priority = i; // 数组顺序即优先级（首位优先级最高）
        r.mirror = obj["mirror"].toBool(false);
        r.comment = obj["comment"].toString();
        if (r.isValid())
            repos.append(r);
    }
    return repos;
}

void ConfigManager::setRepositories(const QVector<ckan::Repository> &repos)
{
    QJsonArray arr;
    for (int i = 0; i < repos.size(); ++i) {
        const ckan::Repository &r = repos.at(i);
        QJsonObject obj;
        obj["name"] = r.name;
        obj["uri"] = r.uri;
        obj["mirror"] = r.mirror;
        obj["comment"] = r.comment;
        arr.append(obj);
    }
    m_config["repositories"] = arr;
    save();
    emit configChanged();
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
