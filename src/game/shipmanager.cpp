// 飞船域：craft 文件列举、解析、缩略图
#include "shipmanager.h"
#include "trash.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

QString ShipManager::getShipsDir(const QString &gamePath)
{
    return QDir(gamePath).filePath("Ships");
}

QStringList ShipManager::listCraftFiles(const QString &gamePath, const QString &type)
{
    return listCraftFilesIn(QDir(getShipsDir(gamePath)).filePath(type));
}

QStringList ShipManager::listCraftFilesIn(const QString &dirPath)
{
    QStringList result;
    QDir dir(dirPath);
    if (!dir.exists()) {
        return result;
    }

    // 仅保留以 .craft 结尾的文件，天然排除 .loadmeta、*.craft.original 等伴随文件
    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& f : files) {
        if (f.endsWith(QStringLiteral(".craft"), Qt::CaseInsensitive)) {
            result.append(f);
        }
    }
    return result;
}

ShipInfo ShipManager::loadCraftInfo(const QString &craftFilePath)
{
    ShipInfo info;
    info.fileName = QFileInfo(craftFilePath).fileName();

    QFile file(craftFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        info.name = QFileInfo(craftFilePath).completeBaseName();
        return info;
    }

    // craft 为 KSP config 语法：ship/version/description 均位于顶层（braceDepth==0）
    QTextStream in(&file);
    int braceDepth = 0;
    int partCount = 0;
    bool partBlockPending = false; // 顶层的 PART 关键字后紧跟 '{' 才算一个完整部件块
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.contains('{')) {
            if (braceDepth == 0 && partBlockPending) {
                partCount++;
            }
            braceDepth++;
            partBlockPending = false;
            continue;
        }
        if (line.contains('}')) {
            braceDepth--;
            continue;
        }
        if (braceDepth == 0) {
            // PART 独占一行且无 '='，标记为待开块的顶层部件（子块内的 key 均在 braceDepth>0，不会误计）
            if (line.compare(QLatin1String("PART"), Qt::CaseInsensitive) == 0) {
                partBlockPending = true;
            } else if (line.contains('=')) {
                int eq = line.indexOf('=');
                QString key = line.left(eq).trimmed();
                QString value = line.mid(eq + 1).trimmed();
                if (key == "ship") {
                    info.name = value;
                } else if (key == "version") {
                    info.version = value;
                } else if (key == "description") {
                    // 用首个 '=' 拆分，保证 description 值内出现 '=' 也不会被截断
                    info.description = value;
                }
            }
        }
    }
    file.close();

    info.partCount = partCount;

    if (info.name.isEmpty()) {
        info.name = QFileInfo(craftFilePath).completeBaseName();
    }
    return info;
}

QString ShipManager::getShipThumbPath(const QString &gamePath, const QString &type,
                                          const QString &craftFileName)
{
    // 缩略图与 craft 文件同名（去 .craft），位于 Ships/@thumbs/{type}/
    const QString base = QFileInfo(craftFileName).completeBaseName();
    const QString dir = QDir(getShipsDir(gamePath)).filePath(QStringLiteral("@thumbs/") + type);

    const QString png = QDir(dir).filePath(base + QStringLiteral(".png"));
    if (QFileInfo::exists(png)) {
        return png;
    }
    const QString jpg = QDir(dir).filePath(base + QStringLiteral(".jpg"));
    if (QFileInfo::exists(jpg)) {
        return jpg;
    }
    return QString();
}

QString ShipManager::getPlayerShipThumbPath(const QString &gamePath, const QString &saveName,
                                                const QString &type, const QString &craftFileName)
{
    // 玩家自制载具缩略图在 游戏根目录/thumbs/，命名 <存档名>_<type>_<载具基名>.png，与 Ships/@thumbs
    // 的原版图不同目录。KSP 生成的缩略图固定 .png，故只查 .png；文件名做大小写不敏感匹配，
    // 兼容 Windows/Linux/macOS 之间大小写不一致的场景。
    const QString base = QFileInfo(craftFileName).completeBaseName();
    const QDir dir(QDir(gamePath).filePath(QStringLiteral("thumbs")));
    if (!dir.exists()) {
        return QString();
    }
    const QString needle = saveName + QLatin1Char('_') + type + QLatin1Char('_')
                           + base + QLatin1String(".png");
    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& f : files) {
        if (f.compare(needle, Qt::CaseInsensitive) == 0) {
            return dir.filePath(f);
        }
    }
    return QString();
}

bool ShipManager::moveCraftToTrash(const QString &craftFilePath)
{
    return trashutil::moveToTrash(craftFilePath);
}