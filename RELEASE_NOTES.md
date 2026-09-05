# Release Notes

> **重要提示 / Important Note:** 本 Release 含有更新器（`updater.exe`）的更新。如果您正在使用早期的 v1.2.0 版本，由于其自带的老版更新器无法获取到自身的新二进制，自动更新可能无法替换本组件——届时请手动替换 `updater.exe`。
>
> This release ships an updated built-in updater (`updater.exe`). If you are still on the older v1.2.0, its built-in updater cannot fetch its own new binary, so the auto-update may fail to replace this component — please replace `updater.exe` manually in that case.

## v1.2.1 — Instance Icons, Stability & Architecture Refactor (2026-09-05)

A maintenance-and-polish release focused on robustness and a large internal refactor: per-instance themed icons and Beyond Home auto-naming, guarded cross-process registry access with atomic persistence, SHA256 verifiable self-updates straight from GitHub's API, cancellable batch uninstalls with atomic rollback, and a six-step architecture refactor that cleanly splits the monolith without any UI or behavior change.

### Instance List Icons & Beyond Home Auto-Naming

- **Per-instance icon** (`instanceitemwidget.cpp`, `instanceiconmanager.cpp`) — each instance now shows a fixed 40×40 icon to the left of its name, next to the selection checkbox.

- **Source resolution by name suffix** (case-insensitive `resolveSource`): names containing `RP-1` use `resources/instanceicons/RP-1.png` (highest priority); names containing `RSS` or `Sol` (Sol being RSS's super-beautified fork) use `RSS.png`; everything else (vanilla, RO, etc.) reads the icon embedded in the KSP executable.

- **Executable icon extraction** (Windows only) — `SHGetFileInfo` + `SHGetImageList` take the largest available frame (256 → 48 → 32 via `SHIL_JUMBO/EXTRALARGE/LARGE`), painted to a DIB and turned into a `QImage`. Non-Windows or extraction failure leaves the slot blank.

- **Performance** — large bitmaps (RSS ≈ 10 MB, RP-1 ≈ 700 KB) and exe icons are loaded/extracted on background threads (`QtConcurrent`) and cached, so the list never stutters on refresh; loaded images are scaled to ~80 px.

- **Fixes** — exe extraction failed on config forward-slash paths (now normalized via `QDir::toNativeSeparators`), and parallel extractions only let the first succeed (the Shell image list is not thread-safe; `QMutex` now serializes Shell calls). RSS/RP-1/BeyondHome icons are compiled into the exe through `resources/resources.qrc`.

- **Beyond Home auto-naming** (`gameinstance.cpp` `detectInstallKindTags`) — detecting a `GameData/BeyondHome` folder (case-insensitive; a planet pack like RSS/Sol, not coexisting with them) appends a `Beyond Home` tag to the suggested instance name (e.g. `KSP 1.12.5 Beyond Home`), shared by manual add and Steam discovery; only affects newly added instances.

### Registry Thread-Safety & Cross-Process Locking

- **Thread-safe registry** (`registry.{h,cpp}`) — a `QRecursiveMutex` (shared_ptr-shared, copy-safe) guards all in-memory access (`loadFromJson`/`toJson`/`clear`/`registerModule`/`unregisterModule` and every direct reader in `gameinstance`/`ckan`/`moduleinstaller`/`relationshipresolver`/`manualGameDataFolders`), so concurrent scans and installs can no longer race.

- **Atomic persistence** (`gameinstance.cpp` `saveRegistry`) — `registry.json` is written atomically with `QSaveFile` (temp file + rename), so a crash/power loss cannot corrupt it; serialization under lock and disk write happen inside one critical section.

- **Cross-process lock** (`engageRegistryLock`/`acquireRegistryLock`, `FileLock` on `registry.locked`) — when another process (official CKAN or a second launcher) holds the lock, this process refuses writes and returns false, preventing concurrent corruption of `registry.json`.

- **UI gating** (mods page) — the mods page acquires the lock before loading the index; if the lock is held it shows a "registry is locked, please close other launcher instances or official CKAN" dialog, clears the table and disables the mod/refresh buttons, then polls `registry.locked` every 10 s and auto-recovers with the index once the lock is released. Polling runs only while the mods page is active.

### Update Integrity via GitHub SHA256 Digest

- **Direct authoritative digest** (`updatemanager.cpp`) — since GitHub exposes a SHA256 digest for every release asset (`assets[].digest`, i.e. `sha256:<64hex>` — the same value as the "Copy SHA256" button on the release page), `parseRelease` extracts it per asset name via `digestHexFromApi`. No need for the publisher to upload a `.sha256` file.

- **Verify-on-download** (`verifyAndFinish`) — the local SHA256 is computed right after the ZIP download and compared against the digest before finishing; this drops the extra network request for a separate checksum file. Missing/malformed digests or hash mismatches clean up the temp pack and abort with an `updateError`.

- **Removal** — `.sha256` asset handling (`checksumUrl`, `fetchChecksumAndVerify`, `parseSha256FromChecksum`) and its tests were deleted.

### Cancellable Batch Uninstall with Atomic Rollback

- **Root-cause fix** — a temporary `ModuleInstaller` in `CKan::uninstall` couldn't be reached by `cancelInstall()`, and the uninstall loop never checked the cancel flag, so uninstall couldn't be cancelled. Operations now share a single installer (`ensureInstaller()`), and `CKan::uninstallMany` resets the cancel flag at entry so cancel truly aborts in-flight uninstalls.

- **Batch atomicity** (`ModuleInstaller::uninstallMany`) — all targets share one transaction and one uninstall order (dependencies released outside-in); any failure or cancellation rolls back the transaction and restores the registry snapshot (recovering deleted files), and `saveRegistry` + `commit` happen once only on full success. Single-module uninstall reuses this path.

- **Cancel restores everything** — each uninstall loop iteration adds a cancel checkpoint; on cancel the whole program/files/registry state rolls back to pre-uninstall, returning `InstallResult.cancelled=true`, with the UI showing "已取消卸载，已恢复原状" (treated as success and refreshed, no install-history write).

- **UI** (mods page) — an indeterminate progress bar with "正在卸载：{mod}（共 N 个）" plus a cancel button during uninstall; the confirmation dialog shows the cascade-dependency count derived from `CKan::uninstallPlan` (shared read-only logic with the real uninstall), so what's promised matches what happens.

- **Upgrade path unaffected** — `installFromCache` still calls single-module uninstall inside an external transaction, with rollback handled by the caller; the cancel flag is reset during the download phase.

### Performance Optimizations

- **No per-cell deep copies** (`modtablemodel`) — `data()`/`statusAt()`/`filterAcceptsRow()` previously deep-copied a whole `CkanModule` per cell/role; a non-copying `modulePtr(row)` now returns a reference pointer on all hot paths (the `moduleAt` value API is kept for external callers).

- **Faster search** (`CKan::search`) — instead of copying and sorting the full version table per identifier, `latestFor`→`versionsFor` became a single linear scan for the latest version, skipping deep copies and sorting, with `reserve` to cut reallocation.

- **Async table population** — `maybePopulateMods` moved to the background via `QtConcurrent::run` + `QFutureWatcher<QVector<CkanModule>>`, completing with `onModsLoadFinished` back on the main thread; consecutive reloads keep only the latest load, and the refresh-count hint is set after completion.

### Launch / Stop Stability Fixes

- **Non-blocking stop** (`instancemanager.cpp`) — `waitForFinished(3000)` after `terminate()` froze the UI for up to 3 s; a single-shot `QTimer` now hard-`kill`s on timeout, with memory-job release and UI reset driven by the `finished` signal. A normal exit stops the timer so there's no mis-kill.

- **Deferred pid read** (`launchGame`/`applyGameOptions`) — `state()`/`processId()` may not be valid right after `start()`, so previously a pid of 0 silently skipped priority/memory limits. Target options are staged now and applied on `QProcess::started`, once the pid is real.

### Control-Flow Type-Safety

- **Index refresh status** — the `"已取消"` magic string was replaced by a typed `enum class IndexRefreshStatus { Success, Failed, Cancelled }`, and the `indexRefreshed` signal signature became `(IndexRefreshStatus, QString error)`; cancellation is decided by the `m_indexCancelRequested` flag, never by string comparison.

- **Folder-conflict choice** — the `{"__CANCEL__"}` placeholder sentinel became a typed `FolderConflictChoice` (`action ∈ {OverwriteAll, DeleteOld, Cancel}` + `foldersToDelete`), eliminating sentinel-string control flow.

### Internal Architecture Refactor (Six Steps, Behavior-Preserving)

- **Dialogs out of the business layer** (`src/moddecision.{h,cpp}`) — the conflict/suggestion/provider/disk-space/confirm dialogs once embedded in `ckanmanager.cpp` are now an injectable `moddecision::Hooks` callback set; `CKanManager` injects the real Qt implementation by default and exposes `setModDecisions()` for tests or `ModsController`. `resolveAndInstall`/`importAsync` only call the hooks — the business layer no longer depends on widgets.

- **Read-only service layer** (`src/services/`) — `IndexService` (index queries) and `ScanService` (unmanaged DLL scans) were split out; then `CacheService` (cache dir + precise cleanup + pure `knownCacheFileNames`), `ModpackService` (CKAN export + install-history snapshot), `UninstallService` (cascade plan + installed-filter), and `InstallService` (the synchronous pre-install decisions) were added. All services are injected with an instance pointer via `setCkan`, bound/unbound together with `openInstance`/`closeInstance`.

- **UI orchestration split** — a thin `ModsController` (holding the real Qt `moddecision::Hooks`, injected at construction) sits behind the brand-new `ModsTabPage` (search/status/tag filters, mod table, detail tabs, progress bars, action buttons, single-module import, install history). `GameSettingsTabPage`, `DlcTabPage`, `AdvancedTabPage` and a `ModpackController` (export/import flows) took over the remaining `InstanceDetailPage` tabs, reducing it to a shell of sidebar navigation, content assembly, entry points and signal wiring.

- **Facade slimming & dead-code removal** — four zero-reference facade passthroughs were deleted (`hasInstance`, `isInstalling`, `allIdentifiers`, `indexSize`); instance binding, index queries, and install/uninstall/upgrade operations remain. External behavior and signal semantics are unchanged; both test suites and the full build pass.

### Build & Tooling

- **Version** — bumped from 1.2.0 to 1.2.1 via `sync_version.py` (CMake `project(... VERSION ...)` and `src/appversion.h`).

- **Tests** — `test_launcher` gained `TestInstanceIconSource` (suffix resolution, RP-1 over RSS, Sol → RSS, case-insensitivity, RO → exe fallback), `TestUpdaterManager::digestHex`, `TestCacheService.knownCacheFileNames`, `TestInstallService.nullInstanceFails` and `TestIndexService.isNewerVersion`; `test_libckan` gained `uninstallCancelledRestoresEverything` and `uninstallPlanMatchesBatch`. All suites pass.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)

- **Language**: C++17

- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain

- **Testing**: `test_libckan` + `test_launcher` (all passed)