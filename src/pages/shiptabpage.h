#ifndef SHIPTABPAGE_H
#define SHIPTABPAGE_H

#include <QWidget>
#include <QListWidget>
#include <QStringList>
#include <QFutureWatcher>
#include <QVector>
#include "../configmanager.h"
#include "../instancemanager.h"

class QTabWidget;
class QStackedWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QPlainTextEdit;

// 后台线程返回的一条飞船列表项：类型(VAB/SPH) + 完整路径 + 解析信息
struct ShipListEntry {
    QString type;
    QString craftPath;
    ShipInfo info;
};

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
    void setShipsBase(const QString& basePath);   // 自定义飞船根目录（Ships 的父目录）；实例传实例根、存档传 saves/存档名
    // 是否启用「预制件」tab。说明：预制件属于单个存档（实例根/saves/存档名/Subassemblies），
    // 实例模式下实例根没有单一预制件目录，故仅存档详情页的飞船管理启用。
    void setSubassembliesEnabled(bool on);
    // 进入飞船 tab 时调用：后台线程扫描并解析 VAB/SPH（+预制件）飞船列表，完成后回主线程填充（不阻塞 UI）。
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
    void onShipsLoadFinished();

private:
    void setupUI();
    void addShipRow(QListWidget* list, const QString& craftPath, const QString& type, const ShipInfo& info);
    void showDetail(const QString& path, int typeIndex);
    void loadDetail(const QString& path, int typeIndex);
    void importShipFiles(const QStringList& paths, int typeIndex);
    void refreshImportButtonLabel(int tabIndex);
    // 创建并配置一个类型列表（对象名/拖拽/点击连接复用）；调用方 setSubassembliesEnabled 按需创建第 3 个
    QListWidget* createTypeList(QWidget* parent);
    // 类型 i 对应的目录：VAB/SPH 在 Ships/{type}，预制件在 存档根/Subassemblies
    QString dirForType(int typeIndex) const;
    QString tabLabelForType(int typeIndex) const;
    // 类型名（entry.type，即 VAB/SPH/Subassemblies）→ 类型下标；未知返回 0
    int typeIndexFromName(const QString& type) const;

    QString m_shipsBasePath;
    bool m_subassembliesEnabled = false;
    // 是否为存档模式（setShipsBase 传存档目录）：玩家自制载具缩略图（游戏根目录/thumbs）只在存档模式下读取，
    // 因缩略图按存档名命名；实例模式飞船多存档共享、无单一存档名，保持不读该目录。
    bool m_saveMode = false;

    QTabWidget* m_typeTabs;
    QListWidget* m_lists[3];     // 0=VAB, 1=SPH, 2=Subassemblies(预制件，仅存档模式启用)
    QStackedWidget* m_stack;
    QPushButton* m_importBtn;

    // 详情页控件
    QPushButton* m_backButton;
    QLabel* m_detailName;
    QLabel* m_detailVersion;
    QLabel* m_detailPartCount;
    QPlainTextEdit* m_detailDescription;
    QLabel* m_detailThumb;

    QFutureWatcher<QVector<ShipListEntry>>* m_shipsLoadWatcher = nullptr;
};

#endif // SHIPTABPAGE_H