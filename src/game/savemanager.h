#ifndef GAME_SAVEMANAGER_H
#define GAME_SAVEMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>

// 存档域：存档目录列举、persistent.sfs 解析、小绿人(Roster)编辑
struct SaveInfo {
    QString folderName;  // 存档文件夹名
    QString title;       // Title
    QString version;     // version
    QString mode;        // Mode
    QString seed;        // Seed
    bool modded;         // modded
    QString envInfo;     // envInfo
    QString versionFull; // versionFull
    QString versionCreated; // versionCreated
    QString persistentTimestamp; // persistentTimestamp
};

struct KerbalInfo {
    QString name;        // 姓名
    QString originalName;// 原始姓名（用于保存时定位）
    QString gender;      // 性别
    QString type;        // 类型
    QString trait;       // 职业
    double brave;        // 勇敢度
    double dumb;         // 愚蠢度
    bool badS;           // 坏蛋
    bool veteran;        // 老兵
    bool hero;           // 英雄
    // 用于记录原始位置信息，方便保存
    int lineNumber;      // KERBAL块起始行
};

class SaveManager
{
public:
    static QStringList listSaves(const QString& gamePath);
    static SaveInfo loadSaveInfo(const QString& saveFolderPath);
    static QList<KerbalInfo> loadKerbals(const QString& saveFolderPath);
    static bool saveKerbals(const QString& saveFolderPath, const QList<KerbalInfo>& kerbals);
    // 从 ROSTER 区块移除指定（原名称）小绿人的 KERBAL 数据块；重命名其姓名（均立即写盘）
    static bool deleteKerbal(const QString& saveFolderPath, const QString& originalName);
    static bool renameKerbal(const QString& saveFolderPath, const QString& originalName, const QString& newName);
    static QString getSavesDir(const QString& gamePath);
    // 将存档文件夹移动到系统回收站（Windows）。其他平台（macOS/Linux）无统一回收站 API，
    // 直接永久删除整个文件夹。删除后存档将无法恢复，请先由界面提示用户。
    static bool moveSaveToTrash(const QString& saveFolderPath);
};

#endif // GAME_SAVEMANAGER_H
