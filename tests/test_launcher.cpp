#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QDir>
#include <cstring>

#include "miniz.h"
#include "steamdiscovery.h"
#include "instancemanager.h"
#include "updatemanager.h"
#include "instanceiconmanager.h"
#include "ckanmanager.h"
#include "services/indexservice.h"
#include "services/cacheservice.h"
#include "services/installservice.h"
#include "pages/modtablemodel.h"
#include "playerloganalyzer.h"

// 构造一个仅填关键字段的 CkanModule（identifier/name/version 必有，其余可空）
static ckan::CkanModule makeMod(const QString &identifier, const QString &name, const QString &version,
                                const QStringList &tags = {}, const QString &abstract = {})
{
    ckan::CkanModule m;
    m.identifier = identifier;
    m.name = name;
    m.version = version;
    m.tags = tags;
    m.abstract = abstract;
    return m;
}

// 启动器逻辑测试：Steam 库发现（仅与 src/steamdiscovery.cpp 相关，不依赖 libckan）。
class TestSteamDiscovery : public QObject
{
    Q_OBJECT
private slots:
    void parseLibraryFoldersExtractsPaths()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vdfPath = dir.filePath(QStringLiteral("libraryfolders.vdf"));
        QFile f(vdfPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(
            "\"libraryfolders\"\n"
            "{\n"
            "\t\"0\"\n"
            "\t{\n"
            "\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n"
            "\t\t\"label\"\t\t\"\"\n"
            "\t\t\"apps\"\n"
            "\t\t{\n"
            "\t\t\t\"220200\"\t\t\"0\"\n"
            "\t\t}\n"
            "\t}\n"
            "\t\"1\"\n"
            "\t{\n"
            "\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n"
            "\t\t\"label\"\t\t\"\"\n"
            "\t}\n"
            "\t// 注释行应被忽略\n"
            "\t\"692030\"\n"
            "\t{\n"
            "\t\t\"path\"\t\t\"E:\\\\SteamGames\"\n"
            "\t}\n"
            "}\n");
        f.close();

        // 解析出的库路径应包含主库与所有附加库（含注释行之后的条目）
        const QStringList paths = SteamDiscovery::parseLibraryFolders(vdfPath);
        QCOMPARE(paths.size(), 3);
        QVERIFY(paths.contains(QStringLiteral("C:\\Program Files (x86)\\Steam")));
        QVERIFY(paths.contains(QStringLiteral("D:\\SteamLibrary")));
        QVERIFY(paths.contains(QStringLiteral("E:\\SteamGames")));
    }

    void parseLibraryFoldersMissingFileReturnsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(SteamDiscovery::parseLibraryFolders(
                    dir.filePath(QStringLiteral("not_exist.vdf"))).isEmpty());
    }

    void parseLibraryFoldersMalformedReturnsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString vdfPath = dir.filePath(QStringLiteral("bad.vdf"));
        QFile f(vdfPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("\"libraryfolders\"\n{\n\t\"0\" \"unterminated\n"); // 未闭合字符串
        f.close();
        QVERIFY(SteamDiscovery::parseLibraryFolders(vdfPath).isEmpty());
    }
};

// 实例目录合法性检测：settings.cfg 不是必须项，仅需 GameData + KSP 可执行文件。
class TestValidKSPPath : public QObject
{
    Q_OBJECT
private:
    InstanceManager &mgr;
public:
    TestValidKSPPath() : mgr(InstanceManager::instance()) {}

private slots:
    void freshInstallWithoutSettingsCfgIsValid()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("GameData"))));
        QFile exe(dir.filePath(QStringLiteral("KSP_x64.exe")));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();
        // 无 settings.cfg（全新未运行安装）应判为合法
        QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("settings.cfg"))));
        QVERIFY(mgr.isValidKSPPath(dir.path()));
    }

    void missingGameDataIsInvalid()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile exe(dir.filePath(QStringLiteral("KSP.exe")));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();
        QVERIFY(!mgr.isValidKSPPath(dir.path()));
    }

    void missingExecutableIsInvalid()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("GameData"))));
        QFile cfg(dir.filePath(QStringLiteral("settings.cfg")));
        QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.close();
        // 仅有 settings.cfg + GameData、无 KSP 可执行文件，不应判为合法
        QVERIFY(!mgr.isValidKSPPath(dir.path()));
    }

    void unixExecutableFallbackIsValid()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("GameData"))));
        QFile exe(dir.filePath(QStringLiteral("KSP.x86_64")));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();
        QVERIFY(mgr.isValidKSPPath(dir.path()));
    }
};

