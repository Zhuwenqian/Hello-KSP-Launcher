// 存档域：存档列举、persistent.sfs 解析、小绿人编辑
#include "savemanager.h"
#include "trash.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

namespace {

QString getPersistentSfsPath(const QString &saveFolderPath)
{
    return QDir(saveFolderPath).filePath("persistent.sfs");
}

} // namespace

QString SaveManager::getSavesDir(const QString &gamePath)
{
    return QDir(gamePath).filePath("saves");
}

bool SaveManager::moveSaveToTrash(const QString &saveFolderPath)
{
    return trashutil::moveToTrash(saveFolderPath);
}

QStringList SaveManager::listSaves(const QString &gamePath)
{
    QStringList saves;
    QString savesPath = getSavesDir(gamePath);
    QDir savesDir(savesPath);
    if (!savesDir.exists()) {
        return saves;
    }

    QStringList excludeNames = {"DarkMultiPlayer", "scenarios", "training"};
    QStringList entries = savesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        if (!excludeNames.contains(entry, Qt::CaseInsensitive)) {
            QString savePath = savesDir.filePath(entry);
            // 检查persistent.sfs是否存在
            if (QFile::exists(getPersistentSfsPath(savePath))) {
                saves.append(entry);
            }
        }
    }
    return saves;
}

SaveInfo SaveManager::loadSaveInfo(const QString &saveFolderPath)
{
    SaveInfo info;
    info.folderName = QDir(saveFolderPath).dirName();
    info.modded = false;

    QString sfsPath = getPersistentSfsPath(saveFolderPath);
    QFile file(sfsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return info;
    }

    QTextStream in(&file);
    int braceDepth = 0;
    bool inGameBlock = false;
    int gameBlockDepth = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) {
            continue;
        }

        if (line.startsWith("GAME")) {
            inGameBlock = true;
            gameBlockDepth = braceDepth;
            continue;
        }

        if (line.contains('{')) {
            braceDepth++;
            continue;
        }
        if (line.contains('}')) {
            braceDepth--;
            if (inGameBlock && braceDepth <= gameBlockDepth) {
                inGameBlock = false;
                break;
            }
            continue;
        }

        if (inGameBlock && braceDepth == gameBlockDepth + 1 && line.contains('=')) {
            int eqPos = line.indexOf('=');
            QString key = line.left(eqPos).trimmed();
            QString value = line.mid(eqPos + 1).trimmed();

            if (key == "version") info.version = value;
            else if (key == "Title") info.title = value;
            else if (key == "Mode") info.mode = value;
            else if (key == "Seed") info.seed = value;
            else if (key == "modded") info.modded = (value.toLower() == "true");
            else if (key == "envInfo") info.envInfo = value;
            else if (key == "versionFull") info.versionFull = value;
            else if (key == "versionCreated") info.versionCreated = value;
            else if (key == "persistentTimestamp") info.persistentTimestamp = value;
        }
    }

    file.close();
    return info;
}

QList<KerbalInfo> SaveManager::loadKerbals(const QString &saveFolderPath)
{
    QList<KerbalInfo> kerbals;

    QString sfsPath = getPersistentSfsPath(saveFolderPath);
    QFile file(sfsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return kerbals;
    }

    QTextStream in(&file);
    QStringList allLines;
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }
    file.close();

    int braceDepth = 0;
    bool inRoster = false;
    int rosterDepth = 0;
    bool inKerbal = false;
    int kerbalStartLine = -1;
    KerbalInfo currentKerbal;
    int kerbalDepth = 0;

    for (int i = 0; i < allLines.size(); ++i) {
        QString line = allLines[i];
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith("//")) {
            continue;
        }

        if (trimmed == "ROSTER") {
            inRoster = true;
            rosterDepth = braceDepth;
            continue;
        }

        if (inRoster && trimmed == "KERBAL" && braceDepth == rosterDepth + 1) {
            inKerbal = true;
            kerbalDepth = braceDepth;
            kerbalStartLine = i;
            currentKerbal = KerbalInfo();
            currentKerbal.lineNumber = kerbalStartLine;
            currentKerbal.brave = 0.5;
            currentKerbal.dumb = 0.5;
            currentKerbal.badS = false;
            currentKerbal.veteran = false;
            currentKerbal.hero = false;
            currentKerbal.originalName.clear();
            continue;
        }

        if (trimmed.contains('{')) {
            braceDepth++;
            continue;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (inKerbal && braceDepth <= kerbalDepth) {
                kerbals.append(currentKerbal);
                inKerbal = false;
            }
            if (inRoster && braceDepth <= rosterDepth) {
                inRoster = false;
                break;
            }
            continue;
        }

        if (inKerbal && braceDepth == kerbalDepth + 1 && trimmed.contains('=')) {
            int eqPos = trimmed.indexOf('=');
            QString key = trimmed.left(eqPos).trimmed();
            QString value = trimmed.mid(eqPos + 1).trimmed();

            if (key == "name") {
                currentKerbal.name = value;
                currentKerbal.originalName = value;
            } else if (key == "gender") {
                currentKerbal.gender = value;
            } else if (key == "type") {
                currentKerbal.type = value;
            } else if (key == "trait") {
                currentKerbal.trait = value;
            } else if (key == "brave") {
                currentKerbal.brave = value.toDouble();
            } else if (key == "dumb") {
                currentKerbal.dumb = value.toDouble();
            } else if (key == "badS") {
                currentKerbal.badS = (value.toLower() == "true");
            } else if (key == "veteran") {
                currentKerbal.veteran = (value.toLower() == "true");
            } else if (key == "hero") {
                currentKerbal.hero = (value.toLower() == "true");
            }
        }
    }

    return kerbals;
}

