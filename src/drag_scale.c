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
#include "window/window_placement.h"
#include "window_procedure/ole_drop_target.h"
#include "drawing/drawing_render.h"

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
static UINT g_scaleApplyIntervalMs = 0;
static BOOL g_scaleTargetValid = FALSE;
static BOOL g_scaleTargetPluginMode = FALSE;
static float g_scaleTarget = 1.0f;
static POINT g_scaleTargetAnchor = {0};
static double g_scaleGestureAnchorRatioX = 0.5;
static double g_scaleGestureAnchorRatioY = 0.5;
static DWORD g_scaleGestureSerial = 0;
static DWORD g_lastScaleWheelTick = 0;
static DWORD g_lastScaleApplyTick = 0;
static DWORD g_suppressDragUntilTick = 0;
static BOOL g_dragBlockedUntilLeftUp = FALSE;
static BOOL g_dragBlockNeedsReleaseCooldown = FALSE;
static BOOL g_dragAnchorValid = FALSE;
static POINT g_dragStartCursorPos = {0};
static RECT g_dragStartWindowRect = {0};
static UINT_PTR g_dragApplyTimer = 0;
static HWND g_dragApplyTimerHwnd = NULL;
static BOOL g_pendingScaleResizeAnchorPostScale = FALSE;
static DWORD g_pendingScaleResizeAnchorUntilTick = 0;
static BOOL g_manualEditPositionValid = FALSE;
static HWND g_manualEditPositionHwnd = NULL;
static POINT g_manualEditPosition = {0};

static UINT_PTR g_configSaveTimer = 0;
static HWND g_configSaveTimerHwnd = NULL;

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define SCALE_APPLY_TIMER_ID 42426
/* Preserve 60 FPS for normal clocks, but bound full-DIB work for large
 * Markdown/image windows where UpdateLayeredWindow dominates frame cost. */
#define SCALE_APPLY_INTERVAL_MS 16u
#define SCALE_APPLY_INTERVAL_MEDIUM_MS 24u
#define SCALE_APPLY_INTERVAL_LARGE_MS 33u
#define SCALE_APPLY_INTERVAL_HUGE_MS 42u
#define SCALE_MEDIUM_WINDOW_PIXELS 500000ull
#define SCALE_LARGE_WINDOW_PIXELS 2000000ull
#define SCALE_HUGE_WINDOW_PIXELS 6000000ull
#define SCALE_APPLY_IDLE_STOP_MS 80u
#define SCALE_INITIAL_RESPONSE_MS 8u
#define SCALE_SMOOTH_RESPONSE_MS 28.0
#define SCALE_MAX_BLEND_PER_FRAME 0.52
#define SCALE_FRAME_DELTA_MAX_MS 48u
#define SCALE_SETTLE_ABS_EPSILON 0.0005f
#define SCALE_SETTLE_REL_EPSILON 0.0002f
#define SCALE_DRAG_SUPPRESS_MS 120u
#define SCALE_DRAG_RELEASE_SUPPRESS_MS 120u
#define SCALE_POST_RESIZE_ANCHOR_MS 1200u
#define EDIT_DRAG_APPLY_TIMER_ID 42428
#define EDIT_DRAG_APPLY_INTERVAL_MS 8u
static DWORD g_lastDragApplyTick = 0;

static void SetPendingScaleResizeAnchorWithRatio(HWND hwnd, POINT anchor,
                                                 double ratioX, double ratioY);
static void ForceClearPendingScaleResizeAnchor(void);
static BOOL IsPostScaleResizeAnchorActive(HWND hwnd);
static BOOL IsLeftButtonPhysicallyDown(void);
static BOOL IsValidDragScaleWindow(HWND hwnd);
static VOID CALLBACK ScaleApplyTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR idEvent, DWORD dwTime);

static DWORD TickElapsedMs(DWORD now, DWORD then) {
    return (DWORD)(now - then);
}

