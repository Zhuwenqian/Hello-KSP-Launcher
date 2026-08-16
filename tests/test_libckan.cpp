#include <QtTest/QtTest>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QFileInfo>

#include "miniz.h"

#include "ckan/version.h"
#include "ckan/relationship.h"
#include "ckan/ckanmodule.h"
#include "ckan/moduleinstalldescriptor.h"
#include "ckan/moduleinstaller.h"
#include "ckan/installedmodule.h"
#include "ckan/registry.h"
#include "ckan/gameinstance.h"
#include "ckan/relationshipresolver.h"
#include "ckan/downloader.h"
#include "ckan/repoindex.h"

using namespace ckan;

static Relationship rel(const QString &name, const QString &minVer = QString(),
                        const QString &maxVer = QString(), bool minIncl = true, bool maxIncl = true)
{
    Relationship r;
    r.name = name;
    r.minVersion = minVer;
    r.maxVersion = maxVer;
    r.minInclusive = minIncl;
    r.maxInclusive = maxIncl;
    return r;
}

static Relationship dep(const QString &name, const QString &minVer = QString())
{
    Relationship r = rel(name, minVer);
    r.type = Relationship::Type::Depends;
    return r;
}

static Relationship prov(const QString &name)
{
    Relationship r;
    r.type = Relationship::Type::Provides;
    r.name = name;
    return r;
}

static CkanModule makeModule(const QString &id, const QString &version,
                             const QVector<Relationship> &depends = {},
                             const QVector<Relationship> &recommends = {},
                             const QVector<Relationship> &conflicts = {},
                             const QVector<Relationship> &provides = {})
{
    CkanModule m;
    m.identifier = id;
    m.name = id;
    m.version = version;
    m.depends = depends;
    m.recommends = recommends;
    m.conflicts = conflicts;
    m.provides = provides;
    return m;
}

static QMap<QString, QVector<CkanModule>> makeIndex(const QVector<CkanModule> &mods)
{
    QMap<QString, QVector<CkanModule>> idx;
    for (const CkanModule &m : mods)
        idx[m.identifier].append(m);
    return idx;
}

// 用 miniz 构造一个内存 zip（供安装测试使用）
static QByteArray makeZip(const QList<QPair<QString, QByteArray>> &files)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    mz_zip_writer_init_heap(&zip, 0, 0);
    for (const auto &f : files)
        mz_zip_writer_add_mem(&zip, f.first.toUtf8().constData(),
                              f.second.constData(), f.second.size(), 0);
    size_t size = 0;
    void *buf = nullptr;
    mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
    QByteArray out(static_cast<const char *>(buf), static_cast<int>(size));
    mz_free(buf);
    return out;
}

// ---------------------------------------------------------------------------
// 版本
// ---------------------------------------------------------------------------
class TestModuleVersion : public QObject
{
    Q_OBJECT
private slots:
    void basicOrdering()
    {
        QVERIFY(ModuleVersion(QStringLiteral("1.0")) < ModuleVersion(QStringLiteral("2.0")));
        QVERIFY(ModuleVersion(QStringLiteral("1.10")) > ModuleVersion(QStringLiteral("1.9")));
        QVERIFY(ModuleVersion(QStringLiteral("1.2.3")) == ModuleVersion(QStringLiteral("1.2.3")));
        QVERIFY(ModuleVersion(QStringLiteral("1.0.0")) > ModuleVersion(QStringLiteral("1.0")));
    }
    void epoch()
    {
        QVERIFY(ModuleVersion(QStringLiteral("1:1.0")) > ModuleVersion(QStringLiteral("0:5.0")));
        QCOMPARE(ModuleVersion(QStringLiteral("2:1.0")).epoch(), 2);
    }
    void validity()
    {
        QVERIFY(ModuleVersion(QStringLiteral("1.0")).isValid());
    }
};

class TestGameVersion : public QObject
{
    Q_OBJECT
private slots:
    void parse()
    {
        const GameVersion v(QStringLiteral("1.12.3"));
        QVERIFY(v.isValid());
        QCOMPARE(v.major(), 1);
        QCOMPARE(v.minor(), 12);
        QCOMPARE(v.patch(), 3);
        QCOMPARE(v.build(), 0);
        QCOMPARE(v.toString(), QStringLiteral("1.12.3"));
    }
    void parseBuild()
    {
        const GameVersion v(QStringLiteral("1.12.3.1234"));
        QVERIFY(v.isValid());
        QCOMPARE(v.build(), 1234);
    }
    void invalid()
    {
        QVERIFY(!GameVersion(QStringLiteral("abc")).isValid());
        QVERIFY(!GameVersion(QString()).isValid());
    }
    void ordering()
    {
        QVERIFY(GameVersion(QStringLiteral("1.12.3")) < GameVersion(QStringLiteral("1.12.4")));
        QVERIFY(GameVersion(QStringLiteral("1.12.3")) < GameVersion(QStringLiteral("1.13.0")));
        QVERIFY(GameVersion(QStringLiteral("1.12.3")) == GameVersion(QStringLiteral("1.12.3.0")));
    }
};

