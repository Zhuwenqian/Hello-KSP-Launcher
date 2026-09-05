#ifndef GAMESETTINGSTABPAGE_H
#define GAMESETTINGSTABPAGE_H

#include <QWidget>
#include <QList>

#include "../instancemanager.h"

class QTreeWidget;
class QLineEdit;
class QStyledItemDelegate;

// 实例详情页「游戏设置」二级 tab：设置项树 + 实时搜索 + 保存。
// 业务经 InstanceManager 读写 settings，页面自身持有设置缓存并负责过滤/回写。
class GameSettingsTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit GameSettingsTabPage(QWidget *parent = nullptr);

    // 装载指定游戏目录的设置项（清空重建树，并按当前搜索词过滤）。
    void loadGameSettings(const QString &gamePath);
    // 保存当前树中修改后的设置；成功弹「已保存」。返回是否成功。
    bool saveGameSettings();

private slots:
    void onSettingsSearchChanged(const QString &text);
    void onSaveSettingsClicked();

private:
    QTreeWidget* m_settingsTree = nullptr;
    QLineEdit*   m_settingsSearchEdit = nullptr;
    QList<GameSetting> m_currentSettings;
    QString m_gamePath; // 当前绑定的游戏目录（保存设置的回写目标）
};

#endif // GAMESETTINGSTABPAGE_H