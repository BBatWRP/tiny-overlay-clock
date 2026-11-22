# EdgeClock

## Overview
EdgeClock is a lightweight, single-file Win32 desktop clock application written in C++. It displays the current time (HH:MM) in the bottom-right corner of the screen with a transparent, outlined text overlay. The clock auto-hides when fullscreen apps are detected or the taskbar is raised, using smooth slide animations.

## Architecture
This is a **single-file Win32 application** (`EdgeClock.cpp`, ~2,500 lines). All logic resides in one `.cpp` file with no external dependencies beyond the Windows SDK and GDI+.

### Key Components
Line numbers drift with every change — search for the name instead.

| Component | Purpose |
|---|---|
| `Defaults` / `Config` namespaces | Factory defaults (the only copy) and live settings |
| `ClampConfig` | Repairs out-of-range / NaN values; used by load, import and the Settings dialog |
| `SaveConfig` / `LoadConfig` | Persist settings to `HKCU\Software\EdgeClock` (typed `RegGetValue` reads) |
| `IsStartupEnabled` / `SetStartup` | Manage "Run on Startup" via `HKCU\...\Run` registry key |
| `BuildClockPath` / `RenderClock` / `PresentClock` | Glyph path once per update → cached DIB → `UpdateLayeredWindow` |
| `RecalculateAll` / `RefreshClockText` | The only draw path, and the only correct way to request one |
| `ShowContextMenu` | System tray right-click menu |
| `SettingsWndProc` / `DoSettingsDialog` | Dark-themed modal settings dialog with live preview |
| `EvaluateVisibility` | Auto-hide logic (hover, taskbar, fullscreen) |
| `WindowProc` | Main window procedure: timers, animation |
| `EnableEfficiencyMode` | Enables EcoQoS / IDLE priority for power efficiency |
| `WinMain` | Entry point: single-instance mutex, init, message loop |

### Timer System
- **Timer 1** (120/500/1000ms, adaptive — `ArmLogicTimer`): visibility safety net
  behind the WinEvent hooks. Snappy near the cursor, relaxed otherwise, stopped
  entirely while manually hidden.
- **Timer 2** (boundary-armed — `ArmClockTimer`): clock text. Armed for the next
  second/minute boundary rather than polled, and **killed outright while
  hidden** — nothing is on screen to keep current, so the redraw happens on the
  way back in instead (`StartSlide` → `RefreshClockText`).
- **Timer 3** (16ms, `Config::refreshRate`): animation tick — smooth slide up/down.

While manually hidden all three timers are off: the process sleeps in
`GetMessage` until a tray click or a WinEvent.

### The one rule for the clock text
`RecalculateAll` is the only function that draws, so it is the only function
that writes `g_lastRendered` — the string currently on screen. Do not keep a
second "last time I saw" copy at a call site; that is what the pre-v1.7 code did
and the two drifted. `RefreshClockText()` is the only correct way to ask for an
update: it renders when the string changed **or** when the surface is gone, and
is a no-op otherwise.

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

### Build flags are load-bearing (all measured)
| Flag | Why |
|---|---|
| `-Os` | The build previously had **no** `-O` flag at all. Smaller code means fewer resident pages *and* faster execution — it paid for the aggressive trim on its own. |
| `-fno-exceptions -fno-rtti` | Nothing here throws, catches, or uses `typeid`/`dynamic_cast`. Drops most of the static C++ runtime: 270 KB → 166 KB. |
| `-ffunction-sections -fdata-sections -Wl,--gc-sections -s` | Discards unreferenced code and symbols. |

Exe size across the three steps: **688 KB → 270 KB → 166 KB** (−76%).

### Memory strategy (measured, do not "optimise" further without re-measuring)
`SetProcessWorkingSetSize(-1, -1)` runs immediately after every render (inside
`RecalculateAll`) and when the clock goes hidden. Because a render happens at
most once a minute, the pages it returns are only faulted back once a minute.
Exception: formats that show seconds (`FormatHasSeconds`: `%S`, `%X`, `%c`)
render every second, so they skip the per-render trim and only trim on park.

