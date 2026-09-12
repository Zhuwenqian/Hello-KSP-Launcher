#include "updatemanager.h"
#include "appversion.h"
#include "miniz.h"
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
#include <QCryptographicHash>
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
    // GitHub 对每个 release 资产自动提供 SHA256 摘要（digest 字段，形如 "sha256:<64hex>"，
    // 官网发布页的「复制 SHA256」即此值）。缺失/格式异常时校验阶段会明确报错中止，保证来源完整。
    info.expectedDigest = digestHexFromApi(root.value("assets").toArray(),
        info.assetName);
    info.updaterUpdate = bodyIndicatesUpdaterUpdate(info.body);
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
        if (!ok) {
            m_lastError = tr("下载失败：%1").arg(err);
            cleanupStagedZip();
            m_working = false;
            emit updateError(m_lastError);
            return;
        }
        // 校验已下载 zip 的 SHA256 与 GitHub 提供的 release 摘要；校验通过才放行。
        verifyAndFinish(stagingZipPath());
    });
}

QString UpdaterManager::digestHexFromApi(const QJsonArray &assets,
                                         const QString &assetName)
{
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        if (a.value("name").toString().compare(assetName, Qt::CaseInsensitive) != 0)
            continue;
        const QString digest = a.value("digest").toString().trimmed();
        const QString prefix = QStringLiteral("sha256:");
        if (!digest.startsWith(prefix, Qt::CaseInsensitive)) return QString();
        const QString hex = digest.mid(prefix.size());
        if (hex.size() != 64) return QString();
        bool allHex = true;
        for (const QChar &c : hex)
            if (!c.isDigit() && !(c >= QLatin1Char('a') && c <= QLatin1Char('f'))
                              && !(c >= QLatin1Char('A') && c <= QLatin1Char('F'))) { allHex = false; break; }
        if (!allHex) return QString();
        return hex.toLower();
    }
    return QString();
}

bool UpdaterManager::fileSha256(const QString &path, QString *hexOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buf;
    while (!(buf = f.read(1 << 20)).isEmpty())
        hash.addData(buf);
    if (hexOut) *hexOut = QString::fromLatin1(hash.result().toHex());
    return true;
}

void UpdaterManager::verifyAndFinish(const QString &zipPath)
{
    auto fail = [this, zipPath](const QString &msg) {
        m_lastError = msg;
        cleanupStagedZip();
        m_working = false;
        emit updateError(m_lastError);
    };

    // 摘要缺失：来源完整性无法验证，明确中止更新。
    if (m_latest.expectedDigest.isEmpty()) {
        fail(tr("GitHub 未提供该资产的 SHA256 摘要，已中止更新（来源完整性校验失败）"));
        return;
    }

    QString actual;
    if (!fileSha256(zipPath, &actual)) {
        fail(tr("无法读取已下载的更新包计算 SHA256"));
        return;
    }
    if (QString::compare(actual, m_latest.expectedDigest, Qt::CaseInsensitive) != 0) {
        fail(tr("SHA256 校验失败：下载的更新包与 GitHub 提供的摘要不一致，更新已中止。"));
        return;
    }

    // 校验通过：放行进入应用阶段
    m_working = false;
    emit downloadFinished(zipPath);
}

// 清理已下载但未通过校验的暂存 zip（尽力而为，忽略删除失败）。
void UpdaterManager::cleanupStagedZip()
{
    const QString path = stagingZipPath();
    if (QFileInfo::exists(path))
        QFile::remove(path);
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
    QStringList args = {
        QStringLiteral("--dir"), appDir,
        QStringLiteral("--apply"), zipPath,
        QStringLiteral("--wait-pid"), QString::number(QCoreApplication::applicationPid())
    };
    // 本 Release 含更新器更新：更新器自身受保留名单约束永远无法替换自己（updater.exe 在
    // 清理/搬运时都被跳过），故令 updater 保留 .updater_update（含 zip），并写 pending 标记，
    // 由更新完成后重启的新版启动器用该 zip 替换 updater.exe（见 applyPendingUpdaterUpdate）。
    // 仅在 Windows 处理（updater.exe 组件，跨平台规则见 main.cpp 调用处）。
#if defined(_WIN32)
    if (m_latest.updaterUpdate) {
        const QString updDir = QDir(appDir).filePath(QStringLiteral(".updater_update"));
        QDir().mkpath(updDir);
        QFile mk(QDir(updDir).filePath(QStringLiteral("updater_pending")));
        if (mk.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            mk.write(QFileInfo(zipPath).fileName().toUtf8());
            mk.close();
        }
        args << QStringLiteral("--keep-zip");
    }
#endif
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

bool UpdaterManager::bodyIndicatesUpdaterUpdate(const QString &body)
{
    // 与发布说明中的固定提示块保持一致（双语关键短语命中其一即可）。
    return body.contains(QStringLiteral("本 Release 含有更新器"))
        || body.contains(QStringLiteral("ships an updated built-in updater"));
}

bool UpdaterManager::findZipEntryByBaseName(const QString &zipPath,
                                            const QString &baseName,
                                            QString *outEntry)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof(z));
    if (!mz_zip_reader_init_file(&z, zipPath.toUtf8().constData(), 0))
        return false;
    QString found;
    const mz_uint count = mz_zip_reader_get_num_files(&z);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        if (st.m_is_directory) continue;
        const QString name = QString::fromUtf8(st.m_filename);
        // basename = 最后一个 '/' 之后的部分；兼容 zip 内含单一顶层发布目录的结构
        const QString base = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (base.compare(baseName, Qt::CaseInsensitive) == 0) { found = name; break; }
    }
    mz_zip_reader_end(&z);
    if (outEntry) *outEntry = found;
    return !found.isEmpty();
}

