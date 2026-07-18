/**
 * @file window_core.c
 * @brief Core window creation and lifecycle management
 */

#include "window/window_core.h"
#include "window.h"
#include "window/window_visual_effects.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
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
#define DISPLAY_RESTORE_DELAY_MS 750
#define FULLSCREEN_RESTORE_RETRY_MS 1000
#define FULLSCREEN_RECT_TOLERANCE_PX 2
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
BOOL CLOCK_WINDOW_POSITION_MANUAL = FALSE;
static DWORD g_systemPositionGuardUntil = 0;
static BOOL g_pendingSystemPositionRestore = FALSE;
static BOOL g_displayRestoreDeferredForFullscreen = FALSE;
static BOOL g_positionTemporarilyRelocatedForDisplay = FALSE;
static BOOL g_placementRetryNeeded = FALSE;

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
    if (!IsTrayIconActive(hwnd)) {
        LOG_WARNING("Tray icon initialization failed; skipping tray animation startup");
        return;
    }

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

    int showCommand = (nCmdShow == SW_HIDE) ? SW_HIDE : SW_SHOWNOACTIVATE;
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    RefreshWindowTopmostState(hwnd);
}

static BOOL IsCurrentProcessWindow(HWND hwnd) {
    DWORD processId = 0;
    if (!hwnd) return FALSE;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

static BOOL IsSpecialWindowPositionX(int posX) {
    return posX == DEFAULT_WINDOW_POS_X || posX == -1;
}

static int ClampInt64ToInt(long long value) {
    if (value < INT_MIN) return INT_MIN;
    if (value > INT_MAX) return INT_MAX;
    return (int)value;
}

static int AddIntsClamped(int first, int second) {
    return ClampInt64ToInt((long long)first + second);
}

static BOOL IsShellOrDesktopWindow(HWND hwnd) {
    wchar_t className[64] = {0};
    if (!hwnd || GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, L"Progman") == 0 ||
           wcscmp(className, L"WorkerW") == 0 ||
           wcscmp(className, L"SHELLDLL_DefView") == 0 ||
           wcscmp(className, L"Shell_TrayWnd") == 0 ||
           wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

static BOOL IsFullscreenForegroundWindowActive(HWND hwnd) {
    HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == hwnd || foreground == GetDesktopWindow()) {
        return FALSE;
    }
    if (!IsWindow(foreground) || !IsWindowVisible(foreground) || IsIconic(foreground)) {
        return FALSE;
    }
    if (IsShellOrDesktopWindow(foreground)) {
        return FALSE;
    }

    DWORD foregroundProcessId = 0;
    GetWindowThreadProcessId(foreground, &foregroundProcessId);
    if (foregroundProcessId == GetCurrentProcessId()) {
        return FALSE;
    }

    SetLastError(0);
    LONG_PTR style = GetWindowLongPtr(foreground, GWL_STYLE);
    if (style == 0 && GetLastError() != 0) {
        return FALSE;
    }
    if ((style & WS_CHILD) != 0) {
        return FALSE;
    }
    if ((style & (WS_CAPTION | WS_THICKFRAME)) != 0 && IsZoomed(foreground)) {
        return FALSE;
    }

    RECT foregroundRect;
    if (!GetWindowRect(foreground, &foregroundRect) ||
        foregroundRect.right <= foregroundRect.left ||
        foregroundRect.bottom <= foregroundRect.top) {
        return FALSE;
    }

    HMONITOR hMon = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hMon, &mi)) {
        return FALSE;
    }

    return foregroundRect.left <= mi.rcMonitor.left + FULLSCREEN_RECT_TOLERANCE_PX &&
           foregroundRect.top <= mi.rcMonitor.top + FULLSCREEN_RECT_TOLERANCE_PX &&
           foregroundRect.right >= mi.rcMonitor.right - FULLSCREEN_RECT_TOLERANCE_PX &&
           foregroundRect.bottom >= mi.rcMonitor.bottom - FULLSCREEN_RECT_TOLERANCE_PX;
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

typedef struct MonitorVisibilityCheck {
    RECT windowRect;
    BOOL visible;
} MonitorVisibilityCheck;

