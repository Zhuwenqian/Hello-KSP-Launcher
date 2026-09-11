#ifndef PLAYERLOGANALYZER_H
#define PLAYERLOGANALYZER_H

#include <QString>
#include <QtGlobal>

// KSP 崩溃日志分析器。
//
// 当游戏硬崩溃（Hard Crash，闪退出桌面）时，Unity 会在 Player.log 末尾写入一段
// "Caught fatal signal - signo:N" 段落；内存溢出（OutOfMemoryException）也会导致游戏
// 直接崩溃。本模块用于从 Player.log（可能很大）的尾部快速识别这些崩溃痕迹，并给出
// 中文释义，供启动器在游戏异常退出后弹窗提示用户。
//
// 设计为纯逻辑/极轻 IO：`analyzePlayerLog` 只做字符串解析（供单元测试），
// `analyzePlayerLogFile` 只读取文件末尾固定大小的一段，避免对整个可能数百 MB 的文件做全量读取。
namespace playerlog {

struct PlayerLogAnalysis {
    enum Kind {
        LogMissing, // 日志文件不存在/无法读取（或启动器不支持该平台日志路径）
        NoCrash,    // 日志尾部未发现崩溃痕迹
        HardCrash,  // 检测到 Unity 原生崩溃（Caught fatal signal - signo:N）
        OutOfMemory // 检测到内存溢出（OutOfMemoryException / Out of memory）
    };
    Kind kind = LogMissing;
    int signo = -1; // 仅当 kind == HardCrash 时有意义（见 describeSigno），否则为 -1
};

// 纯字符串解析：从日志内容中判定崩溃类型。content 通常是日志文件尾部一段。
PlayerLogAnalysis analyzePlayerLog(const QString &content);

// 读取日志文件末尾 maxTailBytes 字节后调用 analyzePlayerLog。
// 文件不存在或无法打开返回 { LogMissing }。maxTailBytes<=0 时退化为全量读取。
PlayerLogAnalysis analyzePlayerLogFile(const QString &logPath, qint64 maxTailBytes = 512 * 1024);

// KSP 默认 Player.log 路径：
//   Windows: %USERPROFILE%/AppData/LocalLow/Squad/Kerbal Space Program/Player.log
QString defaultKspPlayerLogPath();

// 返回 signo 对应的 Signals 标识与中文释义；未知 signo 返回通用提示。
QString describeSigno(int signo);

} // namespace playerlog

#endif // PLAYERLOGANALYZER_H