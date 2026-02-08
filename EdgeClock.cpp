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
#include <commdlg.h> 
#include <dwmapi.h> 

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif


using namespace Gdiplus;

// =============================================================
//   CONFIGURATION
// =============================================================
namespace Config {
    // --- Appearance Settings ---
    // These control the visual look of the clock.
    int fontSize = 14;                  
    WCHAR fontName[32] = L"Tektur"; // Default font
    int outlineWidth = 2;         
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
    int offsetX = 0;              
    int offsetY = 0;

    // --- Menu Presets ---
    // Pre-defined sizes for the context menu.
    const int sizeSmall = 8;
    const int sizeMedium = 14;
    const int sizeLarge = 20;              
}



void SaveConfig() {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\EdgeClock"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, _T("FontSize"), 0, REG_DWORD, (const BYTE*)&Config::fontSize, sizeof(int));
        RegSetValueEx(hKey, _T("OutlineWidth"), 0, REG_DWORD, (const BYTE*)&Config::outlineWidth, sizeof(int));
        RegSetValueEx(hKey, _T("AnimDuration"), 0, REG_DWORD, (const BYTE*)&Config::animDuration, sizeof(int));
        RegSetValueEx(hKey, _T("TaskbarThreshold"), 0, REG_DWORD, (const BYTE*)&Config::taskbarThreshold, sizeof(int));
        RegSetValueEx(hKey, _T("OffsetX"), 0, REG_DWORD, (const BYTE*)&Config::offsetX, sizeof(int));
        RegSetValueEx(hKey, _T("OffsetY"), 0, REG_DWORD, (const BYTE*)&Config::offsetY, sizeof(int));
        RegSetValueEx(hKey, _T("TextColor"), 0, REG_DWORD, (const BYTE*)&Config::textColor, sizeof(COLORREF));
        RegSetValueEx(hKey, _T("OutlineColor"), 0, REG_DWORD, (const BYTE*)&Config::outlineColor, sizeof(COLORREF));
        RegSetValueEx(hKey, _T("FontName"), 0, REG_SZ, (const BYTE*)Config::fontName, (lstrlenW(Config::fontName) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
    }
}

void LoadConfig() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\EdgeClock"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(int);
        RegQueryValueEx(hKey, _T("FontSize"), NULL, NULL, (LPBYTE)&Config::fontSize, &size);
        RegQueryValueEx(hKey, _T("OutlineWidth"), NULL, NULL, (LPBYTE)&Config::outlineWidth, &size);
        RegQueryValueEx(hKey, _T("AnimDuration"), NULL, NULL, (LPBYTE)&Config::animDuration, &size);
        RegQueryValueEx(hKey, _T("TaskbarThreshold"), NULL, NULL, (LPBYTE)&Config::taskbarThreshold, &size);
        RegQueryValueEx(hKey, _T("OffsetX"), NULL, NULL, (LPBYTE)&Config::offsetX, &size);
        RegQueryValueEx(hKey, _T("OffsetY"), NULL, NULL, (LPBYTE)&Config::offsetY, &size);
        RegQueryValueEx(hKey, _T("TextColor"), NULL, NULL, (LPBYTE)&Config::textColor, &size);
        RegQueryValueEx(hKey, _T("OutlineColor"), NULL, NULL, (LPBYTE)&Config::outlineColor, &size);
        
        DWORD currentSize = sizeof(Config::fontName);
        RegQueryValueEx(hKey, _T("FontName"), NULL, NULL, (LPBYTE)Config::fontName, &currentSize);
        
        RegCloseKey(hKey);
    }
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

    // Create GDI+ Graphics object for high-quality rendering
    Graphics graphics(hdcMem);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    // Clear background with fully transparent color
    graphics.Clear(Color(0, 0, 0, 0));

    FontFamily fontFamily(Config::fontName);
    if (fontFamily.GetLastStatus() != Ok) {
        FontFamily ffm(L"Montserrat");
        if (ffm.GetLastStatus() == Ok) {
        }
    }

    Font font(&fontFamily, (REAL)Config::fontSize, FontStyleBold, UnitPoint);
    // Use GenericTypographic to eliminate inner padding for strict alignment
    StringFormat format(StringFormat::GenericTypographic());
    format.SetAlignment(StringAlignmentFar);
    format.SetLineAlignment(StringAlignmentFar);

    TCHAR timeBuf[10];
    GetTime(timeBuf, 10);

    GraphicsPath path;
    RectF rect(0, 0, (REAL)width, (REAL)height); 
    
    path.AddString(timeBuf, -1, &fontFamily, FontStyleBold, (REAL)Config::fontSize, rect, &format);

    Pen pen(Color(255, GetRValue(Config::outlineColor), GetGValue(Config::outlineColor), GetBValue(Config::outlineColor)), (REAL)Config::outlineWidth);
    pen.SetLineJoin(LineJoinRound);
    graphics.DrawPath(&pen, &path);

    SolidBrush brush(Color(255, GetRValue(Config::textColor), GetGValue(Config::textColor), GetBValue(Config::textColor)));
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

