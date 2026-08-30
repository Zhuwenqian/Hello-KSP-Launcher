// 实例管理器 - 整合包导出（GameData 打包为 ZIP）
#include "instancemanager.h"
#include "miniz.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QSet>

namespace {

// 单个导出文件写入阈值：<= 1MB 视为小文件（KSP 模组绝大多数文件在此范围）。
// 小文件读入受限缓冲后一次 add_mem 写入，避免逐块回调 + 多次 seek 的开销；
// 大文件走流式回调，内存占用固定。
constexpr qint64 kSmallFileThreshold = 1024 * 1024;

// 文件读取回调结构体，供 miniz 流式读取使用
struct FileReadContext {
    QFile* file;
};

size_t fileReadCallback(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
    FileReadContext* ctx = static_cast<FileReadContext*>(pOpaque);
    if (ctx->file->seek(static_cast<qint64>(file_ofs))) {
        return static_cast<size_t>(ctx->file->read(static_cast<char*>(pBuf), static_cast<qint64>(n)));
    }
    return 0;
}

// 写缓冲后端：miniz 写文件时按递增偏移顺序写入，但单次 pWrite 往往很小（本地头 30B、
// 文件名、extra、压缩输出块）。若直接把每个小块写进无缓冲 QFile，都会触发一次 seek+write
// 系统调用，海量小文件下开销巨大。此封装把顺序小块攒进缓冲，凑满阈值后一次性整块落盘；
// 遇非顺序偏移（理论不会出现，为稳健兜底）则先刷缓冲再 seek 直写。
struct BufferedFileWriter {
    QFile* file = nullptr;
    QByteArray buf;
    mz_uint64 written = 0;        // 已落盘的总字节数（绝对偏移）
    qint64 flushThreshold = 1024 * 1024; // 每凑满 1MB 刷一次盘

    size_t write(mz_uint64 file_ofs, const void* p, size_t n) {
        // 当前缓冲覆盖 [written, written+buf.size())，下一次应写在其末尾。
        if (file_ofs != written + static_cast<mz_uint64>(buf.size())) {
            if (!flush())
                return 0;
            if (file_ofs != written) { // 非顺序：清空缓冲后按偏移直写
                if (!file->seek(static_cast<qint64>(file_ofs)))
                    return 0;
                written = file_ofs;
            }
        }
        buf.append(static_cast<const char*>(p), static_cast<int>(n));
        if (buf.size() >= flushThreshold && !flush())
            return 0;
        return n;
    }

    bool flush() {
        if (buf.isEmpty())
            return true;
        const qint64 n = file->write(buf.constData(), buf.size());
        if (n != buf.size())
            return false;
        written += static_cast<mz_uint64>(buf.size());
        buf.clear();
        return true;
    }
};

// 遍历 GameData 下所有需要打包的文件，排除 Squad/SquadExpansion 和 ModuleManager.* 文件。
// 对每个符合的文件调用 fn(absPath, zipPath, size)。用回调而非收集列表，避免巨型整合包把
// 全部文件条目驻留内存；导出过程中不额外保存文件清单。
template <typename Fn>
bool walkExportFiles(const QString& gameDataPath, const QString& basePath, Fn&& fn) {
    auto isExcludedDir = [](const QString& path) -> bool {
        QString normalized = QDir::fromNativeSeparators(path);
        // 取 GameData 之后的第一级目录名，排除 Squad/SquadExpansion
        int idx = normalized.indexOf("/GameData/", Qt::CaseInsensitive);
        if (idx < 0) return false;
        QString rest = normalized.mid(idx + 10); // "/GameData/" = 10 chars
        if (rest.startsWith("Squad/", Qt::CaseInsensitive)
            || rest.startsWith("SquadExpansion/", Qt::CaseInsensitive))
            return true;
        if (rest.compare("Squad", Qt::CaseInsensitive) == 0
            || rest.compare("SquadExpansion", Qt::CaseInsensitive) == 0)
            return true;
        return false;
    };

    // 仅在 GameData 根目录下排除的 ModuleManager.* 文件
    static const QStringList excludedRootFiles = {
        "ModuleManager.ConfigCache",
        "ModuleManager.ConfigSHA",
        "ModuleManager.Physics",
        "ModuleManager.TechTree"
    };
    auto isExcludedRootFile = [&](const QString& absPath) -> bool {
        QFileInfo info(absPath);
        if (QDir::toNativeSeparators(info.absolutePath())
                .compare(QDir::toNativeSeparators(gameDataPath), Qt::CaseInsensitive) == 0) {
            return excludedRootFiles.contains(info.fileName());
        }
        return false;
    };

    QDirIterator it(gameDataPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString absPath = it.filePath();
        if (isExcludedDir(absPath) || isExcludedRootFile(absPath))
            continue;
        const QFileInfo info = it.fileInfo();
        // basePath 是游戏根目录，相对路径自动包含 GameData/ 前缀
        const QString zipPath = QDir(basePath).relativeFilePath(absPath).replace('\\', '/');
        if (!fn(absPath, zipPath, info.size()))
            return false;
    }
    return true;
}

// 确保 zip 中的父目录条目已存在（去重，保留空目录）。
void ensureExportDirs(mz_zip_archive& zip, const QString& dirZipPath, QSet<QString>& dirsAdded) {
    if (dirZipPath.isEmpty() || dirZipPath == ".")
        return;
    QString accum;
    for (const QString& part : dirZipPath.split('/')) {
        accum += part + "/";
        if (!dirsAdded.contains(accum)) {
            dirsAdded.insert(accum);
            mz_zip_writer_add_mem(&zip, accum.toUtf8().constData(), nullptr, 0, 2);
        }
    }
}

} // namespace

