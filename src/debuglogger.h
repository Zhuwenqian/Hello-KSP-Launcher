#ifndef DEBUGLOGGER_H
#define DEBUGLOGGER_H

#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QtGlobal>

// 调试日志：仅当设置中启用“调试模式”时，从本会话启动起向启动器目录下 HKSPL.log
// 写入全部 qInfo/qDebug/qWarning/qCritical 输出。是否启用在启动时按已持久化的配置
// 一次性决定，因此“开启”的本会话不写、从下次启动起生效；关闭时本会话一路不写，
// 也不删除既有日志文件。每次启动都会清空重写 HKSPL.log。
class DebugLogger
{
public:
    static DebugLogger& instance();

    // 按当前配置启用调试日志（启动时调用一次，不即时响应运行中改动）。
    void start();
    bool enabled() const { return m_enabled; }

private:
    DebugLogger();
    ~DebugLogger();

    // 供 qInstallMessageHandler 安装的全局消息处理器入口。
    static void logMessage(QtMsgType type, const QMessageLogContext &ctx,
                           const QString &msg);
    void writeLine(QtMsgType type, const QString &msg);

    QMutex m_mutex;
    QFile m_file;
    QTextStream m_stream;
    bool m_enabled = false;
};

#endif // DEBUGLOGGER_H