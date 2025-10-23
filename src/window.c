/**
 * @file window.c
 * @brief Main window management with DPI awareness and visual effects
 * @version 2.0 - Refactored for modularity, eliminated code duplication, added comprehensive logging
 * 
 * Provides centralized window lifecycle management including:
 * - DPI-aware window creation and initialization
 * - Multi-monitor support with active display detection
 * - Transparency and blur effects (Windows 10+)
 * - Click-through and always-on-top behaviors
 * - Desktop wallpaper-level attachment
 */
#include "../include/window.h"
#include "../include/window_procedure.h"
#include "../include/timer.h"
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/log.h"
#include "../include/tray_animation.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Window class and identification */
#define WINDOW_CLASS_NAME L"CatimeWindow"
#define WINDOW_TITLE L"Catime"
#define PROGMAN_CLASS L"Progman"
#define WORKERW_CLASS L"WorkerW"
#define SHELLDLL_CLASS L"SHELLDLL_DefView"

/** Visual effect parameters */
#define BLUR_ALPHA_VALUE 180
#define BLUR_GRADIENT_COLOR 0x00202020
#define COLOR_KEY_BLACK RGB(0, 0, 0)
#define ALPHA_OPAQUE 255

/** System configuration */
#define CONSOLE_CODEPAGE_GBK 936
#define DEFAULT_TRAY_ANIMATION_SPEED_MS 150

/** DWM library */
#define DWMAPI_DLL L"dwmapi.dll"
#define SHCORE_DLL L"shcore.dll"
#define USER32_DLL L"user32.dll"

/* ============================================================================
 * Global window state
 * ============================================================================ */

/** Window geometry */
int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;

/** Window interaction state */
BOOL CLOCK_EDIT_MODE = FALSE;
BOOL CLOCK_IS_DRAGGING = FALSE;
POINT CLOCK_LAST_MOUSE_POS = {0, 0};
BOOL CLOCK_WINDOW_TOPMOST = TRUE;

/** Text rendering optimization */
RECT CLOCK_TEXT_RECT = {0, 0, 0, 0};
BOOL CLOCK_TEXT_RECT_VALID = FALSE;

/** Font configuration */
float CLOCK_FONT_SCALE_FACTOR = 1.0f;
int CLOCK_BASE_FONT_SIZE = 24;

/* ============================================================================
 * DWM function pointers
 * ============================================================================ */

typedef HRESULT (WINAPI *pfnDwmEnableBlurBehindWindow)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
static pfnDwmEnableBlurBehindWindow _DwmEnableBlurBehindWindow = NULL;

/* ============================================================================
 * Windows composition structures (for blur effects)
 * ============================================================================ */

typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0,
    WCA_ACCENT_POLICY = 19,
    WCA_LAST = 27
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

WINUSERAPI BOOL WINAPI SetWindowCompositionAttribute(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* pData);

typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

/* ============================================================================
 * Visual effects: Click-through and blur
 * ============================================================================ */

/**
 * @brief Configure window click-through behavior
 * @param hwnd Window handle
 * @param enable TRUE to enable click-through (transparent to mouse), FALSE to disable
 */
void SetClickThrough(HWND hwnd, BOOL enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_TRANSPARENT;
    
    if (enable) {
        exStyle |= WS_EX_TRANSPARENT;
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, COLOR_KEY_BLACK, ALPHA_OPAQUE, LWA_COLORKEY);
        }
        LOG_INFO("Click-through enabled");
    } else {
        if (exStyle & WS_EX_LAYERED) {
            SetLayeredWindowAttributes(hwnd, 0, ALPHA_OPAQUE, LWA_ALPHA);
        }
        LOG_INFO("Click-through disabled");
    }
    
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/**
 * @brief Initialize DWM functions for blur effects
 * @return TRUE if loaded successfully, FALSE otherwise
 */
