#ifndef SAVESTABPAGE_H
#define SAVESTABPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include "../configmanager.h"

// 实例详情页 - 存档管理 tab。
// 仅含存档列表；不含二级侧边栏/顶栏，导航与导出/导入/浏览均复用实例详情页。
class SavesTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit SavesTabPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadSaves();
    void refreshIcons(const QString& color);

signals:
    // 双击存档进入详情编辑器
    void saveSelected(const QString& saveFolderPath, const QString& instanceName, const QString& instanceId);

private slots:
    void onSaveItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUI();

    QString m_instanceId;
    KSPInstance m_instance;

    QListWidget* m_savesList;
};

#endif // SAVESTABPAGE_H