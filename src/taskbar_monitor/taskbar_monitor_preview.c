/**
 * @file taskbar_monitor_preview.c
 * @brief Temporary taskbar-monitor geometry used while a tray menu is open.
 */

#include "taskbar_monitor_internal.h"

void TaskbarMonitor_ClearMenuPreviewWindowInteraction(void) {
    (void)TaskbarMonitor_SetMenuPreviewPassThrough(FALSE);
}

void TaskbarMonitor_ResetMenuPreviewWindowGeometry(void) {
    g_taskbarMonitor.menuPreviewOriginalWindowRectValid = FALSE;
    g_taskbarMonitor.menuPreviewWindowResized = FALSE;
    g_taskbarMonitor.menuPreviewOriginalParent = NULL;
    SetRectEmpty(&g_taskbarMonitor.menuPreviewOriginalWindowRect);
    g_taskbarMonitor.menuPreviewOriginalWidth = 0;
    g_taskbarMonitor.menuPreviewOriginalHeight = 0;
}

static BOOL GetMonitorWindowRectForPreview(
    HWND parent, RECT* windowRect) {
    if (!IsWindow(g_taskbarMonitor.window) || !windowRect) return FALSE;
    if (parent) {
        if (!IsWindow(parent)) return FALSE;
        return TaskbarMonitor_GetWindowRectInParent(
            g_taskbarMonitor.window, parent, windowRect);
    }
    return GetWindowRect(g_taskbarMonitor.window, windowRect);
}

BOOL TaskbarMonitor_CaptureMenuPreviewWindowGeometry(void) {
    TaskbarMonitor_ResetMenuPreviewWindowGeometry();
    if (!TaskbarMonitor_IsWindowShown(
            g_taskbarMonitor.window)) return FALSE;
    HWND parent = IsWindow(g_taskbarMonitor.host)
        ? g_taskbarMonitor.host : GetParent(g_taskbarMonitor.window);
    RECT windowRect = {0};
    if (!GetMonitorWindowRectForPreview(parent, &windowRect) ||
        windowRect.right <= windowRect.left ||
        windowRect.bottom <= windowRect.top) {
        return FALSE;
    }
    g_taskbarMonitor.menuPreviewOriginalParent = parent;
    g_taskbarMonitor.menuPreviewOriginalWindowRect = windowRect;
    g_taskbarMonitor.menuPreviewOriginalWidth = g_taskbarMonitor.width;
    g_taskbarMonitor.menuPreviewOriginalHeight = g_taskbarMonitor.height;
    g_taskbarMonitor.menuPreviewOriginalWindowRectValid = TRUE;
    return TRUE;
}

static void ClampMenuPreviewRectToParent(RECT* windowRect) {
    HWND parent = g_taskbarMonitor.menuPreviewOriginalParent;
    RECT bounds = {0};
    if (!windowRect || !IsWindow(parent) ||
        !GetClientRect(parent, &bounds)) return;
    if (windowRect->left < bounds.left) windowRect->left = bounds.left;
    if (windowRect->top < bounds.top) windowRect->top = bounds.top;
    if (windowRect->right > bounds.right) windowRect->right = bounds.right;
    if (windowRect->bottom > bounds.bottom) windowRect->bottom = bounds.bottom;
}

static BOOL WindowRectsEqual(const RECT* first, const RECT* second) {
    return first && second &&
           first->left == second->left &&
           first->top == second->top &&
           first->right == second->right &&
           first->bottom == second->bottom;
}

static BOOL SetMenuPreviewWindowGeometry(const RECT* windowRect) {
    if (!windowRect || windowRect->right <= windowRect->left ||
        windowRect->bottom <= windowRect->top ||
        !IsWindow(g_taskbarMonitor.window)) {
        return FALSE;
    }
    RECT current = {0};
    if (!GetMonitorWindowRectForPreview(
            g_taskbarMonitor.menuPreviewOriginalParent, &current)) {
        return FALSE;
    }
    if (!WindowRectsEqual(&current, windowRect) &&
        !SetWindowPos(
            g_taskbarMonitor.window, NULL,
            windowRect->left, windowRect->top,
            windowRect->right - windowRect->left,
            windowRect->bottom - windowRect->top,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW)) {
        return FALSE;
    }
    g_taskbarMonitor.width = windowRect->right - windowRect->left;
    g_taskbarMonitor.height = windowRect->bottom - windowRect->top;
    return TRUE;
}

BOOL TaskbarMonitor_RestoreMenuPreviewWindowGeometry(void) {
    if (!g_taskbarMonitor.menuPreviewOriginalWindowRectValid) return FALSE;
    if (!SetMenuPreviewWindowGeometry(
            &g_taskbarMonitor.menuPreviewOriginalWindowRect)) {
        return FALSE;
    }
    g_taskbarMonitor.width = g_taskbarMonitor.menuPreviewOriginalWidth;
    g_taskbarMonitor.height = g_taskbarMonitor.menuPreviewOriginalHeight;
    g_taskbarMonitor.menuPreviewWindowResized = FALSE;
    return TRUE;
}

BOOL TaskbarMonitor_UpdateMenuPreviewWindowGeometry(void) {
    if (!g_taskbarMonitor.menuPreviewSessionActive ||
        !g_taskbarMonitor.menuPreviewOriginalWindowRectValid ||
        !IsWindow(g_taskbarMonitor.window)) {
        return FALSE;
    }
    int width = 0;
    int height = 0;
    if (!TaskbarMonitor_CalculateMonitorSize(
            g_taskbarMonitor.taskbarWidth,
            g_taskbarMonitor.taskbarHeight,
            g_taskbarMonitor.horizontal, g_taskbarMonitor.dpi,
            g_taskbarMonitor.networkGroupWidth,
            g_taskbarMonitor.resourceGroupWidth,
            g_taskbarMonitor.cpuMemoryEnabled,
            g_taskbarMonitor.networkEnabled, &width, &height)) {
        return FALSE;
    }
    RECT placement = {0};
    if (!TaskbarMonitor_CalculatePreviewPlacement(
            &g_taskbarMonitor.menuPreviewOriginalWindowRect,
            g_taskbarMonitor.horizontal, width, height, &placement)) {
        return FALSE;
    }
    ClampMenuPreviewRectToParent(&placement);
    if (!SetMenuPreviewWindowGeometry(&placement)) return FALSE;
    g_taskbarMonitor.menuPreviewWindowResized =
        !WindowRectsEqual(
            &placement,
            &g_taskbarMonitor.menuPreviewOriginalWindowRect) ||
        g_taskbarMonitor.width !=
            g_taskbarMonitor.menuPreviewOriginalWidth ||
        g_taskbarMonitor.height !=
            g_taskbarMonitor.menuPreviewOriginalHeight;
    return TRUE;
}
