// 实例管理详情页 - 整合包导入/导出功能实现
#include "instancedetailpage.h"
#include "../ckanmanager.h"
#include "ckan/modpackio.h"
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <memory>

void InstanceDetailPage::onExportModpackClicked()
{
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"), tr("实例路径为空，无法导出整合包。"));
        m_exportModpackBtn->setChecked(false);
        return;
    }

    // 弹出菜单让用户选择导出方式
    QMenu menu(this);
    QAction *zipAction = menu.addAction(tr("打包 GameData 为 ZIP"));
    QAction *ckanAction = menu.addAction(tr("导出为 CKAN 文件"));
    // 弹菜单时取消按钮选中态，避免状态残留
    m_exportModpackBtn->setChecked(false);
    const QPoint pos = m_exportModpackBtn->mapToGlobal(QPoint(0, m_exportModpackBtn->height() + 4));
    QAction *selected = menu.exec(pos);
    if (selected == zipAction)
        exportAsZip();
    else if (selected == ckanAction)
        exportAsCkan();
}

void InstanceDetailPage::exportAsZip()
{
    QString gameDataPath = QDir(m_instance.path).filePath("GameData");
    if (!QDir(gameDataPath).exists()) {
        QMessageBox::warning(this, tr("导出失败"), tr("GameData 目录不存在，无法导出整合包。"));
        return;
    }

    // 默认文件名：实例名.zip，保存在启动器根目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString defaultFileName = m_instance.name + ".zip";
    QString defaultFilePath = QDir(appDir).filePath(defaultFileName);

    QString zipFilePath = QFileDialog::getSaveFileName(
        this,
        tr("导出整合包 - 选择保存位置"),
        defaultFilePath,
        tr("ZIP 文件 (*.zip)")
    );

    if (zipFilePath.isEmpty()) {
        return; // 用户取消
    }

    // 确保以 .zip 结尾
    if (!zipFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
        zipFilePath += ".zip";
    }

    // 选择完成后，取消所有按钮选中状态，回到游戏设置
    m_gameSettingsBtn->setChecked(true);
    m_exportModpackBtn->setChecked(false);
    m_contentStack->setCurrentIndex(0);

    // 创建进度对话框
    QProgressDialog progressDialog(tr("正在导出整合包..."), "取消", 0, 100, this);
    progressDialog.setWindowTitle(tr("导出整合包"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setValue(0);
    progressDialog.show();

    // 使用 QCoreApplication::processEvents 确保进度对话框显示
    QCoreApplication::processEvents();

    bool cancelled = false;
    bool success = InstanceManager::instance().exportModpack(
        m_instance.path,
        zipFilePath,
        [&](int progress) {
            // 在 UI 线程中更新进度
            QMetaObject::invokeMethod(&progressDialog, [&progressDialog, &cancelled, progress]() {
                if (progressDialog.wasCanceled()) {
                    cancelled = true;
                    return;
                }
                progressDialog.setValue(progress);
            }, Qt::QueuedConnection);
            // 处理事件以保持 UI 响应
            QCoreApplication::processEvents();
        }
    );

    progressDialog.close();

    if (cancelled) {
        QFile::remove(zipFilePath);
        QMessageBox::information(this, "提示", tr("导出已取消。"));
    } else if (success) {
        QMessageBox::information(this, tr("导出成功"),
            tr("整合包已成功导出到：\n%1").arg(QDir::toNativeSeparators(zipFilePath)));
    } else {
        QMessageBox::warning(this, tr("导出失败"), tr("导出整合包时发生错误，请检查磁盘空间和权限。"));
    }
}

void InstanceDetailPage::exportAsCkan()
{
    // 默认文件名：实例名.ckan，保存在启动器根目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString defaultFileName = m_instance.name + ".ckan";
    QString defaultFilePath = QDir(appDir).filePath(defaultFileName);

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出 CKAN 文件 - 选择保存位置"),
        defaultFilePath,
        tr("CKAN 文件 (*.ckan)")
    );

    if (filePath.isEmpty()) {
        return; // 用户取消
    }

    // 确保以 .ckan 结尾
    if (!filePath.endsWith(".ckan", Qt::CaseInsensitive)) {
        filePath += ".ckan";
    }

    QString error;
    const QByteArray json = CKanManager::instance().exportModpackCkan(&error);
    if (json.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"),
                             error.isEmpty() ? tr("导出 CKAN 文件失败。") : error);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件：%1").arg(file.errorString()));
        return;
    }
    file.write(json);
    file.close();

    QMessageBox::information(this, tr("导出成功"),
        tr("CKAN 文件已成功导出到：\n%1").arg(QDir::toNativeSeparators(filePath)));
}

