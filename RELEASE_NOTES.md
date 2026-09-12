# Release Notes

## v1.3.0 — Save Management Rework, Updater Self-Update & UI Polish (2026-09-12)

This release refactors save management into a sub-tab of the instance detail page, upgrades the backup directory scheme to avoid collisions, lets the built-in updater update itself, and polishes the custom title bar and About page.

### Save Management Now Lives Inside the Instance Detail Page

"Save Management" was previously a standalone full-screen page (`SavesListPage`) that duplicated the instance-detail sidebar and had to route export/import/browse **back** through the mod-management tab. It is now a second-level tab of the instance detail page, consistent with Game Settings and Mod Management.

- **New `savestabpage.{h,cpp}`** — contains only the save list (`loadSaves`, double-click `saveSelected`). Object name `savesTabPage`.
- **`instancedetailpage.{h,cpp}`** — adds a 4th tab (Save Management) to its `QStackedWidget`; the sidebar "Save Management" button now calls `showSection(4)`; `showSection` gained idx=4 selection and `loadSaves`; new `saveSelected` signal; removed the `savesManageRequested` signal.
- **`mainwindow.{h,cpp}`** — dropped `SavesListPage` and the `onSavesManageRequested` / `onSavesNavToDetail` / `onSavesModpackAction` / `onBackFromSavesList` routing; `InstanceDetailPage::saveSelected` connects straight to `onSaveSelected`; `onBackFromSaveDetail` returns to the instance detail page and stays on the save tab.
- **Export/import/browse** — triggered in place from within the save tab, reusing the instance detail sidebar's ZIP/CKAN and browse menus (same `ModpackController`), no tab hopping.
- **Removed** `saveslistpage.{h,cpp}`; `CMakeLists.txt` and `dark/light.qss` `savesListPage` rules cleaned up accordingly.
- **Tests** — both suites build; the UI was verified manually: save management never navigates away, export/import opens in place, and returning from save detail lands back on the save tab.

### Backup Directory Upgrade (Collision-Proof)

Backups previously lived in `backups/{instanceName}/{saveName}/*.zip`; two similarly named instances with the same save name could overwrite each other's backups. The first level now appends the first 8 characters of the instance id (the first UUID segment, e.g. `550e8400`): `backups/{instanceName-idPrefix}/{saveName}/*.zip`. Instance ids are globally unique and unchanged by renaming, so backups stay locatable even after any explicit rename.

- **Directory computation** (`instancemanager_backup.cpp` `getBackupDirForSave`) — `sanitizeFileName(instanceName) + "-" + instanceId.left(8)`; the save-name level keeps the plain name. An empty id falls back to the instance name alone for robustness.
- **Lazy migration** (`migrateLegacyBackups`) — both legacy layouts (`backups/{instanceName}/{saveName}` and the even older single-level `backups/{saveName}`) are migrated on access; if a target already exists (name collision) migration is skipped to stay idempotent; migrated `*.zip` files are moved and old dirs emptied.
- **Instance id threading** — backup-related `InstanceManager` interfaces (`getBackupDirForSave` / `listBackups` / `createBackup` / `restoreBackup`) gained an `instanceId` parameter; the `saveSelected` / `setSavePath` chain `savestabpage → instancedetailpage → mainwindow → savedetailpage` passes the id along (`savestabpage` uses `m_instance.id` directly).
- **Backup zip file naming is unchanged** — `saveName[_note]_yyyyMMdd_HHmmss.zip`; only the directory level changed.

### The Built-in Updater Can Now Update Itself

The updater is exempt from its own keep-list (it always skips `updater.exe` during cleanup/move), so it cannot replace itself. Previously the Release notes asked v1.2.0 users to replace it manually. Now, when a Release notes body marks "this Release ships an updated built-in updater", the update **keeps the update package zip** and the restarted new launcher silently replaces `updater.exe` — no manual step required.

