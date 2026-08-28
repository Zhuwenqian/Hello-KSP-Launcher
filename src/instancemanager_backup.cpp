// 实例管理器 - 存档备份管理
#include "instancemanager.h"
#include "miniz.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>

namespace {

// 替换文件名中的非法字符
QString sanitizeFileName(const QString& name)
{
    QString safe = name;
    safe.replace('<', '_').replace('>', '_').replace(':', '_').replace('"', '_')
        .replace('/', '_').replace('\\', '_').replace('|', '_').replace('?', '_').replace('*', '_');
    return safe;
}

// 递归删除目录下所有内容（含子目录）
void removeDirContents(const QString& dirPath)
{
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            removeDirContents(info.absoluteFilePath());
            QDir().rmdir(info.absoluteFilePath());
        } else {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

// 递归计算目录总大小
qint64 calculateDirSize(const QString& dirPath) {
    qint64 size = 0;
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            size += calculateDirSize(info.absoluteFilePath());
        } else {
            size += info.size();
        }
    }
    return size;
}

// 递归添加文件到ZIP，保持目录结构（miniz高性能实现）
bool addDirectoryToZip(mz_zip_archive& zip, const QString& basePath, const QString& dirPath,
                       qint64 totalSize, qint64& processedSize,
                       std::function<void(int progress)>& progressCallback) {
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo& info : entries) {
        QString relativePath = QDir(basePath).relativeFilePath(info.absoluteFilePath());
        // 转换为ZIP路径格式（正斜杠）
        QString zipPath = relativePath.replace('\\', '/');

        if (info.isDir()) {
            // 目录条目以/结尾
            mz_zip_writer_add_mem(&zip, (zipPath + "/").toUtf8().constData(), nullptr, 0, MZ_DEFAULT_LEVEL);
            if (!addDirectoryToZip(zip, basePath, info.absoluteFilePath(), totalSize, processedSize, progressCallback)) {
                return false;
            }
        } else {
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning() << "Failed to open file for backup:" << info.absoluteFilePath();
                continue;
            }
            QByteArray data = file.readAll();
            file.close();

            if (!mz_zip_writer_add_mem(&zip, zipPath.toUtf8().constData(), data.constData(), data.size(), MZ_DEFAULT_LEVEL)) {
                qWarning() << "Failed to add to zip:" << info.absoluteFilePath()
                           << "error:" << mz_zip_get_last_error(&zip);
                return false;
            }

            processedSize += info.size();
            if (progressCallback && totalSize > 0) {
                int progress = static_cast<int>((processedSize * 100) / totalSize);
                progressCallback(qMin(progress, 99));
            }
        }
    }
    return true;
}

