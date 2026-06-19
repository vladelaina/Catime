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
#include <float.h>
#include <math.h>

BOOL PREVIOUS_TOPMOST_STATE = FALSE;
static BOOL g_editModeForcedTopmost = FALSE;
static BOOL g_editModeTopmostOverride = FALSE;
static BOOL g_pendingScaleResizeAnchorValid = FALSE;
static HWND g_pendingScaleResizeAnchorHwnd = NULL;
static POINT g_pendingScaleResizeAnchor = {0};
static double g_pendingScaleResizeAnchorRatioX = 0.5;
static double g_pendingScaleResizeAnchorRatioY = 0.5;
static UINT_PTR g_scaleApplyTimer = 0;
static HWND g_scaleApplyTimerHwnd = NULL;
static BOOL g_scaleTargetValid = FALSE;
static BOOL g_scaleTargetPluginMode = FALSE;
static float g_scaleTarget = 1.0f;
static POINT g_scaleTargetAnchor = {0};
static double g_scaleGestureAnchorRatioX = 0.5;
static double g_scaleGestureAnchorRatioY = 0.5;
static DWORD g_scaleGestureSerial = 0;
static DWORD g_lastScaleWheelTick = 0;
static DWORD g_suppressDragUntilTick = 0;
static BOOL g_dragBlockedUntilLeftUp = FALSE;
static BOOL g_dragBlockNeedsReleaseCooldown = FALSE;
static BOOL g_pendingScaleResizeAnchorPostScale = FALSE;
static DWORD g_pendingScaleResizeAnchorUntilTick = 0;

static UINT_PTR g_configSaveTimer = 0;
static HWND g_configSaveTimerHwnd = NULL;

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define SCALE_APPLY_TIMER_ID 42426
#define SCALE_APPLY_INTERVAL_MS 16u
#define SCALE_APPLY_IDLE_STOP_MS 80u
#define SCALE_DRAG_SUPPRESS_MS 120u
#define SCALE_DRAG_RELEASE_SUPPRESS_MS 120u
#define SCALE_POST_RESIZE_ANCHOR_MS 1200u

static void SetPendingScaleResizeAnchorWithRatio(HWND hwnd, POINT anchor,
                                                 double ratioX, double ratioY);
static void ForceClearPendingScaleResizeAnchor(void);
static BOOL IsPostScaleResizeAnchorActive(HWND hwnd);

static void SuppressDragForDuration(DWORD durationMs) {
    DWORD until = GetTickCount() + durationMs;
    g_suppressDragUntilTick = until ? until : 1;
}

static void SuppressDragAfterScale(void) {
    SuppressDragForDuration(SCALE_DRAG_SUPPRESS_MS);
}

static BOOL IsDragSuppressedAfterScale(void) {
    if (g_suppressDragUntilTick == 0) {
        return FALSE;
    }

    if ((LONG)(GetTickCount() - g_suppressDragUntilTick) < 0) {
        return TRUE;
    }

    g_suppressDragUntilTick = 0;
    return FALSE;
}

