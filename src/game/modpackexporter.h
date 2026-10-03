#ifndef GAME_MODPACKEXPORTER_H
#define GAME_MODPACKEXPORTER_H

#include <QString>
#include <QByteArray>
#include <functional>

// 整合包域：GameData 打包导出为 ZIP
class ModpackExporter
{
public:
    // 整合包导出。progressCallback 报告进度（0-100）；shouldCancel 在遍历每个文件前被调用，
    // 返回 true 则立即中断导出并返回 false（用于支持用户取消）。
    // packageMetaJson 非空时作为额外条目 hkspl_package.json 写入 zip 根目录（与 GameData 同层）。
    static bool exportModpack(const QString& gamePath, const QString& zipFilePath,
                              const QByteArray& packageMetaJson = QByteArray(),
                              std::function<void(int progress)> progressCallback = nullptr,
                              std::function<bool()> shouldCancel = nullptr);
};

#endif // GAME_MODPACKEXPORTER_H