// 启动参数/Profile 相关：跨平台进程钩子（高优先级 / 浏览器结束命令）的纯函数回归。
class TestLaunchOptions : public QObject
{
    Q_OBJECT
private slots:
    void browserKillCommandsNotEmpty()
    {
        // 高优先级模式下应能生成结束浏览器的命令
        QVERIFY(!processopt::browserKillCommands().isEmpty());
    }

    void browserKillCommandsTargetCommonBrowsers()
    {
        const QList<processopt::KillCommand> cmds = processopt::browserKillCommands();
        QStringList flattened;
        for (const processopt::KillCommand &c : cmds) {
            QVERIFY(!c.program.isEmpty());
            flattened << c.program << c.args;
        }
#if defined(_WIN32)
        // Windows：taskkill 按映像名结束 msedge/chrome/firefox
        QVERIFY(flattened.contains(QStringLiteral("msedge.exe")));
        QVERIFY(flattened.contains(QStringLiteral("chrome.exe")));
        QVERIFY(flattened.contains(QStringLiteral("firefox.exe")));
#else
        // POSIX：pkill 匹配进程名
        QVERIFY(flattened.contains(QStringLiteral("msedge")));
        QVERIFY(flattened.contains(QStringLiteral("firefox")));
        QVERIFY(flattened.contains(QStringLiteral("chrome")));
#endif
    }

    void invalidPidHighPriorityFails()
    {
        // 非法 pid（0 / 负数）不应误判为成功
        QVERIFY(!processopt::setProcessHighPriority(0));
        QVERIFY(!processopt::setProcessHighPriority(-5));
    }

#if defined(_WIN32)
    void windowsMemoryJobRejectsInvalidPid()
    {
        // 内存限制 Job Object：非法 pid 返回空句柄
        QVERIFY(processopt::openMemoryJob(0, 4096) == nullptr);
        QVERIFY(processopt::openMemoryJob(-1, 4096) == nullptr);
    }
#endif
};

// 自更新版本比较（纯逻辑，无需网络）
class TestUpdaterManager : public QObject
{
    Q_OBJECT
private slots:
    void versionComparison();
    void digestHex();
    void updaterUpdateBodyDetection();
    void zipUpdaterEntry();
};

void TestUpdaterManager::versionComparison()
{
    // 简单递增
    QVERIFY(UpdaterManager::versionLess("1.1.2", "1.1.3"));
    QVERIFY(UpdaterManager::versionLess("1.1.2", "1.2.0"));
    QVERIFY(UpdaterManager::versionLess("1.1.2", "2.0.0"));
    // 相等 / 反向
    QVERIFY(!UpdaterManager::versionLess("1.1.2", "1.1.2"));
    QVERIFY(!UpdaterManager::versionLess("1.1.3", "1.1.2"));
    // 段数不同的补齐（1.3 == 1.3.0，2 == 2.0.0）
    QVERIFY(!UpdaterManager::versionLess("1.3", "1.3.0"));
    QVERIFY(UpdaterManager::versionLess("1.3", "1.3.1"));
    QVERIFY(UpdaterManager::versionLess("2", "2.0.1"));
    // 大版本优先
    QVERIFY(UpdaterManager::versionLess("1.9.9", "2.0.0"));
    // 非纯数字段（预发布）→ 字典序兜底，不崩溃
    QVERIFY(!UpdaterManager::versionLess("1.1.2-beta", "1.1.2"));
}

