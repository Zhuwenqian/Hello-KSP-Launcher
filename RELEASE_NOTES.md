# Release Notes

## v1.2.3 — KSP Crash Log Analysis & Modpack Metadata Validation (2026-09-11)

This release hardens two workflows around failure detection and shareability. When the game exits abnormally (a hard crash, not a user-initiated stop), the launcher now automatically reads the tail of KSP's `Player.log`, detects hard crashes or out-of-memory conditions, and explains what happened with targeted advice. Exporting a modpack now writes a `hkspl_package.json` metadata file so imports can be validated up front; mismatched or damaged metadata is rejected gracefully instead of being silently accepted.

### Game Crash Log Analysis

When a game **exits abnormally** (not a user "stop", non-zero exit code), the launcher reads the tail of KSP's `Player.log` and detects a hard crash or an `OutOfMemoryException`; on a match it pops up the reason with suggestions.

- **Analysis module** (`playerloganalyzer.{h,cpp}`) — `analyzePlayerLog` (pure string parsing, unit-testable), `analyzePlayerLogFile` (reads only the last 512 KB of the file to avoid stalling on logs that can grow to hundreds of MB ~ GB over time), `defaultKspPlayerLogPath` (Windows: `%USERPROFILE%/AppData/LocalLow/Squad/Kerbal Space Program/Player.log`), and `describeSigno`.
  - Detection priority: `OutOfMemoryException` / `Out of memory` first; otherwise the native crash block is matched via `Caught fatal signal[^\r\n]*signo:(\d+)`.
  - Full Chinese signo mapping: 11=SIGSEGV segment violation, 6=SIGABRT abnormal termination, 4=SIGILL, 5=SIGTRAP, 7=SIGBUS, 8=SIGFPE; unknown signals get a generic hint.
- **Trigger & dialog** (`mainwindow.cpp` `onGameFinished` / `maybeShowCrashAnalysis`) — on abnormal exit with analysis enabled, shows a `QMessageBox` whose title distinguishes OOM from hard crash; the body includes the signal explanation and targeted advice (for OOM: trim mods, raise memory limit, lower texture quality, close background apps, use lower-resolution planet-pack textures). A "open log file" button opens the folder containing `Player.log`. Missing log / no crash / unsupported platform stay silent.
- **Setting** (`configmanager.{h,cpp}`, `settingspage.{h,cpp}`) — a "game crash log analysis" switch (default on) in the general group, persisted as `crashLogAnalysis` in `HKSPL.json`; missing key falls back to on.
- **Tests** — `test_launcher` adds `TestPlayerLogAnalyzer`: clean/empty log → NoCrash, signo 11/6/8 → HardCrash, OOM → OutOfMemory (and wins over signal), missing file → LogMissing, tail-of-large-file hit on end-of-log crash, and signo descriptions containing key Chinese. Both `ctest` suites pass.

### Modpack Import/Export Metadata Validation

Exporting a modpack now writes `hkspl_package.json` at the zip root (same level as `GameData`) recording launcher version, game version (precision down to Patch, e.g. `1.12.5`), modpack name, and description. Imports read and validate it first, pop up the info for confirmation, and reject at version mismatch.

- **Export dialog** (`modpackcontroller.cpp` `exportAsZip`) — the `QFileDialog::getSaveFileName` becomes a custom `QDialog` with "file name", "description", and "save path + browse" (via `QFileDialog::getExistingDirectory`). A name ending in `.zip` has its suffix de-duplicated.
- **Metadata write** (`instancemanager_modpack.cpp` `exportModpack`) — signature gains `packageMetaJson` (defaults empty for compatibility); when non-empty it is written to the zip root first using `ckan::kModpackMetaFileName`.
- **Metadata read & validate** (`modpackio.{h,cpp}`) — `modpackReadPackageMeta` returns `NotFound/ReadError/Ok`; `modpackVersionCompatible` treats equal major+minor as compatible (patch irrelevant). `kModpackMetaFileName` is the single constant for both export and import file names.
- **Import flow** (`modpackcontroller.cpp` `importFromZip`) — pick zip → validate GameData → read metadata; missing/corrupt/no-valid-`gameVersion` is rejected outright; a valid current instance version that differs in major or minor is rejected (hint only, no info dialog); on pass, a metadata info dialog (name / game version / launcher version / description + a clear-GameData warning) replaces the former confirmation, and "install" starts the import. Launcher version is shown but never validated.
- **Tests** — `test_libckan` adds `readPackageMetaOk`, `readPackageMetaNotFound`, and `versionCompatibleCheck`. Both `ctest` suites pass.

### Shutdown Cleanup: No More Orphan Processes or Stuck Registry Locks

Fixed a bug where closing the launcher while the index was downloading (e.g. right after entering Mod Management) left a background process running: the window closed but the process kept downloading, and because `CKAN/registry.locked` held a live PID, reopening the launcher and entering Mod Management kept reporting the instance as locked with a 10 s poll loop.

- **Root cause** — nobody cancelled in-flight background tasks on exit. `~CKanManager()` (the only place that sets the cancel flag and waits on the watchers) is a static destructor, and its order relative to the global `QThreadPool` destructor's `waitForDone()` (which waits indefinitely for in-flight `QtConcurrent` tasks) was not guaranteed; in practice the pool waited first → the cancel flag was never set → index downloads ran to completion (mirror × resume-retry could drag on) → the process lingered and the lock was never released.
- **Fix (`main.cpp`)** — connect `QCoreApplication::aboutToQuit` (while `QApplication` is still alive) → `CKanManager::instance().closeInstance()`: set the cancel flag + `cancelInstall()` + `waitForFinished()` for all background tasks (downloads poll for cancellation every 200 ms) + close the current instance and release the registry lock. Cleanup can no longer be left to static destruction.
- **Fix (`modpackcontroller.cpp` `importFromZip`)** — the modpack import task is not managed by `CKanManager` (its watcher is owned by the controller) and was only cancellable via the progress dialog's `wasCanceled`; once the dialog was destroyed with the window, nobody cancelled it. Now the dialog's `destroyed` signal sets `cancelRequested` (polled by `modpackImportGameData`), so the background extraction aborts promptly.
- **Exit-safety audit of remaining async operations** (unchanged): mod search / reverse relationships (in-memory index traversal, bounded CPU, sub-second), backup/restore (slot-level synchronous `waitForFinished`, naturally safe at exit), icon loading (fast, bounded), version check/update (main-thread `QNetworkAccessManager`, aborted automatically when `QApplication` is destroyed).
- **Tests** — `test_launcher` adds `TestShutdownCleanup`: `closeInstance()` while a background scan is in flight does not hang and releases `registry.locked`; it is idempotent; safe with no bound instance. Verified live: after closing the window the process exits cleanly within 3 s (ExitCode=0). Both `ctest` suites pass.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)

- **Language**: C++17

- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain

- **Testing**: `test_libckan` + `test_launcher` (all passed)