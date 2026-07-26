/**
 * @file drag_scale_anchor.c
 * @brief Mouse-anchor state carried across scale-triggered resizes.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include <math.h>

double ClampAnchorRatio(double ratio) {
    if (!isfinite(ratio)) {
        return 0.5;
    }
    if (ratio < 0.0) {
        return 0.0;
    }
    if (ratio > 1.0) {
        return 1.0;
    }
    return ratio;
}

void AdvanceScaleGestureSerial(void) {
    g_scaleGestureSerial++;
    if (g_scaleGestureSerial == 0) {
        g_scaleGestureSerial = 1;
    }
}

void SetPendingScaleResizeAnchorWithRatio(HWND hwnd, POINT anchor,
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

BOOL GetPendingScaleResizeAnchorInfo(HWND hwnd, POINT* anchor,
                                     double* ratioX, double* ratioY) {
    if (!GetPendingScaleResizeAnchor(hwnd, anchor)) {
        return FALSE;
    }

    if (ratioX) {
        *ratioX = g_pendingScaleResizeAnchorRatioX;
    }
    if (ratioY) {
        *ratioY = g_pendingScaleResizeAnchorRatioY;
    }
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

void ForceClearPendingScaleResizeAnchor(void) {
    g_pendingScaleResizeAnchorValid = FALSE;
    g_pendingScaleResizeAnchorHwnd = NULL;
    g_pendingScaleResizeAnchor.x = 0;
    g_pendingScaleResizeAnchor.y = 0;
    g_pendingScaleResizeAnchorRatioX = 0.5;
    g_pendingScaleResizeAnchorRatioY = 0.5;
    g_pendingScaleResizeAnchorPostScale = FALSE;
    g_pendingScaleResizeAnchorUntilTick = 0;
}

BOOL IsPostScaleResizeAnchorActive(HWND hwnd) {
    if (!g_pendingScaleResizeAnchorValid ||
        !g_pendingScaleResizeAnchorPostScale ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return FALSE;
    }

    return (LONG)(GetTickCount() - g_pendingScaleResizeAnchorUntilTick) < 0;
}