bool UpdaterManager::extractZipEntry(const QString &zipPath, const QString &entryName,
                                     const QString &destPath)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof(z));
    if (!mz_zip_reader_init_file(&z, zipPath.toUtf8().constData(), 0))
        return false;
    bool ok = false;
    const mz_uint count = mz_zip_reader_get_num_files(&z);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        if (st.m_is_directory) continue;
        if (QString::fromUtf8(st.m_filename) != entryName) continue;
        QDir().mkpath(QFileInfo(destPath).absolutePath());
        ok = mz_zip_reader_extract_to_file(&z, i, destPath.toUtf8().constData(), 0);
        if (!ok) QFile::remove(destPath); // 解压失败清除半成品，保证重试时干净
        break;
    }
    mz_zip_reader_end(&z);
    return ok;
}

UpdaterManager::PendingUpdaterResult UpdaterManager::applyPendingUpdaterUpdate(QString *errorOut)
{
#if !defined(_WIN32)
    Q_UNUSED(errorOut);
    return PendingUpdaterResult::None; // 更新器组件更新仅在 Windows 处理
#else
    auto fail = [errorOut](const QString &msg) {
        if (errorOut) *errorOut = msg;
        qWarning() << "更新器组件更新失败:" << msg;
        return PendingUpdaterResult::Failed;
    };

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString updDir = QDir(appDir).filePath(QStringLiteral(".updater_update"));
    const QString marker = QDir(updDir).filePath(QStringLiteral("updater_pending"));
    if (!QFileInfo::exists(marker))
        return PendingUpdaterResult::None;

    // 标记内容为 zip 文件名；缺失/损坏时回退扫描目录中的 *.zip
    QString zipPath;
    {
        QFile f(marker);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString name = QString::fromUtf8(f.readAll()).trimmed();
            if (!name.isEmpty()) zipPath = QDir(updDir).filePath(name);
            f.close();
        }
    }
    if (zipPath.isEmpty() || !QFileInfo::exists(zipPath)) {
        const QStringList zips =
            QDir(updDir).entryList(QStringList() << QStringLiteral("*.zip"), QDir::Files);
        if (zips.isEmpty())
            return fail(tr("保留的更新包缺失：%1").arg(updDir));
        zipPath = QDir(updDir).filePath(zips.first());
    }

    QString entry;
    if (!findZipEntryByBaseName(zipPath, QStringLiteral("updater.exe"), &entry))
        return fail(tr("更新包中未找到 updater.exe：%1").arg(zipPath));
    const QString tmpExe = QDir(updDir).filePath(QStringLiteral("updater_new.exe"));
    if (!extractZipEntry(zipPath, entry, tmpExe))
        return fail(tr("从更新包解压 updater.exe 失败"));

    const QString target = QDir(appDir).filePath(QStringLiteral("updater.exe"));
    QFile::remove(target); // 启动阶段无进程占用，尽力覆盖
    bool replaced = QFile::rename(tmpExe, target);
    if (!replaced) replaced = QFile::copy(tmpExe, target); // rename 失败兜底复制
    if (!replaced)
        return fail(tr("覆盖 %1 失败").arg(target));

    // 替换成功：清理整个 .updater_update（zip + 标记 + 暂存）
    QDir(updDir).removeRecursively();
    qInfo() << "更新器组件（updater.exe）已由新版启动器完成更新";
    return PendingUpdaterResult::Updated;
#endif
}

void UpdaterManager::cleanupPendingUpdaterUpdate()
{
    const QString updDir = QDir(QCoreApplication::applicationDirPath())
                               .filePath(QStringLiteral(".updater_update"));
    if (QFileInfo::exists(updDir))
        QDir(updDir).removeRecursively();
}