static BOOL CALLBACK CheckWindowVisibilityOnMonitor(HMONITOR monitor,
                                                     HDC hdc,
                                                     LPRECT monitorRect,
                                                     LPARAM contextValue) {
    (void)monitor;
    (void)hdc;
    MonitorVisibilityCheck* check = (MonitorVisibilityCheck*)contextValue;
    if (!check || !monitorRect) return FALSE;

    RECT intersection = {0};
    if (IntersectRect(&intersection, &check->windowRect, monitorRect) &&
        intersection.right - intersection.left >= WINDOW_VISIBLE_MARGIN &&
        intersection.bottom - intersection.top >= WINDOW_VISIBLE_MARGIN) {
        check->visible = TRUE;
        return FALSE;
    }
    return TRUE;
}

static BOOL IsWindowRectVisibleOnAnyMonitor(const RECT* rect) {
    if (!rect || rect->right <= rect->left || rect->bottom <= rect->top) {
        return FALSE;
    }

    MonitorVisibilityCheck check = {0};
    check.windowRect = *rect;
    EnumDisplayMonitors(NULL, NULL, CheckWindowVisibilityOnMonitor,
                        (LPARAM)&check);
    return check.visible;
}

typedef struct MonitorDeviceSearch {
    const wchar_t* monitorId;
    HMONITOR monitor;
    MONITORINFOEXW info;
} MonitorDeviceSearch;

static BOOL GetPersistentMonitorIdW(const wchar_t* displayName,
                                    wchar_t* monitorId,
                                    size_t monitorIdSize) {
    if (!displayName || !*displayName || !monitorId || monitorIdSize == 0) {
        return FALSE;
    }

    DISPLAY_DEVICEW device = {0};
    device.cb = sizeof(device);
    const wchar_t* value = displayName;
    if (EnumDisplayDevicesW(displayName, 0, &device,
                            EDD_GET_DEVICE_INTERFACE_NAME) &&
        device.DeviceID[0] != L'\0') {
        value = device.DeviceID;
    }

    if (wcslen(value) >= monitorIdSize) return FALSE;
    wcscpy_s(monitorId, monitorIdSize, value);
    return TRUE;
}

static BOOL CALLBACK FindMonitorByDeviceCallback(HMONITOR monitor,
                                                  HDC hdc,
                                                  LPRECT monitorRect,
                                                  LPARAM contextValue) {
    (void)hdc;
    (void)monitorRect;
    MonitorDeviceSearch* search = (MonitorDeviceSearch*)contextValue;
    if (!search || !search->monitorId) return FALSE;

    MONITORINFOEXW info = {0};
    info.cbSize = sizeof(info);
    wchar_t monitorId[256] = {0};
    if (GetMonitorInfoW(monitor, (MONITORINFO*)&info) &&
        GetPersistentMonitorIdW(info.szDevice, monitorId,
                                sizeof(monitorId) / sizeof(monitorId[0])) &&
        (_wcsicmp(monitorId, search->monitorId) == 0 ||
         _wcsicmp(info.szDevice, search->monitorId) == 0)) {
        search->monitor = monitor;
        search->info = info;
        return FALSE;
    }
    return TRUE;
}

static BOOL FindMonitorByIdUtf8(const char* monitorId,
                                HMONITOR* outMonitor,
                                MONITORINFOEXW* outInfo) {
    if (!monitorId || !*monitorId || !outMonitor || !outInfo) return FALSE;

    wchar_t monitorIdW[256] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, monitorId, -1,
                            monitorIdW,
                            sizeof(monitorIdW) / sizeof(monitorIdW[0])) <= 0) {
        return FALSE;
    }

    MonitorDeviceSearch search = {0};
    search.monitorId = monitorIdW;
    EnumDisplayMonitors(NULL, NULL, FindMonitorByDeviceCallback,
                        (LPARAM)&search);
    if (!search.monitor) return FALSE;

    *outMonitor = search.monitor;
    *outInfo = search.info;
    return TRUE;
}