bool InstanceManager::exportModpack(const QString &gamePath, const QString &zipFilePath,
                                     std::function<void(int progress)> progressCallback,
                                     std::function<bool()> shouldCancel) const
{
    QString gameDataPath = QDir(gamePath).filePath("GameData");
    QDir gameDataDir(gameDataPath);
    if (!gameDataDir.exists()) {
        qWarning() << "GameData folder does not exist:" << gameDataPath;
        return false;
    }

    if (progressCallback) {
        progressCallback(0);
    }

    // 第一遍：仅统计待打包文件总字节并确认非空，不保存文件清单（省内存）。
    qint64 totalSize = 0;
    int fileCount = 0;
    const bool walkOk = walkExportFiles(gameDataPath, gamePath, [&](const QString&, const QString&, qint64 size) {
        if (shouldCancel && shouldCancel()) {
            return false; // 用户取消：立即中断遍历
        }
        totalSize += size;
        ++fileCount;
        return true;
    });
    if (shouldCancel && shouldCancel()) {
        return false; // 统计阶段用户已取消
    }
    if (!walkOk) {
        qWarning() << "Export file walk interrupted:" << gameDataPath;
        return false;
    }
    if (fileCount == 0) {
        qWarning() << "No files to export in GameData:" << gameDataPath;
        return false;
    }

    // 使用QFile作为写入后端（支持Unicode路径）
    QFile zipFile(zipFilePath);
    if (!zipFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open zip file:" << zipFilePath;
        return false;
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    // 写缓冲后端：miniz 写本地头/文件名/extra/压缩输出等小块时，攒满 1MB 再一次性落盘，
    // 避免海量小文件下每次 seek+write 的系统调用开销。顺序写入，非顺序偏移兜底。
    BufferedFileWriter bufWriter;
    bufWriter.file = &zipFile;
    zip.m_pWrite = [](void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n) -> size_t {
        BufferedFileWriter* writer = static_cast<BufferedFileWriter*>(pOpaque);
        return writer->write(file_ofs, pBuf, n);
    };
    zip.m_pIO_opaque = &bufWriter;

    if (!mz_zip_writer_init_v2(&zip, 0, 0)) {
        qWarning() << "Failed to initialize ZIP writer:" << zipFilePath
                   << "error:" << mz_zip_get_last_error(&zip);
        zipFile.close();
        return false;
    }

    // 第二遍：逐条写入。通用内容始终流式读取；小文件读入受限缓冲后一次写入（KSP 大量小文件）。
    bool success = true;
    qint64 processedSize = 0;
    QSet<QString> dirsAdded;
    success = walkExportFiles(gameDataPath, gamePath,
        [&](const QString& absPath, const QString& zipPath, qint64 size) -> bool {
            // 用户取消：立即中断写入，不再继续打包
            if (shouldCancel && shouldCancel())
                return false;

            ensureExportDirs(zip, QFileInfo(zipPath).path(), dirsAdded);

            QFile file(absPath);
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning() << "Failed to open file for export:" << absPath;
                return true; // 跳过无法打开的文件（保持旧行为）
            }

            bool ok = false;
            if (size <= kSmallFileThreshold) {
                // 小文件：一次性读入并 add_mem，块大小受阈值限制（峰值内存可控）
                const QByteArray data = file.readAll();
                ok = mz_zip_writer_add_mem(
                    &zip, zipPath.toUtf8().constData(),
                    data.constData(), static_cast<size_t>(data.size()), 2);
            } else {
                // 大文件：流式回调读取，内存固定
                FileReadContext ctx = { &file };
                ok = mz_zip_writer_add_read_buf_callback(
                    &zip, zipPath.toUtf8().constData(), fileReadCallback, &ctx,
                    static_cast<mz_uint64>(size),
                    nullptr, nullptr, 0, 2, nullptr, 0, nullptr, 0);
            }
            file.close();

            if (!ok) {
                qWarning() << "Failed to add to zip:" << absPath
                           << "error:" << mz_zip_get_last_error(&zip);
                return false;
            }

            processedSize += size;
            if (progressCallback && totalSize > 0) {
                int progress = static_cast<int>((processedSize * 100) / totalSize);
                progressCallback(qMin(progress, 99));
            }
            return true;
        });

    if (success) {
        if (!mz_zip_writer_finalize_archive(&zip)) {
            success = false;
            qWarning() << "Failed to finalize ZIP archive, error:" << mz_zip_get_last_error(&zip);
        }
    }

    mz_zip_writer_end(&zip);
    // 把写缓冲中剩余的小块全部落盘后再关闭文件，确保归档完整。
    if (success && !bufWriter.flush()) {
        success = false;
        qWarning() << "Failed to flush ZIP write buffer:" << zipFilePath;
    }
    zipFile.close();

    if (!success) {
        QFile::remove(zipFilePath);
        return false;
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return true;
}
