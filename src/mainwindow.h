#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QMenu>
#include <QProcess>
#include <QSet>
#include "configmanager.h"
#include "pages/homepage.h"
#include "pages/instancelistpage.h"
#include "pages/instancedetailpage.h"
#include "pages/settingspage.h"
#include "pages/saveslistpage.h"
#include "pages/savedetailpage.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void applyTheme(const QString& theme);
    void onNavButtonClicked();
    void onAddInstanceRequested();
    void runSteamDiscovery();
    void onInstanceEntered(const QString& id);
    void onBackToInstanceList();
    void onBackToHome();
    void onLaunchClicked();
    void onLaunchSwitchClicked();
    void onGameStarted();
    void onGameFinished(int exitCode, QProcess::ExitStatus status);
    void onGameError(QProcess::ProcessError error);
    void onCurrentInstanceChanged();
    void switchInstanceFromMenu(QAction* action);
    void onSavesManageRequested();
    void onBackFromSavesList();
    void onSaveSelected(const QString& savePath, const QString& instanceName);
    void onSavesNavToDetail(int detailIndex);
    void onSavesModpackAction(int actionKind);
    bool toInstanceDetailPage();
    void onBackFromSaveDetail();
    void onHomeFromSaveDetail();
    void onBackgroundChanged();

private:
    void setupUI();
    void setupSidebar();
    void setupLaunchBar();
    void showPage(QWidget* page);
    void setNavButtonChecked(QPushButton* btn);
    
    void refreshIcons(const QString& theme);
    void updateBackgroundPixmap();
    void applyTransparency();

    // 对建议实例名去重：存在同名则追加 " (n)" 序号
    QString makeUniqueInstanceName(const QString &base, QSet<QString> &usedNames) const;

    QWidget* m_centralWidget;
    QWidget* m_sidebar;
    QStackedWidget* m_contentStack;
    QWidget* m_launchBar;
    QLabel* m_backgroundLabel;       // 底层背景 QLabel,显示背景图
    QWidget* m_contentContainer;     // 包裹 sidebar + content + launchBar,设置透明背景

    // Sidebar buttons
    QPushButton* m_homeBtn;
    QPushButton* m_instanceManageBtn;
    QPushButton* m_instanceListBtn;
    QPushButton* m_settingsBtn;

    // Pages
    HomePage* m_homePage;
    InstanceListPage* m_instanceListPage;
    InstanceDetailPage* m_instanceDetailPage;
    SettingsPage* m_settingsPage;
    SavesListPage* m_savesListPage;
    SaveDetailPage* m_saveDetailPage;

    // Launch bar
    QPushButton* m_launchButton;
    QPushButton* m_launchSwitchButton;
    QLabel* m_currentInstanceLabel;
    QMenu* m_switchMenu;

    QString m_currentTheme;
    bool m_gameRunning;
};

#endif // MAINWINDOW_H
