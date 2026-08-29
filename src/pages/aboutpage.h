#ifndef ABOUTPAGE_H
#define ABOUTPAGE_H

#include <QWidget>
#include <QScrollArea>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QPixmap;

// 关于页：关于 / 鸣谢 / 依赖 / 法律声明 四大板块。
// 每个条目渲染为独立带背景的卡片按钮（标题+描述合并），点击后用系统浏览器打开对应 URL。
class AboutPage : public QWidget
{
    Q_OBJECT
public:
    explicit AboutPage(QWidget *parent = nullptr);

private:
    // 板块小标题
    QLabel* makeSectionTitle(const QString &text, QWidget *parent);
    // 生成一张卡片按钮：标题 + 描述（可选前置图标），整卡可点击打开 url
    QPushButton* makeLinkRow(const QString &title, const QString &desc = QString(),
                             const QString &url = QString(), const QPixmap &icon = QPixmap());
    void openUrl(const QString &url);

    QScrollArea* m_scrollArea;
};

#endif // ABOUTPAGE_H