void TestUpdaterManager::digestHex()
{
    // 标准 "sha256:<64hex>" → 取小写 hex
    QJsonArray arr;
    {
        QJsonObject a;
        a.insert("name", "HKSPL-x86_64.zip");
        a.insert("digest", "sha256:9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");
        arr.append(a);
    }
    QCOMPARE(UpdaterManager::digestHexFromApi(arr, "HKSPL-x86_64.zip"),
             QStringLiteral("9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"));
    // 同名但大小写不同也要匹配
    QCOMPARE(UpdaterManager::digestHexFromApi(arr, "hkspl-x86_64.ZIP"),
             QStringLiteral("9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"));
    // 名称不匹配 → 空
    QVERIFY(UpdaterManager::digestHexFromApi(arr, "other.zip").isEmpty());
    // 非 sha256: 前缀 → 拒绝
    {
        QJsonArray bad;
        QJsonObject a;
        a.insert("name", "HKSPL-x86_64.zip");
        a.insert("digest", "md5:deadbeef");
        bad.append(a);
        QVERIFY(UpdaterManager::digestHexFromApi(bad, "HKSPL-x86_64.zip").isEmpty());
    }
    // 长度不足 64 / 含非 hex → 拒绝
    {
        QJsonArray bad;
        QJsonObject a;
        a.insert("name", "HKSPL-x86_64.zip");
        a.insert("digest", "sha256:aabb");
        bad.append(a);
        QVERIFY(UpdaterManager::digestHexFromApi(bad, "HKSPL-x86_64.zip").isEmpty());
    }
}

void TestUpdaterManager::updaterUpdateBodyDetection()
{
    // 完整双语提示块 → 命中
    QVERIFY(UpdaterManager::bodyIndicatesUpdaterUpdate(QStringLiteral(
        "> **重要提示 / Important Note:** 本 Release 含有更新器（`updater.exe`）的更新。"
        "> This release ships an updated built-in updater (`updater.exe`).")));
    // 仅中文短语 → 命中
    QVERIFY(UpdaterManager::bodyIndicatesUpdaterUpdate(QStringLiteral(
        "本 Release 含有更新器（updater.exe）的更新")));
    // 仅英文短语 → 命中
    QVERIFY(UpdaterManager::bodyIndicatesUpdaterUpdate(QStringLiteral(
        "This release ships an updated built-in updater (updater.exe).")));
    // 普通更新日志（无更新器提示）→ 不命中
    QVERIFY(!UpdaterManager::bodyIndicatesUpdaterUpdate(QStringLiteral(
        "修复了若干 bug，提升了稳定性。")));
    // 空 body → 不命中
    QVERIFY(!UpdaterManager::bodyIndicatesUpdaterUpdate(QString()));
}

