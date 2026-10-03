#ifndef GAME_GAMESETTINGSMANAGER_H
#define GAME_GAMESETTINGSMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>

// 游戏设置域：settings.cfg 解析/回写与 DLC 检测
struct GameSetting {
    QString key;         // 原始键名 (如 SCREEN_RESOLUTION_WIDTH)
    QString value;       // 值
    QString displayName; // 中文显示名
    QString category;    // 分类
    bool slider = false; // 是否用拖动条编辑（音量类）
};

struct DLCDetection {
    QString id;
    QString displayName;
    bool installed;
};

class GameSettingsManager
{
public:
    static QList<GameSetting> loadGameSettings(const QString& gamePath);
    static bool saveGameSettings(const QString& gamePath, const QList<GameSetting>& settings);
    static QList<DLCDetection> detectDLCs(const QString& gamePath);
};

#endif // GAME_GAMESETTINGSMANAGER_H
