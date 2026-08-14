#include "repoindex.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QRegularExpression>
#include <functional>

#include "downloader.h"
#include "version.h"

// miniz 原始 deflate 解压头
#include "miniz.h"

namespace ckan {

namespace {

static QString g_cacheDir;

// 将仓库名转换为安全的缓存文件名
QString safeRepoName(const QString &name)
{
    QString s = name;
    s.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return s.isEmpty() ? QStringLiteral("default") : s;
}

// 从 gzip 格式内存数据解压为原始 tar 字节。
bool gunzip(const QByteArray &gz, QByteArray *out)
{
    const quint8 *src = reinterpret_cast<const quint8 *>(gz.constData());
    const size_t len = static_cast<size_t>(gz.size());
    if (len < 18) return false;
    if (src[0] != 0x1f || src[1] != 0x8b || src[2] != 8) return false;

    size_t off = 10;
    const quint8 flags = src[3];
    if (flags & 0x04) { // FEXTRA
        if (off + 2 > len) return false;
        const size_t xlen = src[off] | (src[off + 1] << 8);
        off += 2 + xlen;
    }
    if (flags & 0x08) { while (off < len && src[off] != 0) ++off; ++off; }
    if (flags & 0x10) { while (off < len && src[off] != 0) ++off; ++off; }
    if (flags & 0x02) off += 2;
    if (off >= len) return false;

    size_t outLen = 0;
    void *raw = tinfl_decompress_mem_to_heap(src + off, len - off, &outLen, 0);
    if (!raw) return false;
    out->resize(static_cast<qsizetype>(outLen));
    memcpy(out->data(), raw, outLen);
    mz_free(raw);
    return true;
}

// 解析 tar 归档，对每个普通文件调用回调 (路径, 内容)。errno 无关。
void parseTar(const QByteArray &tar,
              const std::function<void(const QString &, const QByteArray &)> &callback)
{
    size_t off = 0;
    const size_t size = static_cast<size_t>(tar.size());
    while (off + 512 <= size) {
        const quint8 *block = reinterpret_cast<const quint8 *>(tar.constData()) + off;
        bool allZero = true;
        for (int i = 0; i < 512; ++i) { if (block[i] != 0) { allZero = false; break; } }
        if (allZero) break;

        // 文件名（100 字节）
        char name[101] = {0};
        int nameLen = 0;
        for (; nameLen < 100 && block[nameLen] != 0; ++nameLen)
            name[nameLen] = static_cast<char>(block[nameLen]);
        const QString entryName = QString::fromUtf8(name);

        // 类型标志（offset 156）
        const char typeflag = static_cast<char>(block[156]);

        // 文件大小（octal，offset 124，12 字节）
        quint64 fileSize = 0;
        for (int i = 124; i < 136 && block[i] != 0 && block[i] != ' '; ++i)
            if (block[i] >= '0' && block[i] <= '7')
                fileSize = fileSize * 8 + static_cast<quint64>(block[i] - '0');

        // 数据区（按 512 对齐）
        const size_t dataOff = off + 512;
        const size_t padded = static_cast<size_t>((fileSize + 511) / 512) * 512;
        if (dataOff + fileSize > size) break;

        if (typeflag == '0' || typeflag == '\0') {
            const QByteArray data(tar.constData() + dataOff, static_cast<int>(fileSize));
            callback(entryName, data);
        }
        off = dataOff + padded;
    }
}

} // namespace

bool RepoIndex::parseTarGz(const QByteArray &tarGz, QMap<QString, QVector<CkanModule>> *index,
                           QString *error)
{
    QByteArray tar;
    if (!gunzip(tarGz, &tar)) {
        if (error) *error = QStringLiteral("failed to gunzip repository archive");
        return false;
    }
    index->clear();
    parseTar(tar, [&](const QString &entryName, const QByteArray &data) {
        if (!entryName.endsWith(QStringLiteral(".ckan"), Qt::CaseInsensitive))
            return;
        QString err;
        const CkanModule m = CkanModule::fromJson(data, &err);
        if (m.isValid())
            (*index)[m.identifier].append(m);
    });
    return true;
}

bool RepoIndex::build(const Repository &repo, const QStringList &mirrors,
                      QMap<QString, QVector<CkanModule>> *index, QString *error,
                      const std::function<void(qint64, qint64)> &onProgress,
                      std::atomic_bool *cancelFlag)
{
    Downloader dl;
    QByteArray data;
    if (!dl.downloadProgressed(repo.uri, mirrors, &data, error, nullptr, onProgress, cancelFlag))
        return false;
    return parseTarGz(data, index, error);
}

void RepoIndex::setCacheDir(const QString &dir)
{
    g_cacheDir = dir;
}

QString RepoIndex::cacheDir()
{
    return g_cacheDir;
}

bool RepoIndex::buildCached(const Repository &repo, const QStringList &mirrors,
                            QMap<QString, QVector<CkanModule>> *index, QString *error,
                            bool forceRefresh, qint64 maxAgeSecs,
                            const std::function<void(qint64, qint64)> &onProgress,
                            std::atomic_bool *cancelFlag)
{
    // 未配置缓存目录：退回每次下载
    if (g_cacheDir.isEmpty())
        return build(repo, mirrors, index, error, onProgress, cancelFlag);

    QDir().mkpath(g_cacheDir);
    const QString cacheFile = QDir(g_cacheDir).filePath(safeRepoName(repo.name) + QStringLiteral(".tar.gz"));

    // 尝试使用新鲜缓存
    if (!forceRefresh) {
        const QFileInfo fi(cacheFile);
        if (fi.exists()) {
            const qint64 age = fi.lastModified().secsTo(QDateTime::currentDateTime());
            if (age >= 0 && age < maxAgeSecs) {
                QFile f(cacheFile);
                if (f.open(QIODevice::ReadOnly)) {
                    const QByteArray data = f.readAll();
                    if (parseTarGz(data, index, error))
                        return true;
                }
            }
        }
    }

    // 下载并写入缓存
    Downloader dl;
    QByteArray data;
    if (!dl.downloadProgressed(repo.uri, mirrors, &data, error, nullptr, onProgress, cancelFlag))
        return false;
    QSaveFile sf(cacheFile);
    if (sf.open(QIODevice::WriteOnly)) {
        sf.write(data);
        sf.commit();
    }
    return parseTarGz(data, index, error);
}

QVector<CkanModule> RepoIndex::versionsFor(const QMap<QString, QVector<CkanModule>> &index,
                                           const QString &identifier)
{
    QVector<CkanModule> v = index.value(identifier);
    std::sort(v.begin(), v.end(), [](const CkanModule &a, const CkanModule &b) {
        return ModuleVersion(a.version) > ModuleVersion(b.version);
    });
    return v;
}

CkanModule RepoIndex::latestFor(const QMap<QString, QVector<CkanModule>> &index,
                                const QString &identifier)
{
    const QVector<CkanModule> v = versionsFor(index, identifier);
    return v.isEmpty() ? CkanModule() : v.first();
}

} // namespace ckan