static UINT GetScaleApplyInterval(HWND hwnd) {
    RECT rect = {0};
    if (!IsValidDragScaleWindow(hwnd) || !GetClientRect(hwnd, &rect)) {
        return SCALE_APPLY_INTERVAL_MS;
    }

    LONG width = rect.right - rect.left;
    LONG height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return SCALE_APPLY_INTERVAL_MS;
    }

    ULONGLONG pixels = (ULONGLONG)(ULONG)width * (ULONGLONG)(ULONG)height;
    if (pixels >= SCALE_HUGE_WINDOW_PIXELS) {
        return SCALE_APPLY_INTERVAL_HUGE_MS;
    }
    if (pixels >= SCALE_LARGE_WINDOW_PIXELS) {
        return SCALE_APPLY_INTERVAL_LARGE_MS;
    }
    if (pixels >= SCALE_MEDIUM_WINDOW_PIXELS) {
        return SCALE_APPLY_INTERVAL_MEDIUM_MS;
    }
    return SCALE_APPLY_INTERVAL_MS;
}

static void ClearManualEditPosition(void) {
    g_manualEditPositionValid = FALSE;
    g_manualEditPositionHwnd = NULL;
    g_manualEditPosition.x = 0;
    g_manualEditPosition.y = 0;
}

static void RecordManualEditPosition(HWND hwnd, int x, int y) {
    if (!CLOCK_EDIT_MODE || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    g_manualEditPositionValid = TRUE;
    g_manualEditPositionHwnd = hwnd;
    g_manualEditPosition.x = x;
    g_manualEditPosition.y = y;
}

void MarkManualEditWindowPosition(HWND hwnd) {
    RECT rect = {0};
    if (GetWindowRect(hwnd, &rect)) {
        RecordManualEditPosition(hwnd, rect.left, rect.top);
    }
}

static void ResetDragApplyThrottle(void) {
    g_lastDragApplyTick = 0;
}

static BOOL ShouldApplyDragMoveNow(DWORD now) {
    if (g_lastDragApplyTick == 0) {
        return TRUE;
    }

    return TickElapsedMs(now, g_lastDragApplyTick) >= EDIT_DRAG_APPLY_INTERVAL_MS;
}

static BOOL ApplyDragPositionForCursor(HWND hwnd, POINT cursorPos) {
    if (!g_dragAnchorValid) {
        return FALSE;
    }

    int newX = g_dragStartWindowRect.left + (cursorPos.x - g_dragStartCursorPos.x);
    int newY = g_dragStartWindowRect.top + (cursorPos.y - g_dragStartCursorPos.y);

    RECT beforeRect = {0};
    BOOL alreadyAtTarget = GetWindowRect(hwnd, &beforeRect) &&
                           beforeRect.left == newX &&
                           beforeRect.top == newY;

    if (alreadyAtTarget) {
        CLOCK_LAST_MOUSE_POS = cursorPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        RecordManualEditPosition(hwnd, newX, newY);
        return TRUE;
    }

    BOOL moved = SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (moved) {
        CLOCK_LAST_MOUSE_POS = cursorPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        g_lastDragApplyTick = GetTickCount();
        RecordManualEditPosition(hwnd, newX, newY);
        return TRUE;
    }

    return FALSE;
}

static void StopDragApplyTimer(HWND hwnd) {
    if (g_dragApplyTimer == 0) {
        return;
    }

    HWND timerHwnd = g_dragApplyTimerHwnd ? g_dragApplyTimerHwnd : hwnd;
    if ((!hwnd || timerHwnd == hwnd) && timerHwnd && IsWindow(timerHwnd)) {
        KillTimer(timerHwnd, EDIT_DRAG_APPLY_TIMER_ID);
    }

    if (!hwnd || timerHwnd == hwnd) {
        g_dragApplyTimer = 0;
        g_dragApplyTimerHwnd = NULL;
    }
}

static VOID CALLBACK DragApplyTimerProc(HWND hwnd,
                                        UINT msg,
                                        UINT_PTR idEvent,
                                        DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != EDIT_DRAG_APPLY_TIMER_ID ||
        hwnd != g_dragApplyTimerHwnd) {
        return;
    }

    StopDragApplyTimer(hwnd);

    if (!CLOCK_EDIT_MODE ||
        !CLOCK_IS_DRAGGING ||
        !g_dragAnchorValid ||
        GetCapture() != hwnd ||
        !IsLeftButtonPhysicallyDown()) {
        return;
    }

    POINT cursorPos = {0};
    if (GetCursorPos(&cursorPos)) {
        ApplyDragPositionForCursor(hwnd, cursorPos);
    }
}

