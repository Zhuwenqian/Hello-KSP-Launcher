#ifndef SAVEDETAILPAGE_H
#define SAVEDETAILPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QProgressDialog>

class QTimer;
#include "../instancemanager.h"

class ShipTabPage;
class QLineEdit;
class QTabWidget;

class SaveDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit SaveDetailPage(QWidget *parent = nullptr);

    void setSavePath(const QString& saveFolderPath, const QString& instanceName, const QString& instanceId);
    void loadSaveData();
    void refreshIcons(const QString& color);

signals:
    void backClicked();
    void homeClicked();

private slots:
    void onBackClicked();
    void onHomeClicked();
    void onNavButtonClicked();
    void onKerbalItemClicked(QListWidgetItem* item);
    void onSaveKerbalsClicked();
    void onBackToKerbalList();
    void onKerbalDeleteRequested(int index);
    void onKerbalRenameRequested(int index);
    void onCreateBackupClicked();
    void onRefreshBackupsClicked();
    void onDeleteBackupClicked(const QString& filePath);
    void onRevealBackupClicked(const QString& filePath);
    void onRestoreBackupClicked(const QString& filePath);

private:
    void setupUI();
    void setupSaveInfoTab();
    void setupKerbalsTab();
    void setupShipsTab();
    void setupBackupsTab();
    void showKerbalDetail(const KerbalInfo& kerbal);
    void populateKerbalList();
    bool collectKerbalData(QList<KerbalInfo>& kerbals);
    void refreshBackupList();
    void rebuildBackupList();

    QString m_saveFolderPath;
    QString m_saveName;
    QString m_instanceName;
    QString m_instanceId;
    SaveInfo m_saveInfo;
    QList<KerbalInfo> m_kerbals;

    QPushButton* m_backButton;
    QPushButton* m_homeButton;
    QLabel* m_titleLabel;

    QWidget* m_sidebar;
    QStackedWidget* m_contentStack;

    QPushButton* m_saveInfoBtn;
    QPushButton* m_kerbalsBtn;
    QPushButton* m_shipsBtn;
    QPushButton* m_backupsBtn;

    // 存档信息页面
    QTreeWidget* m_infoTree;

    // Kerbals页面
    QStackedWidget* m_kerbalsStack;
    QLineEdit* m_kerbalSearchEdit;
    QTimer* m_kerbalSearchDebounce = nullptr; // 搜索防抖：populateKerbalList 全量重建列表，逐键触发开销大
    QTabWidget* m_kerbalTabWidget;
    QListWidget* m_applicantList; // type=Applicant（及未识别类型的兜底）
    QListWidget* m_crewList;      // type=Crew
    QWidget* m_kerbalDetailWidget;
    QTreeWidget* m_kerbalDetailTree;
    QPushButton* m_saveKerbalsBtn;
    QString m_currentKerbalName;

    // 备份管理页面
    QWidget* m_backupsTab;
    QListWidget* m_backupList;
    QLineEdit* m_backupSearchEdit;
    QList<BackupInfo> m_backups; // 最近一次读取的全部备份（供搜索过滤）
    QPushButton* m_createBackupBtn;
    QPushButton* m_refreshBackupsBtn;

    // 飞船管理页面（复用实例详情页的 ShipTabPage）
    ShipTabPage* m_shipsPage;
};

#endif // SAVEDETAILPAGE_H
