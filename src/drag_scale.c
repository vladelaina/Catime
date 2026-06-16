/**
 * @file drag_scale.c
 * @brief Interactive window dragging and scaling with debounced saves
 * 
 * Debounced config saves reduce disk I/O during continuous operations.
 * Mouse-anchored scaling keeps wheel zoom aligned with the cursor.
 */

#include <windows.h>
#include "window.h"
#include "config.h"
#include "drag_scale.h"
#include "log.h"
#include "plugin/plugin_data.h"
#include "window/window_desktop_integration.h"

#include "color/color_parser.h"
#include "color/color_state.h"

BOOL PREVIOUS_TOPMOST_STATE = FALSE;
static BOOL g_editModeForcedTopmost = FALSE;
static BOOL g_editModeTopmostOverride = FALSE;
static BOOL g_pendingScaleResizeAnchorValid = FALSE;
static HWND g_pendingScaleResizeAnchorHwnd = NULL;
static POINT g_pendingScaleResizeAnchor = {0};
static int g_pendingScaleWheelDelta = 0;

static UINT_PTR g_configSaveTimer = 0;
static HWND g_configSaveTimerHwnd = NULL;

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

static BOOL IsValidDragScaleWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static inline void RefreshWindow(HWND hwnd, BOOL eraseBackground) {
    InvalidateRect(hwnd, NULL, eraseBackground);
}

static inline float ClampScaleFactor(float scale) {
    if (scale < MIN_SCALE_FACTOR) return MIN_SCALE_FACTOR;
    if (scale > MAX_SCALE_FACTOR) return MAX_SCALE_FACTOR;
    return scale;
}

static inline int AbsInt(int value) {
    return value < 0 ? -value : value;
}

static int CalculateAnchoredPosition(int originalPos, int originalSize, int newSize, int anchorPos) {
    if (originalSize <= 0) {
        return originalPos;
    }

    double anchorRatio = (double)(anchorPos - originalPos) / (double)originalSize;
    return anchorPos - (int)(anchorRatio * (double)newSize + 0.5);
}

static void SetPendingScaleResizeAnchor(HWND hwnd, POINT anchor) {
    g_pendingScaleResizeAnchorValid = TRUE;
    g_pendingScaleResizeAnchorHwnd = hwnd;
    g_pendingScaleResizeAnchor = anchor;
}

BOOL GetPendingScaleResizeAnchor(HWND hwnd, POINT* anchor) {
    if (!anchor ||
        !g_pendingScaleResizeAnchorValid ||
        g_pendingScaleResizeAnchorHwnd != hwnd) {
        return FALSE;
    }

    *anchor = g_pendingScaleResizeAnchor;
    return TRUE;
}

void ClearPendingScaleResizeAnchor(HWND hwnd) {
    if (!g_pendingScaleResizeAnchorValid ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return;
    }

    g_pendingScaleResizeAnchorValid = FALSE;
    g_pendingScaleResizeAnchorHwnd = NULL;
    g_pendingScaleResizeAnchor.x = 0;
    g_pendingScaleResizeAnchor.y = 0;
}

static VOID CALLBACK ConfigSaveTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != TIMER_ID_CONFIG_SAVE ||
        hwnd != g_configSaveTimerHwnd ||
        !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
    g_configSaveTimer = 0;
    g_configSaveTimerHwnd = NULL;
    SaveWindowSettings(hwnd);
}

/* Debouncing: Only save after operations stop for CONFIG_SAVE_DELAY_MS */
void ScheduleConfigSave(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) return;

    if (g_configSaveTimer != 0) {
        HWND timerHwnd = g_configSaveTimerHwnd ? g_configSaveTimerHwnd : hwnd;
        if (IsValidDragScaleWindow(timerHwnd)) {
            KillTimer(timerHwnd, TIMER_ID_CONFIG_SAVE);
        }
        g_configSaveTimer = 0;
        g_configSaveTimerHwnd = NULL;
    }
    
    g_configSaveTimer = SetTimer(hwnd, TIMER_ID_CONFIG_SAVE, 
                                 CONFIG_SAVE_DELAY_MS, 
                                 (TIMERPROC)ConfigSaveTimerProc);
    if (g_configSaveTimer) {
        g_configSaveTimerHwnd = hwnd;
    } else {
        SaveWindowSettings(hwnd);
    }
}

void CancelScheduledConfigSave(HWND hwnd) {
    if (g_configSaveTimer == 0) return;

    HWND timerHwnd = g_configSaveTimerHwnd ? g_configSaveTimerHwnd : hwnd;
    if (IsValidDragScaleWindow(timerHwnd)) {
        KillTimer(timerHwnd, TIMER_ID_CONFIG_SAVE);
    }
    g_configSaveTimer = 0;
    g_configSaveTimerHwnd = NULL;
}

void StartDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;

    SetCapture(hwnd);
    if (GetCapture() != hwnd) {
        LOG_WARNING("Failed to capture mouse for edit-mode drag");
        return;
    }

    CLOCK_IS_DRAGGING = TRUE;
    GetCursorPos(&CLOCK_LAST_MOUSE_POS);
}

