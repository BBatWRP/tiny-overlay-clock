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

## Known Issues
- None currently — all previously known bugs have been fixed.

## Recent Fixes
- Log path now resolves next to exe (was relative to CWD)
- Startup registry path is now quoted for paths with spaces
- `IsStartupEnabled` validates the stored path matches the current exe
- Settings dialog GDI resources properly nulled after deletion (was use-after-free)
- Settings class brush uses stock object (was leaked)
- Size presets now persist to registry via `SaveConfig()`
- `WM_ENDSESSION` handler for graceful shutdown/logoff cleanup
- Mutex handle properly closed on exit
- Manifest declares PerMonitorV2 DPI awareness
