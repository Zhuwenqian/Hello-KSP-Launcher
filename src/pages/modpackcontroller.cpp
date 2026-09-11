// 整合包导入/导出流程控制器
#include "modpackcontroller.h"
#include "../ckanmanager.h"
#include "../instancemanager.h"
#include "../configmanager.h"
#include "ckan/modpackio.h"
#include "appversion.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <memory>

ModpackController::ModpackController(QWidget *dialogParent, QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
}

void ModpackController::setInstance(const KSPInstance &inst)
{
    m_instance = inst;
}

void ModpackController::exportAsZip()
{
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("导出失败"), tr("实例路径为空，无法导出整合包。"));
        return;
    }
    const QString gameDataPath = QDir(m_instance.path).filePath("GameData");
    if (!QDir(gameDataPath).exists()) {
        QMessageBox::warning(m_dialogParent, tr("导出失败"), tr("GameData 目录不存在，无法导出整合包。"));
        return;
    }

    // 自定义导出对话框：文件名 + 描述 + 保存路径（可浏览选择）
    const QString appDir = QCoreApplication::applicationDirPath();
    QDialog dlg(m_dialogParent);
    dlg.setWindowTitle(tr("导出整合包"));
    dlg.setModal(true);

    QFormLayout *form = new QFormLayout;
    QLineEdit *nameEdit = new QLineEdit(m_instance.name);
    QLineEdit *descEdit = new QLineEdit;
    QLineEdit *dirEdit = new QLineEdit(QDir::toNativeSeparators(appDir));
    QPushButton *browseBtn = new QPushButton(tr("浏览..."));
    connect(browseBtn, &QPushButton::clicked, this, [dirEdit, this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            m_dialogParent, tr("选择导出保存目录"), dirEdit->text());
        if (!dir.isEmpty())
            dirEdit->setText(QDir::toNativeSeparators(dir));
    });
    QHBoxLayout *dirRow = new QHBoxLayout;
    dirRow->addWidget(dirEdit, 1);
    dirRow->addWidget(browseBtn);

    form->addRow(tr("文件名(&N):"), nameEdit);
    form->addRow(tr("描述(&D):"), descEdit);
    form->addRow(tr("保存路径(&P):"), dirRow);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("导出"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *root = new QVBoxLayout(&dlg);
    root->addLayout(form);
    root->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return; // 用户取消

    const QString packageName = nameEdit->text().trimmed();
    if (packageName.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("导出失败"), tr("整合包名称不能为空。"));
        return;
    }
    // 名称可能自带 .zip 后缀，规范化后拼接保存路径
    const QString base = packageName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
                             ? packageName.left(packageName.size() - 4)
                             : packageName;
    const QString dirText = dirEdit->text().trimmed();
    const QString zipFilePath =
        QDir(dirText.isEmpty() ? appDir : dirText).filePath(base + QStringLiteral(".zip"));

    // 构造整合包元数据（zip 根目录 hkspl_package.json）
    const ckan::GameVersion gv = CKanManager::instance().detectedVersion();
    QJsonObject meta;
    meta.insert(QStringLiteral("launcherVersion"), QStringLiteral(HKSPL_APP_VERSION));
    meta.insert(QStringLiteral("name"), base);
    meta.insert(QStringLiteral("gameVersion"),
                gv.isValid() ? gv.withoutBuild().toString() : QString());
    meta.insert(QStringLiteral("description"), descEdit->text().trimmed());
    const QByteArray metaJson = QJsonDocument(meta).toJson(QJsonDocument::Compact);

    emit showSettingsRequested();

    // 创建进度对话框
    QProgressDialog progressDialog(tr("正在导出整合包..."), "取消", 0, 100, m_dialogParent);
    progressDialog.setWindowTitle(tr("导出整合包"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setValue(0);
    progressDialog.show();
    QCoreApplication::processEvents();

    bool cancelled = false;
    const bool success = InstanceManager::instance().exportModpack(
        m_instance.path, zipFilePath, metaJson,
        [&](int progress) {
            if (cancelled || progressDialog.wasCanceled()) {
                cancelled = true;
                return;
            }
            progressDialog.setValue(progress);
            QCoreApplication::processEvents();
        },
        [&]() -> bool {
            QCoreApplication::processEvents();
            if (progressDialog.wasCanceled()) {
                cancelled = true;
                return true;
            }
            return cancelled;
        });

    progressDialog.close();

    if (cancelled) {
        QFile::remove(zipFilePath);
        QMessageBox::information(m_dialogParent, "提示", tr("导出已取消。"));
    } else if (success) {
        QMessageBox::information(m_dialogParent, tr("导出成功"),
            tr("整合包已成功导出到：\n%1").arg(QDir::toNativeSeparators(zipFilePath)));
    } else {
        QMessageBox::warning(m_dialogParent, tr("导出失败"), tr("导出整合包时发生错误，请检查磁盘空间和权限。"));
    }
}

void ModpackController::exportAsCkan()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString defaultFileName = m_instance.name + ".ckan";
    const QString defaultFilePath = QDir(appDir).filePath(defaultFileName);

    QString filePath = QFileDialog::getSaveFileName(
        m_dialogParent, tr("导出 CKAN 文件 - 选择保存位置"), defaultFilePath, tr("CKAN 文件 (*.ckan)"));
    if (filePath.isEmpty())
        return; // 用户取消
    if (!filePath.endsWith(".ckan", Qt::CaseInsensitive))
        filePath += ".ckan";

    QString error;
    const QByteArray json = CKanManager::instance().exportModpackCkan(&error);
    if (json.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("导出失败"),
                             error.isEmpty() ? tr("导出 CKAN 文件失败。") : error);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(m_dialogParent, tr("导出失败"), tr("无法写入文件：%1").arg(file.errorString()));
        return;
    }
    file.write(json);
    file.close();

    QMessageBox::information(m_dialogParent, tr("导出成功"),
        tr("CKAN 文件已成功导出到：\n%1").arg(QDir::toNativeSeparators(filePath)));
}