static BOOL EnsureDragApplyTimer(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }

    if (g_dragApplyTimer != 0 && g_dragApplyTimerHwnd == hwnd) {
        return TRUE;
    }

    StopDragApplyTimer(NULL);
    g_dragApplyTimer = SetTimer(hwnd,
                                EDIT_DRAG_APPLY_TIMER_ID,
                                EDIT_DRAG_APPLY_INTERVAL_MS,
                                (TIMERPROC)DragApplyTimerProc);
    if (!g_dragApplyTimer) {
        g_dragApplyTimerHwnd = NULL;
        return FALSE;
    }

    g_dragApplyTimerHwnd = hwnd;
    return TRUE;
}

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

static void ClearDragAnchor(void) {
    g_dragAnchorValid = FALSE;
    g_dragStartCursorPos.x = 0;
    g_dragStartCursorPos.y = 0;
    ZeroMemory(&g_dragStartWindowRect, sizeof(g_dragStartWindowRect));
}

static BOOL SetDragAnchorFromCurrentWindow(HWND hwnd, POINT cursorPos) {
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        ClearDragAnchor();
        return FALSE;
    }

    g_dragStartCursorPos = cursorPos;
    g_dragStartWindowRect = windowRect;
    g_dragAnchorValid = TRUE;
    return TRUE;
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