static BOOL GetMonitorPlacementData(const RECT* windowRect,
                                    char* monitorId,
                                    size_t monitorIdSize,
                                    int* monitorOffsetX,
                                    int* monitorOffsetY,
                                    BOOL* taskbarAvailable,
                                    BOOL* taskbarAnchored,
                                    int* taskbarAxisRatio,
                                    int* taskbarCrossOffset) {
    if (!windowRect || !monitorId || monitorIdSize == 0 ||
        !monitorOffsetX || !monitorOffsetY || !taskbarAvailable ||
        !taskbarAnchored ||
        !taskbarAxisRatio || !taskbarCrossOffset) {
        return FALSE;
    }

    HMONITOR monitor = MonitorFromRect(windowRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info = {0};
    wchar_t monitorIdW[256] = {0};
    info.cbSize = sizeof(info);
    if (!monitor || !GetMonitorInfoW(monitor, (MONITORINFO*)&info) ||
        !GetPersistentMonitorIdW(info.szDevice, monitorIdW,
                                 sizeof(monitorIdW) / sizeof(monitorIdW[0])) ||
        monitorIdSize > INT_MAX ||
        WideCharToMultiByte(CP_UTF8, 0, monitorIdW, -1,
                            monitorId, (int)monitorIdSize,
                            NULL, NULL) <= 0) {
        monitorId[0] = '\0';
        return FALSE;
    }

    *monitorOffsetX = windowRect->left - info.rcMonitor.left;
    *monitorOffsetY = windowRect->top - info.rcMonitor.top;
    *taskbarAvailable = FALSE;
    *taskbarAnchored = FALSE;
    *taskbarAxisRatio = 0;
    *taskbarCrossOffset = 0;

    RECT taskbarRect = {0};
    RECT intersection = {0};
    if (!GetTaskbarRectForMonitor(monitor, &taskbarRect)) {
        return TRUE;
    }
    *taskbarAvailable = TRUE;
    if (!IntersectRect(&intersection, windowRect, &taskbarRect)) return TRUE;

    if (WindowPlacement_CaptureTaskbarAnchor(
            windowRect, &taskbarRect, &info.rcMonitor,
            taskbarAxisRatio, taskbarCrossOffset)) {
        *taskbarAnchored = TRUE;
    }
    return TRUE;
}

static BOOL TryResolvePlacementMetadata(const char* configPath,
                                        int width,
                                        int height,
                                        int* posX,
                                        int* posY,
                                        BOOL* placementUnavailable) {
    if (!configPath || !posX || !posY || !placementUnavailable ||
        !CLOCK_WINDOW_POSITION_MANUAL) {
        return FALSE;
    }

    char monitorId[256] = {0};
    ReadIniString(INI_SECTION_DISPLAY, WINDOW_MONITOR_ID_KEY, "",
                  monitorId, sizeof(monitorId), configPath);
    if (monitorId[0] == '\0') return FALSE;

    HMONITOR monitor = NULL;
    MONITORINFOEXW info = {0};
    if (!FindMonitorByIdUtf8(monitorId, &monitor, &info)) {
        *placementUnavailable = TRUE;
        return FALSE;
    }

    int monitorOffsetX = ReadIniInt(INI_SECTION_DISPLAY,
                                    WINDOW_MONITOR_OFFSET_X_KEY, 0, configPath);
    int monitorOffsetY = ReadIniInt(INI_SECTION_DISPLAY,
                                    WINDOW_MONITOR_OFFSET_Y_KEY, 0, configPath);
    *posX = AddIntsClamped(info.rcMonitor.left, monitorOffsetX);
    *posY = AddIntsClamped(info.rcMonitor.top, monitorOffsetY);

    if (!ReadIniBool(INI_SECTION_DISPLAY, WINDOW_TASKBAR_ANCHORED_KEY,
                     FALSE, configPath)) {
        return TRUE;
    }

    RECT taskbarRect = {0};
    if (!GetTaskbarRectForMonitor(monitor, &taskbarRect)) {
        *placementUnavailable = TRUE;
        g_placementRetryNeeded = TRUE;
        return TRUE;
    }

    int ratio = ReadIniInt(INI_SECTION_DISPLAY,
                           WINDOW_TASKBAR_AXIS_RATIO_KEY, 0, configPath);
    int crossOffset = ReadIniInt(INI_SECTION_DISPLAY,
                                 WINDOW_TASKBAR_CROSS_OFFSET_KEY, 0, configPath);
    if (!WindowPlacement_ResolveTaskbarAnchor(
            &taskbarRect, &info.rcMonitor, width, height,
            ratio, crossOffset, posX, posY)) {
        *placementUnavailable = TRUE;
        return FALSE;
    }
    return TRUE;
}

void ClampWindowPositionToVisibleMonitor(int width, int height, int* x, int* y) {
    if (!x || !y || width <= 0 || height <= 0) return;

    RECT rc = {*x, *y, AddIntsClamped(*x, width),
               AddIntsClamped(*y, height)};
    HMONITOR hMon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hMon, &mi)) {
        GetPrimaryMonitorInfo(&mi);
    }

    if ((long long)*x + WINDOW_VISIBLE_MARGIN > mi.rcMonitor.right) {
        *x = mi.rcMonitor.right - WINDOW_VISIBLE_MARGIN;
    }
    if ((long long)*x + width - WINDOW_VISIBLE_MARGIN < mi.rcMonitor.left) {
        *x = ClampInt64ToInt((long long)mi.rcMonitor.left - width +
                             WINDOW_VISIBLE_MARGIN);
    }
    if ((long long)*y + WINDOW_VISIBLE_MARGIN > mi.rcMonitor.bottom) {
        *y = mi.rcMonitor.bottom - WINDOW_VISIBLE_MARGIN;
    }
    if ((long long)*y + height - WINDOW_VISIBLE_MARGIN < mi.rcMonitor.top) {
        *y = ClampInt64ToInt((long long)mi.rcMonitor.top - height +
                             WINDOW_VISIBLE_MARGIN);
    }
}

