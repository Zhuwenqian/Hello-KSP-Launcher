# Release Notes

## v1.4.0 — Ship Management, Progressive Mod Selection & UI Performance (2026-09-18)

This release adds a full ship (`.craft`) manager for instances and saves with drag-and-drop import, makes the recommends/suggests dialogs progressive and per-module, speeds up the instance detail page through lazy loading, and lets the mod list persist its view state per instance.

### Ship Management (Instances & Saves)

A new ship manager lists, inspects and imports `.craft` files for any instance and for any save, mirroring the save manager's tabbed placement.

- **Instance ship manager** (`shiptabpage.{h,cpp}`, new) — an `QTabWidget` with **VAB / SPH** tabs listing `.craft` files (auto-excluding `.loadmeta`, `.craft.original`, etc.); each row shows the ship name (sans `.craft`) and game version (parsed from `version = X.Y.Z`, "Unknown" when missing).
- **Ship detail (second page in-list)** — clicking a row opens name (`ship = XXX`), game version, and a read-only description (`description = XXX`), with the matching thumbnail from `Ships/@thumbs/VAB|SPH` on the right (rocket SVG fallback when missing).
- **Parsing** (`instancemanager_ships.cpp`) — `listCraftFiles` (filename-sorted, `.craft` only), `loadCraftInfo` (read-only `ship/version/description`, name falls back to filename), `getShipThumbPath` (png/jpg), `moveCraftToTrash`.
- **Entry points** — the instance detail sidebar gained a "Ship Management" 5th tab (`showSection(5)`, `rocket.svg`); the save detail page gained a "Ship Management" 3rd tab reusing the same `ShipTabPage` via a settable base path (`setShipsBase`) that auto-locates `instanceRoot/saves/saveName/Ships/VAB|SPH`.
- **Import ships** — the list toolbar gained an "Import Ships" button (`download.svg`); import via `QFileDialog` (multi-select) or by directly **dragging-and-dropping** `.craft` onto a VAB/SPH tab; imports into the current tab's folder; case-insensitive name collisions prompt an "Overwrite Ship" confirm (Yes = overwrite / No = skip / Cancel = abort the rest), preserving the target's original casing.
- **Delete to recycle bin** — each row now has a trash button (replacing the old decorative arrow in the save list too); deletion moves the `.craft` / save folder to the system recycle bin on Windows (`SHFileOperation(FO_DELETE + FOF_ALLOWUNDO)`), permanent delete elsewhere.
- **Translations / tests** — new ShipTabPage strings fully translated to en_US (0 unfinished); `test_launcher` adds `TestShips` (top-level key parsing, nested keys don't override, name fallback, list filtering, thumbnail location). No automated tests for the import / trash flows (they depend on modal dialogs and instance paths, consistent with project convention).

### Progressive, Per-Module Recommends/Suggests Dialogs

Installing a module now resolves and asks for its recommendations/suggestions **module by module**, instead of aggregating everything into one giant list.

- **Collection** (`relationshipresolver.{h,cpp}`, `ckan.{h,cpp}`) — new `collectOptionalFor(parent, curInstallSet, wantRecommends)` collects only a **single parent module's** own Recommends/Suggests candidates, skipping already-installed/selected/conflicting ones, with **no auto-cascade**. The old all-in-one aggregation is no longer used for popups.
- **Install flow** (`installservice.cpp` `resolveInstallSet`) — two-phase progressive resolution:
  - **Phase 1 (Recommends)** — for each explicitly-installed module, show a recommends dialog one at a time; merge selections into the install set and queue newly added modules (FIFO) until nothing new appears.
  - **Phase 2 (Suggests)** — show a suggests dialog per explicit module; if a newly-chosen module's recommends weren't shown in phase 1, ask them first, then its suggests.
  - Each module is installed once (deduplicated by identifier); modules with no candidates are silently skipped; each dialog is titled with its source parent.
- **Selection dialog** (`moddecision.{h,cpp}`) — `askOptionalModules` gained a Trak-a-dock **"Select all / Select none"** toggle button that flips with the current state, plus a `parentName` context header. The **Cancel button has been removed** — the only way out is "Install Selected"; abandoning an install returns you to mod management.
- **Tests** — `test_libckan` adds `collectOptionalNoCascade` (per-module, no cascade, no cross-contamination) and `collectOptionalSkipsInstalledAndSelected`. All green.

### Mod Management UI (Splitter & Per-Instance State)

- **Draggable list/detail splitter** (`modstabpage.cpp`, `configmanager.{h,cpp}`) — a vertical `QSplitter` (`modSplitter`) sits between the mod table and the 4 detail tabs (metadata/files/relationships/versions); dragging it resizes the list height (list stays on top, detail tabs move down). Initial 3:2 split, both panes non-collapsible; the bottom action bar stays put. The top-pane height persists to `HKSPL.json` (`modSplitterTopHeight`, global) with 250ms debounced saving; applied exactly in the first `showEvent` by the splitter's real height.
- **Per-instance list state** (`configmanager.{h,cpp}`, `modstabpage.cpp`) — each instance's mod list restores its exact view across instance switches/restarts: search text, status filter (All/Installed/Upgradable/Not installed), tag filter, detail tab index, sort column+order, vertical scroll, and the selected module. Saved on change (300ms debounce; flushed before switching instances) into a dedicated `modListViewState` section keyed by instance id. Restored after the async list load (`restoreListStateAfterLoad`) with an `m_restorePending` flag distinguishing first-entry load from same-instance refresh.

### Instance Detail Page — Lazy Loading

Entering instance management no longer loads everything ahead of time; each secondary tab fetches its data only when entered.

- `instancedetailpage.cpp` `showSection` — mod manager tab triggers `prepareMods()` (async index + DLL scan), save tab `loadSaves()`, ship tab `loadShips()`. The page opens on "Game Settings" and does no heavy work up front.
- **Mods tab** (`modstabpage.{h,cpp}`) — `setInstance` only records the instance, and `setTabActive(true)` re-prepares mods on each entry.
- **Saves tab** (`savestabpage.{h,cpp}`) — `loadSaves()` runs in a `QtConcurrent::run` background thread iterating the directory and parsing `persistent.sfs` per file, filling the list via a `QFutureWatcher` (with a "Loading saves..." placeholder); post-delete refresh reuses the same async path.
- **Ships tab** (`shiptabpage.{h,cpp}`) — `loadShips()` parses VAB + SPH in a background thread and fills via watcher; row construction is split out as `addShipRow`.
- **Thread safety** — `InstanceManager::listSaves/loadSaveInfo/listCraftFiles/loadCraftInfo` are pure file reads with no shared mutable state, safe to call off the main thread.

### Crash Log Dialog — Context & One-Click Log Packing

- **Context window** (`playerloganalyzer.{h,cpp}`) — `analyzePlayerLog` gained a `context` field; `extractPlayerLogContext` takes the key error line (crash marker `Caught fatal signal` / OOM) and walks back up to 40 lines (`kContextLeadingLines`) from there to the end of the log.
- **Dialog** (`mainwindow.cpp` `maybeShowCrashAnalysis`) — a read-only, selectable `QPlainTextEdit` (min height 240px) now pastes the error context inside the message box, plus a new "Save Log" button.
- **One-click packing** (`mainwindow.{h,cpp}` `packLogToZip`) — streams the whole `Player.log` into a zip (miniz callbacks, never loading a multi-hundred-MB log into memory), saved next to the launcher as `<cleaned-instance>-<id8>_<yyMMdd_HHmmss>.zip`; illegal filename characters are folded to underscores. Success shows the full path; failure warns without leaving a partial zip.
- **Tests** — `test_launcher` adds three `extractPlayerLogContext` cases (≥40-line backtrace start, short-log head start, no crash returns empty).

### UI Polish

- **Secondary sidebar alignment** — the instance-detail and save-detail secondary menus now run the full window height as a left column, width unified to **220px** matching the home sidebar (the back/title bar moves into the content area top).

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain
- **Testing**: `test_libckan` + `test_launcher` (all passed)