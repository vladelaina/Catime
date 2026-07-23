/**
 * @file window_topmost_retry.c
 * @brief Topmost retry scheduling and taskbar overlap enforcement
 */
#include "window/window_desktop_integration.h"
#include "window_desktop_integration_internal.h"

#include "config.h"
#include "log.h"
#include "timer/timer.h"
#include "../../resource/resource.h"

#define TOPMOST_APPLY_RETRY_INTERVAL_MS 500
#define TOPMOST_APPLY_MAX_RETRIES 5
#define TOPMOST_APPLY_RETRY_COOLDOWN_MS 30000

static int s_retriesRemaining = 0;
static BOOL s_retryActive = FALSE;
static BOOL s_retryTarget = TRUE;
static DWORD s_cooldownUntil = 0;

BOOL WindowTopmostRetry_IsCoolingDown(BOOL targetTopmost) {
    DWORD now = GetTickCount();
    return s_cooldownUntil != 0 && s_retryTarget == targetTopmost &&
           (LONG)(s_cooldownUntil - now) > 0;
}

void WindowTopmostRetry_ResetForRequest(void) {
    s_retryActive = FALSE;
    s_cooldownUntil = 0;
}

void WindowTopmostRetry_Clear(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
    }
    s_retriesRemaining = 0;
    s_retryActive = FALSE;
    s_cooldownUntil = 0;
}

BOOL WindowTopmostRetry_Schedule(HWND hwnd, BOOL targetTopmost) {
    BOOL targetChanged;
    BOOL hadActiveRetry;

    if (!IsWindow(hwnd)) return FALSE;
    targetChanged = s_retryTarget != targetTopmost;
    hadActiveRetry = s_retryActive && !targetChanged;
    if (!targetChanged &&
        WindowTopmostRetry_IsCoolingDown(targetTopmost)) {
        return FALSE;
    }
    if (!s_retryActive || targetChanged) {
        s_retriesRemaining = TOPMOST_APPLY_MAX_RETRIES;
        s_retryTarget = targetTopmost;
        s_cooldownUntil = 0;
    }
    if (!SetTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY,
                  TOPMOST_APPLY_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("Failed to schedule topmost retry (error=%lu)",
                    GetLastError());
        if (!hadActiveRetry) {
            s_retriesRemaining = 0;
            s_retryActive = FALSE;
        }
        return FALSE;
    }
    s_retryActive = TRUE;
    return TRUE;
}

BOOL HandleTopmostApplyRetry(HWND hwnd) {
    if (!WindowDesktop_IsValid(hwnd, "HandleTopmostApplyRetry")) return TRUE;
    if (CLOCK_IS_DRAGGING) return TRUE;
    if (s_retriesRemaining <= 0) {
        s_retryActive = FALSE;
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
        return TRUE;
    }

    --s_retriesRemaining;
    if (WindowTopmost_ApplyInternal(hwnd,
                                    CLOCK_WINDOW_EFFECTIVE_TOPMOST,
                                    FALSE, FALSE, FALSE, FALSE)) {
        WindowTopmostRetry_Clear(hwnd);
    } else if (s_retriesRemaining <= 0) {
        LOG_WARNING("Topmost retry exhausted; retaining the requested runtime target");
        WindowTopmost_LogDiagnostics(hwnd, "retry-exhausted",
                                     CLOCK_WINDOW_EFFECTIVE_TOPMOST);
        s_retryActive = FALSE;
        s_cooldownUntil = GetTickCount() +
                          TOPMOST_APPLY_RETRY_COOLDOWN_MS;
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
    } else if (!SetTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY,
                         TOPMOST_APPLY_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("Failed to continue topmost retry (error=%lu)",
                    GetLastError());
    }
    return TRUE;
}

BOOL EnforceTopmostOverTaskbar(HWND hwnd) {
    RECT windowRect;
    BOOL overlaps;
    BOOL actualTopmost = FALSE;
    BOOL actualKnown;

    if (!WindowDesktop_IsValid(hwnd, "EnforceTopmostOverTaskbar") ||
        !CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        return FALSE;
    }
    if ((!IsWindowVisible(hwnd) || IsIconic(hwnd)) &&
        HandleTopmostHiddenEvent(hwnd)) {
        return TRUE;
    }
    if (!GetWindowRect(hwnd, &windowRect)) return FALSE;

    overlaps = WindowDesktop_OverlapsAnyTaskbar(&windowRect);
    actualKnown = WindowDesktop_GetTopmostState(hwnd, &actualTopmost);
    if (!overlaps && actualKnown && actualTopmost) return FALSE;
    if (!SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        DWORD error = GetLastError();
        if (WindowTopmostRetry_Schedule(hwnd, TRUE)) {
            LOG_WARNING("Topmost reassert failed (overlap=%d error=%lu)",
                        overlaps, error);
            WindowTopmost_LogDiagnostics(hwnd, "reassert-failure", TRUE);
        }
        return overlaps;
    }
    if (!WindowDesktop_GetTopmostState(hwnd, &actualTopmost) ||
        !actualTopmost) {
        if (WindowTopmostRetry_Schedule(hwnd, TRUE)) {
            LOG_WARNING("Topmost reassert did not set WS_EX_TOPMOST");
            WindowTopmost_LogDiagnostics(hwnd, "reassert-mismatch", TRUE);
        }
    }
    return overlaps;
}

void WindowTopmostRetry_Cleanup(HWND hwnd) {
    WindowTopmostRetry_Clear(hwnd);
}
