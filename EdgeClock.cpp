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
#include <gdiplus.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlobj.h>
#include <objbase.h>
#include <commctrl.h> 
#include <commdlg.h> 
#include <dwmapi.h> 

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif


using namespace Gdiplus;

// =============================================================
//   CONFIGURATION
// =============================================================
static HANDLE g_hMutex = NULL; // Global mutex for single-instance

void Log(const char* msg) {
    static char logPath[MAX_PATH] = {0};
    if (!logPath[0]) {
        // Resolve log path next to the exe, not relative to CWD
        GetModuleFileNameA(NULL, logPath, MAX_PATH);
        char* lastSlash = strrchr(logPath, '\\');
        if (lastSlash) strcpy(lastSlash + 1, "EdgeClock_Log.txt");
        else strcpy(logPath, "EdgeClock_Log.txt");
    }
    FILE* f = fopen(logPath, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

namespace Config {
    // --- Appearance Settings ---
    // These control the visual look of the clock.
    float fontSize = 14.0f;                  
    WCHAR fontName[32] = L"Segoe UI"; // Default font
    float outlineWidth = 2.0f;         
    COLORREF textColor = RGB(255, 255, 255);
    COLORREF outlineColor = RGB(0, 0, 0);
    
    // --- Animation & Performance ---
    // animDuration: How long the slide animation takes in milliseconds.
    int animDuration = 500;       
    int refreshRate = 10;         // Timer tick interval in ms
    
    // --- Tray Icon ---
    const TCHAR trayTooltip[] = _T("Edge Clock");
    
    // --- Auto-Hide Logic ---
    // Thresholds for determining when to hide the clock.
    int taskbarThreshold = 4;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // --- Clock Text ---
    WCHAR timeFormat[32] = L"%H:%M"; // strftime format (e.g. %H:%M:%S, %I:%M %p, %a %d %H:%M)
    int opacity = 100;               // Clock opacity in percent (20-100)

    // --- Menu Presets ---
    const float sizeSmall = 16.0f;
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

void LoadConfig() {
    Log("Loading Config...");
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\EdgeClock"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD sizeFloat = sizeof(float);
        DWORD sizeInt = sizeof(int);
        
        // Try reading floats as binary first (backward compatibility might fail effectively but resets to default)
        RegQueryValueEx(hKey, _T("FontSize"), NULL, NULL, (LPBYTE)&Config::fontSize, &sizeFloat);
        RegQueryValueEx(hKey, _T("OutlineWidth"), NULL, NULL, (LPBYTE)&Config::outlineWidth, &sizeFloat);
        
        RegQueryValueEx(hKey, _T("AnimDuration"), NULL, NULL, (LPBYTE)&Config::animDuration, &sizeInt);
        RegQueryValueEx(hKey, _T("TaskbarThreshold"), NULL, NULL, (LPBYTE)&Config::taskbarThreshold, &sizeInt);
        RegQueryValueEx(hKey, _T("OffsetX"), NULL, NULL, (LPBYTE)&Config::offsetX, &sizeFloat);
        RegQueryValueEx(hKey, _T("OffsetY"), NULL, NULL, (LPBYTE)&Config::offsetY, &sizeFloat);
        RegQueryValueEx(hKey, _T("TextColor"), NULL, NULL, (LPBYTE)&Config::textColor, &sizeInt);
        RegQueryValueEx(hKey, _T("OutlineColor"), NULL, NULL, (LPBYTE)&Config::outlineColor, &sizeInt);
        
        DWORD currentSize = sizeof(Config::fontName);
        if (RegQueryValueEx(hKey, _T("FontName"), NULL, NULL, (LPBYTE)Config::fontName, &currentSize) == ERROR_SUCCESS) {
             Config::fontName[31] = L'\0'; // Safety null-terminate
        }

        DWORD fmtSize = sizeof(Config::timeFormat);
        if (RegQueryValueEx(hKey, _T("TimeFormat"), NULL, NULL, (LPBYTE)Config::timeFormat, &fmtSize) == ERROR_SUCCESS) {
             Config::timeFormat[31] = L'\0';
        }
        RegQueryValueEx(hKey, _T("Opacity"), NULL, NULL, (LPBYTE)&Config::opacity, &sizeInt);

        RegCloseKey(hKey);
    }

    // Safety Checks
    if (Config::fontSize < 4.0f) Config::fontSize = 14.0f;
    if (Config::fontSize > 200.0f) Config::fontSize = 72.0f;
    if (Config::animDuration < 0) Config::animDuration = 500;
    if (Config::opacity < 20) Config::opacity = 20;
    if (Config::opacity > 100) Config::opacity = 100;
    if (!IsValidTimeFormat(Config::timeFormat)) lstrcpyW(Config::timeFormat, L"%H:%M");
    Log("Config Loaded.");
}
// =============================================================

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1001

#define ID_MENU_EXIT 2001
#define ID_SIZE_SMALL 2002
#define ID_SIZE_MEDIUM 2003
#define ID_SIZE_LARGE 2004
#define ID_SIZE_CUSTOM 2005 
#define ID_MENU_TOGGLE_HIDE 2006 
#define ID_MENU_STARTUP 2007 
#define ID_MENU_SETTINGS 2008

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
// Bottom-right corner (absolute virtual-screen coords) of the target monitor
int screenH = 0;
int screenW = 0;
SIZE clockSize = {0, 0};
bool manualHidden = false;

// Time-based animation state (immune to timer jitter under IDLE priority)
ULONGLONG animStartTick = 0;
float animStartY = 0.0f;

UINT g_msgTaskbarCreated = 0; // "TaskbarCreated" broadcast (Explorer restart)

ULONG_PTR gdiplusToken;
NOTIFYICONDATA nid = { 0 };

// Resolve the monitor that hosts the taskbar (falls back to primary) and
// cache its absolute right/bottom edge for clock positioning.
void UpdateScreenMetrics() {
    HWND hTray = FindWindow(_T("Shell_TrayWnd"), NULL);
    HMONITOR hMon;
    if (hTray) {
        hMon = MonitorFromWindow(hTray, MONITOR_DEFAULTTOPRIMARY);
    } else {
        POINT pt = {0, 0};
        hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(hMon, &mi)) {
        screenW = mi.rcMonitor.right;
        screenH = mi.rcMonitor.bottom;
    } else {
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
    }
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

void UpdateLayeredWindowContent(HWND hwnd) {
    int width = clockSize.cx;
    int height = clockSize.cy;

    if (width <= 0 || height <= 0) return;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Create GDI+ Graphics object for high-quality rendering
    Graphics graphics(hdcMem);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    // Clear background with fully transparent color
    graphics.Clear(Color(0, 0, 0, 0));

    // Robust Font Loading
    FontFamily* pFamily = new FontFamily(Config::fontName);
    if (pFamily->GetLastStatus() != Ok) {
        delete pFamily;
        pFamily = new FontFamily(L"Segoe UI");
        if (pFamily->GetLastStatus() != Ok) {
             delete pFamily;
             pFamily = new FontFamily(L"Arial");
        }
    }

    // Use GenericTypographic to eliminate inner padding for strict alignment
    StringFormat format(StringFormat::GenericTypographic());
    format.SetAlignment(StringAlignmentFar);
    format.SetLineAlignment(StringAlignmentFar);

    TCHAR timeBuf[64];
    GetTime(timeBuf, 64);

    GraphicsPath path;
    RectF rect(0, 0, (REAL)width - (REAL)Config::outlineWidth, (REAL)height); 
    
    path.AddString(timeBuf, -1, pFamily, FontStyleBold, (REAL)Config::fontSize, rect, &format);

    Pen pen(Color(255, GetRValue(Config::outlineColor), GetGValue(Config::outlineColor), GetBValue(Config::outlineColor)), (REAL)Config::outlineWidth);
    pen.SetLineJoin(LineJoinRound);
    graphics.DrawPath(&pen, &path);

    SolidBrush brush(Color(255, GetRValue(Config::textColor), GetGValue(Config::textColor), GetBValue(Config::textColor)));
    graphics.FillPath(&brush, &path);
    
    delete pFamily; // Clean up

    POINT ptSrc = { 0, 0 };
    SIZE size = { width, height };
    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = (BYTE)(Config::opacity * 255 / 100);
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, hdcScreen, NULL, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

// Calculates the exact size needed for the clock text
// This ensures the window is exactly the size of the text + outline,
// preventing any clipping or unnecessary empty space.
SIZE CalculateTextSize() {
    HDC hdc = GetDC(NULL);
    Graphics graphics(hdc);
    
    FontFamily* pFamily = new FontFamily(Config::fontName);
    if (pFamily->GetLastStatus() != Ok) {
        delete pFamily;
        pFamily = new FontFamily(L"Segoe UI");
        if (pFamily->GetLastStatus() != Ok) {
             delete pFamily;
             pFamily = new FontFamily(L"Arial");
        }
    }

    Font font(pFamily, (REAL)Config::fontSize, FontStyleBold, UnitPoint);

    TCHAR timeBuf[64];
    GetTime(timeBuf, 64);

    RectF boundingBox(0,0,0,0);
    PointF origin(0, 0);
    Status s = graphics.MeasureString(timeBuf, -1, &font, origin, StringFormat::GenericTypographic(), &boundingBox);
    if (s != Ok) {
        Log("MeasureString Failed!");
        // Fallback size
        boundingBox.Width = 100;
        boundingBox.Height = 50;
    }
    
    // Calcluate bounding box strictly based on content + outline
    SIZE sz;
    sz.cx = (int)ceil(boundingBox.Width + Config::outlineWidth * 2.0f); 
    sz.cy = (int)ceil(boundingBox.Height + Config::outlineWidth * 2.0f);
    
    delete pFamily;
    ReleaseDC(NULL, hdc);

    return sz;
}

void RecalculateAll(HWND hwnd) {
    clockSize = CalculateTextSize();
    targetY = screenH - clockSize.cy - (int)Config::offsetY;
    
    if (currentState == STATE_VISIBLE) {
        currentYVal = (float)targetY;
        SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - (int)Config::offsetX, targetY, clockSize.cx, clockSize.cy, SWP_NOACTIVATE);
    } else {
        SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - (int)Config::offsetX, (int)currentYVal, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    
    UpdateLayeredWindowContent(hwnd);
}

void InitTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    
    // Load Icon from Resource (ID 101)
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
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

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    
    // Use Config values for menu display logic
    // Quick size presets
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeSmall ? MF_CHECKED : 0), ID_SIZE_SMALL, _T("Size: Small"));
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeMedium ? MF_CHECKED : 0), ID_SIZE_MEDIUM, _T("Size: Normal"));
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeLarge ? MF_CHECKED : 0), ID_SIZE_LARGE, _T("Size: Large"));
    // Removed redundant "Size: Custom..." since we have Settings now
    
    AppendMenu(hMenu, MF_STRING, ID_MENU_SETTINGS, _T("Settings..."));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), ID_MENU_STARTUP, _T("Run on Startup"));
    AppendMenu(hMenu, MF_STRING, ID_MENU_TOGGLE_HIDE, manualHidden ? _T("Show Clock") : _T("Hide Clock"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, _T("Exit"));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
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
    int animDuration, opacity;
    COLORREF textColor, outlineColor;
    WCHAR fontName[32];
    WCHAR timeFormat[32];
} g_cfgSnapshot;
bool g_settingsReady = false; // Suppress preview during WM_CREATE init
bool g_settingsOpen = false;  // Re-entry guard (tray clicks reach the disabled parent)

