# EdgeClock

## Overview
EdgeClock is a lightweight, single-file Win32 desktop clock application written in C++. It displays the current time (HH:MM) in the bottom-right corner of the screen with a transparent, outlined text overlay. The clock auto-hides when fullscreen apps are detected or the taskbar is raised, using smooth slide animations.

## Architecture
This is a **single-file Win32 application** (`EdgeClock.cpp`, ~1090 lines). All logic resides in one `.cpp` file with no external dependencies beyond the Windows SDK and GDI+.

### Key Components
| Component | Lines | Purpose |
|---|---|---|
| `Config` namespace | 40–67 | Default settings (font, colors, animation, offsets) |
| `SaveConfig` / `LoadConfig` | 71–118 | Persist settings to `HKCU\Software\EdgeClock` registry |
| `IsStartupEnabled` / `SetStartup` | 326–352 | Manage "Run on Startup" via `HKCU\...\Run` registry key |
| `UpdateLayeredWindowContent` | 175–244 | GDI+ rendering of the clock text onto a layered window |
| `CalculateTextSize` | 249–287 | Measures text bounding box to size the window exactly |
| `ShowContextMenu` | 354–376 | System tray right-click menu |
| `SettingsWndProc` | 434–749 | Dark-themed settings dialog (colors, font, offsets, animation) |
| `WindowProc` | 782–973 | Main window procedure: timers, animation, auto-hide logic |
| `EnableEfficiencyMode` | 977–1018 | Enables EcoQoS / IDLE priority for power efficiency |
| `WinMain` | 1020–1089 | Entry point: single-instance mutex, init, message loop |

### Timer System
- **Timer 1** (300ms): Logic check — determines if clock should hide (mouse hover, taskbar visible, fullscreen app)
- **Timer 2** (1000ms): Clock update — redraws time text when minute changes
- **Timer 3** (10ms, dynamic): Animation tick — handles smooth slide up/down

### Configuration Storage
All settings stored in Windows Registry at `HKEY_CURRENT_USER\Software\EdgeClock`. Startup auto-run registered at `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`.

## Build
```bat
windres EdgeClock.rc -o resource.o
g++ -o EdgeClock.exe EdgeClock.cpp resource.o -lgdi32 -luser32 -lgdiplus -lcomdlg32 -lole32 -luuid -mwindows -ldwmapi -lcomctl32 -static
```
Or run `build.bat`. Requires MinGW with `g++` and `windres`.

`build.bat` compiles with `-Wall -Wextra` and the tree is warning-clean, so any
warning that appears is new and worth reading.

### Checks before a release
Run `check.bat`. It is free, deterministic and offline — three stages:
1. `-Wall -Wextra` syntax pass
2. stricter pass: `-Wshadow -Wformat=2 -Wnull-dereference -Wcast-align` at `-O2`
   (`-Wnull-dereference` needs optimisation to see the data flow)
3. GDI/handle pairing audit (`GetDC`/`ReleaseDC`, `CreateCompatibleDC`/`DeleteDC`,
   `CreatePopupMenu`/`DestroyMenu` — the last may legitimately differ because
   `DestroyMenu` recursively frees attached submenus)

Notes: `g++ -fanalyzer` is a no-op here — GCC's static analyzer only supports C,
not C++. `cppcheck`/`clang-tidy` are not installed on this machine; if added they
would slot into `check.bat` as a fourth stage.

### Linked Libraries
`gdi32`, `user32`, `gdiplus`, `comdlg32`, `ole32`, `uuid`, `dwmapi`, `comctl32` — all statically linked.

## Key Files
| File | Purpose |
|---|---|
| `EdgeClock.cpp` | Entire application source |
| `EdgeClock.rc` | Resource script (icon, manifest, version info) |
| `EdgeClock.exe.manifest` | Application manifest for Common Controls v6 |
| `EdgeClock.iss` | Inno Setup installer script |
| `build.bat` | One-click build script |
| `clock_23989.ico` | Application icon |

## Important Patterns
- **Layered Window**: Uses `WS_EX_LAYERED | WS_EX_TRANSPARENT` for click-through transparent overlay
- **GDI+ GraphicsPath**: Text rendered as outlined path (not plain DrawString) for border effect
- **Single Instance**: Uses a named mutex `EdgeClock_GlobalInstance_Mutex`
- **RAM Trimming**: Calls `SetProcessWorkingSetSize(-1, -1)` after init and each minute redraw

