# Release Notes

## v1.0.0 — Initial Release (2026-08-12)

Hello KSP Launcher is a modern, lightweight launcher for Kerbal Space Program built with Qt 6 and C++17.

### New Features

#### Instance Management
- Add, rename, delete, and switch between multiple KSP installations.
- Automatic detection of KSP game root from the selected executable.
- Quick instance switching via the launch bar dropdown menu.

#### Game Launch
- One-click launch with configurable post-launch behavior:
  - **Keep Open** — Launcher stays open after game starts.
  - **Minimize** — Launcher minimizes to the taskbar.
  - **Auto-Close** — Launcher closes automatically.
- Game running state detection and UI feedback.

#### Settings Editor
- Tree-view browsing of the KSP `settings.cfg` file with grouped categories.
- Boolean values rendered as animated toggle switches (`ToggleSwitch` widget).
- Inline editing for string and numeric values.
- One-click save with file write-back.

#### Mod & DLC Detection
- Automatic detection of installed DLCs (Making History, Breaking Ground, etc.) with clear installed/not-installed status.
- Third-party mod listing from the `GameData` directory.

#### Save Management
- List all saves for a KSP instance with metadata (version, mode, modded status).
- **Save Info** tab — read-only display of save metadata (title, version, mode, seed, timestamp, etc.).
- **Kerbal Editor** — Browse and edit Kerbal attributes:
  - Editable fields: name, gender, trait, bravery, stupidity.
  - Boolean fields (badS, veteran, hero) use toggle switches.
  - Inline editors for gender (combo box), bravery/stupidity (spin box).
  - Save changes back to the persistent SFS file.
- **Backup Manager** — Create, list, and delete save backups with a progress dialog. Reveal backup files in the system file explorer.

#### Modpack Export
- Export the `GameData` directory as a ZIP file using the miniz library.
- Smart exclusions at the GameData root:
  - `Squad` folder
  - `SquadExpansion` folder
  - `ModuleManager.ConfigCache`
  - `ModuleManager.ConfigSHA`
  - `ModuleManager.Physics`
  - `ModuleManager.TechTree`
- Progress dialog with cancel support.

#### UI & Theming
- **Translucent UI** — Sidebar, launch bar, and content cards use RGBA semi-transparent backgrounds (sidebar ~65%, cards ~75%).
- **Custom Background** — Choose any PNG/JPG image as the launcher background. Cover-mode scaling (keep aspect ratio, crop overflow). User-selected images are copied to the app directory to prevent breakage from moved/deleted source files. One-click reset to default.
- **Dark & Light Themes** — Both themes support translucency and background images. Icons automatically tinted to match the active theme.
- **Custom ToggleSwitch** — Smooth animated toggle switch widget replacing combo boxes for boolean values throughout the UI.

#### Launch Arguments
- Per-instance custom command-line arguments (e.g. `-force-d3d11 -popupwindow`).
- Persisted in the configuration file.

#### Configuration
- Settings stored in `HKSPL.json` (JSON format) next to the executable.
- Persisted settings: current instance, theme, language, launch behavior, background image path, per-instance launch args.

### Technical Details

- **Framework:** Qt 6 (Core, Widgets, Svg, Concurrent)
- **Language:** C++17
- **Build System:** CMake ≥ 3.16
- **ZIP Support:** miniz (bundled in `thirdparty/`)
- **Background Manager:** Singleton pattern for loading, caching, and switching background images.
- **Config Manager:** Singleton pattern with Qt signals for change notifications.
- **Instance Manager:** Game process lifecycle management with signals for started/finished/error.
- **Backgrounds:** Default background image bundled in resources. User images stored in `<appDir>/backgrounds/` with UUID-based filenames.

### Known Issues

- None reported at initial release.

### Notes

- The default background image is sourced from `Kerbal Space Program BG2.png`.
- On Windows, use Qt's bundled mingw toolchain for building (not standalone mingw).