void RecalculateAll(HWND hwnd); // fwd decl

// Reads all dialog controls + temp colors/font into Config (with clamping)
// and repaints the clock. Used for both live preview and Save.
void ApplyPreview(HWND hwnd) {
    TCHAR buf[64];

    GetDlgItemText(hwnd, ID_EDIT_FONT_SIZE, buf, 64);
    float v = (float)_wtof(buf);
    if (v < 4.0f) v = 14.0f;
    if (v > 200.0f) v = 72.0f;
    Config::fontSize = v;

    GetDlgItemText(hwnd, ID_EDIT_OUTLINE_WIDTH, buf, 64);
    v = (float)_wtof(buf);
    if (v < 0.0f) v = 0.0f;
    if (v > 50.0f) v = 50.0f;
    Config::outlineWidth = v;

    GetDlgItemText(hwnd, ID_EDIT_OFFSET_X, buf, 64);
    Config::offsetX = (float)_wtof(buf);
    GetDlgItemText(hwnd, ID_EDIT_OFFSET_Y, buf, 64);
    Config::offsetY = (float)_wtof(buf);

    GetDlgItemText(hwnd, ID_EDIT_TIME_FORMAT, buf, 64);
    buf[31] = L'\0';
    if (IsValidTimeFormat(buf)) lstrcpyW(Config::timeFormat, buf);

    Config::animDuration = (int)SendMessage(GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED), TBM_GETPOS, 0, 0) * 100;

    int op = (int)SendMessage(GetDlgItem(hwnd, ID_TRACK_OPACITY), TBM_GETPOS, 0, 0);
    if (op < 20) op = 20;
    if (op > 100) op = 100;
    Config::opacity = op;

    Config::textColor = tempTextColor;
    Config::outlineColor = tempOutlineColor;
    lstrcpyW(Config::fontName, tempFontName);

    HWND hParent = GetParent(hwnd);
    if (hParent) RecalculateAll(hParent);
}

