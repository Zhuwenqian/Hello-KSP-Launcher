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

QString InstanceManager::getBackupDirForSave(const QString &saveName) const
{
    QString rootDir = getBackupsRootDir();
    // 移除文件名中的非法字符
    QString safeName = saveName;
    safeName.replace('<', '_').replace('>', '_').replace(':', '_').replace('"', '_')
             .replace('/', '_').replace('\\', '_').replace('|', '_').replace('?', '_').replace('*', '_');
    QString saveBackupDir = QDir(rootDir).filePath(safeName);
    QDir dir(saveBackupDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return saveBackupDir;
}

QList<BackupInfo> InstanceManager::listBackups(const QString &saveName) const
{
    QList<BackupInfo> backups;
    QString backupDir = getBackupDirForSave(saveName);
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

        // 从文件名解析时间戳：存档名_yyyyMMdd_HHmmss.zip
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
        }
        if (!backup.timestamp.isValid()) {
            backup.timestamp = info.lastModified();
        }

        backups.append(backup);
    }

    return backups;
}

bool InstanceManager::createBackup(const QString &saveFolderPath, const QString &saveName,
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

    QString backupDir = getBackupDirForSave(saveName);

    // 生成文件名：存档名_yyyyMMdd_HHmmss.zip
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString safeSaveName = saveName;
    safeSaveName.replace('<', '_').replace('>', '_').replace(':', '_').replace('"', '_')
                  .replace('/', '_').replace('\\', '_').replace('|', '_').replace('?', '_').replace('*', '_');
    QString backupFileName = QString("%1_%2.zip").arg(safeSaveName, timestamp);
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
