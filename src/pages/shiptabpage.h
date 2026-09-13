#ifndef SHIPTABPAGE_H
#define SHIPTABPAGE_H

#include <QWidget>
#include <QListWidget>
#include <QStringList>
#include "../configmanager.h"

class QTabWidget;
class QStackedWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QPlainTextEdit;

// 实例详情页 - 飞船管理 tab。
// 含 VAB/SPH 两个类型列表 + 列表↔详情两页切换：
//  - 默认进入 VAB；点击行（含详情缩略图）进入详情页，详情页带「返回」回列表。
//  - 每行右侧删除按钮把 craft 移到回收站。
//  - 顶部「导入飞船」按钮或把 .craft 文件拖到列表均可导入（按当前 tab 类型），重名提示覆盖。
class ShipTabPage : public QWidget
{
    Q_OBJECT
public:
    explicit ShipTabPage(QWidget *parent = nullptr);

    void setInstanceId(const QString& id);
    void loadShips();
    void refreshIcons(const QString& color);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onTabChanged(int index);
    void onShipItemClicked(QListWidgetItem* item);
    void onBackToListClicked();
    void onDeleteShipClicked(const QString& craftPath, const QString& type);
    void onImportShipsClicked();

private:
    void setupUI();
    void populateList(int typeIndex, const QString& path);
    void showDetail(const QString& path, int typeIndex);
    void loadDetail(const QString& path, int typeIndex);
    void importShipFiles(const QStringList& paths, int typeIndex);

    QString m_instanceId;
    KSPInstance m_instance;

    QTabWidget* m_typeTabs;
    QListWidget* m_lists[2];     // 0=VAB, 1=SPH
    QStackedWidget* m_stack;
    QPushButton* m_importBtn;

    // 详情页控件
    QPushButton* m_backButton;
    QLabel* m_detailName;
    QLabel* m_detailVersion;
    QPlainTextEdit* m_detailDescription;
    QLabel* m_detailThumb;
};

#endif // SHIPTABPAGE_H