static void RestoreManualTopLeftAfterEditLayout(HWND hwnd,
                                                const RECT* manualRect) {
    if (!manualRect || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    RECT layoutRect = {0};
    POINT restorePosition = {0};
    if (!GetWindowRect(hwnd, &layoutRect) ||
        !WindowPlacement_GetManualTopLeftRestore(
            manualRect, &layoutRect, &restorePosition)) {
        return;
    }

    LOG_DEBUG("Restoring manual edit position after layout: (%ld, %ld) -> (%ld, %ld)",
              layoutRect.left, layoutRect.top,
              manualRect->left, manualRect->top);
    if (SetWindowPos(hwnd, NULL,
                     restorePosition.x, restorePosition.y,
                     0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        CLOCK_WINDOW_POS_X = restorePosition.x;
        CLOCK_WINDOW_POS_Y = restorePosition.y;
    }
}

static void FinishDragWindow(HWND hwnd,
                             BOOL saveSettings,
                             BOOL refreshAfterDrag,
                             BOOL applyFinalPosition) {
    if (applyFinalPosition &&
        CLOCK_IS_DRAGGING &&
        g_dragAnchorValid &&
        IsValidDragScaleWindow(hwnd)) {
        POINT finalCursor = {0};
        if (GetCursorPos(&finalCursor)) {
            ApplyDragPositionForCursor(hwnd, finalCursor);
        }
    }
    StopDragApplyTimer(hwnd);

    CLOCK_IS_DRAGGING = FALSE;
    ClearDragAnchor();
    ResetDragApplyThrottle();

    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }

    if (saveSettings && CLOCK_EDIT_MODE) {
        ScheduleConfigSave(hwnd);
    }

    if (refreshAfterDrag && IsValidDragScaleWindow(hwnd)) {
        RefreshWindow(hwnd, FALSE);
    }
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

    /* Scaling becomes the latest placement gesture and owns the next anchor. */
    ClearManualEditPosition();
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
    g_scaleApplyIntervalMs = 0;
    g_scaleTargetValid = FALSE;
    g_scaleTargetPluginMode = FALSE;
    g_scaleTarget = 1.0f;
    g_scaleTargetAnchor.x = 0;
    g_scaleTargetAnchor.y = 0;
    g_scaleGestureAnchorRatioX = 0.5;
    g_scaleGestureAnchorRatioY = 0.5;
    g_lastScaleWheelTick = 0;
    g_lastScaleApplyTick = 0;
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

static float GetScaleSettleTolerance(float targetScale) {
    float relative = fabsf(targetScale) * SCALE_SETTLE_REL_EPSILON;
    return relative > SCALE_SETTLE_ABS_EPSILON
        ? relative
        : SCALE_SETTLE_ABS_EPSILON;
}

static BOOL ApplySmoothedScaleTarget(HWND hwnd, DWORD elapsedMs) {
    if (!g_scaleTargetValid || !CLOCK_EDIT_MODE ||
        !IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }

    float currentScale = GetActiveScaleFactor(g_scaleTargetPluginMode);
    float tolerance = GetScaleSettleTolerance(g_scaleTarget);
    double remaining = (double)g_scaleTarget - (double)currentScale;
    if (fabs(remaining) <= (double)tolerance) {
        ApplyScaleToWindow(hwnd,
                           g_scaleTargetPluginMode,
                           g_scaleTarget,
                           g_scaleTargetAnchor);
        return TRUE;
    }

    if (elapsedMs == 0) elapsedMs = 1;
    if (elapsedMs > SCALE_FRAME_DELTA_MAX_MS) {
        elapsedMs = SCALE_FRAME_DELTA_MAX_MS;
    }

    double blend = 1.0 - exp(-(double)elapsedMs / SCALE_SMOOTH_RESPONSE_MS);
    if (blend > SCALE_MAX_BLEND_PER_FRAME) {
        blend = SCALE_MAX_BLEND_PER_FRAME;
    }
    double nextValue = (double)currentScale + remaining * blend;
    float nextScale = ClampScaleFactor(nextValue);
    if (fabs((double)g_scaleTarget - (double)nextScale) <=
        (double)tolerance) {
        nextScale = g_scaleTarget;
    }

    ApplyScaleToWindow(hwnd,
                       g_scaleTargetPluginMode,
                       nextScale,
                       g_scaleTargetAnchor);
    return nextScale == g_scaleTarget;
}

static void RefreshScaleApplyTimerInterval(HWND hwnd) {
    if (g_scaleApplyTimer == 0 || g_scaleApplyTimerHwnd != hwnd) {
        return;
    }

    UINT desiredInterval = GetScaleApplyInterval(hwnd);
    if (desiredInterval == g_scaleApplyIntervalMs) {
        return;
    }

    UINT_PTR updatedTimer = SetTimer(hwnd,
                                     SCALE_APPLY_TIMER_ID,
                                     desiredInterval,
                                     (TIMERPROC)ScaleApplyTimerProc);
    if (updatedTimer != 0) {
        g_scaleApplyTimer = updatedTimer;
        g_scaleApplyIntervalMs = desiredInterval;
    }
}

static VOID CALLBACK ScaleApplyTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != SCALE_APPLY_TIMER_ID ||
        hwnd != g_scaleApplyTimerHwnd) {
        return;
    }

    DWORD now = GetTickCount();
    DWORD elapsedMs = g_lastScaleApplyTick != 0
        ? TickElapsedMs(now, g_lastScaleApplyTick)
        : SCALE_APPLY_INTERVAL_MS;
    g_lastScaleApplyTick = now;
    BOOL targetReached = ApplySmoothedScaleTarget(hwnd, elapsedMs);
    if (!targetReached) {
        RefreshScaleApplyTimerInterval(hwnd);
    }
    BOOL inputIdle = TickElapsedMs(now, g_lastScaleWheelTick) >=
                     SCALE_APPLY_IDLE_STOP_MS;
    if (!g_scaleTargetValid ||
        !CLOCK_EDIT_MODE ||
        (inputIdle && targetReached)) {
        if (g_scaleTargetValid && CLOCK_EDIT_MODE &&
            inputIdle && targetReached) {
            ScheduleConfigSave(hwnd);
        }
        StopScaleApplyTimer(hwnd);
    }
}