BOOL InitDWMFunctions(void) {
    HMODULE hDwmapi = LoadLibraryW(DWMAPI_DLL);
    if (hDwmapi) {
        _DwmEnableBlurBehindWindow = (pfnDwmEnableBlurBehindWindow)GetProcAddress(hDwmapi, "DwmEnableBlurBehindWindow");
        if (_DwmEnableBlurBehindWindow) {
            LOG_INFO("DWM blur functions loaded successfully");
            return TRUE;
        }
    }
    LOG_WARNING("Failed to load DWM blur functions");
    return FALSE;
}

/**
 * @brief Apply accent policy to window
 * @param hwnd Window handle
 * @param accentState Accent state to apply
 */
static void ApplyAccentPolicy(HWND hwnd, ACCENT_STATE accentState) {
    ACCENT_POLICY policy = {0};
    policy.AccentState = accentState;
    policy.AccentFlags = 0;
    policy.GradientColor = (accentState == ACCENT_ENABLE_BLURBEHIND) ? 
                          ((BLUR_ALPHA_VALUE << 24) | BLUR_GRADIENT_COLOR) : 0;
    
    WINDOWCOMPOSITIONATTRIBDATA data = {0};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);
    
    if (SetWindowCompositionAttribute) {
        SetWindowCompositionAttribute(hwnd, &data);
    } else if (_DwmEnableBlurBehindWindow) {
        DWM_BLURBEHIND bb = {0};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = (accentState != ACCENT_DISABLED);
        bb.hRgnBlur = NULL;
        _DwmEnableBlurBehindWindow(hwnd, &bb);
    }
}

/**
 * @brief Enable or disable blur-behind effect
 * @param hwnd Window handle
 * @param enable TRUE to enable blur, FALSE to disable
 */
void SetBlurBehind(HWND hwnd, BOOL enable) {
    ApplyAccentPolicy(hwnd, enable ? ACCENT_ENABLE_BLURBEHIND : ACCENT_DISABLED);
    LOG_INFO("Blur effect %s", enable ? "enabled" : "disabled");
}

/* ============================================================================
 * Multi-monitor support
 * ============================================================================ */

/**
 * @brief Check if monitor is active and usable
 * @param hMonitor Monitor handle
 * @return TRUE if active with valid work area, FALSE otherwise
 */
static BOOL IsMonitorActive(HMONITOR hMonitor) {
    if (!hMonitor) return FALSE;
    
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    
    if (!GetMonitorInfo(hMonitor, &mi)) return FALSE;
    
    return (mi.rcWork.right > mi.rcWork.left && mi.rcWork.bottom > mi.rcWork.top);
}

/**
 * @brief Find best active monitor for window placement
 * @return Handle to active monitor (prioritizes primary)
 */
