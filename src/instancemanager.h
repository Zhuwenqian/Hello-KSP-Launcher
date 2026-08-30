#ifndef INSTANCEMANAGER_H
#define INSTANCEMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QProcess>
#include <QList>
#include <QDateTime>
#include <functional>

struct GameSetting {
    QString key;         // 原始键名 (如 SCREEN_RESOLUTION_WIDTH)
    QString value;       // 值
    QString displayName; // 中文显示名
    QString category;    // 分类
};

struct DLCDetection {
    QString id;
    QString displayName;
    bool installed;
};

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

struct BackupInfo {
    QString fileName;    // 备份文件名
    QString filePath;    // 完整文件路径
    QString saveName;    // 存档名称
    QDateTime timestamp; // 备份时间
    qint64 fileSize;     // 文件大小（字节）
    QString note;        // 备注（如"恢复前备份"，空为普通备份）
};

class InstanceManager : public QObject
{
    Q_OBJECT
public:
    static InstanceManager& instance();

    QList<GameSetting> loadGameSettings(const QString& gamePath) const;
    bool saveGameSettings(const QString& gamePath, const QList<GameSetting>& settings) const;
    QList<DLCDetection> detectDLCs(const QString& gamePath) const;
    QStringList listMods(const QString& gamePath) const;
    bool launchGame(const QString& exePath, const QString& args = QString());
    void stopGame();
    QString detectGameRoot(const QString& exePath) const;
    bool isValidKSPPath(const QString& path) const;

    // 存档管理
    QStringList listSaves(const QString& gamePath) const;
    SaveInfo loadSaveInfo(const QString& saveFolderPath) const;
    QList<KerbalInfo> loadKerbals(const QString& saveFolderPath) const;
    bool saveKerbals(const QString& saveFolderPath, const QList<KerbalInfo>& kerbals) const;
    QString getSavesDir(const QString& gamePath) const;
    QString getPersistentSfsPath(const QString& saveFolderPath) const;

    // 备份管理
    // 备份目录结构：backups/{实例名}/{存档名}/*.zip
    QString getBackupsRootDir() const;
    QString getBackupDirForSave(const QString& instanceName, const QString& saveName) const;
    QList<BackupInfo> listBackups(const QString& instanceName, const QString& saveName) const;
    bool createBackup(const QString& saveFolderPath, const QString& instanceName,
                      const QString& saveName, const QString& note = QString(),
                      std::function<void(int progress)> progressCallback = nullptr) const;
    bool deleteBackup(const QString& backupFilePath) const;
    bool revealBackupInExplorer(const QString& backupFilePath) const;
    // 从备份恢复：恢复前自动备份当前状态，然后清空存档目录并解压备份。删除内容前请先由界面提示用户。
    bool restoreBackup(const QString& backupFilePath, const QString& saveFolderPath,
                       const QString& instanceName, const QString& saveName,
                       std::function<void(int progress)> progressCallback = nullptr) const;

    // 整合包导出。progressCallback 报告进度（0-100）；shouldCancel 在遍历每个文件前被调用，
    // 返回 true 则立即中断导出并返回 false（用于支持用户取消）。
    bool exportModpack(const QString& gamePath, const QString& zipFilePath,
                       std::function<void(int progress)> progressCallback = nullptr,
                       std::function<bool()> shouldCancel = nullptr) const;

private:
    // 将旧单层结构 backups/{存档名}/ 中的备份迁移到 backups/{实例名}/{存档名}/
    void migrateLegacyBackups(const QString& instanceName, const QString& saveName) const;

signals:
    void gameStarted();
    void gameFinished(int exitCode, QProcess::ExitStatus status);
    void gameError(QProcess::ProcessError error);

private:
    explicit InstanceManager(QObject *parent = nullptr);
    ~InstanceManager();
    InstanceManager(const InstanceManager&) = delete;
    InstanceManager& operator=(const InstanceManager&) = delete;

    QProcess* m_gameProcess;
};

#endif // INSTANCEMANAGER_H
