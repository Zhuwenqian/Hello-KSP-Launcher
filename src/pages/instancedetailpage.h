#ifndef INSTANCEDETAILPAGE_H
#define INSTANCEDETAILPAGE_H

#include <QWidget>

#include <QPushButton>
#include <QStackedWidget>

#include "../configmanager.h"

class QLabel;
class QMenu;
class QAction;
class ModsTabPage;
class GameSettingsTabPage;
class DlcTabPage;
class AdvancedTabPage;
class ModpackController;

// 实例详情页：仅作为壳 + 侧栏导航 + 对外入口。
// 各二级 tab（游戏设置/DLC/模组管理/高级）已拆为独立页面类，由本页持有并接线；
// 整合包导入/导出业务流程拆到 ModpackController，本页只做菜单入口与信号转发。
class InstanceDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstanceDetailPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadCurrentInstance();
    void refreshIcons(const QString& color);
    // 外部切换详情页的二级 tab：0=游戏设置 1=DLC 2=模组管理 3=高级（供存档等实例子页跳回）
    void showSection(int detailIndex);
    // 供存档等实例子页触发原属于详情页侧栏的动作
    void triggerExportModpack();
    void triggerImportModpack();
    void triggerBrowse();

signals:
    void backClicked();
    void savesManageRequested();

private slots:
    void onBackClicked();
    void onNavButtonClicked();
    void onExportModpackClicked();
    void onImportModpackClicked();
    void onBrowseClicked();
    void onBrowseActionTriggered();
    // ModpackController 信号 → 页面导航/模组页接线
    void onModpackShowSettings();
    void onModpackReloadMods();
    void onModpackModsInstall(const QStringList &identifiers);

private:
    void setupUI();
    void setupBrowseMenu();
    void updateBrowseMenuState();
    void openBrowseTarget(const QString& path);
    // 装载各 tab 数据并绑定模组页实例
    void refreshData();

    QString m_instanceId;
    KSPInstance m_instance;

    QPushButton* m_backButton;
    QLabel* m_titleLabel;
    QWidget* m_detailSidebar;
    QStackedWidget* m_contentStack;

    QPushButton* m_gameSettingsBtn;
    QPushButton* m_dlcBtn;
    QPushButton* m_modsBtn;
    QPushButton* m_savesBtn;
    QPushButton* m_advancedBtn;
    QPushButton* m_exportModpackBtn;
    QPushButton* m_importModpackBtn;
    QPushButton* m_browseBtn;

    // 浏览菜单
    QMenu* m_browseMenu;
    QAction* m_browseRootAction;
    QList<QAction*> m_browseActions;
    QStringList m_browsePaths;

    // 拆出的二级 tab 页面
    GameSettingsTabPage* m_gameSettingsTabPage = nullptr;
    DlcTabPage* m_dlcTabPage = nullptr;
    ModsTabPage* m_modsTabPage = nullptr;
    AdvancedTabPage* m_advancedTabPage = nullptr;

    // 整合包导入/导出流程控制器
    ModpackController* m_modpackController = nullptr;
};

#endif // INSTANCEDETAILPAGE_H