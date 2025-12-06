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
#define PROGMAN_CLASS L"Progman"
#define COLOR_KEY_BLACK RGB(0, 0, 0)
#define ALPHA_OPAQUE 255
#define DEFAULT_TRAY_ANIMATION_SPEED_MS 150  /* 150ms balances smoothness with CPU usage (6.7 FPS) */

/* ============================================================================
 * Global window state (definitions)
 * ============================================================================ */

/* Initial dimensions chosen to fit typical time formats (e.g., "12:34:56") at default font size */
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};
BOOL CLOCK_WINDOW_TOPMOST = TRUE;
int CLOCK_WINDOW_OPACITY = 100;

RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

float CLOCK_FONT_SCALE_FACTOR = 1.0f;
float PLUGIN_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;
BOOL CLOCK_GLOW_EFFECT = FALSE;
BOOL CLOCK_GLASS_EFFECT = FALSE;

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
 * @brief Make window always-on-top for overlay mode
 * @param hwnd Window handle
 */
static void ApplyTopmostMode(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    LOG_INFO("Window set to topmost mode");
}

/**
 * @brief Make window normal z-order, parented to Progman
 * @param hwnd Window handle
 */
static void ApplyNormalMode(HWND hwnd) {
    HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
    if (hProgman) {
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
        LOG_INFO("Window parented to Progman for Win+D protection");
    }
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    LOG_INFO("Window set to normal mode");
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

    if (CLOCK_WINDOW_TOPMOST) {
        ApplyTopmostMode(hwnd);
    } else {
        ApplyNormalMode(hwnd);
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

