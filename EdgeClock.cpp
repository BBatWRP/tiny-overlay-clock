#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <tchar.h>
#include <ctime>
#include <stdio.h> // For file logging
#include <locale.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlobj.h>
#include <objbase.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif


using namespace Gdiplus;

// =============================================================
//   CONFIGURATION
// =============================================================
static HANDLE g_hMutex = NULL; // Global mutex for single-instance

// Appends a timestamped line to %LOCALAPPDATA%\EdgeClock\EdgeClock_Log.txt.
// Not next to the exe: an installed copy lives in Program Files, which a
// normal user cannot write, so logging there silently did nothing. The file is
// rotated (truncated) once per process when it exceeds kLogMaxBytes so it can
// never grow without bound across launches.
void Log(const char* msg) {
    static const DWORD kLogMaxBytes = 32768;
    static char logPath[MAX_PATH] = {0};
    static bool rotateChecked = false;
    const char* mode = "a";

    if (!logPath[0]) {
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, logPath)) &&
            strlen(logPath) + 32 < MAX_PATH) {
            strcat(logPath, "\\EdgeClock");
            CreateDirectoryA(logPath, NULL); // Fine if it already exists
            strcat(logPath, "\\EdgeClock_Log.txt");
        } else {
            // Fallback: next to the exe (works for a portable copy)
            GetModuleFileNameA(NULL, logPath, MAX_PATH);
            char* lastSlash = strrchr(logPath, '\\');
            if (lastSlash) strcpy(lastSlash + 1, "EdgeClock_Log.txt");
            else strcpy(logPath, "EdgeClock_Log.txt");
        }
    }

    if (!rotateChecked) {
        rotateChecked = true;
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(logPath, GetFileExInfoStandard, &fad) &&
            fad.nFileSizeHigh == 0 && fad.nFileSizeLow > kLogMaxBytes) {
            mode = "w"; // Start a fresh log
        }
    }

    FILE* f = fopen(logPath, mode);
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "%04u-%02u-%02u %02u:%02u:%02u  %s\n", st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, msg);
        fclose(f);
    }
}

// Factory defaults — the one place they are written. Used to initialise
// Config, by ClampConfig() to repair bad values, and by the Settings dialog's
// "Defaults" button.
namespace Defaults {
    const float    fontSize = 14.0f;
    const WCHAR    fontName[] = L"Segoe UI";
    const float    outlineWidth = 2.0f;
    const COLORREF textColor = RGB(255, 255, 255);
    const COLORREF outlineColor = RGB(0, 0, 0);
    const int      animDuration = 500;
    const int      taskbarThreshold = 4;
    const WCHAR    timeFormat[] = L"%H:%M";
    const int      opacity = 100;
    const int      fontWeight = 1;
    const int      fxIntensity = 100;
}

namespace Config {
    // --- Appearance Settings ---
    float fontSize = Defaults::fontSize;
    WCHAR fontName[32] = L"Segoe UI";  // = Defaults::fontName (arrays can't copy-init)
    float outlineWidth = Defaults::outlineWidth;
    COLORREF textColor = Defaults::textColor;
    COLORREF outlineColor = Defaults::outlineColor;

    // --- Animation & Performance ---
    int animDuration = Defaults::animDuration; // Slide duration in ms
    int refreshRate = 16;         // Animation tick interval in ms (~60fps)

    // --- Tray Icon ---
    const TCHAR trayTooltip[] = _T("Edge Clock");

    // --- Auto-Hide Logic ---
    int taskbarThreshold = Defaults::taskbarThreshold;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // --- Clock Text ---
    WCHAR timeFormat[32] = L"%H:%M"; // = Defaults::timeFormat; strftime format
    int opacity = Defaults::opacity;         // Clock opacity in percent (20-100)
    int effectMode = 0;                      // 0 = Outline, 1 = Soft Shadow, 2 = Glow, 3 = Pill
    int fontWeight = Defaults::fontWeight;   // 0 = Regular, 1 = Bold, 2 = Italic, 3 = Bold Italic
    int fxIntensity = Defaults::fxIntensity; // Effect strength in percent (10-200)

    // --- Placement ---
    int corner = 0;                  // 0 = BR, 1 = BL, 2 = TR, 3 = TL
    int monitorIndex = 0;            // 0 = Auto (taskbar monitor), 1..N = explicit

    // --- UI ---
    int language = 0;                // 0 = Auto (from OS), 1 = English, 2 = Thai

    // --- Menu Presets ---
    // Small is the default size, so a fresh install shows a ticked preset.
    const float sizeSmall = Defaults::fontSize;
    const float sizeMedium = 24.0f;
    const float sizeLarge = 32.0f;              
}



void SaveConfig() {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\EdgeClock"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, _T("FontSize"), 0, REG_BINARY, (const BYTE*)&Config::fontSize, sizeof(float));
        RegSetValueEx(hKey, _T("OutlineWidth"), 0, REG_BINARY, (const BYTE*)&Config::outlineWidth, sizeof(float));
        RegSetValueEx(hKey, _T("AnimDuration"), 0, REG_DWORD, (const BYTE*)&Config::animDuration, sizeof(int));
        RegSetValueEx(hKey, _T("TaskbarThreshold"), 0, REG_DWORD, (const BYTE*)&Config::taskbarThreshold, sizeof(int));
        RegSetValueEx(hKey, _T("OffsetX"), 0, REG_BINARY, (const BYTE*)&Config::offsetX, sizeof(float));
        RegSetValueEx(hKey, _T("OffsetY"), 0, REG_BINARY, (const BYTE*)&Config::offsetY, sizeof(float));
        RegSetValueEx(hKey, _T("TextColor"), 0, REG_DWORD, (const BYTE*)&Config::textColor, sizeof(COLORREF));
        RegSetValueEx(hKey, _T("OutlineColor"), 0, REG_DWORD, (const BYTE*)&Config::outlineColor, sizeof(COLORREF));
        RegSetValueEx(hKey, _T("FontName"), 0, REG_SZ, (const BYTE*)Config::fontName, (lstrlenW(Config::fontName) + 1) * sizeof(WCHAR));
        RegSetValueEx(hKey, _T("TimeFormat"), 0, REG_SZ, (const BYTE*)Config::timeFormat, (lstrlenW(Config::timeFormat) + 1) * sizeof(WCHAR));
        RegSetValueEx(hKey, _T("Opacity"), 0, REG_DWORD, (const BYTE*)&Config::opacity, sizeof(int));
        RegSetValueEx(hKey, _T("EffectMode"), 0, REG_DWORD, (const BYTE*)&Config::effectMode, sizeof(int));
        RegSetValueEx(hKey, _T("FontWeight"), 0, REG_DWORD, (const BYTE*)&Config::fontWeight, sizeof(int));
        RegSetValueEx(hKey, _T("FxIntensity"), 0, REG_DWORD, (const BYTE*)&Config::fxIntensity, sizeof(int));
        RegSetValueEx(hKey, _T("Corner"), 0, REG_DWORD, (const BYTE*)&Config::corner, sizeof(int));
        RegSetValueEx(hKey, _T("MonitorIndex"), 0, REG_DWORD, (const BYTE*)&Config::monitorIndex, sizeof(int));
        RegSetValueEx(hKey, _T("Language"), 0, REG_DWORD, (const BYTE*)&Config::language, sizeof(int));
        RegCloseKey(hKey);
    }
}

// Validates a strftime format string: every '%' must be followed by a
// known specifier. Prevents msvcrt's invalid-parameter handler from
// aborting on garbage formats.
bool IsValidTimeFormat(const WCHAR* f) {
    if (!f || !*f) return false;
    const WCHAR* allowed = L"aAbBcdHIjmMpSUwWxXyYzZ%";
    for (const WCHAR* p = f; *p; ++p) {
        if (*p == L'%') {
            ++p;
            if (*p == L'#') ++p; // msvcrt's no-leading-zero flag (e.g. %#I)
            if (!*p || !wcschr(allowed, *p)) return false;
        }
    }
    return true;
}

// Forces every setting into its valid range. Called after loading from the
// registry and after importing a settings file.
// Float checks are written as !(in range) so NaN — which fails every
// comparison — is caught too (an imported "FontSize=nan" used to slip through).
void ClampConfig() {
    // Out-of-range clamps to the nearest bound (so typing "1" on the way to
    // "12" previews at 4, not a jump to the default); only NaN resets.
    if (Config::fontSize != Config::fontSize) Config::fontSize = Defaults::fontSize;
    if (Config::fontSize < 4.0f) Config::fontSize = 4.0f;
    if (Config::fontSize > 200.0f) Config::fontSize = 200.0f;
    if (!(Config::outlineWidth >= 0.0f)) Config::outlineWidth = 0.0f;
    if (Config::outlineWidth > 50.0f) Config::outlineWidth = 50.0f;
    // Offsets beyond any real monitor only push the clock off-screen, and a
    // huge or NaN value is undefined behaviour once cast to int for placement.
    if (!(Config::offsetX >= -5000.0f && Config::offsetX <= 5000.0f)) Config::offsetX = 0.0f;
    if (!(Config::offsetY >= -5000.0f && Config::offsetY <= 5000.0f)) Config::offsetY = 0.0f;
    if (Config::taskbarThreshold < 0 || Config::taskbarThreshold > 64) Config::taskbarThreshold = Defaults::taskbarThreshold;
    if (Config::animDuration < 0) Config::animDuration = Defaults::animDuration;
    if (Config::animDuration > 5000) Config::animDuration = 5000;
    if (Config::opacity < 20) Config::opacity = 20;
    if (Config::opacity > 100) Config::opacity = 100;
    if (Config::effectMode < 0 || Config::effectMode > 3) Config::effectMode = 0;
    if (Config::fontWeight < 0 || Config::fontWeight > 3) Config::fontWeight = Defaults::fontWeight;
    if (Config::fxIntensity < 10) Config::fxIntensity = 10;
    if (Config::fxIntensity > 200) Config::fxIntensity = 200;
    if (Config::corner < 0 || Config::corner > 3) Config::corner = 0;
    if (Config::monitorIndex < 0 || Config::monitorIndex > 8) Config::monitorIndex = 0;
    if (Config::language < 0 || Config::language > 2) Config::language = 0;
    Config::fontName[31] = L'\0';
    Config::timeFormat[31] = L'\0';
    if (!IsValidTimeFormat(Config::timeFormat)) lstrcpyW(Config::timeFormat, Defaults::timeFormat);
    if (!Config::fontName[0]) lstrcpyW(Config::fontName, Defaults::fontName);
}

// Registry readers: each call gets its own size and an exact type, and the
// target is only written on success. A missing, mistyped or oversized value
// leaves the default in place instead of overrunning the neighbouring global.
static void RegReadDword(HKEY k, LPCTSTR name, void* out) {
    DWORD v, sz = sizeof(v);
    if (RegGetValue(k, NULL, name, RRF_RT_REG_DWORD, NULL, &v, &sz) == ERROR_SUCCESS)
        memcpy(out, &v, sizeof(v));
}

static void RegReadFloat(HKEY k, LPCTSTR name, float* out) {
    float v; DWORD sz = sizeof(v);
    if (RegGetValue(k, NULL, name, RRF_RT_REG_BINARY, NULL, &v, &sz) == ERROR_SUCCESS &&
        sz == sizeof(v))
        *out = v;
}

static void RegReadStr32(HKEY k, LPCTSTR name, WCHAR* out) {
    WCHAR v[32]; DWORD sz = sizeof(v);
    if (RegGetValue(k, NULL, name, RRF_RT_REG_SZ, NULL, v, &sz) == ERROR_SUCCESS)
        lstrcpynW(out, v, 32);
}

void LoadConfig() {
    Log("Loading Config...");
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\EdgeClock"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegReadFloat(hKey, _T("FontSize"), &Config::fontSize);
        RegReadFloat(hKey, _T("OutlineWidth"), &Config::outlineWidth);
        RegReadFloat(hKey, _T("OffsetX"), &Config::offsetX);
        RegReadFloat(hKey, _T("OffsetY"), &Config::offsetY);

        RegReadDword(hKey, _T("AnimDuration"), &Config::animDuration);
        RegReadDword(hKey, _T("TaskbarThreshold"), &Config::taskbarThreshold);
        RegReadDword(hKey, _T("TextColor"), &Config::textColor);
        RegReadDword(hKey, _T("OutlineColor"), &Config::outlineColor);
        RegReadDword(hKey, _T("Opacity"), &Config::opacity);
        RegReadDword(hKey, _T("EffectMode"), &Config::effectMode);
        RegReadDword(hKey, _T("FontWeight"), &Config::fontWeight);
        RegReadDword(hKey, _T("FxIntensity"), &Config::fxIntensity);
        RegReadDword(hKey, _T("Corner"), &Config::corner);
        RegReadDword(hKey, _T("MonitorIndex"), &Config::monitorIndex);
        RegReadDword(hKey, _T("Language"), &Config::language);

        RegReadStr32(hKey, _T("FontName"), Config::fontName);
        RegReadStr32(hKey, _T("TimeFormat"), Config::timeFormat);

        RegCloseKey(hKey);
    }

    ClampConfig();
    Log("Config Loaded.");
}
// =============================================================
//   LOCALIZATION (English / Thai)
// =============================================================
enum StrId {
    S_TITLE, S_SEC_TEXT, S_SEC_EFFECT, S_SEC_BEHAVIOR, S_SEC_POSITION,
    S_FONT, S_CHOOSE, S_SIZE, S_WEIGHT, S_TEXT_COLOR, S_FORMAT, S_CUSTOM,
    S_EFFECT, S_FX_WIDTH, S_FX_COLOR, S_POWER, S_OPACITY,
    S_ANIM_SPEED, S_CORNER, S_MONITOR, S_OFFSET_X, S_OFFSET_Y,
    S_SAVE, S_DEFAULTS, S_CANCEL,
    S_FX_OUTLINE, S_FX_SHADOW, S_FX_GLOW, S_FX_PILL,
    S_W_REGULAR, S_W_BOLD, S_W_ITALIC, S_W_BOLDITALIC,
    S_CNR_BR, S_CNR_BL, S_CNR_TR, S_CNR_TL,
    S_AUTO, S_MONITOR_N, S_CUSTOM_OPT,
    S_FMT_24, S_FMT_24S, S_FMT_12, S_FMT_12S, S_FMT_DATE,
    S_MENU_SIZE_S, S_MENU_SIZE_M, S_MENU_SIZE_L, S_MENU_SETTINGS,
    S_MENU_STARTUP, S_MENU_HIDE, S_MENU_SHOW, S_MENU_EXIT,
    S_MENU_LANG, S_LANG_AUTO, S_LANG_EN, S_LANG_TH,
    S_MENU_EXPORT, S_MENU_IMPORT,
    S_FILE_FILTER, S_IMPORT_OK, S_IMPORT_FAIL, S_EXPORT_FAIL,
    S_MON_MISSING,
    S_COUNT
};

static const WCHAR* kStrEN[S_COUNT] = {
    L"EdgeClock Settings", L"TEXT", L"EFFECT", L"BEHAVIOR", L"POSITION",
    L"Font", L"Choose…", L"Size (px)", L"Weight", L"Color", L"Format", L"Custom",
    L"Effect", L"Width", L"Color", L"Power", L"Opacity",
    L"Slide time", L"Corner", L"Monitor", L"Offset X", L"Y",
    L"Save", L"Defaults", L"Cancel",
    L"Outline", L"Soft Shadow", L"Glow", L"Pill",
    L"Regular", L"Bold", L"Italic", L"Bold Italic",
    L"Bottom-Right", L"Bottom-Left", L"Top-Right", L"Top-Left",
    L"Auto", L"Monitor %d", L"Custom…",
    L"24-hour  (14:35)", L"24-hour + seconds", L"12-hour  (2:35 PM)",
    L"12-hour + seconds", L"Date + time",
    L"Size: Small", L"Size: Normal", L"Size: Large", L"Settings…",
    L"Run on Startup", L"Hide Clock", L"Show Clock", L"Exit",
    L"Language", L"Auto (system)", L"English", L"Thai",
    L"Export Settings…", L"Import Settings…",
    L"EdgeClock settings", L"Settings imported.", L"Could not read that settings file.",
    L"Could not write the settings file.",
    L"%s (not connected)"
};

