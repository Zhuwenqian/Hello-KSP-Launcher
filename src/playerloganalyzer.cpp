#include "playerloganalyzer.h"

#include <QFile>
#include <QDir>
#include <QRegularExpression>

namespace playerlog {

// 只保留尾部一段，避免对可能数百 MB 的 Player.log 做全量读取。
static const qint64 kTailBytes = 512 * 1024;

namespace {

// Unity 原生崩溃转储段：日志里出现 "Crash!!!" 且其后跟着 "OUTPUTTING STACK TRACE" 堆栈输出。
// 返回崩溃段起始偏移；不是原生崩溃转储时返回 -1。
int locateCrashDumpOffset(const QString &content)
{
    const int off = content.lastIndexOf(QStringLiteral("Crash!!!"));
    if (off < 0)
        return -1;
    return content.indexOf(QStringLiteral("OUTPUTTING STACK TRACE"), off) >= 0 ? off : -1;
}

// 取 off 之前最后一条有意义的日志行（跳过空行与 Unity 的 "(Filename: ...)" 定位行）。
static QString lastMeaningfulLogLineBefore(const QString &content, int off)
{
    const QStringList lines = content.left(off).split(QLatin1Char('\n'));
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QStringLiteral("(Filename:")))
            continue; // Unity 日志行尾附带的代码定位行，不算实际日志内容
        return line;
    }
    return QString();
}

// 加载日志行：KSP / Unity 在加载贴图、模型等资源时会写 "Load MBM texture: xxx"、
// "Loading texture ..."、"LoadTexture ..." 之类的行。崩溃紧跟在这类行之后是本类崩溃的前提。
static bool isLoadLogLine(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("^\\s*load\\w*\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(line).hasMatch();
}

// 崩溃前最近 kExceptionScanLines 行内是否出现 Mono 端抛出的托管异常。
// 本类原生内存溢出不会在 Mono 栈上抛异常，所以一旦前面有异常，就不能判定为原生溢出
// （更可能是模组代码/资源本身的问题）。
static bool hasManagedExceptionBefore(const QString &content, int off)
{
    constexpr int kExceptionScanLines = 40;
    const QStringList lines = content.left(off).split(QLatin1Char('\n'));
    int scanned = 0;
    for (int i = lines.size() - 1; i >= 0 && scanned < kExceptionScanLines; --i, ++scanned) {
        const QString &line = lines.at(i);
        if (line.contains(QStringLiteral("Exception"))
            || line.contains(QStringLiteral("Unhandled exception"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// 取 content 末尾最多 lineCount 行的文本，用于未识别出崩溃原因时展示最近的日志段落。
static QString lastLines(const QString &content, int lineCount)
{
    if (content.isEmpty() || lineCount <= 0)
        return QString();
    const QStringList lines = content.split(QLatin1Char('\n'));
    return lines.mid(qMax(0, lines.size() - lineCount)).join(QLatin1Char('\n'));
}

} // namespace

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
    if (kind == PlayerLogAnalysis::NativeLoadOutOfMemory)
        return locateCrashDumpOffset(content);
    if (kind == PlayerLogAnalysis::HardCrash) {
        static const QRegularExpression re(
            QStringLiteral("Caught fatal signal"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(content);
        return m.hasMatch() ? m.capturedStart() : -1;
    }
    return -1;
}

// 按已知 kind 提取上下文（内部辅助）：不重复判定类型，避免与 analyzePlayerLog 互相递归。
static QString extractPlayerLogContextFor(const QString &content, PlayerLogAnalysis::Kind kind);

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
        // 加载阶段的原生崩溃：Unity 原生崩溃转储（Crash!!! + 堆栈段）紧跟在 "Load xxx"
        // 加载日志行之后，且崩溃前 Mono 端没有抛托管异常。加载贴图/模型时的解压与上传走
        // 引擎原生代码，不经 Mono 栈，内存不足时游戏会直接闪退并在日志里留下这种转储。
        const int dumpOff = locateCrashDumpOffset(content);
        if (dumpOff >= 0) {
            const QString loadLine = lastMeaningfulLogLineBefore(content, dumpOff);
            if (!loadLine.isEmpty() && isLoadLogLine(loadLine)
                && !hasManagedExceptionBefore(content, dumpOff)) {
                result.kind = PlayerLogAnalysis::NativeLoadOutOfMemory;
                result.loadHint = loadLine;
            }
        }
        if (result.kind == PlayerLogAnalysis::NoCrash) {
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
    }

    if (result.kind == PlayerLogAnalysis::HardCrash
        || result.kind == PlayerLogAnalysis::OutOfMemory
        || result.kind == PlayerLogAnalysis::NativeLoadOutOfMemory) {
        result.context = extractPlayerLogContextFor(content, result.kind);
    } else {
        // 无法判定原因时，仍给弹窗提供最近的日志段落，便于用户/模组作者排查
        result.context = lastLines(content, playerlog::kContextLeadingLines);
    }
    return result;
}

QString extractPlayerLogContext(const QString &content)
{
    // 直接复用 analyzePlayerLog 的判定结果，避免两处检测规则各写一遍
    const PlayerLogAnalysis analysis = analyzePlayerLog(content);
    return extractPlayerLogContextFor(content, analysis.kind);
}

// 按已知 kind 提取上下文（内部辅助）：不重复判定类型，避免与 analyzePlayerLog 互相递归。
static QString extractPlayerLogContextFor(const QString &content, PlayerLogAnalysis::Kind kind)
{
    if (kind != PlayerLogAnalysis::HardCrash && kind != PlayerLogAnalysis::OutOfMemory
        && kind != PlayerLogAnalysis::NativeLoadOutOfMemory)
        return QString();

    const int errOff = locateErrorOffset(content, kind);
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