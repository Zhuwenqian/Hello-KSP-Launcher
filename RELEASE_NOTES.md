# Release Notes
> **重要提示 / Important Note:** 本 Release 含有更新器（`updater.exe`）的更新。如果您正在使用早期的 v1.2.0 系列版本，由于其自带的老版更新器无法获取到自身的新二进制，自动更新可能无法替换本组件——届时请手动替换 `updater.exe`。
>
> This release ships an updated built-in updater (`updater.exe`). If you are still on the older v1.2.0 series, its built-in updater cannot fetch its own new binary, so the auto-update may fail to replace this component — please replace `updater.exe` manually in that case.

## v1.4.2 — Graphics Backend, PRE Toggle, Volume Sliders & Safer Mod Operations (2026-10-03)

This release adds a per-instance graphics backend selector and a temporary PhysicsRangeExtender disable toggle to the advanced page, turns game volume entries into sliders, locks mod operations against concurrent runs, and fixes a UI freeze when browsing large cached packages, an updater failure path that could wipe the install directory, download resume corruption, and slow background resizing.

### Launch Configuration

- **Graphics backend selector (experimental)** (`configmanager.{h,cpp}`, `advancedtabpage.{h,cpp}`, `mainwindow.cpp`) — a per-instance dropdown above the custom launch-args box. Options follow the detected game version: pre-1.8 defaults DirectX 9 (no argument) with OpenGL (`-force-opengl`) and DirectX 11 (`-force-d3d11`) offered; 1.8+ defaults DirectX 11 with OpenGL and DirectX 12 (`-force-d3d12`) offered. The six reserved `-force-*` renderer arguments are stripped from custom launch args on save and again at launch, so stale configs can never leak into the game. The choice persists per instance (`graphicsBackend` in `HKSPL.json`) and is applied even when launching without visiting the advanced page; version-mismatched picks silently fall back to the default renderer. When a backend was explicitly selected, the crash-analysis dialog now shows "Graphics backend used for this launch: OpenGL (-force-opengl)" as its first line, ready to paste into mod-author bug reports.
- **Temporary PhysicsRangeExtender disable** (`instancemanager.{h,cpp}`, `advancedtabpage.{h,cpp}`) — a toggle below process priority (visible only when `GameData\PhysicsRangeExtender\Plugins\PhysicsRangeExtender.dll` exists) renames the DLL to `.disabled` immediately (no "confirm" needed) and restores it automatically when the game exits — including force-kill and launcher shutdown. The switch reflects on-disk state: a leftover `.disabled` from a previous run shows as enabled and can be restored by hand; a failed rename (DLL in use) snaps the toggle back with a warning.

### Game Settings

- **Volume sliders** (`instancemanager_keymap.{h,cpp}`, `instancemanager_settings.cpp`, `gamesettingstabpage.cpp`, `dark.qss` / `light.qss`) — master/ship/ambience/music/UI/voice volume entries render as sliders (0–1, step 0.01) with a two-decimal numeric readout on the right that updates live while dragging. The slider flag travels with the keymap data instead of hard-coded key names; unparseable values still fall back to the plain text editor, and saving goes through the existing path (the value is read from a `Qt::UserRole` so the number never ghosts through the transparent row).

### Mod Management

- **Operation locking** (`modstabpage.{h,cpp}`) — install/upgrade/uninstall (and import) buttons stay disabled for the entire duration of any mod operation and until `operationFinished` (success, failure, or cancel). Previously, changing the selection, toggling checkboxes, select-all, or a background index/DLL-scan completing would recompute button state and re-enable the action buttons mid-operation, allowing a second concurrent write. All entry points are covered — including version-history install and automatic `.ckan` modpack batch installs, which previously never locked at all. Cancel remains available and unlocks through the same path.
- **Files tab no longer freezes on big packages** (`modstabpage.{h,cpp}`, `moduleinstaller.{h,cpp}`) — selecting a cached 1 GB package used to stall the UI for seconds. Two root causes fixed: details loading no longer builds the file tree eagerly (the tab shows a hint until opened, and rebuilding happens immediately only when the Files tab is already open), and the tree is now built on a background thread (`QtConcurrent::run`). A new `findCacheZipFast()` identifies the cached zip by reading only its central directory (milliseconds) instead of hashing the entire file into memory; the full SHA256 check still guards actual installs. Child nodes materialize lazily on expand (UI cost proportional to what you open, not the package size), directories sort off-thread, and rapid selection changes cancel stale scans via a generation counter.

### Fixes

- **Updater failure flow** (`updater/main.cpp`) — `fatal()` now really terminates the process. Previously a missing or corrupt update package logged the error and *kept running*, clearing the app directory and moving an empty stage — worst case, the launcher was wiped with no new version in place. `fatal()` also clears the `.updating` flag so the old version remains launchable after a failed update.
- **Download robustness** (`ckan/downloader.cpp`) — synchronous downloads now carry a transfer timeout (a stalled server no longer hangs the event loop forever), and HTTP ≥ 400 responses are no longer appended to the partial file and resumed from a wrong offset — the downloader abandons that mirror and moves to the next one; only genuine transport interruptions resume in place.
- **Downloader async handler stacking** (`ckan/downloader.{h,cpp}`) — the async completion handler is connected once in the constructor instead of on every `downloadAsync()` call, which previously stacked one handler per call and fired the completion path multiple times per reply.
- **Background scaling performance** (`mainwindow.{h,cpp}`) — window resizing no longer smooth-rescales the background image on every event; during the drag the current pixmap is stretched instantly via `scaledContents`, and a 150 ms idle timer triggers a single precise rescale once resizing stops.
- **Kerbal search debounce** (`savedetailpage.{h,cpp}`) — typing in the kerbal search box is debounced (250 ms) instead of clearing and rebuilding both lists on every keystroke; programmatic refreshes still populate immediately and keep the search term.

### Tech Stack

- **Framework**: Qt 6.12 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; MinGW (Qt-bundled); Inno Setup 7 installer
- **Testing**: `test_libckan` + `test_launcher` (all passed)