static const WCHAR* kStrTH[S_COUNT] = {
    L"ตั้งค่า EdgeClock", L"ข้อความ", L"เอฟเฟกต์", L"พฤติกรรม", L"ตำแหน่ง",
    L"ฟอนต์", L"เลือก…", L"ขนาด (px)", L"น้ำหนัก", L"สี", L"รูปแบบ", L"กำหนดเอง",
    L"เอฟเฟกต์", L"ความหนา", L"สี", L"ความเข้ม", L"ความทึบ",
    L"เวลาเลื่อน", L"มุมจอ", L"จอภาพ", L"ระยะ X", L"Y",
    L"บันทึก", L"ค่าเริ่มต้น", L"ยกเลิก",
    L"เส้นขอบ", L"เงานุ่ม", L"เรืองแสง", L"พื้นหลังแคปซูล",
    L"ปกติ", L"หนา", L"เอียง", L"หนาเอียง",
    L"ขวาล่าง", L"ซ้ายล่าง", L"ขวาบน", L"ซ้ายบน",
    L"อัตโนมัติ", L"จอที่ %d", L"กำหนดเอง…",
    L"24 ชม.  (14:35)", L"24 ชม. + วินาที", L"12 ชม.  (2:35 PM)",
    L"12 ชม. + วินาที", L"วันที่ + เวลา",
    L"ขนาด: เล็ก", L"ขนาด: ปกติ", L"ขนาด: ใหญ่", L"ตั้งค่า…",
    L"เปิดพร้อม Windows", L"ซ่อนนาฬิกา", L"แสดงนาฬิกา", L"ออก",
    L"ภาษา", L"อัตโนมัติ (ตามระบบ)", L"English", L"ไทย",
    L"ส่งออกการตั้งค่า…", L"นำเข้าการตั้งค่า…",
    L"ไฟล์ตั้งค่า EdgeClock", L"นำเข้าการตั้งค่าแล้ว", L"อ่านไฟล์ตั้งค่าไม่ได้",
    L"เขียนไฟล์ตั้งค่าไม่ได้",
    L"%s (ไม่ได้เชื่อมต่อ)"
};

static const WCHAR** g_str = kStrEN;

// Resolves Config::language (0 = follow the OS UI language) into the table
// used by L(). Called at startup and whenever the user switches language.
void ApplyLanguage() {
    int lang = Config::language;
    if (lang == 0) {
        lang = ((GetUserDefaultUILanguage() & 0x3FF) == 0x1E /*LANG_THAI*/) ? 2 : 1;
    }
    g_str = (lang == 2) ? kStrTH : kStrEN;

    // Day/month names (%a %A %b %B) and %c/%x come from the CRT's LC_TIME,
    // which is "C" unless set — so Thai mode used to show "Sun 22 Nov".
    // Thai uses the msvcrt locale name (it rejects "th-TH"; measured), which
    // also gives the Buddhist-era year in %c/%x, as Thai readers expect.
    // English is "C", not "en-US": msvcrt rejects that name too.
    if (lang == 2 && _wsetlocale(LC_TIME, L"Thai_Thailand.874")) return;
    _wsetlocale(LC_TIME, L"C");
}

inline const WCHAR* L(int id) { return g_str[id]; }
// =============================================================

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1001

#define ID_MENU_EXIT 2001
#define ID_SIZE_SMALL 2002
#define ID_SIZE_MEDIUM 2003
#define ID_SIZE_LARGE 2004
#define ID_MENU_TOGGLE_HIDE 2006 
#define ID_MENU_STARTUP 2007 
#define ID_MENU_SETTINGS 2008
#define ID_MENU_EXPORT 2009
#define ID_MENU_IMPORT 2010
#define ID_LANG_AUTO 2011
#define ID_LANG_EN 2012
#define ID_LANG_TH 2013

#define ID_BTN_TEXT_COLOR 3001
#define ID_BTN_OUTLINE_COLOR 3002
#define ID_EDIT_FONT_SIZE 3003
#define ID_EDIT_OUTLINE_WIDTH 3004
#define ID_BTN_SAVE 3005
#define ID_BTN_CANCEL 3006
#define ID_BTN_ID_FONT 3007
#define ID_EDIT_OFFSET_X 3008
#define ID_EDIT_OFFSET_Y 3009
#define ID_EDIT_ANIM_SPEED 3010
#define ID_STATIC_FONT_NAME 3011
#define ID_TRACK_ANIM_SPEED 3012
#define ID_STATIC_DURATION_VAL 3013
#define ID_BTN_RESET 3014
#define ID_EDIT_TIME_FORMAT 3015
#define ID_TRACK_OPACITY 3016
#define ID_STATIC_OPACITY_VAL 3017
#define ID_COMBO_EFFECT 3018
#define ID_COMBO_FORMAT 3019
#define ID_COMBO_WEIGHT 3020
#define ID_TRACK_FXPOWER 3021
#define ID_STATIC_FXPOWER_VAL 3022
#define ID_COMBO_CORNER 3023
#define ID_COMBO_MONITOR 3024

// Static controls in these ID ranges get special colouring:
// section headers use an accent text colour, separators are painted as
// 1px lines by returning a grey brush from WM_CTLCOLORSTATIC.
#define ID_SECTION_FIRST 3100
#define ID_SECTION_LAST  3119
#define ID_SEP_FIRST     3120
#define ID_SEP_LAST      3139

enum AnimState {
    STATE_VISIBLE,
    STATE_HIDDEN,
    STATE_SLIDING_UP,
    STATE_SLIDING_DOWN
};

AnimState currentState = STATE_SLIDING_UP;
// Floating point state for precise animation
float currentYVal = 0.0f;
int targetY = 0;
SIZE clockSize = {0, 0};
bool manualHidden = false;

// Time-based animation state (immune to timer jitter under IDLE priority)
ULONGLONG animStartTick = 0;
float animStartY = 0.0f;
int g_animAlpha = 100; // Alpha (percent of Config::opacity) for the current frame

UINT g_msgTaskbarCreated = 0; // "TaskbarCreated" broadcast (Explorer restart)

ULONG_PTR gdiplusToken;
NOTIFYICONDATA nid = { 0 };

// --- Monitor geometry ---
#define EC_MAX_MONITORS 8
RECT g_mon = {0, 0, 0, 0};            // Rect of the monitor the clock lives on
RECT g_work = {0, 0, 0, 0};           // That monitor's work area (the clock's anchor)
HMONITOR g_hMonClock = NULL;          // That monitor's handle
RECT g_monRects[EC_MAX_MONITORS];
HMONITOR g_monHandles[EC_MAX_MONITORS];
int  g_monCount = 0;

BOOL CALLBACK MonEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM) {
    if (g_monCount < EC_MAX_MONITORS) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMon, &mi)) {
            g_monHandles[g_monCount] = hMon;
            g_monRects[g_monCount++] = mi.rcMonitor;
        }
    }
    return TRUE;
}

// Enumerates monitors and resolves which one hosts the clock: either the
// user's explicit choice or (Auto) the monitor hosting the taskbar.
void UpdateScreenMetrics() {
    g_monCount = 0;
    EnumDisplayMonitors(NULL, NULL, MonEnumProc, 0);

    HMONITOR hMon;
    if (Config::monitorIndex >= 1 && Config::monitorIndex <= g_monCount) {
        hMon = g_monHandles[Config::monitorIndex - 1];
    } else {
        HWND hTray = FindWindow(_T("Shell_TrayWnd"), NULL);
        if (hTray) {
            hMon = MonitorFromWindow(hTray, MONITOR_DEFAULTTOPRIMARY);
        } else {
            POINT pt = {0, 0};
            hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        }
    }
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(hMon, &mi)) {
        g_hMonClock = hMon;
        g_mon = mi.rcMonitor;
        g_work = mi.rcWork;
    } else {
        g_hMonClock = NULL;
        SetRect(&g_mon, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        g_work = g_mon;
    }
}

// --- Corner-aware placement -------------------------------------------------
// corner: 0 = bottom-right, 1 = bottom-left, 2 = top-right, 3 = top-left.
//
// The clock is anchored to the WORK AREA, not the raw monitor rect. With an
// auto-hide taskbar the two are identical, so nothing changes there. With an
// always-visible taskbar the work area stops at the taskbar's edge, so the
// clock sits right next to it instead of underneath it — where the old
// monitor-rect anchor made the taskbar count as "raised" forever and the clock
// never appeared at all.
inline bool ClockAtBottom() { return Config::corner == 0 || Config::corner == 1; }
inline bool ClockAtRight()  { return Config::corner == 0 || Config::corner == 2; }

// X of the clock window (absolute), honouring offsetX away from its edge.
int ClockX() {
    return ClockAtRight() ? g_work.right - clockSize.cx - (int)Config::offsetX
                          : g_work.left + (int)Config::offsetX;
}

// Y when fully visible.
int VisibleY() {
    return ClockAtBottom() ? g_work.bottom - clockSize.cy - (int)Config::offsetY
                           : g_work.top + (int)Config::offsetY;
}

// Y when fully hidden — just past the edge the clock slides toward. The fade
// reaches zero exactly here, so stopping at the work-area edge (on top of a
// permanent taskbar) leaves nothing visible.
int HiddenY() {
    return ClockAtBottom() ? g_work.bottom : g_work.top - clockSize.cy;
}

void GetTime(TCHAR* buffer, int size) {
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (_tcsftime(buffer, size, Config::timeFormat, timeinfo) == 0) {
        _tcsftime(buffer, size, _T("%H:%M"), timeinfo); // Fallback on bad format
    }
}

// Padding (per side) the current text effect needs around the glyphs so
// nothing clips at the window edge. Shadow/glow spread wider than a stroke.
float EffectPad() {
    switch (Config::effectMode) {
        case 1: // Soft shadow: offset + blur spread
        case 2: // Glow: halo radius
            return Config::outlineWidth * 2.0f + 2.0f;
        case 3: // Pill: breathing room inside the plate. outlineWidth is folded
                // in so the "FX Width" control still does something in this mode.
            return Config::fontSize * 0.30f + Config::outlineWidth + 3.0f;
        default: // Outline stroke
            return Config::outlineWidth;
    }
}

// --- Cached GDI+ font family -------------------------------------------------
// Constructing a FontFamily hits the font table each time; the clock re-renders
// on every minute tick and on every keystroke during live preview, so the
// family is cached and only rebuilt when the configured name changes.
FontFamily* g_pFamily = NULL;
WCHAR g_familyCached[32] = L"";

void FreeFontCache() {
    if (g_pFamily) { delete g_pFamily; g_pFamily = NULL; }
    g_familyCached[0] = L'\0';
}

// --- Why GDI+ stays initialised ---------------------------------------------
// Shutting GDI+ down between renders was measured and rejected: it saved only
// ~90 KB of working set (SetProcessWorkingSetSize already returns GDI+'s heap
// pages) while nearly doubling CPU, because GdiplusStartup/Shutdown runs every
// minute. GDI+ is therefore started once and the aggressive trim below does the
// memory work.
bool g_gdiplusUp = false;
extern bool g_settingsOpen; // Defined with the Settings dialog state below

void GdiplusEnsure() {
    if (g_gdiplusUp) return;
    GdiplusStartupInput input;
    if (GdiplusStartup(&gdiplusToken, &input, NULL) == Ok) g_gdiplusUp = true;
}

FontFamily* GetFontFamily() {
    if (g_pFamily && wcscmp(g_familyCached, Config::fontName) == 0) return g_pFamily;

    FreeFontCache();
    g_pFamily = new FontFamily(Config::fontName);
    if (g_pFamily->GetLastStatus() != Ok) {
        delete g_pFamily;
        g_pFamily = new FontFamily(L"Segoe UI");
        if (g_pFamily->GetLastStatus() != Ok) {
            delete g_pFamily;
            g_pFamily = new FontFamily(L"Arial");
        }
    }
    lstrcpynW(g_familyCached, Config::fontName, 32);
    return g_pFamily;
}

// Maps Config::fontWeight to a GDI+ FontStyle, falling back to a style the
// family actually ships (many fonts have no italic or no bold face).
INT FontStyleFor(FontFamily* fam) {
    INT style;
    switch (Config::fontWeight) {
        case 0:  style = FontStyleRegular; break;
        case 2:  style = FontStyleItalic; break;
        case 3:  style = FontStyleBoldItalic; break;
        default: style = FontStyleBold; break;
    }
    if (fam && !fam->IsStyleAvailable(style)) {
        if (fam->IsStyleAvailable(FontStyleRegular)) return FontStyleRegular;
        if (fam->IsStyleAvailable(FontStyleBold)) return FontStyleBold;
    }
    return style;
}

// Builds the glyph outline for the current time at the origin and returns its
// tight bounds. Both sizing and rendering use this single path, so the window
// is exactly as large as the ink plus effect padding (no wasted blit area).
// textOut receives the exact string that went into the path. The caller records
// it as "what is currently on screen", so it must come from this one GetTime()
// call — sampling the clock a second time could straddle a second boundary and
// leave the cache claiming a string that was never drawn.
bool BuildClockPath(GraphicsPath& path, RectF& bounds, TCHAR* textOut, int textCap) {
    TCHAR timeBuf[64];
    GetTime(timeBuf, 64);
    if (!timeBuf[0]) return false;
    if (textOut && textCap > 0) {
        _tcsncpy(textOut, timeBuf, textCap - 1);
        textOut[textCap - 1] = _T('\0');
    }

    FontFamily* fam = GetFontFamily();
    StringFormat format(StringFormat::GenericTypographic());
    PointF origin(0.0f, 0.0f);

    if (path.AddString(timeBuf, -1, fam, FontStyleFor(fam),
                       (REAL)Config::fontSize, origin, &format) != Ok) return false;
    if (path.GetBounds(&bounds) != Ok) return false;
    return bounds.Width > 0.0f && bounds.Height > 0.0f;
}

// --- Cached layered-window surface ------------------------------------------
// The rendered text lives in a 32bpp DIB. Sliding and fading only re-present
// this surface (a single UpdateLayeredWindow), so animation costs no GDI+ work.
HDC     g_hdcCache = NULL;
HBITMAP g_hbmCache = NULL;
HBITMAP g_hbmOld = NULL;
int     g_cacheW = 0, g_cacheH = 0;

void FreeSurface() {
    if (g_hdcCache) {
        if (g_hbmOld) SelectObject(g_hdcCache, g_hbmOld);
        DeleteDC(g_hdcCache);
        g_hdcCache = NULL;
        g_hbmOld = NULL;
    }
    if (g_hbmCache) { DeleteObject(g_hbmCache); g_hbmCache = NULL; }
    g_cacheW = g_cacheH = 0;
}

bool EnsureSurface(int w, int h) {
    if (g_hdcCache && g_cacheW == w && g_cacheH == h) return true;
    FreeSurface();

    HDC hdcScreen = GetDC(NULL);
    g_hdcCache = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // Top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    g_hbmCache = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);

    if (!g_hdcCache || !g_hbmCache) { FreeSurface(); return false; }

    g_hbmOld = (HBITMAP)SelectObject(g_hdcCache, g_hbmCache);
    g_cacheW = w;
    g_cacheH = h;
    return true;
}

