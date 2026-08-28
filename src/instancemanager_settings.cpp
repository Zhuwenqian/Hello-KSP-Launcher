// 实例管理器 - 游戏设置 / DLC 检测 / 模组列表
#include "instancemanager.h"
#include "instancemanager_keymap.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

QList<GameSetting> InstanceManager::loadGameSettings(const QString &gamePath) const
{
    QList<GameSetting> settings;
    QString settingsPath = QDir(gamePath).filePath("settings.cfg");
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return settings;
    }

    QTextStream in(&file);
    int braceDepth = 0;
    bool inBlock = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) {
            continue;
        }

        if (line.contains('{')) {
            braceDepth++;
            if (braceDepth == 1 && !inBlock) {
                inBlock = true;
            }
            continue;
        }
        if (line.contains('}')) {
            braceDepth--;
            if (braceDepth == 0) {
                inBlock = false;
            }
            continue;
        }

        if (braceDepth == 0 && line.contains('=')) {
            int eqPos = line.indexOf('=');
            QString key = line.left(eqPos).trimmed();
            // Skip version entry
            if (key == "SETTINGS_FILE_VERSION") {
                continue;
            }
            GameSetting s;
            s.key = key;
            s.value = line.mid(eqPos + 1).trimmed();
            const InstanceKeyInfo ki = instanceGetKeyInfo(key);
            // 隐藏高级设置不再展示（保留的原值在保存时不受影响）
            if (ki.hidden) continue;
            s.displayName = ki.displayName;
            s.category = ki.category;
            settings.append(s);
        }
    }

    file.close();
    return settings;
}

bool InstanceManager::saveGameSettings(const QString &gamePath, const QList<GameSetting> &settings) const
{
    QString settingsPath = QDir(gamePath).filePath("settings.cfg");
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Read original lines to preserve comments and structure
    QStringList originalLines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        originalLines.append(in.readLine());
    }
    file.close();

    // Build key-value map from new settings
    QMap<QString, QString> valueMap;
    for (const GameSetting& s : settings) {
        valueMap[s.key] = s.value;
    }

    // Rewrite lines updating values
    int braceDepth = 0;
    bool inBlock = false;
    for (int i = 0; i < originalLines.size(); ++i) {
        QString line = originalLines[i];
        QString trimmed = line.trimmed();

        if (trimmed.contains('{')) {
            braceDepth++;
            if (braceDepth == 1 && !inBlock) {
                inBlock = true;
            }
            continue;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (braceDepth == 0) {
                inBlock = false;
            }
            continue;
        }

        if (braceDepth == 0 && trimmed.contains('=') && !trimmed.startsWith("//")) {
            int eqPos = trimmed.indexOf('=');
            QString key = trimmed.left(eqPos).trimmed();
            if (valueMap.contains(key)) {
                int originalEqPos = line.indexOf('=');
                QString prefix = line.left(originalEqPos + 1);
                originalLines[i] = prefix + " " + valueMap[key];
            }
        }
    }

    // Write back
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

QList<DLCDetection> InstanceManager::detectDLCs(const QString &gamePath) const
{
    QList<DLCDetection> dlcs;

    DLCDetection mh;
    mh.id = "MakingHistory";
    mh.displayName = "Making History";
    mh.installed = QDir(QDir(gamePath).filePath("GameData/SquadExpansion/MakingHistory")).exists();
    dlcs.append(mh);

    DLCDetection bg;
    bg.id = "Serenity";
    bg.displayName = "Breaking Ground";
    bg.installed = QDir(QDir(gamePath).filePath("GameData/SquadExpansion/Serenity")).exists();
    dlcs.append(bg);

    return dlcs;
}

QStringList InstanceManager::listMods(const QString &gamePath) const
{
    QStringList mods;
    QString gameDataPath = QDir(gamePath).filePath("GameData");
    QDir gameData(gameDataPath);
    if (!gameData.exists()) {
        return mods;
    }

    QStringList entries = gameData.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList excluded = {"Squad", "SquadExpansion"};
    for (const QString& entry : entries) {
        if (!excluded.contains(entry, Qt::CaseInsensitive)) {
            mods.append(entry);
        }
    }

    return mods;
}
