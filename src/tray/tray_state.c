/**
 * @file tray_state.c
 * @brief Tray window validation, active-state checks, and retry scheduling.
 */

#include "tray_internal.h"
#include "log.h"
#include "../../resource/resource.h"

void CancelTrayRecreateRetry(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    }
    g_trayRecreateRetryCount = 0;
    g_lastTrayRecreateRetryTick = 0;
    g_trayRecreateRetryLimitLogged = FALSE;
}

void ScheduleTrayRecreateRetry(HWND hwnd) {
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd) ||
        IsTrayIconActiveForWindow(hwnd)) {
        return;
    }

    DWORD now = GetTickCount();
    if (g_trayRecreateRetryCount >= TRAY_RECREATE_RETRY_MAX_ATTEMPTS &&
        g_lastTrayRecreateRetryTick != 0 &&
        (DWORD)(now - g_lastTrayRecreateRetryTick) >=
            TRAY_RECREATE_RETRY_RESET_MS) {
        g_trayRecreateRetryCount = 0;
        g_trayRecreateRetryLimitLogged = FALSE;
    }
    if (g_trayRecreateRetryCount >= TRAY_RECREATE_RETRY_MAX_ATTEMPTS) {
        if (!g_trayRecreateRetryLimitLogged) {
            LOG_WARNING("Tray icon recreation retry limit reached");
            g_trayRecreateRetryLimitLogged = TRUE;
        }
        return;
    }

    if (!SetTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID,
                  TRAY_RECREATE_RETRY_DELAY_MS,
                  TrayRecreateRetryTimerProc)) {
        LOG_WARNING("Failed to schedule tray icon recreation retry (error=%lu)",
                    GetLastError());
        return;
    }
    g_trayRecreateRetryCount++;
    g_lastTrayRecreateRetryTick = now ? now : 1u;
}

BOOL IsValidTrayMainWindow(HWND hwnd) {
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

HWND GetValidTrayMainWindow(void) {
    HWND hwnd = g_mainHwnd;
    if (!IsValidTrayMainWindow(hwnd)) {
        g_mainHwnd = NULL;
        return NULL;
    }
    return hwnd;
}

BOOL IsTrayIconActiveForWindow(HWND hwnd) {
    return IsValidTrayMainWindow(hwnd) && g_trayIconActive &&
           nid.hWnd == hwnd && nid.uID == CLOCK_ID_TRAY_APP_ICON;
}

BOOL IsTrayIconActive(HWND hwnd) {
    return IsTrayIconActiveForWindow(hwnd);
}
