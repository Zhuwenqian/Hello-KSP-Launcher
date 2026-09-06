# Release Notes

## v1.2.2 — Smaller Exe, Faster Mod List & Better Mod Install UX (2026-09-06)

A polish-and-triage release focused on cold-start speed, search/filter performance, mod-installation decision dialogs, and cleanup of several niggling warnings. Default background and large instance icons leave the executable, the mod table gets row-level caching with debounced search, recommended mods now pop up for selection, a debug-log mode writes `HKSPL.log`, uninstall removes leftover folders, and a handful of false-conflict/icon/shutdown warnings are fixed. First-run language now defaults to English.

### Leaner Executable for Faster Cold Start

- **qrc slim-down** (`resources/resources.qrc`) — the default background `backgrounds/default.png` and the three large instance icons (`instanceicons/BeyondHome.png`, `RP-1.png`, `RSS.png`) are no longer compiled into the exe; only small SVG icons, QSS, and the .ico stay in. Roughly 2.7 MB of images leave the binary, dropping `HelloKSPLauncher.exe` to ~2.32 MB and lightening the qrc parse/decompress load at startup.

- **External-first loading** (`backgroundmanager.cpp`) — the default background is resolved in order: `resources/backgrounds/` next to the exe → `backgrounds/` → the source directory → qrc as a final fallback.

- **Instance icons** (`instanceiconmanager.cpp`) — candidate icon paths are expanded with exe-adjacent `resources/instanceicons/` and `instanceicons/`, again with qrc as fallback.

- **Publish copying** (`CMakeLists.txt`) — `file(COPY)` copies `resources/backgrounds` and `resources/instanceicons` into `dist/resources/`, so the shipped layout is unchanged.

### Faster Mod List Search & Filtering

Searches/filters/status checks on large indexes were a main-thread stall hotspot: `filterAcceptsRow`/`data()` re-did lowercase conversion, version-range parsing, and installed-status queries per row, per cell. This round trades precomputation for lookups, keeping behavior identical (with regression tests).

- **Per-row precomputed cache** (`modtablemodel.{h,cpp}`) — a new `RowCache` caches the merged lowercase searchable string, `compatibleCurrent`, `compatibleRange`, and `status`. `setModules` rebuilds the static cache in one pass; `refreshStatus` (after install/uninstall) and `setCompatibilityContext` (game version / extra range changes) rebuild only the affected fields. `statusAt`/rendering now use O(1) cache reads, dropping the costly `installedVersion`-plus-version-sort recomputation per cell.

- **Pre-parsed search tokens** (`ModsFilterProxyModel`) — `parseSearchTokens` turns the search text into a `SearchToken` list (plain keywords + `@author/@desc/@license/@depend/@provides/@tag`) once per `setSearchText`, reused across rows instead of re-splitting/re-lowercasing every keyword per row.

- **Filter reads the cache** — compatibility/status checks in `filterAcceptsRow` read `RowCache`; plain keywords hit the merged lowercase string with a single `contains`.

- **Input debounce** (`modstabpage.{h,cpp}`) — a non-empty search kicks a 150 ms single-shot timer; rapid typing collapses into one filter pass, refresh fires on pause, clearing restores the full list immediately.

- **Regression tests** — `test_launcher` gains `TestModsTableFilter` (4 cases: plain search, field search, cached status filter, compatibility cache rebuilt on context change); both `ctest` suites pass.

### Recommend-Module Selection Dialog (Aligned with Official CKAN)

Installing a module that recommends others no longer auto-installs them silently; a checkbox dialog now lets you pick (all selected by default, individually or fully deselected), matching the official CKAN install dialog.

- **Resolver collection** (`relationshipresolver.{h,cpp}`) — `resolve()` gains a `collectRecommends` flag (default off, unchanged behavior). When on, recommends are no longer auto-installed but collected into `ResolutionResult.recommendedModules` (cascading, deduped, cycle-guarded, skipping already-installed/selected/conflicting entries). The shared candidate lookup is factored into a `candidatesForRel` lambda reused by both `processRel` (auto-install) and the collection path, removing duplication.

- **Facade pass-through** (`ckan.{h,cpp}`) — `resolveInstallMany` passes `collectRecommends` through to the resolver.

- **Decision hooks** (`moddecision.{h,cpp}`) — the `Hooks` set gains `recommends` (sharing the same `OptionalModulesHandler` signature as Suggests); `askSuggests` is generalized to `askOptionalModules`, and a new `askRecommends` shows a "recommended modules" title defaulting to all-selected.