static HMONITOR FindBestActiveMonitor(void) {
    HMONITOR hPrimary = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (IsMonitorActive(hPrimary)) {
        return hPrimary;
    }
    
    LOG_WARNING("Primary monitor inactive, searching for active display");
    
    DISPLAY_DEVICEW dispDevice = {0};
    dispDevice.cb = sizeof(DISPLAY_DEVICEW);
    
    for (DWORD iDevNum = 0; EnumDisplayDevicesW(NULL, iDevNum, &dispDevice, 0); iDevNum++) {
        if (!(dispDevice.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
        
        DEVMODEW devMode = {0};
        devMode.dmSize = sizeof(DEVMODEW);
        
        if (EnumDisplaySettingsW(dispDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode)) {
            POINT pt = {devMode.dmPosition.x + 1, devMode.dmPosition.y + 1};
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
            
            if (hMon && IsMonitorActive(hMon)) {
                LOG_INFO("Found active monitor: %d", iDevNum);
                return hMon;
            }
        }
    }
    
    LOG_WARNING("No active monitor found, falling back to primary");
    return hPrimary;
}

/**
 * @brief Check if window is visible on current monitor
 * @param hwnd Window handle
 * @param hMonitor Monitor handle
 * @return TRUE if window visible on monitor, FALSE otherwise
 */
static BOOL IsWindowVisibleOnMonitor(HWND hwnd, HMONITOR hMonitor) {
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return FALSE;
    
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return FALSE;
    
    RECT intersection;
    return IntersectRect(&intersection, &rect, &mi.rcWork);
}

/**
 * @brief Center window on target monitor
 * @param hwnd Window handle
 * @param hMonitor Target monitor
 */
static void CenterWindowOnMonitor(HWND hwnd, HMONITOR hMonitor) {
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return;
    
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return;
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    int newX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    int newY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    
    // Ensure within bounds
    if (newX < mi.rcWork.left) newX = mi.rcWork.left;
    if (newY < mi.rcWork.top) newY = mi.rcWork.top;
    if (newX + width > mi.rcWork.right) newX = mi.rcWork.right - width;
    if (newY + height > mi.rcWork.bottom) newY = mi.rcWork.bottom - height;
    
    SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    LOG_INFO("Window centered on monitor at (%d, %d)", newX, newY);
    
    SaveWindowSettings(hwnd);
}

/**
 * @brief Adjust window position to ensure visibility on active monitor
 * @param hwnd Window handle
 * @param forceOnScreen TRUE to force repositioning if needed
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen) {
    if (!forceOnScreen) return;
    
    HMONITOR hCurrentMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    BOOL needsReposition = FALSE;
    
    // Check if monitor is invalid or inactive
    if (!hCurrentMonitor || !IsMonitorActive(hCurrentMonitor)) {
        LOG_WARNING("Window on invalid/inactive monitor, repositioning needed");
        needsReposition = TRUE;
    } 
    // Check if window is visible on current monitor
    else if (!IsWindowVisibleOnMonitor(hwnd, hCurrentMonitor)) {
        LOG_WARNING("Window not visible on current monitor, repositioning needed");
        needsReposition = TRUE;
    }
    
    if (needsReposition) {
        HMONITOR hTargetMonitor = FindBestActiveMonitor();
        CenterWindowOnMonitor(hwnd, hTargetMonitor);
    }
}

/* ============================================================================
 * Window settings persistence
 * ============================================================================ */

/**
 * @brief Save window position and scale to configuration
 * @param hwnd Window handle
 */
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

    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", CLOCK_WINDOW_POS_X, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", CLOCK_WINDOW_POS_Y, config_path);

    char scaleStr[16];
    snprintf(scaleStr, sizeof(scaleStr), "%.2f", CLOCK_WINDOW_SCALE);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr, config_path);
    
    LOG_INFO("Window settings saved: pos(%d, %d), scale(%.2f)", 
             CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, CLOCK_WINDOW_SCALE);
}

/* ============================================================================
 * DPI awareness initialization
 * ============================================================================ */

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

