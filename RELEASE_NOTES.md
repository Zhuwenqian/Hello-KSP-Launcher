# Release Notes

## v1.2.0 — Launch Profiles, Self-Update & Custom Title Bar (2026-09-04)

A feature release delivering per-instance launch profiles (memory cap + process priority), a complete self-update pipeline backed by a standalone `updater.exe`, and a translucent theme-colored title bar — plus shared-DLL version disambiguation and a few UI touches.

### Instance Launch Configuration (Profile)

- **Per-instance launch profile** (`instancedetailpage_settings.cpp`, `configmanager.{h,cpp}`, `instancemanager.{h,cpp}`) — each instance's "Advanced" page now carries a full launch profile instead of a single argument string, bundling three settings persisted in `HKSPL.json` via new fields `launchMemoryMB` (default 0) and `launchHighPriority` (default false) on `KSPInstance`: custom launch arguments, system-level process memory cap, and process priority.

- **Memory cap** (`processopt.cpp`) — since KSP is a 64-bit Unity game with no Java-style `-Xmx`, a system-level limit is enforced: a **Job Object** with `JobMemoryLimit` is assigned to the game process on Windows (`openMemoryJob`/`closeMemoryJob`, released on exit/teardown), and `RLIMIT_AS` via `QProcess::setChildProcessModifier` on POSIX. Unit is MB; `0` means unlimited (UI cap 64 GB).

- **Process priority** — a "High / Low" dropdown: **High** terminates Edge/Chrome/Firefox browsers first (`taskkill /IM` on Windows, `pkill` on POSIX) then raises the game's priority (`SetPriorityClass(HIGH_PRIORITY_CLASS)` / `setpriority(-5)`); **Low** takes no action. The browser-kill warning is shown once on "Confirm Save", not on every page entry or at launch.

### Self-Update System (standalone updater.exe)

- **Architecture** — `UpdaterManager` queries GitHub Releases (`api.github.com/repos/Zhuwenqian/Hello-KSP-Launcher/releases/latest`), compares versions semver-style (`versionLess`: multi-segment numeric compare, lexicographic fallback for non-numeric segments), and if newer, downloads the x86\_64 ZIP to `<app dir>/.updater_update/`. `updateflow.cpp` orchestrates the dialogs; the standalone `updater.exe` performs the actual swap and relaunch.

- **Isolated run** (`updatemanager.cpp applyUpdate`) — `updater.exe` plus its runtime (Qt6Core + MinGW runtime DLLs) are copied to the system temp dir `HKSPL_updater_self` before launch so the updater never locks the installed binaries; the main process quits and the updater waits (`--wait-pid <pid>`) for it to exit before replacing files. Quit uses a normal event-loop exit plus a 3-second watchdog thread that hard-terminates (`std::_Exit`) if the loop can't drain, guaranteeing the old process always dies so the updater never hits locked/orphaned files.

- **Keep-list replacement** (`updater/main.cpp`) — only `ckan_cache`, `HKSPL.json`, `backups`, `updater(.exe)`, and `.updater_update` are preserved; everything else is deleted then extracted fresh from the pack (byte-identical, no stale files). ZIP entry paths are sanitized against path traversal, a single top-level release folder becomes the new root, and `updater.log` is written for diagnostics.

- **Update window guard** — `updater` writes a `.updating` marker on start and clears it before relaunching the new build; `main.cpp` detects the marker and exits with an "updating" notice, so a user double-clicking the launcher mid-update never starts a locked or half-replaced program.

- **UI** (`settingspage.cpp`) — new "Auto-check for updates on startup" toggle (`ConfigManager.autoCheckUpdate`, on by default; silent check at launch) and a manual "Check for Updates" button. The new-version prompt uses a custom dialog with a scrollable release-notes pane (`QPlainTextEdit`, 180–300 px) and pinned Update/Cancel buttons, so long notes never clip the buttons.

### Shared-DLL Disambiguation & Misc

- **Shared-DLL successor resolution** (`gameinstance.cpp`) — when the same DLL is reused across successor mods (BDArmory.dll by BDArmory, BDAc, BDAP), the installed identifier is now disambiguated by the actual detected KSP version: pick the first successor whose max supported version `>=` the game version, falling back to the newest successor when the version is unknown or exceeds all caps. Fixes false "conflicts with BDArmory" install errors when reinstalling BDAP.

- **Force AD rescan** (`ckanmanager.cpp`) — `scanUnmanagedDllsAsync` gained a `force` flag so the manual "Refresh repo" action also re-scans manually-installed (AD) mods instead of relying on the cached scan.

- **Homepage title removed** (`homepage.cpp`) — the redundant main title was dropped (the custom title bar already shows the app name).

- **About page release link** (`aboutpage.cpp`) — the first row of the About page is now clickable and jumps to the current version's GitHub Release page (`…/releases/tag/v{version}`), driven by the single `HKSPL_APP_VERSION` source.

### Custom Title Bar

- **Frameless translucent title bar** (`mainwindow.cpp setupTitleBar`) — the system chrome is replaced with a semi-transparent theme-colored bar (`rgba(28,28,31,200)` dark / `rgba(255,255,255,210)` light) layered over the background, with custom minimize/maximize(restore)/close buttons (`resources/icons/window-*.svg`, registered in `resources.qrc`).

- **Interactions** — drag to move (delegated to `QWindow::startSystemMove` past a threshold for Aero snap, with a drag threshold that avoids clashing with double-click maximize), double-click toggles maximize, `WM_NCHITTEST` edge resizing, and square corners when maximized / rounded otherwise (`DwmSetWindowAttribute` DWMWA\_WINDOW\_CORNER\_PREFERENCE; CMake links `dwmapi` on Windows).

### Build & Tooling

- **CMake** — new `add_executable(updater WIN32 …)` target compiling `src/updater/main.cpp` + miniz to `dist/`; links `dwmapi` for the corner preference API.

- **Tests** — `test_launcher` gained semantic-version-comparison and update-check-logic cases plus `TestLaunchOptions` (browser-kill commands cover all three browsers, high priority fails on invalid pid, memory Job returns null handle for invalid pid); `test_libckan` added shared-DLL disambiguation cases.

- **Version** — bumped from 1.1.2 to 1.2.0 via `sync_version.py`.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)

- **Language**: C++17

- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain

- **Testing**: `test_libckan` + `test_launcher` (all passed)

