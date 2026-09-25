#include "instancemanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

QString InstanceManager::getShipsDir(const QString &gamePath) const
{
    return QDir(gamePath).filePath("Ships");
}

QStringList InstanceManager::listCraftFiles(const QString &gamePath, const QString &type) const
{
    QStringList result;
    QString dirPath = QDir(getShipsDir(gamePath)).filePath(type);
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

ShipInfo InstanceManager::loadCraftInfo(const QString &craftFilePath) const
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

QString InstanceManager::getShipThumbPath(const QString &gamePath, const QString &type,
                                          const QString &craftFileName) const
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

bool InstanceManager::moveCraftToTrash(const QString &craftFilePath) const
{
    QFileInfo info(craftFilePath);
    if (!info.exists()) {
        return false;
    }

#ifdef Q_OS_WIN
    // 与 moveSaveToTrash 一致的回收站实现：SHFileOperation(FO_DELETE + FOF_ALLOWUNDO)
    // 将单个 craft 文件移入系统回收站（可撤销）。pFrom 须以双 null 结尾的宽字符。
    std::wstring ws = QDir::toNativeSeparators(craftFilePath).toStdWString();
    std::vector<wchar_t> from(ws.begin(), ws.end());
    from.push_back(L'\0');
    from.push_back(L'\0');

    SHFILEOPSTRUCT op = {};
    op.hwnd = nullptr; // 界面已弹确认框，不需要系统再显示删除确认
    op.wFunc = FO_DELETE;
    op.pFrom = from.data();
    op.pTo = nullptr;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    return (SHFileOperation(&op) == 0);
#else
    // macOS / Linux 无统一回收站 API，直接永久删除文件
    return QFile::remove(craftFilePath);
#endif
}