void ModpackController::importFromZip()
{
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"), tr("实例路径为空，无法导入整合包。"));
        return;
    }
    if (!QDir(m_instance.path + QStringLiteral("/GameData")).exists()) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"), tr("GameData 目录不存在，无法导入整合包。"));
        return;
    }

    const QString zipFilePath = QFileDialog::getOpenFileName(
        m_dialogParent, tr("导入整合包 - 选择 ZIP 文件"), QString(), tr("ZIP 文件 (*.zip)"));
    if (zipFilePath.isEmpty())
        return; // 用户取消

    // 先校验 ZIP 内确实包含 GameData（避免清空后才报错）
    QString prefix, error;
    if (!ckan::modpackZipGameDataPrefix(zipFilePath, &prefix, &error)) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"), error.isEmpty()
            ? tr("所选文件不是有效的整合包（缺少 GameData 目录）。") : error);
        return;
    }

    // 读取并校验整合包元数据（hkspl_package.json）；缺失/损坏时直接拒绝。
    QByteArray metaJson;
    const ckan::ModpackMetaStatus metaStatus =
        ckan::modpackReadPackageMeta(zipFilePath, &metaJson, &error);
    if (metaStatus == ckan::ModpackMetaStatus::NotFound) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            tr("所选文件不是有效的整合包（缺少 %1 元数据文件）。").arg(
                QString::fromLatin1(ckan::kModpackMetaFileName)));
        return;
    }
    if (metaStatus == ckan::ModpackMetaStatus::ReadError) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            error.isEmpty() ? tr("读取整合包元数据失败。") : error);
        return;
    }
    QJsonParseError pe;
    const QJsonDocument metaDoc = QJsonDocument::fromJson(metaJson, &pe);
    if (pe.error != QJsonParseError::NoError || !metaDoc.isObject()) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"), tr("整合包元数据已损坏，无法导入。"));
        return;
    }
    const QJsonObject meta = metaDoc.object();
    const QString pkgVersionStr = meta.value(QStringLiteral("gameVersion")).toString();
    const ckan::GameVersion pkgVersion(pkgVersionStr);
    if (pkgVersionStr.isEmpty() || !pkgVersion.isValid()) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            tr("整合包缺少有效的游戏版本信息，无法导入。"));
        return;
    }

    // 版本校验：整合包与当前实例的 major+minor 必须相同，否则拒绝（拒绝后不再弹信息弹窗）。
    const ckan::GameVersion currentVersion = CKanManager::instance().detectedVersion();
    if (currentVersion.isValid() && !ckan::modpackVersionCompatible(pkgVersion, currentVersion)) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            tr("整合包游戏版本为 %1，与当前实例版本 %2 不兼容，已拒绝导入。")
                .arg(pkgVersion.withoutBuild().toString(),
                     currentVersion.withoutBuild().toString()));
        return;
    }

    // 元数据信息弹窗（替代原清空确认）：展示整合包信息并请用户确认开始安装。
    QDialog info(m_dialogParent);
    info.setWindowTitle(tr("导入整合包"));
    info.setModal(true);
    QFormLayout *infoForm = new QFormLayout;
    infoForm->addRow(tr("整合包名称:"),
        new QLabel(meta.value(QStringLiteral("name")).toString().isEmpty()
                       ? QFileInfo(zipFilePath).completeBaseName()
                       : meta.value(QStringLiteral("name")).toString()));
    infoForm->addRow(tr("游戏版本:"), new QLabel(pkgVersion.withoutBuild().toString()));
    const QString launcherVer = meta.value(QStringLiteral("launcherVersion")).toString();
    infoForm->addRow(tr("启动器版本:"),
        new QLabel(launcherVer.isEmpty() ? tr("未知") : launcherVer));
    infoForm->addRow(tr("描述:"),
        new QLabel(meta.value(QStringLiteral("description")).toString()));
    QLabel *warn = new QLabel(
        tr("导入将删除当前实例 GameData 中除 Squad、SquadExpansion 外的所有模组，\n"
           "并用整合包中的模组替换。是否继续？"));
    warn->setWordWrap(true);
    QDialogButtonBox *infoButtons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    infoButtons->button(QDialogButtonBox::Ok)->setText(tr("确定安装"));
    infoButtons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(infoButtons, &QDialogButtonBox::accepted, &info, &QDialog::accept);
    connect(infoButtons, &QDialogButtonBox::rejected, &info, &QDialog::reject);
    auto *infoRoot = new QVBoxLayout(&info);
    infoRoot->addLayout(infoForm);
    infoRoot->addWidget(warn);
    infoRoot->addWidget(infoButtons);
    if (info.exec() != QDialog::Accepted)
        return; // 用户取消

    emit showSettingsRequested();

    // 解压/清空为磁盘密集操作，放到后台线程执行，避免阻塞主线程（大整合包卡顿）。
    // 进度经跨线程队列信号回到主线程更新进度框；取消标志与结果经堆对象在工作线程与
    // 主线程间共享（按值捕获共享指针），进度框用 QPointer 防止页面销毁后访问悬垂对象。
    QPointer<QProgressDialog> progressDialog(
        new QProgressDialog(tr("正在导入整合包..."), "取消", 0, 100, m_dialogParent));
    progressDialog->setWindowTitle(tr("导入整合包"));
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(0);
    progressDialog->setValue(0);
    progressDialog->show();

    auto cancelRequested = std::make_shared<std::atomic_bool>(false);
    const QString gameDir = m_instance.path;

    // 进度对话框销毁（关闭窗口/退出应用）视为取消：立即置取消标志，让后台解压线程
    // 尽快中止（modpackImportGameData 周期轮询该标志）。否则窗口关闭后该任务不受
    // CKanManager 退出清理管辖，进程会为等待这段磁盘长操作而长时间残留。
    QObject::connect(progressDialog, &QObject::destroyed,
                     [cancelRequested]() { cancelRequested->store(true); });

    auto watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    auto future = QtConcurrent::run(
        [zipFilePath, gameDir, cancelRequested, progressDialog]() -> QPair<bool, QString> {
            QString err;
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
        if (progressDialog)
            progressDialog->close();
        const QPair<bool, QString> r = watcher->result();
        watcher->deleteLater();

        if (cancelRequested->load()) {
            QMessageBox::information(m_dialogParent, tr("提示"), tr("导入已取消，原模组可能已被部分替换。"));
        } else if (r.first) {
            // 导入后重建 CKan（注册表已清空、文件已替换），下次进入模组页时重新扫描
            emit modsReloadRequested();
            QMessageBox::information(m_dialogParent, tr("导入成功"),
                tr("整合包已导入到当前实例：\n%1").arg(QDir::toNativeSeparators(zipFilePath)));
        } else {
            QMessageBox::warning(m_dialogParent, tr("导入失败"),
                r.second.isEmpty() ? tr("导入整合包时发生错误。") : r.second);
            // 导入失败仍需重建 CKan，避免旧的注册表状态残留
            emit modsReloadRequested();
        }
        if (progressDialog)
            progressDialog->deleteLater();
    });
    watcher->setFuture(future);
}