static BOOL EnsureScaleApplyTimer(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) return FALSE;
    if (g_scaleApplyTimer != 0 && g_scaleApplyTimerHwnd == hwnd) return TRUE;

    StopScaleApplyTimer(hwnd);
    UINT interval = GetScaleApplyInterval(hwnd);
    g_scaleApplyTimer = SetTimer(hwnd,
                                 SCALE_APPLY_TIMER_ID,
                                 interval,
                                 (TIMERPROC)ScaleApplyTimerProc);
    if (!g_scaleApplyTimer) {
        return FALSE;
    }

    g_scaleApplyTimerHwnd = hwnd;
    g_scaleApplyIntervalMs = interval;
    g_lastScaleApplyTick = GetTickCount();
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

void FinalizeScaleWindowGestureForManualMove(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    ApplyPendingScaleTarget(hwnd);
    StopScaleApplyTimer(hwnd);
    ForceClearPendingScaleResizeAnchor();
}

DWORD GetScaleWindowGestureSerial(HWND hwnd) {
    if (!IsScaleWindowGestureActive(hwnd)) {
        return 0;
    }

    return g_scaleGestureSerial;
}

DWORD GetScaleWindowVisualSerial(HWND hwnd) {
    DWORD activeSerial = GetScaleWindowGestureSerial(hwnd);
    if (activeSerial != 0) {
        return activeSerial;
    }

    if (g_pendingScaleResizeAnchorValid &&
        g_pendingScaleResizeAnchorHwnd == hwnd &&
        IsPostScaleResizeAnchorActive(hwnd)) {
        return g_scaleGestureSerial;
    }

    return 0;
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

    /* This API is reached only by explicit drag/keyboard/scale gestures. */
    CLOCK_WINDOW_POSITION_MANUAL = TRUE;

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

    if (CLOCK_IS_DRAGGING) {
        return;
    }

    if (IsDragBlockedUntilLeftUp()) {
        return;
    }

    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        return;
    }

    SetCapture(hwnd);
    if (GetCapture() != hwnd) {
        LOG_WARNING("Failed to capture mouse for edit-mode drag");
        return;
    }

    POINT cursorPos;
    if (!GetCursorPos(&cursorPos) ||
        !SetDragAnchorFromCurrentWindow(hwnd, cursorPos)) {
        ReleaseCapture();
        LOG_WARNING("Failed to initialize edit-mode drag anchor");
        return;
    }

    /* A manual drag supersedes any short-lived cursor anchor left by scaling. */
    if (g_pendingScaleResizeAnchorValid) {
        LOG_DEBUG("Discarding stale scale resize anchor before manual drag");
    }
    ForceClearPendingScaleResizeAnchor();

    CLOCK_IS_DRAGGING = TRUE;
    CLOCK_LAST_MOUSE_POS = cursorPos;
    StopDragApplyTimer(hwnd);
    ResetDragApplyThrottle();
    StopDrawingRenderAnimationTimer(hwnd);
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
    ClearPendingSystemPositionRestore();
    EnsureWindowVisibleWithTopmostState(hwnd);
    StopScaleApplyTimer(hwnd);
    ClearDragBlockUntilLeftUp();
    ClearManualEditPosition();

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
    InitializeOleDropTarget(hwnd);
    RefreshWindow(hwnd, TRUE);
    
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

