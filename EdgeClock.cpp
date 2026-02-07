#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <tchar.h>
#include <ctime>
#include <gdiplus.h>
#include <shellapi.h>
#include <commctrl.h> 

using namespace Gdiplus;

// =============================================================
//   CONFIGURATION
// =============================================================
namespace Config {
    // --- Appearance ---
    int fontSize = 14;                  // Current font size (modifiable via tray)
    const WCHAR fontName[] = L"Tektur"; // Font Family
    const int outlineWidth = 2;         // Border thickness
    
    // --- Animation & Performance ---
    const int animationSpeed = 8;       // Pixels per frame (Lower = Smoother/Slower)
    const int refreshRate = 10;         // Animation Timer Interval (ms)
    
    // --- Tray Icon ---
    const TCHAR iconFileName[] = _T("clock_23989.ico");
    const TCHAR trayTooltip[] = _T("Edge Clock");
    
    // --- Menu Presets ---
    const int sizeSmall = 8;
    const int sizeMedium = 14;
    const int sizeLarge = 20;

    // --- Auto-Hide Logic ---
    const int taskbarThreshold = 4;     // Sensitivity for detecting Taskbar (px)
    const int offsetX = 3;              // Extra spacing from right of screen
    const int offsetY = -2;              // Extra spacing from bottom of screen
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

#define ID_EDIT_CUSTOM 3001
#define ID_BTN_OK 3002
#define ID_BTN_CANCEL 3003

enum AnimState {
    STATE_VISIBLE,
    STATE_HIDDEN,
    STATE_SLIDING_UP,
    STATE_SLIDING_DOWN
};

AnimState currentState = STATE_SLIDING_UP;
int currentY = 0;
int targetY = 0;
int screenH = 0;
int screenW = 0;
SIZE clockSize = {0, 0};
bool manualHidden = false;

ULONG_PTR gdiplusToken;
NOTIFYICONDATA nid = { 0 };

void GetTime(TCHAR* buffer, int size) {
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    _tcsftime(buffer, size, _T("%H:%M"), timeinfo);
}

void UpdateLayeredWindowContent(HWND hwnd) {
    int width = clockSize.cx;
    int height = clockSize.cy;

    if (width <= 0 || height <= 0) return;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    Graphics graphics(hdcMem);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    graphics.Clear(Color(0, 0, 0, 0));

    FontFamily fontFamily(Config::fontName);
    if (fontFamily.GetLastStatus() != Ok) {
        FontFamily ffm(L"Montserrat");
        if (ffm.GetLastStatus() == Ok) {
        }
    }

    Font font(&fontFamily, (REAL)Config::fontSize, FontStyleBold, UnitPoint);
    StringFormat format;
    format.SetAlignment(StringAlignmentFar);
    format.SetLineAlignment(StringAlignmentFar);

    TCHAR timeBuf[10];
    GetTime(timeBuf, 10);

    GraphicsPath path;
    RectF rect(0, 0, (REAL)width - 4, (REAL)height - 4); 
    
    path.AddString(timeBuf, -1, &fontFamily, FontStyleBold, (REAL)Config::fontSize, rect, &format);

    Pen pen(Color(255, 0, 0, 0), (REAL)Config::outlineWidth);
    pen.SetLineJoin(LineJoinRound);
    graphics.DrawPath(&pen, &path);

    SolidBrush brush(Color(255, 255, 255, 255));
    graphics.FillPath(&brush, &path);

    POINT ptSrc = { 0, 0 };
    SIZE size = { width, height };
    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, hdcScreen, NULL, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

SIZE CalculateTextSize() {
    HDC hdc = GetDC(NULL);
    Graphics graphics(hdc);
    
    FontFamily fontFamily(Config::fontName);
    Font font(&fontFamily, (REAL)Config::fontSize, FontStyleBold, UnitPoint);
    
    TCHAR timeBuf[10];
    GetTime(timeBuf, 10);

    RectF boundingBox;
    PointF origin(0, 0);

    graphics.MeasureString(timeBuf, -1, &font, origin, &boundingBox);
    
    ReleaseDC(NULL, hdc);

    SIZE sz;
    sz.cx = (int)boundingBox.Width + Config::outlineWidth * 2 + 15;
    sz.cy = (int)boundingBox.Height + Config::outlineWidth * 2 + 10;
    return sz;
}

void RecalculateAll(HWND hwnd) {
    clockSize = CalculateTextSize();
    targetY = screenH - clockSize.cy - Config::offsetY;
    
    if (currentState == STATE_VISIBLE) {
        currentY = targetY;
        SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - Config::offsetX, currentY, clockSize.cx, clockSize.cy, SWP_NOACTIVATE);
    } else {
        SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - Config::offsetX, currentY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    
    UpdateLayeredWindowContent(hwnd);
}

void InitTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    
    // Load .ico file from Config
    nid.hIcon = (HICON)LoadImage(NULL, Config::iconFileName, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
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
        TCHAR path[MAX_PATH];
        DWORD size = sizeof(path);
        if (RegQueryValueEx(hKey, _T("EdgeClock"), NULL, NULL, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
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
            RegSetValueEx(hKey, _T("EdgeClock"), 0, REG_SZ, (LPBYTE)path, (lstrlen(path) + 1) * sizeof(TCHAR));
        } else {
            RegDeleteValue(hKey, _T("EdgeClock"));
        }
        RegCloseKey(hKey);
    }
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    
    // Use Config values for menu display logic
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeSmall ? MF_CHECKED : 0), ID_SIZE_SMALL, _T("Size: Small"));
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeMedium ? MF_CHECKED : 0), ID_SIZE_MEDIUM, _T("Size: Normal"));
    AppendMenu(hMenu, MF_STRING | (Config::fontSize == Config::sizeLarge ? MF_CHECKED : 0), ID_SIZE_LARGE, _T("Size: Large"));
    AppendMenu(hMenu, MF_STRING, ID_SIZE_CUSTOM, _T("Size: Custom..."));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), ID_MENU_STARTUP, _T("Run on Startup"));
    AppendMenu(hMenu, MF_STRING, ID_MENU_TOGGLE_HIDE, manualHidden ? _T("Show Clock") : _T("Hide Clock"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, _T("Exit"));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// ... Custom Input Window Logic ...
int resultSize = 0;
LRESULT CALLBACK InputWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CREATE) {
        CreateWindow(_T("STATIC"), _T("Size (pt):"), WS_VISIBLE | WS_CHILD, 10, 15, 60, 20, hwnd, NULL, NULL, NULL);
        HWND hEdit = CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 70, 12, 50, 20, hwnd, (HMENU)100, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("OK"), WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 15, 45, 50, 25, hwnd, (HMENU)IDOK, NULL, NULL);
        CreateWindow(_T("BUTTON"), _T("Cancel"), WS_VISIBLE | WS_CHILD, 75, 45, 60, 25, hwnd, (HMENU)IDCANCEL, NULL, NULL);
        SetWindowText(hEdit, _T("")); 
        SetFocus(hEdit);
        return 0;
    }
    if (uMsg == WM_COMMAND) {
        if (LOWORD(wParam) == IDOK) {
            TCHAR buf[32];
            GetWindowText(GetDlgItem(hwnd, 100), buf, 32);
            resultSize = _ttoi(buf);
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == IDCANCEL) {
            resultSize = 0;
            DestroyWindow(hwnd);
        }
    }
    if (uMsg == WM_CLOSE) DestroyWindow(hwnd);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DoCustomSizeInput(HWND parent) {
    const TCHAR CLASS_NAME[] = _T("InputWndParams");
    WNDCLASS wc = { };
    if (!GetClassInfo(GetModuleHandle(NULL), CLASS_NAME, &wc)) {
        wc.lpfnWndProc = InputWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClass(&wc);
    }
    
    resultSize = 0;
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, CLASS_NAME, _T("Font Size"), WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU, 
        (GetSystemMetrics(SM_CXSCREEN)/2)-75, (GetSystemMetrics(SM_CYSCREEN)/2)-50, 150, 120, parent, NULL, GetModuleHandle(NULL), NULL);
    
    EnableWindow(parent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    
    if (resultSize > 0) {
        Config::fontSize = resultSize;
        RecalculateAll(parent);
    }
}
// .................................

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
        RecalculateAll(hwnd);
        currentY = screenH; 
        SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - Config::offsetX, currentY, clockSize.cx, clockSize.cy, SWP_SHOWWINDOW);

        InitTrayIcon(hwnd);

        SetTimer(hwnd, 1, 300, NULL);
        SetTimer(hwnd, 2, 1000, NULL);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
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
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_MEDIUM:
                Config::fontSize = Config::sizeMedium;
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_LARGE:
                Config::fontSize = Config::sizeLarge;
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_CUSTOM:
                DoCustomSizeInput(hwnd);
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

    case WM_TIMER: {
        if (wParam == 3) { // Animation
            bool animating = false;
            // Use Config::animationSpeed
            if (currentState == STATE_SLIDING_UP) {
                currentY -= Config::animationSpeed;
                if (currentY <= targetY) {
                    currentY = targetY;
                    currentState = STATE_VISIBLE;
                    KillTimer(hwnd, 3);
                }
                animating = true;
            } else if (currentState == STATE_SLIDING_DOWN) {
                currentY += Config::animationSpeed;
                if (currentY >= screenH) {
                    currentY = screenH;
                    currentState = STATE_HIDDEN;
                    KillTimer(hwnd, 3);
                }
                animating = true;
            } else if (currentState == STATE_VISIBLE) {
                 if (currentY != targetY) {
                     currentY = targetY;
                     animating = true;
                 }
            }

            if (animating) {
                SetWindowPos(hwnd, HWND_TOPMOST, screenW - clockSize.cx - Config::offsetX, currentY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return 0;
        }

        if (wParam == 2) { // Clock Update
            if (currentState == STATE_VISIBLE || currentState == STATE_SLIDING_UP) {
               static TCHAR lastTime[10] = {0};
               TCHAR curTime[10];
               GetTime(curTime, 10);
               if (_tcscmp(lastTime, curTime) != 0) {
                   _tcscpy(lastTime, curTime);
                   UpdateLayeredWindowContent(hwnd);
               }
            }
            return 0;
        }

        if (wParam == 1) { // Logic Check
            bool shouldHide = manualHidden;
            
            // Mouse Hover Check (Check against the visible position to prevent flickering)
            POINT ptMouse;
            GetCursorPos(&ptMouse);
            int clkX = screenW - clockSize.cx - Config::offsetX;
            int clkY = targetY; // Use targetY (visible position)
            if (ptMouse.x >= clkX && ptMouse.x <= clkX + clockSize.cx && 
                ptMouse.y >= clkY && ptMouse.y <= clkY + clockSize.cy) {
                shouldHide = true;
            }
            
            HWND hTray = FindWindow(_T("Shell_TrayWnd"), NULL);
            if (hTray) {
                RECT rcTray;
                GetWindowRect(hTray, &rcTray);
                // Use Config::taskbarThreshold
                if (rcTray.top < screenH - Config::taskbarThreshold) { 
                    shouldHide = true;
                }
            }

            if (!shouldHide) {
                HWND hFore = GetForegroundWindow();
                if (hFore) {
                    RECT rc; 
                    GetWindowRect(hFore, &rc);
                    if ((rc.right - rc.left) == screenW && (rc.bottom - rc.top) == screenH) {
                         TCHAR cName[256];
                         GetClassName(hFore, cName, 256);
                         if (_tcscmp(cName, _T("Progman")) != 0 && _tcscmp(cName, _T("WorkerW")) != 0 && _tcscmp(cName, _T("Shell_TrayWnd")) != 0) {
                             shouldHide = true;
                         }
                    }
                }
            }

            if (shouldHide) {
                if (currentState == STATE_VISIBLE || currentState == STATE_SLIDING_UP) {
                    currentState = STATE_SLIDING_DOWN;
                    SetTimer(hwnd, 3, Config::refreshRate, NULL);
                }
            } else {
                if (currentState == STATE_HIDDEN || currentState == STATE_SLIDING_DOWN) {
                    currentState = STATE_SLIDING_UP;
                    SetTimer(hwnd, 3, Config::refreshRate, NULL);
                }
            }
            return 0;
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveTrayIcon();
        GdiplusShutdown(gdiplusToken);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ... (previous code)

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
    // Enable Efficiency Mode / EcoQoS immediately
    EnableEfficiencyMode();

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    const TCHAR CLASS_NAME[] = _T("EdgeClockTray");
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        CLASS_NAME, _T("Edge Clock"),
        WS_POPUP,
        0, 0, 100, 50,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
