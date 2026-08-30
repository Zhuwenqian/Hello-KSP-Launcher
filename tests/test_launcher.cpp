#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QDir>

#include "steamdiscovery.h"
#include "instancemanager.h"

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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    TestSteamDiscovery tSteamDisc;
    failures += QTest::qExec(&tSteamDisc, argc, argv);
    TestValidKSPPath tValid;
    failures += QTest::qExec(&tValid, argc, argv);
    return failures;
}

#include "test_launcher.moc"