// 将备份ZIP解压到目标目录（QFile后端以支持Unicode路径，miniz高性能实现）
// 带路径穿越防护与进度回调
bool extractZipToDirectory(const QString& zipFilePath, const QString& destPath,
                           const std::function<void(int progress)>& progressCallback)
{
    QFile zipFile(zipFilePath);
    if (!zipFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open backup zip:" << zipFilePath;
        return false;
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    zip.m_pRead = [](void* pOpaque, mz_uint64 file_ofs, void* pBuf, size_t n) -> size_t {
        QFile* f = static_cast<QFile*>(pOpaque);
        if (f->seek(static_cast<qint64>(file_ofs))) {
            return static_cast<size_t>(f->read(static_cast<char*>(pBuf), static_cast<qint64>(n)));
        }
        return 0;
    };
    zip.m_pIO_opaque = &zipFile;

    if (!mz_zip_reader_init(&zip, static_cast<mz_uint64>(zipFile.size()), 0)) {
        qWarning() << "Failed to init zip reader:" << zipFilePath
                   << "error:" << mz_zip_get_last_error(&zip);
        zipFile.close();
        return false;
    }

    const int fileCount = static_cast<int>(mz_zip_reader_get_num_files(&zip));

    // 预统计解压总大小用于进度显示
    mz_uint64 totalBytes = 0;
    for (int i = 0; i < fileCount; ++i) {
        mz_zip_archive_file_stat fstat;
        if (mz_zip_reader_file_stat(&zip, i, &fstat)) {
            totalBytes += fstat.m_uncomp_size;
        }
    }

    if (progressCallback) progressCallback(0);

    QDir dest(destPath);
    if (!dest.exists()) {
        dest.mkpath(".");
    }

    // 安全路径：确保解压结果落在destPath内，防止路径穿越
    const QString destAbs = QDir::cleanPath(destPath);
    mz_uint64 extractedBytes = 0;
    bool ok = true;

    for (int i = 0; i < fileCount; ++i) {
        mz_zip_archive_file_stat fstat;
        if (!mz_zip_reader_file_stat(&zip, i, &fstat)) {
            ok = false;
            break;
        }

        QString entryName = QString::fromUtf8(fstat.m_filename);

        // 路径穿越防护
        QString normalized = QDir::cleanPath(entryName);
        if (normalized.startsWith("..") && (normalized == ".." || normalized.startsWith("../"))) {
            qWarning() << "Blocked path traversal entry in backup:" << entryName;
            ok = false;
            break;
        }

        QString targetPath = QDir::cleanPath(QDir(destAbs).filePath(entryName));
        if (!targetPath.startsWith(destAbs)) {
            qWarning() << "Blocked path traversal entry in backup:" << entryName;
            ok = false;
            break;
        }

        // 目录条目
        if (fstat.m_is_directory || entryName.endsWith('/')) {
            QDir().mkpath(targetPath);
            continue;
        }

        // 确保父目录存在
        QFileInfo fi(targetPath);
        if (!QDir().mkpath(fi.absolutePath())) {
            ok = false;
            break;
        }

        size_t memSize = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(i), &memSize, 0);
        if (!data) {
            qWarning() << "Failed to extract entry:" << entryName
                       << "error:" << mz_zip_get_last_error(&zip);
            ok = false;
            break;
        }

        QFile outFile(targetPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "Failed to write file:" << targetPath;
            mz_free(data);
            ok = false;
            break;
        }
        outFile.write(static_cast<const char*>(data), static_cast<qint64>(memSize));
        outFile.close();
        mz_free(data);

        extractedBytes += fstat.m_uncomp_size;
        if (progressCallback && totalBytes > 0) {
            int progress = static_cast<int>((extractedBytes * 100) / totalBytes);
            progressCallback(qMin(progress, 99));
        }
    }

    mz_zip_reader_end(&zip);
    zipFile.close();

    if (ok && progressCallback) progressCallback(100);
    return ok;
}

} // namespace

QString InstanceManager::getBackupsRootDir() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString backupsDir = QDir(appDir).filePath("backups");
    QDir dir(backupsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return backupsDir;
}