// ---------------------------------------------------------------------------
// 关系约束
// ---------------------------------------------------------------------------
class TestRelationship : public QObject
{
    Q_OBJECT
private slots:
    void unconstrained()
    {
        Relationship r;
        r.name = QStringLiteral("foo");
        QVERIFY(r.versionSatisfies(QStringLiteral("any.thing")));
    }
    void minInclusive()
    {
        Relationship r = rel(QStringLiteral("foo"), QStringLiteral("1.0"));
        QVERIFY(r.versionSatisfies(QStringLiteral("1.0")));
        QVERIFY(r.versionSatisfies(QStringLiteral("2.0")));
        QVERIFY(!r.versionSatisfies(QStringLiteral("0.9")));
    }
    void minExclusive()
    {
        Relationship r = rel(QStringLiteral("foo"), QStringLiteral("1.0"), {}, false, true);
        QVERIFY(!r.versionSatisfies(QStringLiteral("1.0")));
        QVERIFY(r.versionSatisfies(QStringLiteral("1.1")));
    }
    void maxInclusive()
    {
        Relationship r = rel(QStringLiteral("foo"), {}, QStringLiteral("2.0"), true, true);
        QVERIFY(r.versionSatisfies(QStringLiteral("2.0")));
        QVERIFY(!r.versionSatisfies(QStringLiteral("2.1")));
    }
};

// ---------------------------------------------------------------------------
// 模块元数据
// ---------------------------------------------------------------------------
class TestCkanModule : public QObject
{
    Q_OBJECT
private slots:
    void fromJson()
    {
        const QByteArray json =
            "{\"identifier\":\"ModA\",\"name\":\"Mod A\",\"version\":\"1.2.3\","
            "\"ksp_version\":\"1.12.3\",\"depends\":[\"DepMod\"],"
            "\"provides\":[\"virtual-a\"]}";
        QString err;
        const CkanModule m = CkanModule::fromJson(json, &err);
        QVERIFY(m.isValid());
        QCOMPARE(m.identifier, QStringLiteral("ModA"));
        QCOMPARE(m.version, QStringLiteral("1.2.3"));
        QCOMPARE(m.depends.size(), 1);
        QCOMPARE(m.depends.at(0).name, QStringLiteral("DepMod"));
        QVERIFY(m.providesList().contains(QStringLiteral("virtual-a")));
        QVERIFY(m.providesList().contains(QStringLiteral("ModA")));
    }
    void fromJsonMissingIdentifier()
    {
        QString err;
        const CkanModule m = CkanModule::fromJson("{\"version\":\"1.0\"}", &err);
        QVERIFY(!m.isValid());
        QVERIFY(!err.isEmpty());
    }
    void fromJsonMissingVersion()
    {
        QString err;
        const CkanModule m = CkanModule::fromJson("{\"identifier\":\"ModA\"}", &err);
        QVERIFY(!m.isValid());
        QVERIFY(!err.isEmpty());
    }
    void compatible()
    {
        // 新规则：只看中版本门槛（固定 1.9.0，中版本 >= 9 兼容），不依赖当前 KSP 版本
        CkanModule m = makeModule(QStringLiteral("ModA"), QStringLiteral("1.0"));
        m.kspVersion = QStringLiteral("1.12.3"); // 中版本 12 >= 9
        QVERIFY(m.isCompatible(GameVersion(QStringLiteral("1.12.3"))));
        QVERIFY(m.isCompatible(GameVersion(QStringLiteral("2.0.0"))));
        QVERIFY(m.isCompatible(GameVersion(QStringLiteral("1.12.3.220"))));
        QVERIFY(m.isCompatible(GameVersion())); // 参数不参与判断

        m.kspVersion = QStringLiteral("1.9.0"); // 中版本 9 == 门槛，兼容
        QVERIFY(m.isCompatible(GameVersion(QStringLiteral("1.12.3"))));

        m.kspVersion = QStringLiteral("1.8.1"); // 中版本 8 < 9，不兼容
        QVERIFY(!m.isCompatible(GameVersion(QStringLiteral("1.12.3"))));

        m.kspVersion = QString(); // 未声明 ksp_version 视为兼容
        QVERIFY(m.isCompatible(GameVersion(QStringLiteral("1.12.3"))));

        m.kspVersion = QStringLiteral("not-a-version"); // 非法 ksp_version 视为兼容
        QVERIFY(m.isCompatible(GameVersion(QStringLiteral("1.12.3"))));
    }
    void effectiveInstallDefault()
    {
        const CkanModule m = makeModule(QStringLiteral("ModA"), QStringLiteral("1.0"));
        const QVector<ModuleInstallDescriptor> stanzas = m.effectiveInstallStanzas();
        QCOMPARE(stanzas.size(), 1);
        QCOMPARE(stanzas.at(0).find, QStringLiteral("ModA"));
        QCOMPARE(stanzas.at(0).installTo, QStringLiteral("GameData"));
    }
};

