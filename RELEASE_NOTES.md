# Release Notes

## v1.1.0 — Major Enhancement (2026-08-28)

A large set of improvements since v1.0.0 covering mod management, dependency resolution, security hardening, performance, and maintainability.

### Mod Management

- **Four-Tab Mod Details** — Selecting a mod shows four tabs on the right:
  - **Metadata**: name/version/identifier/description/author/license/KSP version/release date/download and install size.
  - **Contents**: tree of files inside the cached zip (scanned with miniz); a "Download archive" button appears when the mod is not cached, refreshing the listing automatically once downloaded.
  - **Relationships**: lazy-loaded multi-level dependency/conflict tree (depends/recommends/suggests/conflicts), with a "Show reverse relationships" toggle that scans the full index.
  - **Versions**: all historical versions listed in descending version order, with one-click install/downgrade (downgrades require confirmation).
- **Batch Operations** — checkboxes in the first column of the mod table, with "Select all / Clear" and "Batch install / upgrade / uninstall" buttons; concurrent progress bar with cancel/timeout.
- **Atomic Install/Uninstall/Upgrade** — transaction rollback via `TxFileManager`; any failure (including cancel) rolls back fully, preventing leftover files and corrupted registry state.
- **Import Single Mod** — "Import mod" supports `.zip` or `.ckan`; falls back to matching a repository download hash by file SHA256 when no metadata is embedded.
- **Install History** — every install/uninstall/upgrade writes a timestamped snapshot to `instance/CKAN/history/` (official history metapackage format), keeping the latest 200 entries; viewable via a dialog.
- **Enhanced Search** — field-level syntax `@author @desc @license @depend(s) @provides` plus plain keyword matching.
- **Repository Tag Display/Filter** — new "Tags" column and an "All tags" dropdown filter; supports `@tag/@tags:value` search.

### Dependency Resolution (aligned with official CKAN semantics)

- **Version Compatibility** — unversioned builds expand to half-open ranges (e.g. `1.12.5` → `[1.12.5.0, 1.12.6.0)`), fixing mods like kRPC being wrongly flagged incompatible.
- **Compatible Versions Option** — top-bar "Compatible versions" selects a contiguous version-line range; defaults derive dynamically from the detected game version, and it drives both list filtering and dependency resolution.
- **any_of Dependencies** — supports the official `any_of` child relationships, satisfied when any child is satisfied.
- **Independent min/max Keys** — relationship constraints support standalone `min_version`/`max_version` (including inclusive flags).
- **File-Level Overwrite Conflict Detection** — checks `Registry::fileOwner` before writing; files owned by other mods are rejected and the whole operation rolls back.
- **Multiple Provider Selection** — when a virtual package has multiple providers, a dialog lets the user choose (mirrors official TooManyModsProvideKraken).
- **Bidirectional Conflict Arbitration / recommends Cascading / release_status Filtering / DLC Interception.**
- **Disk Space Pre-check** — checks the cache drive before download and the game drive before install; on shortage the user may "Ignore and continue" or "Cancel".

### Sources & Cache

- **Multiple Repositories** — add/remove/reorder repositories in settings with prioritized merging of index and download counts; mirror prefixes only apply to GitHub sources.
- **Download Concurrency & Rate Limit** — configurable concurrency (1–8); per-connection download rate limit (MB/s) applied uniformly to index and mod downloads.
- **Official CKAN Cache Compatibility** — recognizes official naming (`{identifier}-{version}.zip` and `{hash}-{identifier}-{version}.zip`) so existing CKAN download caches can be reused directly.
- **SHA256 Download Verification + Multithreading** — mismatched hashes fail the download and fall back to a mirror; failures are never written to cache.
- **Index Refresh Interval / Download Source**, **selectable Download Cache Folder with migration**, and **Precise Cache Cleanup** (never deletes unrelated files).

### Modpacks

- **Export** — "Export as CKAN file" generates an official-format metapackage: dependency-topologically sorted, excluding DLC/auto-installed/manually-installed mods, with `ksp_version_min/max` written from the detected version line.
- **Import** — supports ZIP (auto-detects the GameData hierarchy, clears old mods then extracts) and `.ckan` (parses `depends` and installs from the repository).
- **Path Traversal Protection** — ZIP import and mod installation both normalize and validate paths; absolute or escaping entries are rejected wholesale.

### Instance Management

- **Steam Library Auto-Discovery** — scans Steam libraries on startup (registry + `libraryfolders.vdf`) and adds discovered KSP installs to the instance list.
- **Build ID → Version Mapping** — reads `buildID64.txt`/`buildID.txt` first to derive the semantic version, falling back to parsing `readme.txt`.
- **Game Settings UI Polish** — settings search/filter, hidden over-advanced items (control points/color presets/PAW, original values preserved on save), full English-friendly names, and hidden `LANGUAGE`.
- **Live Browse-Target Visibility** and kOS script directory compatibility for both `Ships/Script` and `Ships/Scripts`.

### Security & Performance (full code review P0/P1/P2)

- **Zip Slip** path traversal prevention (import/install), **index decompression-bomb protection** (≤64MB compressed, ≤256MB decompressed, ISIZE integrity check).
- **Concurrent Index Refresh Race Fix** — index mutex + atomic build-and-swap, eliminating dangling references.
- **Instance Switch/Close use-after-free Fix** — cancels in-flight tasks and waits for threads before deleting objects.
- **Mod Detail Page Entry Lag Optimization** (background DLL scan + caching + progressive loading) and **theme-aware semi-transparent contrast for detail tabs**.
- **Background Modpack Import** with non-blocking UI.

### App Experience & Other

- **Application Icon** — covers window/taskbar/exe icons (`appicon.ico`, injected via windres, avoiding the whitespace-path windres pitfall).
- **Downloader Progress Fix** — installer explicitly moved to the main thread, fixing the stalled install download progress bar.
- **Translations Completed** — zh_CN / en_US fully finished; "Kerbal" unified to "小绿人" (zh) and Serenity wording unified to Breaking Ground.
- **libckan as Shared Library** — fully exported for open source, four-layer decoupling, with build/tests separated from the launcher.

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain
- **Testing**: `test_libckan` + `test_launcher`, coverage of dependency resolution, transaction rollback, and security hardening

---