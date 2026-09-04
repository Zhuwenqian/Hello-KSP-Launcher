#include "updatemanager.h"
#include "appversion.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <thread>
#include <chrono>
#include <cstdlib>

// 更新源仓库
static const char* const kRepoApi =
    "https://api.github.com/repos/Zhuwenqian/Hello-KSP-Launcher/releases/latest";

UpdaterManager& UpdaterManager::instance()
{
    static UpdaterManager inst;
    return inst;
}

UpdaterManager::UpdaterManager(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_reply(nullptr),
      m_file(nullptr),
      m_working(false),
      m_quiet(false)
{
}

UpdaterManager::~UpdaterManager()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

QString UpdaterManager::currentVersion()
{
    return QStringLiteral(HKSPL_APP_VERSION);
}

bool UpdaterManager::versionLess(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const QStringList pb = b.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    QVector<int> ia, ib;
    bool okA = true, okB = true;
    for (const QString &s : pa) { bool ok=false; const int v=s.toInt(&ok); if(!ok){okA=false;break;} ia.append(v); }
    for (const QString &s : pb) { bool ok=false; const int v=s.toInt(&ok); if(!ok){okB=false;break;} ib.append(v); }
    if (!okA || !okB) return a < b;
    const int n = qMax(ia.size(), ib.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < ia.size() ? ia[i] : 0;
        const int vb = i < ib.size() ? ib[i] : 0;
        if (va != vb) return va < vb;
    }
    return false; // 相等
}

QString UpdaterManager::stagingZipPath() const
{
    // 与应用同目录的临时更新目录（同盘，保证 updater 快速替换）：
    // <应用目录>/.updater_update/update-<version>.zip
    const QString dir = QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral(".updater_update")));
    const QString fname = m_latest.assetName.isEmpty()
        ? QStringLiteral("update.zip")
        : QStringLiteral("update-%1.zip").arg(m_latest.version);
    return QDir(dir).filePath(fname);
}

void UpdaterManager::checkForUpdate(bool quiet)
{
    if (m_working) return;
    m_working = true;
    m_quiet = quiet;
    m_lastError.clear();

    QNetworkRequest req(QUrl(QString::fromUtf8(kRepoApi)));
    req.setRawHeader("User-Agent", "HelloKSPLauncher");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(30000); // 30s 传输超时（连接+闲置）

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString err = reply->errorString();
        const bool ok = (reply->error() == QNetworkReply::NoError);
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        m_working = false;

        if (!ok) {
            m_lastError = tr("网络请求失败：%1").arg(err);
            if (!m_quiet) emit updateCheckFailed(m_lastError);
            return;
        }
        if (!parseRelease(data)) {
            if (!m_quiet) emit updateCheckFailed(m_lastError);
            return;
        }
        emit updateCheckDone();
    });
}

bool UpdaterManager::parseRelease(const QByteArray &json)
{
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = tr("更新信息解析失败");
        return false;
    }
    const QJsonObject root = doc.object();
    QString tag = root.value("tag_name").toString().trimmed();
    if (tag.startsWith(QLatin1Char('v'))) tag.remove(0, 1);

    ReleaseInfo info;
    info.version = tag;
    info.body = root.value("body").toString();

    const QJsonArray assets = root.value("assets").toArray();
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        const QString name = a.value("name").toString();
        const QString url = a.value("browser_download_url").toString();
        if (!name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) continue;
        if (!name.contains(QStringLiteral("x86_64"), Qt::CaseInsensitive)) continue;
        info.assetName = name;
        info.assetUrl = url;
        break;
    }
    if (info.assetUrl.isEmpty()) {
        m_lastError = tr("仓库中未找到 x86_64 发布包");
        return false;
    }
    info.hasUpdate = versionLess(currentVersion(), info.version);
    m_latest = info;
    return true;
}