// ---------------------------------------------------------------------------
// 安装规则
// ---------------------------------------------------------------------------
class TestModuleInstallDescriptor : public QObject
{
    Q_OBJECT
private slots:
    void defaultStanza()
    {
        const ModuleInstallDescriptor d = ModuleInstallDescriptor::defaultStanza(QStringLiteral("ModA"));
        QCOMPARE(d.find, QStringLiteral("ModA"));
        QCOMPARE(d.installTo, QStringLiteral("GameData"));
    }
    void fromJsonValid()
    {
        ModuleInstallDescriptor d;
        QString err;
        QVERIFY(ModuleInstallDescriptor::fromJsonObject(
            QJsonObject{{QStringLiteral("find"), QStringLiteral("ModA")}},
            &d, &err));
        QCOMPARE(d.find, QStringLiteral("ModA"));
    }
    void fromJsonConflict()
    {
        ModuleInstallDescriptor d;
        QString err;
        QVERIFY(!ModuleInstallDescriptor::fromJsonObject(
            QJsonObject{{QStringLiteral("file"), QStringLiteral("a")},
                        {QStringLiteral("find"), QStringLiteral("b")}},
            &d, &err));
        QVERIFY(!err.isEmpty());
    }
    void findInstallable()
    {
        ModuleInstallDescriptor d;
        QString err;
        QVERIFY(ModuleInstallDescriptor::fromJsonObject(
            QJsonObject{{QStringLiteral("find"), QStringLiteral("ModA")}},
            &d, &err));
        const QStringList entries = {QStringLiteral("ModA/a.dll"),
                                     QStringLiteral("ModA/ModA.ckan"),
                                     QStringLiteral("Other/x.txt")};
        const QVector<InstallableFile> files = d.findInstallableFiles(entries,
                                                                      QStringLiteral("GameData"),
                                                                      &err);
        // 只匹配 ModA/a.dll，跳过 .ckan 与无关文件
        QCOMPARE(files.size(), 1);
        QCOMPARE(files.at(0).sourceName, QStringLiteral("ModA/a.dll"));
        QVERIFY(files.at(0).destination.startsWith(QStringLiteral("GameData/")));
    }
    void safeCacheFileName()
    {
        // version 带 epoch（如 "1:3.4.0"）：冒号在 Windows 上会导致 ADS 读写错位，
        // 清洗后应得到一个可安全用作文件名的小写安全串。
        QCOMPARE(ModuleInstaller::safeCacheFileName(QStringLiteral("1:3.4.0")),
                 QStringLiteral("1_3.4.0"));
        QCOMPARE(ModuleInstaller::safeCacheFileName(QStringLiteral("2:1.0")),
                 QStringLiteral("2_1.0"));
        // 普通版本号原样保留
        QCOMPARE(ModuleInstaller::safeCacheFileName(QStringLiteral("2.21.0.4")),
                 QStringLiteral("2.21.0.4"));
        // 其余非法字符一并清洗
        QCOMPARE(ModuleInstaller::safeCacheFileName(QStringLiteral("a:b\\c/d?e*f<g>h|i\"j")),
                 QStringLiteral("a_b_c_d_e_f_g_h_i_j"));
    }
    void actualGameDataFolders_data()
    {
        QTest::addColumn<QByteArray>("zip");
        QTest::addColumn<QJsonArray>("install");
        QTest::addColumn<QStringList>("expected");

        // 默认规则：find=identifier, install_to=GameData -> 顶层文件夹 = 标识符
        QTest::newRow("default") << makeZip({{QStringLiteral("SomeMod/a.dll"), QByteArray("dll")}})
                                 << QJsonArray{}
                                 << QStringList({QStringLiteral("SomeMod")});

        // as 重命名优先：zip 里是 Source/foo，目标是 GameData/Renamed/foo
        QTest::newRow("asRename") << makeZip({{QStringLiteral("Source/foo.txt"), QByteArray("x")}})
                                  << QJsonArray{QJsonObject{{QStringLiteral("find"), QStringLiteral("Source")},
                                                            {QStringLiteral("as"),     QStringLiteral("Renamed")}}}
                                  << QStringList({QStringLiteral("Renamed")});

        // 嵌套 install_to：GameData/Sub -> 顶层文件夹 = Sub
        QTest::newRow("nested") << makeZip({{QStringLiteral("Plugin/AB_Data/a.dll"), QByteArray("dll")}})
                                << QJsonArray{QJsonObject{{QStringLiteral("file"), QStringLiteral("Plugin/AB_Data")},
                                                          {QStringLiteral("install_to"), QStringLiteral("GameData/Sub")}}}
                                << QStringList({QStringLiteral("Sub")});

        // 多规则：同时写入两个不同顶层文件夹
        QTest::newRow("multiFolders") << makeZip({{QStringLiteral("A/a.dll"), QByteArray("dll")},
                                                  {QStringLiteral("B/b.dll"), QByteArray("dll")}})
                                      << QJsonArray{QJsonObject{{QStringLiteral("find"), QStringLiteral("A")}},
                                                    QJsonObject{{QStringLiteral("find"), QStringLiteral("B")}}}
                                      << QStringList({QStringLiteral("A"), QStringLiteral("B")});

        // 非 GameData 目标忽略
        QTest::newRow("nonGameData") << makeZip({{QStringLiteral("Ships/MyShip/ship.craft"), QByteArray("x")}})
                                     << QJsonArray{QJsonObject{{QStringLiteral("file"), QStringLiteral("Ships/MyShip")},
                                                               {QStringLiteral("install_to"), QStringLiteral("Ships")}}}
                                     << QStringList{};
    }
    void actualGameDataFolders()
    {
        QFETCH(QByteArray, zip);
        QFETCH(QJsonArray, install);
        QFETCH(QStringList, expected);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString zipPath = dir.filePath(QStringLiteral("mod.zip"));
        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::WriteOnly));
        zipFile.write(zip);
        zipFile.close();

        CkanModule mod = makeModule(QStringLiteral("SomeMod"), QStringLiteral("1.0"));
        if (!install.isEmpty()) {
            QVector<ModuleInstallDescriptor> stanzas;
            for (const QJsonValue &v : install) {
                ModuleInstallDescriptor d;
                QString err;
                QVERIFY2(ModuleInstallDescriptor::fromJsonObject(v.toObject(), &d, &err),
                         qPrintable(err));
                stanzas.append(d);
            }
            mod.install = stanzas;
        }

        QString err;
        const QStringList folders = ModuleInstaller::actualGameDataFolders(zipPath, mod, &err);
        QCOMPARE(err, QString());
        QCOMPARE(folders, expected);
    }
};

