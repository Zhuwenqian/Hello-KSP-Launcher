#include "gameinstance.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace ckan {

static QString normalized(const QString &p)
{
    QString n = QDir::cleanPath(p);
    n.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return n;
}

GameInstance::GameInstance(const QString &gameDir, const QString &name)
    : m_gameDir(normalized(gameDir)), m_name(name)
{
    setupCkanDirectories();
}

void GameInstance::setupCkanDirectories()
{
    QDir().mkpath(ckanDir());
    QDir().mkpath(historyDir());
    if (!m_customDownloadDir.isEmpty())
        QDir().mkpath(m_customDownloadDir);
    // 兼容原 CKAN：默认 downloads 存于 CKAN/downloads（用户可覆盖为启动器 downloads）
    QDir().mkpath(downloadDir());
}

QString GameInstance::toRelativeGameDir(const QString &abs) const
{
    const QString a = normalized(abs);
    const QString g = normalized(m_gameDir);
    if (a.startsWith(g + QLatin1Char('/')))
        return a.mid(g.size() + 1);
    return a;
}

QString GameInstance::toAbsoluteGameDir(const QString &rel) const
{
    QString r = normalized(rel);
    while (r.startsWith(QLatin1Char('/'))) r = r.mid(1);
    return normalized(m_gameDir + (r.isEmpty() ? QString() : QStringLiteral("/") + r));
}

GameVersion GameInstance::detectVersion() const
{
    // 1) buildID 文件
    const QStringList buildFiles = { QStringLiteral("buildID64.txt"), QStringLiteral("buildID.txt") };
    for (const QString &bf : buildFiles) {
        QFile f(m_gameDir + QLatin1Char('/') + bf);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(f.readAll()).trimmed();
            f.close();
            // 形如 "12345\n" 的 buildID 无法直接推导版本号，跳过
            bool isNumeric = false;
            content.toInt(&isNumeric);
            if (!isNumeric && !content.isEmpty())
                return GameVersion(content);
        }
    }
    // 2) readme.txt 中的版本行
    QFile rf(m_gameDir + QStringLiteral("/readme.txt"));
    if (rf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(rf.readAll());
        rf.close();
        static const QRegularExpression re(QStringLiteral("Version\\s*\\n?\\s*(\\d+\\.\\d+(\\.\\d+)?)"),
                                           QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(content);
        if (m.hasMatch())
            return GameVersion(m.captured(1));
    }
    return GameVersion();
}

Registry *GameInstance::registry()
{
    if (!m_registryLoaded)
        loadRegistry();
    return &m_registry;
}

void GameInstance::loadRegistry()
{
    QFile f(registryPath());
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QString err;
        m_registry = Registry::fromJson(f.readAll(), &err);
        f.close();
    }
    m_registryLoaded = true;
}

void GameInstance::saveRegistry() const
{
    QDir().mkpath(ckanDir());
    QFile f(registryPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_registry.toJson());
        f.close();
    }
}

bool GameInstance::isValid() const
{
    const bool hasExec = QFileInfo::exists(m_gameDir + QStringLiteral("/KSP_x64.exe"))
                      || QFileInfo::exists(m_gameDir + QStringLiteral("/KSP.exe"))
                      || QFileInfo::exists(m_gameDir + QStringLiteral("/KSP.x86_64"))
                      || QDir(m_gameDir).exists(QStringLiteral("GameData"));
    return hasExec;
}

} // namespace ckan