/**
 * @file window_core.c
 * @brief Core window creation and lifecycle management
 */

#include "window/window_core.h"
#include "window/window_visual_effects.h"
#include "window/window_desktop_integration.h"
#include "window_procedure/window_procedure.h"
#include "tray/tray.h"
#include "tray/tray_animation_core.h"
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define WINDOW_CLASS_NAME L"CatimeWindowClass"
#define WINDOW_TITLE L"Catime"
#define COLOR_KEY_BLACK RGB(0, 0, 0)
#define ALPHA_OPAQUE 255
#define DEFAULT_TRAY_ANIMATION_SPEED_MS 150  /* 150ms balances smoothness with CPU usage (6.7 FPS) */
#define SYSTEM_POSITION_GUARD_MS 3000
#define WINDOW_VISIBLE_MARGIN 20

/* ============================================================================
 * Global window state (definitions)
 * ============================================================================ */

/* Initial dimensions chosen to fit typical time formats (e.g., "12:34:56") at default font size */
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;
static DWORD g_systemPositionGuardUntil = 0;

BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};
BOOL CLOCK_WINDOW_TOPMOST = TRUE;
BOOL CLOCK_WINDOW_EFFECTIVE_TOPMOST = TRUE;
int CLOCK_WINDOW_OPACITY = 100;

RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

float CLOCK_FONT_SCALE_FACTOR = 1.0f;
float PLUGIN_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;
TextEffectType CLOCK_TEXT_EFFECT = TEXT_EFFECT_NONE;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * @brief Initialize system tray icon with animation
 * @param hwnd Window handle
 * @param hInstance Application instance
 */
static void InitializeTrayAndAnimation(HWND hwnd, HINSTANCE hInstance) {
    InitTrayIcon(hwnd, hInstance);
    StartTrayAnimation(hwnd, DEFAULT_TRAY_ANIMATION_SPEED_MS);
    LOG_INFO("Tray icon and animation initialized");
}

/**
 * @brief Configure window layering, visibility, and z-order
 * @param hwnd Window handle
 * @param nCmdShow Initial show command from WinMain
 */
static void ApplyInitialWindowState(HWND hwnd, int nCmdShow) {
    // Note: SetLayeredWindowAttributes is called by HandleWindowCreate via SetClickThrough
    SetBlurBehind(hwnd, FALSE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    RefreshWindowTopmostState(hwnd);
}

static BOOL IsSpecialWindowPositionX(int posX) {
    return posX == DEFAULT_WINDOW_POS_X || posX == -1 || posX == -2;
}

static void GetPrimaryMonitorInfo(MONITORINFO* mi) {
    if (!mi) return;

    POINT pt = {0, 0};
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    mi->cbSize = sizeof(*mi);
    if (!GetMonitorInfo(hMon, mi)) {
        mi->rcMonitor.left = 0;
        mi->rcMonitor.top = 0;
        mi->rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi->rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
        mi->rcWork = mi->rcMonitor;
    }
}

static void ClampWindowPositionToMonitor(int width, int height, int* x, int* y) {
    if (!x || !y || width <= 0 || height <= 0) return;

    RECT rc = {*x, *y, *x + width, *y + height};
    HMONITOR hMon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hMon, &mi)) {
        GetPrimaryMonitorInfo(&mi);
    }

    if (*x + WINDOW_VISIBLE_MARGIN > mi.rcMonitor.right) {
        *x = mi.rcMonitor.right - WINDOW_VISIBLE_MARGIN;
    }
    if (*x + width - WINDOW_VISIBLE_MARGIN < mi.rcMonitor.left) {
        *x = mi.rcMonitor.left - width + WINDOW_VISIBLE_MARGIN;
    }
    if (*y + WINDOW_VISIBLE_MARGIN > mi.rcMonitor.bottom) {
        *y = mi.rcMonitor.bottom - WINDOW_VISIBLE_MARGIN;
    }
    if (*y + height - WINDOW_VISIBLE_MARGIN < mi.rcMonitor.top) {
        *y = mi.rcMonitor.top - height + WINDOW_VISIBLE_MARGIN;
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    LOG_INFO("Creating main window");
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassW(&wc)) {
        LOG_WINDOWS_ERROR("Window class registration failed");
        return NULL;
    }
    
    /* WS_EX_TOOLWINDOW prevents taskbar button */
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (!CLOCK_WINDOW_TOPMOST) {
        exStyle |= WS_EX_NOACTIVATE;
    }
    
    int initialWidth = (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE);
    int initialHeight = (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE);

    ResolveConfiguredWindowPosition(initialWidth, initialHeight,
                                    &CLOCK_WINDOW_POS_X, &CLOCK_WINDOW_POS_Y);

    HWND hwnd = CreateWindowExW(
        exStyle,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_POPUP,
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        initialWidth, initialHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        LOG_WINDOWS_ERROR("Window creation failed");
        return NULL;
    }

    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);
    
    InitializeTrayAndAnimation(hwnd, hInstance);
    ApplyInitialWindowState(hwnd, nCmdShow);
    
    LOG_INFO("Main window created successfully (handle: 0x%p)", hwnd);
    return hwnd;
}