void UpdaterManager::downloadRelease()
{
    if (m_latest.assetUrl.isEmpty()) {
        emit updateError(tr("没有已下载的更新任务，请先检查更新"));
        return;
    }
    if (m_working) return;
    m_working = true;

    const QString path = stagingZipPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_file = new QFile(path, this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_working = false;
        emit updateError(tr("无法创建更新暂存文件：%1").arg(path));
        return;
    }

    QNetworkRequest req(QUrl(m_latest.assetUrl));
    req.setRawHeader("User-Agent", "HelloKSPLauncher");
    req.setRawHeader("Accept", "application/octet-stream");
    req.setTransferTimeout(30000);

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit downloadProgress(received, total); });
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_file) m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        const bool ok = (m_reply->error() == QNetworkReply::NoError);
        const QString err = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        if (m_file) {
            m_file->close();
            m_file->deleteLater();
            m_file = nullptr;
        }
        m_working = false;
        if (!ok) {
            m_lastError = tr("下载失败：%1").arg(err);
            emit updateError(m_lastError);
            return;
        }
        emit downloadFinished(stagingZipPath());
    });
}

bool UpdaterManager::applyUpdate(const QString &zipPath)
{
    if (!QFile::exists(zipPath)) {
        emit updateError(tr("更新包不存在：%1").arg(zipPath));
        return false;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
#if defined(_WIN32)
    const QString updaterName = QStringLiteral("updater.exe");
    const QString helloExe = QStringLiteral("HelloKSPLauncher.exe");
    // updater 依赖的运行时（Qt6Core + MinGW 运行时），从安装目录复制到隔离目录，
    // 避免 updater 自身加载这些 DLL 而占用安装目录文件，导致无法删除/替换。
    const QStringList runtimeDlls = {
        QStringLiteral("Qt6Core.dll"),
        QStringLiteral("libgcc_s_seh-1.dll"),
        QStringLiteral("libstdc++-6.dll"),
        QStringLiteral("libwinpthread-1.dll")
    };
#else
    const QString updaterName = QStringLiteral("updater");
    const QString helloExe = QStringLiteral("HelloKSPLauncher");
    const QStringList runtimeDlls;
#endif
    const QString updaterSrc = QDir(appDir).filePath(updaterName);
    const QString hello = QDir(appDir).filePath(helloExe);
    if (!QFile::exists(updaterSrc)) {
        emit updateError(tr("未找到更新组件：%1").arg(updaterSrc));
        return false;
    }
    if (!QFile::exists(hello)) {
        emit updateError(tr("未找到主程序：%1").arg(hello));
        return false;
    }

    // 构建隔离的自更新目录（系统临时目录），从中启动 updater
    const QString selfDir = QDir::cleanPath(
        QDir(QDir::tempPath()).filePath(QStringLiteral("HKSPL_updater_self")));
    {
        QDir sdir(selfDir);
        if (sdir.exists())
            sdir.removeRecursively();
        sdir.mkpath(".");
    }
    auto copyToSelf = [&](const QString &src) -> bool {
        return QFile::copy(src, QDir(selfDir).filePath(QFileInfo(src).fileName()));
    };
    if (!copyToSelf(updaterSrc)) {
        emit updateError(tr("准备更新组件失败"));
        return false;
    }
    for (const QString &dll : runtimeDlls) {
        if (!copyToSelf(QDir(appDir).filePath(dll))) {
            emit updateError(tr("准备更新组件失败（缺少 %1）").arg(dll));
            QDir(selfDir).removeRecursively();
            return false;
        }
    }

    const QString updaterSelf = QDir(selfDir).filePath(updaterName);
    const QStringList args = {
        QStringLiteral("--dir"), appDir,
        QStringLiteral("--apply"), zipPath,
        QStringLiteral("--wait-pid"), QString::number(QCoreApplication::applicationPid())
    };
    if (!QProcess::startDetached(updaterSelf, args)) {
        emit updateError(tr("无法启动更新组件：%1").arg(updaterSelf));
        QDir(selfDir).removeRecursively();
        return false;
    }
    // 更新器已在隔离目录启动。主程序应立即退出，
    // 由更新器等待本进程结束（--wait-pid）后替换文件并重启新版。
    // 先用事件循环正常退出；同时挂一个后台看门狗线程，3 秒内若进程仍未结束
    // （主线程被完全阻塞、事件循环派发不掉 quit 时），无条件强制终止，
    // 确保旧进程必然退出，更新器不会等超时后拿着被占用的文件去删除。
    // 注意：看门狗必须在事件循环之外工作，故用独立线程而非 QTimer。
    QCoreApplication::quit();
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::_Exit(0);
    }).detach();
    return true;
}