// Blits the cached surface to the window at (x, y) with the given alpha.
// Also moves/resizes the window, so no separate SetWindowPos is needed.
void PresentClock(HWND hwnd, int x, int y, int alphaPercent) {
    if (!g_hdcCache || g_cacheW <= 0 || g_cacheH <= 0) return;
    if (alphaPercent < 0) alphaPercent = 0;
    if (alphaPercent > 100) alphaPercent = 100;

    POINT ptSrc = { 0, 0 };
    POINT ptDst = { x, y };
    SIZE size = { g_cacheW, g_cacheH };
    BLENDFUNCTION blend;
    ZeroMemory(&blend, sizeof(blend));
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = (BYTE)(alphaPercent * 255 / 100);
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, NULL, &ptDst, &size, g_hdcCache, &ptSrc, 0, &blend, ULW_ALPHA);
}

// Adds a rounded rectangle (used by the Pill effect) to a path.
void AddRoundRect(GraphicsPath& p, const RectF& r, float rad) {
    if (rad * 2.0f > r.Width) rad = r.Width / 2.0f;
    if (rad * 2.0f > r.Height) rad = r.Height / 2.0f;
    if (rad <= 0.5f) { p.AddRectangle(r); return; }
    float d = rad * 2.0f;
    p.AddArc(r.X, r.Y, d, d, 180.0f, 90.0f);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270.0f, 90.0f);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0.0f, 90.0f);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90.0f, 90.0f);
    p.CloseFigure();
}

inline BYTE ScaleAlpha(float base) {
    float a = base * (Config::fxIntensity / 100.0f);
    if (a < 0.0f) a = 0.0f;
    if (a > 255.0f) a = 255.0f;
    return (BYTE)a;
}

// Draws an already-built glyph path into the cached surface. The path is passed
// in (and mutated in place by the placement transform) so that building the
// outline — by far the most expensive step — happens exactly once per update.
void RenderClock(GraphicsPath& path, const RectF& b) {
    if (!g_hdcCache) return;

    Graphics graphics(g_hdcCache);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.Clear(Color(0, 0, 0, 0)); // Fully transparent

    // Shift the ink so it sits inset by pad on every side
    float pad = EffectPad();
    Matrix place;
    place.Translate(pad - b.X, pad - b.Y);
    path.Transform(&place);

    BYTE fxR = GetRValue(Config::outlineColor);
    BYTE fxG = GetGValue(Config::outlineColor);
    BYTE fxB = GetBValue(Config::outlineColor);
    SolidBrush brush(Color(255, GetRValue(Config::textColor),
                                GetGValue(Config::textColor),
                                GetBValue(Config::textColor)));

    float ew = Config::outlineWidth;
    if (ew < 0.5f) ew = 0.5f;

    switch (Config::effectMode) {
        case 1: { // Soft Shadow: blurred dark copy offset down-right, then clean text
            GraphicsPath* shadow = path.Clone();
            if (shadow) {
                Matrix m;
                float off = ew * 0.75f + 1.0f;
                m.Translate(off, off);
                shadow->Transform(&m);

                // Cheap gaussian-ish blur: stacked strokes, wide -> narrow, low alpha
                const int layers = 4;
                BYTE la = ScaleAlpha(22.0f);
                for (int i = layers; i >= 1; --i) {
                    Pen p(Color(la, fxR, fxG, fxB), ew * 2.0f * i / (float)layers);
                    p.SetLineJoin(LineJoinRound);
                    graphics.DrawPath(&p, shadow);
                }
                SolidBrush shadowFill(Color(ScaleAlpha(130.0f), fxR, fxG, fxB));
                graphics.FillPath(&shadowFill, shadow);
                delete shadow;
            }
            graphics.FillPath(&brush, &path);
            break;
        }
        case 2: { // Glow: concentric low-alpha halos around the glyphs
            const int layers = 5;
            BYTE la = ScaleAlpha(28.0f);
            for (int i = layers; i >= 1; --i) {
                Pen p(Color(la, fxR, fxG, fxB), ew * 2.0f * i / (float)layers);
                p.SetLineJoin(LineJoinRound);
                graphics.DrawPath(&p, &path);
            }
            graphics.FillPath(&brush, &path);
            break;
        }
        case 3: { // Pill: translucent rounded plate behind the glyphs
            RectF plate(0.5f, 0.5f, (REAL)g_cacheW - 1.0f, (REAL)g_cacheH - 1.0f);
            GraphicsPath pill;
            AddRoundRect(pill, plate, plate.Height / 2.0f);
            SolidBrush plateBrush(Color(ScaleAlpha(170.0f), fxR, fxG, fxB));
            graphics.FillPath(&plateBrush, &pill);
            graphics.FillPath(&brush, &path);
            break;
        }
        default: { // Outline (classic)
            // A pen width of 0 means "hairline" to GDI+, so an outline width of
            // zero would still paint a 1px border. Honour "no outline" instead.
            if (Config::outlineWidth > 0.0f) {
                Pen pen(Color(ScaleAlpha(255.0f), fxR, fxG, fxB), (REAL)Config::outlineWidth);
                pen.SetLineJoin(LineJoinRound);
                graphics.DrawPath(&pen, &path);
            }
            graphics.FillPath(&brush, &path);
            break;
        }
    }
}

void ArmClockTimer(HWND hwnd); // fwd decl

// True when the format changes every second: %S, and %X / %c, which include
// seconds in msvcrt's output. The '#' flag (%#S) is skipped like in
// IsValidTimeFormat. A plain substring search for "%S" missed all three.
bool FormatHasSeconds() {
    for (const WCHAR* p = Config::timeFormat; *p; ++p) {
        if (*p != L'%') continue;
        ++p;
        if (*p == L'#') ++p;
        if (*p == L'S' || *p == L'X' || *p == L'c') return true;
        if (!*p) break;
    }
    return false;
}

// The time string currently painted into the cached surface — i.e. what the
// user is actually looking at. Owned by RecalculateAll(), the only function
// that draws, so it can never drift from the pixels the way a caller-side
// "last time I saw" copy can. Empty means "nothing valid on screen".
TCHAR g_lastRendered[64] = {0};

// Re-measures the text, resizes the cached surface, redraws it and presents it
// at the configured corner. This is the only expensive path; animation frames
// reuse the surface via PresentClock().
void RecalculateAll(HWND hwnd) {
    GdiplusEnsure();

    {
        GraphicsPath path;
        RectF b;
        float pad = EffectPad();

        TCHAR drawn[64] = {0};
        bool built = BuildClockPath(path, b, drawn, 64);
        if (built) {
            clockSize.cx = (int)ceil(b.Width + pad * 2.0f);
            clockSize.cy = (int)ceil(b.Height + pad * 2.0f);
        } else {
            clockSize.cx = 1;
            clockSize.cy = 1;
        }
        if (clockSize.cx < 1) clockSize.cx = 1;
        if (clockSize.cy < 1) clockSize.cy = 1;

        targetY = VisibleY();
        if (currentState == STATE_VISIBLE) currentYVal = (float)targetY;
        else if (currentState == STATE_HIDDEN) currentYVal = (float)HiddenY();

        bool shown = false;
        if (built && EnsureSurface(clockSize.cx, clockSize.cy)) {
            RenderClock(path, b); // Reuses the path built above — no second build
            int alpha = (currentState == STATE_HIDDEN) ? 0
                      : (currentState == STATE_VISIBLE) ? Config::opacity
                      : Config::opacity * g_animAlpha / 100;
            PresentClock(hwnd, ClockX(), (int)currentYVal, alpha);
            shown = true;
        }

        // Only claim a string is on screen once it really was drawn. A failed
        // build (bad font, empty format) leaves the cache empty so the next
        // tick retries instead of concluding "nothing changed" for a minute.
        if (shown) _tcscpy(g_lastRendered, drawn);
        else       g_lastRendered[0] = _T('\0');
    } // GDI+ objects destroyed before the working set is trimmed

    // Hidden means the surface is parked off-screen where nobody can see it,
    // and StartSlide() rebuilds it on the way back in. Holding its DIB for the
    // whole idle period buys nothing, so give it back before trimming.
    if (currentState == STATE_HIDDEN) {
        FreeSurface();
        g_lastRendered[0] = _T('\0');
    }

    // The render just touched GDI+'s heaps and the glyph rasteriser; hand those
    // pages straight back. They are only needed again on the next redraw, at
    // most once a minute, so this keeps the resident set well under 1 MB.
    // Skipped during live preview (re-renders on every keystroke) and for
    // seconds formats (re-renders every second): emptying the working set that
    // often only churns page faults. Those still trim when the clock parks.
    // ponytail: seconds formats never trim while visible; throttle to once a
    // minute if their resident set ever matters.
    if (!g_settingsOpen && !FormatHasSeconds())
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    ArmClockTimer(hwnd);
}

// Brings the on-screen text up to date, doing the expensive render only when it
// would actually change something. Two things make that check reliable:
// g_lastRendered is written by the render path itself, and it is empty whenever
// the surface is gone — so a clock coming back from hidden always redraws.
// Returns true if a render happened.
bool RefreshClockText(HWND hwnd) {
    if (currentState == STATE_HIDDEN) return false; // Nothing visible to correct

    TCHAR cur[64];
    GetTime(cur, 64);
    if (g_hdcCache && _tcscmp(g_lastRendered, cur) == 0) return false;

    RecalculateAll(hwnd);
    return true;
}

bool g_ownTrayIcon = false; // nid.hIcon came from LoadImage and must be destroyed

void DestroyTrayIconImage() {
    if (g_ownTrayIcon && nid.hIcon) DestroyIcon(nid.hIcon);
    nid.hIcon = NULL;
    g_ownTrayIcon = false;
}

void InitTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    
    // Load icon resource 101 at the tray's real size. LoadIcon always picks the
    // 32 px (SM_CXICON) image and the shell shrinks it, which blurs it. The .ico
    // carries 16-128 px images, so this picks the exact one for the DPI (20 px
    // at 125%, 24 px at 150%...). LoadIconMetric would do the same but needs
    // comctl32 v6 at load time; without the manifest the exe would not start.
    // Not LR_SHARED, so it is destroyed in DestroyTrayIconImage().
    nid.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(101), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    g_ownTrayIcon = (nid.hIcon != NULL);
    if (!nid.hIcon) {
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    _tcscpy(nid.szTip, Config::trayTooltip);
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

bool IsStartupEnabled() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        TCHAR path[MAX_PATH * 2] = {0}; // Ensure buffer is fully zeroed
        DWORD size = sizeof(path);
        if (RegQueryValueEx(hKey, _T("EdgeClock"), NULL, NULL, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            
            // Convert to lowercase for case-insensitive comparison
            TCHAR lowerPath[MAX_PATH * 2];
            _tcscpy(lowerPath, path);
            _tcslwr(lowerPath);
            
            // Check if it contains the executable name
            if (_tcsstr(lowerPath, _T("edgeclock.exe")) != NULL) {
                return true;
            } else {
                return false;
            }
        }
        RegCloseKey(hKey);
    }
    return false;
}

void SetStartup(bool enable) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            TCHAR path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            
            // Wrap in quotes so paths with spaces work correctly using concatenation
            // This avoids formatting bugs with _stprintf and %s on wide strings
            TCHAR quoted[MAX_PATH + 4];
            _tcscpy(quoted, _T("\""));
            _tcscat(quoted, path);
            _tcscat(quoted, _T("\""));
            
            // Write to registry
            if (RegSetValueEx(hKey, _T("EdgeClock"), 0, REG_SZ, (LPBYTE)quoted, (lstrlen(quoted) + 1) * sizeof(TCHAR)) == ERROR_SUCCESS) {
                Log("Startup Registry Key created/updated successfully.");
            } else {
                Log("ERROR: Failed to write Startup Registry Key.");
            }
        } else {
            if (RegDeleteValue(hKey, _T("EdgeClock")) == ERROR_SUCCESS) {
                Log("Startup Registry Key deleted successfully.");
            } else {
                Log("ERROR: Failed to delete Startup Registry Key.");
            }
        }
        RegCloseKey(hKey);
    } else {
        Log("ERROR: Failed to open Startup Registry Key for writing.");
    }
}

// =============================================================
//   SETTINGS EXPORT / IMPORT  (UTF-16LE key=value text file)
// =============================================================
static const WCHAR kSettingsHeader[] = L"EdgeClockSettings=1";

bool ExportSettings(HWND owner) {
    WCHAR path[MAX_PATH];
    lstrcpyW(path, L"EdgeClock-settings.txt");
    WCHAR filter[128];
    _sntprintf(filter, 100, L"%s (*.txt)", L(S_FILE_FILTER));
    filter[100] = L'\0';
    // Build the double-NUL terminated filter pattern in place
    size_t flen = wcslen(filter);
    wcscpy(filter + flen + 1, L"*.txt");
    filter[flen + 1 + 5 + 1] = L'\0';

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetSaveFileNameW(&ofn)) return true; // Cancelled, not an error

    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;

    WCHAR bom = 0xFEFF;
    fwrite(&bom, sizeof(WCHAR), 1, f);

    WCHAR body[1024];
    _snwprintf(body, 1024,
        L"%s\r\n"
        L"FontName=%s\r\nTimeFormat=%s\r\n"
        L"FontSize=%.3f\r\nOutlineWidth=%.3f\r\nOffsetX=%.3f\r\nOffsetY=%.3f\r\n"
        L"TextColor=%lu\r\nOutlineColor=%lu\r\n"
        L"AnimDuration=%d\r\nOpacity=%d\r\nEffectMode=%d\r\nFontWeight=%d\r\n"
        L"FxIntensity=%d\r\nCorner=%d\r\nMonitorIndex=%d\r\nLanguage=%d\r\n",
        kSettingsHeader, Config::fontName, Config::timeFormat,
        Config::fontSize, Config::outlineWidth, Config::offsetX, Config::offsetY,
        (unsigned long)Config::textColor, (unsigned long)Config::outlineColor,
        Config::animDuration, Config::opacity, Config::effectMode, Config::fontWeight,
        Config::fxIntensity, Config::corner, Config::monitorIndex, Config::language);
    body[1023] = L'\0';

    fwrite(body, sizeof(WCHAR), wcslen(body), f);
    fclose(f);
    return true;
}

