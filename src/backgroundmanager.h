#ifndef BACKGROUNDMANAGER_H
#define BACKGROUNDMANAGER_H

#include <QObject>
#include <QPixmap>
#include <QString>

// BackgroundManager: 单例，负责管理启动器背景图片
// - 默认背景打包在资源中(:/backgrounds/default.png)
// - 用户自定义背景被复制到 <appDir>/backgrounds/user_bg_<uuid>.<ext>
// - 提供 backgroundChanged 信号通知 UI 更新
class BackgroundManager : public QObject
{
    Q_OBJECT
public:
    static BackgroundManager& instance();

    // 当前缓存的背景图
    const QPixmap& pixmap() const { return m_pixmap; }

    // 当前背景是否有效
    bool hasBackground() const { return !m_pixmap.isNull(); }

    // 初始化：从 ConfigManager 读取设置并加载背景
    // loadPixmapIfPossible 为 false 时只同步配置，不立即解码图片
    void initialize();

    // 加载默认背景(资源中的 default.png)
    void loadDefault();

    // 从绝对路径加载用户背景
    // 行为：若路径有效则复制到 appDir/backgrounds/，更新 ConfigManager
    // 返回 true 表示成功，失败时保留原背景不变
    bool setUserBackground(const QString& sourceFilePath);

    // 重置为默认背景
    void resetToDefault();

    // 当前背景来源的展示文本(用于设置界面)
    QString currentSourceDescription() const;

signals:
    void backgroundChanged();

private:
    explicit BackgroundManager(QObject* parent = nullptr);
    BackgroundManager(const BackgroundManager&) = delete;
    BackgroundManager& operator=(const BackgroundManager&) = delete;

    // 通用加载：path 为空表示加载默认资源；否则从文件加载
    void loadFromSource(const QString& path);

    // 用户背景存放目录
    QString backgroundDir() const;

    // 解析 ConfigManager 中的 backgroundPath：
    // - 空字符串：使用默认背景
    // - 关键字 "default"：使用默认背景
    // - 其他：视为用户背景文件绝对路径
    static QString resolveStoredPath(const QString& stored);

    QPixmap m_pixmap;
};

#endif // BACKGROUNDMANAGER_H