void SaveWindowSettings(HWND hwnd) {
    if (!hwnd) return;

    if (IsSystemPositionChangeGuardActive() && !CLOCK_EDIT_MODE) {
        LOG_INFO("Skipping window settings save during system position guard");
        return;
    }

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        LOG_WARNING("Failed to get window rect for saving");
        return;
    }
    
    CLOCK_WINDOW_POS_X = rect.left;
    CLOCK_WINDOW_POS_Y = rect.top;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char posXStr[16], posYStr[16], scaleStr[16], pluginScaleStr[16];
    snprintf(posXStr, sizeof(posXStr), "%d", CLOCK_WINDOW_POS_X);
    snprintf(posYStr, sizeof(posYStr), "%d", CLOCK_WINDOW_POS_Y);
    snprintf(scaleStr, sizeof(scaleStr), "%.2f", CLOCK_WINDOW_SCALE);
    snprintf(pluginScaleStr, sizeof(pluginScaleStr), "%.2f", PLUGIN_FONT_SCALE_FACTOR);
    
    IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", posXStr},
        {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", posYStr},
        {INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr},
        {INI_SECTION_DISPLAY, "PLUGIN_SCALE", pluginScaleStr}
    };
    
    if (WriteIniMultipleAtomic(config_path, updates, 4)) {
        LOG_INFO("Window settings saved (batch): pos(%d, %d), scale(%.2f), plugin_scale(%.2f)", 
                 CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, CLOCK_WINDOW_SCALE, PLUGIN_FONT_SCALE_FACTOR);
    } else {
        LOG_WARNING("Failed to save window settings");
    }
}

void ResolveConfiguredWindowPosition(int width, int height, int* outX, int* outY) {
    if (!outX || !outY) return;

    int posX = CLOCK_WINDOW_POS_X;
    int posY = CLOCK_WINDOW_POS_Y;
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);

    int configPosX = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X",
                                CLOCK_WINDOW_POS_X, configPath);
    int configPosY = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y",
                                CLOCK_WINDOW_POS_Y, configPath);

    MONITORINFO mi = {0};
    GetPrimaryMonitorInfo(&mi);
    int screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;

    if (IsSpecialWindowPositionX(configPosX)) {
        if (configPosX == -1) {
            posX = mi.rcMonitor.left + (screenWidth - width) / 2;
        } else {
            posX = mi.rcMonitor.left + (int)(screenWidth * 0.618f) - (width / 2);
            if (posX + width > mi.rcMonitor.right) {
                posX = mi.rcMonitor.right - width - WINDOW_VISIBLE_MARGIN;
            }
        }
    } else {
        posX = configPosX;
    }

    if (configPosY == DEFAULT_WINDOW_POS_Y) {
        posY = mi.rcMonitor.top;
    } else {
        posY = configPosY;
    }

    ClampWindowPositionToMonitor(width, height, &posX, &posY);

    *outX = posX;
    *outY = posY;
}

void BeginSystemPositionChangeGuard(HWND hwnd) {
    g_systemPositionGuardUntil = GetTickCount() + SYSTEM_POSITION_GUARD_MS;
    if (hwnd && IsWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_DISPLAY_RESTORE);
        SetTimer(hwnd, TIMER_ID_DISPLAY_RESTORE, 750, NULL);
    }
}

BOOL IsSystemPositionChangeGuardActive(void) {
    if (g_systemPositionGuardUntil == 0) return FALSE;
    if ((LONG)(GetTickCount() - g_systemPositionGuardUntil) < 0) return TRUE;
    g_systemPositionGuardUntil = 0;
    return FALSE;
}

void RestoreWindowPositionAfterSystemChange(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    if (CLOCK_EDIT_MODE) return;

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int posX = CLOCK_WINDOW_POS_X;
    int posY = CLOCK_WINDOW_POS_Y;

    ResolveConfiguredWindowPosition(width, height, &posX, &posY);

    if (posX != rect.left || posY != rect.top) {
        SetWindowPos(hwnd, NULL, posX, posY, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        LOG_INFO("Restored window position after system display change: (%d, %d)", posX, posY);
    }

    CLOCK_WINDOW_POS_X = posX;
    CLOCK_WINDOW_POS_Y = posY;
    InvalidateRect(hwnd, NULL, FALSE);
}

BOOL OpenFileDialog(HWND hwnd, wchar_t* filePath, DWORD maxPath) {
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = maxPath;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"";
    
    BOOL result = GetOpenFileNameW(&ofn);
    if (result) {
        LOG_INFO("File selected: %S", filePath);
    } else {
        LOG_INFO("File dialog cancelled");
    }
    return result;
}

