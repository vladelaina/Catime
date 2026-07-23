#include "window_core_internal.h"

BOOL WindowCore_ScheduleDisplayRestoreTimer(HWND hwnd, UINT delayMs) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    if (!SetTimer(hwnd, TIMER_ID_DISPLAY_RESTORE, delayMs, NULL)) {
        LOG_WARNING(
            "Failed to schedule display restore timer (delay=%u, error=%lu)",
            delayMs, GetLastError());
        return FALSE;
    }
    return TRUE;
}

BOOL BeginSystemPositionChangeGuard(HWND hwnd) {
    g_systemPositionGuardUntil = GetTickCount() + SYSTEM_POSITION_GUARD_MS;
    g_pendingSystemPositionRestore = TRUE;
    return WindowCore_ScheduleDisplayRestoreTimer(
        hwnd, DISPLAY_RESTORE_DELAY_MS);
}

BOOL IsSystemPositionChangeGuardActive(void) {
    if (g_systemPositionGuardUntil == 0) return FALSE;
    if ((LONG)(GetTickCount() - g_systemPositionGuardUntil) < 0) return TRUE;
    g_systemPositionGuardUntil = 0;
    return FALSE;
}

void RestoreWindowPositionAfterSystemChange(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    if (CLOCK_EDIT_MODE) {
        ClearPendingSystemPositionRestore();
        return;
    }
    if (WindowCore_IsFullscreenForegroundWindowActive(hwnd)) {
        g_systemPositionGuardUntil = GetTickCount() + SYSTEM_POSITION_GUARD_MS;
        g_pendingSystemPositionRestore = TRUE;
        if (!g_displayRestoreDeferredForFullscreen) {
            LOG_INFO(
                "Deferring window position restore while fullscreen foreground window is active");
            g_displayRestoreDeferredForFullscreen = TRUE;
        }
        WindowCore_ScheduleDisplayRestoreTimer(
            hwnd, FULLSCREEN_RESTORE_RETRY_MS);
        return;
    }
    if (g_displayRestoreDeferredForFullscreen) {
        LOG_INFO(
            "Resuming deferred window position restore after fullscreen foreground window ended");
        g_displayRestoreDeferredForFullscreen = FALSE;
    }
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = CLOCK_WINDOW_POS_X;
    int y = CLOCK_WINDOW_POS_Y;
    ResolveConfiguredWindowPosition(width, height, &x, &y);
    if (x != rect.left || y != rect.top) {
        SetWindowPos(hwnd, NULL, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        LOG_INFO(
            "Restored window position after system display change: (%d, %d)",
            x, y);
    }
    CLOCK_WINDOW_POS_X = x;
    CLOCK_WINDOW_POS_Y = y;
    g_pendingSystemPositionRestore = g_placementRetryNeeded;
    if (g_pendingSystemPositionRestore) {
        WindowCore_ScheduleDisplayRestoreTimer(
            hwnd, DISPLAY_RESTORE_DELAY_MS);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

void TryRestorePendingWindowPosition(HWND hwnd) {
    if (g_pendingSystemPositionRestore) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
}

void ClearPendingSystemPositionRestore(void) {
    g_pendingSystemPositionRestore = FALSE;
    g_displayRestoreDeferredForFullscreen = FALSE;
    g_systemPositionGuardUntil = 0;
    g_positionTemporarilyRelocatedForDisplay = FALSE;
    g_placementRetryNeeded = FALSE;
}
