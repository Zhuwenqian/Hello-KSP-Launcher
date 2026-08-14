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
    // mirrors 为下载镜像。
    InstallResult install(const QVector<CkanModule> &modules,
                          const QString &downloadDir,
                          const QStringList &mirrors = {});

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

signals:
    void installProgress(const QString &identifier, int percent);
    void moduleInstalled(const QString &identifier);
    // 字节级进度：identifier 当前模组，doneBytes 全部已下载字节数，
    // totalBytes 批量总字节数，speedBps 当前模组实时速度（字节/秒）。
    void byteProgress(const QString &identifier, qint64 doneBytes, qint64 totalBytes, qint64 speedBps);

private:
    GameInstance *m_instance;
    std::atomic_bool m_cancelRequested{false};
};

} // namespace ckan

#endif // CKAN_MODULEINSTALLER_H