static BOOL ScheduleDisplayRestoreTimer(HWND hwnd, UINT delayMs) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }
    if (!SetTimer(hwnd, TIMER_ID_DISPLAY_RESTORE, delayMs, NULL)) {
        LOG_WARNING("Failed to schedule display restore timer (delay=%u, error=%lu)",
                    delayMs, GetLastError());
        return FALSE;
    }
    return TRUE;
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
    if (!CLOCK_EDIT_MODE) {
        exStyle |= WS_EX_TRANSPARENT;
    }
    if (CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        exStyle |= WS_EX_TOPMOST;
    } else {
        exStyle |= WS_EX_NOACTIVATE;
    }
    
    int initialWidth = ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH, CLOCK_WINDOW_SCALE);
    int initialHeight = ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT, CLOCK_WINDOW_SCALE);

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
        UnregisterClassW(WINDOW_CLASS_NAME, hInstance);
        return NULL;
    }

    InitializeTrayAndAnimation(hwnd, hInstance);
    ApplyInitialWindowState(hwnd, nCmdShow);
    if (g_pendingSystemPositionRestore) {
        ScheduleDisplayRestoreTimer(hwnd, DISPLAY_RESTORE_DELAY_MS);
    }
    
    LOG_INFO("Main window created successfully (handle: 0x%p)", hwnd);
    return hwnd;
}

HWND FindCurrentProcessMainWindow(void) {
    HWND hwnd = NULL;

    while ((hwnd = FindWindowExW(NULL, hwnd, WINDOW_CLASS_NAME, WINDOW_TITLE)) != NULL) {
        if (IsCurrentProcessWindow(hwnd)) {
            return hwnd;
        }
    }

    return NULL;
}