- **Detection** (`updatemanager.{h,cpp}` `bodyIndicatesUpdaterUpdate`) — matches the bilingual phrases in the Release body (Chinese "本 Release 含有更新器" or English "ships an updated built-in updater"); on a hit `ReleaseInfo.updaterUpdate=true`.
- **Keep zip** (`updatemanager.cpp` `applyUpdate`) — on Windows, if `updaterUpdate`, writes an `updater_pending` marker (containing the zip filename) into `.updater_update/` and passes `--keep-zip` to the updater; `src/updater/main.cpp` parses `--keep-zip` and no longer deletes the `.updater_update` directory on completion.
- **New launcher replacement** (`main.cpp`, after the startup guard, Windows only) — `applyPendingUpdaterUpdate` detects `updater_pending` → looks up `updater.exe` in the kept zip by basename (tolerates the single-top-level release layout, case-insensitive) → extracts to a temp file → overwrites the app dir's `updater.exe` → on success removes `.updater_update` entirely. On failure a dialog offers "Retry / Ignore" (Ignore calls `cleanupPendingUpdaterUpdate`); normally silent and non-blocking.
- **Zip entry API** (`updatemanager.{h,cpp}`) — `findZipEntryByBaseName` / `extractZipEntry` (miniz-based) for locating and extracting updater entries at any depth.
- **Translations** — 7 new UI strings with full en_US (`556` all translated), `.qm` rebuilt and synced to `dist/translations`.
- **Tests** — `test_launcher` adds `updaterUpdateBodyDetection` and `zipUpdaterEntry` (both-locale detection, no false positive on plain logs, any-depth lookup, case-insensitivity, missing entry, extraction content match). Both `ctest` suites pass.

### Custom Title Bar Edge Resizing Fixed

`Qt::FramelessWindowHint` strips the native `WS_THICKFRAME` style that enables window resizing, so the frameless window could not be resized from its edges.

- **Patch** (`mainwindow.{h,cpp}` `applyNativeFrameStyle`, Windows only) — keeps the frameless custom-drawn look, then after the first `showEvent` and in `toggleMaximize`, re-adds `WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX` via `SetWindowLongPtr(GWL_STYLE)` and calls `SetWindowPos(SWP_FRAMECHANGED)` so the system recomputes the non-client area. `WS_CAPTION` is deliberately not restored, so no system title bar appears.
- **Hit-test** (`WM_NCHITTEST`) — the edge/corner grab area widened from 6px to 8px to pair with the restored `WS_THICKFRAME` for smooth native resizing.
- **Result** — all four edges resize freely and smoothly while the custom-drawn frameless title bar is preserved.

### About Page Entry Icons

- **Credits section** — KSP-CKAN team (`ckan.png`), Hello Minecraft Launcher project (`hmcl.png`); the two mirrors gh-proxy.com and ghfast.top share the GitHub icon (`github.png`).
- **Dependencies section** — Qt 6 (`qt6.png`).
- **Implementation** — 4 PNGs registered into `resources/resources.qrc`; `aboutpage.cpp` loads the `QPixmap` in its constructor and passes it as the 4th arg to `makeLinkRow`.

### Full English (en_US) Localization

The source contains 548 UI strings but `translations/hello_ksp_launcher_en_US.ts` had only 506. Re-extracted with `lupdate` (`build/en_fresh.ts`) and compared to fill in the 43 missing English translations added by recent versions (crash log analysis, modpack import/export, debug mode, recommends/suggests dialogs, batch & uninstall progress, registry-lock hint, self-update SHA256, etc.). All 548 strings now have translations (`lrelease` reports 0 unfinished); the existing 506 were untouched. `.qm` was rebuilt and synced to `dist/translations`. Since English is the default language, first launch now shows a complete English UI.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)

- **Language**: C++17

- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain

- **Testing**: `test_libckan` + `test_launcher` (all passed)