static BOOL IsLeftButtonPhysicallyDown(void) {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

static void ClearDragBlockUntilLeftUp(void) {
    if (!g_dragBlockedUntilLeftUp) {
        return;
    }

    g_dragBlockedUntilLeftUp = FALSE;
    if (g_dragBlockNeedsReleaseCooldown && CLOCK_EDIT_MODE) {
        SuppressDragForDuration(SCALE_DRAG_RELEASE_SUPPRESS_MS);
    }
    g_dragBlockNeedsReleaseCooldown = FALSE;
}

static BOOL IsDragBlockedUntilLeftUp(void) {
    if (!g_dragBlockedUntilLeftUp) {
        return FALSE;
    }

    if (!IsLeftButtonPhysicallyDown()) {
        ClearDragBlockUntilLeftUp();
        return FALSE;
    }

    return TRUE;
}

static void BlockDragUntilLeftUp(HWND hwnd) {
    if (g_dragBlockedUntilLeftUp) {
        return;
    }

    if (!IsLeftButtonPhysicallyDown() &&
        !CLOCK_IS_DRAGGING &&
        GetCapture() != hwnd) {
        return;
    }

    g_dragBlockedUntilLeftUp = TRUE;
    g_dragBlockNeedsReleaseCooldown = TRUE;
}

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

static inline float ClampScaleFactor(double scale) {
    if (!isfinite(scale)) {
        return scale > 0.0 ? FLT_MAX : MIN_SCALE_FACTOR;
    }
    if (scale < MIN_SCALE_FACTOR) return MIN_SCALE_FACTOR;
    if (scale > (double)FLT_MAX) return FLT_MAX;
    return (float)scale;
}

static double CalculateWheelScaleDelta(int delta, int stepPercent, float currentScale) {
    if (delta == 0) return 0.0;
    if (stepPercent <= 0) stepPercent = 1;

    double wheelUnits = (double)delta / (double)WHEEL_DELTA;
    double scaleStep = (double)stepPercent / 100.0;
    double speedFactor = sqrt((double)currentScale);
    if (!isfinite(speedFactor) || speedFactor < 1.0) speedFactor = 1.0;

    double scaleDelta = wheelUnits * scaleStep * speedFactor;
    if (!isfinite(scaleDelta)) return 0.0;
    return scaleDelta;
}

static double ClampAnchorRatio(double ratio) {
    if (!isfinite(ratio)) return 0.5;
    if (ratio < 0.0) return 0.0;
    if (ratio > 1.0) return 1.0;
    return ratio;
}

static void AdvanceScaleGestureSerial(void) {
    g_scaleGestureSerial++;
    if (g_scaleGestureSerial == 0) {
        g_scaleGestureSerial = 1;
    }
}

static float GetActiveScaleFactor(BOOL pluginMode) {
    return pluginMode ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
}

static void SetActiveScaleFactor(BOOL pluginMode, float scale) {
    if (pluginMode) {
        PLUGIN_FONT_SCALE_FACTOR = scale;
    } else {
        CLOCK_FONT_SCALE_FACTOR = scale;
        CLOCK_WINDOW_SCALE = scale;
    }
}

static BOOL ApplyScaleToWindow(HWND hwnd, BOOL pluginMode, float newScale, POINT anchor) {
    float oldScale = GetActiveScaleFactor(pluginMode);
    if (oldScale <= 0.0f || newScale == oldScale) return FALSE;

    SetActiveScaleFactor(pluginMode, newScale);
    SetPendingScaleResizeAnchorWithRatio(hwnd,
                                         anchor,
                                         g_scaleGestureAnchorRatioX,
                                         g_scaleGestureAnchorRatioY);

    RefreshWindow(hwnd, FALSE);
    return TRUE;
}

static void StopScaleApplyTimer(HWND hwnd) {
    BOOL keepAnchorForPostScaleResize =
        CLOCK_EDIT_MODE &&
        (g_scaleApplyTimer != 0 || g_scaleTargetValid) &&
        g_pendingScaleResizeAnchorValid &&
        (!hwnd || g_pendingScaleResizeAnchorHwnd == hwnd);

    if (g_scaleApplyTimer != 0 || g_scaleTargetValid) {
        SuppressDragAfterScale();
    }

    HWND timerHwnd = g_scaleApplyTimerHwnd ? g_scaleApplyTimerHwnd : hwnd;
    if (g_scaleApplyTimer != 0 && IsValidDragScaleWindow(timerHwnd)) {
        KillTimer(timerHwnd, SCALE_APPLY_TIMER_ID);
    }

    g_scaleApplyTimer = 0;
    g_scaleApplyTimerHwnd = NULL;
    g_scaleTargetValid = FALSE;
    g_scaleTargetPluginMode = FALSE;
    g_scaleTarget = 1.0f;
    g_scaleTargetAnchor.x = 0;
    g_scaleTargetAnchor.y = 0;
    g_scaleGestureAnchorRatioX = 0.5;
    g_scaleGestureAnchorRatioY = 0.5;
    g_lastScaleWheelTick = 0;
    if (keepAnchorForPostScaleResize) {
        DWORD until = GetTickCount() + SCALE_POST_RESIZE_ANCHOR_MS;
        g_pendingScaleResizeAnchorPostScale = TRUE;
        g_pendingScaleResizeAnchorUntilTick = until ? until : 1;
    } else {
        ForceClearPendingScaleResizeAnchor();
    }
}

static BOOL ApplyPendingScaleTarget(HWND hwnd) {
    if (!g_scaleTargetValid || !IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }
    if (!CLOCK_EDIT_MODE) {
        StopScaleApplyTimer(hwnd);
        return FALSE;
    }

    return ApplyScaleToWindow(hwnd,
                              g_scaleTargetPluginMode,
                              g_scaleTarget,
                              g_scaleTargetAnchor);
}

static VOID CALLBACK ScaleApplyTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != SCALE_APPLY_TIMER_ID ||
        hwnd != g_scaleApplyTimerHwnd) {
        return;
    }

    ApplyPendingScaleTarget(hwnd);

    DWORD now = GetTickCount();
    if (!g_scaleTargetValid ||
        !CLOCK_EDIT_MODE ||
        (DWORD)(now - g_lastScaleWheelTick) >= SCALE_APPLY_IDLE_STOP_MS) {
        StopScaleApplyTimer(hwnd);
    }
}

