// EdgeClock self-test: deterministic, offline, no windows created.
// Compiles the real EdgeClock.cpp into this file (its WinMain is simply
// unused under -mconsole) and checks the logic that has bitten before:
// placement, taskbar overlap, time-format validation, Thai date names.
// Run via check.bat (stage 4), or:
//   g++ tests\selftest.cpp -o selftest.exe -mconsole -lgdi32 -luser32 -lgdiplus
//       -lcomdlg32 -lole32 -luuid -ldwmapi -lcomctl32 -luxtheme -static
#include "../EdgeClock.cpp"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void SetMonitor(RECT mon, RECT work) { g_mon = mon; g_work = work; }

static bool TrayHides(int l, int t, int r, int b) {
    targetY = VisibleY();
    RECT tray = { l, t, r, b };
    return TrayCoversClock(tray);
}

int main() {
    const RECT mon = { 0, 0, 1920, 1080 };
    clockSize.cx = 63; clockSize.cy = 28;
    Config::offsetX = 0; Config::offsetY = 0;
    Config::taskbarThreshold = 4;

    // --- Placement: auto-hide taskbar (work area == monitor) -------------
    SetMonitor(mon, mon);
    Config::corner = 0; // bottom-right
    CHECK(ClockX() == 1857);
    CHECK(VisibleY() == 1052);
    CHECK(HiddenY() == 1080);
    Config::corner = 3; // top-left
    CHECK(ClockX() == 0);
    CHECK(VisibleY() == 0);
    CHECK(HiddenY() == -28);

    // --- Placement: always-visible bottom taskbar -> sits above it -------
    RECT work = { 0, 0, 1920, 1032 };
    SetMonitor(mon, work);
    Config::corner = 0;
    CHECK(VisibleY() == 1032 - 28);
    CHECK(HiddenY() == 1032);

    // --- Taskbar overlap ---------------------------------------------------
    SetMonitor(mon, mon);
    Config::corner = 0;
    CHECK(!TrayHides(0, 1078, 1920, 1126)); // auto-hidden: 2 px sliver on screen
    CHECK( TrayHides(0, 1032, 1920, 1080)); // raised bottom bar
    CHECK( TrayHides(1872, 0, 1920, 1080)); // raised RIGHT bar over a right corner
    CHECK(!TrayHides(0, 0, 48, 1080));      // raised left bar, clock is on the right
    Config::corner = 2;                     // top-right
    CHECK(!TrayHides(0, 1032, 1920, 1080)); // bottom bar cannot cover a top corner
    CHECK( TrayHides(0, 0, 1920, 48));      // top bar can

    // Negative offset pushes part of the clock below the screen edge. That
    // off-screen part must not "touch" the auto-hidden sliver (found on a real
    // setup with OffsetY = -6: the clock never came back).
    Config::corner = 0;
    Config::offsetY = -6.0f;
    CHECK(!TrayHides(0, 1078, 1920, 1138));
    CHECK( TrayHides(0, 1032, 1920, 1080));
    Config::offsetY = 0.0f;

    SetMonitor(mon, work);                  // always-visible bottom bar:
    Config::corner = 0;                     // clock sits beside it, never hides
    CHECK(!TrayHides(0, 1032, 1920, 1080));

    SetMonitor(mon, mon);
    Config::taskbarThreshold = 0;           // threshold 0: the sliver counts
    CHECK( TrayHides(0, 1078, 1920, 1126));
    Config::taskbarThreshold = 4;

    // --- Time formats --------------------------------------------------------
    CHECK( IsValidTimeFormat(L"%H:%M"));
    CHECK( IsValidTimeFormat(L"%#I:%M %p"));
    CHECK( IsValidTimeFormat(L"100%%"));
    CHECK(!IsValidTimeFormat(L""));
    CHECK(!IsValidTimeFormat(L"%"));
    CHECK(!IsValidTimeFormat(L"%#"));
    CHECK(!IsValidTimeFormat(L"%Q"));
    lstrcpyW(Config::timeFormat, L"%H:%M");    CHECK(!FormatHasSeconds());
    lstrcpyW(Config::timeFormat, L"%#S");      CHECK( FormatHasSeconds());
    lstrcpyW(Config::timeFormat, L"%c");       CHECK( FormatHasSeconds());

    // --- Localised day names -------------------------------------------------
    WCHAR out[64];
    lstrcpyW(Config::timeFormat, L"%a");
    Config::language = 2; ApplyLanguage();
    GetTime(out, 64);
    CHECK(out[0] >= 0x0E00 && out[0] <= 0x0E7F); // Thai block
    Config::language = 1; ApplyLanguage();
    GetTime(out, 64);
    CHECK(out[0] >= L'A' && out[0] <= L'Z');     // "Mon".."Sun"

    // --- Clamping ------------------------------------------------------------
    Config::monitorIndex = EC_MAX_MONITORS;     ClampConfig(); CHECK(Config::monitorIndex == EC_MAX_MONITORS);
    Config::monitorIndex = EC_MAX_MONITORS + 1; ClampConfig(); CHECK(Config::monitorIndex == 0);

    if (g_fail) { printf("%d check(s) failed\n", g_fail); return 1; }
    printf("selftest: all checks passed\n");
    return 0;
}
