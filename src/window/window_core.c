/**
 * @file window_core.c
 * @brief Main window creation and shared window state
 */

#include "window_core_internal.h"
#include "multi_window.h"

int CLOCK_BASE_WINDOW_WIDTH = 200;
int CLOCK_BASE_WINDOW_HEIGHT = 100;
float CLOCK_WINDOW_SCALE = 1.0f;
int CLOCK_WINDOW_POS_X = 100;
int CLOCK_WINDOW_POS_Y = 100;
BOOL CLOCK_WINDOW_POSITION_MANUAL = FALSE;
BOOL CLOCK_WINDOW_TASKBAR_ANCHORED = FALSE;
DWORD g_systemPositionGuardUntil = 0;
BOOL g_pendingSystemPositionRestore = FALSE;
BOOL g_displayRestoreDeferredForFullscreen = FALSE;
BOOL g_positionTemporarilyRelocatedForDisplay = FALSE;
BOOL g_placementRetryNeeded = FALSE;

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

static void InitializeTrayAndAnimation(HWND hwnd, HINSTANCE instance) {
    InitTrayIcon(hwnd, instance);
    if (!IsTrayIconActive(hwnd)) {
        LOG_WARNING(
            "Tray icon initialization failed; skipping tray animation startup");
        return;
    }
    StartTrayAnimation(hwnd, DEFAULT_TRAY_ANIMATION_SPEED_MS);
    LOG_INFO("Tray icon and animation initialized");
}

static void ApplyInitialWindowState(HWND hwnd, int commandShow) {
    SetBlurBehind(hwnd, FALSE);
    int showCommand = commandShow == SW_HIDE ? SW_HIDE : SW_SHOWNOACTIVATE;
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    RefreshWindowTopmostState(hwnd);
}

BOOL WindowCore_IsCurrentProcessWindow(HWND hwnd) {
    if (!hwnd) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    LOG_INFO("Creating main window");
    if (!MultiWindow_BeginMainWindowCreation()) return NULL;
    WNDCLASSW windowClass = {0};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = WINDOW_CLASS_NAME;
    if (!RegisterClassW(&windowClass)) {
        LOG_WINDOWS_ERROR("Window class registration failed");
        MultiWindow_EndMainWindowCreation();
        return NULL;
    }

    DWORD extendedStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (!CLOCK_EDIT_MODE) extendedStyle |= WS_EX_TRANSPARENT;
    if (CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        extendedStyle |= WS_EX_TOPMOST;
    } else {
        extendedStyle |= WS_EX_NOACTIVATE;
    }
    int initialWidth = ScaleWindowDimensionClamped(
        CLOCK_BASE_WINDOW_WIDTH, CLOCK_WINDOW_SCALE);
    int initialHeight = ScaleWindowDimensionClamped(
        CLOCK_BASE_WINDOW_HEIGHT, CLOCK_WINDOW_SCALE);
    ResolveConfiguredWindowPosition(
        initialWidth, initialHeight,
        &CLOCK_WINDOW_POS_X, &CLOCK_WINDOW_POS_Y);
    HWND hwnd = CreateWindowExW(
        extendedStyle, WINDOW_CLASS_NAME, WINDOW_TITLE, WS_POPUP,
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        initialWidth, initialHeight, NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        LOG_WINDOWS_ERROR("Window creation failed");
        UnregisterClassW(WINDOW_CLASS_NAME, hInstance);
        MultiWindow_EndMainWindowCreation();
        return NULL;
    }
    MultiWindow_EndMainWindowCreation();
    if (MultiWindow_IsSecondary()) {
        /* Persist the newly assigned slot immediately.  Without this, a
           TaskbarCreated/display recovery before the user moves the window
           would fall back to the primary window's shared coordinates. */
        MarkWindowSettingsDirty(WINDOW_SETTINGS_DIRTY_POSITION);
        SaveWindowSettings(hwnd);
    }
    InitializeTrayAndAnimation(hwnd, hInstance);
    ApplyInitialWindowState(hwnd, nCmdShow);
    if (g_pendingSystemPositionRestore) {
        WindowCore_ScheduleDisplayRestoreTimer(
            hwnd, DISPLAY_RESTORE_DELAY_MS);
    }
    LOG_INFO("Main window created successfully (handle: 0x%p)", hwnd);
    return hwnd;
}

HWND FindCurrentProcessMainWindow(void) {
    HWND window = NULL;
    while ((window = FindWindowExW(
                NULL, window, WINDOW_CLASS_NAME, WINDOW_TITLE)) != NULL) {
        if (WindowCore_IsCurrentProcessWindow(window)) return window;
    }
    return NULL;
}
