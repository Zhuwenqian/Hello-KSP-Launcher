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
    QString getBackupsRootDir() const;
    QString getBackupDirForSave(const QString& saveName) const;
    QList<BackupInfo> listBackups(const QString& saveName) const;
    bool createBackup(const QString& saveFolderPath, const QString& saveName,
                      std::function<void(int progress)> progressCallback = nullptr) const;
    bool deleteBackup(const QString& backupFilePath) const;
    bool revealBackupInExplorer(const QString& backupFilePath) const;

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