## Known Issues / Limitations
- Font weight is limited to the styles GDI+ exposes (Regular/Bold/Italic/Bold Italic); Light/Thin requires picking a separate family such as "Segoe UI Light"
- Fade during slide is always on (not configurable)
- Localization covers English and Thai only

## Recent Fixes
- Log path now resolves next to exe (was relative to CWD)
- Startup registry path is now quoted for paths with spaces
- `IsStartupEnabled` checks the stored path contains `edgeclock.exe` (substring check, not a full path match against the current exe)
- Settings dialog GDI resources properly nulled after deletion (was use-after-free)
- Settings class brush uses stock object (was leaked)
- Size presets now persist to registry via `SaveConfig()`
- `WM_ENDSESSION` handler for graceful shutdown/logoff cleanup
- Mutex handle properly closed on exit
- Manifest declares PerMonitorV2 DPI awareness
- Settings dialog GDI brushes/font freed in `WM_DESTROY` (was `WM_CLOSE`, leaked on Save/Cancel)
- "Defaults" button only updates temp/UI state — Cancel after Defaults no longer mutates live config
- Save clamps font size (4–200) and outline width (0–50) against invalid input
- Slide animation distance includes `offsetY` so duration matches the configured value

## Recent Improvements (2026-07)
- **Multi-monitor**: clock positions on the monitor hosting the taskbar (`UpdateScreenMetrics`)
- **Fullscreen detection**: `SHQueryUserNotificationState` (D3D fullscreen / presentation / F11) + per-monitor rect fallback
- **Taskbar edge-aware**: only a bottom taskbar triggers auto-hide (`ABM_GETTASKBARPOS`)
- **Custom time format**: `Config::timeFormat` (strftime, validated) — supports seconds, 12h, date
- **Opacity setting**: `Config::opacity` (20–100%) via `SourceConstantAlpha`
- **Live preview**: settings apply instantly; Cancel/X restores a snapshot taken at dialog open
- **Time-based animation**: wall-clock progress + ease-out cubic (immune to timer jitter under IDLE priority)
- **Tray icon survives Explorer restart** (`TaskbarCreated` re-registration)
- **Left-click tray icon opens Settings**; `WM_DPICHANGED` handled
- Removed auto-recreation of Start Menu shortcut (installer owns it)
- `WM_DRAWITEM` button painting deduplicated into `DrawDarkButton`

## v1.6 — Performance rework + UX (2026-07-25)
**Rendering core** — the expensive GDI+ work is separated from presentation:
- `BuildClockPath` builds the glyph outline **once** per update; both sizing and
  drawing use it, and `GetBounds` gives an exact fit (the old code measured with
  a point-size font but drew with pixel em sizes, so windows were oversized)
- The rendered text lives in a cached 32bpp DIB (`EnsureSurface`/`RenderClock`);
  `PresentClock` is a single `UpdateLayeredWindow` that moves, resizes and
  blends in one call — so slide + fade cost **no** GDI+ work per frame
- `FontFamily` is cached and rebuilt only when the font name changes

**Wakeups** — `ArmClockTimer` schedules the text timer for the next second/minute
boundary instead of polling every 1000 ms (60 wakeups/min → 1), drops to
once-a-minute while hidden, and the logic tick no longer makes cross-process
shell calls (taskbar edge is derived geometrically; `SHQueryUserNotificationState`
is throttled to foreground changes or every 5th tick). Timer 1 stops entirely
while manually hidden. Working set is trimmed when going hidden, after the
Settings dialog closes, and every 10th minute — not every minute.
Measured vs v1.5.0, both hidden for 60 s: **CPU 125 ms → 47 ms, working set
5.2 MB → 0.5 MB**.

**Features**: Pill effect mode, font weight, effect intensity (`fxIntensity`),
corner selection (4 corners), explicit monitor selection, fade during slide,
English/Thai localization (`ApplyLanguage`/`L()`), settings export/import
(UTF-16 key=value file via the tray menu).

**Settings dialog**: DPI-scaled (`QueryDpi`/`DS()` — required because the
manifest declares PerMonitorV2, so Windows does not scale it for us), two-column
layout with section headers and separator lines, full keyboard navigation
(`WS_TABSTOP` + `IsDialogMessage`, Enter→Save, Esc→Cancel, focus rectangles).

**Other**: log file rotates at 32 KB; `ClampConfig` centralises range checks;
`WM_APP+1` from a second instance now un-hides the clock.