// ---------------------------------------------------------------------------
// 注册表
// ---------------------------------------------------------------------------
class TestRegistry : public QObject
{
    Q_OBJECT
private slots:
    void roundtrip()
    {
        Registry reg;
        const CkanModule m = makeModule(QStringLiteral("ModA"), QStringLiteral("1.2.3"));
        InstalledModule im;
        im.identifier = QStringLiteral("ModA");
        im.module = m;
        im.files = {QStringLiteral("GameData/ModA/a.dll"),
                    QStringLiteral("GameData/ModA/b.dll")};
        reg.registerModule(im);

        const QByteArray json = reg.toJson();
        Registry reg2 = Registry::fromJson(json);
        QVERIFY(reg2.isValid());
        QVERIFY(reg2.isInstalled(QStringLiteral("ModA")));
        QCOMPARE(reg2.installedVersion(QStringLiteral("ModA")), QStringLiteral("1.2.3"));
        QCOMPARE(reg2.fileOwner(QStringLiteral("GameData/ModA/a.dll")),
                 QStringLiteral("ModA"));
    }
    void unregister()
    {
        Registry reg;
        const CkanModule m = makeModule(QStringLiteral("ModA"), QStringLiteral("1.0"));
        InstalledModule im;
        im.identifier = QStringLiteral("ModA");
        im.module = m;
        im.files = {QStringLiteral("GameData/ModA/a.dll")};
        reg.registerModule(im);
        reg.unregisterModule(QStringLiteral("ModA"));
        QVERIFY(!reg.isInstalled(QStringLiteral("ModA")));
        QVERIFY(reg.fileOwner(QStringLiteral("GameData/ModA/a.dll")).isEmpty());
    }
    void invalidJson()
    {
        QString err;
        const Registry reg = Registry::fromJson("not json", &err);
        QVERIFY(!err.isEmpty());
    }
    void licenseRoundtrip()
    {
        // 注册表序列化必须保留 license 必填字段，供官方 CKAN 校验
        CkanModule m = makeModule(QStringLiteral("ModA"), QStringLiteral("1.2.3"));
        m.license = {QStringLiteral("MIT"), QStringLiteral("GPL-3.0")};
        m.author = {QStringLiteral("Alice")};
        m.kspVersion = QStringLiteral("1.12.3");
        InstalledModule im;
        im.identifier = QStringLiteral("ModA");
        im.module = m;
        im.files = {QStringLiteral("GameData/ModA/a.dll")};
        Registry reg;
        reg.registerModule(im);

        const QByteArray json = reg.toJson();
        QVERIFY(json.contains("\"license\""));
        QVERIFY(json.contains("\"MIT\""));

        const Registry reg2 = Registry::fromJson(json);
        QVERIFY(reg2.isValid());
        const InstalledModule *im2 = reg2.installed(QStringLiteral("ModA"));
        QVERIFY(im2);
        QCOMPARE(im2->module.license.size(), 2);
        QCOMPARE(im2->module.license[0], QStringLiteral("MIT"));
        QCOMPARE(im2->module.license[1], QStringLiteral("GPL-3.0"));
        QCOMPARE(im2->module.author.size(), 1);
        QCOMPARE(im2->module.author[0], QStringLiteral("Alice"));
        QCOMPARE(im2->module.kspVersion, QStringLiteral("1.12.3"));
    }
};