static BOOL EnsureScaleApplyTimer(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) return FALSE;
    if (g_scaleApplyTimer != 0 && g_scaleApplyTimerHwnd == hwnd) return TRUE;

    StopScaleApplyTimer(hwnd);
    g_scaleApplyTimer = SetTimer(hwnd,
                                 SCALE_APPLY_TIMER_ID,
                                 SCALE_APPLY_INTERVAL_MS,
                                 (TIMERPROC)ScaleApplyTimerProc);
    if (!g_scaleApplyTimer) {
        return FALSE;
    }

    g_scaleApplyTimerHwnd = hwnd;
    AdvanceScaleGestureSerial();
    return TRUE;
}

static void SetPendingScaleResizeAnchorWithRatio(HWND hwnd, POINT anchor,
                                                 double ratioX, double ratioY) {
    g_pendingScaleResizeAnchorValid = TRUE;
    g_pendingScaleResizeAnchorHwnd = hwnd;
    g_pendingScaleResizeAnchor = anchor;
    g_pendingScaleResizeAnchorRatioX = ClampAnchorRatio(ratioX);
    g_pendingScaleResizeAnchorRatioY = ClampAnchorRatio(ratioY);
    g_pendingScaleResizeAnchorPostScale = FALSE;
    g_pendingScaleResizeAnchorUntilTick = 0;
}

BOOL GetPendingScaleResizeAnchor(HWND hwnd, POINT* anchor) {
    if (g_pendingScaleResizeAnchorPostScale &&
        !IsPostScaleResizeAnchorActive(hwnd)) {
        ForceClearPendingScaleResizeAnchor();
    }

    if (!anchor ||
        !g_pendingScaleResizeAnchorValid ||
        g_pendingScaleResizeAnchorHwnd != hwnd) {
        return FALSE;
    }

    *anchor = g_pendingScaleResizeAnchor;
    return TRUE;
}

BOOL GetPendingScaleResizeAnchorInfo(HWND hwnd, POINT* anchor, double* ratioX, double* ratioY) {
    if (!GetPendingScaleResizeAnchor(hwnd, anchor)) {
        return FALSE;
    }

    if (ratioX) *ratioX = g_pendingScaleResizeAnchorRatioX;
    if (ratioY) *ratioY = g_pendingScaleResizeAnchorRatioY;
    return TRUE;
}

