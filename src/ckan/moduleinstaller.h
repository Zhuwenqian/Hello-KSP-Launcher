#ifndef CKAN_MODULEINSTALLER_H
#define CKAN_MODULEINSTALLER_H

#include <QString>
#include <QVector>
#include <QObject>
#include <atomic>

#include "ckan_export.h"
#include "ckanmodule.h"
#include "installedmodule.h"

namespace ckan {

class GameInstance;
class Repository;

// 安装结果
struct CKAN_API InstallResult {
    bool    ok = false;
    QString error;
    QStringList installedIdentifiers;
};

// 模块安装器：下载 zip -> miniz 解压 -> 按 install 规则复制到 GameData -> 更新 registry。
class CKAN_API ModuleInstaller : public QObject
{
    Q_OBJECT
public:
    explicit ModuleInstaller(GameInstance *instance, QObject *parent = nullptr);

    // 安装一批模块（已由解析器展开依赖）。downloadDir 为 zip 缓存目录。
    // 便捷入口：先 downloadModules 下载全部，再 installFromCache 写入。
    InstallResult install(const QVector<CkanModule> &modules,
                          const QString &downloadDir,
                          const QStringList &foldersToDelete = {},
                          const QStringList &mirrorPrefixes = {},
                          bool preferModuleMirrors = false);

    // 阶段一：下载全部模块的 zip 到 downloadDir（复用已存在且有效的缓存，只补缺失/损坏的）。
    // 全程报 byteProgress 聚合进度，支持 cancel() 中止。返回是否全部就绪。
    // maxConcurrent 为并行下载数（>1 时多模块同时下载），默认 3。
    bool downloadModules(const QVector<CkanModule> &modules,
                         const QString &downloadDir,
                         const QStringList &mirrorPrefixes,
                         bool preferModuleMirrors,
                         QString *error,
                         int maxConcurrent = 3);

    // 阶段二：从缓存安装（不再下载）。zip 缺失/损坏时返回错误。
    // foldersToDelete 为相对 GameData 的顶层文件夹名：写入前若命中，先递归删除旧文件夹。
    InstallResult installFromCache(const QVector<CkanModule> &modules,
                                   const QString &downloadDir,
                                   const QStringList &foldersToDelete = {});

    // 以 zip 实际内容为准，返回该模块将写入的 GameData 顶层文件夹名列表（相对 GameData）。
    // 读取 zip 条目后套用 install 规则推导真实目标，绝不依赖预估。可用于下载后冲突检测。
    static QStringList actualGameDataFolders(const QString &zipPath, const CkanModule &mod,
                                             QString *error = nullptr);

    // 卸载模块：删除 registry 记录的文件，更新 registry。
    InstallResult uninstall(const QString &identifier);

    // 请求中止当前安装任务（线程安全）。正在下载的模组会被中止，
    // 已下载的临时数据不会写入缓存文件。
    void cancel();

    // 供测试/外部：从 zip 提取安装文件列表
    static bool listZipEntries(const QString &zipPath, QStringList *entries, QString *error);

    // 清洗缓存文件名中的非法字符（Windows 不含冒号/斜杠等）。
    // version 可能带 epoch（如 "1:3.4.0"），冒号在 NTFS 上会变成 ADS 分隔符导致读写错位。
    static QString safeCacheFileName(const QString &s);

    // 递归删除目录（含所有子项）。返回是否成功。
    static bool removeDirRecursively(const QString &absPath);

signals:
    void installProgress(const QString &identifier, int percent);
    void moduleInstalled(const QString &identifier);
    // 字节级进度：identifier 当前模组，doneBytes 全部已下载字节数，
    // totalBytes 批量总字节数，speedBps 当前模组实时速度（字节/秒）。
    void byteProgress(const QString &identifier, qint64 doneBytes, qint64 totalBytes, qint64 speedBps);

private:
    GameInstance *m_instance;
    std::atomic_bool m_cancelRequested{false};

    // 单个模组下载任务（并行下载 worker 的输入）
    struct DownloadTask {
        CkanModule mod;
        QString zipPath;      // 缓存写入路径
        QStringList mirrors;  // 拼接了镜像前缀的备用 URL
        qint64 size = 1;      // downloadSize（未知时为 1）
    };
    // 单个模组下载结果
    struct DownloadOutcome {
        bool ok = false;
        QString identifier;
        QString error;
    };
    // 下载单个模组（在并发池线程内执行）。doneBytes 为跨线程累计的已下载字节数。
    DownloadOutcome downloadOneTask(const DownloadTask &task, std::atomic<qint64> &doneBytes,
                                    qint64 totalBytes, bool preferModuleMirrors);
};

} // namespace ckan

#endif // CKAN_MODULEINSTALLER_H