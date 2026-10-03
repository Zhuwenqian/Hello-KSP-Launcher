// 回收站工具：存档/飞船删除共用的系统回收站封装
#include "trash.h"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include <vector>

namespace trashutil {

bool moveToTrash(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }

#ifdef Q_OS_WIN
    // Windows: SHFileOperation(FO_DELETE + FOF_ALLOWUNDO) 移入回收站，可撤销。
    // pFrom 必须以双 null 结尾，且为宽字符。
    std::wstring ws = QDir::toNativeSeparators(path).toStdWString();
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
    // macOS / Linux 无统一回收站 API：
    // 目录直接递归删除，文件直接删除
    if (info.isDir()) {
        QDir dir(path);
        return dir.removeRecursively();
    }
    return QFile::remove(path);
#endif
}

} // namespace trashutil