void TestUpdaterManager::zipUpdaterEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString zip = dir.filePath(QStringLiteral("release.zip"));
    // 构造一个含单一顶层发布目录（Release/）的 zip，内含 updater.exe 与普通文件
    const QString srcExe = dir.filePath(QStringLiteral("src/updater.exe"));
    QDir().mkpath(QFileInfo(srcExe).absolutePath());
    {
        QFile f(srcExe);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("fake-updater-binary");
    }
    const QString srcReadme = dir.filePath(QStringLiteral("src/README.txt"));
    {
        QFile f(srcReadme);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("readme");
    }
    {
        mz_zip_archive z;
        std::memset(&z, 0, sizeof(z));
        QVERIFY(mz_zip_writer_init_file(&z, zip.toUtf8().constData(), 0));
        QVERIFY(mz_zip_writer_add_file(&z, "Release/updater.exe",
                                       srcExe.toUtf8().constData(), nullptr, 0, 0));
        QVERIFY(mz_zip_writer_add_file(&z, "Release/README.txt",
                                       srcReadme.toUtf8().constData(), nullptr, 0, 0));
        QVERIFY(mz_zip_writer_finalize_archive(&z));
        QVERIFY(mz_zip_writer_end(&z));
    }

    // 按 basename 查找 updater.exe（任意深度、大小写不敏感）
    QString entry;
    QVERIFY(UpdaterManager::findZipEntryByBaseName(zip, QStringLiteral("updater.exe"), &entry));
    QCOMPARE(entry, QStringLiteral("Release/updater.exe"));
    QString entryUpper;
    QVERIFY(UpdaterManager::findZipEntryByBaseName(zip, QStringLiteral("Updater.EXE"), &entryUpper));
    QCOMPARE(entryUpper, entry);
    // 不存在的条目 → 未找到
    QString miss;
    QVERIFY(!UpdaterManager::findZipEntryByBaseName(zip, QStringLiteral("missing.dll"), &miss));
    // 其他文件按 basename 同样可找到（查找逻辑非 updater 特化）
    QString readmeEntry;
    QVERIFY(UpdaterManager::findZipEntryByBaseName(zip, QStringLiteral("README.txt"), &readmeEntry));
    QCOMPARE(readmeEntry, QStringLiteral("Release/README.txt"));

    // 解压条目到目标路径，内容一致
    const QString outExe = dir.filePath(QStringLiteral("out/updater.exe"));
    QVERIFY(UpdaterManager::extractZipEntry(zip, entry, outExe));
    {
        QFile f(outExe);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("fake-updater-binary"));
    }
    // 解压不存在的条目 → 失败
    QVERIFY(!UpdaterManager::extractZipEntry(zip, QStringLiteral("nope/updater.exe"),
                                             dir.filePath(QStringLiteral("out2/updater.exe"))));
    // zip 路径无效 → 查找失败
    QVERIFY(!UpdaterManager::findZipEntryByBaseName(
        dir.filePath(QStringLiteral("not_exist.zip")), QStringLiteral("updater.exe"), &miss));
}

// 实例列表图标的来源判定（按实例名后缀：RP-1 > RSS/Sol > exe）
class TestInstanceIconSource : public QObject
{
    Q_OBJECT
private slots:
    void resolveSource();
};

void TestInstanceIconSource::resolveSource()
{
    using Src = InstanceIconManager::Source;
    // 纯净/无后缀 → exe 图标
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5"))), Src::Exe);
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP"))), Src::Exe);
    // RSS → RSS 图标
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5 RSS"))), Src::Rss);
    // Sol 是 RSS 的超级美化分支 → 同样使用 RSS 图标
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5 Sol"))), Src::Rss);
    // RP-1 → RP-1 图标，且优先级高于 RSS
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5 RP-1"))), Src::Rp1);
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5 RSS RP-1"))), Src::Rp1);
    // RO / 其它后缀 → exe 图标（只有 RSS/Sol/RP-1 有专属图标）
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5 RO"))), Src::Exe);
    // 不区分大小写
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("ksp 1.12.5 rp-1"))), Src::Rp1);
    QCOMPARE(Src(InstanceIconManager::resolveSource(QStringLiteral("KSP 1.12.5 rss"))), Src::Rss);
}

