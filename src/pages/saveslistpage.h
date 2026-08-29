#ifndef SAVESLISTPAGE_H
#define SAVESLISTPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include "../configmanager.h"

class SavesListPage : public QWidget
{
    Q_OBJECT
public:
    explicit SavesListPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadSaves();
    void refreshIcons(const QString& color);

signals:
    void backClicked();
    void saveSelected(const QString& saveFolderPath, const QString& instanceName);
    // 从存档管理页跳回实例管理详情页的某个二级 tab（0=游戏设置 1=DLC 2=模组管理 3=高级）
    void navToDetail(int detailIndex);
    // 从存档管理页触发挥详情页侧栏的动作：0=导出整合包 1=导入整合包 2=浏览
    void modpackActionRequested(int actionKind);

private slots:
    void onBackClicked();
    void onSaveItemDoubleClicked(QListWidgetItem* item);
    void onInstanceNavClicked();

private:
    void setupUI();

    QString m_instanceId;
    KSPInstance m_instance;

    QPushButton* m_backButton;
    QLabel* m_titleLabel;
    QListWidget* m_savesList;

    // 左侧实例管理二级菜单（与详情页一致）
    QWidget* m_instanceSidebar;
    QPushButton* m_navGameSettingsBtn;
    QPushButton* m_navDlcBtn;
    QPushButton* m_navModsBtn;
    QPushButton* m_navSavesBtn;
    QPushButton* m_navAdvancedBtn;
    QPushButton* m_navExportBtn;
    QPushButton* m_navImportBtn;
    QPushButton* m_navBrowseBtn;
};

#endif // SAVESLISTPAGE_H
