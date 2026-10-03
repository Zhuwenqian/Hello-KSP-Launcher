#ifndef GAME_SHIPMANAGER_H
#define GAME_SHIPMANAGER_H

#include <QString>
#include <QStringList>

// 飞船域：Ships/craft 文件列举、解析与缩略图定位
struct ShipInfo {
    QString fileName;    // 完整文件名 (xxx.craft)
    QString name;        // ship = XXX（飞船名）
    QString version;     // version = X.Y.Z（游戏版本）
    QString description; // description = XXXX
    int partCount = 0;   // 顶层 PART{...} 块数量（部件数）
};

class ShipManager
{
public:
    // Ships 根目录（…/Ships）；type 为 "VAB" 或 "SPH"，对应 Ships/VAB、Ships/SPH。
    static QString getShipsDir(const QString& gamePath);
    // 列出 type 子目录下的 .craft 文件（排除 .loadmeta、*.craft.original 等），按文件名排序
    static QStringList listCraftFiles(const QString& gamePath, const QString& type);
    // 列出任意目录下的 .craft 文件（与 listCraftFiles 相同过滤）；预制件目录（存档名/Subassemblies）用它获取
    static QStringList listCraftFilesIn(const QString& dirPath);
    // 解析 craft 顶层的 ship/version/description；name 缺失时回退为文件名（去 .craft 后缀）
    static ShipInfo loadCraftInfo(const QString& craftFilePath);
    // 缩略图路径（Ships/@thumbs/{type}/{名称}.png|.jpg），找不到返回空串
    static QString getShipThumbPath(const QString& gamePath, const QString& type, const QString& craftFileName);
    // 玩家自制载具缩略图：游戏根目录 thumbs/{存档名}_{type}_{基名}.png（大小写不敏感、仅 .png），找不到返回空串。
    // 仅存档模式飞船管理使用；实例模式飞船多存档共享、无单一存档名，不读取这里。
    static QString getPlayerShipThumbPath(const QString& gamePath, const QString& saveName,
                                          const QString& type, const QString& craftFileName);
    // 将单个 craft 文件移动到系统回收站（Windows）；其他平台直接永久删除
    static bool moveCraftToTrash(const QString& craftFilePath);
};

#endif // GAME_SHIPMANAGER_H