bool SaveManager::saveKerbals(const QString &saveFolderPath, const QList<KerbalInfo> &kerbals)
{
    QString sfsPath = getPersistentSfsPath(saveFolderPath);
    QFile file(sfsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QStringList originalLines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        originalLines.append(in.readLine());
    }
    file.close();

    // 括号匹配验证
    int openBraces = 0;
    for (const QString& line : originalLines) {
        openBraces += line.count('{');
        openBraces -= line.count('}');
    }
    if (openBraces != 0) {
        return false; // 括号不匹配，拒绝保存
    }

    // 建立原始名称到KerbalInfo的映射（因为name可能被修改）
    QMap<QString, const KerbalInfo*> kerbalMap;
    for (const KerbalInfo& k : kerbals) {
        kerbalMap[k.originalName] = &k;
    }

    // 解析并更新
    int braceDepth = 0;
    bool inRoster = false;
    int rosterDepth = 0;
    bool inKerbal = false;
    int kerbalDepth = 0;
    QString currentKerbalOrigName;

    for (int i = 0; i < originalLines.size(); ++i) {
        QString& line = originalLines[i];
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith("//")) {
            continue;
        }

        if (trimmed == "ROSTER") {
            inRoster = true;
            rosterDepth = braceDepth;
        } else if (inRoster && trimmed == "KERBAL" && braceDepth == rosterDepth + 1) {
            inKerbal = true;
            kerbalDepth = braceDepth;
            currentKerbalOrigName.clear();
        }

        if (trimmed.contains('{')) {
            braceDepth++;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (inKerbal && braceDepth <= kerbalDepth) {
                inKerbal = false;
                currentKerbalOrigName.clear();
            }
            if (inRoster && braceDepth <= rosterDepth) {
                inRoster = false;
            }
        }

        if (inKerbal && braceDepth == kerbalDepth + 1 && trimmed.contains('=')) {
            int eqPos = trimmed.indexOf('=');
            QString key = trimmed.left(eqPos).trimmed();
            int originalEqPos = line.indexOf('=');
            QString prefix = line.left(originalEqPos + 1);

            if (key == "name") {
                QString value = trimmed.mid(eqPos + 1).trimmed();
                // 记录当前Kerbal的原始名称
                if (currentKerbalOrigName.isEmpty()) {
                    currentKerbalOrigName = value;
                }
                // 如果该Kerbal在修改列表中，使用新名称
                if (kerbalMap.contains(currentKerbalOrigName)) {
                    const KerbalInfo* k = kerbalMap[currentKerbalOrigName];
                    line = prefix + " " + k->name;
                }
            } else if (!currentKerbalOrigName.isEmpty() && kerbalMap.contains(currentKerbalOrigName)) {
                const KerbalInfo* k = kerbalMap[currentKerbalOrigName];
                if (key == "gender") {
                    line = prefix + " " + k->gender;
                } else if (key == "type") {
                    // type不可编辑，保持原样
                } else if (key == "trait") {
                    line = prefix + " " + k->trait;
                } else if (key == "brave") {
                    line = prefix + " " + QString::number(k->brave, 'f', 1);
                } else if (key == "dumb") {
                    line = prefix + " " + QString::number(k->dumb, 'f', 1);
                } else if (key == "badS") {
                    line = prefix + " " + (k->badS ? "True" : "False");
                } else if (key == "veteran") {
                    line = prefix + " " + (k->veteran ? "True" : "False");
                } else if (key == "hero") {
                    line = prefix + " " + (k->hero ? "True" : "False");
                }
            }
        }
    }

    // 括号再次验证
    openBraces = 0;
    for (const QString& line : originalLines) {
        openBraces += line.count('{');
        openBraces -= line.count('}');
    }
    if (openBraces != 0) {
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    for (const QString& l : originalLines) {
        out << l << "\n";
    }
    file.close();
    return true;
}

