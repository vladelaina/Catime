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
