#include <QtTest/QtTest>
#include <QSet>

#include "ckan/version.h"
#include "ckan/relationship.h"
#include "ckan/ckanmodule.h"
#include "ckan/moduleinstalldescriptor.h"
#include "ckan/installedmodule.h"
#include "ckan/registry.h"
#include "ckan/relationshipresolver.h"
#include "ckan/downloader.h"

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
        const CkanModule m = makeModule(QStringLiteral("ModA"), QStringLiteral("1.0"));
        CkanModule bounded = m;
        bounded.kspVersion = QStringLiteral("1.12.3");
        QVERIFY(bounded.isCompatible(GameVersion(QStringLiteral("1.12.3"))));
        QVERIFY(!bounded.isCompatible(GameVersion(QStringLiteral("2.0.0"))));
        QVERIFY(bounded.isCompatible(GameVersion(QStringLiteral("1.12.3.220"))));
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
    TestRelationshipResolver tResolver;
    TestDownloader tDownloader;
    failures += runSuite(argc, argv, tModVer);
    failures += runSuite(argc, argv, tGameVer);
    failures += runSuite(argc, argv, tRel);
    failures += runSuite(argc, argv, tMod);
    failures += runSuite(argc, argv, tInstall);
    failures += runSuite(argc, argv, tReg);
    failures += runSuite(argc, argv, tResolver);
    failures += runSuite(argc, argv, tDownloader);
    return failures;
}

#include "test_libckan.moc"