BOOL SaveWindowSettings(HWND hwnd) {
    if (!hwnd) return FALSE;

    if ((IsSystemPositionChangeGuardActive() || g_pendingSystemPositionRestore) &&
        !CLOCK_EDIT_MODE) {
        return FALSE;
    }

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        LOG_WARNING("Failed to get window rect for saving");
        return FALSE;
    }
    
    CLOCK_WINDOW_POS_X = rect.left;
    CLOCK_WINDOW_POS_Y = rect.top;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char posXStr[16], posYStr[16], scaleStr[64], pluginScaleStr[64];
    char monitorId[256] = {0};
    char monitorOffsetXStr[16], monitorOffsetYStr[16];
    char taskbarAxisRatioStr[16], taskbarCrossOffsetStr[16];
    int monitorOffsetX = 0;
    int monitorOffsetY = 0;
    int taskbarAxisRatio = 0;
    int taskbarCrossOffset = 0;
    BOOL taskbarAnchored = FALSE;
    BOOL taskbarAvailable = FALSE;
    snprintf(posXStr, sizeof(posXStr), "%d", CLOCK_WINDOW_POS_X);
    snprintf(posYStr, sizeof(posYStr), "%d", CLOCK_WINDOW_POS_Y);
    snprintf(scaleStr, sizeof(scaleStr), "%.9g", CLOCK_WINDOW_SCALE);
    snprintf(pluginScaleStr, sizeof(pluginScaleStr), "%.9g", PLUGIN_FONT_SCALE_FACTOR);
    BOOL placementDataAvailable = GetMonitorPlacementData(
        &rect, monitorId, sizeof(monitorId),
        &monitorOffsetX, &monitorOffsetY, &taskbarAvailable,
        &taskbarAnchored, &taskbarAxisRatio, &taskbarCrossOffset);
    snprintf(monitorOffsetXStr, sizeof(monitorOffsetXStr), "%d", monitorOffsetX);
    snprintf(monitorOffsetYStr, sizeof(monitorOffsetYStr), "%d", monitorOffsetY);
    snprintf(taskbarAxisRatioStr, sizeof(taskbarAxisRatioStr), "%d",
             taskbarAxisRatio);
    snprintf(taskbarCrossOffsetStr, sizeof(taskbarCrossOffsetStr), "%d",
             taskbarCrossOffset);

    if ((!CLOCK_WINDOW_POSITION_MANUAL ||
         g_positionTemporarilyRelocatedForDisplay) && !CLOCK_EDIT_MODE) {
        /* Preserve automatic placement sentinels and unavailable-monitor
         * coordinates. Independent scale changes are still safe to persist. */
        const IniKeyValue scaleUpdates[] = {
            {INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr},
            {INI_SECTION_DISPLAY, "PLUGIN_SCALE", pluginScaleStr}
        };
        BOOL saved = WriteIniMultipleAtomic(
            config_path, scaleUpdates,
            sizeof(scaleUpdates) / sizeof(scaleUpdates[0]));
        if (!saved) {
            LOG_WARNING("Failed to save window scale while preserving unavailable-monitor position");
        }
        return saved;
    }

    IniKeyValue updates[11];
    size_t updateCount = 0;
    updates[updateCount++] = (IniKeyValue){
        INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", posXStr};
    updates[updateCount++] = (IniKeyValue){
        INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", posYStr};
    updates[updateCount++] = (IniKeyValue){
        INI_SECTION_DISPLAY, WINDOW_POSITION_MANUAL_KEY, "TRUE"};
    if (placementDataAvailable) {
        updates[updateCount++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_MONITOR_ID_KEY, monitorId};
        updates[updateCount++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_MONITOR_OFFSET_X_KEY,
            monitorOffsetXStr};
        updates[updateCount++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_MONITOR_OFFSET_Y_KEY,
            monitorOffsetYStr};
    }
    if (taskbarAvailable) {
        updates[updateCount++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_TASKBAR_ANCHORED_KEY,
            taskbarAnchored ? "TRUE" : "FALSE"};
        updates[updateCount++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_TASKBAR_AXIS_RATIO_KEY,
            taskbarAxisRatioStr};
        updates[updateCount++] = (IniKeyValue){
            INI_SECTION_DISPLAY, WINDOW_TASKBAR_CROSS_OFFSET_KEY,
            taskbarCrossOffsetStr};
    }
    updates[updateCount++] = (IniKeyValue){
        INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr};
    updates[updateCount++] = (IniKeyValue){
        INI_SECTION_DISPLAY, "PLUGIN_SCALE", pluginScaleStr};

    if (WriteIniMultipleAtomic(config_path, updates, updateCount)) {
        CLOCK_WINDOW_POSITION_MANUAL = TRUE;
        return TRUE;
    } else {
        LOG_WARNING("Failed to save window settings");
        return FALSE;
    }
}

void ResolveConfiguredWindowPosition(int width, int height, int* outX, int* outY) {
    if (!outX || !outY) return;

    int posX;
    int posY;
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);

    int configPosX = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X",
                                CLOCK_WINDOW_POS_X, configPath);
    int configPosY = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y",
                                CLOCK_WINDOW_POS_Y, configPath);
    /* Missing means legacy config: preserve the historical -1/-2 sentinels. */
    CLOCK_WINDOW_POSITION_MANUAL = ReadIniBool(
        INI_SECTION_DISPLAY, WINDOW_POSITION_MANUAL_KEY, FALSE, configPath);
    g_placementRetryNeeded = FALSE;

    MONITORINFO mi = {0};
    GetPrimaryMonitorInfo(&mi);
    if (!CLOCK_WINDOW_POSITION_MANUAL && IsSpecialWindowPositionX(configPosX)) {
        int screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
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

    if (!CLOCK_WINDOW_POSITION_MANUAL && configPosY == DEFAULT_WINDOW_POS_Y) {
        posY = mi.rcMonitor.top;
    } else {
        posY = configPosY;
    }

    BOOL placementUnavailable = FALSE;
    TryResolvePlacementMetadata(configPath, width, height,
                                &posX, &posY, &placementUnavailable);

    RECT configuredRect = {posX, posY, AddIntsClamped(posX, width),
                           AddIntsClamped(posY, height)};
    BOOL configuredPositionVisible = IsWindowRectVisibleOnAnyMonitor(&configuredRect);

    ClampWindowPositionToVisibleMonitor(width, height, &posX, &posY);
    g_positionTemporarilyRelocatedForDisplay =
        CLOCK_WINDOW_POSITION_MANUAL &&
        (placementUnavailable || !configuredPositionVisible);
    if (g_placementRetryNeeded) {
        g_pendingSystemPositionRestore = TRUE;
    }

    *outX = posX;
    *outY = posY;
}

