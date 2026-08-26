/**
 * @file tray_state.c
 * @brief Tray window validation, active-state checks, and retry scheduling.
 */

#include "tray_internal.h"
#include "tray/tray_events.h"
#include "log.h"
#include "../../resource/resource.h"
#include <shellapi.h>

#define TRAY_MODIFY_FAILURE_LOG_INTERVAL_MS 5000u
#define TRAY_HEALTH_DIAGNOSTIC_INTERVAL_MS (15u * 60u * 1000u)

static DWORD g_lastTrayModifyFailureLogTick = 0;
static DWORD g_lastTrayHealthDiagnosticTick = 0;

void CancelTrayRecreateRetry(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    }
    g_trayRecreateRetryCount = 0;
    g_trayRecreateRetryLimitLogged = FALSE;
}

void ScheduleTrayRecreateRetry(HWND hwnd) {
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd) ||
        IsTrayIconActiveForWindow(hwnd)) {
        return;
    }

    UINT retryDelayMs = TRAY_RECREATE_RETRY_DELAY_MS;
    if (g_trayRecreateRetryCount >= TRAY_RECREATE_RETRY_MAX_ATTEMPTS) {
        if (!g_trayRecreateRetryLimitLogged) {
            LOG_WARNING(
                "Tray icon recreation entering background retry mode");
            g_trayRecreateRetryLimitLogged = TRUE;
        }
        retryDelayMs = TRAY_RECREATE_BACKGROUND_RETRY_MS;
    }

    if (!SetTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID,
                  retryDelayMs,
                  TrayRecreateRetryTimerProc)) {
        LOG_WARNING("Failed to schedule tray icon recreation retry (error=%lu)",
                    GetLastError());
        return;
    }
    if (g_trayRecreateRetryCount < TRAY_RECREATE_RETRY_MAX_ATTEMPTS) {
        g_trayRecreateRetryCount++;
    }
}

void StartTrayHealthCheck(HWND hwnd) {
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd) ||
        !IsTrayIconActiveForWindow(hwnd)) {
        return;
    }

    /* A newly registered icon starts with a clean failure budget.  Keeping
     * failures from the previous Explorer registration could cause an
     * otherwise healthy replacement to be invalidated on its first probe. */
    TrayRecoveryPolicy_RecordSuccess(&g_trayRecoveryPolicyState);
    g_lastTrayModifyFailureLogTick = 0;
    g_lastTrayHealthDiagnosticTick = GetTickCount();
    if (!SetTimer(hwnd, TRAY_HEALTH_CHECK_TIMER_ID,
                  TRAY_HEALTH_CHECK_INTERVAL_MS,
                  TrayHealthCheckTimerProc)) {
        LOG_WARNING("Failed to start tray icon health check (error=%lu)",
                    GetLastError());
    }
}

void StopTrayHealthCheck(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, TRAY_HEALTH_CHECK_TIMER_ID);
    }
    TrayRecoveryPolicy_RecordSuccess(&g_trayRecoveryPolicyState);
    g_lastTrayModifyFailureLogTick = 0;
    g_lastTrayHealthDiagnosticTick = 0;
}

void ReportTrayIconModifySuccess(HWND hwnd) {
    if (!IsTrayIconActiveForWindow(hwnd)) {
        return;
    }
    TrayRecoveryPolicy_RecordSuccess(&g_trayRecoveryPolicyState);
}

static void InvalidateTrayIconRegistration(HWND hwnd) {
    if (!IsValidTrayMainWindow(hwnd) || g_trayShuttingDown) {
        return;
    }

    if (!g_trayIconActive) {
        ScheduleTrayRecreateRetry(hwnd);
        return;
    }

    LOG_WARNING(
        "Tray Shell registration appears stale; scheduling recreation");
    g_trayIconActive = FALSE;
    g_trayCallbackRecoveryAllowed = TRUE;
    g_trayCallbackVersion = 0;
    StopTrayHealthCheck(hwnd);
    StopTrayHoverDetection();
    RefreshTrayBackgroundWorkState();
    ScheduleTrayRecreateRetry(hwnd);
}