void StartEditMode(HWND hwnd) {
    EnsureWindowVisibleWithTopmostState(hwnd);

    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;
    
    if (!CLOCK_WINDOW_TOPMOST) {
        g_editModeForcedTopmost = TRUE;
        SetWindowTopmostTransient(hwnd, TRUE);
    }
    
    CLOCK_EDIT_MODE = TRUE;
    g_pendingScaleWheelDelta = 0;
    
    RefreshWindow(hwnd, TRUE);
    SetBlurBehind(hwnd, TRUE);
    SetClickThrough(hwnd, FALSE);
    RefreshWindow(hwnd, TRUE);
    
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

void EndEditMode(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;

    if (CLOCK_IS_DRAGGING) {
        CLOCK_IS_DRAGGING = FALSE;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
    }
    ClearPendingScaleResizeAnchor(hwnd);
    g_pendingScaleWheelDelta = 0;

    CLOCK_EDIT_MODE = FALSE;

    SetBlurBehind(hwnd, FALSE);
    SetClickThrough(hwnd, TRUE);
    CancelScheduledConfigSave(hwnd);
    SaveWindowSettings(hwnd);
    
    if (!WriteConfigColor(CLOCK_TEXT_COLOR)) {
        LOG_WARNING("EndEditMode: failed to persist text color");
    }
    
    if (g_editModeForcedTopmost && !g_editModeTopmostOverride && !PREVIOUS_TOPMOST_STATE) {
        SetWindowTopmostTransient(hwnd, FALSE);
        
        InvalidateRect(hwnd, NULL, TRUE);
        
        KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
    } else {
        RefreshWindow(hwnd, TRUE);
    }

    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;
}

void MarkEditModeTopmostOverride(void) {
    if (CLOCK_EDIT_MODE) {
        g_editModeTopmostOverride = TRUE;
    }
}

void EndDragWindow(HWND hwnd) {
    if (!CLOCK_IS_DRAGGING) return;
    
    CLOCK_IS_DRAGGING = FALSE;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    
    RefreshWindow(hwnd, TRUE);
    if (CLOCK_EDIT_MODE) {
        ScheduleConfigSave(hwnd);
    }
}

/* SWP_NOREDRAW + UpdateWindow maintains smooth dragging */
BOOL HandleDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return FALSE;

    if (GetCapture() != hwnd) {
        CLOCK_IS_DRAGGING = FALSE;
        ScheduleConfigSave(hwnd);
        return FALSE;
    }
    
    POINT currentPos;
    GetCursorPos(&currentPos);
    int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    
    int newX = windowRect.left + deltaX;
    int newY = windowRect.top + deltaY;
    
    SetWindowPos(hwnd, NULL, newX, newY, width, height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    
    CLOCK_LAST_MOUSE_POS = currentPos;
    CLOCK_WINDOW_POS_X = newX;
    CLOCK_WINDOW_POS_Y = newY;
    
    InvalidateRect(hwnd, NULL, FALSE);
    return TRUE;
}

/* Mouse wheel scaling: configurable step per notch, anchored at the cursor */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (!CLOCK_EDIT_MODE) {
        g_pendingScaleWheelDelta = 0;
        return FALSE;
    }
    
    BOOL isPluginMode = PluginData_IsActive();
    float oldScale = isPluginMode ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
    if (oldScale <= 0.0f) return FALSE;

    g_pendingScaleWheelDelta += delta;
    int wheelSteps = g_pendingScaleWheelDelta / WHEEL_DELTA;
    if (wheelSteps == 0) return FALSE;
    g_pendingScaleWheelDelta -= wheelSteps * WHEEL_DELTA;
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int oldWidth = windowRect.right - windowRect.left;
    int oldHeight = windowRect.bottom - windowRect.top;
    if (oldWidth <= 0 || oldHeight <= 0) return FALSE;

    POINT cursorPos;
    if (!GetCursorPos(&cursorPos) ||
        cursorPos.x < windowRect.left || cursorPos.x > windowRect.right ||
        cursorPos.y < windowRect.top || cursorPos.y > windowRect.bottom) {
        cursorPos.x = windowRect.left + oldWidth / 2;
        cursorPos.y = windowRect.top + oldHeight / 2;
    }
    
    BOOL isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    int stepPercent = isCtrlDown ? g_AppConfig.display.scale_step_fast 
                                 : g_AppConfig.display.scale_step_normal;
    if (stepPercent <= 0) stepPercent = 1;
    
    float scaleFactor = 1.0f + (stepPercent / 100.0f);
    float newScale = oldScale;
    int stepCount = AbsInt(wheelSteps);
    for (int i = 0; i < stepCount; i++) {
        newScale = (wheelSteps > 0)
            ? newScale * scaleFactor
            : newScale / scaleFactor;
    }
    
    newScale = ClampScaleFactor(newScale);
    
    if (newScale == oldScale) return FALSE;
    
    if (isPluginMode) {
        PLUGIN_FONT_SCALE_FACTOR = newScale;
    } else {
        CLOCK_FONT_SCALE_FACTOR = newScale;
        CLOCK_WINDOW_SCALE = newScale;
    }
    
    float scalingRatio = newScale / oldScale;
    int newWidth = (int)(oldWidth * scalingRatio);
    int newHeight = (int)(oldHeight * scalingRatio);
    
    int newX = CalculateAnchoredPosition(windowRect.left, oldWidth, newWidth, cursorPos.x);
    int newY = CalculateAnchoredPosition(windowRect.top, oldHeight, newHeight, cursorPos.y);
    
    SetWindowPos(hwnd, NULL, 
        newX, newY, newWidth, newHeight,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    SetPendingScaleResizeAnchor(hwnd, cursorPos);

    CLOCK_WINDOW_POS_X = newX;
    CLOCK_WINDOW_POS_Y = newY;
    
    RefreshWindow(hwnd, FALSE);
    ScheduleConfigSave(hwnd);
    return TRUE;
}
