#ifndef MODDECISION_H
#define MODDECISION_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "ckan/ckan.h"

// 安装/卸载过程中的交互决策：把原本内嵌在业务层（CKanManager）的 UI 弹窗
// 抽象为可注入的回调钩子。业务层只消费决策结果，不构造任何 Qt Widget；
// 真实 Qt 弹窗由 UI 层提供（后续统一由 ModsController 注入），测试可注入桩替代。
namespace moddecision {

// 文件夹冲突处理决策（下载完成后与手动占用折叠冲突）
enum class ConflictAction { OverwriteAll, DeleteOld, Cancel };
struct ConflictChoice {
    ConflictAction action = ConflictAction::OverwriteAll;
    QStringList foldersToDelete; // 仅 action==DeleteOld 时有效
};

// 磁盘空间不足提示所需信息（纯标量，供弹窗展示；不向业务层泄漏 QStorageInfo）
struct DiskSpacePrompt {
    bool forDownload = false; // true=下载缓存盘，false=游戏盘
    QString path;             // 被检查的路径
    QString rootPath;         // 所在磁盘根路径
    qint64 required = 0;      // 所需字节
    qint64 available = 0;     // 可用字节
};

// 各类决策回调
using ConflictHandler = std::function<ConflictChoice(const QStringList &conflicts)>;
using SuggestHandler =
    std::function<QVector<ckan::CkanModule>(const QVector<ckan::CkanModule> &suggests, bool *cancelled)>;
using ProviderHandler =
    std::function<QVector<ckan::CkanModule>(const QVector<ckan::ProviderChoice> &choices, bool *cancelled)>;
using DiskSpaceHandler = std::function<bool(const DiskSpacePrompt &prompt)>; // true=忽略继续
using ConfirmHandler = std::function<bool(const QString &title, const QString &message)>; // true=确认

struct Hooks {
    ConflictHandler  conflict;
    SuggestHandler   suggests;
    ProviderHandler  providers;
    DiskSpaceHandler diskSpace;
    ConfirmHandler   confirm;
};

// 真实 Qt 弹窗默认实现（UI 层）。CKanManager 构造时注入它以保证行为不回归；
// 测试 / 后续 ModsController 可注入自定义实现。
Hooks makeDefaultModDecisions();

} // namespace moddecision

#endif // MODDECISION_H