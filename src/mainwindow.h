#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QMenu>
#include <QProcess>
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

private slots:
    void applyTheme(const QString& theme);
    void onNavButtonClicked();
    void onAddInstanceRequested();
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
    void onSaveSelected(const QString& savePath);
    void onBackFromSaveDetail();
    void onHomeFromSaveDetail();

private:
    void setupUI();
    void setupSidebar();
    void setupLaunchBar();
    void showPage(QWidget* page);
    void setNavButtonChecked(QPushButton* btn);
    void addTestInstanceIfEmpty();
    void refreshIcons(const QString& theme);

    QWidget* m_centralWidget;
    QWidget* m_sidebar;
    QStackedWidget* m_contentStack;
    QWidget* m_launchBar;

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