BOOL IsScaleWindowGestureActive(HWND hwnd) {
    return CLOCK_EDIT_MODE &&
           g_scaleApplyTimer != 0 &&
           g_scaleApplyTimerHwnd == hwnd &&
           IsValidDragScaleWindow(hwnd);
}

DWORD GetScaleWindowGestureSerial(HWND hwnd) {
    if (!IsScaleWindowGestureActive(hwnd)) {
        return 0;
    }

    return g_scaleGestureSerial;
}

void ClearPendingScaleResizeAnchor(HWND hwnd) {
    if (g_scaleApplyTimer != 0 &&
        (!hwnd || g_scaleApplyTimerHwnd == hwnd)) {
        return;
    }

    if (!g_pendingScaleResizeAnchorValid ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return;
    }

    if (IsPostScaleResizeAnchorActive(hwnd)) {
        return;
    }

    ForceClearPendingScaleResizeAnchor();
}

void ConsumePendingScaleResizeAnchor(HWND hwnd) {
    if (!g_pendingScaleResizeAnchorValid ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return;
    }

    ForceClearPendingScaleResizeAnchor();
}

static void ForceClearPendingScaleResizeAnchor(void) {
    g_pendingScaleResizeAnchorValid = FALSE;
    g_pendingScaleResizeAnchorHwnd = NULL;
    g_pendingScaleResizeAnchor.x = 0;
    g_pendingScaleResizeAnchor.y = 0;
    g_pendingScaleResizeAnchorRatioX = 0.5;
    g_pendingScaleResizeAnchorRatioY = 0.5;
    g_pendingScaleResizeAnchorPostScale = FALSE;
    g_pendingScaleResizeAnchorUntilTick = 0;
}

static BOOL IsPostScaleResizeAnchorActive(HWND hwnd) {
    if (!g_pendingScaleResizeAnchorValid ||
        !g_pendingScaleResizeAnchorPostScale ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return FALSE;
    }

    if ((LONG)(GetTickCount() - g_pendingScaleResizeAnchorUntilTick) < 0) {
        return TRUE;
    }

    return FALSE;
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

    if (!IsLeftButtonPhysicallyDown()) {
        return;
    }

    if (IsDragBlockedUntilLeftUp()) {
        return;
    }

    if (IsDragSuppressedAfterScale()) {
        return;
    }

    SetCapture(hwnd);
    if (GetCapture() != hwnd) {
        LOG_WARNING("Failed to capture mouse for edit-mode drag");
        return;
    }

    CLOCK_IS_DRAGGING = TRUE;
    GetCursorPos(&CLOCK_LAST_MOUSE_POS);
}

BOOL TryStartDragWindowFromMouseMove(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || CLOCK_IS_DRAGGING) return FALSE;
    if (!IsLeftButtonPhysicallyDown()) {
        IsDragBlockedUntilLeftUp();
        return FALSE;
    }
    if (GetCapture() && GetCapture() != hwnd) return FALSE;
    if (IsDragBlockedUntilLeftUp()) return FALSE;
    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) return FALSE;

    StartDragWindow(hwnd);
    return CLOCK_IS_DRAGGING;
}

