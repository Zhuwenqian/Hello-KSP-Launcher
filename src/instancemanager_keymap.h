#ifndef INSTANCEMANAGER_KEYMAP_H
#define INSTANCEMANAGER_KEYMAP_H

#include <QString>

// 游戏设置键的显示信息（中文显示名 + 分类）
struct InstanceKeyInfo {
    QString displayName;
    QString category;
    // 是否在游戏设置界面隐藏（隐藏的高级设置，值保存时保留不动）
    bool hidden = false;
};

// 查询指定设置键（settings.cfg 键名）的显示名与分类；
// 英文界面返回英文显示名/分类，未知键返回键名本身与「其他」分类。
// 实现在 instancemanager_keymap.cpp。
InstanceKeyInfo instanceGetKeyInfo(const QString &key);

#endif // INSTANCEMANAGER_KEYMAP_H