// ---------------------------------------------------------------------------
// 依赖解析
// ---------------------------------------------------------------------------
class TestRelationshipResolver : public QObject
{
    Q_OBJECT
private slots:
    void simpleDep()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("B"))});
        const CkanModule B = makeModule(QStringLiteral("B"), QStringLiteral("1.0"));
        const auto idx = makeIndex({A, B});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QVERIFY(!r.missing);
        QVERIFY(!r.conflicted);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(ids.contains(QStringLiteral("A")));
        QVERIFY(ids.contains(QStringLiteral("B")));
    }
    void virtualDep()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("virtual-lib"))});
        const CkanModule P = makeModule(QStringLiteral("P"), QStringLiteral("1.0"),
                                        {}, {}, {}, {prov(QStringLiteral("virtual-lib"))});
        const auto idx = makeIndex({A, P});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QVERIFY(!r.missing);
        QVERIFY(!r.conflicted);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(ids.contains(QStringLiteral("A")));
        QVERIFY(ids.contains(QStringLiteral("P")));
    }
    void missingDep()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("Nope"))});
        const auto idx = makeIndex({A});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QVERIFY(r.missing);
        QVERIFY(r.notFound.contains(QStringLiteral("Nope")));
    }
    void conflict()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"));
        const CkanModule B = makeModule(QStringLiteral("B"), QStringLiteral("1.0"),
                                        {}, {}, {dep(QStringLiteral("A"))});
        const auto idx = makeIndex({A, B});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A, B}, reg, false);
        QVERIFY(r.conflicted);
        QVERIFY(!r.conflicts.isEmpty());
    }
    void unionSharedDependency()
    {
        // 批量安装：两个待装模组共享同一依赖，应只安装一份依赖
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("S"))});
        const CkanModule B = makeModule(QStringLiteral("B"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("S"))});
        const CkanModule S = makeModule(QStringLiteral("S"), QStringLiteral("1.0"));
        const auto idx = makeIndex({A, B, S});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A, B}, reg, false);
        QVERIFY(!r.missing);
        QVERIFY(!r.conflicted);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(ids.contains(QStringLiteral("A")));
        QVERIFY(ids.contains(QStringLiteral("B")));
        QVERIFY(ids.contains(QStringLiteral("S")));
    }
    void recommendsAutoInstalled()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {}, {dep(QStringLiteral("R"))});
        const CkanModule R = makeModule(QStringLiteral("R"), QStringLiteral("1.0"));
        const auto idx = makeIndex({A, R});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, true);
        QVERIFY(!r.missing);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(ids.contains(QStringLiteral("R")));
    }
    void recommendsSkippedWithoutAuto()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {}, {dep(QStringLiteral("R"))});
        const CkanModule R = makeModule(QStringLiteral("R"), QStringLiteral("1.0"));
        const auto idx = makeIndex({A, R});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(!ids.contains(QStringLiteral("R")));
    }
    void versionConstraintPicksBest()
    {
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("B"), QStringLiteral("2.0"))});
        QMap<QString, QVector<CkanModule>> idx;
        idx[QStringLiteral("A")] = {A};
        idx[QStringLiteral("B")] = {makeModule(QStringLiteral("B"), QStringLiteral("1.0")),
                                    makeModule(QStringLiteral("B"), QStringLiteral("2.0")),
                                    makeModule(QStringLiteral("B"), QStringLiteral("3.0"))};
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QVERIFY(!r.missing);
        bool found300 = false;
        for (const CkanModule &m : r.modulesToInstall)
            if (m.identifier == QStringLiteral("B") && m.version == QStringLiteral("3.0"))
                found300 = true;
        QVERIFY(found300);
    }
    void adDependencySatisfied()
    {
        // A 依赖 FooLib，而 FooLib 是手动安装（DLL 扫描）的 AD 模组：
        // 应视为已满足，不下载 FooLib，也不报缺失。
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("FooLib"))});
        const CkanModule FooLib = makeModule(QStringLiteral("FooLib"), QStringLiteral("1.0"));
        const auto idx = makeIndex({A, FooLib});
        RelationshipResolver resolver(idx);
        Registry reg;
        reg.installedDlls[QStringLiteral("FooLib")] = QStringLiteral("GameData/FooLib/FooLib.dll");
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QVERIFY(!r.missing);
        QVERIFY(!r.conflicted);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(ids.contains(QStringLiteral("A")));
        QVERIFY(!ids.contains(QStringLiteral("FooLib"))); // 依赖已被 AD 满足，不下载
    }
    void nonAdDependencyStillDownloaded()
    {
        // 依赖不是 AD 模组时，仍应进入待安装集合
        const CkanModule A = makeModule(QStringLiteral("A"), QStringLiteral("1.0"),
                                        {dep(QStringLiteral("B"))});
        const CkanModule B = makeModule(QStringLiteral("B"), QStringLiteral("1.0"));
        const auto idx = makeIndex({A, B});
        RelationshipResolver resolver(idx);
        Registry reg;
        const ResolutionResult r = resolver.resolve({A}, reg, false);
        QVERIFY(!r.missing);
        QSet<QString> ids;
        for (const CkanModule &m : r.modulesToInstall) ids.insert(m.identifier);
        QVERIFY(ids.contains(QStringLiteral("B")));
    }
};