void EndEditMode(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;

    BOOL hadActiveDrag = CLOCK_IS_DRAGGING;
    if (hadActiveDrag) {
        /* A missed left-button-up must not snap to a later right-click point. */
        FinishDragWindow(hwnd, FALSE, FALSE, IsLeftButtonPhysicallyDown());
        /* Exiting mid-drag is still an explicit manual placement gesture. */
        CLOCK_WINDOW_POSITION_MANUAL = TRUE;
    }
    ApplyPendingScaleTarget(hwnd);
    StopScaleApplyTimer(hwnd);
    ConsumePendingScaleResizeAnchor(hwnd);
    ClearDragBlockUntilLeftUp();

    RECT manualRect = {0};
    BOOL hasManualRect = GetWindowRect(hwnd, &manualRect);
    if (hasManualRect &&
        g_manualEditPositionValid &&
        g_manualEditPositionHwnd == hwnd) {
        OffsetRect(&manualRect,
                   g_manualEditPosition.x - manualRect.left,
                   g_manualEditPosition.y - manualRect.top);
    }
    CLOCK_EDIT_MODE = FALSE;

    SetBlurBehind(hwnd, FALSE);
    SetClickThrough(hwnd, TRUE);
    CleanupOleDropTarget(hwnd);
    CancelScheduledConfigSave(hwnd);

    /* Topmost/desktop ownership changes can trigger another layout pass.
     * Complete them before the one authoritative layout and save below. */
    if (g_editModeForcedTopmost &&
        !g_editModeTopmostOverride &&
        !PREVIOUS_TOPMOST_STATE) {
        SetWindowTopmostTransient(hwnd, FALSE);
        KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
    }

    /* Finish normal-mode sizing before persisting the authoritative position. */
    RefreshWindow(hwnd, TRUE);
    UpdateWindow(hwnd);
    if (hasManualRect) {
        RestoreManualTopLeftAfterEditLayout(hwnd, &manualRect);
    }
    SaveWindowSettings(hwnd);
    ClearManualEditPosition();
    
    if (!WriteConfigColor(CLOCK_TEXT_COLOR)) {
        LOG_WARNING("EndEditMode: failed to persist text color");
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

    FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
}

static void CancelDragForScale(HWND hwnd) {
    BlockDragUntilLeftUp(hwnd);
    if (!CLOCK_IS_DRAGGING) return;

    FinishDragWindow(hwnd, FALSE, FALSE, FALSE);
}

/* Absolute cursor anchoring keeps movement aligned even when mouse messages coalesce. */
static BOOL HandleDragWindowInternal(HWND hwnd, BOOL leftButtonDown) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return FALSE;

    if (IsDragBlockedUntilLeftUp()) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }

    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        FinishDragWindow(hwnd, FALSE, FALSE, FALSE);
        return FALSE;
    }

    if (!leftButtonDown) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }

    if (GetCapture() != hwnd) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }
    
    POINT currentPos;
    if (!GetCursorPos(&currentPos)) {
        return TRUE;
    }

    if (!g_dragAnchorValid &&
        !SetDragAnchorFromCurrentWindow(hwnd, CLOCK_LAST_MOUSE_POS)) {
        return TRUE;
    }

    DWORD now = GetTickCount();
    int deltaFromLastX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaFromLastY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    if (deltaFromLastX == 0 && deltaFromLastY == 0) {
        return TRUE;
    }

    if (!ShouldApplyDragMoveNow(now)) {
        if (!EnsureDragApplyTimer(hwnd)) {
            ApplyDragPositionForCursor(hwnd, currentPos);
        }
        return TRUE;
    }

    int newX = g_dragStartWindowRect.left + (currentPos.x - g_dragStartCursorPos.x);
    int newY = g_dragStartWindowRect.top + (currentPos.y - g_dragStartCursorPos.y);

    BOOL moved = SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (moved) {
        CLOCK_LAST_MOUSE_POS = currentPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        g_lastDragApplyTick = now;
        RecordManualEditPosition(hwnd, newX, newY);
    }

    return TRUE;
}

BOOL HandleDragWindowWithButtonState(HWND hwnd, BOOL leftButtonDown) {
    return HandleDragWindowInternal(hwnd, leftButtonDown);
}

BOOL HandleDragWindow(HWND hwnd) {
    return HandleDragWindowInternal(hwnd, IsLeftButtonPhysicallyDown());
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
        g_scaleTargetAnchor = cursorPos;
        if (hadActiveTimer) {
            AdvanceScaleGestureSerial();
        }
    }

    g_scaleTargetValid = TRUE;
    g_scaleTargetPluginMode = isPluginMode;
    g_scaleTarget = newScale;
    g_lastScaleWheelTick = GetTickCount();
    ScheduleConfigSave(hwnd);

    if (!hadActiveTimer) {
        ApplySmoothedScaleTarget(hwnd, SCALE_INITIAL_RESPONSE_MS);
    }

    return TRUE;
}
