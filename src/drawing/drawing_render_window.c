/**
 * @file drawing_render_window.c
 * @brief Window sizing and placement after measurement.
 */

#include "drawing_render_internal.h"

void AdjustWindowSize(HWND hwnd, const SIZE* textSize, RECT* rect) {
    SIZE targetSize;
    DWORD scaleGestureSerial = GetScaleWindowGestureSerial(hwnd);
    if (CLOCK_IS_DRAGGING && scaleGestureSerial == 0) {
        ClearPendingScaleResizeAnchor(hwnd);
        return;
    }

    if (!GetConstrainedRenderWindowSize(hwnd, textSize, &targetSize)) {
        ClearPendingScaleResizeAnchor(hwnd);
        return;
    }

    POINT resizeAnchor = {0};
    double resizeAnchorRatioX = 0.5;
    double resizeAnchorRatioY = 0.5;
    BOOL hasResizeAnchor = GetPendingScaleResizeAnchorInfo(hwnd,
                                                           &resizeAnchor,
                                                           &resizeAnchorRatioX,
                                                           &resizeAnchorRatioY);
    LONG currentClientWidth = rect->right - rect->left;
    LONG currentClientHeight = rect->bottom - rect->top;

    if (targetSize.cx == currentClientWidth &&
        targetSize.cy == currentClientHeight) {
        if (hasResizeAnchor && scaleGestureSerial == 0) {
            ConsumePendingScaleResizeAnchor(hwnd);
        } else {
            ClearPendingScaleResizeAnchor(hwnd);
        }
        return;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int newX = windowRect.left;
    int newY = windowRect.top;
    if (hasResizeAnchor) {
        newX = resizeAnchor.x - (int)(resizeAnchorRatioX * (double)targetSize.cx + 0.5);
        newY = resizeAnchor.y - (int)(resizeAnchorRatioY * (double)targetSize.cy + 0.5);
    } else {
        HMONITOR monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {0};
        RECT taskbarRect = {0};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor && GetMonitorInfo(monitor, &monitorInfo) &&
            GetTaskbarRectForMonitor(monitor, &taskbarRect) &&
            WindowPlacement_ShouldPreserveTaskbarAnchor(
                CLOCK_WINDOW_TASKBAR_ANCHORED,
                &windowRect, &taskbarRect)) {
            int axisRatio = 0;
            int crossOffset = 0;
            if (WindowPlacement_CaptureTaskbarAnchor(
                    &windowRect, &taskbarRect, &monitorInfo.rcMonitor,
                    &axisRatio, &crossOffset)) {
                WindowPlacement_ResolveTaskbarAnchor(
                    &taskbarRect, &monitorInfo.rcMonitor,
                    targetSize.cx, targetSize.cy,
                    axisRatio, crossOffset, &newX, &newY);
            }
        }
    }

    ClampWindowPositionToVisibleMonitor(targetSize.cx, targetSize.cy,
                                        &newX, &newY);

    SetWindowPos(hwnd, NULL,
        newX, newY,
        targetSize.cx,
        targetSize.cy,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    if (hasResizeAnchor && scaleGestureSerial == 0) {
        ConsumePendingScaleResizeAnchor(hwnd);
    } else {
        ClearPendingScaleResizeAnchor(hwnd);
    }

    CLOCK_WINDOW_POS_X = newX;
    CLOCK_WINDOW_POS_Y = newY;

    GetClientRect(hwnd, rect);
}