// ---------------------------------------------------------------------------
// GameData DLL 扫描
// ---------------------------------------------------------------------------
class TestGameInstance : public QObject
{
    Q_OBJECT
private slots:
    void scanIgnoresStockAndDeduplicates()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto mkfile = [&](const QString &rel, const QByteArray &content) {
            const QString abs = dir.filePath(rel);
            QDir().mkpath(QFileInfo(abs).absolutePath());
            QFile f(abs);
            QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(rel));
            f.write(content);
            f.close();
        };

        // KSP 官方目录（应排除）
        mkfile(QStringLiteral("GameData/Squad/StockDll.dll"), "x");
        mkfile(QStringLiteral("GameData/SquadExpansion/EasterEggs/egg.dll"), "x");

        // 手动模组 DLL
        mkfile(QStringLiteral("GameData/ModA/ModA.dll"), "x");
        mkfile(QStringLiteral("GameData/SomePath/MyMod.Core.dll"), "x"); // 标识符取文件名第一个点之前
        mkfile(QStringLiteral("GameData/MyMod/MyMod.Core-KSP.dll"), "x"); // 与上面应去重为一个 MyMod

        GameInstance gi(dir.path(), QStringLiteral("test"));
        const QMap<QString, QString> dlls = gi.scanUnmanagedDlls();

        QCOMPARE(dlls.size(), 2);
        QVERIFY(dlls.contains(QStringLiteral("ModA")));
        QCOMPARE(dlls.value(QStringLiteral("ModA")),
                 QStringLiteral("GameData/ModA/ModA.dll"));
        QVERIFY(dlls.contains(QStringLiteral("MyMod")));
        // 官方目录的 DLL 不应出现
        QVERIFY(!dlls.contains(QStringLiteral("StockDll")));
        QVERIFY(!dlls.contains(QStringLiteral("egg")));
    }

    void scanMissingGameDataReturnsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GameInstance gi(dir.path(), QStringLiteral("test"));
        QVERIFY(gi.scanUnmanagedDlls().isEmpty());
    }

    void uninstallCascade()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GameInstance gi(dir.path(), QStringLiteral("test"));

        // 依赖链：C 依赖 B，B 依赖 A，外加独立的 D
        auto registerMod = [&](const CkanModule &m) {
            InstalledModule im;
            im.identifier = m.identifier;
            im.module = m;
            im.files = {QStringLiteral("GameData/%1/x.dll").arg(m.identifier)};
            gi.registry()->registerModule(im);
        };
        registerMod(makeModule(QStringLiteral("A"), QStringLiteral("1.0")));
        registerMod(makeModule(QStringLiteral("B"), QStringLiteral("1.0"), {dep(QStringLiteral("A"))}));
        registerMod(makeModule(QStringLiteral("C"), QStringLiteral("1.0"), {dep(QStringLiteral("B"))}));
        registerMod(makeModule(QStringLiteral("D"), QStringLiteral("1.0")));

        ModuleInstaller installer(&gi);
        const InstallResult r = installer.uninstall(QStringLiteral("A"));
        QVERIFY(r.ok);
        // 卸载顺序自外向内：先 C、B，最后 A
        QCOMPARE(r.installedIdentifiers,
                 QStringList({QStringLiteral("C"), QStringLiteral("B"), QStringLiteral("A")}));
        QVERIFY(!gi.registry()->isInstalled(QStringLiteral("A")));
        QVERIFY(!gi.registry()->isInstalled(QStringLiteral("B")));
        QVERIFY(!gi.registry()->isInstalled(QStringLiteral("C")));
        // 不依赖 A 的模组保留
        QVERIFY(gi.registry()->isInstalled(QStringLiteral("D")));
    }

    void uninstallNoDependentsOnlyTarget()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GameInstance gi(dir.path(), QStringLiteral("test"));
        InstalledModule im;
        im.identifier = QStringLiteral("D");
        im.module = makeModule(QStringLiteral("D"), QStringLiteral("1.0"));
        im.files = {QStringLiteral("GameData/D/x.dll")};
        gi.registry()->registerModule(im);

        ModuleInstaller installer(&gi);
        const InstallResult r = installer.uninstall(QStringLiteral("D"));
        QVERIFY(r.ok);
        QCOMPARE(r.installedIdentifiers, QStringList({QStringLiteral("D")}));
        QVERIFY(!gi.registry()->isInstalled(QStringLiteral("D")));
    }

    void manualGameDataFoldersDetected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto mkfile = [&](const QString &rel, const QByteArray &content) {
            const QString abs = dir.filePath(rel);
            QDir().mkpath(QFileInfo(abs).absolutePath());
            QFile f(abs);
            QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(rel));
            f.write(content);
            f.close();
        };
        // 官方目录（排除）
        mkfile(QStringLiteral("GameData/Squad/x.dll"), "x");
        // 手动占用（无 DLL 的纯配置/纹理模组）
        mkfile(QStringLiteral("GameData/ManualCfg/Config.cfg"), "x");
        // 已登记安装模组的文件夹（应排除）
        mkfile(QStringLiteral("GameData/Tracked/x.dll"), "x");

        GameInstance gi(dir.path(), QStringLiteral("test"));
        InstalledModule im;
        im.identifier = QStringLiteral("Tracked");
        im.module = makeModule(QStringLiteral("Tracked"), QStringLiteral("1.0"));
        im.files = {QStringLiteral("GameData/Tracked/x.dll")};
        gi.registry()->registerModule(im);

        const QStringList manual = gi.manualGameDataFolders();
        QCOMPARE(manual, QStringList({QStringLiteral("GameData/ManualCfg")}));
    }

    void installDeletesOldFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GameInstance gi(dir.path(), QStringLiteral("test"));

        // 已有手动占用的 GameData/ModA 文件夹（含一个额外的、新模组不提供的文件）
        const QString oldExtra = dir.filePath(QStringLiteral("GameData/ModA/legacy.cfg"));
        QDir().mkpath(QFileInfo(oldExtra).absolutePath());
        QFile fe(oldExtra);
        QVERIFY(fe.open(QIODevice::WriteOnly));
        fe.write("legacy");
        fe.close();

        // 构造 zip：ModA/a.dll
        const QByteArray zip = makeZip({qMakePair(QStringLiteral("ModA/a.dll"), QByteArray("dll"))});
        const QString dl = dir.filePath(QStringLiteral("dl"));
        QDir().mkpath(dl);
        QFile zf(dl + QStringLiteral("/ModA_1.0.zip"));
        QVERIFY(zf.open(QIODevice::WriteOnly));
        zf.write(zip);
        zf.close();

        CkanModule mod = makeModule(QStringLiteral("ModA"), QStringLiteral("1.0"));
        mod.downloadUrls = QStringList{QStringLiteral("file:///dummy/mod.zip")}; // 缓存已预置，此 URL 不会真正使用
        ModuleInstaller installer(&gi);
        // foldersToDelete = {"ModA"}：写入前应删除整个 GameData/ModA
        const InstallResult r = installer.install({mod}, dl, {QStringLiteral("ModA")});
        QVERIFY2(r.ok, qPrintable(r.error));
        // 旧的手动文件被删除
        QVERIFY(!QFileInfo::exists(oldExtra));
        // 新文件已安装
        QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("GameData/ModA/a.dll"))));
        // 已登记为安装
        QVERIFY(gi.registry()->isInstalled(QStringLiteral("ModA")));
    }
};

