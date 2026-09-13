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
    // 关键错误上下文：以关键错误（崩溃标记/OOM）所在行为基准，往前回溯 kContextLeadingLines
    // 行作为起点，一直到 content 末尾的文本。仅当 kind 为 HardCrash / OutOfMemory 时填充，
    // 其余情况为空。供弹窗的滚动文本框展示。
    QString context;
};

// 关键错误上下文向前回溯的行数（含关键错误所在行）。
constexpr int kContextLeadingLines = 40;

// 纯字符串解析：从日志内容中判定崩溃类型。content 通常是日志文件尾部一段。
PlayerLogAnalysis analyzePlayerLog(const QString &content);

// 返回关键错误上下文文本：定位关键错误（崩溃标记/OOM）所在行，往前回溯
// kContextLeadingLines 行作为起点，返回 [起点, content末尾] 的文本。
// content 中无关键错误时返回空字符串。纯字符串解析，供单元测试与弹窗展示。
QString extractPlayerLogContext(const QString &content);

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