typedef enum {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

/**
 * @brief Initialize DPI awareness with fallback support
 * @return TRUE if any level of DPI awareness was set
 */
static BOOL InitializeDpiAwareness(void) {
    LOG_INFO("Initializing DPI awareness");
    
    HMODULE hUser32 = GetModuleHandleW(USER32_DLL);
    if (!hUser32) {
        LOG_WARNING("Failed to get user32.dll handle");
        return FALSE;
    }
    
    // Try Windows 10 1703+ (best)
    typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    SetProcessDpiAwarenessContextFunc setDpiCtx = 
        (SetProcessDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
    
    if (setDpiCtx) {
        if (setDpiCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            LOG_INFO("DPI awareness: Per-Monitor V2 (Windows 10 1703+)");
            return TRUE;
        }
    }
    
    // Try Windows 8.1+ (fallback)
    HMODULE hShcore = LoadLibraryW(SHCORE_DLL);
    if (hShcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
        SetProcessDpiAwarenessFunc setDpiAwareness = 
            (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        
        if (setDpiAwareness) {
            if (SUCCEEDED(setDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE))) {
                LOG_INFO("DPI awareness: Per-Monitor (Windows 8.1+)");
                FreeLibrary(hShcore);
                return TRUE;
            }
        }
        FreeLibrary(hShcore);
    }
    
    // Final fallback: basic DPI awareness
    #ifndef _INC_WINUSER
    WINUSERAPI BOOL WINAPI SetProcessDPIAware(VOID);
    #endif
    
    if (SetProcessDPIAware()) {
        LOG_INFO("DPI awareness: System (legacy)");
        return TRUE;
    }
    
    LOG_WARNING("Failed to set any DPI awareness level");
    return FALSE;
}

/* ============================================================================
 * Font initialization
 * ============================================================================ */

/**
 * @brief Initialize fonts from configuration or embedded resources
 * @param hInstance Application instance
 * @return TRUE on success, FALSE on failure
 */
static BOOL InitializeFonts(HINSTANCE hInstance) {
    LOG_INFO("Initializing fonts");
    
    // Extract embedded fonts on first run
    if (IsFirstRun()) {
        LOG_INFO("First run detected, extracting embedded fonts");
        if (ExtractEmbeddedFontsToFolder(hInstance)) {
            SetFirstRunCompleted();
            LOG_INFO("Embedded fonts extracted successfully");
        } else {
            LOG_WARNING("Failed to extract embedded fonts");
        }
    }
    
    // Check font license acceptance
    if (NeedsFontLicenseVersionAcceptance()) {
        LOG_INFO("Font license acceptance required (will be handled in UI)");
    }
    
    // Load font from configuration
    CheckAndFixFontPath();
    
    // Extract font filename from FONT_FILE_NAME (may contain path prefix)
    char actualFontFileName[MAX_PATH];
    const char* localappdata_prefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    } else {
        strncpy(actualFontFileName, FONT_FILE_NAME, sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    
    if (!LoadFontByNameAndGetRealName(hInstance, actualFontFileName, 
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        LOG_ERROR("Failed to load font: %s", actualFontFileName);
        return FALSE;
    }
    
    LOG_INFO("Font loaded successfully: %s", FONT_INTERNAL_NAME);
    return TRUE;
}

/* ============================================================================
 * Application initialization
 * ============================================================================ */

/**
 * @brief Initialize default application settings
 * @return TRUE on success
 */
static BOOL InitializeDefaultSettings(void) {
    LOG_INFO("Initializing default settings");
    
    SetConsoleOutputCP(CONSOLE_CODEPAGE_GBK);
    SetConsoleCP(CONSOLE_CODEPAGE_GBK);
    
    ReadConfig();
    CLOCK_FONT_SCALE_FACTOR = CLOCK_WINDOW_SCALE;
    
    UpdateStartupShortcut();
    InitializeDefaultLanguage();
    
    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
    
    LOG_INFO("Default settings initialized");
    return TRUE;
}

/**
 * @brief Initialize application components and subsystems
 * @param hInstance Application instance handle
 * @return TRUE if initialization succeeded, FALSE on error
 */
BOOL InitializeApplication(HINSTANCE hInstance) {
    LOG_INFO("Application initialization started");
    
    if (!InitializeDpiAwareness()) {
        LOG_WARNING("DPI awareness initialization failed, continuing anyway");
    }
    
    if (!InitializeDefaultSettings()) {
        LOG_ERROR("Default settings initialization failed");
        return FALSE;
    }
    
    if (!InitializeFonts(hInstance)) {
        LOG_ERROR("Font initialization failed");
        return FALSE;
    }
    
    LOG_INFO("Application initialization completed successfully");
    return TRUE;
}

/* ============================================================================
 * Main window creation
 * ============================================================================ */

/**
 * @brief Initialize tray icon and animation
 * @param hwnd Window handle
 * @param hInstance Application instance
 */
static void InitializeTrayAndAnimation(HWND hwnd, HINSTANCE hInstance) {
    InitTrayIcon(hwnd, hInstance);
    StartTrayAnimation(hwnd, DEFAULT_TRAY_ANIMATION_SPEED_MS);
    LOG_INFO("Tray icon and animation initialized");
}

/**
 * @brief Apply topmost mode to window
 * @param hwnd Window handle
 */
static void ApplyTopmostMode(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    LOG_INFO("Window set to topmost mode");
}

/**
 * @brief Apply normal (non-topmost) mode to window
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
 * @brief Apply initial window state and visibility
 * @param hwnd Window handle
 * @param nCmdShow Initial show command
 */
static void ApplyInitialWindowState(HWND hwnd, int nCmdShow) {
    SetLayeredWindowAttributes(hwnd, COLOR_KEY_BLACK, ALPHA_OPAQUE, LWA_COLORKEY);
    SetBlurBehind(hwnd, FALSE);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    if (CLOCK_WINDOW_TOPMOST) {
        ApplyTopmostMode(hwnd);
    } else {
        ApplyNormalMode(hwnd);
    }
}

/**
 * @brief Create and initialize main application window
 * @param hInstance Application instance handle
 * @param nCmdShow Window show command
 * @return Window handle on success, NULL on failure
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    LOG_INFO("Creating main window");
    
    // Register window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassW(&wc)) {
        LOG_ERROR("Window class registration failed");
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }
    
    // Configure extended styles
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (!CLOCK_WINDOW_TOPMOST) {
        exStyle |= WS_EX_NOACTIVATE;
    }
    
    // Calculate initial size
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
        LOG_ERROR("Window creation failed");
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);
    
    InitializeTrayAndAnimation(hwnd, hInstance);
    ApplyInitialWindowState(hwnd, nCmdShow);
    
    LOG_INFO("Main window created successfully (handle: 0x%p)", hwnd);
    return hwnd;
}

/* ============================================================================
 * Dialog and utility functions
 * ============================================================================ */

/**
 * @brief Open file selection dialog
 * @param hwnd Parent window handle
 * @param filePath Buffer to receive selected file path
 * @param maxPath Maximum buffer size
 * @return TRUE if file selected, FALSE if cancelled
 */
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

/* ============================================================================
 * Window z-order management
 * ============================================================================ */

/**
 * @brief Set window always-on-top behavior
 * @param hwnd Window handle
 * @param topmost TRUE for topmost, FALSE for normal
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost) {
    LOG_INFO("Setting window topmost: %s", topmost ? "true" : "false");
    
    CLOCK_WINDOW_TOPMOST = topmost;
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (topmost) {
        exStyle &= ~WS_EX_NOACTIVATE;
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        exStyle &= ~WS_EX_NOACTIVATE;
        
        HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
        if (hProgman) {
            SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
        } else {
            LOG_WARNING("Progman window not found, clearing parent");
            SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
        }
        
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        AdjustWindowPosition(hwnd, TRUE);
    }
    
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
    LOG_INFO("Window topmost setting applied and saved");
}

/**
 * @brief Find WorkerW window that hosts desktop wallpaper
 * @return WorkerW window handle or NULL if not found
 */
static HWND FindDesktopWorkerWindow(void) {
    HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
    if (!hProgman) return NULL;
    
    HWND hWorkerW = FindWindowExW(NULL, NULL, WORKERW_CLASS, NULL);
    while (hWorkerW) {
        HWND hView = FindWindowExW(hWorkerW, NULL, SHELLDLL_CLASS, NULL);
        if (hView) {
            LOG_INFO("Found desktop WorkerW window");
            return hWorkerW;
        }
        hWorkerW = FindWindowExW(NULL, hWorkerW, WORKERW_CLASS, NULL);
    }
    
    LOG_WARNING("Desktop WorkerW window not found");
    return hProgman;
}

/**
 * @brief Attach window to desktop wallpaper level
 * @param hwnd Window handle
 */
void ReattachToDesktop(HWND hwnd) {
    LOG_INFO("Reattaching window to desktop level");
    
    HWND hDesktop = FindDesktopWorkerWindow();
    
    if (hDesktop) {
        SetParent(hwnd, hDesktop);
        LOG_INFO("Window parented to desktop worker");
    } else {
        SetParent(hwnd, NULL);
        LOG_WARNING("Desktop worker not found, clearing parent");
    }
    
    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