// 业务服务层：Repository 索引版本比较（纯逻辑，不依赖真实仓库）
class TestIndexService : public QObject
{
    Q_OBJECT
private slots:
    void isNewerVersion()
    {
        using services::IndexService;
        // simple increments
        QVERIFY(IndexService::isNewerVersion(QStringLiteral("1.1.3"), QStringLiteral("1.1.2")));
        QVERIFY(IndexService::isNewerVersion(QStringLiteral("2.0.0"), QStringLiteral("1.9.9")));
        // equal / reverse
        QVERIFY(!IndexService::isNewerVersion(QStringLiteral("1.1.2"), QStringLiteral("1.1.2")));
        QVERIFY(!IndexService::isNewerVersion(QStringLiteral("1.1.2"), QStringLiteral("1.1.3")));
        // segment padding (1.3 == 1.3.0)
        QVERIFY(!IndexService::isNewerVersion(QStringLiteral("1.3"), QStringLiteral("1.3.0")));
        QVERIFY(IndexService::isNewerVersion(QStringLiteral("1.3.1"), QStringLiteral("1.3")));

        // 无索引 / 无效版本不抛、返回 false（达到 UI 层的乐观路径语义）
        services::IndexService svc; // 未 setCkan → 查询空
        QVERIFY(!svc.isUpgradable(QStringLiteral("any.identifier")));
        QVERIFY(!svc.indexReady());
    }
};

// 下载缓存服务：缓存 zip 文件名匹配（精确清理的核心决策，纯逻辑）
class TestCacheService : public QObject
{
    Q_OBJECT
private slots:
    void knownCacheFileNames()
    {
        using services::CacheService;
        const QString id = QStringLiteral("TweakScale");
        const QString ver = QStringLiteral("2.4.8.4");
        const QString url =
            QStringLiteral("https://github.com/net-lisias-ksp/TweakScale/releases/download/2.4.8.4/TweakScale_2.4.8.4.zip");
        // 必须同时给出官方 <hash8>-<id>-<ver>.zip、<id>-<ver>.zip 与 <id>_<safe>.zip 三种名
        const QStringList names = CacheService::knownCacheFileNames(id, ver, url);
        QVERIFY(names.size() >= 3);
        QVERIFY(names.contains(id + QLatin1Char('_') + ckan::CKan::safeCacheFileName(ver) + QStringLiteral(".zip")));
        QVERIFY(names.contains(QStringLiteral("TweakScale-2.4.8.4.zip")));
        QVERIFY(names.contains(ckan::CKan::officialCacheFileName(id, ver, url)));
        // 为空标识符/版本时不产出（不作为已知文件参与清理 → 不误删）
        QVERIFY(CacheService::knownCacheFileNames(QString(), ver, url).isEmpty());
        QVERIFY(CacheService::knownCacheFileNames(id, QString(), url).isEmpty());
    }
};

// 安装前置决策服务：未绑定实例时的失败分支（无依赖、不触弹窗）
class TestInstallService : public QObject
{
    Q_OBJECT
private slots:
    void nullInstanceFails()
    {
        services::InstallService svc; // 未 setCkan
        const auto res = svc.resolveInstallSet({}, true, false, false);
        QVERIFY(!res.ok);
        QVERIFY(!res.cancelled);
        QVERIFY(!res.nothingToDo);
        QVERIFY(!res.error.isEmpty());

        const services::InstallService svc2; // 未 setCkan
        QVERIFY(!svc2.resolveInstallSet({ ckan::CkanModule{} }, true, false, false).ok);
    }
};

// 模组表过滤与行级缓存：预计算缓存/预解析搜索词后，过滤行为与原逐行实时计算语义一致
// （回归：缓存路径不得改变搜索结果）。
class TestModsTableFilter : public QObject
{
    Q_OBJECT
private slots:
    void generalSearchHitsNameIdentifierAndAbstract()
    {
        ModsTableModel model;
        model.setModules({
            makeMod(QStringLiteral("ModA"), QStringLiteral("Kerbal Flight"), QStringLiteral("1.0")),
            makeMod(QStringLiteral("ksp-mod"), QStringLiteral("KSP Mod Helper"), QStringLiteral("2.0")),
            makeMod(QStringLiteral("ModC"), QStringLiteral("Realism Overhaul"), QStringLiteral("3.0"),
                    {}, QStringLiteral("adds ksp realism")),
        });
        ModsFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        QCOMPARE(proxy.rowCount(), 3);

        proxy.setSearchText(QStringLiteral("ksp"));
        QCOMPARE(proxy.rowCount(), 2); // ksp-mod(标识符) + ModC(摘要)
        proxy.setSearchText(QStringLiteral("kerbal"));
        QCOMPARE(proxy.rowCount(), 1); // 仅 ModA(名称)
        proxy.setSearchText(QStringLiteral("realism"));
        QCOMPARE(proxy.rowCount(), 1); // 仅 ModC(摘要)
        proxy.setSearchText(QStringLiteral("  ")); // 空白视为清空
        QCOMPARE(proxy.rowCount(), 3);
    }

