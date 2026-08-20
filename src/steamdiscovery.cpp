#include "steamdiscovery.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QVariant>
#include <QMetaType>
#include <QDebug>

namespace {

// ---------------------------------------------------------------------------
// VDF(KeyValues1Text) 极简解析：仅处理 libraryfolders.vdf 所需子集——
// 引号字符串、花括号嵌套、// 与 /* */ 注释。结果以 QVariantMap 表示，
// 嵌套块对应 QVariantMap，普通键值对对应 QString。
// ---------------------------------------------------------------------------
class VdfParser {
public:
    static QVariantMap parse(const QString &input, bool *ok = nullptr)
    {
        VdfParser p(input);
        QVariantMap root;
        if (ok) *ok = p.parseNode(root);
        return root;
    }

private:
    explicit VdfParser(const QString &input) : m_input(input) {}

    bool parseNode(QVariantMap &out)
    {
        for (;;) {
            skipWs();
            if (atEnd()) return true;
            if (peek() == QLatin1Char('}')) { consume(); return true; }
            const QString key = readString();
            if (key.isNull()) return false;
            skipWs();
            if (atEnd()) return false;
            if (peek() == QLatin1Char('{')) {
                consume();
                QVariantMap child;
                if (!parseNode(child)) return false;
                out[key] = child;
            } else {
                const QString value = readString();
                if (value.isNull()) return false;
                out[key] = value;
            }
        }
    }

    void skipWs()
    {
        for (;;) {
            if (atEnd()) return;
            const QChar c = peek();
            if (c == QLatin1Char('/') && m_pos + 1 < m_input.size()) {
                const QChar c2 = m_input.at(m_pos + 1);
                if (c2 == QLatin1Char('/')) {
                    m_pos += 2;
                    while (!atEnd() && peek() != QLatin1Char('\n')) ++m_pos;
                    continue;
                }
                if (c2 == QLatin1Char('*')) {
                    m_pos += 2;
                    while (m_pos + 1 < m_input.size()
                           && !(m_input.at(m_pos) == QLatin1Char('*')
                                && m_input.at(m_pos + 1) == QLatin1Char('/'))) {
                        ++m_pos;
                    }
                    if (m_pos + 1 < m_input.size()) m_pos += 2;
                    continue;
                }
            }
            if (c.isSpace()) { ++m_pos; continue; }
            return;
        }
    }

    QString readString()
    {
        skipWs();
        if (atEnd() || peek() != QLatin1Char('"')) return QString();
        ++m_pos;
        // 空字符串值 ""：返回非 null 空串，避免被上层误判为解析失败
        if (!atEnd() && peek() == QLatin1Char('"')) { ++m_pos; return QStringLiteral(""); }
        QString s;
        for (;;) {
            if (atEnd()) return QString(); // 未闭合的字符串
            const QChar c = peek();
            if (c == QLatin1Char('"')) { ++m_pos; return s; }
            if (c == QLatin1Char('\\') && m_pos + 1 < m_input.size()) {
                ++m_pos;
                s += peek();
                ++m_pos;
                continue;
            }
            s += c;
            ++m_pos;
        }
    }

    bool atEnd() const { return m_pos >= m_input.size(); }
    QChar peek() const { return m_input.at(m_pos); }
    void consume() { ++m_pos; }

    const QString &m_input;
    int m_pos = 0;
};

} // namespace

QStringList SteamDiscovery::discoverKSPDirs()
{
    QStringList result;
#ifdef Q_OS_WIN
    const QString steamDir = steamInstallPath();
    if (steamDir.isEmpty()) return result;

    // 库根目录：主库由注册表指向 Steam 安装目录；附加库来自 libraryfolders.vdf。
    QStringList libraryRoots = parseLibraryFolders(
        QDir(steamDir).filePath(QStringLiteral("config/libraryfolders.vdf")));
    if (libraryRoots.isEmpty())
        libraryRoots.append(steamDir);

    // 兼容新旧 Steam：库目录名可能是 steamapps 或 SteamApps。
    static const QStringList appRelPaths = {
        QStringLiteral("steamapps"), QStringLiteral("SteamApps")
    };
    for (const QString &lib : libraryRoots) {
        for (const QString &rel : appRelPaths) {
            const QString kspDir = QDir(QDir(QDir(lib).filePath(rel))
                                        .filePath(QStringLiteral("common")))
                                        .filePath(QStringLiteral("Kerbal Space Program"));
            if (!QDir(kspDir).exists()) continue;
            if (QDir(kspDir).exists(QStringLiteral("settings.cfg"))
                && QDir(kspDir).exists(QStringLiteral("GameData"))) {
                result.append(QDir::cleanPath(kspDir));
                break;
            }
        }
    }
#endif
    return result;
}

QStringList SteamDiscovery::parseLibraryFolders(const QString &vdfPath)
{
    QStringList result;
    QFile file(vdfPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    const QString data = QString::fromUtf8(file.readAll());
    file.close();

    bool ok = false;
    const QVariantMap root = VdfParser::parse(data, &ok);
    if (!ok) return result;

    const QVariant libs = root.value(QStringLiteral("libraryfolders"));
    if (libs.metaType().id() != QMetaType::QVariantMap) return result;
    const QVariantMap folders = libs.toMap();
    for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
        if (it.value().metaType().id() != QMetaType::QVariantMap) continue;
        const QString p = it.value().toMap().value(QStringLiteral("path")).toString().trimmed();
        if (!p.isEmpty()) result.append(p);
    }
    return result;
}

QString SteamDiscovery::steamInstallPath()
{
#ifdef Q_OS_WIN
    // 参照 CKAN-master：HKEY_CURRENT_USER\Software\Valve\Steam 的 SteamPath
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"),
                  QSettings::NativeFormat);
    return reg.value(QStringLiteral("SteamPath")).toString().trimmed();
#else
    return QString();
#endif
}