void InstanceDetailPage::onImportModpackClicked()
{
    m_importModpackBtn->setChecked(false);
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(this, tr("导入失败"), tr("实例路径为空，无法导入整合包。"));
        return;
    }
    if (!QDir(m_instance.path + QStringLiteral("/GameData")).exists()) {
        QMessageBox::warning(this, tr("导入失败"), tr("GameData 目录不存在，无法导入整合包。"));
        return;
    }

    // 弹出菜单让用户选择导入方式
    QMenu menu(this);
    QAction *zipAction = menu.addAction(tr("从 ZIP 导入"));
    QAction *ckanAction = menu.addAction(tr("从 .ckan 文件导入"));
    const QPoint pos = m_importModpackBtn->mapToGlobal(QPoint(0, m_importModpackBtn->height() + 4));
    QAction *selected = menu.exec(pos);
    if (selected == zipAction)
        importFromZip();
    else if (selected == ckanAction)
        importFromCkan();
}

void InstanceDetailPage::importFromZip()
{
    const QString zipFilePath = QFileDialog::getOpenFileName(
        this, tr("导入整合包 - 选择 ZIP 文件"), QString(),
        tr("ZIP 文件 (*.zip)"));
    if (zipFilePath.isEmpty()) return; // 用户取消

    // 先校验 ZIP 内确实包含 GameData（避免清空后才报错）
    QString prefix, error;
    if (!ckan::modpackZipGameDataPrefix(zipFilePath, &prefix, &error)) {
        QMessageBox::warning(this, tr("导入失败"), error.isEmpty()
            ? tr("所选文件不是有效的整合包（缺少 GameData 目录）。") : error);
        return;
    }

    // 清空提示：删除 GameData 下除 Squad/SquadExpansion 外的所有内容
    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this, tr("导入整合包"),
        tr("导入将删除当前实例 GameData 中除 Squad、SquadExpansion 外的所有模组，\n"
           "并用 ZIP 中的模组替换。是否继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    // 完成后回到游戏设置并取消按钮选中态
    m_gameSettingsBtn->setChecked(true);
    m_importModpackBtn->setChecked(false);
    m_contentStack->setCurrentIndex(0);

    // 解压/清空为磁盘密集操作，放到后台线程执行，避免阻塞主线程（大整合包卡顿）。
    // 进度经跨线程队列信号回到主线程更新进度框；取消标志与结果经堆对象在工作线程与
    // 主线程间共享（按值捕获共享指针），进度框用 QPointer 防止页面销毁后访问悬垂对象。
    QPointer<QProgressDialog> progressDialog(
        new QProgressDialog(tr("正在导入整合包..."), "取消", 0, 100, this));
    progressDialog->setWindowTitle(tr("导入整合包"));
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(0);
    progressDialog->setValue(0);
    progressDialog->show();

    auto cancelRequested = std::make_shared<std::atomic_bool>(false);
    const QString gameDir = m_instance.path;

    auto watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    auto future = QtConcurrent::run(
        [zipFilePath, gameDir, cancelRequested, progressDialog]() -> QPair<bool, QString> {
            QString err;
            // 进度回调在工作线程调用：仅投递到主线程更新进度框（Qt::QueuedConnection），
            // 不再调用 QCoreApplication::processEvents（主线程专属，工作线程调用不安全）。
            const auto onProgress = [cancelRequested, progressDialog](int permille) {
                QMetaObject::invokeMethod(progressDialog.data(),
                    [cancelRequested, progressDialog, permille]() {
                        if (progressDialog.isNull())
                            return;
                        if (progressDialog->wasCanceled()) {
                            cancelRequested->store(true);
                            return;
                        }
                        progressDialog->setValue(permille / 10);
                    }, Qt::QueuedConnection);
            };
            const bool ok = ckan::modpackImportGameData(zipFilePath, gameDir,
                                                        onProgress, cancelRequested.get(), &err);
            return qMakePair(ok, err);
        });
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this,
            [this, watcher, zipFilePath, cancelRequested, progressDialog]() {
        if (progressDialog) progressDialog->close();
        const QPair<bool, QString> r = watcher->result();
        watcher->deleteLater();

        if (cancelRequested->load()) {
            QMessageBox::information(this, tr("提示"), tr("导入已取消，原模组可能已被部分替换。"));
        } else if (r.first) {
            // 导入后重建 CKan（注册表已清空、文件已替换），下次进入模组页时重新扫描
            CKanManager::instance().closeInstance();
            prepareMods();
            // 预填充模型，使模组管理页立即反映解压出的内容
            maybePopulateMods();
            QMessageBox::information(this, tr("导入成功"),
                tr("整合包已导入到当前实例：\n%1").arg(QDir::toNativeSeparators(zipFilePath)));
        } else {
            QMessageBox::warning(this, tr("导入失败"),
                r.second.isEmpty() ? tr("导入整合包时发生错误。") : r.second);
            // 导入失败仍需重建 CKan，避免旧的注册表状态残留
            CKanManager::instance().closeInstance();
            prepareMods();
        }
        if (progressDialog) progressDialog->deleteLater();
    });
    watcher->setFuture(future);
}