// Returns 1 = imported, 0 = unreadable file, -1 = user cancelled the picker.
// (It returned true for a cancel, and the caller then announced "Settings
// imported." for a file that was never chosen.)
int ImportSettings(HWND owner) {
    WCHAR path[MAX_PATH] = {0};
    WCHAR filter[128];
    _sntprintf(filter, 100, L"%s (*.txt)", L(S_FILE_FILTER));
    filter[100] = L'\0';
    size_t flen = wcslen(filter);
    wcscpy(filter + flen + 1, L"*.txt");
    filter[flen + 1 + 5 + 1] = L'\0';

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return -1; // Cancelled: nothing to report

    FILE* f = _wfopen(path, L"rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (bytes <= 2 || bytes > 65536) { fclose(f); return false; }

    WCHAR* text = (WCHAR*)malloc(bytes + sizeof(WCHAR) * 2);
    if (!text) { fclose(f); return false; }
    size_t got = fread(text, 1, bytes, f);
    fclose(f);
    text[got / sizeof(WCHAR)] = L'\0';

    WCHAR* p = text;
    if (*p == 0xFEFF) ++p; // Skip BOM
    if (!wcsstr(p, kSettingsHeader)) { free(text); return false; }

    // Values go straight into Config; unknown keys are ignored and
    // ClampConfig() below repairs anything out of range or malformed.
    for (WCHAR* line = p; line && *line; ) {
        WCHAR* eol = wcspbrk(line, L"\r\n");
        if (eol) *eol = L'\0';

        WCHAR* eq = wcschr(line, L'=');
        if (eq) {
            *eq = L'\0';
            const WCHAR* key = line;
            const WCHAR* val = eq + 1;

            if      (!wcscmp(key, L"FontName"))     { lstrcpynW(Config::fontName, val, 32); }
            else if (!wcscmp(key, L"TimeFormat"))   { lstrcpynW(Config::timeFormat, val, 32); }
            else if (!wcscmp(key, L"FontSize"))     Config::fontSize = (float)_wtof(val);
            else if (!wcscmp(key, L"OutlineWidth")) Config::outlineWidth = (float)_wtof(val);
            else if (!wcscmp(key, L"OffsetX"))      Config::offsetX = (float)_wtof(val);
            else if (!wcscmp(key, L"OffsetY"))      Config::offsetY = (float)_wtof(val);
            else if (!wcscmp(key, L"TextColor"))    Config::textColor = (COLORREF)_wtoi64(val);
            else if (!wcscmp(key, L"OutlineColor")) Config::outlineColor = (COLORREF)_wtoi64(val);
            else if (!wcscmp(key, L"AnimDuration")) Config::animDuration = _wtoi(val);
            else if (!wcscmp(key, L"Opacity"))      Config::opacity = _wtoi(val);
            else if (!wcscmp(key, L"EffectMode"))   Config::effectMode = _wtoi(val);
            else if (!wcscmp(key, L"FontWeight"))   Config::fontWeight = _wtoi(val);
            else if (!wcscmp(key, L"FxIntensity"))  Config::fxIntensity = _wtoi(val);
            else if (!wcscmp(key, L"Corner"))       Config::corner = _wtoi(val);
            else if (!wcscmp(key, L"MonitorIndex")) Config::monitorIndex = _wtoi(val);
            else if (!wcscmp(key, L"Language"))     Config::language = _wtoi(val);
        }

        if (!eol) break;
        line = eol + 1;
        while (*line == L'\r' || *line == L'\n') ++line;
    }
    free(text);

    ClampConfig();
    ApplyLanguage();
    SaveConfig();
    return true;
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    
    // Use Config values for menu display logic
    // Quick size presets
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeSmall ? MF_CHECKED : 0), ID_SIZE_SMALL, L(S_MENU_SIZE_S));
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeMedium ? MF_CHECKED : 0), ID_SIZE_MEDIUM, L(S_MENU_SIZE_M));
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeLarge ? MF_CHECKED : 0), ID_SIZE_LARGE, L(S_MENU_SIZE_L));
    // Removed redundant "Size: Custom..." since we have Settings now

    AppendMenu(hMenu, MF_STRING, ID_MENU_SETTINGS, L(S_MENU_SETTINGS));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), ID_MENU_STARTUP, L(S_MENU_STARTUP));
    AppendMenu(hMenu, MF_STRING, ID_MENU_TOGGLE_HIDE, manualHidden ? L(S_MENU_SHOW) : L(S_MENU_HIDE));

    // Language submenu
    HMENU hLang = CreatePopupMenu();
    AppendMenu(hLang, MF_STRING | (Config::language == 0 ? MF_CHECKED : 0), ID_LANG_AUTO, L(S_LANG_AUTO));
    AppendMenu(hLang, MF_STRING | (Config::language == 1 ? MF_CHECKED : 0), ID_LANG_EN, L(S_LANG_EN));
    AppendMenu(hLang, MF_STRING | (Config::language == 2 ? MF_CHECKED : 0), ID_LANG_TH, L(S_LANG_TH));
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hLang, L(S_MENU_LANG));

    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_MENU_EXPORT, L(S_MENU_EXPORT));
    AppendMenu(hMenu, MF_STRING, ID_MENU_IMPORT, L(S_MENU_IMPORT));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, L(S_MENU_EXIT));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu); // Also destroys the attached submenu
}

// Start Menu shortcut creation removed — the Inno Setup installer owns
// shortcut management, so a user-deleted shortcut stays deleted.

// ... Settings Window Logic ...
COLORREF tempTextColor;
COLORREF tempOutlineColor;
WCHAR tempFontName[32];
HWND hColorBtn1, hColorBtn2;

// Snapshot of live config taken when the dialog opens, restored on Cancel.
// Enables live preview: every control change applies to Config immediately.
struct ConfigSnapshot {
    float fontSize, outlineWidth, offsetX, offsetY;
    int animDuration, opacity, effectMode;
    int fontWeight, fxIntensity, corner, monitorIndex;
    COLORREF textColor, outlineColor;
    WCHAR fontName[32];
    WCHAR timeFormat[32];
} g_cfgSnapshot;
bool g_settingsReady = false; // Suppress preview during WM_CREATE init
bool g_settingsOpen = false;  // Re-entry guard (tray clicks reach the disabled parent)
bool g_fmtInvalid = false;    // Custom format box holds an unusable format (painted red)
bool g_uiBusy = false;        // A tray-launched modal (file dialog, message box) is up

void RecalculateAll(HWND hwnd); // fwd decl

// Reads all dialog controls + temp colors/font into Config (with clamping)
// and repaints the clock. Used for both live preview and Save.
void ApplyPreview(HWND hwnd) {
    TCHAR buf[64];

    // Ranges are enforced once, by ClampConfig() at the end of this function.
    GetDlgItemText(hwnd, ID_EDIT_FONT_SIZE, buf, 64);
    Config::fontSize = (float)_wtof(buf);

    GetDlgItemText(hwnd, ID_EDIT_OUTLINE_WIDTH, buf, 64);
    Config::outlineWidth = (float)_wtof(buf);

    GetDlgItemText(hwnd, ID_EDIT_OFFSET_X, buf, 64);
    Config::offsetX = (float)_wtof(buf);
    GetDlgItemText(hwnd, ID_EDIT_OFFSET_Y, buf, 64);
    Config::offsetY = (float)_wtof(buf);

    // An invalid format is never applied (the clock keeps the last good one),
    // so the box turns red to say why typing seems to do nothing.
    GetDlgItemText(hwnd, ID_EDIT_TIME_FORMAT, buf, 64);
    buf[31] = L'\0';
    bool fmtOk = IsValidTimeFormat(buf);
    if (fmtOk) lstrcpyW(Config::timeFormat, buf);
    if (g_fmtInvalid == fmtOk) {
        g_fmtInvalid = !fmtOk;
        InvalidateRect(GetDlgItem(hwnd, ID_EDIT_TIME_FORMAT), NULL, TRUE);
    }

    Config::animDuration = (int)SendMessage(GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED), TBM_GETPOS, 0, 0) * 100;

    int op = (int)SendMessage(GetDlgItem(hwnd, ID_TRACK_OPACITY), TBM_GETPOS, 0, 0);
    if (op < 20) op = 20;
    if (op > 100) op = 100;
    Config::opacity = op;

    int pw = (int)SendMessage(GetDlgItem(hwnd, ID_TRACK_FXPOWER), TBM_GETPOS, 0, 0);
    if (pw >= 10 && pw <= 200) Config::fxIntensity = pw;

    int fx = (int)SendMessage(GetDlgItem(hwnd, ID_COMBO_EFFECT), CB_GETCURSEL, 0, 0);
    if (fx >= 0 && fx <= 3) Config::effectMode = fx;

    int wt = (int)SendMessage(GetDlgItem(hwnd, ID_COMBO_WEIGHT), CB_GETCURSEL, 0, 0);
    if (wt >= 0 && wt <= 3) Config::fontWeight = wt;

    int cn = (int)SendMessage(GetDlgItem(hwnd, ID_COMBO_CORNER), CB_GETCURSEL, 0, 0);
    if (cn >= 0 && cn <= 3) Config::corner = cn;

    HWND hMonCombo = GetDlgItem(hwnd, ID_COMBO_MONITOR);
    int monSel = (int)SendMessage(hMonCombo, CB_GETCURSEL, 0, 0);
    if (monSel >= 0) {
        int mi = (int)SendMessage(hMonCombo, CB_GETITEMDATA, monSel, 0);
        if (mi >= 0 && mi <= EC_MAX_MONITORS) Config::monitorIndex = mi;
    }

    Config::textColor = tempTextColor;
    Config::outlineColor = tempOutlineColor;
    lstrcpyW(Config::fontName, tempFontName);
    ClampConfig();

    HWND hParent = GetParent(hwnd);
    if (hParent) {
        UpdateScreenMetrics(); // corner/monitor may have moved the target
        RecalculateAll(hParent);
    }
}

void RevertPreview(HWND hwnd) {
    Config::fontSize = g_cfgSnapshot.fontSize;
    Config::outlineWidth = g_cfgSnapshot.outlineWidth;
    Config::offsetX = g_cfgSnapshot.offsetX;
    Config::offsetY = g_cfgSnapshot.offsetY;
    Config::animDuration = g_cfgSnapshot.animDuration;
    Config::opacity = g_cfgSnapshot.opacity;
    Config::effectMode = g_cfgSnapshot.effectMode;
    Config::fontWeight = g_cfgSnapshot.fontWeight;
    Config::fxIntensity = g_cfgSnapshot.fxIntensity;
    Config::corner = g_cfgSnapshot.corner;
    Config::monitorIndex = g_cfgSnapshot.monitorIndex;
    Config::textColor = g_cfgSnapshot.textColor;
    Config::outlineColor = g_cfgSnapshot.outlineColor;
    lstrcpyW(Config::fontName, g_cfgSnapshot.fontName);
    lstrcpyW(Config::timeFormat, g_cfgSnapshot.timeFormat);

    HWND hParent = GetParent(hwnd);
    if (hParent) {
        UpdateScreenMetrics();
        RecalculateAll(hParent);
    }
}

// Dark Mode Resources
HBRUSH hBrushDark = NULL;
HBRUSH hBrushEdit = NULL;
HBRUSH hBrushSep = NULL;
HBRUSH hBrushBad = NULL;
HFONT hFontSegoe = NULL;

// --- DPI scaling for the Settings dialog ------------------------------------
// The manifest declares PerMonitorV2, so Windows does NOT scale our windows
// for us: every coordinate below is authored at 96 DPI and scaled through DS().
int g_dlgDpi = 96;

int QueryDpi(HWND hwnd) {
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn pGetDpiForWindow = NULL;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE hUser = GetModuleHandle(_T("user32.dll"));
        // Cast through void* — casting FARPROC directly trips -Wcast-function-type
        if (hUser) pGetDpiForWindow = (GetDpiForWindowFn)(void*)GetProcAddress(hUser, "GetDpiForWindow");
    }
    if (pGetDpiForWindow && hwnd) {
        UINT d = pGetDpiForWindow(hwnd);
        if (d >= 72) return (int)d;
    }
    HDC hdc = GetDC(hwnd);
    int d = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return d >= 72 ? d : 96;
}

inline int DS(int v) { return MulDiv(v, g_dlgDpi, 96); }

// The finished layout is ~596x466 logical units. At very high scaling (250%+)
// that would not fit the work area and the button row would fall off the
// bottom, so the layout scale is capped to what fits on the given monitor.
int CapDlgDpi(int dpi, HMONITOR hMon) {
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hMon, &mi)) {
        int maxByH = MulDiv(96, mi.rcWork.bottom - mi.rcWork.top, 466);
        int maxByW = MulDiv(96, mi.rcWork.right - mi.rcWork.left, 596);
        int cap = maxByH < maxByW ? maxByH : maxByW;
        if (cap >= 96 && dpi > cap) return cap;
    }
    return dpi;
}

// Client -> window rect for a given DPI. Under PerMonitorV2 the caption and
// borders scale with the monitor, which plain AdjustWindowRectEx (system DPI)
// gets wrong on a secondary monitor; the ForDpi variant is Windows 10 1607+.
void AdjustRectForDpi(RECT* rc, HWND hwnd, UINT dpi) {
    typedef BOOL (WINAPI *AdjustForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static AdjustForDpiFn pAdjust = NULL;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE hUser = GetModuleHandle(_T("user32.dll"));
        if (hUser) pAdjust = (AdjustForDpiFn)(void*)GetProcAddress(hUser, "AdjustWindowRectExForDpi");
    }
    DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (pAdjust) pAdjust(rc, style, FALSE, exStyle, dpi);
    else AdjustWindowRectEx(rc, style, FALSE, exStyle);
}

// Logical (96-DPI) client size of the finished layout, recorded by WM_CREATE
// so WM_DPICHANGED can resize the window without re-running the layout.
int g_dlgClientW96 = 0, g_dlgClientH96 = 0;

void SetModernFont(HWND hwnd) {
    if (!hFontSegoe) {
        // Slightly larger, cleaner font (DPI-scaled)
        hFontSegoe = CreateFont(DS(19), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));
    }
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFontSegoe, TRUE);
}

HWND StyleControl(HWND hCtrl) {
    SetModernFont(hCtrl);
    return hCtrl;
}

// --- Hover tracking for owner-drawn buttons ---
HWND g_hoverBtn = NULL;