void RevertPreview(HWND hwnd) {
    Config::fontSize = g_cfgSnapshot.fontSize;
    Config::outlineWidth = g_cfgSnapshot.outlineWidth;
    Config::offsetX = g_cfgSnapshot.offsetX;
    Config::offsetY = g_cfgSnapshot.offsetY;
    Config::animDuration = g_cfgSnapshot.animDuration;
    Config::opacity = g_cfgSnapshot.opacity;
    Config::textColor = g_cfgSnapshot.textColor;
    Config::outlineColor = g_cfgSnapshot.outlineColor;
    lstrcpyW(Config::fontName, g_cfgSnapshot.fontName);
    lstrcpyW(Config::timeFormat, g_cfgSnapshot.timeFormat);

    HWND hParent = GetParent(hwnd);
    if (hParent) RecalculateAll(hParent);
}

// Dark Mode Resources
HBRUSH hBrushDark = NULL;
HBRUSH hBrushEdit = NULL;
HFONT hFontSegoe = NULL;

void SetModernFont(HWND hwnd) {
    if (!hFontSegoe) {
        // Slightly larger, cleaner font
        hFontSegoe = CreateFont(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Segoe UI"));
    }
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFontSegoe, TRUE);
}

HWND StyleControl(HWND hCtrl) {
    SetModernFont(hCtrl);
    return hCtrl;
}