void InstanceDetailPage::importFromCkan()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("导入整合包 - 选择 CKAN 文件"), QString(),
        tr("CKAN 文件 (*.ckan)"));
    if (filePath.isEmpty()) return; // 用户取消

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("导入失败"),
            tr("无法读取文件：%1").arg(file.errorString()));
        return;
    }
    const QByteArray json = file.readAll();
    file.close();

    QString error;
    const QStringList identifiers = ckan::modpackCkanDepends(json, &error);
    if (identifiers.isEmpty()) {
        QMessageBox::warning(this, tr("导入失败"),
            error.isEmpty() ? tr("CKAN 文件不包含任何可安装模组。") : error);
        return;
    }

    // 提醒：将删除现有模组并从仓库下载清单中的模组
    const QMessageBox::StandardButton confirm = QMessageBox::question(
        this, tr("导入整合包"),
        tr("导入将删除当前实例 GameData 中除 Squad、SquadExpansion 外的所有模组，\n"
           "并从仓库解析下载以下 %1 个模组及其依赖：\n\n%2\n\n是否继续？")
            .arg(identifiers.size()).arg(identifiers.join(QStringLiteral("\n"))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (confirm != QMessageBox::Yes) return;

    // 先清空现有模组（保留 Squad/SquadExpansion），再跳转到模组管理界面下载
    QString clearError;
    if (!ckan::modpackClearGameData(m_instance.path, &clearError)) {
        QMessageBox::warning(this, tr("导入失败"),
            tr("导入整合包时发生错误。\n%1").arg(clearError));
        return;
    }

    // 注册表被删除，立即刷新 libckan 内存中的已安装数据，避免旧 registry 滞留
    CKanManager::instance().reloadRegistry();

    // 跳转到模组管理界面进行下载
    m_importModpackBtn->setChecked(false);
    m_modsBtn->setChecked(true);
    m_contentStack->setCurrentIndex(2);
    m_modsTabActive = true;

    CKanManager &mgr = CKanManager::instance();
    // 确保实例绑定与数据就绪；索引未就绪则进入待装清单，就绪后自动开始安装
    prepareMods();
    if (mgr.indexReady()) {
        mgr.installBatchAsync(identifiers);
    } else {
        m_pendingCkanIdentifiers = identifiers;
        setDetailNote(tr("正在加载 CKAN 仓库索引，就绪后将自动开始安装所选模组..."));
    }
}
