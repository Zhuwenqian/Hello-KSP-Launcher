#ifndef INSTANCEDETAILPAGE_H
#define INSTANCEDETAILPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include "../configmanager.h"
#include "../instancemanager.h"

class InstanceDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstanceDetailPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadCurrentInstance();

signals:
    void backClicked();

private slots:
    void onBackClicked();
    void onNavButtonClicked();
    void refreshData();
    void onSaveSettingsClicked();
    void onSaveLaunchArgsClicked();

private:
    void setupUI();
    void setupGameSettingsTab();
    void setupDLCTab();
    void setupModsTab();
    void setupAdvancedTab();
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
    QPushButton* m_advancedBtn;

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
