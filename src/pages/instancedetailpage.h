#ifndef INSTANCEDETAILPAGE_H
#define INSTANCEDETAILPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMenu>
#include "../configmanager.h"
#include "../instancemanager.h"

class InstanceDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstanceDetailPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadCurrentInstance();
    void refreshIcons(const QString& color);

signals:
    void backClicked();
    void savesManageRequested();

private slots:
    void onBackClicked();
    void onNavButtonClicked();
    void refreshData();
    void onSaveSettingsClicked();
    void onSaveLaunchArgsClicked();
    void onExportModpackClicked();
    void onBrowseClicked();
    void onBrowseActionTriggered();

private:
    void setupUI();
    void setupGameSettingsTab();
    void setupDLCTab();
    void setupModsTab();
    void setupAdvancedTab();
    void setupBrowseMenu();
    void updateBrowseMenuState();
    void openBrowseTarget(const QString& path);
    void loadGameSettings();
    bool saveGameSettings();
    void loadDLCs();
    void loadMods();
    void loadLaunchArgs();

    QList<GameSetting> m_currentSettings;

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

    // 浏览菜单
    QPushButton* m_browseBtn;
    QMenu* m_browseMenu;
    QAction* m_browseRootAction;
    QList<QAction*> m_browseActions;
    QStringList m_browsePaths;

    // Game Settings tab
    QTreeWidget* m_settingsTree;

    // DLC tab
    QListWidget* m_dlcList;

    // Mods tab
    QListWidget* m_modList;

    // Advanced tab
    QLineEdit* m_launchArgsEdit;
    QPushButton* m_saveLaunchArgsBtn;
};

#endif // INSTANCEDETAILPAGE_H