bool SaveManager::deleteKerbal(const QString &saveFolderPath, const QString &originalName)
{
    QString sfsPath = getPersistentSfsPath(saveFolderPath);
    QFile file(sfsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // 括号匹配验证
    int openBraces = 0;
    for (const QString& l : lines) {
        openBraces += l.count('{');
        openBraces -= l.count('}');
    }
    if (openBraces != 0) {
        return false; // 括号不匹配，拒绝修改
    }

    // 定位匹配 originalName 的 KERBAL 块
    int braceDepth = 0;
    bool inRoster = false;
    int rosterDepth = 0;
    bool inKerbal = false;
    int kerbalDepth = 0;
    QString kerbalName;
    int blockStart = -1;
    int blockEnd = -1;

    for (int i = 0; i < lines.size(); ++i) {
        QString trimmed = lines[i].trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("//")) {
            continue;
        }

        if (trimmed == "ROSTER") {
            inRoster = true;
            rosterDepth = braceDepth;
            continue;
        }

        if (inRoster && trimmed == "KERBAL" && braceDepth == rosterDepth + 1) {
            inKerbal = true;
            kerbalDepth = braceDepth;
            kerbalName.clear();
            blockStart = i;
            continue;
        }

        if (trimmed.contains('{')) {
            braceDepth++;
            continue;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (inKerbal && braceDepth <= kerbalDepth) {
                if (kerbalName == originalName) {
                    blockEnd = i;
                    break;
                }
                inKerbal = false;
                kerbalName.clear();
            }
            if (inRoster && braceDepth <= rosterDepth) {
                inRoster = false;
            }
            continue;
        }

        if (inKerbal && braceDepth == kerbalDepth + 1 && trimmed.contains('=')) {
            int eqPos = trimmed.indexOf('=');
            if (trimmed.left(eqPos).trimmed() == "name") {
                kerbalName = trimmed.mid(eqPos + 1).trimmed();
            }
        }
    }

    if (blockStart < 0 || blockEnd < 0) {
        return false; // 未找到匹配的小绿人
    }

    lines.erase(lines.begin() + blockStart, lines.begin() + blockEnd + 1);

    // 括号再次验证
    openBraces = 0;
    for (const QString& l : lines) {
        openBraces += l.count('{');
        openBraces -= l.count('}');
    }
    if (openBraces != 0) {
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    for (const QString& l : lines) {
        out << l << "\n";
    }
    file.close();
    return true;
}

bool SaveManager::renameKerbal(const QString &saveFolderPath, const QString &originalName, const QString &newName)
{
    QString sfsPath = getPersistentSfsPath(saveFolderPath);
    QFile file(sfsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // 括号匹配验证
    int openBraces = 0;
    for (const QString& l : lines) {
        openBraces += l.count('{');
        openBraces -= l.count('}');
    }
    if (openBraces != 0) {
        return false;
    }

    int braceDepth = 0;
    bool inRoster = false;
    int rosterDepth = 0;
    bool inKerbal = false;
    int kerbalDepth = 0;
    bool renamed = false;

    for (QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("//")) {
            continue;
        }

        if (trimmed == "ROSTER") {
            inRoster = true;
            rosterDepth = braceDepth;
        } else if (inRoster && trimmed == "KERBAL" && braceDepth == rosterDepth + 1) {
            inKerbal = true;
            kerbalDepth = braceDepth;
        }

        if (trimmed.contains('{')) {
            braceDepth++;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (inKerbal && braceDepth <= kerbalDepth) {
                inKerbal = false;
            }
            if (inRoster && braceDepth <= rosterDepth) {
                inRoster = false;
            }
        }

        if (inKerbal && braceDepth == kerbalDepth + 1 && trimmed.contains('=')) {
            int eqPos = trimmed.indexOf('=');
            if (trimmed.left(eqPos).trimmed() == "name") {
                if (trimmed.mid(eqPos + 1).trimmed() == originalName) {
                    int origEqPos = line.indexOf('=');
                    QString prefix = line.left(origEqPos + 1);
                    line = prefix + " " + newName;
                    renamed = true;
                }
            }
        }
    }

    if (!renamed) {
        return false;
    }

    openBraces = 0;
    for (const QString& l : lines) {
        openBraces += l.count('{');
        openBraces -= l.count('}');
    }
    if (openBraces != 0) {
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    for (const QString& l : lines) {
        out << l << "\n";
    }
    file.close();
    return true;
}