// Shared owner-draw renderer for the flat dark-grey buttons
void DrawDarkButton(LPDRAWITEMSTRUCT pDIS, LPCTSTR text) {
    COLORREF bg = (pDIS->itemState & ODS_SELECTED) ? RGB(80, 80, 80) : RGB(60, 60, 60);
    HBRUSH hBrush = CreateSolidBrush(bg);
    FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
    DeleteObject(hBrush);

    HBRUSH hBorder = CreateSolidBrush(RGB(100, 100, 100));
    FrameRect(pDIS->hDC, &pDIS->rcItem, hBorder);
    DeleteObject(hBorder);

    SetBkMode(pDIS->hDC, TRANSPARENT);
    SetTextColor(pDIS->hDC, RGB(220, 220, 220));
    HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFontSegoe);
    DrawText(pDIS->hDC, text, -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(pDIS->hDC, hOldFont);
}

// Main procedure for the Settings Dialog
// Handles all user interactions in the settings window.
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_CREATE: {
            g_settingsReady = false;

            // Snapshot live config for Cancel/revert (live preview writes to Config)
            g_cfgSnapshot.fontSize = Config::fontSize;
            g_cfgSnapshot.outlineWidth = Config::outlineWidth;
            g_cfgSnapshot.offsetX = Config::offsetX;
            g_cfgSnapshot.offsetY = Config::offsetY;
            g_cfgSnapshot.animDuration = Config::animDuration;
            g_cfgSnapshot.opacity = Config::opacity;
            g_cfgSnapshot.textColor = Config::textColor;
            g_cfgSnapshot.outlineColor = Config::outlineColor;
            lstrcpyW(g_cfgSnapshot.fontName, Config::fontName);
            lstrcpyW(g_cfgSnapshot.timeFormat, Config::timeFormat);

            // "Minimal Modern" Design - Dark Theme
            hBrushDark = CreateSolidBrush(RGB(32, 32, 32)); // Dark Background
            hBrushEdit = CreateSolidBrush(RGB(50, 50, 50)); // Darker Grey Edit Fields

            int xL = 20, xR = 100, y = 20, h = 26, gap = 40;

            // --- Font Selection ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Font"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            StyleControl(CreateWindow(_T("STATIC"), Config::fontName, WS_VISIBLE | WS_CHILD | SS_ENDELLIPSIS | SS_CENTERIMAGE, xR, y, 100, h, hwnd, (HMENU)ID_STATIC_FONT_NAME, NULL, NULL));
            // "Change" button
            StyleControl(CreateWindow(_T("BUTTON"), _T("..."), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 210, y, 50, h, hwnd, (HMENU)ID_BTN_ID_FONT, NULL, NULL));

            y += gap;
            // --- Size & Outline ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Size (pt)"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            // Use generic text edit, manual float parsing
            HWND hSize = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, xR, y, 50, h, hwnd, (HMENU)ID_EDIT_FONT_SIZE, NULL, NULL));
            TCHAR floatBuf[16];
            _stprintf(floatBuf, _T("%.1f"), Config::fontSize);
            SetWindowText(hSize, floatBuf);

            y += gap;
            StyleControl(CreateWindow(_T("STATIC"), _T("Outline"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            HWND hOutline = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, xR, y, 50, h, hwnd, (HMENU)ID_EDIT_OUTLINE_WIDTH, NULL, NULL));
             _stprintf(floatBuf, _T("%.1f"), Config::outlineWidth);
            SetWindowText(hOutline, floatBuf);

            y += gap;
            // --- Colors ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Text Color"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            hColorBtn1 = CreateWindow(_T("BUTTON"), _T(""), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xR, y, 50, h, hwnd, (HMENU)ID_BTN_TEXT_COLOR, NULL, NULL);

            StyleControl(CreateWindow(_T("STATIC"), _T("Border Clr"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 160, y, 70, h, hwnd, NULL, NULL, NULL));
            hColorBtn2 = CreateWindow(_T("BUTTON"), _T(""), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 230, y, 50, h, hwnd, (HMENU)ID_BTN_OUTLINE_COLOR, NULL, NULL);

            y += gap;
            // --- Animation ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Anim Spd"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            
            // Slider 0-20 (0.0s - 2.0s)
            HWND hTrack = CreateWindow(TRACKBAR_CLASS, _T(""), WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS | TBS_ENABLESELRANGE, xR - 10, y, 120, 30, hwnd, (HMENU)ID_TRACK_ANIM_SPEED, NULL, NULL);
            SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELPARAM(0, 20)); 
            SendMessage(hTrack, TBM_SETPOS, TRUE, Config::animDuration / 100);

            // Value Label
            TCHAR durBuf[16];
            _stprintf(durBuf, _T("%.1fs"), Config::animDuration / 1000.0);
            StyleControl(CreateWindow(_T("STATIC"), durBuf, WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 230, y, 40, h, hwnd, (HMENU)ID_STATIC_DURATION_VAL, NULL, NULL));

            y += gap;
            // --- Offsets ---
             // Allow negative & float (remove ES_NUMBER)
            StyleControl(CreateWindow(_T("STATIC"), _T("Offset X"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            HWND hOffX = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, xR, y, 50, h, hwnd, (HMENU)ID_EDIT_OFFSET_X, NULL, NULL));
             _stprintf(floatBuf, _T("%.1f"), Config::offsetX);
            SetWindowText(hOffX, floatBuf);

            StyleControl(CreateWindow(_T("STATIC"), _T("Y"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE | SS_CENTER, 160, y, 30, h, hwnd, NULL, NULL, NULL));
            HWND hOffY = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, 200, y, 50, h, hwnd, (HMENU)ID_EDIT_OFFSET_Y, NULL, NULL));
             _stprintf(floatBuf, _T("%.1f"), Config::offsetY);
            SetWindowText(hOffY, floatBuf);

            y += gap;
            // --- Time Format (strftime) ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Format"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            HWND hFmt = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER, xR, y, 160, h, hwnd, (HMENU)ID_EDIT_TIME_FORMAT, NULL, NULL));
            SetWindowText(hFmt, Config::timeFormat);

            y += gap;
            // --- Opacity ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Opacity"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, xL, y, 70, h, hwnd, NULL, NULL, NULL));
            HWND hOpTrack = CreateWindow(TRACKBAR_CLASS, _T(""), WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS, xR - 10, y, 120, 30, hwnd, (HMENU)ID_TRACK_OPACITY, NULL, NULL);
            SendMessage(hOpTrack, TBM_SETRANGE, TRUE, MAKELPARAM(20, 100));
            SendMessage(hOpTrack, TBM_SETPOS, TRUE, Config::opacity);
            TCHAR opBuf[16];
            _stprintf(opBuf, _T("%d%%"), Config::opacity);
            StyleControl(CreateWindow(_T("STATIC"), opBuf, WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 230, y, 40, h, hwnd, (HMENU)ID_STATIC_OPACITY_VAL, NULL, NULL));

            y += gap + 10;
            // --- Save / Cancel / Reset ---
            StyleControl(CreateWindow(_T("BUTTON"), _T("Save"), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 10, y, 80, 30, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL));
            StyleControl(CreateWindow(_T("BUTTON"), _T("Defaults"), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 100, y, 80, 30, hwnd, (HMENU)ID_BTN_RESET, NULL, NULL));
            StyleControl(CreateWindow(_T("BUTTON"), _T("Cancel"), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 190, y, 80, 30, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL));

            tempTextColor = Config::textColor;
            tempOutlineColor = Config::outlineColor;
            lstrcpyW(tempFontName, Config::fontName);
            g_settingsReady = true; // Enable live preview from here on
            break;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetTextColor((HDC)wParam, RGB(220, 220, 220)); // Light Text
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (INT_PTR)hBrushDark; 

        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)wParam, RGB(255, 255, 255)); // White Text
            SetBkColor((HDC)wParam, RGB(50, 50, 50)); // Dark Grey Background
            return (INT_PTR)hBrushEdit;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID == ID_BTN_SAVE) {
                // Modern Blue Accent
                COLORREF bg = (pDIS->itemState & ODS_SELECTED) ? RGB(0, 100, 180) : RGB(0, 120, 215);
                HBRUSH hBrush = CreateSolidBrush(bg);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);

                // Text
                SetBkMode(pDIS->hDC, TRANSPARENT);
                SetTextColor(pDIS->hDC, RGB(255, 255, 255));
                HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFontSegoe);
                DrawText(pDIS->hDC, _T("Save"), -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(pDIS->hDC, hOldFont);
                return TRUE;
            }
            if (pDIS->CtlID == ID_BTN_CANCEL) { DrawDarkButton(pDIS, _T("Cancel")); return TRUE; }
            if (pDIS->CtlID == ID_BTN_RESET)  { DrawDarkButton(pDIS, _T("Defaults")); return TRUE; }
            if (pDIS->CtlID == ID_BTN_ID_FONT){ DrawDarkButton(pDIS, _T("...")); return TRUE; }
            if (pDIS->CtlID == ID_BTN_TEXT_COLOR || pDIS->CtlID == ID_BTN_OUTLINE_COLOR) {
                HBRUSH hBrush = CreateSolidBrush(pDIS->CtlID == ID_BTN_TEXT_COLOR ? tempTextColor : tempOutlineColor);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);
                
                HBRUSH hBorder = CreateSolidBrush(RGB(100, 100, 100)); // Darker border
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

            switch(wmId) {
                case ID_BTN_ID_FONT: {
                    CHOOSEFONT cf = {0};
                    LOGFONT lf = {0};
                    cf.lStructSize = sizeof(cf);
                    cf.hwndOwner = hwnd;
                    cf.lpLogFont = &lf;
                    // CF_NOSIZESEL hides the size list, removing redundancy
                    cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT | CF_NOSIZESEL;
                    lstrcpyW(lf.lfFaceName, tempFontName);
                    
                    // We still init lfHeight for the sample text in the dialog
                    HDC hdc = GetDC(hwnd);
                    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
                    ReleaseDC(hwnd, hdc);
                    int currentSize = (int)Config::fontSize;
                    if (currentSize <= 0) currentSize = 14;
                    lf.lfHeight = -MulDiv(currentSize, dpi, 72);

                    if (ChooseFont(&cf)) {
                        lstrcpyW(tempFontName, lf.lfFaceName);
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
                    tempTextColor = RGB(255, 255, 255);
                    tempOutlineColor = RGB(0, 0, 0);
                    lstrcpyW(tempFontName, L"Segoe UI");
                    const float defFontSize = 14.0f;
                    const float defOutlineWidth = 2.0f;
                    const int defAnimDuration = 500;
                    const float defOffsetX = 0.0f;
                    const float defOffsetY = 0.0f;

                    TCHAR buf[32];
                    _stprintf(buf, _T("%.1f"), defFontSize);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_FONT_SIZE), buf);

                    _stprintf(buf, _T("%.1f"), defOutlineWidth);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OUTLINE_WIDTH), buf);

                    SendMessage(GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED), TBM_SETPOS, TRUE, defAnimDuration / 100);
                    _stprintf(buf, _T("%.1fs"), defAnimDuration / 1000.0);
                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_DURATION_VAL), buf);

                    _stprintf(buf, _T("%.1f"), defOffsetX);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OFFSET_X), buf);

                    _stprintf(buf, _T("%.1f"), defOffsetY);
                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OFFSET_Y), buf);

                    SetWindowText(GetDlgItem(hwnd, ID_EDIT_TIME_FORMAT), L"%H:%M");

                    SendMessage(GetDlgItem(hwnd, ID_TRACK_OPACITY), TBM_SETPOS, TRUE, 100);
                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_OPACITY_VAL), _T("100%"));

                    SetWindowText(GetDlgItem(hwnd, ID_STATIC_FONT_NAME), tempFontName);

                    InvalidateRect(hColorBtn1, NULL, TRUE);
                    InvalidateRect(hColorBtn2, NULL, TRUE);
                    ApplyPreview(hwnd);
                    break;
                }
            }
            break;
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
    
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, CLASS_NAME, _T("EdgeClock Settings"), 
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU, 
        (GetSystemMetrics(SM_CXSCREEN)/2)-150, (GetSystemMetrics(SM_CYSCREEN)/2)-220, 300, 440, parent, NULL, GetModuleHandle(NULL), NULL);
    
    // Enable Dark Mode for Title Bar
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    EnableWindow(parent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    g_settingsOpen = false;
}