LRESULT CALLBACK ButtonHoverProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_MOUSEMOVE:
            if (g_hoverBtn != hwnd) {
                g_hoverBtn = hwnd;
                InvalidateRect(hwnd, NULL, TRUE);
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            break;
        case WM_MOUSELEAVE:
            if (g_hoverBtn == hwnd) {
                g_hoverBtn = NULL;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        case WM_NCDESTROY:
            if (g_hoverBtn == hwnd) g_hoverBtn = NULL;
            RemoveWindowSubclass(hwnd, ButtonHoverProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

HWND HoverButton(HWND hBtn) {
    SetWindowSubclass(hBtn, ButtonHoverProc, 1, 0);
    return hBtn;
}

// Shared owner-draw renderer for the flat dark-grey buttons
void DrawDarkButton(LPDRAWITEMSTRUCT pDIS, LPCTSTR text) {
    bool hover = (g_hoverBtn == pDIS->hwndItem);
    COLORREF bg = (pDIS->itemState & ODS_SELECTED) ? RGB(85, 85, 85)
                : hover ? RGB(74, 74, 74) : RGB(60, 60, 60);
    HBRUSH hBrush = CreateSolidBrush(bg);
    FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
    DeleteObject(hBrush);

    HBRUSH hBorder = CreateSolidBrush(hover ? RGB(140, 140, 140) : RGB(100, 100, 100));
    FrameRect(pDIS->hDC, &pDIS->rcItem, hBorder);
    DeleteObject(hBorder);

    SetBkMode(pDIS->hDC, TRANSPARENT);
    SetTextColor(pDIS->hDC, hover ? RGB(255, 255, 255) : RGB(220, 220, 220));
    HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFontSegoe);
    DrawText(pDIS->hDC, text, -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(pDIS->hDC, hOldFont);

    // Keyboard focus needs to be visible now that Tab navigation works
    if (pDIS->itemState & ODS_FOCUS) {
        RECT rcF = pDIS->rcItem;
        InflateRect(&rcF, -3, -3);
        DrawFocusRect(pDIS->hDC, &rcF);
    }
}

// --- Time format presets shown in the Settings combo ---
// Labels come from the string table (S_FMT_24 + i), in this same order.
const WCHAR* const kFormatPresets[] = {
    L"%H:%M", L"%H:%M:%S", L"%I:%M %p", L"%I:%M:%S %p", L"%a %d %H:%M",
};
const int kNumFormatPresets = (int)(sizeof(kFormatPresets) / sizeof(kFormatPresets[0]));

// Returns the preset index matching fmt, or kNumFormatPresets ("Custom")
int MatchFormatPreset(const WCHAR* fmt) {
    for (int i = 0; i < kNumFormatPresets; ++i)
        if (wcscmp(fmt, kFormatPresets[i]) == 0) return i;
    return kNumFormatPresets;
}

// Main procedure for the Settings Dialog
// Handles all user interactions in the settings window.
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_CREATE: {
            g_settingsReady = false;
            // DoSettingsDialog creates the window on the monitor under the
            // cursor, so this is already that monitor's DPI.
            g_dlgDpi = CapDlgDpi(QueryDpi(hwnd), MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY));

            // Snapshot live config for Cancel/revert (live preview writes to Config)
            g_cfgSnapshot.fontSize = Config::fontSize;
            g_cfgSnapshot.outlineWidth = Config::outlineWidth;
            g_cfgSnapshot.offsetX = Config::offsetX;
            g_cfgSnapshot.offsetY = Config::offsetY;
            g_cfgSnapshot.animDuration = Config::animDuration;
            g_cfgSnapshot.opacity = Config::opacity;
            g_cfgSnapshot.effectMode = Config::effectMode;
            g_cfgSnapshot.fontWeight = Config::fontWeight;
            g_cfgSnapshot.fxIntensity = Config::fxIntensity;
            g_cfgSnapshot.corner = Config::corner;
            g_cfgSnapshot.monitorIndex = Config::monitorIndex;
            g_cfgSnapshot.textColor = Config::textColor;
            g_cfgSnapshot.outlineColor = Config::outlineColor;
            lstrcpyW(g_cfgSnapshot.fontName, Config::fontName);
            lstrcpyW(g_cfgSnapshot.timeFormat, Config::timeFormat);

            // "Minimal Modern" Design - Dark Theme
            hBrushDark = CreateSolidBrush(RGB(32, 32, 32)); // Dark Background
            hBrushEdit = CreateSolidBrush(RGB(50, 50, 50)); // Darker Grey Edit Fields
            hBrushSep  = CreateSolidBrush(RGB(70, 70, 70)); // Section separator lines
            hBrushBad  = CreateSolidBrush(RGB(110, 40, 40)); // Invalid custom format
            g_fmtInvalid = false;

            // Two-column layout, authored at 96 DPI and scaled via DS().
            const int LX = 16, LLW = 74, LCX = 94, LCW = 176;   // left column
            const int RX = 300, RLW = 74, RCX = 378, RCW = 160;  // right column
            const int H = 24, GAP = 32, HDRH = 18;
            int sectionId = ID_SECTION_FIRST, sepId = ID_SEP_FIRST;
            TCHAR buf[64];

            // Helper macros keep the control wall readable
            #define MK(cls, txt, style, x, y, w, h, id) \
                CreateWindow(cls, txt, WS_VISIBLE | WS_CHILD | (style), DS(x), DS(y), DS(w), DS(h), \
                             hwnd, (HMENU)(UINT_PTR)(id), NULL, NULL)
            #define LABEL(txt, x, y, w) \
                StyleControl(MK(_T("STATIC"), txt, SS_CENTERIMAGE, x, y, w, H, 0))
            #define EDIT(x, y, w, id) \
                StyleControl(MK(_T("EDIT"), _T(""), WS_BORDER | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL, x, y, w, H, id))
            #define COMBO(x, y, w, id) \
                StyleControl(MK(_T("COMBOBOX"), _T(""), WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, x, y, w, 220, id))
            #define ODBTN(txt, x, y, w, h, id) \
                HoverButton(StyleControl(MK(_T("BUTTON"), txt, WS_TABSTOP | BS_OWNERDRAW, x, y, w, h, id)))
            #define SECTION(txt, x, y, w) do { \
                StyleControl(MK(_T("STATIC"), txt, SS_CENTERIMAGE, x, y, w, HDRH, sectionId++)); \
                MK(_T("STATIC"), _T(""), 0, x, (y) + HDRH + 2, w, 1, sepId++); \
            } while (0)

            // ================= LEFT COLUMN =================
            int y = 12;
            SECTION(L(S_SEC_TEXT), LX, y, LCX - LX + LCW);
            y += HDRH + 12;

            // Font: name + chooser
            LABEL(L(S_FONT), LX, y, LLW);
            StyleControl(MK(_T("STATIC"), Config::fontName, SS_ENDELLIPSIS | SS_CENTERIMAGE,
                            LCX, y, 108, H, ID_STATIC_FONT_NAME));
            ODBTN(L(S_CHOOSE), LCX + 114, y, 62, H, ID_BTN_ID_FONT);

            y += GAP;
            // Size + Weight
            LABEL(L(S_SIZE), LX, y, LLW);
            _stprintf(buf, _T("%.1f"), Config::fontSize);
            SetWindowText(EDIT(LCX, y, 52, ID_EDIT_FONT_SIZE), buf);
            LABEL(L(S_WEIGHT), LCX + 58, y, 48);
            HWND hWeight = COMBO(LCX + 108, y, 68, ID_COMBO_WEIGHT);
            SetWindowTheme(hWeight, L"DarkMode_CFD", NULL);
            for (int i = 0; i < 4; ++i) SendMessage(hWeight, CB_ADDSTRING, 0, (LPARAM)L(S_W_REGULAR + i));
            SendMessage(hWeight, CB_SETCURSEL, Config::fontWeight, 0);

            y += GAP;
            // Text colour
            LABEL(L(S_TEXT_COLOR), LX, y, LLW);
            hColorBtn1 = ODBTN(_T(""), LCX, y, 52, H, ID_BTN_TEXT_COLOR);

            y += GAP;
            // Time format preset
            LABEL(L(S_FORMAT), LX, y, LLW);
            HWND hFmtCombo = COMBO(LCX, y, LCW, ID_COMBO_FORMAT);
            SetWindowTheme(hFmtCombo, L"DarkMode_CFD", NULL);
            for (int i = 0; i < kNumFormatPresets; ++i)
                SendMessage(hFmtCombo, CB_ADDSTRING, 0, (LPARAM)L(S_FMT_24 + i));
            SendMessage(hFmtCombo, CB_ADDSTRING, 0, (LPARAM)L(S_CUSTOM_OPT));
            int fmtSel = MatchFormatPreset(Config::timeFormat);
            SendMessage(hFmtCombo, CB_SETCURSEL, fmtSel, 0);

            y += GAP;
            // Custom strftime string
            LABEL(L(S_CUSTOM), LX, y, LLW);
            HWND hFmt = EDIT(LCX, y, LCW, ID_EDIT_TIME_FORMAT);
            // Config::timeFormat is 32 WCHARs. Without a limit, characters past
            // 31 were silently cut off, which could split a %-code and make
            // the whole format invalid.
            SendMessage(hFmt, EM_LIMITTEXT, 31, 0);
            SetWindowText(hFmt, Config::timeFormat);
            // Read-only (not disabled) when a preset is active: stays legible
            // on the dark theme and the user can still see/copy the format
            SendMessage(hFmt, EM_SETREADONLY, fmtSel != kNumFormatPresets, 0);

            y += GAP + 8;
            SECTION(L(S_SEC_EFFECT), LX, y, LCX - LX + LCW);
            y += HDRH + 12;

            LABEL(L(S_EFFECT), LX, y, LLW);
            HWND hFx = COMBO(LCX, y, LCW, ID_COMBO_EFFECT);
            SetWindowTheme(hFx, L"DarkMode_CFD", NULL);
            for (int i = 0; i < 4; ++i) SendMessage(hFx, CB_ADDSTRING, 0, (LPARAM)L(S_FX_OUTLINE + i));
            SendMessage(hFx, CB_SETCURSEL, Config::effectMode, 0);

            y += GAP;
            // FX width + FX colour
            LABEL(L(S_FX_WIDTH), LX, y, LLW);
            _stprintf(buf, _T("%.1f"), Config::outlineWidth);
            SetWindowText(EDIT(LCX, y, 52, ID_EDIT_OUTLINE_WIDTH), buf);
            LABEL(L(S_FX_COLOR), LCX + 58, y, 48);
            hColorBtn2 = ODBTN(_T(""), LCX + 108, y, 52, H, ID_BTN_OUTLINE_COLOR);

            y += GAP;
            // Effect intensity
            LABEL(L(S_POWER), LX, y, LLW);
            HWND hPow = MK(TRACKBAR_CLASS, _T(""), WS_TABSTOP, LCX - 4, y, 124, 28, ID_TRACK_FXPOWER);
            SendMessage(hPow, TBM_SETRANGE, TRUE, MAKELPARAM(10, 200));
            SendMessage(hPow, TBM_SETPOS, TRUE, Config::fxIntensity);
            _stprintf(buf, _T("%d%%"), Config::fxIntensity);
            StyleControl(MK(_T("STATIC"), buf, SS_CENTERIMAGE, LCX + 124, y, 52, H, ID_STATIC_FXPOWER_VAL));

            y += GAP;
            // Opacity
            LABEL(L(S_OPACITY), LX, y, LLW);
            HWND hOpTrack = MK(TRACKBAR_CLASS, _T(""), WS_TABSTOP, LCX - 4, y, 124, 28, ID_TRACK_OPACITY);
            SendMessage(hOpTrack, TBM_SETRANGE, TRUE, MAKELPARAM(20, 100));
            SendMessage(hOpTrack, TBM_SETPOS, TRUE, Config::opacity);
            _stprintf(buf, _T("%d%%"), Config::opacity);
            StyleControl(MK(_T("STATIC"), buf, SS_CENTERIMAGE, LCX + 124, y, 52, H, ID_STATIC_OPACITY_VAL));

            int leftBottom = y + GAP;

            // ================= RIGHT COLUMN =================
            y = 12;
            SECTION(L(S_SEC_BEHAVIOR), RX, y, RCX - RX + RCW);
            y += HDRH + 12;

            LABEL(L(S_ANIM_SPEED), RX, y, RLW);
            HWND hTrack = MK(TRACKBAR_CLASS, _T(""), WS_TABSTOP, RCX - 4, y, 108, 28, ID_TRACK_ANIM_SPEED);
            SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELPARAM(0, 20));
            SendMessage(hTrack, TBM_SETPOS, TRUE, Config::animDuration / 100);
            _stprintf(buf, _T("%.1fs"), Config::animDuration / 1000.0);
            StyleControl(MK(_T("STATIC"), buf, SS_CENTERIMAGE, RCX + 108, y, 52, H, ID_STATIC_DURATION_VAL));

            y += GAP + 8;
            SECTION(L(S_SEC_POSITION), RX, y, RCX - RX + RCW);
            y += HDRH + 12;

            LABEL(L(S_CORNER), RX, y, RLW);
            HWND hCorner = COMBO(RCX, y, RCW, ID_COMBO_CORNER);
            SetWindowTheme(hCorner, L"DarkMode_CFD", NULL);
            for (int i = 0; i < 4; ++i) SendMessage(hCorner, CB_ADDSTRING, 0, (LPARAM)L(S_CNR_BR + i));
            SendMessage(hCorner, CB_SETCURSEL, Config::corner, 0);

            y += GAP;
            LABEL(L(S_MONITOR), RX, y, RLW);
            HWND hMonCombo = COMBO(RCX, y, RCW, ID_COMBO_MONITOR);
            SetWindowTheme(hMonCombo, L"DarkMode_CFD", NULL);
            // Each entry carries its monitor number as item data, so the list
            // can hold a saved monitor that is unplugged right now. Showing
            // "Auto" for it instead meant pressing Save quietly forgot the
            // user's choice while a laptop was undocked.
            {
                int selIdx = 0;
                LRESULT at = SendMessage(hMonCombo, CB_ADDSTRING, 0, (LPARAM)L(S_AUTO));
                SendMessage(hMonCombo, CB_SETITEMDATA, at, 0);
                for (int i = 1; i <= g_monCount; ++i) {
                    _sntprintf(buf, 64, L(S_MONITOR_N), i);
                    buf[63] = _T('\0');
                    at = SendMessage(hMonCombo, CB_ADDSTRING, 0, (LPARAM)buf);
                    SendMessage(hMonCombo, CB_SETITEMDATA, at, i);
                    if (i == Config::monitorIndex) selIdx = (int)at;
                }
                if (Config::monitorIndex > g_monCount) {
                    TCHAR name[32];
                    _sntprintf(name, 32, L(S_MONITOR_N), Config::monitorIndex);
                    name[31] = _T('\0');
                    _sntprintf(buf, 64, L(S_MON_MISSING), name);
                    buf[63] = _T('\0');
                    at = SendMessage(hMonCombo, CB_ADDSTRING, 0, (LPARAM)buf);
                    SendMessage(hMonCombo, CB_SETITEMDATA, at, Config::monitorIndex);
                    selIdx = (int)at;
                }
                SendMessage(hMonCombo, CB_SETCURSEL, selIdx, 0);
            }

            y += GAP;
            // Offsets — allow negative & float (no ES_NUMBER)
            LABEL(L(S_OFFSET_X), RX, y, RLW);
            _stprintf(buf, _T("%.1f"), Config::offsetX);
            SetWindowText(EDIT(RCX, y, 60, ID_EDIT_OFFSET_X), buf);
            LABEL(L(S_OFFSET_Y), RCX + 66, y, 20);
            _stprintf(buf, _T("%.1f"), Config::offsetY);
            SetWindowText(EDIT(RCX + 92, y, 60, ID_EDIT_OFFSET_Y), buf);

            int rightBottom = y + GAP;

            // ================= BUTTON ROW =================
            int by = (leftBottom > rightBottom ? leftBottom : rightBottom) + 6;
            ODBTN(L(S_SAVE), RX + 6, by, 82, 30, ID_BTN_SAVE);
            ODBTN(L(S_DEFAULTS), RX + 94, by, 82, 30, ID_BTN_RESET);
            ODBTN(L(S_CANCEL), RX + 182, by, 82, 30, ID_BTN_CANCEL);

            #undef SECTION
            #undef ODBTN
            #undef COMBO
            #undef EDIT
            #undef LABEL
            #undef MK

            // Size the window to the finished layout and centre it on the
            // monitor under the cursor (all in physical pixels).
            g_dlgClientW96 = RX + 282;
            g_dlgClientH96 = by + 30 + 14;
            RECT rcNeed = { 0, 0, DS(g_dlgClientW96), DS(g_dlgClientH96) };
            AdjustRectForDpi(&rcNeed, hwnd, (UINT)QueryDpi(hwnd));
            int wndW = rcNeed.right - rcNeed.left;
            int wndH = rcNeed.bottom - rcNeed.top;

            POINT ptCur;
            GetCursorPos(&ptCur);
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            RECT area;
            if (GetMonitorInfo(MonitorFromPoint(ptCur, MONITOR_DEFAULTTOPRIMARY), &mi)) area = mi.rcWork;
            else SetRect(&area, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

            int px = area.left + ((area.right - area.left) - wndW) / 2;
            int py = area.top + ((area.bottom - area.top) - wndH) / 2;
            if (py < area.top) py = area.top;
            SetWindowPos(hwnd, NULL, px, py, wndW, wndH, SWP_NOZORDER | SWP_NOACTIVATE);

            tempTextColor = Config::textColor;
            tempOutlineColor = Config::outlineColor;
            lstrcpyW(tempFontName, Config::fontName);
            g_settingsReady = true; // Enable live preview from here on
            break;
        }

        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, hBrushDark ? hBrushDark : (HBRUSH)GetStockObject(BLACK_BRUSH));
            return 1;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            int cid = GetDlgCtrlID((HWND)lParam);
            // A 1px-high static painted with the grey brush = separator line
            if (cid >= ID_SEP_FIRST && cid <= ID_SEP_LAST) return (INT_PTR)hBrushSep;
            SetBkMode((HDC)wParam, TRANSPARENT);
            if (cid >= ID_SECTION_FIRST && cid <= ID_SECTION_LAST)
                SetTextColor((HDC)wParam, RGB(120, 175, 235)); // Section header accent
            else
                SetTextColor((HDC)wParam, RGB(220, 220, 220)); // Light Text
            return (INT_PTR)hBrushDark;
        }

        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)wParam, RGB(255, 255, 255)); // White Text
            if (g_fmtInvalid && GetDlgCtrlID((HWND)lParam) == ID_EDIT_TIME_FORMAT) {
                SetBkColor((HDC)wParam, RGB(110, 40, 40)); // Invalid format
                return (INT_PTR)hBrushBad;
            }
            SetBkColor((HDC)wParam, RGB(50, 50, 50)); // Dark Grey Background
            return (INT_PTR)hBrushEdit;

        case WM_CTLCOLORLISTBOX: // Combo dropdown lists
            SetTextColor((HDC)wParam, RGB(230, 230, 230));
            SetBkColor((HDC)wParam, RGB(50, 50, 50));
            return (INT_PTR)hBrushEdit;

        case WM_NOTIFY: {
            // Custom-draw the trackbars so they match the dark theme:
            // flat grey channel + accent-blue thumb, no tick marks.
            LPNMHDR hdr = (LPNMHDR)lParam;
            if ((hdr->idFrom == ID_TRACK_ANIM_SPEED || hdr->idFrom == ID_TRACK_OPACITY ||
                 hdr->idFrom == ID_TRACK_FXPOWER) && hdr->code == NM_CUSTOMDRAW) {
                LPNMCUSTOMDRAW cd = (LPNMCUSTOMDRAW)lParam;
                if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
                    if (cd->dwItemSpec == TBCD_CHANNEL) {
                        HBRUSH hCh = CreateSolidBrush(RGB(85, 85, 85));
                        RECT rc = cd->rc;
                        // Slim the channel to a 4px bar, vertically centered
                        int mid = (rc.top + rc.bottom) / 2;
                        rc.top = mid - 2; rc.bottom = mid + 2;
                        FillRect(cd->hdc, &rc, hCh);
                        DeleteObject(hCh);
                        return CDRF_SKIPDEFAULT;
                    }
                    if (cd->dwItemSpec == TBCD_THUMB) {
                        HBRUSH hTh = CreateSolidBrush(RGB(0, 120, 215));
                        HPEN hOldPen = (HPEN)SelectObject(cd->hdc, GetStockObject(NULL_PEN));
                        HBRUSH hOldBr = (HBRUSH)SelectObject(cd->hdc, hTh);
                        RoundRect(cd->hdc, cd->rc.left, cd->rc.top, cd->rc.right + 1, cd->rc.bottom + 1, 6, 6);
                        SelectObject(cd->hdc, hOldBr);
                        SelectObject(cd->hdc, hOldPen);
                        DeleteObject(hTh);
                        return CDRF_SKIPDEFAULT;
                    }
                    if (cd->dwItemSpec == TBCD_TICS) return CDRF_SKIPDEFAULT;
                }
            }
            break;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID == ID_BTN_SAVE) {
                // Modern Blue Accent (lighter on hover, darker when pressed)
                bool hover = (g_hoverBtn == pDIS->hwndItem);
                COLORREF bg = (pDIS->itemState & ODS_SELECTED) ? RGB(0, 100, 180)
                            : hover ? RGB(30, 140, 235) : RGB(0, 120, 215);
                HBRUSH hBrush = CreateSolidBrush(bg);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);

                // Text
                SetBkMode(pDIS->hDC, TRANSPARENT);
                SetTextColor(pDIS->hDC, RGB(255, 255, 255));
                HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFontSegoe);
                DrawText(pDIS->hDC, L(S_SAVE), -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(pDIS->hDC, hOldFont);
                if (pDIS->itemState & ODS_FOCUS) {
                    RECT rcF = pDIS->rcItem;
                    InflateRect(&rcF, -3, -3);
                    DrawFocusRect(pDIS->hDC, &rcF);
                }
                return TRUE;
            }
            if (pDIS->CtlID == ID_BTN_CANCEL) { DrawDarkButton(pDIS, L(S_CANCEL)); return TRUE; }
            if (pDIS->CtlID == ID_BTN_RESET)  { DrawDarkButton(pDIS, L(S_DEFAULTS)); return TRUE; }
            if (pDIS->CtlID == ID_BTN_ID_FONT){ DrawDarkButton(pDIS, L(S_CHOOSE)); return TRUE; }
            if (pDIS->CtlID == ID_BTN_TEXT_COLOR || pDIS->CtlID == ID_BTN_OUTLINE_COLOR) {
                HBRUSH hBrush = CreateSolidBrush(pDIS->CtlID == ID_BTN_TEXT_COLOR ? tempTextColor : tempOutlineColor);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);

                bool hover = (g_hoverBtn == pDIS->hwndItem);
                HBRUSH hBorder = CreateSolidBrush(hover ? RGB(170, 170, 170) : RGB(100, 100, 100));
                FrameRect(pDIS->hDC, &pDIS->rcItem, hBorder);
                DeleteObject(hBorder);
                return TRUE;
            }
            break;
        }

        case WM_HSCROLL: {
             TCHAR valBuf[16];
             if ((HWND)lParam == GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED)) {
                 int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                 _stprintf(valBuf, _T("%.1fs"), pos / 10.0);
                 SetWindowText(GetDlgItem(hwnd, ID_STATIC_DURATION_VAL), valBuf);
                 if (g_settingsReady) ApplyPreview(hwnd);
             } else if ((HWND)lParam == GetDlgItem(hwnd, ID_TRACK_OPACITY)) {
                 int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                 _stprintf(valBuf, _T("%d%%"), pos);
                 SetWindowText(GetDlgItem(hwnd, ID_STATIC_OPACITY_VAL), valBuf);
                 if (g_settingsReady) ApplyPreview(hwnd);
             } else if ((HWND)lParam == GetDlgItem(hwnd, ID_TRACK_FXPOWER)) {
                 int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                 _stprintf(valBuf, _T("%d%%"), pos);
                 SetWindowText(GetDlgItem(hwnd, ID_STATIC_FXPOWER_VAL), valBuf);
                 if (g_settingsReady) ApplyPreview(hwnd);
             }
             break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);

            // Live preview: any edit field change applies immediately
            if (HIWORD(wParam) == EN_CHANGE && g_settingsReady &&
                (wmId == ID_EDIT_FONT_SIZE || wmId == ID_EDIT_OUTLINE_WIDTH ||
                 wmId == ID_EDIT_OFFSET_X || wmId == ID_EDIT_OFFSET_Y ||
                 wmId == ID_EDIT_TIME_FORMAT)) {
                ApplyPreview(hwnd);
                break;
            }

            if (HIWORD(wParam) == CBN_SELCHANGE &&
                (wmId == ID_COMBO_EFFECT || wmId == ID_COMBO_WEIGHT ||
                 wmId == ID_COMBO_CORNER || wmId == ID_COMBO_MONITOR)) {
                if (g_settingsReady) ApplyPreview(hwnd);
                break;
            }

            if (HIWORD(wParam) == CBN_SELCHANGE && wmId == ID_COMBO_FORMAT) {
                int sel = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                HWND hEdit = GetDlgItem(hwnd, ID_EDIT_TIME_FORMAT);
                if (sel >= 0 && sel < kNumFormatPresets) {
                    SendMessage(hEdit, EM_SETREADONLY, TRUE, 0);
                    // SetWindowText fires EN_CHANGE -> ApplyPreview
                    SetWindowText(hEdit, kFormatPresets[sel]);
                } else { // Custom…
                    SendMessage(hEdit, EM_SETREADONLY, FALSE, 0);
                    // Not while the list is still open: arrowing onto
                    // "Custom…" used to yank focus into the box and close the
                    // list, so the user's next Enter hit Save. CBN_CLOSEUP
                    // below hands focus over once the choice is final.
                    if (!SendMessage((HWND)lParam, CB_GETDROPPEDSTATE, 0, 0)) SetFocus(hEdit);
                }
                break;
            }
            if (HIWORD(wParam) == CBN_CLOSEUP && wmId == ID_COMBO_FORMAT) {
                if ((int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0) == kNumFormatPresets)
                    SetFocus(GetDlgItem(hwnd, ID_EDIT_TIME_FORMAT)); // Not WM_NEXTDLGCTL: plain window, no dialog manager
                break;
            }

            // IsDialogMessage turns Enter/Esc into these
            if (wmId == IDOK) wmId = ID_BTN_SAVE;
            else if (wmId == IDCANCEL) wmId = ID_BTN_CANCEL;

            switch(wmId) {
                case ID_BTN_ID_FONT: {
                    CHOOSEFONT cf = {0};
                    LOGFONT lf = {0};
                    cf.lStructSize = sizeof(cf);
                    cf.hwndOwner = hwnd;
                    cf.lpLogFont = &lf;
                    // CF_NOSIZESEL hides the size list (Size has its own box).
                    // No CF_EFFECTS: its colour/underline/strikeout controls
                    // were silently ignored. The style list (Bold/Italic) is
                    // kept and drives the Weight combo, so it is no longer
                    // thrown away either.
                    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSIZESEL;
                    lstrcpyW(lf.lfFaceName, tempFontName);
                    {
                        int w = (int)SendMessage(GetDlgItem(hwnd, ID_COMBO_WEIGHT), CB_GETCURSEL, 0, 0);
                        lf.lfWeight = (w == 1 || w == 3) ? FW_BOLD : FW_NORMAL;
                        lf.lfItalic = (w == 2 || w == 3) ? TRUE : FALSE;
                    }
                    
                    // We still init lfHeight for the sample text in the dialog
                    HDC hdc = GetDC(hwnd);
                    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
                    ReleaseDC(hwnd, hdc);
                    int currentSize = (int)Config::fontSize;
                    if (currentSize <= 0) currentSize = 14;
                    lf.lfHeight = -MulDiv(currentSize, dpi, 72);

                    if (ChooseFont(&cf)) {
                        lstrcpyW(tempFontName, lf.lfFaceName);
                        // Map the picked style onto the four styles GDI+ has.
                        // Light/Thin faces map to Regular (see Known Issues).
                        bool bold = lf.lfWeight >= FW_SEMIBOLD;
                        bool italic = lf.lfItalic != 0;
                        int w = bold ? (italic ? 3 : 1) : (italic ? 2 : 0);
                        SendMessage(GetDlgItem(hwnd, ID_COMBO_WEIGHT), CB_SETCURSEL, w, 0);
                        SetWindowText(GetDlgItem(hwnd, ID_STATIC_FONT_NAME), tempFontName);
                        ApplyPreview(hwnd);
                    }
                    break;
                }
                case ID_BTN_TEXT_COLOR: {
                    CHOOSECOLOR cc = {0};
                    static COLORREF acrCustClr[16];
                    cc.lStructSize = sizeof(cc);
                    cc.hwndOwner = hwnd;
                    cc.lpCustColors = (LPDWORD)acrCustClr;
                    cc.rgbResult = tempTextColor;
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColor(&cc)) {
                        tempTextColor = cc.rgbResult;
                        InvalidateRect(hColorBtn1, NULL, TRUE);
                        ApplyPreview(hwnd);
                    }
                    break;
                }
                case ID_BTN_OUTLINE_COLOR: {
                    CHOOSECOLOR cc = {0};
                    static COLORREF acrCustClr[16];
                    cc.lStructSize = sizeof(cc);
                    cc.hwndOwner = hwnd;
                    cc.lpCustColors = (LPDWORD)acrCustClr;
                    cc.rgbResult = tempOutlineColor;
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColor(&cc)) {
                        tempOutlineColor = cc.rgbResult;
                        InvalidateRect(hColorBtn2, NULL, TRUE);
                        ApplyPreview(hwnd);
                    }
                    break;
                }
                case ID_BTN_SAVE: {
                    // Live preview already keeps Config in sync; apply once
                    // more (final clamp) and persist.
                    ApplyPreview(hwnd);
                    SaveConfig();
                    DestroyWindow(hwnd);
                    break;
                }
                case ID_BTN_CANCEL:
                    RevertPreview(hwnd); // Undo live-preview changes
                    DestroyWindow(hwnd);
                    break;
                case ID_BTN_RESET: {
                    // Reset temp state + UI controls to defaults. The
                    // EN_CHANGE/ApplyPreview flow previews it live; Cancel
                    // still restores the pre-dialog snapshot.
                    tempTextColor = Defaults::textColor;
                    tempOutlineColor = Defaults::outlineColor;
                    lstrcpyW(tempFontName, Defaults::fontName);

                    TCHAR buf[32];
                    _stprintf(buf, _T("%.1f"), Defaults::fontSize);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_FONT_SIZE), buf);

                    _stprintf(buf, _T("%.1f"), Defaults::outlineWidth);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OUTLINE_WIDTH), buf);

                    SendMessage(GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED), TBM_SETPOS, TRUE, Defaults::animDuration / 100);
                    _stprintf(buf, _T("%.1fs"), Defaults::animDuration / 1000.0);
                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_DURATION_VAL), buf);

                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OFFSET_X), _T("0.0"));
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OFFSET_Y), _T("0.0"));

                    SendMessage(GetDlgItem(hwnd, ID_COMBO_FORMAT), CB_SETCURSEL,
                                MatchFormatPreset(Defaults::timeFormat), 0);
                    SendMessage(GetDlgItem(hwnd, ID_EDIT_TIME_FORMAT), EM_SETREADONLY, TRUE, 0);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_TIME_FORMAT), Defaults::timeFormat);

                    SendMessage(GetDlgItem(hwnd, ID_COMBO_EFFECT), CB_SETCURSEL, 0, 0);
                    SendMessage(GetDlgItem(hwnd, ID_COMBO_WEIGHT), CB_SETCURSEL, Defaults::fontWeight, 0);
                    SendMessage(GetDlgItem(hwnd, ID_COMBO_CORNER), CB_SETCURSEL, 0, 0); // Bottom-right
                    SendMessage(GetDlgItem(hwnd, ID_COMBO_MONITOR), CB_SETCURSEL, 0, 0); // Auto

                    SendMessage(GetDlgItem(hwnd, ID_TRACK_FXPOWER), TBM_SETPOS, TRUE, Defaults::fxIntensity);
                    _stprintf(buf, _T("%d%%"), Defaults::fxIntensity);
                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_FXPOWER_VAL), buf);

                    SendMessage(GetDlgItem(hwnd, ID_TRACK_OPACITY), TBM_SETPOS, TRUE, Defaults::opacity);
                    _stprintf(buf, _T("%d%%"), Defaults::opacity);
                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_OPACITY_VAL), buf);

                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_FONT_NAME), tempFontName);

                    InvalidateRect(hColorBtn1, NULL, TRUE);
                    InvalidateRect(hColorBtn2, NULL, TRUE);
                    ApplyPreview(hwnd);
                    break;
                }
            }
            break;
        }
        case 0x02E0 /*WM_DPICHANGED*/: {
            // Dragged onto a monitor with different scaling. Under PerMonitorV2
            // Windows rescales nothing for us, so rescale every control's
            // rect and the font by new/old, then size the window to the
            // layout at the new scale (not Windows' suggestion, which ignores
            // the work-area cap).
            const RECT* sug = (const RECT*)lParam;
            UINT newDpi = HIWORD(wParam);
            int oldDlg = g_dlgDpi;
            int newDlg = CapDlgDpi((int)newDpi, MonitorFromRect(sug, MONITOR_DEFAULTTONEAREST));

            if (newDlg != oldDlg) {
                g_dlgDpi = newDlg;
                HFONT hOldFont = hFontSegoe;
                hFontSegoe = NULL; // SetModernFont recreates it at the new DPI
                for (HWND c = GetWindow(hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
                    RECT rc;
                    GetWindowRect(c, &rc);
                    MapWindowPoints(NULL, hwnd, (LPPOINT)&rc, 2);
                    int x = MulDiv(rc.left, newDlg, oldDlg);
                    int y = MulDiv(rc.top, newDlg, oldDlg);
                    int w = MulDiv(rc.right - rc.left, newDlg, oldDlg);
                    int h = MulDiv(rc.bottom - rc.top, newDlg, oldDlg);
                    // A drop-down combo's window height is its LIST height;
                    // its rect reports only the closed field, so use the
                    // height it was created with.
                    TCHAR cls[16];
                    if (GetClassName(c, cls, 16) && lstrcmpi(cls, _T("ComboBox")) == 0) h = DS(220);
                    if (hOldFont && (HFONT)SendMessage(c, WM_GETFONT, 0, 0) == hOldFont)
                        SetModernFont(c);
                    MoveWindow(c, x, y, w, h, FALSE);
                }
                if (hOldFont) DeleteObject(hOldFont);
            }

            RECT rcNeed = { 0, 0, DS(g_dlgClientW96), DS(g_dlgClientH96) };
            AdjustRectForDpi(&rcNeed, hwnd, newDpi);
            SetWindowPos(hwnd, NULL, sug->left, sug->top,
                         rcNeed.right - rcNeed.left, rcNeed.bottom - rcNeed.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_CLOSE:
            RevertPreview(hwnd); // X button = Cancel
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            // Cleanup here (not WM_CLOSE) so Save/Cancel paths that call
            // DestroyWindow directly also release the GDI resources.
            if (hBrushDark) { DeleteObject(hBrushDark); hBrushDark = NULL; }
            if (hBrushEdit) { DeleteObject(hBrushEdit); hBrushEdit = NULL; }
            if (hBrushSep)  { DeleteObject(hBrushSep);  hBrushSep = NULL; }
            if (hBrushBad)  { DeleteObject(hBrushBad);  hBrushBad = NULL; }
            if (hFontSegoe) { DeleteObject(hFontSegoe); hFontSegoe = NULL; }
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DoSettingsDialog(HWND parent) {
    if (g_settingsOpen) return; // Tray clicks can still reach the disabled parent
    g_settingsOpen = true;
    const TCHAR CLASS_NAME[] = _T("EdgeClockSettings");
    WNDCLASS wc = { };
    if (!GetClassInfo(GetModuleHandle(NULL), CLASS_NAME, &wc)) {
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Stock brush, no leak
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClass(&wc);
    }
    
    // WM_CREATE lays the controls out and resizes/centres the window for the
    // actual DPI, so the initial size here is only a placeholder. The initial
    // POSITION matters though: it must already be on the monitor under the
    // cursor, because that is what WM_CREATE's DPI query sees. CW_USEDEFAULT
    // put it on the primary monitor and the layout came out at the wrong
    // scale once it was moved to a monitor with different scaling.
    POINT ptCur;
    GetCursorPos(&ptCur);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    RECT area;
    if (GetMonitorInfo(MonitorFromPoint(ptCur, MONITOR_DEFAULTTOPRIMARY), &mi)) area = mi.rcWork;
    else SetRect(&area, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    // Hidden until laid out, so it never flashes at the placeholder spot
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, CLASS_NAME, L(S_TITLE),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        area.left, area.top, 600, 480, parent, NULL, GetModuleHandle(NULL), NULL);
    if (!hDlg) { g_settingsOpen = false; return; }
    
    // Enable Dark Mode for Title Bar
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    ShowWindow(hDlg, SW_SHOW); // After the dark caption is set: no white flash
    SetForegroundWindow(hDlg);

    EnableWindow(parent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        // IsDialogMessage gives us Tab/Shift+Tab/arrow navigation plus
        // Enter -> IDOK and Esc -> IDCANCEL on this plain popup window.
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    g_settingsOpen = false;

    // comdlg32 and the common controls leave a sizeable working set behind
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}


// Begins a slide animation from the current position, capturing the start
// time/position for the time-based interpolation in the animation timer.
void StartSlide(HWND hwnd, AnimState newState) {
    currentState = newState;

    // Coming back into view: the cached surface still holds whatever the time
    // was when we hid, and no text timer ran while hidden. Redraw NOW rather
    // than arming a timer — arming alone was the old behaviour and it meant the
    // clock slid in showing a stale minute for up to a full minute.
    // RefreshClockText is a no-op when the text has not actually changed, so a
    // quick hover in and out still costs no GDI+ work.
    if (newState == STATE_SLIDING_UP) {
        RefreshClockText(hwnd);
        ArmClockTimer(hwnd); // Resume boundary ticks (killed while hidden)
    }

    animStartTick = GetTickCount64();
    animStartY = currentYVal;
    SetTimer(hwnd, 3, Config::refreshRate, NULL);
}

// Re-asserts topmost z-order. PresentClock() moves the window without touching
// z-order, so this runs only at animation end and on each text update.
void AssertTopmost(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Schedules the clock-text timer to fire just after the next boundary it can
// possibly change on: the next second when the format shows seconds, the next
// minute otherwise. Compared to polling every second this drops the idle wakeup
// rate to once per minute, and the text never lags the real clock.
void ArmClockTimer(HWND hwnd) {
    // Hidden: nothing is on screen to keep current, and the surface has been
    // freed. StartSlide() redraws on the way back in, so stop waking at all
    // instead of firing once a minute to do nothing.
    if (currentState == STATE_HIDDEN) {
        KillTimer(hwnd, 2);
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    // Per-second ticks only in the states where WM_TIMER 2 actually redraws.
    bool seconds = FormatHasSeconds() &&
                   (currentState == STATE_VISIBLE || currentState == STATE_SLIDING_UP);

    // Signed arithmetic on purpose. wSecond is 0-59 normally but Windows reports
    // 60 during a leap second when leap-second support is on; as UINT that made
    // (60 - 60) * 1000 - wMilliseconds wrap to ~4.29e9, which SetTimer clamps to
    // USER_TIMER_MAXIMUM — a 24-day freeze. Clamped both ends now, so the worst
    // any arithmetic slip here can cost is one late tick.
    int delay = seconds ? (1000 - (int)st.wMilliseconds)
                        : ((60 - (int)st.wSecond) * 1000 - (int)st.wMilliseconds);
    delay += 35;                          // Land past the boundary, clear of the
                                          // ~15.6 ms timer granularity
    if (delay < 50) delay = 50;           // Never spin
    if (delay > 61000) delay = 61000;     // Never sleep past the next minute.
                                          // The largest legitimate value is
                                          // 60035, so this only ever catches a
                                          // slip, never a real boundary.
    SetTimer(hwnd, 2, (UINT)delay, NULL);
}

// --- Event-driven visibility -------------------------------------------------
// Most of what decides whether the clock hides — the foreground window and the
// taskbar sliding in and out — is something Windows can tell us about, so we
// listen for those instead of asking 200 times a minute. A timer still runs as
// a safety net (a missed event must never leave the clock stranded over a
// fullscreen app) and to watch the mouse, which a click-through window cannot
// be notified about. Its interval adapts to how close the cursor is.
HWND g_hwndMain = NULL;
HWINEVENTHOOK g_hookForeground = NULL;
HWINEVENTHOOK g_hookTaskbar = NULL;

void EvaluateVisibility(HWND hwnd); // fwd decl

#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

// True when w is a real application window sized exactly to the clock's
// monitor — what a fullscreen game, video player or F11 browser looks like.
// Exact equality on purpose: a maximised window overhangs the monitor by its
// resize border, so "covers" would hide the clock behind every maximised window.
bool CoversClockMonitor(HWND w) {
    if (!w || w == g_hwndMain) return false;
    RECT rc;
    if (!GetWindowRect(w, &rc) || !EqualRect(&rc, &g_mon)) return false;
    TCHAR cls[64];
    if (!GetClassName(w, cls, 64)) return false;
    // The desktop and the taskbar can be monitor-sized without being "fullscreen"
    return _tcscmp(cls, _T("Progman")) != 0 && _tcscmp(cls, _T("WorkerW")) != 0 &&
           _tcscmp(cls, _T("Shell_TrayWnd")) != 0 && _tcscmp(cls, _T("Shell_SecondaryTrayWnd")) != 0;
}

// Finds the top-most real window over the centre of the clock's monitor and
// asks whether it is fullscreen there. Needed because the foreground window
// may be on another monitor while a fullscreen video plays on ours.
// Walks z-order with plain rect tests rather than WindowFromPoint, which sends
// WM_NCHITTEST across processes and could stall on a hung app.
bool TopWindowCoversClockMonitor() {
    POINT c = { (g_mon.left + g_mon.right) / 2, (g_mon.top + g_mon.bottom) / 2 };
    for (HWND w = GetTopWindow(NULL); w; w = GetWindow(w, GW_HWNDNEXT)) {
        if (w == g_hwndMain || !IsWindowVisible(w) || IsIconic(w)) continue;
        // Overlays (game bars, GPU overlays, click-through widgets like this
        // clock) can be monitor-sized but are never "the app in fullscreen".
        LONG ex = GetWindowLong(w, GWL_EXSTYLE);
        if (ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) continue;
        if ((ex & WS_EX_LAYERED) && (ex & WS_EX_TRANSPARENT)) continue;
        // Suspended UWP apps keep WS_VISIBLE but are cloaked (not drawn)
        BOOL cloaked = FALSE;
        if (SUCCEEDED(DwmGetWindowAttribute(w, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
            continue;
        RECT rc;
        if (!GetWindowRect(w, &rc) || !PtInRect(&rc, c)) continue;
        return CoversClockMonitor(w); // The first real window there decides
    }
    return false;
}

// True when a taskbar at rcTray overlaps the clock's visible position.
// The bar's rect is shrunk by the threshold first: an auto-hidden bar leaves a
// 1-2 px sliver on screen, which must not count as raised. This one overlap
// test covers every edge the bar can be docked to (a raised right-hand bar
// over a right-corner clock used to be ignored), and an always-visible bar
// never overlaps because the clock is anchored to the work area beside it.
bool TrayCoversClock(RECT rcTray) {
    int thr = Config::taskbarThreshold;
    InflateRect(&rcTray, -thr, -thr);
    RECT rcClock = { ClockX(), targetY, ClockX() + clockSize.cx, targetY + clockSize.cy };
    // Only the on-screen part counts: a negative offset pushes the window past
    // the monitor edge, and that invisible strip used to "touch" the
    // auto-hidden sliver and keep the clock hidden for good.
    RECT rcOnScreen, rcHit;
    if (!IntersectRect(&rcOnScreen, &rcClock, &g_mon)) return false;
    return IntersectRect(&rcHit, &rcTray, &rcOnScreen) != FALSE;
}

// Picks the safety-net interval: snappy when the cursor is near the clock,
// relaxed otherwise, and slowest of all while the clock is already hidden.
void ArmLogicTimer(HWND hwnd) {
    UINT interval;
    POINT pt;
    if (GetCursorPos(&pt)) {
        const int NEAR_PX = 200;
        int x = ClockX(), y = targetY;
        int dx = (pt.x < x) ? x - pt.x : (pt.x > x + clockSize.cx ? pt.x - x - clockSize.cx : 0);
        int dy = (pt.y < y) ? y - pt.y : (pt.y > y + clockSize.cy ? pt.y - y - clockSize.cy : 0);
        bool nearClock = (dx < NEAR_PX && dy < NEAR_PX);
        interval = nearClock ? 120 : (currentState == STATE_HIDDEN ? 1000 : 500);
    } else {
        interval = 500;
    }
    SetTimer(hwnd, 1, interval, NULL);
}

// Decides whether the clock should be hidden and starts the matching
// slide. Driven both by the safety-net timer and by the WinEvent hooks,
// so it must stay cheap.
void EvaluateVisibility(HWND hwnd) {
        // Manually hidden and already parked: nothing can change this until
        // the user toggles it back, so stop polling entirely.
        if (manualHidden && currentState == STATE_HIDDEN) {
            KillTimer(hwnd, 1);
            return;
        }

        bool shouldHide = manualHidden;

        // Mouse Hover Check (Check against the visible position to prevent flickering)
        if (!shouldHide) {
            POINT ptMouse;
            GetCursorPos(&ptMouse);
            int clkX = ClockX();
            int clkY = targetY; // Use targetY (visible position)
            if (ptMouse.x >= clkX && ptMouse.x <= clkX + clockSize.cx &&
                ptMouse.y >= clkY && ptMouse.y <= clkY + clockSize.cy) {
                shouldHide = true;
            }
        }

        // Taskbar check — purely geometric so this costs no cross-process
        // calls: the bar must be a horizontal one, sitting on the same
        // monitor and the same edge as the clock, and actually raised.
        // (A side or opposite-edge taskbar can never cover us.)
        if (!shouldHide) {
            static HWND s_hTray = NULL;
            RECT rcTray, rcHit;

            // The cached handle must still exist AND still be the bar on our
            // monitor — on multi-monitor setups the clock may live on a
            // display served by a Shell_SecondaryTrayWnd instead.
            bool ok = s_hTray && IsWindow(s_hTray) &&
                      GetWindowRect(s_hTray, &rcTray) &&
                      IntersectRect(&rcHit, &rcTray, &g_mon);
            if (!ok) {
                s_hTray = FindWindow(_T("Shell_TrayWnd"), NULL);
                ok = s_hTray && GetWindowRect(s_hTray, &rcTray) &&
                     IntersectRect(&rcHit, &rcTray, &g_mon);
                if (!ok) {
                    HWND sec = NULL;
                    while ((sec = FindWindowEx(NULL, sec, _T("Shell_SecondaryTrayWnd"), NULL)) != NULL) {
                        if (GetWindowRect(sec, &rcTray) && IntersectRect(&rcHit, &rcTray, &g_mon)) {
                            s_hTray = sec;
                            ok = true;
                            break;
                        }
                    }
                }
            }

            if (ok && TrayCoversClock(rcTray)) shouldHide = true;
        }

        // Fullscreen detection. Everything here is scoped to the clock's own
        // monitor: a fullscreen video on another display has nothing to hide
        // from, and the global shell state used to hide the clock everywhere.
        //
        // The expensive parts (the shell query is cross-process, the z-order
        // walk touches every top-level window) run only when the foreground
        // window changed or every 5th tick, to catch in-place F11 toggles.
        if (!shouldHide) {
            static HWND s_lastFore = NULL;
            static int s_slowTick = 0;
            static bool s_slowHide = false;

            HWND hFore = GetForegroundWindow();
            if (hFore != s_lastFore || ++s_slowTick >= 5) {
                s_lastFore = hFore;
                s_slowTick = 0;
                s_slowHide = false;

                QUERY_USER_NOTIFICATION_STATE quns;
                if (SUCCEEDED(SHQueryUserNotificationState(&quns))) {
                    if (quns == QUNS_PRESENTATION_MODE) {
                        s_slowHide = true; // A deliberate, system-wide setting
                    } else if (quns == QUNS_RUNNING_D3D_FULL_SCREEN) {
                        // Global state: only ours if the app doing it has
                        // focus on our monitor (exclusive-mode games may not
                        // have a window rect that matches the monitor).
                        // Not QUNS_BUSY: Windows keeps reporting it while a
                        // fullscreen game sits MINIMISED (measured with
                        // Roblox), which hid the clock after alt-tabbing out.
                        // A real borderless-fullscreen window is caught by
                        // the rect checks below and per tick, so BUSY only
                        // ever added false hides.
                        s_slowHide = !g_hMonClock || (hFore &&
                            MonitorFromWindow(hFore, MONITOR_DEFAULTTONULL) == g_hMonClock);
                    }
                }
                // Fullscreen on our monitor while focus is on another one
                // (watching a video while working next to it).
                if (!s_slowHide) s_slowHide = TopWindowCoversClockMonitor();
            }
            if (s_slowHide) shouldHide = true;
        }

        // Cheap per-tick check: the foreground window exactly covers our monitor
        if (!shouldHide && CoversClockMonitor(GetForegroundWindow())) shouldHide = true;

        // Perform logic check (should we hide?)
        if (shouldHide) {
            if (currentState == STATE_VISIBLE || currentState == STATE_SLIDING_UP) {
                StartSlide(hwnd, STATE_SLIDING_DOWN);
            }
        } else {
            if (currentState == STATE_HIDDEN || currentState == STATE_SLIDING_DOWN) {
                StartSlide(hwnd, STATE_SLIDING_UP);
            }
        }
}

void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
                           LONG idObject, LONG, DWORD, DWORD) {
    if (!g_hwndMain) return;
    // LOCATIONCHANGE also fires for carets and the mouse cursor; only whole
    // windows moving can uncover or cover the clock.
    if (event == EVENT_OBJECT_LOCATIONCHANGE && (idObject != OBJID_WINDOW || !hwnd)) return;
    EvaluateVisibility(g_hwndMain);
}

// The taskbar hook is scoped to explorer's tray thread so we are not woken by
// every window move on the system.
void InstallVisibilityHooks(HWND hwnd) {
    g_hwndMain = hwnd;

    if (!g_hookForeground) {
        g_hookForeground = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                          NULL, WinEventProc, 0, 0,
                                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    if (g_hookTaskbar) { UnhookWinEvent(g_hookTaskbar); g_hookTaskbar = NULL; }
    HWND hTray = FindWindow(_T("Shell_TrayWnd"), NULL);
    if (hTray) {
        DWORD pid = 0;
        DWORD tid = GetWindowThreadProcessId(hTray, &pid);
        g_hookTaskbar = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
                                        NULL, WinEventProc, pid, tid,
                                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
}

void RemoveVisibilityHooks() {
    if (g_hookForeground) { UnhookWinEvent(g_hookForeground); g_hookForeground = NULL; }
    if (g_hookTaskbar) { UnhookWinEvent(g_hookTaskbar); g_hookTaskbar = NULL; }
    g_hwndMain = NULL;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Re-add tray icon when Explorer restarts (icon is lost otherwise)
    if (g_msgTaskbarCreated && uMsg == g_msgTaskbarCreated) {
        Shell_NotifyIcon(NIM_ADD, &nid);
        UpdateScreenMetrics();
        RecalculateAll(hwnd);
        InstallVisibilityHooks(hwnd); // Explorer restarted: re-scope the tray hook
        return 0;
    }

    switch (uMsg) {
    case WM_CREATE:
        g_msgTaskbarCreated = RegisterWindowMessage(_T("TaskbarCreated"));
        UpdateScreenMetrics();
        RecalculateAll(hwnd);
        currentState = STATE_VISIBLE;
        currentYVal = (float)targetY; 
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        PresentClock(hwnd, ClockX(), (int)currentYVal, Config::opacity);
        AssertTopmost(hwnd);

        InitTrayIcon(hwnd);

        InstallVisibilityHooks(hwnd);
        ArmLogicTimer(hwnd);
        ArmClockTimer(hwnd);

        // Trim RAM after initialization
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        if (uMsg == WM_DISPLAYCHANGE || (uMsg == WM_SETTINGCHANGE && wParam == SPI_SETWORKAREA)) {
            UpdateScreenMetrics();
            RecalculateAll(hwnd);
            return 0;
        }
        break;

    case 0x02E0 /*WM_DPICHANGED*/:
        // PerMonitorV2: re-measure text and reposition on the new DPI
        UpdateScreenMetrics();
        RecalculateAll(hwnd);
        return 0;

    case WM_TIMECHANGE:
        // Clock set by the user, NTP or a DST shift: the pending timer was armed
        // against the old time, so redraw and re-arm against the new one.
        RecalculateAll(hwnd);
        return 0;

    case WM_POWERBROADCAST:
        // Resume from sleep. WM_TIMER 2 was armed against pre-sleep ticks and
        // under IDLE priority + EcoQoS its delivery can lag, so correct the text
        // now instead of showing pre-sleep time while the timer catches up.
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            if (RefreshClockText(hwnd)) AssertTopmost(hwnd);
            ArmClockTimer(hwnd);
            EvaluateVisibility(hwnd);
        }
        return TRUE;

    case WM_TRAYICON:
        // Settings is modal over live config. Letting the tray menu run under
        // it would SaveConfig() half-edited preview values that Cancel then
        // only reverts in memory — so any tray click just surfaces the dialog.
        if (g_settingsOpen) {
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                HWND hDlg = FindWindow(_T("EdgeClockSettings"), NULL);
                if (hDlg) SetForegroundWindow(hDlg);
            }
            return 0;
        }
        if (g_uiBusy) { // Import/Export dialog or its message box is up
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                HWND hPop = GetLastActivePopup(hwnd);
                if (hPop && hPop != hwnd) SetForegroundWindow(hPop);
            }
            return 0;
        }
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        } else if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            DoSettingsDialog(hwnd); // Left-click opens Settings
        }
        return 0;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
            case ID_MENU_EXIT:
                DestroyWindow(hwnd);
                break;
            case ID_SIZE_SMALL:
                Config::fontSize = Config::sizeSmall;
                SaveConfig();
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_MEDIUM:
                Config::fontSize = Config::sizeMedium;
                SaveConfig();
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_LARGE:
                Config::fontSize = Config::sizeLarge;
                SaveConfig();
                RecalculateAll(hwnd);
                break;
            case ID_MENU_SETTINGS:
                DoSettingsDialog(hwnd);
                break;
            case ID_MENU_STARTUP:
                SetStartup(!IsStartupEnabled());
                break;
            case ID_MENU_TOGGLE_HIDE:
                manualHidden = !manualHidden;
                // Restart the safety net (it stops itself while manually hidden)
                ArmLogicTimer(hwnd);
                EvaluateVisibility(hwnd);
                break;
            case ID_LANG_AUTO:
            case ID_LANG_EN:
            case ID_LANG_TH:
                Config::language = (wmId == ID_LANG_AUTO) ? 0 : (wmId == ID_LANG_EN) ? 1 : 2;
                ApplyLanguage();
                SaveConfig();
                // Day/month names may have changed language with it
                if (RefreshClockText(hwnd)) AssertTopmost(hwnd);
                break;
            // The file dialogs and message boxes below pump messages, so tray
            // clicks still arrive while they are up. g_uiBusy stops those from
            // opening Settings over an import: Settings would snapshot the
            // pre-import config, and Cancel would then restore it in memory
            // while the registry kept the imported values.
            case ID_MENU_EXPORT:
                g_uiBusy = true;
                if (!ExportSettings(hwnd))
                    MessageBox(hwnd, L(S_EXPORT_FAIL), L(S_TITLE), MB_OK | MB_ICONWARNING);
                g_uiBusy = false;
                break;
            case ID_MENU_IMPORT:
                g_uiBusy = true;
                switch (ImportSettings(hwnd)) {
                    case 1:
                        UpdateScreenMetrics();
                        RecalculateAll(hwnd);
                        MessageBox(hwnd, L(S_IMPORT_OK), L(S_TITLE), MB_OK | MB_ICONINFORMATION);
                        break;
                    case 0:
                        MessageBox(hwnd, L(S_IMPORT_FAIL), L(S_TITLE), MB_OK | MB_ICONWARNING);
                        break;
                    default: break; // Cancelled
                }
                g_uiBusy = false;
                break;
        }
        return 0;
    }

    // --- Animation Logic ---
    // Handles the sliding up/down animation of the clock with floating point precision.
    case WM_TIMER: {
        if (wParam == 3) { // Animation Timer
            bool animating = false;
            bool parked = false; // Slide-down just finished; release after presenting

            // TIME-BASED ANIMATION with ease-out cubic.
            // Progress derives from wall-clock elapsed time, so the slide
            // always finishes in exactly animDuration ms even when timer
            // ticks are delayed (IDLE priority / EcoQoS).
            if (currentState == STATE_SLIDING_UP || currentState == STATE_SLIDING_DOWN) {
                float endY = (currentState == STATE_SLIDING_UP) ? (float)targetY : (float)HiddenY();
                float totalTime = (float)Config::animDuration;

                float progress;
                if (totalTime <= 0.0f) {
                    progress = 1.0f;
                } else {
                    progress = (float)(GetTickCount64() - animStartTick) / totalTime;
                    if (progress > 1.0f) progress = 1.0f;
                }

                // Ease-out cubic: fast start, gentle landing
                float inv = 1.0f - progress;
                float eased = 1.0f - inv * inv * inv;

                currentYVal = animStartY + (endY - animStartY) * eased;
                animating = true;

                // Cross-fade alongside the slide, derived from POSITION rather
                // than progress: if a slide reverses mid-flight the alpha stays
                // continuous instead of snapping back to the start of the ramp.
                float hidY = (float)HiddenY();
                float span = (float)targetY - hidY;
                float fade = (span != 0.0f) ? (currentYVal - hidY) / span : 1.0f;
                if (fade < 0.0f) fade = 0.0f;
                if (fade > 1.0f) fade = 1.0f;
                g_animAlpha = (int)(fade * 100.0f);

                if (progress >= 1.0f) {
                    currentYVal = endY;
                    bool wasUp = (currentState == STATE_SLIDING_UP);
                    currentState = wasUp ? STATE_VISIBLE : STATE_HIDDEN;
                    g_animAlpha = wasUp ? 100 : 0;
                    KillTimer(hwnd, 3);
                    if (wasUp) AssertTopmost(hwnd);
                    else       parked = true;
                }
            } else if (currentState == STATE_VISIBLE) {
                 if ((int)currentYVal != targetY) {
                     currentYVal = (float)targetY;
                     g_animAlpha = 100;
                     animating = true;
                 }
                 KillTimer(hwnd, 3);
            } else {
                 KillTimer(hwnd, 3); // Hidden — nothing to animate
            }

            if (animating) {
                PresentClock(hwnd, ClockX(), (int)currentYVal,
                             Config::opacity * g_animAlpha / 100);
            }

            // Strictly after the present above: FreeSurface() first would turn
            // that PresentClock into a no-op and strand the clock one frame
            // short of hidden.
            if (parked) {
                ArmClockTimer(hwnd); // Hidden — stops the text timer entirely
                FreeSurface();       // The DIB is the bulk of the idle footprint
                g_lastRendered[0] = _T('\0');
                SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            }
            return 0;
        }

        if (wParam == 2) { // Clock text update (fires on the second/minute boundary)
            if (RefreshClockText(hwnd)) AssertTopmost(hwnd);
            // Re-armed unconditionally and last, so no path out of this handler
            // can leave the clock with no pending tick. ArmClockTimer stops the
            // timer by itself when hidden.
            ArmClockTimer(hwnd);
            return 0;
        }

        if (wParam == 1) { // Visibility safety net
            EvaluateVisibility(hwnd);
            // EvaluateVisibility stops the timer once the clock is parked by the
            // user; re-arming unconditionally here would defeat that.
            if (!(manualHidden && currentState == STATE_HIDDEN)) ArmLogicTimer(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_ENDSESSION:
        // Graceful cleanup when Windows shuts down or user logs off
        if (wParam) {
            // With Settings open, Config holds uncommitted preview values;
            // the registry already has the last saved state.
            if (!g_settingsOpen) SaveConfig();
            RemoveTrayIcon();
        }
        return 0;

    case WM_APP + 1:
        // A second instance was launched — treat it as "show me"
        manualHidden = false;
        ArmLogicTimer(hwnd);
        EvaluateVisibility(hwnd);
        AssertTopmost(hwnd);
        return 0;

    case WM_DESTROY:
        RemoveVisibilityHooks();
        RemoveTrayIcon();
        DestroyTrayIconImage();
        FreeSurface();
        FreeFontCache();
        if (g_gdiplusUp) { GdiplusShutdown(gdiplusToken); g_gdiplusUp = false; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}



// Enable Efficiency Mode (EcoQoS) for Windows 11
// This reduces the clock's power consumption background impact.
void EnableEfficiencyMode() {
    // Defines for EcoQoS (in case they are missing from older headers)
    #ifndef PROCESS_POWER_THROTTLING_CURRENT_VERSION
    #define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
    #endif

    #ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
    #define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
    #endif

    #ifndef ProcessPowerThrottling
    #define ProcessPowerThrottling (PROCESS_INFORMATION_CLASS)4
    #endif

    typedef struct _PROCESS_POWER_THROTTLING_STATE_LOCAL {
        ULONG Version;
        ULONG ControlMask;
        ULONG StateMask;
    } PROCESS_POWER_THROTTLING_STATE_LOCAL;

    // 1. Set Priority to IDLE (Low)
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

    // 2. Enable EcoQoS (Power Throttling) via Dynamic Loading
    HMODULE hKernel32 = GetModuleHandle(_T("kernel32.dll"));
    if (hKernel32) {
        typedef BOOL (WINAPI *SetProcessInformationFunc)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
        SetProcessInformationFunc pSetProcessInformation =
            (SetProcessInformationFunc)(void*)GetProcAddress(hKernel32, "SetProcessInformation");

        if (pSetProcessInformation) {
            PROCESS_POWER_THROTTLING_STATE_LOCAL PowerThrottling;
            RtlZeroMemory(&PowerThrottling, sizeof(PowerThrottling));
            PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
            PowerThrottling.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;

            pSetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling, sizeof(PowerThrottling));
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Log("Starting EdgeClock...");
    
    // --- Single Instance Check ---
    g_hMutex = CreateMutex(NULL, TRUE, _T("EdgeClock_GlobalInstance_Mutex"));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Log("EdgeClock is already running. Exiting redundant instance.");
        if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = NULL; }
        
        // Find the existing window and bring it to the front if it's hidden behind something
        HWND hExisting = FindWindowEx(NULL, NULL, _T("EdgeClockTray"), _T("Edge Clock"));
        if (hExisting) {
            PostMessage(hExisting, WM_APP + 1, 0, 0); // Custom message to wake it up if needed.
        }
        
        return 0; // Exit immediately
    }

    // Enable Efficiency Mode / EcoQoS immediately
    EnableEfficiencyMode();

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Init Common Controls for "Modern" Visual Styles (requires manifest)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES; // Add BAR_CLASSES for Trackbar
    InitCommonControlsEx(&icex);

    LoadConfig();
    ApplyLanguage();

    GdiplusEnsure();
    Log("GDI+ Started.");

    const TCHAR CLASS_NAME[] = _T("EdgeClockTray");
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    
    if (!RegisterClass(&wc)) {
        Log("RegisterClass Failed!");
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        CLASS_NAME, _T("Edge Clock"),
        WS_POPUP,
        0, 0, 100, 50,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        Log("CreateWindow Failed!");
        return 0;
    }

    Log("Entering Message Loop...");
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CoUninitialize();
    if (g_hMutex) { ReleaseMutex(g_hMutex); CloseHandle(g_hMutex); g_hMutex = NULL; }
    return 0;
}