    void fieldSearchAuthorTagAndDepends()
    {
        ckan::CkanModule m = makeMod(QStringLiteral("FieldMod"), QStringLiteral("Field Mod"),
                                     QStringLiteral("1.0"));
        m.author = { QStringLiteral("NecroBones"), QStringLiteral("linuxgurugamer") };
        m.tags = { QStringLiteral("physics"), QStringLiteral("science") };
        ckan::Relationship rel;
        rel.name = QStringLiteral("ModuleManager");
        m.depends = { rel };

        ModsTableModel model;
        model.setModules({ m });
        ModsFilterProxyModel proxy;
        proxy.setSourceModel(&model);

        proxy.setSearchText(QStringLiteral("@author:necro"));
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setSearchText(QStringLiteral("@tag:SCIENCE"));
        QCOMPARE(proxy.rowCount(), 1); // 大小写不敏感
        proxy.setSearchText(QStringLiteral("@tag:zzz"));
        QCOMPARE(proxy.rowCount(), 0);
        proxy.setSearchText(QStringLiteral("@depend:modulemanager"));
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setSearchText(QStringLiteral("@unknown:x"));
        QCOMPARE(proxy.rowCount(), 1); // 未知字段不否决
        proxy.setSearchText(QStringLiteral("ksp @author:none"));
        QCOMPARE(proxy.rowCount(), 0); // AND 关系：任一 token 不命中即否决
    }

    void statusFilterUsesPrecomputedCache()
    {
        ModsTableModel model;
        model.setModules({
            makeMod(QStringLiteral("ModA"), QStringLiteral("Alpha"), QStringLiteral("1.0")),
            makeMod(QStringLiteral("ModB"), QStringLiteral("Beta"), QStringLiteral("1.0")),
        });
        ModsFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        // 测试环境无已安装状态 → 全部 NotInstalled（缓存路径）
        proxy.setStatusFilter(static_cast<int>(ModsTableModel::NotInstalled));
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setStatusFilter(static_cast<int>(ModsTableModel::Installed));
        QCOMPARE(proxy.rowCount(), 0);
        proxy.setStatusFilter(-1);
        QCOMPARE(proxy.rowCount(), 2);

        // refreshStatus 重建缓存不破坏行数与状态语义
        model.refreshStatus();
        QCOMPARE(model.statusAt(0), ModsTableModel::NotInstalled);
        QCOMPARE(proxy.rowCount(), 2);
    }

    void compatibilityFilterWithGameVersionAndRange()
    {
        ckan::CkanModule a = makeMod(QStringLiteral("A"), QStringLiteral("A Mod"), QStringLiteral("1.0"));
        a.kspVersionMin = QStringLiteral("1.12");
        a.kspVersionMax = QStringLiteral("1.12");
        const ckan::CkanModule b = makeMod(QStringLiteral("B"), QStringLiteral("B Mod"), QStringLiteral("1.0"));

        ModsTableModel model;
        model.setModules({ a, b });
        ModsFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        // 未设置游戏版本 → 无效版本按兼容处理，全部显示
        QCOMPARE(proxy.rowCount(), 2);
        // 设置 1.8：A(1.12 限定) 不兼容默认隐藏，仅 B
        proxy.setGameVersion(ckan::GameVersion(1, 8, 0));
        QCOMPARE(proxy.rowCount(), 1);
        // 显示不兼容 → A 恢复
        proxy.setShowIncompatible(true);
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setShowIncompatible(false);
        // 勾选覆盖 1.12 的额外区间 → A 经区间判定恢复显示（兼容缓存随上下文重建）
        proxy.setCompatRange(ckan::GameVersionRange(ckan::GameVersion(1, 12, 0), true,
                                                    ckan::GameVersion(1, 12, 0), true));
        QCOMPARE(proxy.rowCount(), 2);
    }
};

