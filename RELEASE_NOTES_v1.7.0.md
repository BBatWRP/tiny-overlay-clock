# EdgeClock v1.7.0

A correctness release. The headline fix is a visible one — the clock could show
the wrong time after auto-hiding — and hunting it down turned up two more
defects in the same code path, one of which could stop the clock for weeks.
Idle footprint drops further as a side effect.

## 🐛 Fixes

**The clock showed a stale time after every hidden period.** After the clock
auto-hid — a fullscreen game, the taskbar rising, hovering the corner — it slid
back into view still showing the minute it hid at, and kept showing it until the
next minute rolled over. Up to a full minute wrong, 30 seconds on average.

v1.6 moved the text update onto a timer armed for the next minute boundary
rather than a poll. Coming back into view only *armed* that timer, and animation
frames re-present a cached surface rather than redrawing, so nothing actually
refreshed until the boundary arrived. The clock now redraws on the way in. It
skips the work when the text has not changed, so a quick hover away and back
still costs nothing.

**A leap second could freeze the clock for 24 days.** The timer delay was
computed as `(60 - wSecond) * 1000 - wMilliseconds` in unsigned arithmetic.
Windows reports `wSecond == 60` during a leap second when leap-second support is
enabled, which wrapped the expression to roughly 4.29 billion milliseconds;
`SetTimer` clamped that to its maximum of 24.8 days. The clock face would simply
stop. Now computed signed and clamped at both ends — the largest legitimate
delay is 60035 ms, so the clamp only ever catches a slip.

**Resume from sleep no longer shows pre-sleep time.** A timer armed before
suspend can be delivered late on resume under EcoQoS and idle priority. The
clock now corrects itself on the resume notification instead of waiting.

## ⚡ Idle footprint

- **The text timer stops entirely while hidden.** There is nothing on screen to
  keep current, so v1.6's once-a-minute wakeup was pure waste. The redraw
  happens on the way back into view instead.
- **The render surface is released when the clock parks hidden** and rebuilt on
  the way in. The bitmap is width × height × 4 bytes — a few KB at the default
  24 px, but around 400 KB at the 200 px maximum, so this scales with font size.
- **Hidden from the tray menu now runs no timers at all.** The process sleeps in
  its message loop until you ask for the clock back.
- The boundary guard went from 20 ms to 35 ms, past the ~15.6 ms system timer
  granularity, so an early fire no longer costs an extra re-arm.

Resident set while running sits around 0.5 MB.

## 🧹 Internals

The "time currently on screen" cache used to live as a static inside the timer
handler, while roughly ten other call sites re-rendered without touching it — so
the cache and the pixels could disagree. It is now owned by the single function
that draws, and is only updated once a render has actually succeeded, so a
failed draw retries on the next tick instead of claiming a string that was never
painted.

## 📦 Downloads

- `EdgeClockSetup_v1.7.exe` — Windows installer (recommended; upgrades any
  existing install in place)
- `EdgeClock.exe` — portable, no installation needed
