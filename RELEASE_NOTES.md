# Release Notes

## v1.4.1 — Part Counting, Subassemblies & Kerbal Management (2026-10-02)

This release enriches the ship manager with part counts, search and player-made vessel thumbnails, adds a Subassemblies tab for saves, introduces full kerbal management (tabs, search, delete/rename, sliders), adds backup date filtering, fixes duplicate update popups, and ships an Inno Setup installer.

### Ship Management

- **Part counting** (`instancemanager_ships.cpp`) — `loadCraftInfo` now derives `ShipInfo::partCount` by counting top-level `PART { … }` blocks in a `.craft` (the parser ignores nested keys). The count shows on every list row ("Game version: X · Parts: N") and on the detail page.
- **Player-made vessel thumbnails** (`instancemanager_ships.cpp`) — when the stock `Ships/@thumbs` image is missing, save-mode ship management now looks up `<game root>/thumbs/<save>_<VAB|SPH>_<craft>.png` (case-insensitive, `.png` only). Instance mode keeps stock-only thumbnails (ships are shared across saves, no single save name); subassemblies always fall back to the rocket icon.
- **Sharp fallback icon** (`iconutils.{h,cpp}`) — new `tintedPixmap` rasterizes a tinted SVG at the target pixel size; the rocket fallback renders at the thumbnail box's real size instead of an upscaled 96px bitmap.
- **Ship search** (`shiptabpage.{h,cpp}`) — a search box on the list page live-filters VAB/SPH/Subassemblies by case-insensitive display-name match; clear button, gray centered "no matching ships/subassemblies" empty states, placeholder rows hidden while filtering, and the filter is re-applied after refresh (delete/import).
- **Detail back handled by the parent** — the per-page "Back" button is gone; the parent save/instance detail page's top "Back" pops the ship detail first (`isDetailVisible()` / `goBackToList()`).

### Subassemblies (Saves Only)

- **Third "Subassemblies" tab** (`shiptabpage.{h,cpp}`) — enabled via `setSubassembliesEnabled(true)` for the save detail page's ship manager; lists `<save root>/Subassemblies/` (sibling of `Ships/`). The instance manager stays VAB/SPH, as subassemblies belong to a single save.
- **Fully wired** — import (button label follows the tab; directory auto-created on first import; importing from its own directory blocked), drag-and-drop, delete to trash, overwrite prompts, and the "no subassemblies detected" empty state, all worded for subassemblies. Listed through the new `listCraftFilesIn(dir)` helper.

### Kerbal Management (Save Detail)

- **Applicant/Crew tabs** (`savedetailpage.{h,cpp}`) — the kerbal list is split into a `QTabWidget` ("Applicants" / "Crew"); only `type == "Crew"` goes to the Crew page, everything else falls back to Applicants, with unrecognized types shown verbatim. Pane styling added to both `dark.qss` / `light.qss`.
- **Search box** — case-insensitive name matching, or `@jobs:Pilot` filtering by trait (English trait name, case-insensitive partial match); the filter survives refreshes, with distinct "no kerbals detected" vs "no matching applicants/crew" empty states.
- **Delete & rename** (`instancemanager_saves.cpp`) — a per-row "…" menu offers delete (removes the `KERBAL` block from `persistent.sfs`'s `ROSTER`) and rename (rewrites the `name =` line); both validate brace balance before and after writing, and write to disk immediately.
- **Sliders for brave/dumb** — double-spin editors are replaced by permanent sliders (0–1.0, step 0.1) with a live value label; values are collected from a custom role on save. Styled in both themes. The gender combo shows localized male/female labels while still saving `Male`/`Female`; type/gender are translated on display.

### Backup Date Filtering

- **Time filter** (`savedetailpage.cpp`) — a search box on the backups toolbar tokenizes the query into digits: a single number matches any timestamp component (year/month/day/hour/minute/second), multiple numbers match the parts in order starting from year (e.g. `2026-01-01 12:30`, `10-02`). The list is cached so keystrokes don't re-read the disk; a "no matching backups" empty state appears when nothing matches.

### Fixes & Behavior Changes

- **Update popups** (`updateflow.cpp`) — updater signals are connected once per process (static guard) instead of re-connected on every check; previously auto plus repeated manual checks stacked handlers and fired multiple dialogs, with later ones parentless.
- **Default language** for fresh configs is now `en_US`; the mirror update source is renamed to "Mirror (zwqbook.cn)" (`configmanager.cpp`, `settingspage.cpp`).

### Packaging & Build

- **Inno Setup installer** (`installer/helloksplauncher.iss`, `make-installer.ps1`, new) — builds a per-user installer (`{userpf}`, no UAC requirement, admin override allowed) with a language dialog, desktop-icon task and lzma2/max compression. Runtime-written files (`HKSPL.json`, `backups`, `ckan_cache`, `generic`, logs) are excluded and removed by the uninstaller; the version is auto-synced from `src/appversion.h`. Output: `HelloKSPLauncher-<version>-setup.exe`.
- **Qt 6.12 toolchain** (`build.ps1`) — configures from scratch (MinGW Makefiles, explicit compiler/prefix paths), runs `windeployqt` to keep `dist/` DLLs in sync with the build Qt, and copies `Qt6Concurrent.dll` for `libckan.dll`.
- Misc: version bumped to 1.4.1; `.gitignore` adds `.vscode/` and `*-setup.exe`; `_gen_icons.py` renamed to `gen_icons.py`; all new strings translated to en_US/zh_CN with `.qm` rebuilt.

### Tech Stack

- **Framework**: Qt 6.12 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; MinGW (Qt-bundled); Inno Setup 7 installer
- **Testing**: `test_libckan` + `test_launcher` (all passed)
