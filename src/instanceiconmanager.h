#ifndef INSTANCEICONMANAGER_H
#define INSTANCEICONMANAGER_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QMap>
#include <QSet>

// 管理实例列表项图标：
//  - 按实例名后缀判定图标来源：RP-1(最高优先级) > RSS(含 Sol 分支) > 读取 exe 图标。
//  - RSS/RP-1 大图与 exe 图标均在后台线程加载/提取并缓存，避免列表刷新卡顿。
class InstanceIconManager : public QObject
{
    Q_OBJECT
public:
    static InstanceIconManager& instance();

    // 请求为某实例计算并异步交付图标；完成时发出 iconReady(instanceId, image)。
    // image 为空表示无可用图标（留空）。
    void requestIcon(const QString& instanceId, const QString& instanceName,
                     const QString& exePath);

    // 供单元测试/自检使用的纯函数：根据实例名判定图标来源。
    enum class Source { None, Rss, Rp1, Exe };
    static Source resolveSource(const QString& instanceName);

signals:
    void iconReady(const QString& instanceId, const QImage& icon);

private:
    explicit InstanceIconManager(QObject *parent = nullptr);

    // 静态大图缓存（按文件路径）
    QMap<QString, QImage> m_staticCache;
    // exe 图标缓存（按 exe 路径）
    QMap<QString, QImage> m_exeCache;
    // 正在后台加载的实例 id（去重，避免同实例重复发起）
    QSet<QString> m_pending;
};

#endif // INSTANCEICONMANAGER_H