// KSP 崩溃日志分析：尾部解析判定硬崩溃/OOM（纯逻辑，一 feature 一测试）
class TestPlayerLogAnalyzer : public QObject
{
    Q_OBJECT
private slots:
    void noCrashReturnsNoCrash()
    {
        using playerlog::PlayerLogAnalysis;
        // 正常日志尾部不应出现崩溃痕迹
        const PlayerLogAnalysis r =
            playerlog::analyzePlayerLog(QStringLiteral("Loading texture...\nDone loading.\nShutting down."));
        QCOMPARE(r.kind, PlayerLogAnalysis::NoCrash);
        QCOMPARE(r.signo, -1);
    }

    void emptyContentIsNoCrash()
    {
        using playerlog::PlayerLogAnalysis;
        QCOMPARE(playerlog::analyzePlayerLog(QString()).kind, PlayerLogAnalysis::NoCrash);
    }

    void segfaultSigno11Detected()
    {
        using playerlog::PlayerLogAnalysis;
        const QString tail = QStringLiteral(
            "Something happened.\n"
            "Caught fatal signal - signo:11 code:1 errno:0 addr:0xf0\n"
            "Obtained 15 stack frames.");
        const PlayerLogAnalysis r = playerlog::analyzePlayerLog(tail);
        QCOMPARE(r.kind, PlayerLogAnalysis::HardCrash);
        QCOMPARE(r.signo, 11);
    }

    void abortSigno6Detected()
    {
        using playerlog::PlayerLogAnalysis;
        const PlayerLogAnalysis r = playerlog::analyzePlayerLog(
            QStringLiteral("Assertion failed...\nCaught fatal signal - signo:6 code:0"));
        QCOMPARE(r.kind, PlayerLogAnalysis::HardCrash);
        QCOMPARE(r.signo, 6);
    }

    void outOfMemoryExceptionDetected()
    {
        using playerlog::PlayerLogAnalysis;
        const PlayerLogAnalysis r = playerlog::analyzePlayerLog(
            QStringLiteral("OutOfMemoryException: Out of memory trying to allocate 4096 bytes"));
        QCOMPARE(r.kind, PlayerLogAnalysis::OutOfMemory);
        // OOM 优先于信号，即使同段出现 signo 也判为内存溢出
    }

    void outOfMemoryPrioritizedOverSignal()
    {
        using playerlog::PlayerLogAnalysis;
        const PlayerLogAnalysis r = playerlog::analyzePlayerLog(
            QStringLiteral("OutOfMemoryException: Out of memory\nCaught fatal signal - signo:11"));
        QCOMPARE(r.kind, PlayerLogAnalysis::OutOfMemory);
    }

    void missingFileReturnsLogMissing()
    {
        using playerlog::PlayerLogAnalysis;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // 不存在的文件
        QCOMPARE(playerlog::analyzePlayerLogFile(dir.filePath(QStringLiteral("nope.log"))).kind,
                 PlayerLogAnalysis::LogMissing);
    }

