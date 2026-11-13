# Release Notes

> **Long-term support (LTS) notice:** Hello KSP Launcher has reached a stable, feature-complete state. Due to the author's limited time and energy, **this project will not be updated for a long period of time**. v1.4.3 is the **long-term support release** — it is the last planned release for the foreseeable future, so please keep this installer. Issue reports are still welcome, but fixes may take a very long time.

## v1.4.3 — LTS Release: Smarter Crash Diagnostics, Always-On Crash Report & Icon Refresh (2026-11-13)

This release is centered on crash diagnostics: the launcher now recognizes out-of-memory crashes that happen while the game is loading assets, always shows the crash report even when no known cause can be identified, and fixes a stack overflow in the log analyzer that could crash the launcher itself on certain crash logs. It also refreshes several action icons and reorganizes the internal instance-management code.

### Crash Diagnosis

- **Crash report is now always shown** (`playerloganalyzer.{h,cpp}`, `mainwindow.{h,cpp}`, `pages/settingspage.cpp`) — previously the analysis dialog only appeared when the game exited with a non-zero code. Now the report appears whenever the exit code is non-zero **or** the log contains crash traces, and when the analysis finds no known cause it still opens, titled "游戏异常退出（未识别出明确原因）" with the exit code and the **last 40 lines of the log** as context. Only a clean exit (code 0, no crash traces) stays silent. A missing `Player.log` is also reported, with likely reasons. The separate "游戏进程发生错误" popup was removed — launch failures are still reported by the launch path — and the "打开日志文件 / 保存日志" buttons are only offered when the log actually exists.
- **Native out-of-memory during asset loading** (`playerloganalyzer.{h,cpp}`, `mainwindow.cpp`, `pages/settingspage.cpp`) — a new crash kind, `NativeLoadOutOfMemory`, covers logs that end right after a `Load ...` line with a Unity native crash dump (`Crash!!!` followed by `OUTPUTTING STACK TRACE`) and no managed exception beforehand — texture/model decompression and upload run in engine-native code, outside the Mono stack. The dialog shows the last log line before the crash (e.g. `Load MBM texture: KFC/parts/...`), explains the native path, and gives separate advice for "not enough memory" and "corrupt mod asset". If any of the three conditions does not hold, detection falls back to the existing OOM / fatal-signal paths.
- **Stack overflow in log analysis fixed** (`playerloganalyzer.cpp`) — `analyzePlayerLog` and `extractPlayerLogContext` called each other recursively, so any log containing `Caught fatal signal` or OOM markers overflowed the stack and crashed the launcher precisely when the crash dialog was supposed to help. Context extraction now goes through an internal helper that takes the already-detected kind; public signatures are unchanged.

### Interface

- **New action icons** (`gen_icons.py`, `resources.qrc`, `pages/instancedetailpage.cpp`, `pages/modstabpage.cpp`, `pages/savedetailpage.cpp`) — added and registered `export.svg` (bent share arrow) for "export modpack" and `kerbal.svg` (Kerbal silhouette) for "manage Kerbals"; "import modpack" and "import mod" now use the download icon, "save info" uses the list icon, and the compatibility-version button is text-only.

### Internal

- **`InstanceManager` split by domain** (`src/game/`) — the ~2300-line god class that handled processes, saves, Kerbals, ships, backups, modpacks, key bindings, game settings and DLC was split into `GameProcessManager`, `GameSettingsManager` (+ `settingskeymap`), `SaveManager`, `ShipManager`, `BackupManager` and `ModpackExporter`, with a shared `trash` helper replacing two copies of the Windows recycle-bin code. No behavior change; an unused `listMods` helper was removed.

### Tech Stack

- **Framework**: Qt 6.12 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; MinGW (Qt-bundled); Inno Setup 7 installer
- **Testing**: `test_libckan` + `test_launcher` (all passed)