void StartEditMode(HWND hwnd) {
    EnsureWindowVisibleWithTopmostState(hwnd);
    StopScaleApplyTimer(hwnd);
    ClearDragBlockUntilLeftUp();

    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;
    
    if (!CLOCK_WINDOW_TOPMOST) {
        g_editModeForcedTopmost = TRUE;
        SetWindowTopmostTransient(hwnd, TRUE);
    }
    
    CLOCK_EDIT_MODE = TRUE;
    
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
    ApplyPendingScaleTarget(hwnd);
    StopScaleApplyTimer(hwnd);
    ClearPendingScaleResizeAnchor(hwnd);
    ClearDragBlockUntilLeftUp();

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
    if (!IsLeftButtonPhysicallyDown()) {
        ClearDragBlockUntilLeftUp();
    }
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

static void CancelDragForScale(HWND hwnd) {
    BlockDragUntilLeftUp(hwnd);
    if (!CLOCK_IS_DRAGGING) return;

    CLOCK_IS_DRAGGING = FALSE;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

/* SWP_NOREDRAW + UpdateWindow maintains smooth dragging */
BOOL HandleDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return FALSE;

    if (IsDragBlockedUntilLeftUp()) {
        CLOCK_IS_DRAGGING = FALSE;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        return FALSE;
    }

    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        CLOCK_IS_DRAGGING = FALSE;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        return FALSE;
    }

    if (!IsLeftButtonPhysicallyDown()) {
        CLOCK_IS_DRAGGING = FALSE;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        ScheduleConfigSave(hwnd);
        return FALSE;
    }

    if (GetCapture() != hwnd) {
        CLOCK_IS_DRAGGING = FALSE;
        ScheduleConfigSave(hwnd);
        return FALSE;
    }
    
    POINT currentPos;
    GetCursorPos(&currentPos);
    int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    if (deltaX == 0 && deltaY == 0) {
        return TRUE;
    }
    
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

    return TRUE;
}

/* Mouse wheel scaling: configurable step per notch, anchored at the cursor */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (!CLOCK_EDIT_MODE) {
        StopScaleApplyTimer(hwnd);
        return FALSE;
    }

    SuppressDragAfterScale();
    BlockDragUntilLeftUp(hwnd);
    CancelDragForScale(hwnd);
    
    BOOL isPluginMode = PluginData_IsActive();
    float oldScale = isPluginMode ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
    if (oldScale <= 0.0f) {
        return FALSE;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int oldWidth = windowRect.right - windowRect.left;
    int oldHeight = windowRect.bottom - windowRect.top;
    if (oldWidth <= 0 || oldHeight <= 0) {
        return FALSE;
    }

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
    float baseScale = (g_scaleTargetValid && g_scaleTargetPluginMode == isPluginMode)
        ? g_scaleTarget
        : oldScale;
    double scaleDelta = CalculateWheelScaleDelta(delta, stepPercent, baseScale);
    if (scaleDelta == 0.0) {
        return FALSE;
    }

    float newScale = ClampScaleFactor((double)baseScale + scaleDelta);

    if (newScale == oldScale && !g_scaleTargetValid) {
        return FALSE;
    }

    BOOL hadActiveTimer = g_scaleApplyTimer != 0 && g_scaleApplyTimerHwnd == hwnd;
    BOOL shouldStartGesture =
        !hadActiveTimer || !g_scaleTargetValid || g_scaleTargetPluginMode != isPluginMode;

    if (!EnsureScaleApplyTimer(hwnd)) {
        if (shouldStartGesture) {
            g_scaleGestureAnchorRatioX =
                ClampAnchorRatio((double)(cursorPos.x - windowRect.left) / (double)oldWidth);
            g_scaleGestureAnchorRatioY =
                ClampAnchorRatio((double)(cursorPos.y - windowRect.top) / (double)oldHeight);
        }
        BOOL applied = ApplyScaleToWindow(hwnd, isPluginMode, newScale, cursorPos);
        if (applied) {
            ScheduleConfigSave(hwnd);
        }
        return applied;
    }

    if (shouldStartGesture) {
        g_scaleGestureAnchorRatioX =
            ClampAnchorRatio((double)(cursorPos.x - windowRect.left) / (double)oldWidth);
        g_scaleGestureAnchorRatioY =
            ClampAnchorRatio((double)(cursorPos.y - windowRect.top) / (double)oldHeight);
        if (hadActiveTimer) {
            AdvanceScaleGestureSerial();
        }
    }

    g_scaleTargetValid = TRUE;
    g_scaleTargetPluginMode = isPluginMode;
    g_scaleTarget = newScale;
    g_scaleTargetAnchor = cursorPos;
    g_lastScaleWheelTick = GetTickCount();
    ScheduleConfigSave(hwnd);

    if (!hadActiveTimer) {
        ApplyPendingScaleTarget(hwnd);
    }

    return TRUE;
}
