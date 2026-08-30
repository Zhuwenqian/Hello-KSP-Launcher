# Release Notes

## v1.1.2 — Modpack Pack/Extract Performance Overhaul (2026-08-30)

A performance-focused release that reworks both modpack packing and extraction to fix memory blow-ups, I/O bottlenecks, and unresponsive UI on huge packs, plus fallback browsing for uncached mods and relaxed instance validation.

### Modpack Export (Packing GameData to ZIP)

- **No in-memory file list** (`instancemanager_modpack.cpp`) — the old `collectExportFiles` gathered every `ExportFileEntry` into a `QList` and sorted it, so a giant pack's manifest lived entirely in RAM. Export now walks the tree twice via a callback (`walkExportFiles`): the first pass only sums total bytes and confirms the pack is non-empty, the second pass writes entries one by one. No manifest is retained; the by-path sort was dropped in exchange (memory wins over disk locality).
- **Small-file fast path** — files ≤1 MB (the overwhelming majority of KSP mod files) are read into a bounded buffer and written once through `mz_zip_writer_add_mem`, avoiding per-block callbacks and repeated seeks; files >1 MB still stream through `mz_zip_writer_add_read_buf_callback` with fixed memory per file. Directory entries reuse the `ensureExportDirs` dedup cache.
- **Buffered disk writes** (`BufferedFileWriter` behind `mz_zip_writer`'s `m_pWrite`) — miniz emits many small chunks (30-byte local headers, file names, extra fields, compressed output); writing each straight to disk caused a flood of `seek+write` syscalls, the true I/O bottleneck. Sequential chunks are now coalesced and flushed to disk in 1 MB batches (out-of-order offsets flush-then-seek as a safety fallback), with the remaining buffer flushed after finalize and before close. The backend stays `QFile`, so Unicode paths still work.
- **Cancelable export** — a new `shouldCancel` callback is checked before each file; the progress dialog's Cancel button now actually interrupts the export instead of just closing the dialog.

### Modpack Import (Extracting to GameData)

- **No whole-pack-in-memory** (`modpackio.cpp`) — the old `openZipFromFile` slurped the entire zip with `readAll()`. Import now opens via `mz_zip_reader_init_cfile` with wide-char `_wfopen` (UTF-8 `fopen` on POSIX) and lazily `fseek`/`fread`s on demand; `OpenedZip::close()` both ends the reader and `fclose`s the file (miniz does not own the file in CFILE mode). This also fixes the UI freeze from the synchronous pre-import prefix probe.
- **Giant-pack memory** — per-file extraction switched from `extract_to_heap` (whole file into the heap) to the `extract_iter_*` streaming iterator (fixed ~96 KB buffer, CRC check preserved); the `QVector` of all entry indexes/paths is gone (two passes: first counts bytes and validates every path against Zip Slip, only then clears GameData; second extracts entry by entry); `DO_NOT_SORT_CENTRAL_DIRECTORY` drops the central-directory sort array. Progress is now reported smoothly every 1 MB.
- **Scattered small files** (KSP mods are mostly tiny files) — extraction is split by size: files ≤1 MB are decompressed once via `extract_to_heap` and written in a single syscall, avoiding the ~100 KB alloc/free overhead of a streaming iterator per small file; larger files still use the iterator for bounded memory. Parent directories are cached in a `QSet` so each one is `mkpath`ed only once, eliminating tens of thousands of redundant directory syscalls.

### Mod Management

- **"Files" tab browses installed dir when uncached** (`CKan::installedGameDataEntries` + `showContentsTab`) — if the zip isn't cached but the mod is installed (including manually-installed AD mods), the tab now recursively browses the installed folder instead of just saying "download first": level 1 is always `GameData`, level 2 is the mod folder, with subfolders expandable and file sizes shown. The "Download ZIP" button remains for viewing the zip's own listing. The lookup covers both registry file ownership and AD DLL paths.
- **AD mods show installed version** (`CKan::autoDetectedVersion` + `showVersionsTab`) — manually-installed mods have no version in the registry, so the Versions tab couldn't mark them installed. Now the version is derived from the DLL file name per official DllScanner semantics (`ModuleManager.4.2.3.dll` → `4.2.3`), falling back (Windows only) to the DLL's embedded PE file-version resource; matches show "Installed (AD)" via exact string match first, then a numeric comparison tolerant of `.0` suffixes; if nothing matches, the latest version is marked "Installed (AD)". `libckan` links the `version` import library on Windows.

### Instance Management

- **New-instance validation no longer requires `settings.cfg`** (`isValidKSPPath`) — `settings.cfg` is only generated after the game first launches, so a fresh, never-run KSP install couldn't be added. Validity is now "KSP executable present (`KSP_x64.exe` / `KSP.exe` / `KSP.x86_64`) AND `GameData` exists"; Steam auto-discovery (`steamdiscovery.cpp`) was relaxed the same way, and the warning text no longer mentions `settings.cfg`.

### Build & Tooling

- **Sidebar "Game" section label** (`mainwindow.cpp`) — a " 游戏" section header now sits above the Home entry.
- **CMake** — `libckan` sets `PREFIX ""` so the artifact is `libckan.dll` instead of `liblibckan.dll` under MinGW; links `version` on Windows for the PE version lookup; `test_launcher` now also compiles `instancemanager.cpp`.
- **Tests** — new `TestCkanInstalledBrowse` cases (registry + AD dir entries, AD file-name version derivation, empty fallback) and 4 new instance-validity regressions (fresh install without settings.cfg valid, missing GameData invalid, settings.cfg-only without exe invalid, Unix executable fallback valid).
- **Version** — bumped to 1.1.2 via `sync_version.py` (also fixed a latent bug in it that dropped a title line's newline when stamping the version).

### Tech Stack

- **Framework**: Qt 6 (Widgets, Svg, Network, Concurrent)
- **Language**: C++17
- **Build**: CMake ≥ 3.16; Windows uses Qt's bundled mingw toolchain
- **Testing**: `test_libckan` + `test_launcher` (all passed)

---
