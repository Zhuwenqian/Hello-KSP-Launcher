#include "updateflow.h"
#include "updatemanager.h"
#include "configmanager.h"
#include <QMessageBox>
#include <QProgressDialog>
#include <QPointer>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>

namespace updateflow {

static QPointer<QWidget> s_parent;      // 当前流程宿主（自动检查时为 null）
static QPointer<QProgressDialog> s_progress;
static bool s_manual = false;

static void onCheckDone();
static void onCheckFailed(const QString &err);
static void onDownloadProgress(qint64 received, qint64 total);
static void onDownloadFinished(const QString &zipPath);
static void onUpdateError(const QString &err);

static void promptAndApply()
{
    UpdaterManager &um = UpdaterManager::instance();

    // 用自定义对话框替代 QMessageBox：发布日志单独放入可滚动区，
    // 按钮始终固定在最下方，长日志不会被视口裁剪导致按不到按钮。
    QDialog dlg(s_parent);
    dlg.setWindowTitle(QObject::tr("发现新版本"));
    dlg.setModal(true);
    dlg.setMinimumWidth(480);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QLabel *info = new QLabel(QObject::tr(
        "检测到新版本 v%1（当前 v%2）。\n是否下载并更新？更新过程会自动替换文件并重启启动器。")
        .arg(um.latest().version, UpdaterManager::currentVersion()), &dlg);
    info->setWordWrap(true);
    lay->addWidget(info);

    const QString body = um.latest().body.trimmed();
    if (!body.isEmpty()) {
        lay->addWidget(new QLabel(QObject::tr("更新日志"), &dlg));
        QPlainTextEdit *notes = new QPlainTextEdit(&dlg);
        notes->setPlainText(body);
        notes->setReadOnly(true);
        notes->setMinimumHeight(180);
        notes->setMaximumHeight(300);
        notes->setTabStopDistance(notes->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
        lay->addWidget(notes);
    }

    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Yes | QDialogButtonBox::No, &dlg);
    box->button(QDialogButtonBox::Yes)->setText(QObject::tr("更新"));
    box->button(QDialogButtonBox::No)->setText(QObject::tr("取消"));
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(box);

    if (dlg.exec() != QDialog::Accepted) {
        s_parent = nullptr;
        return;
    }

    s_progress = new QProgressDialog(s_parent);
    s_progress->setWindowTitle(QObject::tr("更新下载"));
    s_progress->setLabelText(QObject::tr("正在下载更新..."));
    s_progress->setRange(0, 100);
    s_progress->setWindowModality(Qt::WindowModal);
    s_progress->setMinimumWidth(380);
    s_progress->setCancelButton(nullptr);
    s_progress->show();
    um.downloadRelease();
}

static void onCheckDone()
{
    UpdaterManager &um = UpdaterManager::instance();
    if (!um.latest().hasUpdate) {
        if (s_manual) {
            QMessageBox::information(s_parent,
                QObject::tr("检查更新"),
                QObject::tr("当前已是最新版本 v%1。").arg(UpdaterManager::currentVersion()));
        }
        s_parent = nullptr;
        return;
    }
    promptAndApply();
}

static void onCheckFailed(const QString &err)
{
    QMessageBox::warning(s_parent, QObject::tr("检查更新失败"), err);
    s_parent = nullptr;
}

static void onDownloadProgress(qint64 received, qint64 total)
{
    if (!s_progress) return;
    if (total <= 0) {
        s_progress->setRange(0, 0);
        s_progress->setLabelText(QObject::tr("正在下载更新...（已下载 %1 MB）")
            .arg(received / (1024.0 * 1024.0), 0, 'f', 1));
        return;
    }
    s_progress->setMaximum(int(total / 1024));
    s_progress->setValue(int(received / 1024));
    s_progress->setLabelText(QObject::tr("正在下载更新...（%1%）")
        .arg(total > 0 ? int(received * 100.0 / total) : 0));
}

static void onDownloadFinished(const QString &zipPath)
{
    if (s_progress) { s_progress->close(); s_progress->deleteLater(); }
    s_progress = nullptr;
    s_parent = nullptr;
    // 启动更新器（替换+重启新版）并退出主程序
    UpdaterManager::instance().applyUpdate(zipPath);
}

static void onUpdateError(const QString &err)
{
    if (s_progress) { s_progress->close(); s_progress->deleteLater(); }
    s_progress = nullptr;
    QMessageBox::warning(s_parent, QObject::tr("更新失败"), err);
    s_parent = nullptr;
}

static void ensureConnected()
{
    UpdaterManager &um = UpdaterManager::instance();
    // 静态函数转发；Qt::UniqueConnection 避免重复检查时叠加连接
    QObject::connect(&um, &UpdaterManager::updateCheckDone, []() { onCheckDone(); });
    QObject::connect(&um, &UpdaterManager::updateCheckFailed, [](const QString &e) { onCheckFailed(e); });
    QObject::connect(&um, &UpdaterManager::downloadProgress,
                     [](qint64 r, qint64 t) { onDownloadProgress(r, t); });
    QObject::connect(&um, &UpdaterManager::downloadFinished,
                     [](const QString &z) { onDownloadFinished(z); });
    QObject::connect(&um, &UpdaterManager::updateError, [](const QString &e) { onUpdateError(e); });
}

void checkSilent()
{
    if (!ConfigManager::instance().autoCheckUpdate()) return;
    s_parent = nullptr;
    s_manual = false;
    s_progress = nullptr;
    ensureConnected();
    UpdaterManager::instance().checkForUpdate(true);
}

void checkManual(QWidget *parent)
{
    s_parent = parent;
    s_manual = true;
    s_progress = nullptr;
    ensureConnected();
    UpdaterManager::instance().checkForUpdate(false);
}

} // namespace updateflow