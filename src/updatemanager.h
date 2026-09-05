#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QProcess;
class QJsonArray;

// 自更新管理器（主程序侧）：
//  1) checkForUpdate() 查询 GitHub Releases/latest，得到最新版本信息（版本号 / 资产 / 更新日志）。
//  2) downloadRelease() 下载 x86_64 zip 到应用目录的临时统一更新目录，带进度信号、可写文件。
//  3) applyUpdate(zipPath) 启动独立汉 concurrent updater.exe（负责备份、替换、重启），随后主程序退出。
// 更新源仓库：https://github.com/Zhuwenqian/Hello-KSP-Launcher
class UpdaterManager : public QObject
{
    Q_OBJECT
public:
    struct ReleaseInfo {
        QString version;     // 去前导 v 的版本号，如 1.1.2
        QString assetUrl;    // x86_64 zip 资产下载地址
        QString assetName;   // 资产文件名（便于生成暂存 zip 名）
        QString expectedDigest; // GitHub API 提供的资产 SHA256（小写 hex，含 "sha256:" 前缀的关系）
        QString body;        // Release body（更新日志）
        bool hasUpdate = false; // 服务端版本是否高于当前本地版本
    };

    static UpdaterManager& instance();

    // 当前本地版本号（来自 appversion.h）
    static QString currentVersion();
    // 语义化版本比较：a < b 返回 true。按 major.minor[.patch] 数字段依次比较，
    // 缺血字段按 0 处理；任一段非纯数字（如含元数据后缀）时回退为字符串字典序比较。
    static bool versionLess(const QString &a, const QString &b);

    // 查询最新 Release（异步，结果经 updateCheckDone/updateCheckFailed 返回）
    void checkForUpdate(bool quiet = false);
    // 下载 x86_64 zip 资产到应用目录响应；下载完成后 emit downloadFinished
    void downloadRelease();
    // 下载完成后的 zip 绝对路径（下载前为预定目标路径）
    QString stagingZipPath() const;

    // 应用更新：启动 updater.exe 处理替换并重启，随后退出当前进程。
    // 形参 zipPath 为已下载完成的新版本 zip 绝对路径。
    bool applyUpdate(const QString &zipPath);

    ReleaseInfo latest() const { return m_latest; }
    bool busy() const { return m_working; }

    // 从 GitHub 资产对象的 digest 字段提取期望 SHA256（形如 "sha256:<64hex>"）。
    // 纯静态函数，返回小写 64 位 hex；缺失 / 非 "sha256:" 前缀 / 非 64 位 hex 返回空串。
    static QString digestHexFromApi(const QJsonArray &assets, const QString &assetName);

signals:
    void updateCheckDone();                          // 查询完成，latest() 可读
    void updateCheckFailed(const QString &error);    // 查询失败（网络/无更新源/解析失败）
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &zipPath);   // 下载完成并已写盘
    void updateError(const QString &message);        // 下载/应用阶段错误

private:
    explicit UpdaterManager(QObject *parent = nullptr);
    ~UpdaterManager();
    UpdaterManager(const UpdaterManager&) = delete;
    UpdaterManager& operator=(const UpdaterManager&) = delete;

    // 解析 Releases/latest JSON；失败返回 false 并置 m_lastError
    bool parseRelease(const QByteArray& json);
    // 计算文件 SHA256（小写 64 位十六进制）；文件不可读返回 false。
    static bool fileSha256(const QString &path, QString *hexOut);
    // 校验已下载 zip 的 SHA256 与 release 摘要是否一致：一致发 downloadFinished，
    // 失败则清理暂存包并 updateError。
    void verifyAndFinish(const QString &zipPath);
    // 清理已下载但未通过校验的暂存 zip（尽力而为，忽略删除失败）。
    void cleanupStagedZip();

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply;
    QFile* m_file;
    ReleaseInfo m_latest;
    QString m_lastError;
    bool m_working;
    bool m_quiet; // 静默检查（自动检查）：失败不弹窗
};

#endif // UPDATEMANAGER_H