// ---------------------------------------------------------------------------
// 下载器：进度回调 / 超时 / 取消
// ---------------------------------------------------------------------------
class TestDownloader : public QObject
{
    Q_OBJECT
private slots:
    void fileDownloadProgress()
    {
        // 用本地文件验证 downloadProgressed 的成功路径与进度回调
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("payload.bin"));
        QByteArray payload;
        payload.resize(256 * 1024);
        for (int i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>(i & 0xff);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(payload);
        f.close();

        Downloader dl;
        QByteArray out;
        QString err;
        qint64 lastTotal = -1;
        std::atomic_bool cancel{false};
        const bool ok = dl.downloadProgressed(
            QUrl::fromLocalFile(path).toString(), {}, &out, &err, nullptr,
            [&](qint64, qint64 total) { lastTotal = total; }, &cancel);
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(out, payload);
        QCOMPARE(lastTotal, static_cast<qint64>(payload.size()));
    }

    void cancelFlagAbortsDownload()
    {
        // 预置取消标志：函数应返回失败并报告「已取消」
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("payload.bin"));
        QByteArray payload(1024 * 1024, 'x');
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(payload);
        f.close();

        Downloader dl;
        QByteArray out;
        QString err;
        std::atomic_bool cancel{true};
        const bool ok = dl.downloadProgressed(
            QUrl::fromLocalFile(path).toString(), {}, &out, &err, nullptr, nullptr, &cancel);
        QVERIFY(!ok);
        QCOMPARE(err, QStringLiteral("已取消"));
    }

    void resumeAfterConnectionClosed()
    {
        // 本地 HTTP 服务器：首次请求只发一半就断开（模拟 connection closed），
        // 后续 Range 请求返回 206 剩余部分。验证断点续传能拼出完整内容。
        class RangeServer : public QTcpServer
        {
        public:
            QByteArray payload;
            int fullRequests = 0;
            int rangeRequests = 0;
            void startListening() { listen(QHostAddress::LocalHost, 0); }
            quint16 port() const { return serverPort(); }

        protected:
            void incomingConnection(qintptr socketDescriptor) override
            {
                QTcpSocket *s = new QTcpSocket(this);
                s->setSocketDescriptor(socketDescriptor);
                connect(s, &QTcpSocket::readyRead, this, [this, s]() {
                    const QByteArray req = s->readAll();
                    if (!req.contains("\r\n\r\n"))
                        return; // 头未收全，等待
                    const int rangePos = req.indexOf("Range: bytes=");
                    if (rangePos >= 0) {
                        ++rangeRequests;
                        const int start = rangePos + 13;
                        int end = req.indexOf('\r', start);
                        QByteArray rr = req.mid(start, end - start);
                        // Qt 的 toLongLong 无法解析带尾随 '-' 的值（如 "153600-"），需先截取数字部分
                        const int dashIdx = rr.indexOf('-');
                        if (dashIdx >= 0)
                            rr.truncate(dashIdx);
                        qint64 offset = rr.toLongLong();
                        const QByteArray body = payload.mid(static_cast<int>(offset));
                        QByteArray resp;
                        resp += "HTTP/1.1 206 Partial Content\r\n";
                        resp += "Content-Range: bytes "
                              + QByteArray::number(offset) + "-"
                              + QByteArray::number(payload.size() - 1) + "/"
                              + QByteArray::number(payload.size()) + "\r\n";
                        resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
                        resp += "Connection: close\r\n\r\n";
                        resp += body;
                        s->write(resp);
                        s->flush();
                        s->disconnectFromHost();
                    } else {
                        ++fullRequests;
                        const int half = payload.size() / 2;
                        QByteArray resp;
                        resp += "HTTP/1.1 200 OK\r\n";
                        resp += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
                        resp += "Connection: close\r\n\r\n";
                        resp += payload.left(half);
                        s->write(resp);
                        s->flush();
                        s->disconnectFromHost(); // 发送一半后断开 → connection closed
                    }
                });
                connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
            }
        };

        RangeServer server;
        server.payload.resize(300 * 1024);
        for (int i = 0; i < server.payload.size(); ++i)
            server.payload[i] = static_cast<char>(i & 0xff);
        server.startListening();
        QVERIFY(server.isListening());

        Downloader dl;
        QByteArray out;
        QString err;
        const QString url = QStringLiteral("http://127.0.0.1:%1/mod.zip").arg(server.port());
        const bool ok = dl.downloadProgressed(url, {}, &out, &err, nullptr,
                                              nullptr, nullptr, /*resumeAttempts=*/5);
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(out, server.payload);
        QCOMPARE(server.fullRequests, 1);  // 首次全量请求
        QCOMPARE(server.rangeRequests, 1); // 续传一次取回剩余部分
    }
};

