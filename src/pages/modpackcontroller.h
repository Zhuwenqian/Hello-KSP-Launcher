#ifndef MODPACKCONTROLLER_H
#define MODPACKCONTROLLER_H

#include <QObject>
#include <QStringList>

#include "../configmanager.h"

class QWidget;

// 整合包导入/导出流程控制器：把原先堆在 InstanceDetailPage 的导出(ZIP/CKAN)与
// 导入(ZIP/.ckan)流程拆出。本类持有当前实例上下文并负责全部对话框/进度/确认/后台
// 任务编排；需要切回游戏设置、重建 CKan 后刷新模组页、或跳转模组页并带入待装清单时，
// 分别通过信号交给页面接线（门面只做薄导航，不直接摸模组页内部）。本类仍需 Qt 交互
// 控件（QFileDialog/QProgressDialog/QMessageBox），故归 页面/ 目录，由页面持有。
class ModpackController : public QObject
{
    Q_OBJECT
public:
    explicit ModpackController(QWidget *dialogParent, QObject *parent = nullptr);
    // 绑定当前实例（用于获得路径/名称；空路径时相关操作会直接提示失败）。
    void setInstance(const KSPInstance &inst);
    const KSPInstance &instance() const { return m_instance; }

    // 导出整合包：ZIP（打包 GameData）/ CKAN（生成元包 JSON）
    void exportAsZip();
    void exportAsCkan();
    // 导入整合包：从 zip 解压到 GameData / 解析 .ckan 依赖清单
    void importFromZip();
    void importFromCkan();

signals:
    // 流程需要返回游戏设置 tab。
    void showSettingsRequested();
    // 导入修改了 GameData/注册表，需要重建 CKan 并刷新模组页（含预填充模型）。
    void modsReloadRequested();
    // 导入 .ckan 清单：请求跳到模组管理并按清单安装。
    void modsInstallRequested(const QStringList &identifiers);

private:
    QWidget* m_dialogParent = nullptr;
    KSPInstance m_instance;
};

#endif // MODPACKCONTROLLER_H