void ModpackController::importFromCkan()
{
    if (m_instance.path.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"), tr("实例路径为空，无法导入整合包。"));
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        m_dialogParent, tr("导入整合包 - 选择 CKAN 文件"), QString(), tr("CKAN 文件 (*.ckan)"));
    if (filePath.isEmpty())
        return; // 用户取消

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            tr("无法读取文件：%1").arg(file.errorString()));
        return;
    }
    const QByteArray json = file.readAll();
    file.close();

    QString error;
    const QStringList identifiers = ckan::modpackCkanDepends(json, &error);
    if (identifiers.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            error.isEmpty() ? tr("CKAN 文件不包含任何可安装模组。") : error);
        return;
    }

    // 提醒：将删除现有模组并从仓库下载清单中的模组
    const QMessageBox::StandardButton confirm = QMessageBox::question(
        m_dialogParent, tr("导入整合包"),
        tr("导入将删除当前实例 GameData 中除 Squad、SquadExpansion 外的所有模组，\n"
           "并从仓库解析下载以下 %1 个模组及其依赖：\n\n%2\n\n是否继续？")
            .arg(identifiers.size()).arg(identifiers.join(QStringLiteral("\n"))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (confirm != QMessageBox::Yes)
        return;

    // 先清空现有模组（保留 Squad/SquadExpansion），再跳转到模组管理界面下载
    QString clearError;
    if (!ckan::modpackClearGameData(m_instance.path, &clearError)) {
        QMessageBox::warning(m_dialogParent, tr("导入失败"),
            tr("导入整合包时发生错误。\n%1").arg(clearError));
        return;
    }

    // 注册表被删除，立即刷新 libckan 内存中的已安装数据，避免旧 registry 滞留
    CKanManager::instance().reloadRegistry();

    // 交页面接线：切到模组管理 + 索引就绪后自动批量安装
    emit modsInstallRequested(identifiers);
}