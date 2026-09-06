#include "debuglogger.h"

#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include <QMessageLogContext>

#include "configmanager.h"

DebugLogger& DebugLogger::instance()
{
    static DebugLogger inst;
    return inst;
}

DebugLogger::DebugLogger() = default;

DebugLogger::~DebugLogger()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void DebugLogger::start()
{
    const bool debug = ConfigManager::instance().debugMode();
    if (!debug)
        return;

    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen())
        return;

    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath("HKSPL.log");
    m_file.setFileName(path);
    // 每次启动清空重写，只保留本次运行日志
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    m_stream.setDevice(&m_file);
    m_enabled = true;
    qInstallMessageHandler(logMessage);

    m_stream << QDateTime::currentDateTime().toString(Qt::ISODate)
             << " [INFO] [app] 调试日志已启用，本次启动写入 " << path << Qt::endl;
    m_stream.flush();
}

void DebugLogger::logMessage(QtMsgType type, const QMessageLogContext &ctx,
                             const QString &msg)
{
    Q_UNUSED(ctx);
    instance().writeLine(type, msg);
}

void DebugLogger::writeLine(QtMsgType type, const QString &msg)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled || !m_file.isOpen())
        return;

    QString level;
    switch (type) {
    case QtDebugMsg:    level = QStringLiteral("DEBUG"); break;
    case QtInfoMsg:     level = QStringLiteral("INFO");  break;
    case QtWarningMsg:  level = QStringLiteral("WARN");  break;
    case QtCriticalMsg: level = QStringLiteral("ERROR"); break;
    case QtFatalMsg:    level = QStringLiteral("FATAL"); break;
    }

    // 字段：时间戳 [级别] [线程ID] 消息。线程ID用当前操作系统线程标识后16位十六进制。
    m_stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
             << " [" << level << "] ["
             << QString::number(qulonglong(QThread::currentThreadId()), 16).leftJustified(8, QLatin1Char('0'))
             << "] " << msg << Qt::endl;
    m_stream.flush();
}