#ifndef SAVESTABPAGE_H
#define SAVESTABPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QFutureWatcher>
#include <QVector>
#include <QPair>
#include "../configmanager.h"
#include "../instancemanager.h"

// 实例详情页 - 存档管理 tab。
// 仅含存档列表；不含二级侧边栏/顶栏，导航与导出/导入/浏览均复用实例详情页。
class SavesTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit SavesTabPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    // 进入存档 tab 时调用：后台线程扫描并解析存档，完成后回主线程填充列表（不阻塞 UI）。
    void loadSaves();
    void refreshIcons(const QString& color);

signals:
    // 双击存档进入详情编辑器
    void saveSelected(const QString& saveFolderPath, const QString& instanceName, const QString& instanceId);

private slots:
    void onSaveItemDoubleClicked(QListWidgetItem* item);
    void onDeleteSaveClicked(const QString& saveFolderPath);
    void onSavesLoadFinished();

private:
    void setupUI();

    QString m_instanceId;
    KSPInstance m_instance;

    QListWidget* m_savesList;
    QFutureWatcher<QVector<QPair<QString, SaveInfo>>>* m_savesLoadWatcher = nullptr;
};

#endif // SAVESTABPAGE_H