**Rejected after measurement:** shutting GDI+ down between renders
(`GdiplusShutdown` + restart per minute). It saved only ~90 KB — the trim already
returns GDI+'s heap pages — while nearly doubling CPU (78 ms → 141 ms per 150 s).
`GdiplusEnsure()` remains as a one-time lazy init; there is deliberately no
matching release.

**Untried, and a real trade-off:** replacing GDI+ with plain GDI
(`BeginPath`/`StrokeAndFillPath`). It would remove gdiplus.dll's resident heap
entirely, but GDI does not antialias paths the way GDI+ does, so glyph quality
would drop unless rendering at 4× and downsampling — which spends back the CPU
and memory saved. Soft Shadow/Glow would need reimplementing with `ExtCreatePen`.

### Checks before a release
Run `check.bat` (`check.bat --no-pause` when scripted; `build.bat` takes the
same switch). It is free, deterministic and offline — four stages:
1. `-Wall -Wextra` syntax pass
2. stricter pass: `-Wshadow -Wformat=2 -Wnull-dereference -Wcast-align` at `-O2`
   (`-Wnull-dereference` needs optimisation to see the data flow)
3. GDI/handle pairing audit (`GetDC`/`ReleaseDC`, `CreateCompatibleDC`/`DeleteDC`,
   `CreatePopupMenu`/`DestroyMenu` — the last may legitimately differ because
   `DestroyMenu` recursively frees attached submenus)
4. `tests/selftest.cpp` — `#include`s the real `EdgeClock.cpp` and asserts the
   logic that has regressed before: work-area placement, taskbar overlap,
   time-format validation, Thai day names, clamping. Add a case when touching
   any of these.

Notes: `g++ -fanalyzer` is a no-op here — GCC's static analyzer only supports C,
not C++. `cppcheck`/`clang-tidy` are not installed on this machine; if added they
would slot into `check.bat` as a fifth stage. Keep both `.bat` files pure ASCII
with CRLF endings: a UTF-8 em dash in a `REM` line once made `cmd` print
`'M' is not recognized`.

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
| `check.bat` | Pre-release checks (4 stages, see above) |
| `tests/selftest.cpp` | Offline logic self-test, run by `check.bat` |
| `clock_23989.ico` | Application icon — 16/20/24/32/40/48/64/128 px, so the tray gets an exact size at every DPI |

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
- Log path is `%LOCALAPPDATA%\EdgeClock\EdgeClock_Log.txt`, timestamped (next to the exe failed silently under Program Files)
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

## v1.7 — stale-time fix + idle footprint (2026-08-03)
**The bug**: after any hidden period the clock slid back in showing the time
from when it hid, for up to a full minute. v1.6 moved the text timer to
boundary-arming, and `StartSlide(STATE_SLIDING_UP)` only *armed* that timer —
animation frames re-blit the cached surface, so nothing redrew until the next
boundary. Fixed by rendering on the way in (`RefreshClockText`), not scheduling.

**Leap second could freeze the clock for 24 days**: `ArmClockTimer` computed
`(60 - st.wSecond) * 1000 - st.wMilliseconds` in `UINT`. Windows reports
`wSecond == 60` during a leap second when leap-second support is enabled, so the
expression wrapped to ~4.29e9 ms and `SetTimer` clamped it to
`USER_TIMER_MAXIMUM` (24.8 days). Now computed signed and clamped to
[50, 61000] — the largest legitimate value is 60035, so the clamp only ever
catches a slip.

**Idle footprint**: timer 2 is killed while hidden (was 1 pointless wakeup/min),
and the surface DIB is freed when the clock parks hidden and rebuilt on the way
in. The DIB is `w * h * 4` — ~7 KB at the default 24 px, ~400 KB at the 200 px
maximum, so this scales with font size. The boundary guard also went 20 ms → 35 ms
(past the ~15.6 ms timer granularity) so an early fire no longer costs a
re-arm tick.

**Ordering that matters**: the park cleanup (`FreeSurface`) runs *after* the
final `PresentClock` of the slide-down. Freeing first makes that call a no-op
(`PresentClock` bails on a null surface) and strands the clock one frame short
of hidden.

