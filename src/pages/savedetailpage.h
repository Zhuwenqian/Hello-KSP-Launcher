#ifndef SAVEDETAILPAGE_H
#define SAVEDETAILPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QProgressDialog>
#include "../instancemanager.h"

class SaveDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit SaveDetailPage(QWidget *parent = nullptr);

    void setSavePath(const QString& saveFolderPath);
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
    void onCreateBackupClicked();
    void onRefreshBackupsClicked();
    void onDeleteBackupClicked(const QString& filePath);
    void onRevealBackupClicked(const QString& filePath);

private:
    void setupUI();
    void setupSaveInfoTab();
    void setupKerbalsTab();
    void setupBackupsTab();
    void showKerbalDetail(const KerbalInfo& kerbal);
    bool collectKerbalData(QList<KerbalInfo>& kerbals);
    void refreshBackupList();

    QString m_saveFolderPath;
    QString m_saveName;
    SaveInfo m_saveInfo;
    QList<KerbalInfo> m_kerbals;

    QPushButton* m_backButton;
    QPushButton* m_homeButton;
    QLabel* m_titleLabel;

    QWidget* m_sidebar;
    QStackedWidget* m_contentStack;

    QPushButton* m_saveInfoBtn;
    QPushButton* m_kerbalsBtn;
    QPushButton* m_backupsBtn;

    // 存档信息页面
    QTreeWidget* m_infoTree;

    // Kerbals页面
    QStackedWidget* m_kerbalsStack;
    QListWidget* m_kerbalList;
    QWidget* m_kerbalDetailWidget;
    QTreeWidget* m_kerbalDetailTree;
    QPushButton* m_backToKerbalListBtn;
    QPushButton* m_saveKerbalsBtn;
    QString m_currentKerbalName;

    // 备份管理页面
    QWidget* m_backupsTab;
    QListWidget* m_backupList;
    QPushButton* m_createBackupBtn;
    QPushButton* m_refreshBackupsBtn;
};

#endif // SAVEDETAILPAGE_H