// Begins a slide animation from the current position, capturing the start
// time/position for the time-based interpolation in the animation timer.
void StartSlide(HWND hwnd, AnimState newState) {
    currentState = newState;
    animStartTick = GetTickCount64();
    animStartY = currentYVal;
    SetTimer(hwnd, 3, Config::refreshRate, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Re-add tray icon when Explorer restarts (icon is lost otherwise)
    if (g_msgTaskbarCreated && uMsg == g_msgTaskbarCreated) {
        Shell_NotifyIcon(NIM_ADD, &nid);
        UpdateScreenMetrics();
        RecalculateAll(hwnd);
        return 0;
    }

    switch (uMsg) {
    case WM_CREATE:
        g_msgTaskbarCreated = RegisterWindowMessage(_T("TaskbarCreated"));
        UpdateScreenMetrics();
        RecalculateAll(hwnd);
        currentState = STATE_VISIBLE;
        currentYVal = (float)targetY; 
        SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - (int)Config::offsetX, (int)currentYVal, clockSize.cx, clockSize.cy, SWP_SHOWWINDOW);

        InitTrayIcon(hwnd);

        SetTimer(hwnd, 1, 300, NULL);
        SetTimer(hwnd, 2, 1000, NULL);
        
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

    case WM_TRAYICON:
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
            // Case ID_SIZE_CUSTOM removed
            case ID_MENU_SETTINGS:
                DoSettingsDialog(hwnd);
                break;
            case ID_MENU_STARTUP:
                SetStartup(!IsStartupEnabled());
                break;
            case ID_MENU_TOGGLE_HIDE:
                manualHidden = !manualHidden;
                // Force an immediate check or let timer handle it
                break;
        }
        return 0;
    }

    // --- Animation Logic ---
    // Handles the sliding up/down animation of the clock with floating point precision.
    case WM_TIMER: {
        if (wParam == 3) { // Animation Timer
            bool animating = false;

            // TIME-BASED ANIMATION with ease-out cubic.
            // Progress derives from wall-clock elapsed time, so the slide
            // always finishes in exactly animDuration ms even when timer
            // ticks are delayed (IDLE priority / EcoQoS).
            if (currentState == STATE_SLIDING_UP || currentState == STATE_SLIDING_DOWN) {
                float endY = (currentState == STATE_SLIDING_UP) ? (float)targetY : (float)screenH;
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

                if (progress >= 1.0f) {
                    currentYVal = endY;
                    currentState = (currentState == STATE_SLIDING_UP) ? STATE_VISIBLE : STATE_HIDDEN;
                    KillTimer(hwnd, 3);
                }
            } else if (currentState == STATE_VISIBLE) {
                 if ((int)currentYVal != targetY) {
                     currentYVal = (float)targetY;
                     animating = true;
                 }
                 KillTimer(hwnd, 3);
            } else {
                 KillTimer(hwnd, 3); // Hidden — nothing to animate
            }

            if (animating) {
                SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - (int)Config::offsetX, (int)currentYVal, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return 0;
        }

        if (wParam == 2) { // Clock Update
            if (currentState == STATE_VISIBLE || currentState == STATE_SLIDING_UP) {
               static TCHAR lastTime[64] = {0};
               TCHAR curTime[64];
               GetTime(curTime, 64);
               if (_tcscmp(lastTime, curTime) != 0) {
                   _tcscpy(lastTime, curTime);
                   UpdateLayeredWindowContent(hwnd);
                   // Trim RAM after drawing new minute
                   SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
               }
            }
            return 0;
        }

        if (wParam == 1) { // Logic Check
            bool shouldHide = manualHidden;
            
            // Mouse Hover Check (Check against the visible position to prevent flickering)
            POINT ptMouse;
            GetCursorPos(&ptMouse);
            int clkX = screenW - clockSize.cx - (int)Config::offsetX;
            int clkY = targetY; // Use targetY (visible position)
            if (ptMouse.x >= clkX && ptMouse.x <= clkX + clockSize.cx && 
                ptMouse.y >= clkY && ptMouse.y <= clkY + clockSize.cy) {
                shouldHide = true;
            }
            
            // Taskbar check — edge-aware. Only a bottom taskbar can cover the
            // clock; left/right/top placements never overlap the bottom edge.
            HWND hTray = FindWindow(_T("Shell_TrayWnd"), NULL);
            if (hTray) {
                APPBARDATA abd = { sizeof(abd) };
                UINT edge = ABE_BOTTOM; // Assume bottom if ABM query fails
                if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
                    edge = abd.uEdge;
                }
                if (edge == ABE_BOTTOM) {
                    RECT rcTray;
                    GetWindowRect(hTray, &rcTray);
                    if (rcTray.top < screenH - Config::taskbarThreshold) {
                        shouldHide = true;
                    }
                }
            }

            // Fullscreen / presentation detection via the shell (robust for
            // D3D games, F11 fullscreen, presentation mode)
            if (!shouldHide) {
                QUERY_USER_NOTIFICATION_STATE quns;
                if (SUCCEEDED(SHQueryUserNotificationState(&quns))) {
                    if (quns == QUNS_RUNNING_D3D_FULL_SCREEN ||
                        quns == QUNS_PRESENTATION_MODE ||
                        quns == QUNS_BUSY) {
                        shouldHide = true;
                    }
                }
            }

            // Fallback: foreground window exactly covering its own monitor
            if (!shouldHide) {
                HWND hFore = GetForegroundWindow();
                if (hFore) {
                    RECT rc;
                    GetWindowRect(hFore, &rc);
                    HMONITOR hMon = MonitorFromWindow(hFore, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi = { sizeof(mi) };
                    if (GetMonitorInfo(hMon, &mi) && EqualRect(&rc, &mi.rcMonitor)) {
                         TCHAR cName[256];
                         GetClassName(hFore, cName, 256);
                         if (_tcscmp(cName, _T("Progman")) != 0 && _tcscmp(cName, _T("WorkerW")) != 0 && _tcscmp(cName, _T("Shell_TrayWnd")) != 0) {
                             shouldHide = true;
                         }
                    }
                }
            }

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
            return 0;
        }
        return 0;
    }

    case WM_ENDSESSION:
        // Graceful cleanup when Windows shuts down or user logs off
        if (wParam) {
            SaveConfig();
            RemoveTrayIcon();
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        GdiplusShutdown(gdiplusToken);
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
        SetProcessInformationFunc pSetProcessInformation = (SetProcessInformationFunc)GetProcAddress(hKernel32, "SetProcessInformation");

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

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
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
