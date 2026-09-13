#include "playerloganalyzer.h"

#include <QFile>
#include <QDir>
#include <QRegularExpression>

namespace playerlog {

// 只保留尾部一段，避免对可能数百 MB 的 Player.log 做全量读取。
static const qint64 kTailBytes = 512 * 1024;

// 定位关键错误在 content 中的起始偏移；无匹配返回 -1。
// kind 决定使用哪类关键错误线索（OOM 优先于 HardCrash，与 analyzePlayerLog 一致）。
static int locateErrorOffset(const QString& content, PlayerLogAnalysis::Kind kind)
{
    if (kind == PlayerLogAnalysis::OutOfMemory) {
        int off = content.indexOf(QStringLiteral("OutOfMemoryException"));
        if (off < 0)
            off = content.indexOf(QStringLiteral("Out of memory"), 0, Qt::CaseInsensitive);
        return off;
    }
    if (kind == PlayerLogAnalysis::HardCrash) {
        static const QRegularExpression re(
            QStringLiteral("Caught fatal signal"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(content);
        return m.hasMatch() ? m.capturedStart() : -1;
    }
    return -1;
}

PlayerLogAnalysis analyzePlayerLog(const QString &content)
{
    PlayerLogAnalysis result;
    result.kind = PlayerLogAnalysis::NoCrash;

    // 内存溢出：Unity 抛出 OutOfMemoryException，或日志中出现内存不足描述。
    // 这种异常会导致游戏直接崩溃，优先于原生信号处理。
    if (content.contains(QStringLiteral("OutOfMemoryException"))
        || content.contains(QStringLiteral("Out of memory"), Qt::CaseInsensitive)) {
        result.kind = PlayerLogAnalysis::OutOfMemory;
    } else {
        // 原生崩溃段落格式：Caught fatal signal - signo:11 code:1 errno:0 addr:(nil)
        // 崩溃痕迹总是写在日志末尾，故只需在尾段匹配。
        static const QRegularExpression re(
            QStringLiteral("Caught fatal signal[^\\r\\n]*signo:(\\d+)"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = re.match(content);
        if (match.hasMatch()) {
            result.kind = PlayerLogAnalysis::HardCrash;
            result.signo = match.captured(1).toInt();
        }
    }

    if (result.kind == PlayerLogAnalysis::HardCrash
        || result.kind == PlayerLogAnalysis::OutOfMemory) {
        result.context = extractPlayerLogContext(content);
    }
    return result;
}

QString extractPlayerLogContext(const QString &content)
{
    const PlayerLogAnalysis analysis = analyzePlayerLog(content);
    if (analysis.kind != PlayerLogAnalysis::HardCrash
        && analysis.kind != PlayerLogAnalysis::OutOfMemory)
        return QString();

    const int errOff = locateErrorOffset(content, analysis.kind);
    if (errOff < 0)
        return QString();

    // 从关键错误所在行往前回溯 kContextLeadingLines 行作为显示起点；
    // 不足该行长时从 content 开头开始。
    int start = errOff;
    // 先对齐到关键错误所在行的行首
    {
        const int nl = content.lastIndexOf('\n', start - 1);
        start = (nl >= 0) ? nl + 1 : 0;
    }
    // 再向上跨越 kContextLeadingLines 条换行（每条换行对应上一行行首偏移）。
    // 注意：start 位于行首时，紧邻其前的 '\n' 是上一行的结尾，需从 start-2 起往前查找
    // 上上一行的换行，才能正确走到上一行的行首，避免原地踏步。
    int moved = 0;
    while (moved < playerlog::kContextLeadingLines && start > 0) {
        const int nl = content.lastIndexOf('\n', start - 2);
        if (nl < 0) {
            start = 0;
            break;
        }
        start = nl + 1;
        ++moved;
    }

    // 截取 [start, content末尾]。EOF 方向上会包含完整日志上下文 + 崩溃段。
    return content.mid(start);
}

PlayerLogAnalysis analyzePlayerLogFile(const QString &logPath, qint64 maxTailBytes)
{
    PlayerLogAnalysis result;
    QFile f(logPath);
    if (!f.open(QIODevice::ReadOnly)) {
        result.kind = PlayerLogAnalysis::LogMissing;
        return result;
    }

    const qint64 fileSize = f.size();
    qint64 readSize = fileSize;
    if (maxTailBytes > 0 && fileSize > maxTailBytes) {
        readSize = maxTailBytes;
        f.seek(fileSize - readSize);
    }
    const QByteArray bytes = f.read(readSize);
    f.close();

    return analyzePlayerLog(QString::fromUtf8(bytes));
}

QString defaultKspPlayerLogPath()
{
#if defined(_WIN32)
    return QDir(QDir::homePath()).filePath(
        QStringLiteral("AppData/LocalLow/Squad/Kerbal Space Program/Player.log"));
#else
    // 非 Windows 平台 KSP 日志位置不固定，由调用方显式传入；这里返回空表示"未知/不支持"。
    return QString();
#endif
}

QString describeSigno(int signo)
{
    switch (signo) {
    case 11: // SIGSEGV
        return QStringLiteral("段错误（SIGSEGV）：程序试图访问不允许访问的内存区域，是 KSP 最常见的硬崩溃类型。"
                              "多由模组冲突、损坏的模组或显卡驱动问题引发。");
    case 6:  // SIGABRT
        return QStringLiteral("异常终止（SIGABRT）：程序自检到严重错误（如断言失败）而主动崩溃。"
                              "通常是某个模组的数据或代码触发了内部错误。");
    case 4:  // SIGILL
        return QStringLiteral("非法指令（SIGILL）：程序执行了非法 CPU 指令，多由损坏的程序文件或驱动异常引起。");
    case 5:  // SIGTRAP
        return QStringLiteral("陷阱/断点（SIGTRAP）：通常由调试断点或断言触发，可能与个别模组有关。");
    case 7:  // SIGBUS
        return QStringLiteral("总线错误（SIGBUS）：内存对齐等硬件层的非对称访问错误，可能与内存/驱动异常有关。");
    case 8:  // SIGFPE
        return QStringLiteral("浮点异常（SIGFPE）：发生除零或非法浮点运算，常由模组的计算逻辑错误引起。");
    default:
        return QStringLiteral("未知的硬崩溃信号（signo:%1）。").arg(signo);
    }
}

} // namespace playerlog