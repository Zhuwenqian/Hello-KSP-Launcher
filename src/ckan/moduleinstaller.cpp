#include "moduleinstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QSet>

#include "gameinstance.h"
#include "registry.h"
#include "downloader.h"
#include "moduleinstalldescriptor.h"

#include "miniz.h"

namespace ckan {

ModuleInstaller::ModuleInstaller(GameInstance *instance, QObject *parent)
    : QObject(parent), m_instance(instance)
{
}

bool ModuleInstaller::listZipEntries(const QString &zipPath, QStringList *entries, QString *error)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, zipPath.toUtf8().constData(), 0)) {
        if (error) *error = QStringLiteral("failed to open zip: %1").arg(zipPath);
        return false;
    }
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(&zip, i, &stat))
            entries->append(QString::fromUtf8(stat.m_filename));
    }
    mz_zip_reader_end(&zip);
    return true;
}

namespace {
// 从 zip 提取单文件到 out 内存
bool extractToMem(mz_zip_archive &zip, const char *name, QByteArray *out, QString *error)
{
    const int idx = mz_zip_reader_locate_file(&zip, name, nullptr, 0);
    if (idx < 0) {
        if (error) *error = QStringLiteral("file not found in zip: %1").arg(QString::fromUtf8(name));
        return false;
    }
    size_t size = 0;
    void *mem = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(idx), &size, 0);
    if (!mem) {
        if (error) *error = QStringLiteral("extract failed: %1").arg(QString::fromUtf8(name));
        return false;
    }
    out->resize(static_cast<qsizetype>(size));
    memcpy(out->data(), mem, size);
    mz_free(mem);
    return true;
}

// 判断数据是否具备 ZIP 魔数（PK）
bool looksLikeZip(const QByteArray &data)
{
    if (data.size() < 2) return false;
    const uchar *d = reinterpret_cast<const uchar *>(data.constData());
    return d[0] == 'P' && d[1] == 'K';
}

// 校验缓存文件是否是一个可打开的 ZIP 归档
bool isValidZipFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    if (!looksLikeZip(data)) return false;
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    const bool ok = mz_zip_reader_init_mem(&zip, data.constData(),
                                           static_cast<size_t>(data.size()), 0);
    if (ok) mz_zip_reader_end(&zip);
    return ok;
}

// 某模块提供的所有名称（标识符 + 虚拟包）
QSet<QString> providedNamesOf(const CkanModule &m)
{
    QSet<QString> s;
    s.insert(m.identifier);
    for (const Relationship &r : m.provides)
        if (!r.name.isEmpty()) s.insert(r.name);
    return s;
}

// 判断 dependent 是否依赖 provider 提供的任一名称
bool moduleDependsOn(const CkanModule &dependent, const QSet<QString> &providerNames)
{
    for (const Relationship &rel : dependent.depends)
        if (providerNames.contains(rel.name)) return true;
    return false;
}

// 递归收集反向依赖链并给出卸载顺序（先依赖者、最后 target）。
// 例如 C 依赖 B、B 依赖 A，卸载 A 时顺序为 C,B,A。visited 防止循环依赖。
// 返回 false 表示 target 未安装。
bool collectReverseDeps(const Registry *reg, const QString &target,
                        QSet<QString> &visited, QStringList &order)
{
    if (visited.contains(target)) return true;
    const InstalledModule *targetIm = reg->installed(target);
    if (!targetIm) return false;
    const QSet<QString> providerNames = providedNamesOf(targetIm->module);
    for (auto it = reg->installedModules.constBegin();
         it != reg->installedModules.constEnd(); ++it) {
        const InstalledModule &im = it.value();
        if (im.identifier == target) continue;
        if (moduleDependsOn(im.module, providerNames)
            && !collectReverseDeps(reg, im.identifier, visited, order))
            return false;
    }
    visited.insert(target);
    order.append(target);
    return true;
}
} // namespace

