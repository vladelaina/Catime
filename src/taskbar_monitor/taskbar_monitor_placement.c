/**
 * @file taskbar_monitor_placement.c
 * @brief Pure host-selection and taskbar placement calculations.
 */

#include "taskbar_monitor_internal.h"

static int MinInt(int first, int second) {
    return first < second ? first : second;
}

static int ClampInt(int value, int minimum, int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static BOOL RectHasArea(const RECT* rect) {
    return rect && rect->right > rect->left &&
           rect->bottom > rect->top;
}

BOOL TaskbarMonitor_ShouldUseClassicPlacement(
    BOOL modernTaskbar, BOOL classicHostAvailable,
    BOOL taskListAvailable) {
    return !modernTaskbar && classicHostAvailable &&
           taskListAvailable;
}

BOOL TaskbarMonitor_IsWindows11OrLaterVersion(
    DWORD majorVersion, DWORD buildNumber) {
    return majorVersion > 10 ||
           (majorVersion == 10 && buildNumber >= 22000);
}

BOOL TaskbarMonitor_CalculateClassicPlacement(
    const RECT* taskListRect, BOOL horizontal,
    int monitorWidth, int monitorHeight, int gap,
    int minimumTaskList, RECT* reservedTaskList,
    RECT* monitorRect) {
    if (!RectHasArea(taskListRect) || !reservedTaskList ||
        !monitorRect || monitorWidth <= 0 || monitorHeight <= 0 ||
        gap < 0 || minimumTaskList < 0) {
        return FALSE;
    }

    int taskListWidth = taskListRect->right - taskListRect->left;
    int taskListHeight = taskListRect->bottom - taskListRect->top;
    RECT reserved = *taskListRect;
    RECT monitor = {0};
    if (horizontal) {
        monitorHeight = MinInt(monitorHeight, taskListHeight);
        if ((LONGLONG)taskListWidth - monitorWidth - gap <
            minimumTaskList) {
            return FALSE;
        }
        reserved.right -= monitorWidth + gap;
        monitor.left = reserved.right + gap;
        monitor.top = taskListRect->top +
            (taskListHeight - monitorHeight) / 2;
        monitor.right = monitor.left + monitorWidth;
        monitor.bottom = monitor.top + monitorHeight;
    } else {
        monitorWidth = MinInt(monitorWidth, taskListWidth);
        if ((LONGLONG)taskListHeight - monitorHeight - gap <
            minimumTaskList) {
            return FALSE;
        }
        reserved.bottom -= monitorHeight + gap;
        monitor.left = taskListRect->left +
            (taskListWidth - monitorWidth) / 2;
        monitor.top = reserved.bottom + gap;
        monitor.right = monitor.left + monitorWidth;
        monitor.bottom = monitor.top + monitorHeight;
    }
    *reservedTaskList = reserved;
    *monitorRect = monitor;
    return TRUE;
}

static BOOL HasHorizontalNotificationAnchor(
    const RECT* bounds, const RECT* notificationArea) {
    return RectHasArea(notificationArea) &&
           notificationArea->left > bounds->left &&
           notificationArea->left <= bounds->right &&
           notificationArea->bottom > bounds->top &&
           notificationArea->top < bounds->bottom;
}

static BOOL HasVerticalNotificationAnchor(
    const RECT* bounds, const RECT* notificationArea) {
    return RectHasArea(notificationArea) &&
           notificationArea->top > bounds->top &&
           notificationArea->top <= bounds->bottom &&
           notificationArea->right > bounds->left &&
           notificationArea->left < bounds->right;
}

BOOL TaskbarMonitor_CalculateModernPlacement(
    const RECT* taskbarBounds, const RECT* notificationArea,
    BOOL hasNotificationArea, BOOL horizontal,
    int monitorWidth, int monitorHeight, int gap,
    int fallbackInset, RECT* monitorRect) {
    if (!RectHasArea(taskbarBounds) || !monitorRect ||
        monitorWidth <= 0 || monitorHeight <= 0 ||
        gap < 0 || fallbackInset < 0) {
        return FALSE;
    }

    int boundsWidth = taskbarBounds->right - taskbarBounds->left;
    int boundsHeight = taskbarBounds->bottom - taskbarBounds->top;
    RECT monitor = {0};
    if (horizontal) {
        BOOL useNotification = hasNotificationArea &&
            HasHorizontalNotificationAnchor(
                taskbarBounds, notificationArea);
        int anchor = useNotification ? notificationArea->left
            : taskbarBounds->right - fallbackInset;
        int minimumAnchor = taskbarBounds->left + gap + 1;
        anchor = ClampInt(
            anchor, minimumAnchor, taskbarBounds->right);
        monitor.right = anchor - gap;
        int availableWidth = monitor.right - taskbarBounds->left - gap;
        if (availableWidth <= 0) return FALSE;
        monitorWidth = MinInt(monitorWidth, availableWidth);
        monitorHeight = MinInt(monitorHeight, boundsHeight);
        monitor.left = monitor.right - monitorWidth;
        monitor.top = taskbarBounds->top +
            (boundsHeight - monitorHeight) / 2;
        monitor.bottom = monitor.top + monitorHeight;
    } else {
        BOOL useNotification = hasNotificationArea &&
            HasVerticalNotificationAnchor(
                taskbarBounds, notificationArea);
        int anchor = useNotification ? notificationArea->top
            : taskbarBounds->bottom - fallbackInset;
        int minimumAnchor = taskbarBounds->top + gap + 1;
        anchor = ClampInt(
            anchor, minimumAnchor, taskbarBounds->bottom);
        monitor.bottom = anchor - gap;
        int availableHeight = monitor.bottom - taskbarBounds->top - gap;
        if (availableHeight <= 0) return FALSE;
        monitorHeight = MinInt(monitorHeight, availableHeight);
        monitorWidth = MinInt(monitorWidth, boundsWidth);
        monitor.left = taskbarBounds->left +
            (boundsWidth - monitorWidth) / 2;
        monitor.top = monitor.bottom - monitorHeight;
        monitor.right = monitor.left + monitorWidth;
    }
    *monitorRect = monitor;
    return TRUE;
}