QString InstanceManager::getBackupDirForSave(const QString &instanceName, const QString &saveName) const
{
    QString rootDir = getBackupsRootDir();
    // 先迁移可能存在的旧单层结构备份，再返回新两级目录
    migrateLegacyBackups(instanceName, saveName);

    // 移除文件名中的非法字符（实例名与存档名）
    QString safeInstance = sanitizeFileName(instanceName);
    QString safeSave = sanitizeFileName(saveName);
    QString saveBackupDir = QDir(QDir(rootDir).filePath(safeInstance)).filePath(safeSave);
    QDir dir(saveBackupDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return saveBackupDir;
}

void InstanceManager::migrateLegacyBackups(const QString &instanceName, const QString &saveName) const
{
    QString rootDir = getBackupsRootDir();
    QString safeInstance = sanitizeFileName(instanceName);
    QString safeSave = sanitizeFileName(saveName);
    // 旧单层结构目录（仅当目标两级目录尚不存在时才迁移，保持幂等）
    QString legacyDir = QDir(rootDir).filePath(safeSave);
    QString newDir = QDir(QDir(rootDir).filePath(safeInstance)).filePath(safeSave);

    if (legacyDir == newDir) return;
    if (QDir(newDir).exists()) return;

    QDir legacy(legacyDir);
    if (!legacy.exists()) return;

    QDir newBase = QDir(QDir(rootDir).filePath(safeInstance));
    if (!newBase.exists()) newBase.mkpath(".");

    // 迁移该目录下的zip备份到新位置
    QStringList filters;
    filters << "*.zip";
    const QFileInfoList files = legacy.entryInfoList(filters, QDir::Files);
    for (const QFileInfo& fi : files) {
        if (!QDir().mkpath(newDir)) {
            qWarning() << "Failed to create backup dir for migration:" << newDir;
            return;
        }
        QString target = QDir(newDir).filePath(fi.fileName());
        if (QFile::rename(fi.absoluteFilePath(), target)) {
            qInfo() << "Migrated legacy backup:" << fi.absoluteFilePath() << "->" << target;
        } else if (!QFile::exists(target)) {
            // 重命名失败（可能跨目录/权限），尝试复制后删除原文件
            if (QFile::copy(fi.absoluteFilePath(), target)) {
                QFile::remove(fi.absoluteFilePath());
            } else {
                qWarning() << "Failed to migrate backup:" << fi.absoluteFilePath();
            }
        }
    }

    // 移除空的旧目录
    if (legacy.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
        QDir().rmdir(legacyDir);
    }
}

QList<BackupInfo> InstanceManager::listBackups(const QString &instanceName, const QString &saveName) const
{
    QList<BackupInfo> backups;
    QString backupDir = getBackupDirForSave(instanceName, saveName);
    QDir dir(backupDir);

    QStringList filters;
    filters << "*.zip";
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    for (const QFileInfo& info : entries) {
        BackupInfo backup;
        backup.fileName = info.fileName();
        backup.filePath = info.absoluteFilePath();
        backup.saveName = saveName;
        backup.fileSize = info.size();

        // 从文件名解析时间戳：存档名[_备注]_yyyyMMdd_HHmmss.zip
        QString baseName = info.baseName();
        int lastUnderscore = baseName.lastIndexOf('_');
        if (lastUnderscore > 0) {
            QString timePart = baseName.mid(lastUnderscore + 1);
            int prevUnderscore = baseName.lastIndexOf('_', lastUnderscore - 1);
            if (prevUnderscore > 0) {
                QString datePart = baseName.mid(prevUnderscore + 1, lastUnderscore - prevUnderscore - 1);
                QString dateTimeStr = datePart + timePart;
                backup.timestamp = QDateTime::fromString(dateTimeStr, "yyyyMMddHHmmss");
            }
            // 检测备注标记：形如 xxx_恢复前_日期_时间.zip
            QString labelPart = baseName.mid(0, prevUnderscore);
            if (labelPart.endsWith("_恢复前")) {
                backup.note = QObject::tr("恢复前备份");
            }
        }
        if (!backup.timestamp.isValid()) {
            backup.timestamp = info.lastModified();
        }

        backups.append(backup);
    }

    return backups;
}

bool InstanceManager::createBackup(const QString &saveFolderPath, const QString &instanceName,
                                   const QString &saveName, const QString &note,
                                   std::function<void(int progress)> progressCallback) const
{
    QDir saveDir(saveFolderPath);
    if (!saveDir.exists()) {
        qWarning() << "Save folder does not exist:" << saveFolderPath;
        return false;
    }

    // 检查存档目录是否为空
    QFileInfoList entries = saveDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        qWarning() << "Save folder is empty:" << saveFolderPath;
        return false;
    }

    QString backupDir = getBackupDirForSave(instanceName, saveName);

    // 生成文件名：存档名[_备注]_yyyyMMdd_HHmmss.zip
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString safeSaveName = sanitizeFileName(saveName);
    QString safeNote = sanitizeFileName(note);
    QString suffix = safeNote.isEmpty() ? "" : "_" + safeNote;
    QString backupFileName = QString("%1%2_%3.zip").arg(safeSaveName, suffix, timestamp);
    QString backupFilePath = QDir(backupDir).filePath(backupFileName);

    if (progressCallback) {
        progressCallback(0);
    }

    // 计算存档目录总大小，用于进度显示
    qint64 totalSize = calculateDirSize(saveFolderPath);
    qint64 processedSize = 0;

    // 使用miniz高性能ZIP压缩库创建备份
    // 注意：不能使用mz_zip_writer_init_file（内部用fopen按ANSI代码页解析路径，
    // 中文存档名会因系统代码页（UTF-8/英文）不一致而打不开），
    // 改用QFile作为写入后端（QFile使用Unicode路径，跨代码页均正常）
    QFile zipFile(backupFilePath);
    if (!zipFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open backup file:" << backupFilePath;
        return false;
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    // 自定义写入回调：miniz将数据写入QFile
    zip.m_pWrite = [](void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n) -> size_t {
        QFile* f = static_cast<QFile*>(pOpaque);
        if (f->seek(static_cast<qint64>(file_ofs))) {
            return static_cast<size_t>(f->write(static_cast<const char*>(pBuf), static_cast<qint64>(n)));
        }
        return 0;
    };
    zip.m_pIO_opaque = &zipFile;

    if (!mz_zip_writer_init_v2(&zip, 0, 0)) {
        qWarning() << "Failed to initialize ZIP file:" << backupFilePath
                   << "error:" << mz_zip_get_last_error(&zip);
        zipFile.close();
        return false;
    }

    bool success = addDirectoryToZip(zip, saveFolderPath, saveFolderPath, totalSize, processedSize, progressCallback);

    if (success) {
        if (!mz_zip_writer_finalize_archive(&zip)) {
            success = false;
            qWarning() << "Failed to finalize ZIP archive, error:" << mz_zip_get_last_error(&zip);
        }
    }

    mz_zip_writer_end(&zip);
    zipFile.close();

    if (!success) {
        QFile::remove(backupFilePath);
        return false;
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return true;
}

bool InstanceManager::deleteBackup(const QString &backupFilePath) const
{
    return QFile::remove(backupFilePath);
}

bool InstanceManager::revealBackupInExplorer(const QString &backupFilePath) const
{
    QFileInfo info(backupFilePath);
    if (!info.exists()) {
        return false;
    }

    // Windows: 使用 explorer /select, 来选中文件
    QStringList args;
    args << "/select," << QDir::toNativeSeparators(backupFilePath);
    return QProcess::startDetached("explorer", args);
}

bool InstanceManager::restoreBackup(const QString &backupFilePath, const QString &saveFolderPath,
                                    const QString &instanceName, const QString &saveName,
                                    std::function<void(int progress)> progressCallback) const
{
    QFileInfo backupInfo(backupFilePath);
    if (!backupInfo.exists()) {
        qWarning() << "Backup file does not exist:" << backupFilePath;
        return false;
    }

    QDir saveDir(saveFolderPath);
    if (!saveDir.exists()) {
        qWarning() << "Save folder does not exist:" << saveFolderPath;
        return false;
    }

    // 1. 恢复前自动备份当前状态，作为安全网
    QFileInfoList entries = saveDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    if (!entries.isEmpty()) {
        if (progressCallback) progressCallback(1);
        bool backupOk = createBackup(saveFolderPath, instanceName, saveName, tr("恢复前"), progressCallback);
        if (!backupOk) {
            qWarning() << "Failed to create pre-restore backup; abort restore:" << saveFolderPath;
            return false;
        }
    }

    // 2. 清空存档目录
    if (progressCallback) progressCallback(2);
    removeDirContents(saveFolderPath);

    // 3. 解压备份内容，若解压失败则部分文件可能已写入，返回失败由界面提示
    bool ok = extractZipToDirectory(backupFilePath, saveFolderPath, progressCallback);
    if (!ok) {
        qWarning() << "Failed to extract backup:" << backupFilePath;
    }
    return ok;
}