void ReportTrayIconModifyFailure(HWND hwnd) {
    if (!IsTrayIconActiveForWindow(hwnd) || IsTrayInteractionSuspended()) {
        return;
    }

    DWORD error = GetLastError();
    DWORD now = GetTickCount();
    BOOL shouldRecover = TrayRecoveryPolicy_RecordFailure(
        &g_trayRecoveryPolicyState, now,
        TRAY_MODIFY_FAILURE_RESET_MS,
        TRAY_MODIFY_FAILURE_THRESHOLD);
    DWORD lastLog = g_lastTrayModifyFailureLogTick;
    BOOL logNow = shouldRecover || lastLog == 0 ||
                  (DWORD)(now - lastLog) >=
                      TRAY_MODIFY_FAILURE_LOG_INTERVAL_MS;
    if (logNow) {
        LOG_WARNING("Tray Shell update failed (%u/%u, error=%lu)",
                    g_trayRecoveryPolicyState.consecutiveFailures,
                    (UINT)TRAY_MODIFY_FAILURE_THRESHOLD, error);
        g_lastTrayModifyFailureLogTick = now ? now : 1u;
    }
    if (shouldRecover) {
        Tray_LogDiagnosticSnapshot("health-probe-triggered-recovery", hwnd);
        InvalidateTrayIconRegistration(hwnd);
    }
}

void CALLBACK TrayHealthCheckTimerProc(HWND hwnd, UINT msg,
                                       UINT_PTR id, DWORD time) {
    (void)time;
    if (msg != WM_TIMER || id != TRAY_HEALTH_CHECK_TIMER_ID) {
        return;
    }

    if (!IsTrayIconActiveForWindow(hwnd)) {
        LOG_WARNING("Tray health check stopped because tray identity is inactive: "
                    "hwnd=0x%p", hwnd);
        Tray_LogDiagnosticSnapshot("health-check-inactive", hwnd);
        StopTrayHealthCheck(hwnd);
        return;
    }
    if (IsTrayInteractionSuspended()) {
        return;
    }

    NOTIFYICONDATAW probe = {0};
    probe.cbSize = sizeof(probe);
    probe.hWnd = hwnd;
    probe.uID = CLOCK_ID_TRAY_APP_ICON;
    /* Reassert only the callback route; touching the tooltip here would
     * unnecessarily restart Explorer's native hover delay.  A successful
     * NIM_MODIFY both repairs stale click delivery and proves that Explorer
     * still recognizes the tray item. */
    probe.uFlags = NIF_MESSAGE;
    probe.uCallbackMessage = CLOCK_WM_TRAYICON;

    BOOL modified = Shell_NotifyIconW(NIM_MODIFY, &probe);
    DWORD error = modified ? ERROR_SUCCESS : GetLastError();
    if (modified) {
        ReportTrayIconModifySuccess(hwnd);
    } else {
        ReportTrayIconModifyFailure(hwnd);
    }
    DWORD now = GetTickCount();
    if (g_lastTrayHealthDiagnosticTick == 0 ||
        (DWORD)(now - g_lastTrayHealthDiagnosticTick) >=
            TRAY_HEALTH_DIAGNOSTIC_INTERVAL_MS) {
        g_lastTrayHealthDiagnosticTick = now ? now : 1u;
        LOG_INFO("Tray health checkpoint: modify=%d error=%lu", modified,
                 error);
        Tray_LogDiagnosticSnapshot("health-checkpoint", hwnd);
    }
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

BOOL TryRestoreTrayIconFromCallback(HWND hwnd) {
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd)) {
        return FALSE;
    }
    if (IsTrayIconActiveForWindow(hwnd)) {
        return TRUE;
    }
    if (!g_trayCallbackRecoveryAllowed ||
        nid.hWnd != hwnd || nid.uID != CLOCK_ID_TRAY_APP_ICON) {
        return FALSE;
    }

    /* A callback from Explorer is stronger evidence than a timed-out Shell
     * request.  Restore local state immediately so an old but functional item
     * remains usable while the health probe verifies it in the background. */
    g_trayIconActive = TRUE;
    g_trayCallbackRecoveryAllowed = FALSE;
    g_trayCallbackVersion = NOTIFYICON_VERSION;
    CancelTrayRecreateRetry(hwnd);
    StartTrayHealthCheck(hwnd);
    RefreshTrayBackgroundWorkState();
    LOG_INFO("Tray registration restored from Explorer callback: hwnd=0x%p",
             hwnd);
    Tray_LogDiagnosticSnapshot("callback-restored-registration", hwnd);
    return TRUE;
}

BOOL IsTrayIconActive(HWND hwnd) {
    return IsTrayIconActiveForWindow(hwnd);
}
