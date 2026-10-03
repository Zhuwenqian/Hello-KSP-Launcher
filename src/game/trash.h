#ifndef GAME_TRASH_H
#define GAME_TRASH_H

#include <QString>

namespace trashutil {

// 将文件或文件夹移动到系统回收站（Windows，SHFileOperation + FOF_ALLOWUNDO，可撤销）。
// 其他平台（macOS/Linux）无统一回收站 API，直接永久删除。
bool moveToTrash(const QString &path);

} // namespace trashutil

#endif // GAME_TRASH_H
