#include "backgroundmanager.h"
#include "configmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QDebug>

BackgroundManager& BackgroundManager::instance()
{
    static BackgroundManager inst;
    return inst;
}

BackgroundManager::BackgroundManager(QObject* parent)
    : QObject(parent)
{
}

void BackgroundManager::initialize()
{
    const QString stored = ConfigManager::instance().backgroundPath();
    loadFromSource(resolveStoredPath(stored));
}

QString BackgroundManager::resolveStoredPath(const QString& stored)
{
    if (stored.isEmpty() || stored == QStringLiteral("default")) {
        return QString(); // empty means use resource default
    }
    return stored;
}

QString BackgroundManager::backgroundDir() const
{
    QString dir = QCoreApplication::applicationDirPath();
    return QDir(dir).filePath("backgrounds");
}

void BackgroundManager::loadDefault()
{
    loadFromSource(QString());
}

void BackgroundManager::loadFromSource(const QString& path)
{
    QPixmap pix;
    if (path.isEmpty()) {
        // 加载资源中默认背景
        pix.load(QStringLiteral(":/backgrounds/default.png"));
        if (pix.isNull()) {
            qWarning() << "[BackgroundManager] failed to load default background from resources";
        }
    } else {
        if (!QFile::exists(path)) {
            qWarning() << "[BackgroundManager] background file missing:" << path;
            return;
        }
        pix.load(path);
        if (pix.isNull()) {
            qWarning() << "[BackgroundManager] failed to load background image:" << path;
            return;
        }
    }

    m_pixmap = pix;
    emit backgroundChanged();
}

bool BackgroundManager::setUserBackground(const QString& sourceFilePath)
{
    if (sourceFilePath.isEmpty()) {
        return false;
    }

    QFileInfo srcInfo(sourceFilePath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        qWarning() << "[BackgroundManager] user background source not found:" << sourceFilePath;
        return false;
    }

    // 只支持 png/jpg/jpeg
    const QString suffix = srcInfo.suffix().toLower();
    if (suffix != "png" && suffix != "jpg" && suffix != "jpeg") {
        qWarning() << "[BackgroundManager] unsupported image format:" << suffix;
        return false;
    }

    QDir dir(backgroundDir());
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        qWarning() << "[BackgroundManager] failed to create background dir:" << dir.absolutePath();
        return false;
    }

    // 复制为新文件，避免原文件被移动/删除后失效
    const QString newName = QStringLiteral("user_bg_%1.%2")
                                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces), suffix);
    const QString destPath = dir.filePath(newName);

    // 拷贝前先尝试读取验证图片是否有效
    QPixmap testPix(sourceFilePath);
    if (testPix.isNull()) {
        qWarning() << "[BackgroundManager] source file is not a valid image:" << sourceFilePath;
        return false;
    }

    if (!QFile::copy(sourceFilePath, destPath)) {
        qWarning() << "[BackgroundManager] failed to copy background to:" << destPath;
        return false;
    }

    // 通知配置层保存
    ConfigManager::instance().setBackgroundPath(destPath);

    // 立即加载新背景
    loadFromSource(destPath);
    return true;
}

void BackgroundManager::resetToDefault()
{
    ConfigManager::instance().setBackgroundPath(QString());
    loadDefault();
}

QString BackgroundManager::currentSourceDescription() const
{
    const QString stored = ConfigManager::instance().backgroundPath();
    if (stored.isEmpty() || stored == QStringLiteral("default")) {
        return QStringLiteral("默认背景");
    }
    return stored;
}
