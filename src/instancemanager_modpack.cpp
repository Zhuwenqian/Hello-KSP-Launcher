// 实例管理器 - 整合包导出（GameData 打包为 ZIP）
#include "instancemanager.h"
#include "miniz.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QSet>
#include <algorithm>

namespace {

// 导出文件条目
struct ExportFileEntry {
    QString absolutePath;
    QString zipPath;
    qint64 size;
};

// 收集所有需要打包的文件，排除 Squad/SquadExpansion 和 ModuleManager.* 文件
// 使用 QDirIterator::Subdirectories 单次遍历目录树，避免递归开销
QList<ExportFileEntry> collectExportFiles(const QString& gameDataPath, const QString& basePath,
                                          qint64& totalSize) {
    QList<ExportFileEntry> files;
    totalSize = 0;

    // 排除的根目录名（不区分大小写）
    auto isExcludedDir = [](const QString& path) -> bool {
        // 检查路径是否包含 /Squad/ 或 /SquadExpansion/（在 GameData 下的第一级）
        // 或者路径本身就是 Squad/SquadExpansion 子路径
        QString normalized = QDir::fromNativeSeparators(path);
        // 取 GameData 之后的第一级目录名
        int idx = normalized.indexOf("/GameData/", Qt::CaseInsensitive);
        if (idx < 0) return false;
        QString rest = normalized.mid(idx + 10); // "/GameData/" = 10 chars
        // Squad 或 SquadExpansion 开头
        if (rest.startsWith("Squad/", Qt::CaseInsensitive) || rest.startsWith("SquadExpansion/", Qt::CaseInsensitive)) {
            return true;
        }
        if (rest.compare("Squad", Qt::CaseInsensitive) == 0 || rest.compare("SquadExpansion", Qt::CaseInsensitive) == 0) {
            return true;
        }
        return false;
    };

    // 排除的根目录文件名
    static const QStringList excludedRootFiles = {
        "ModuleManager.ConfigCache",
        "ModuleManager.ConfigSHA",
        "ModuleManager.Physics",
        "ModuleManager.TechTree"
    };

    auto isExcludedRootFile = [&](const QString& absPath) -> bool {
        // 仅在 GameData 根目录下排除
        QFileInfo info(absPath);
        QString parentDir = QDir::toNativeSeparators(info.absolutePath());
        QString gdPath = QDir::toNativeSeparators(gameDataPath);
        if (parentDir.compare(gdPath, Qt::CaseInsensitive) == 0) {
            return excludedRootFiles.contains(info.fileName());
        }
        return false;
    };

    QDirIterator it(gameDataPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString absPath = it.filePath();

        // 排除 Squad/SquadExpansion 下的所有文件
        if (isExcludedDir(absPath)) {
            continue;
        }
        // 排除 GameData 根目录下的 ModuleManager.* 文件
        if (isExcludedRootFile(absPath)) {
            continue;
        }

        QFileInfo info = it.fileInfo();
        ExportFileEntry entry;
        entry.absolutePath = absPath;
        // basePath 是游戏根目录，相对路径自动包含 GameData/ 前缀
        entry.zipPath = QDir(basePath).relativeFilePath(absPath).replace('\\', '/');
        entry.size = info.size();
        totalSize += entry.size;
        files.append(entry);
    }

    // 按路径排序，确保同一目录的文件连续处理，减少磁盘寻道
    std::sort(files.begin(), files.end(), [](const ExportFileEntry& a, const ExportFileEntry& b) {
        return a.absolutePath < b.absolutePath;
    });

    return files;
}

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

// 使用扁平文件列表导出整合包，分两步：先收集所有文件，再按路径顺序写入 ZIP
// 压缩级别 2（快速），流式读取，路径排序提升磁盘访问局部性
bool addExportFilesToZip(mz_zip_archive& zip, const QString& basePath,
                         const QList<ExportFileEntry>& files,
                         qint64 totalSize, qint64& processedSize,
                         std::function<void(int progress)>& progressCallback) {
    // 已添加目录的追踪，避免重复添加空目录条目
    QSet<QString> dirsAdded;

    for (const ExportFileEntry& entry : files) {
        // 确保父目录条目已存在
        QString dirZipPath = QFileInfo(entry.zipPath).path();
        if (!dirZipPath.isEmpty() && dirZipPath != ".") {
            // 逐级添加父目录
            QStringList parts = dirZipPath.split('/');
            QString accum;
            for (const QString& part : parts) {
                accum += part + "/";
                if (!dirsAdded.contains(accum)) {
                    dirsAdded.insert(accum);
                    mz_zip_writer_add_mem(&zip, accum.toUtf8().constData(), nullptr, 0, 2);
                }
            }
        }

        // 流式读取文件并添加到 ZIP
        QFile file(entry.absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open file for export:" << entry.absolutePath;
            continue;
        }

        FileReadContext ctx = { &file };
        bool ok = mz_zip_writer_add_read_buf_callback(
            &zip,
            entry.zipPath.toUtf8().constData(),
            fileReadCallback,
            &ctx,
            entry.size,
            nullptr,  // pFile_time
            nullptr,  // pComment
            0,        // comment_size
            2,        // 压缩级别 2（快速，兼顾 I/O 与压缩比）
            nullptr, 0, nullptr, 0
        );
        file.close();

        if (!ok) {
            qWarning() << "Failed to add to zip:" << entry.absolutePath
                       << "error:" << mz_zip_get_last_error(&zip);
            return false;
        }

        processedSize += entry.size;
        if (progressCallback && totalSize > 0) {
            int progress = static_cast<int>((processedSize * 100) / totalSize);
            progressCallback(qMin(progress, 99));
        }
    }
    return true;
}

} // namespace

bool InstanceManager::exportModpack(const QString &gamePath, const QString &zipFilePath,
                                     std::function<void(int progress)> progressCallback) const
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

    // 先收集所有需要打包的文件（扁平列表，单次遍历目录树），同时计算总大小
    qint64 totalSize = 0;
    QList<ExportFileEntry> files = collectExportFiles(gameDataPath, gamePath, totalSize);
    if (files.isEmpty()) {
        qWarning() << "No files to export in GameData:" << gameDataPath;
        return false;
    }
    qint64 processedSize = 0;

    // 使用QFile作为写入后端（支持Unicode路径）
    QFile zipFile(zipFilePath);
    if (!zipFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open zip file:" << zipFilePath;
        return false;
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    zip.m_pWrite = [](void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n) -> size_t {
        QFile* f = static_cast<QFile*>(pOpaque);
        if (f->seek(static_cast<qint64>(file_ofs))) {
            return static_cast<size_t>(f->write(static_cast<const char*>(pBuf), static_cast<qint64>(n)));
        }
        return 0;
    };
    zip.m_pIO_opaque = &zipFile;

    if (!mz_zip_writer_init_v2(&zip, 0, 0)) {
        qWarning() << "Failed to initialize ZIP writer:" << zipFilePath
                   << "error:" << mz_zip_get_last_error(&zip);
        zipFile.close();
        return false;
    }

    // 使用扁平文件列表按路径顺序写入 ZIP
    bool success = addExportFilesToZip(zip, gamePath, files, totalSize, processedSize, progressCallback);

    if (success) {
        if (!mz_zip_writer_finalize_archive(&zip)) {
            success = false;
            qWarning() << "Failed to finalize ZIP archive, error:" << mz_zip_get_last_error(&zip);
        }
    }

    mz_zip_writer_end(&zip);
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