// ---------------------------------------------------------------------------
// 仓库索引：版本排序与最新版本选取（search() 依赖的逻辑）
// ---------------------------------------------------------------------------
class TestRepoIndex : public QObject
{
    Q_OBJECT
private slots:
    void latestPicksNewestNotAlphabeticalFirst()
    {
        // 回归：Kerbal Konstructs 这类含 v 前缀的版本，必须按版本取最新 v1.12.2.0，
        // 而不能像旧逻辑那样取 m_index 首个（tar 字母序 0.5.1b）版本。
        // 构造顺序故意模拟 tar 字母序：0.5.1b 排在最前。
        auto idx = makeIndex({
            makeModule(QStringLiteral("KerbalKonstructs"), QStringLiteral("0.5.1b")),
            makeModule(QStringLiteral("KerbalKonstructs"), QStringLiteral("1.8.1.15")),
            makeModule(QStringLiteral("KerbalKonstructs"), QStringLiteral("v1.12.2.0")),
        });
        const QVector<CkanModule> sorted =
            RepoIndex::versionsFor(idx, QStringLiteral("KerbalKonstructs"));
        QCOMPARE(sorted.size(), 3);
        QCOMPARE(sorted.first().version, QStringLiteral("v1.12.2.0"));
        QCOMPARE(RepoIndex::latestFor(idx, QStringLiteral("KerbalKonstructs")).version,
                 QStringLiteral("v1.12.2.0"));
    }

    void latestEmptyIndex()
    {
        const QMap<QString, QVector<CkanModule>> idx;
        QVERIFY(!RepoIndex::latestFor(idx, QStringLiteral("Missing")).isValid());
    }
};

static int runSuite(int argc, char *argv[], QObject &suite)
{
    return QTest::qExec(&suite, argc, argv);
}

int main(int argc, char *argv[])
{
    // 下载器依赖事件循环，需先创建 QCoreApplication
    QCoreApplication app(argc, argv);
    int failures = 0;
    TestModuleVersion tModVer;
    TestGameVersion tGameVer;
    TestRelationship tRel;
    TestCkanModule tMod;
    TestModuleInstallDescriptor tInstall;
    TestRegistry tReg;
    TestGameInstance tGameInst;
    TestRelationshipResolver tResolver;
    TestDownloader tDownloader;
    TestRepoIndex tRepoIndex;
    failures += runSuite(argc, argv, tModVer);
    failures += runSuite(argc, argv, tGameVer);
    failures += runSuite(argc, argv, tRel);
    failures += runSuite(argc, argv, tMod);
    failures += runSuite(argc, argv, tInstall);
    failures += runSuite(argc, argv, tReg);
    failures += runSuite(argc, argv, tGameInst);
    failures += runSuite(argc, argv, tResolver);
    failures += runSuite(argc, argv, tDownloader);
    failures += runSuite(argc, argv, tRepoIndex);
    return failures;
}

#include "test_libckan.moc"