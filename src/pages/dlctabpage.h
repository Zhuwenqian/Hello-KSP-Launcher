#ifndef DLCTABPAGE_H
#define DLCTABPAGE_H

#include <QWidget>

class QListWidget;

// 实例详情页「DLC」二级 tab：仅展示当前实例 DLC 的安装状态。
class DlcTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit DlcTabPage(QWidget *parent = nullptr);

    // 装载指定游戏目录下检测到的 DLC 列表（仅显示存在状态，不下载不安装）。
    void loadDLCs(const QString &gamePath);

private:
    QListWidget* m_dlcList = nullptr;
};

#endif // DLCTABPAGE_H