    void fileTailReadDetectsCrashAtEnd()
    {
        using playerlog::PlayerLogAnalysis;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("Player.log"));
        // 前面填充大量无意义内容，崩溃痕迹在文件末尾
        QString content = QString(100000, QLatin1Char('x'));
        content += QStringLiteral("\nCaught fatal signal - signo:8 code:1");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content.toUtf8());
        f.close();

        const PlayerLogAnalysis r = playerlog::analyzePlayerLogFile(path); // 默认只读尾部 512KB
        QCOMPARE(r.kind, PlayerLogAnalysis::HardCrash);
        QCOMPARE(r.signo, 8);
    }

    void describeSignoHasChineseMeaning()
    {
        // 常见崩溃信号应有中文释义；未知信号给出通用提示
        QVERIFY(playerlog::describeSigno(11).contains(QStringLiteral("段错误")));
        QVERIFY(playerlog::describeSigno(6).contains(QStringLiteral("异常终止")));
        QVERIFY(playerlog::describeSigno(999).contains(QStringLiteral("signo:999")));
    }
};

// CKanManager 退出清理（回归：关闭启动器后进程残留、registry.locked 被残留进程
// 占用，导致重开启动器进模组管理一直提示被锁）。closeInstance() 在后台扫描在途时
// 调用必须能取消等待并释放注册表锁，且幂等、未绑定实例时安全。
class TestShutdownCleanup : public QObject
{
    Q_OBJECT
private slots:
    void closeInstanceCancelsScanAndReleasesLock()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("GameData"))));
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("CKAN"))));
        QFile exe(dir.filePath(QStringLiteral("KSP_x64.exe")));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();

        CKanManager &mgr = CKanManager::instance();
        mgr.openInstance(dir.path(), QStringLiteral("shutdown-test"));
        const QString lockPath = dir.filePath(QStringLiteral("CKAN/registry.locked"));
        QVERIFY(mgr.tryAcquireRegistryLock());  // 本进程持有注册表锁
        QVERIFY(QFile::exists(lockPath));

        mgr.scanUnmanagedDllsAsync(true);       // 后台扫描在途
        mgr.closeInstance();                    // 取消并等待 + 释放锁（不得卡死）

        QVERIFY(!QFile::exists(lockPath));     // 锁已随实例释放
        mgr.closeInstance();                    // 幂等：再次调用安全

        // 重新打开后可再次取锁（前一次已彻底释放，无残留占用）
        mgr.openInstance(dir.path(), QStringLiteral("shutdown-test2"));
        QVERIFY(mgr.tryAcquireRegistryLock());
        mgr.closeInstance();
        QVERIFY(!QFile::exists(lockPath));
    }

    void closeInstanceWithoutInstanceIsSafe()
    {
        CKanManager::instance().closeInstance(); // 未绑定实例：无操作、不崩溃
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    TestSteamDiscovery tSteamDisc;
    failures += QTest::qExec(&tSteamDisc, argc, argv);
    TestValidKSPPath tValid;
    failures += QTest::qExec(&tValid, argc, argv);
    TestLaunchOptions tLaunchOpts;
    failures += QTest::qExec(&tLaunchOpts, argc, argv);
    TestUpdaterManager tUpdater;
    failures += QTest::qExec(&tUpdater, argc, argv);
    TestInstanceIconSource tIconSrc;
    failures += QTest::qExec(&tIconSrc, argc, argv);
    TestIndexService tIndex;
    failures += QTest::qExec(&tIndex, argc, argv);
    TestCacheService tCache;
    failures += QTest::qExec(&tCache, argc, argv);
    TestInstallService tInstall;
    failures += QTest::qExec(&tInstall, argc, argv);
    TestModsTableFilter tModsFilter;
    failures += QTest::qExec(&tModsFilter, argc, argv);
    TestPlayerLogAnalyzer tPlayerLog;
    failures += QTest::qExec(&tPlayerLog, argc, argv);
    TestShutdownCleanup tShutdown;
    failures += QTest::qExec(&tShutdown, argc, argv);
    return failures;
}

#include "test_launcher.moc"