**Also**: `WM_POWERBROADCAST` (`PBT_APMRESUMEAUTOMATIC`/`PBT_APMRESUMESUSPEND`)
corrects the text on resume instead of waiting for a timer armed against
pre-sleep ticks — the sibling of the existing `WM_TIMECHANGE` handler.

**Deliberately not done**: unhooking the WinEvent hooks while manually hidden.
`EvaluateVisibility` early-returns in two compares in that state, so the saving
is noise, and it adds a way for the clock to never come back.

## v1.7.2 — review fixes (2026-11-22)
**Placement is anchored to the work area** (`g_work`), not the monitor rect.
Identical with an auto-hide taskbar (verified: work area == monitor here). With
an always-visible taskbar the clock now sits beside it; before, the bar counted
as "raised" forever and a bottom-corner clock never appeared.

**Taskbar hide is one overlap test** (`TrayCoversClock`): the bar's rect,
shrunk by `taskbarThreshold` so the auto-hidden 1-2 px sliver does not count,
intersected with the clock's visible rect. Covers every docking edge — a raised
right-hand bar over a right-corner clock used to be ignored.

**Fullscreen is scoped to the clock's monitor.** `CoversClockMonitor` (exact
`EqualRect` with `g_mon` — "contains" would match every maximised window, which
overhangs by its border) runs per tick on the foreground window. The throttled
slot (foreground change / every 5th tick) adds `SHQueryUserNotificationState`
— D3D only counts if the foreground window is on our monitor; presentation
mode stays global; `QUNS_BUSY` is ignored (measured: Windows keeps reporting
it while a fullscreen game sits minimised, so the clock stayed hidden after
alt-tabbing out; the rect checks catch real borderless fullscreen) — and `TopWindowCoversClockMonitor`, a z-order walk for a
fullscreen window on our monitor while focus is elsewhere. It uses rect tests,
not `WindowFromPoint` (which sends `WM_NCHITTEST` cross-process and can stall
on a hung app), and skips tool/no-activate/click-through/cloaked windows so
overlays never count. Measured: no CPU change at the 15.6 ms resolution.

**Settings dialog**: created on the cursor's monitor (DPI is read in
`WM_CREATE`, so `CW_USEDEFAULT` on the primary gave the wrong scale) and
hidden until laid out; handles `WM_DPICHANGED` by rescaling every child by
new/old (combos use their list height) and resizing via
`AdjustWindowRectExForDpi`. Custom format box: `EM_LIMITTEXT 31`, red when
invalid (`g_fmtInvalid`). Font picker: no `CF_EFFECTS` (its options were
ignored); its Bold/Italic now sets the Weight combo. Monitor combo entries
carry the monitor number as item data, so an unplugged saved monitor is listed
as "(not connected)" instead of silently becoming Auto on Save.

**Other**: tray icon via `LoadImage` at `SM_CXSMICON` (not `LoadIconMetric`:
static-linking it stops the exe loading without comctl32 v6 — measured
`STATUS_ENTRYPOINT_NOT_FOUND`). Thai mode sets `LC_TIME` to
`Thai_Thailand.874` (msvcrt rejects `th-TH`; English must be `"C"`, it rejects
`en-US` too) — `LC_NUMERIC` is deliberately untouched so `_wtof` keeps `.`.
`g_uiBusy` blocks tray clicks while Import/Export dialogs are up. Installer
removes the Run value on uninstall even when the app (not the installer task)
created it.

**Found by driving the real UI** (after the review fixes above):
- `TrayCoversClock` intersects the clock with `g_mon` first. A negative
  `offsetY` pushes the window past the screen edge, and that invisible strip
  "touched" the auto-hidden sliver, so the clock never came back. Covered by
  a self-test case.
- Arrowing onto "Custom…" in the Format list moved focus into the box and
  closed the list, so the next Enter hit Save. Focus now moves on
  `CBN_CLOSEUP` via `SetFocus` (`WM_NEXTDLGCTL` does nothing here: this is a
  plain window, not a dialog, so there is no dialog manager to handle it).
- `ImportSettings` returns 1/0/-1; a cancelled picker used to return true and
  announce "Settings imported."
- The Settings dialog may open without focus when another app holds the
  foreground lock (posted command, not a click): Windows refuses
  `SetForegroundWindow`. Harmless; a tray click is user input and is allowed.
