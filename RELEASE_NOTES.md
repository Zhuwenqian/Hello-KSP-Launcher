# Release Notes

## v1.1.1 — About Page, Resizable Mod Table, & Stop Game (2026-08-29)

A focused feature release adding the About page, draggable/persisted mod-table column widths, the ability to terminate the running game, and a version-bump automation script.

### About Page

- **Sidebar "About" entry** (`mainwindow.cpp`) — a new "About" button (info.svg) sits below the Settings entry and opens `AboutPage`, which scrolls and reuses the settings page's semi-transparent backplane (`settingsContent`).
- **About section** — app icon + launcher name + version read from the single source `HKSPL_APP_VERSION` (`src/appversion.h`, currently v1.1.1); author avatar (author.png, circular-cropped) + name + bilibili link.
- **Acknowledgements** — KSP-CKAN team (C# reference code and repo index), Hello Minecraft Launcher project (UI design reference), and CKAN download mirrors gh-proxy.com / ghfast.top.
- **Dependencies** — Qt 6 Community (LGPL v3), miniz (Copyright richgel999 · MIT), libckan (Copyright Zhu Wenqian · GPL v3).
- **Legal & Notices** — copyright © 2026 Zhu Wenqian; license GPL v3 with a repo LICENSE link.
- **Interactions & i18n** — link rows are clickable cards (hover tint/underline via the `aboutLink`-style object `aboutCard`), opening the system browser through `QDesktopServices`; all strings translated into zh_CN / en_US and recompiled with lrelease.

### Mod Management

- **Draggable column widths** (`instancedetailpage_mods.cpp`) — every column is `Interactive` except the leading checkbox column (fixed 40); drag the header edges (Excel style); the last "Tags" column keeps `stretch-last` to fill remaining space.
- **Persisted globally** (`configmanager.cpp`) — widths are stored once per-app in `HKSPL.json` under `modTableColumnWidths` (array indexed by column), written after a 250 ms debounce; the stretched last column is not persisted. Missing entries fall back to built-in defaults `{40,220,180,90,80,90,70,140}`.
- **Double-click to reset** — double-clicking any header restores that column's built-in default width.

### Game Lifecycle

- **Running game switches to "Stop"** (`mainwindow.cpp`) — the launch button turns red (dynamic property `stopActive`, with new dark/light QSS rules) showing " 停止"; its icon is re-tinted white on the red background.
- **Confirm before terminating** (`onLaunchClicked`) — clicking while running warns "Are you sure you want to terminate the game process? This may cause data loss."; only a explicit Yes proceeds.
- **Graceful then forced exit** (`InstanceManager::stopGame`) — `terminate()` first gives the game a chance to save, then `kill()` if it does not exit within 3 seconds.
- **No spurious error dialog** — a new `m_stoppingGame` flag suppresses the "game process error" warning on intentionally terminated runs (which raise `errorOccurred(Crashed)`) and keeps the window state unchanged; a self-exited game still restores the window if it was minimized.

### Build & Tooling

- **`sync_version.py`** — a cross-platform script that bumps the version across `CMakeLists.txt` `project(... VERSION ...)`, `src/appversion.h`, and the latest "## date: title" entry in `README/功能更新.md` in one step (e.g. `python sync_version.py 1.1.2`).
- **Single version source** — new `src/appversion.h` (`HKSPL_APP_VERSION`) is the app's single source of truth for the displayed version.
- **README polish** — updated the clone URL to the public repo and unified "Kerbal" → "小绿人" wording.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain
- **Testing**: `test_libckan` + `test_launcher` (all passed for the stop-game changes)

---