- **Install flow** (`installservice.{h,cpp}`) — `resolveInstallSet` gains `showRecommends`. When on, resolution runs in collect mode, the chosen recommends are merged back into the install set and re-resolved (their dependencies too), processed before the cascading Suggests popup so suggestions from the updated set surface together. When off, the official default (auto-install recommends) is kept.

- **Setting switch** (`configmanager`, `settingspage`) — new "show recommended modules during install" switch (`installRecommends`, default on); tooltip notes that off auto-installs them.

- **Tests** — `test_libckan` adds 6 cases (collection on/off, cascading collection, already-installed exclusion, cycle termination, conflict skip); both suites pass.

### Uninstall Now Removes Leftover Folders

- **Root cause** (`moduleinstaller.cpp` `uninstallMany`) — uninstall only walked registry entries, deleting files and `unregisterModule`-ing, never cleaning up mod-owned top-level GameData folders, leaving empty directories behind.

- **Fix** — during uninstall the top-level GameData folders written by the batch are collected; after files are deleted they are cleaned up transactionally — deleted only when no longer used by any other registered mod **and** the disk folder holds no remaining files (only empty sub-dirs). Shared folders and folders with manual/unregistered content are preserved; cleanup shares the same transaction so the whole batch rolls back together.

- **Test** — `test_libckan` adds `uninstallRemovesEmptyFolder` (exclusive empty folder deleted, shared folder kept, folder with manual files kept, shared dir emptied when the last owner is uninstalled); `ctest` passes both suites.

### Debug Mode — Optional Run Log to HKSPL.log

A "debug mode" switch in the settings-general group (`ConfigManager.debugMode`, default off). When enabled, from the **next launch** run logs are written to `HKSPL.log` next to the launcher; disabling stops new writes without deleting existing logs.

- **Log module** (`debuglogger.{h,cpp}`) — a `DebugLogger` singleton. `main.cpp` decides once at startup from the persisted `debugMode` (so toggling on applies on the second launch onward); when on, `qInstallMessageHandler` installs a global handler routing all `qInfo/qDebug/qWarning/qCritical` into `HKSPL.log`. Each startup clears and rewrites the file; every line is flushed immediately in the format `timestamp [level] [thread id]` (thread id = last 16 hex digits of the thread id), guarded by a thread-safe mutex.

- **Config** (`configmanager.{h,cpp}`) — adds `debugMode` getter/setter (default `false`) persisted in `HKSPL.json`; falls back to `false` when absent.

- **Instrumentation** — download start/retry/success-failure (byte counts) in `downloader.cpp`; recommended/suggested counts, install-decision counts, and pre-install-uninstall counts in `installservice.cpp`/`moddecision.cpp`; launch args/memory/priority and game stop in `instancemanager.cpp`; Steam-detected KSP paths in `steamdiscovery.cpp`.

### Bug Fixes & Cleanup

- **False "folder conflict" on every install** (`ckanmanager.cpp`) — the conflict dialog was called unconditionally after download even when `r.conflicts` was empty, popping an empty dialog. It now opens only when there are real manually-occupied conflicting folders; genuine conflicts still list each `GameData/...` folder.

- **Warnings removed** —
  - **Missing icons** (`modstabpage.cpp`, `resources/icons/download.svg`, `resources/resources.qrc`) — `x.svg` and `download.svg` never existed. The cancel button now reuses the existing `window-close.svg` (itself a double-line X), and the two "download package" buttons' `download.svg` was recreated in the lucide style (download arrow + container tray) and registered in the qrc. Result: no more `[WARN]` about unresolvable `:/icons/...` on the mods page.
  - **Shutdown `applicationDirPath` warning** (`configmanager.{h,cpp}`) — `ConfigManager` is a function-local static singleton whose destructor outlives `QApplication`; its destructor called `save()`→`getConfigPath()`→`applicationDirPath()` while `qApp` was null. Every write path (setters, `addInstance`, ...) already calls `save()` immediately, so that destructor flush was a redundant safety net — it was removed. `getConfigPath()` still calls `applicationDirPath()` live, so read/write behavior is unchanged and no longer depends on construction order.

- **Default language** (`configmanager.cpp` `loadDefaults`) — with no `HKSPL.json`, `language` now defaults to `en_US` (first launch is English, for a global audience); the fallback for an existing config missing the `language` key is `en_US` too.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)

- **Language**: C++17

- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain

- **Testing**: `test_libckan` + `test_launcher` (all passed)