#ifndef GAME_BACKUPMANAGER_H
#define GAME_BACKUPMANAGER_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <functional>

// 备份域：存档 zip 备份的创建/列举/恢复/删除
struct BackupInfo {
    QString fileName;    // 备份文件名
    QString filePath;    // 完整文件路径
    QString saveName;    // 存档名称
    QDateTime timestamp; // 备份时间
    qint64 fileSize;     // 文件大小（字节）
    QString note;        // 备注（如"恢复前备份"，空为普通备份）
};

class BackupManager
{
public:
    // 备份目录结构：backups/{实例名-id前8位}/{存档名}/*.zip
    // 第1级目录附加实例 id 前8位，最大限度避免「重实例名 + 重存档名」导致的目录冲突。
    static QString getBackupsRootDir();
    static QString getBackupDirForSave(const QString& instanceName, const QString& instanceId,
                                       const QString& saveName);
    static QList<BackupInfo> listBackups(const QString& instanceName, const QString& instanceId,
                                         const QString& saveName);
    static bool createBackup(const QString& saveFolderPath, const QString& instanceName,
                             const QString& instanceId, const QString& saveName,
                             const QString& note = QString(),
                             std::function<void(int progress)> progressCallback = nullptr);
    static bool deleteBackup(const QString& backupFilePath);
    static bool revealBackupInExplorer(const QString& backupFilePath);
    // 从备份恢复：恢复前自动备份当前状态，然后清空存档目录并解压备份。删除内容前请先由界面提示用户。
    static bool restoreBackup(const QString& backupFilePath, const QString& saveFolderPath,
                              const QString& instanceName, const QString& instanceId,
                              const QString& saveName,
                              std::function<void(int progress)> progressCallback = nullptr);
};

#endif // GAME_BACKUPMANAGER_H