// Calculates the exact size needed for the clock text
// This ensures the window is exactly the size of the text + outline,
// preventing any clipping or unnecessary empty space.
SIZE CalculateTextSize() {
    HDC hdc = GetDC(NULL);
    Graphics graphics(hdc);
    
    FontFamily fontFamily(Config::fontName);
    Font font(&fontFamily, (REAL)Config::fontSize, FontStyleBold, UnitPoint);
    
    TCHAR timeBuf[10];
    GetTime(timeBuf, 10);

    RectF boundingBox;
    PointF origin(0, 0);
    // Use GenericTypographic for measurement too
    graphics.MeasureString(timeBuf, -1, &font, origin, StringFormat::GenericTypographic(), &boundingBox);
    
    ReleaseDC(NULL, hdc);

    // Calcluate bounding box strictly based on content + outline
    SIZE sz;
    sz.cx = (int)boundingBox.Width + Config::outlineWidth * 2; 
    sz.cy = (int)boundingBox.Height + Config::outlineWidth * 2;
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

// ... Settings Window Logic ...
COLORREF tempTextColor;
COLORREF tempOutlineColor;
WCHAR tempFontName[32];
HWND hColorBtn1, hColorBtn2;

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

// Main procedure for the Settings Dialog
// Handles all user interactions in the settings window.
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_CREATE: {
            // "Minimal Modern" Design
            // We use a clean white background and standard Windows controls
            // enhanced with Visual Styles (via manifest).
            hBrushDark = CreateSolidBrush(RGB(255, 255, 255)); // White Background
            hBrushEdit = CreateSolidBrush(RGB(255, 255, 255)); // White Edit Fields

            // --- Font Selection ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Font"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, 20, 40, 25, hwnd, NULL, NULL, NULL));
            // Show Font Name in a Read-Only Edit for better visual consistency or just cleaner static
            StyleControl(CreateWindow(_T("STATIC"), Config::fontName, WS_VISIBLE | WS_CHILD | SS_ENDELLIPSIS | SS_CENTERIMAGE, 70, 20, 110, 25, hwnd, (HMENU)ID_STATIC_FONT_NAME, NULL, NULL));
            StyleControl(CreateWindow(_T("BUTTON"), _T("Change"), WS_VISIBLE | WS_CHILD, 190, 20, 60, 25, hwnd, (HMENU)ID_BTN_ID_FONT, NULL, NULL));

            // --- Size & Outline ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Size"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, 60, 40, 25, hwnd, NULL, NULL, NULL));
            HWND hSize = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, 70, 60, 40, 25, hwnd, (HMENU)ID_EDIT_FONT_SIZE, NULL, NULL));
            SetDlgItemInt(hwnd, ID_EDIT_FONT_SIZE, Config::fontSize, FALSE);

            StyleControl(CreateWindow(_T("STATIC"), _T("Outline"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 140, 60, 50, 25, hwnd, NULL, NULL, NULL));
            HWND hOutline = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, 200, 60, 40, 25, hwnd, (HMENU)ID_EDIT_OUTLINE_WIDTH, NULL, NULL));
            SetDlgItemInt(hwnd, ID_EDIT_OUTLINE_WIDTH, Config::outlineWidth, FALSE);

            // --- Colors ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Color"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, 100, 40, 25, hwnd, NULL, NULL, NULL));
            hColorBtn1 = CreateWindow(_T("BUTTON"), _T(""), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 70, 100, 40, 25, hwnd, (HMENU)ID_BTN_TEXT_COLOR, NULL, NULL);

            StyleControl(CreateWindow(_T("STATIC"), _T("Border"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 140, 100, 50, 25, hwnd, NULL, NULL, NULL));
            hColorBtn2 = CreateWindow(_T("BUTTON"), _T(""), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 200, 100, 40, 25, hwnd, (HMENU)ID_BTN_OUTLINE_COLOR, NULL, NULL);

            // --- Animation ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Time:"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, 140, 40, 25, hwnd, NULL, NULL, NULL));
            // Slider 0-20 (0.0s - 2.0s)
            HWND hTrack = CreateWindow(TRACKBAR_CLASS, _T(""), WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS | TBS_ENABLESELRANGE, 60, 140, 120, 30, hwnd, (HMENU)ID_TRACK_ANIM_SPEED, NULL, NULL);
            SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELPARAM(0, 20)); 
            SendMessage(hTrack, TBM_SETPOS, TRUE, Config::animDuration / 100);

            // Value Label
            TCHAR durBuf[16];
            _stprintf(durBuf, _T("%.1fs"), Config::animDuration / 1000.0);
            StyleControl(CreateWindow(_T("STATIC"), durBuf, WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 190, 140, 40, 25, hwnd, (HMENU)ID_STATIC_DURATION_VAL, NULL, NULL));

            // --- Offsets ---
            StyleControl(CreateWindow(_T("STATIC"), _T("Off X"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, 180, 40, 25, hwnd, NULL, NULL, NULL));
            HWND hOffX = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_LEFT | ES_CENTER, 70, 180, 40, 25, hwnd, (HMENU)ID_EDIT_OFFSET_X, NULL, NULL));
            SetDlgItemInt(hwnd, ID_EDIT_OFFSET_X, Config::offsetX, TRUE);

            StyleControl(CreateWindow(_T("STATIC"), _T("Y"), WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE | SS_CENTER, 160, 180, 30, 25, hwnd, NULL, NULL, NULL));
            HWND hOffY = StyleControl(CreateWindow(_T("EDIT"), _T(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_LEFT | ES_CENTER, 200, 180, 40, 25, hwnd, (HMENU)ID_EDIT_OFFSET_Y, NULL, NULL));
            SetDlgItemInt(hwnd, ID_EDIT_OFFSET_Y, Config::offsetY, TRUE);

            // --- Save / Cancel ---
            // Save uses BS_OWNERDRAW for custom color
            StyleControl(CreateWindow(_T("BUTTON"), _T("Save"), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 30, 230, 90, 30, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL));
            // Cancel also uses BS_OWNERDRAW for modern styling
            StyleControl(CreateWindow(_T("BUTTON"), _T("Cancel"), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 140, 230, 90, 30, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL));

            tempTextColor = Config::textColor;
            tempOutlineColor = Config::outlineColor;
            lstrcpyW(tempFontName, Config::fontName);
            break;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            SetTextColor((HDC)wParam, RGB(0, 0, 0)); // Black Text
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (INT_PTR)hBrushDark; // White Brush

        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)wParam, RGB(0, 0, 0));
            SetBkColor((HDC)wParam, RGB(255, 255, 255)); 
            return (INT_PTR)hBrushEdit;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID == ID_BTN_SAVE) {
                // Modern Blue Button
                COLORREF bg = (pDIS->itemState & ODS_SELECTED) ? RGB(0, 100, 180) : RGB(0, 120, 215);
                HBRUSH hBrush = CreateSolidBrush(bg);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);

                // Text
                SetBkMode(pDIS->hDC, TRANSPARENT);
                SetTextColor(pDIS->hDC, RGB(255, 255, 255));
                
                // Use the modern font
                HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFontSegoe);
                DrawText(pDIS->hDC, _T("Save"), -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(pDIS->hDC, hOldFont);
                
                return TRUE;
            }
            if (pDIS->CtlID == ID_BTN_CANCEL) {
                // Modern Gray Button
                COLORREF bg = (pDIS->itemState & ODS_SELECTED) ? RGB(200, 200, 200) : RGB(240, 240, 240);
                HBRUSH hBrush = CreateSolidBrush(bg);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);

                // Border
                HBRUSH hBorder = CreateSolidBrush(RGB(180, 180, 180));
                FrameRect(pDIS->hDC, &pDIS->rcItem, hBorder);
                DeleteObject(hBorder);

                // Text
                SetBkMode(pDIS->hDC, TRANSPARENT);
                SetTextColor(pDIS->hDC, RGB(0, 0, 0));
                
                HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFontSegoe);
                DrawText(pDIS->hDC, _T("Cancel"), -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(pDIS->hDC, hOldFont);
                
                return TRUE;
            }
            if (pDIS->CtlID == ID_BTN_TEXT_COLOR || pDIS->CtlID == ID_BTN_OUTLINE_COLOR) {
                // Draw rounded color swatch
                HBRUSH hBrush = CreateSolidBrush(pDIS->CtlID == ID_BTN_TEXT_COLOR ? tempTextColor : tempOutlineColor);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);
                
                // Draw Border
                HBRUSH hBorder = CreateSolidBrush(RGB(180, 180, 180));
                FrameRect(pDIS->hDC, &pDIS->rcItem, hBorder);
                DeleteObject(hBorder);
                return TRUE;
            }
            break;
        }

        case WM_HSCROLL: {
             if ((HWND)lParam == GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED)) {
                 int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                 TCHAR valBuf[16];
                 _stprintf(valBuf, _T("%.1fs"), pos / 10.0);
                 SetWindowText(GetDlgItem(hwnd, ID_STATIC_DURATION_VAL), valBuf);
             }
             break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch(wmId) {
                case ID_BTN_ID_FONT: {
                    CHOOSEFONT cf = {0};
                    LOGFONT lf = {0};
                    cf.lStructSize = sizeof(cf);
                    cf.hwndOwner = hwnd;
                    cf.lpLogFont = &lf;
                    cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;
                    lstrcpyW(lf.lfFaceName, tempFontName);
                    
                    if (ChooseFont(&cf)) {
                        lstrcpyW(tempFontName, lf.lfFaceName);
                        SetWindowText(GetDlgItem(hwnd, ID_STATIC_FONT_NAME), tempFontName);
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
                    }
                    break;
                }
                case ID_BTN_SAVE: {
                    Config::fontSize = GetDlgItemInt(hwnd, ID_EDIT_FONT_SIZE, NULL, FALSE);
                    Config::outlineWidth = GetDlgItemInt(hwnd, ID_EDIT_OUTLINE_WIDTH, NULL, FALSE);
                    // Read Slider Position -> Duration ms
                    int pos = SendMessage(GetDlgItem(hwnd, ID_TRACK_ANIM_SPEED), TBM_GETPOS, 0, 0);
                    Config::animDuration = pos * 100;
                    Config::offsetX = GetDlgItemInt(hwnd, ID_EDIT_OFFSET_X, NULL, TRUE);
                    Config::offsetY = GetDlgItemInt(hwnd, ID_EDIT_OFFSET_Y, NULL, TRUE);
                    Config::textColor = tempTextColor;
                    Config::outlineColor = tempOutlineColor;
                    lstrcpyW(Config::fontName, tempFontName);
                    
                    SaveConfig();
                    
                    HWND hParent = GetParent(hwnd);
                    if (hParent) RecalculateAll(hParent);
                    
                    DestroyWindow(hwnd);
                    break;
                }
                case ID_BTN_CANCEL:
                    DestroyWindow(hwnd);
                    break;
            }
            break;
        }
        case WM_CLOSE:
            if (hBrushDark) DeleteObject(hBrushDark);
            if (hBrushEdit) DeleteObject(hBrushEdit);
            if (hFontSegoe) DeleteObject(hFontSegoe);
            DestroyWindow(hwnd);
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DoSettingsDialog(HWND parent) {
    const TCHAR CLASS_NAME[] = _T("EdgeClockSettings");
    WNDCLASS wc = { };
    if (!GetClassInfo(GetModuleHandle(NULL), CLASS_NAME, &wc)) {
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClass(&wc);
    }
    
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, CLASS_NAME, _T("EdgeClock Settings"), 
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU, 
        (GetSystemMetrics(SM_CXSCREEN)/2)-140, (GetSystemMetrics(SM_CYSCREEN)/2)-160, 280, 320, parent, NULL, GetModuleHandle(NULL), NULL);
    
    EnableWindow(parent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
        RecalculateAll(hwnd);
        currentState = STATE_VISIBLE;
        currentY = targetY; 
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
                Config::fontSize = 8;
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_MEDIUM:
                Config::fontSize = 14;
                RecalculateAll(hwnd);
                break;
            case ID_SIZE_LARGE:
                Config::fontSize = 20;
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
    // Handles the sliding up/down animation of the clock.
    case WM_TIMER: {
        if (wParam == 3) { // Animation Timer
            bool animating = false;
            // Calculate dynamic step based on duration
            // Total Dist = distance to target. We need constant speed or ease. 
            // Simple Linear Speed = Total Distance / (Duration / RefreshRate)
            // But Total Distance is variable? Let's assume standard slide = ClockHeight.
            // Actually, let's just use proportional speed based on current distance? No, that's easing.
            // Let's us simple constant speed approximation assuming full slide.
            int height = clockSize.cy;
            if (height < 10) height = 50; 
            int ticks = Config::animDuration / Config::refreshRate;
            if (ticks < 1) ticks = 1;
            int step = height / ticks;
            if (step < 1) step = 1;

            if (currentState == STATE_SLIDING_UP) {
                currentY -= step;
                if (currentY <= targetY) {
                    currentY = targetY;
                    currentState = STATE_VISIBLE;
                    KillTimer(hwnd, 3);
                }
                animating = true;
            } else if (currentState == STATE_SLIDING_DOWN) {
                currentY += step;
                if (currentY >= screenH) {
                    currentY = screenH;
                    currentState = STATE_HIDDEN;
                    KillTimer(hwnd, 3);
                }
                animating = true;
            } else if (currentState == STATE_VISIBLE) {
                 // Adjust position if size changed
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

            // Perform logic check (should we hide?)
            if (shouldHide) {
                if (currentState == STATE_VISIBLE || currentState == STATE_SLIDING_UP) {
                    currentState = STATE_SLIDING_DOWN;
                    SetTimer(hwnd, 3, Config::refreshRate, NULL); // Start animation
                }
            } else {
                if (currentState == STATE_HIDDEN || currentState == STATE_SLIDING_DOWN) {
                    currentState = STATE_SLIDING_UP;
                    SetTimer(hwnd, 3, Config::refreshRate, NULL); // Start animation
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
    // Enable Efficiency Mode / EcoQoS immediately
    EnableEfficiencyMode();

    // Init Common Controls for "Modern" Visual Styles (requires manifest)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES; // Add BAR_CLASSES for Trackbar
    InitCommonControlsEx(&icex);

    LoadConfig();

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