BOOL BeginSystemPositionChangeGuard(HWND hwnd) {
    g_systemPositionGuardUntil = GetTickCount() + SYSTEM_POSITION_GUARD_MS;
    g_pendingSystemPositionRestore = TRUE;
    return ScheduleDisplayRestoreTimer(hwnd, DISPLAY_RESTORE_DELAY_MS);
}

BOOL IsSystemPositionChangeGuardActive(void) {
    if (g_systemPositionGuardUntil == 0) return FALSE;
    if ((LONG)(GetTickCount() - g_systemPositionGuardUntil) < 0) return TRUE;
    g_systemPositionGuardUntil = 0;
    return FALSE;
}

void RestoreWindowPositionAfterSystemChange(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    if (CLOCK_EDIT_MODE) {
        /* An interactive placement is authoritative. Do not leave a stale
         * guard behind that would suppress the save when edit mode ends. */
        ClearPendingSystemPositionRestore();
        return;
    }

    if (IsFullscreenForegroundWindowActive(hwnd)) {
        g_systemPositionGuardUntil = GetTickCount() + SYSTEM_POSITION_GUARD_MS;
        g_pendingSystemPositionRestore = TRUE;
        if (!g_displayRestoreDeferredForFullscreen) {
            LOG_INFO("Deferring window position restore while fullscreen foreground window is active");
            g_displayRestoreDeferredForFullscreen = TRUE;
        }
        /* The display-restore timer is one-shot and was already removed by the
         * message handler. Keep polling at a low rate so this state cannot
         * remain pending forever when the fullscreen window later closes. */
        ScheduleDisplayRestoreTimer(hwnd, FULLSCREEN_RESTORE_RETRY_MS);
        return;
    }

    if (g_displayRestoreDeferredForFullscreen) {
        LOG_INFO("Resuming deferred window position restore after fullscreen foreground window ended");
        g_displayRestoreDeferredForFullscreen = FALSE;
    }

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
    g_pendingSystemPositionRestore = g_placementRetryNeeded;
    if (g_pendingSystemPositionRestore) {
        ScheduleDisplayRestoreTimer(hwnd, DISPLAY_RESTORE_DELAY_MS);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

void TryRestorePendingWindowPosition(HWND hwnd) {
    if (!g_pendingSystemPositionRestore) return;
    RestoreWindowPositionAfterSystemChange(hwnd);
}

void ClearPendingSystemPositionRestore(void) {
    g_pendingSystemPositionRestore = FALSE;
    g_displayRestoreDeferredForFullscreen = FALSE;
    g_systemPositionGuardUntil = 0;
    g_positionTemporarilyRelocatedForDisplay = FALSE;
    g_placementRetryNeeded = FALSE;
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

