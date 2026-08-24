#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "steamdiscovery.h"

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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    TestSteamDiscovery tSteamDisc;
    failures += QTest::qExec(&tSteamDisc, argc, argv);
    return failures;
}

#include "test_launcher.moc"
