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
#include <QTableView>
#include <QTextEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QCheckBox>
#include "../configmanager.h"
#include "../instancemanager.h"
#include "modtablemodel.h"

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

    // mod 管理
    void onModSearchChanged(const QString &text);
    void onModFilterChanged(int index);
    void onShowIncompatibleToggled(bool checked);
    void onCompatVersionsClicked();
    void onRefreshModsClicked();
    void onModSelectionChanged();
    void onModDoubleClicked(const QModelIndex &index);
    void onInstallModClicked();
    void onUninstallModClicked();
    void onUpgradeModClicked();
    void onSelectAllClicked();
    void onIndexRefreshed(bool ok, const QString &error);
    void onModOperationFinished(bool ok, const QString &message);
    void onDownloadProgress(const QString &identifier, qint64 doneBytes,
                            qint64 totalBytes, qint64 speedBps);
    void onCancelDownloadClicked();

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
    // 将当前实例勾选的兼容版本区间应用到过滤代理与 CKanManager（供安装/依赖解析使用）
    void applyCompatRange();
    void updateModActionButtons();
    void updateSelectAllButtonText();
    void setModButtonsEnabled(bool enabled);
    void showModDetails(const ckan::CkanModule &mod);
    void showDownloadProgress();
    void hideDownloadProgress();

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
    ModsTableModel* m_modsModel;
    ModsFilterProxyModel* m_modsProxy;
    QTableView* m_modTable;
    QLineEdit* m_modSearchEdit;
    QComboBox* m_modFilterCombo;
    QPushButton* m_refreshModsBtn;
    QPushButton* m_selectAllBtn;
    QCheckBox*   m_showIncompatCheck;
    QPushButton* m_compatBtn; // 兼容版本设置按钮
    QPushButton* m_installModBtn;
    QPushButton* m_uninstallModBtn;
    QPushButton* m_upgradeModBtn;
    QTextEdit* m_modDetailText;
    QString m_currentModIdentifier;
    // 下载进度条与取消
    QWidget*  m_modProgressWidget;
    QProgressBar* m_modProgressBar;
    QLabel*   m_modProgressLabel;
    QPushButton* m_cancelDownloadBtn;

    // Advanced tab
    QLineEdit* m_launchArgsEdit;
    QPushButton* m_saveLaunchArgsBtn;
};

#endif // INSTANCEDETAILPAGE_H
