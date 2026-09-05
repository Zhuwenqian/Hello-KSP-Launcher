#ifndef MODSCONTROLLER_H
#define MODSCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "ckanmanager.h"
#include "moddecision.h"

// 模组管理页的业务编排门面（薄）：把模组 UI 与 CKanManager 直接耦合解掉。
// - 持有真实 Qt 弹窗决策钩子（冲突/建议/提供者/磁盘/确认），构造时注入 CKanManager，
//   供安装/依赖解析等业务层消费（业务层不构造任何 Widget）；
// - 转发安装/卸载/升级/刷新/导入/下载等操作请求，并继发 CKanManager 的进度与结果信号，
//   使 UI 只依赖本控制器一个协调入口，不在各处直接摸单例。
// 本类本身不构造 Widget；只读查询仍由 UI 直接走 CKanManager。
class ModsController : public QObject
{
    Q_OBJECT
public:
    explicit ModsController(QObject *parent = nullptr);

    // ---- 实例绑定 ----
    void openInstance(const QString &gameDir, const QString &instanceName);
    bool tryAcquireRegistryLock();

    // ---- 操作请求（转发 CKanManager）----
    void requestRefreshIndex(bool force);
    void requestScanDlls(bool force);
    void requestInstall(const QString &identifier);          // 自动附带推荐
    void requestInstallVersion(const ckan::CkanModule &mod);
    void requestInstallBatch(const QStringList &identifiers);
    void requestUpgrade(const QString &identifier);
    void requestUpgradeBatch(const QStringList &identifiers);
    void requestUninstall(const QString &identifier);
    void requestUninstallBatch(const QStringList &identifiers);
    // 只读：一次性级联卸载顺序（含目标，依赖者在前）；任一未安装返回空。
    QStringList uninstallPlan(const QStringList &identifiers) const;
    void requestImport(const QString &path);
    void requestDownloadSingle(const ckan::CkanModule &mod);
    void requestCancel();

signals:
    void indexRefreshed(CKanManager::IndexRefreshStatus status, const QString &error);
    void operationFinished(bool ok, const QString &message);
    void unmanagedScanFinished();
    void singleDownloadFinished(bool ok, const QString &identifier, const QString &error);
    void installProgress(const QString &identifier, int percent);
    void downloadProgress(const QString &identifier, qint64 doneBytes,
                          qint64 totalBytes, qint64 speedBps);

private:
    // 安装/卸载流程交互决策（真实 Qt 弹窗）。成员保证在其构造后、任何业务调用前注入。
    moddecision::Hooks m_decisions;
};

#endif // MODSCONTROLLER_H