QString ModuleInstaller::safeCacheFileName(const QString &s)
{
    QString r = s;
    r.replace(QLatin1Char(':'), QLatin1Char('_'));
    r.replace(QLatin1Char('\\'), QLatin1Char('_'));
    r.replace(QLatin1Char('/'), QLatin1Char('_'));
    r.replace(QLatin1Char('?'), QLatin1Char('_'));
    r.replace(QLatin1Char('*'), QLatin1Char('_'));
    r.replace(QLatin1Char('<'), QLatin1Char('_'));
    r.replace(QLatin1Char('>'), QLatin1Char('_'));
    r.replace(QLatin1Char('|'), QLatin1Char('_'));
    r.replace(QLatin1Char('"'), QLatin1Char('_'));
    return r;
}

InstallResult ModuleInstaller::install(const QVector<CkanModule> &modules,
                                       const QString &downloadDir,
                                       const QStringList &mirrors)
{
    InstallResult result;
    m_cancelRequested.store(false);
    QDir().mkpath(downloadDir);
    Registry *reg = m_instance->registry();
    Downloader dl;

    // 批量总字节数（downloadSize 未知时按 1 计，避免除零）
    qint64 totalBytes = 0;
    for (const CkanModule &m : modules)
        totalBytes += (m.downloadSize > 0 ? m.downloadSize : 1);
    qint64 baseline = 0; // 已完成模组的累计字节数

    for (const CkanModule &mod : modules) {
        emit installProgress(mod.identifier, 0);
        if (m_cancelRequested.load()) {
            result.error = QStringLiteral("已取消");
            return result;
        }
        if (mod.isMetapackage()) {
            // 元包无文件，仅注册
            InstalledModule im;
            im.identifier = mod.identifier;
            im.module = mod;
            im.installTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            reg->registerModule(im);
            emit moduleInstalled(mod.identifier);
            result.installedIdentifiers << mod.identifier;
            baseline += (mod.downloadSize > 0 ? mod.downloadSize : 1);
            continue;
        }

        if (mod.downloadUrls.isEmpty()) {
            result.error = QStringLiteral("%1 has no download URL").arg(mod.identifier);
            return result;
        }

        // 1. 下载 zip 到缓存目录（校验为 ZIP 归档，非预期内容会重试镜像）
        //    文件名含 version；version 可能带 epoch（如 "1:3.4.0"），需清洗非法字符
        const QString zipPath = downloadDir + QLatin1Char('/') + mod.identifier + QLatin1Char('_')
                              + safeCacheFileName(mod.version) + QStringLiteral(".zip");
        if (!QFileInfo::exists(zipPath) || !isValidZipFile(zipPath)) {
            if (QFile::exists(zipPath))
                QFile::remove(zipPath); // 清理损坏/无效的缓存后再重新下载
            QByteArray data;
            QString err;
            const Downloader::Validator zipValidator = [](const QByteArray &d) {
                return looksLikeZip(d);
            };

            // 该模组下载进度与实时速度
            QElapsedTimer spd;          spd.start();
            qint64 lastRecv = 0;
            const auto onProgress = [&](qint64 received, qint64) {
                const qint64 now = spd.elapsed();
                qint64 speed = 0;
                if (now > 0) speed = (received - lastRecv) * 1000 / now;
                lastRecv = received;
                spd.restart();
                emit byteProgress(mod.identifier, baseline + received, totalBytes, speed);
            };

            if (!dl.downloadProgressed(mod.downloadUrls.first(), mirrors, &data, &err,
                                       zipValidator, onProgress, &m_cancelRequested)) {
                result.error = m_cancelRequested.load()
                    ? QStringLiteral("已取消：%1").arg(mod.identifier)
                    : QStringLiteral("failed to download %1: %2").arg(mod.identifier, err);
                return result;
            }
            QFile zf(zipPath);
            if (!zf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                result.error = QStringLiteral("cannot write download cache: %1").arg(zipPath);
                return result;
            }
            zf.write(data);
            zf.close();
        }
        baseline += (mod.downloadSize > 0 ? mod.downloadSize : 1);
        emit byteProgress(mod.identifier, baseline, totalBytes, 0);
        emit installProgress(mod.identifier, 30);

        // 2. 读取 zip 到内存并打开
        //    不用 mz_zip_reader_init_file：其内部用 fopen 按 ANSI 代码页解析路径，
        //    当缓存路径含非 ASCII 字符时打开失败，会被误报为 "corrupt zip"。
        QFile zipFile(zipPath);
        if (!zipFile.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("cannot open cached download: %1").arg(zipPath);
            return result;
        }
        const QByteArray zipData = zipFile.readAll();
        zipFile.close();
        if (!looksLikeZip(zipData)) {
            result.error = QStringLiteral("corrupt zip for %1 (size=%2 bytes, 下载内容并非 ZIP 归档)")
                .arg(mod.identifier).arg(zipData.size());
            return result;
        }
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        if (!mz_zip_reader_init_mem(&zip, zipData.constData(),
                                    static_cast<size_t>(zipData.size()), 0)) {
            result.error = QStringLiteral("corrupt zip for %1 (size=%2 bytes)")
                .arg(mod.identifier).arg(zipData.size());
            return result;
        }
        const mz_uint count = mz_zip_reader_get_num_files(&zip);
        QStringList entries;
        for (mz_uint i = 0; i < count; ++i) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&zip, i, &st))
                entries.append(QString::fromUtf8(st.m_filename));
        }

        // 3. 应用 install 规则，得到目标文件列表
        const QString gameData = QStringLiteral("GameData");
        QVector<InstallableFile> files;
        for (const ModuleInstallDescriptor &stanza : mod.effectiveInstallStanzas()) {
            QString err;
            const QVector<InstallableFile> f = stanza.findInstallableFiles(entries, gameData, &err);
            files.append(f);
        }
        if (files.isEmpty()) {
            mz_zip_reader_end(&zip);
            result.error = QStringLiteral("no files matched install rules for %1").arg(mod.identifier);
            return result;
        }
        emit installProgress(mod.identifier, 60);

        // 4. 提取并复制文件到 GameData
        QStringList installedRelPaths;
        for (const InstallableFile &f : files) {
            QByteArray content;
            if (!extractToMem(zip, f.sourceName.toUtf8().constData(), &content, &result.error)) {
                mz_zip_reader_end(&zip);
                return result;
            }
            const QString abs = m_instance->toAbsoluteGameDir(f.destination);
            QDir().mkpath(QFileInfo(abs).absolutePath());
            QFile of(abs);
            if (!of.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                mz_zip_reader_end(&zip);
                result.error = QStringLiteral("cannot write %1").arg(abs);
                return result;
            }
            of.write(content);
            of.close();
            installedRelPaths << f.destination;
        }
        mz_zip_reader_end(&zip);
        emit installProgress(mod.identifier, 90);

        // 5. 更新 registry
        InstalledModule im;
        im.identifier = mod.identifier;
        im.module = mod;
        im.files = installedRelPaths;
        im.installTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        reg->registerModule(im);
        emit moduleInstalled(mod.identifier);
        result.installedIdentifiers << mod.identifier;
        emit installProgress(mod.identifier, 100);
    }

    m_instance->saveRegistry();
    result.ok = true;
    return result;
}

void ModuleInstaller::cancel()
{
    m_cancelRequested.store(true);
}

InstallResult ModuleInstaller::uninstall(const QString &identifier)
{
    InstallResult result;
    Registry *reg = m_instance->registry();
    if (!reg->isInstalled(identifier)) {
        result.error = QStringLiteral("%1 is not installed").arg(identifier);
        return result;
    }
    // 级联卸载：收集所有（直接/间接）依赖 identifier 的模组，
    // 顺序为依赖链自外向内，先卸载依赖者，最后卸载 identifier 本身。
    QSet<QString> visited;
    QStringList order;
    if (!collectReverseDeps(reg, identifier, visited, order)) {
        result.error = QStringLiteral("%1 is not installed").arg(identifier);
        return result;
    }
    for (const QString &id : order) {
        const InstalledModule *im = reg->installed(id);
        if (!im) continue;
        // 删除文件（仅删除归属此模块的文件）
        for (const QString &rel : im->files) {
            const QString abs = m_instance->toAbsoluteGameDir(rel);
            QFile::remove(abs);
        }
        reg->unregisterModule(id);
        result.installedIdentifiers << id;
    }
    m_instance->saveRegistry();
    result.ok = true;
    return result;
}

} // namespace ckan