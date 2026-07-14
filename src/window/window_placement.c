#include "window/window_placement.h"

#include <limits.h>
#include <stdlib.h>

#define TASKBAR_AXIS_RATIO_SCALE 1000000

typedef enum TaskbarEdge {
    TASKBAR_EDGE_TOP,
    TASKBAR_EDGE_BOTTOM,
    TASKBAR_EDGE_LEFT,
    TASKBAR_EDGE_RIGHT
} TaskbarEdge;

static int ClampInt64ToInt(long long value) {
    if (value < INT_MIN) return INT_MIN;
    if (value > INT_MAX) return INT_MAX;
    return (int)value;
}

static int AddIntsClamped(int first, int second) {
    return ClampInt64ToInt((long long)first + second);
}

static BOOL IsValidRect(const RECT* rect) {
    return rect && rect->right > rect->left && rect->bottom > rect->top;
}

static TaskbarEdge GetTaskbarEdge(const RECT* taskbarRect,
                                  const RECT* monitorRect) {
    int width = taskbarRect->right - taskbarRect->left;
    int height = taskbarRect->bottom - taskbarRect->top;
    if (width >= height) {
        long long topDistance =
            llabs((long long)taskbarRect->top - monitorRect->top);
        long long bottomDistance =
            llabs((long long)monitorRect->bottom - taskbarRect->bottom);
        return topDistance <= bottomDistance
            ? TASKBAR_EDGE_TOP : TASKBAR_EDGE_BOTTOM;
    }

    long long leftDistance =
        llabs((long long)taskbarRect->left - monitorRect->left);
    long long rightDistance =
        llabs((long long)monitorRect->right - taskbarRect->right);
    return leftDistance <= rightDistance
        ? TASKBAR_EDGE_LEFT : TASKBAR_EDGE_RIGHT;
}

BOOL WindowPlacement_CaptureTaskbarAnchor(const RECT* windowRect,
                                          const RECT* taskbarRect,
                                          const RECT* monitorRect,
                                          int* axisRatio,
                                          int* crossCenterOffset) {
    if (!IsValidRect(windowRect) || !IsValidRect(taskbarRect) ||
        !IsValidRect(monitorRect) || !axisRatio || !crossCenterOffset) {
        return FALSE;
    }

    TaskbarEdge edge = GetTaskbarEdge(taskbarRect, monitorRect);
    BOOL horizontal = edge == TASKBAR_EDGE_TOP || edge == TASKBAR_EDGE_BOTTOM;
    int axisStart = horizontal ? taskbarRect->left : taskbarRect->top;
    int axisLength = horizontal
        ? taskbarRect->right - taskbarRect->left
        : taskbarRect->bottom - taskbarRect->top;
    if (axisLength <= 0) return FALSE;

    int windowCenterX = windowRect->left +
        (windowRect->right - windowRect->left) / 2;
    int windowCenterY = windowRect->top +
        (windowRect->bottom - windowRect->top) / 2;
    int windowAxisCenter = horizontal ? windowCenterX : windowCenterY;
    long long ratio = ((long long)(windowAxisCenter - axisStart) *
                       TASKBAR_AXIS_RATIO_SCALE) / axisLength;
    if (ratio < 0) ratio = 0;
    if (ratio > TASKBAR_AXIS_RATIO_SCALE) ratio = TASKBAR_AXIS_RATIO_SCALE;
    *axisRatio = (int)ratio;

    switch (edge) {
        case TASKBAR_EDGE_BOTTOM:
            *crossCenterOffset = windowCenterY - taskbarRect->top;
            break;
        case TASKBAR_EDGE_TOP:
            *crossCenterOffset = taskbarRect->bottom - windowCenterY;
            break;
        case TASKBAR_EDGE_RIGHT:
            *crossCenterOffset = windowCenterX - taskbarRect->left;
            break;
        case TASKBAR_EDGE_LEFT:
            *crossCenterOffset = taskbarRect->right - windowCenterX;
            break;
    }
    return TRUE;
}

BOOL WindowPlacement_ResolveTaskbarAnchor(const RECT* taskbarRect,
                                          const RECT* monitorRect,
                                          int windowWidth,
                                          int windowHeight,
                                          int axisRatio,
                                          int crossCenterOffset,
                                          int* outX,
                                          int* outY) {
    if (!IsValidRect(taskbarRect) || !IsValidRect(monitorRect) ||
        windowWidth <= 0 || windowHeight <= 0 || !outX || !outY) {
        return FALSE;
    }

    if (axisRatio < 0) axisRatio = 0;
    if (axisRatio > TASKBAR_AXIS_RATIO_SCALE) {
        axisRatio = TASKBAR_AXIS_RATIO_SCALE;
    }

    TaskbarEdge edge = GetTaskbarEdge(taskbarRect, monitorRect);
    BOOL horizontal = edge == TASKBAR_EDGE_TOP || edge == TASKBAR_EDGE_BOTTOM;
    int axisStart = horizontal ? taskbarRect->left : taskbarRect->top;
    int axisLength = horizontal
        ? taskbarRect->right - taskbarRect->left
        : taskbarRect->bottom - taskbarRect->top;
    int axisCenter = AddIntsClamped(
        axisStart,
        (int)(((long long)axisLength * axisRatio) /
              TASKBAR_AXIS_RATIO_SCALE));

    if (horizontal) {
        *outX = ClampInt64ToInt((long long)axisCenter - windowWidth / 2);
        int windowCenterY = edge == TASKBAR_EDGE_BOTTOM
            ? AddIntsClamped(taskbarRect->top, crossCenterOffset)
            : ClampInt64ToInt((long long)taskbarRect->bottom -
                              crossCenterOffset);
        *outY = ClampInt64ToInt((long long)windowCenterY - windowHeight / 2);
    } else {
        int windowCenterX = edge == TASKBAR_EDGE_RIGHT
            ? AddIntsClamped(taskbarRect->left, crossCenterOffset)
            : ClampInt64ToInt((long long)taskbarRect->right -
                              crossCenterOffset);
        *outX = ClampInt64ToInt((long long)windowCenterX - windowWidth / 2);
        *outY = ClampInt64ToInt((long long)axisCenter - windowHeight / 2);
    }
    return TRUE;
}
