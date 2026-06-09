/**
 * @file window_desktop_integration.c
 * @brief Desktop integration and Z-order management
 */

#include "window/window_desktop_integration.h"
#include "window/window_core.h"
#include "config.h"
#include "timer/timer.h"
#include "plugin/plugin_data.h"
#include "drawing/drawing_render.h"
#include "log.h"
#include "../../resource/resource.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PROGMAN_CLASS L"Progman"
#define WORKERW_CLASS L"WorkerW"
#define SHELLDLL_CLASS L"SHELLDLL_DefView"
#define TASKBAR_CLASS L"Shell_TrayWnd"
#define TOPMOST_APPLY_RETRY_INTERVAL_MS 500
#define TOPMOST_APPLY_MAX_RETRIES 5
#define TOPMOST_APPLY_RETRY_COOLDOWN_MS 30000

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static HWND FindDesktopWorkerWindow(void);
static BOOL IsValidWindowHandle(HWND hwnd, const char* caller);
static void ShowWindowNoActivateIfNeeded(HWND hwnd);
static BOOL GetWindowTopmostState(HWND hwnd, BOOL* outTopmost);
static void LogTopmostDiagnostics(HWND hwnd, const char* phase, BOOL requestedTopmost);
static BOOL ScheduleTopmostApplyRetry(HWND hwnd, BOOL targetTopmost);

static int s_topmostApplyRetriesRemaining = 0;
static BOOL s_topmostApplyRetryActive = FALSE;
static BOOL s_topmostApplyRetryTarget = TRUE;
static DWORD s_topmostApplyRetryCooldownUntil = 0;
static BOOL s_topmostVisibilityRestoreActive = FALSE;
static BOOL s_windowIntentionallyHidden = FALSE;

static BOOL IsTopmostRetryCoolingDown(BOOL targetTopmost) {
    DWORD now = GetTickCount();
    return s_topmostApplyRetryCooldownUntil != 0 &&
           s_topmostApplyRetryTarget == targetTopmost &&
           (LONG)(s_topmostApplyRetryCooldownUntil - now) > 0;
}

/**
 * @brief Find WorkerW window containing SHELLDLL_DefView
 * @return WorkerW window handle, or Progman if WorkerW not found
 */
static HWND FindDesktopWorkerWindow(void) {
    HWND hProgman = FindWindowW(PROGMAN_CLASS, NULL);
    if (!hProgman) return NULL;
    
    HWND hWorkerW = FindWindowExW(NULL, NULL, WORKERW_CLASS, NULL);
    while (hWorkerW) {
        HWND hView = FindWindowExW(hWorkerW, NULL, SHELLDLL_CLASS, NULL);
        if (hView) {
            return hWorkerW;
        }
        hWorkerW = FindWindowExW(NULL, hWorkerW, WORKERW_CLASS, NULL);
    }
    
    LOG_WARNING("Desktop WorkerW window not found");
    return hProgman;
}

static BOOL IsValidWindowHandle(HWND hwnd, const char* caller) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_WARNING("%s called with invalid window handle", caller ? caller : "Topmost API");
        return FALSE;
    }
    return TRUE;
}

static BOOL IsWindowOfClass(HWND hwnd, const wchar_t* className) {
    if (!hwnd || !IsWindow(hwnd) || !className) {
        return FALSE;
    }

    wchar_t actualClass[64] = {0};
    if (GetClassNameW(hwnd, actualClass, _countof(actualClass)) == 0) {
        return FALSE;
    }

    return wcscmp(actualClass, className) == 0;
}

static void ShowWindowNoActivateIfNeeded(HWND hwnd) {
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
}

static BOOL TrySetWindowOwner(HWND hwnd, HWND owner) {
    SetLastError(0);
    LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
    if (result == 0 && GetLastError() != 0) {
        LOG_WARNING("SetWindowLongPtr(GWLP_HWNDPARENT) failed (err=%lu)", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL TrySetWindowNoActivate(HWND hwnd, BOOL noActivate) {
    SetLastError(0);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle == 0 && GetLastError() != 0) {
        LOG_WARNING("GetWindowLong(GWL_EXSTYLE) failed (err=%lu)", GetLastError());
        return FALSE;
    }

    LONG desiredStyle = noActivate ? (exStyle | WS_EX_NOACTIVATE) : (exStyle & ~WS_EX_NOACTIVATE);
    if (desiredStyle == exStyle) {
        return TRUE;
    }

    SetLastError(0);
    LONG result = SetWindowLong(hwnd, GWL_EXSTYLE, desiredStyle);
    if (result == 0 && GetLastError() != 0) {
        LOG_WARNING("SetWindowLong(GWL_EXSTYLE) update failed (err=%lu)", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL GetWindowTopmostState(HWND hwnd, BOOL* outTopmost) {
    if (!outTopmost) return FALSE;

    SetLastError(0);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle == 0 && GetLastError() != 0) {
        LOG_WARNING("GetWindowLong(GWL_EXSTYLE) failed while checking topmost state (err=%lu)",
                    GetLastError());
        return FALSE;
    }

    *outTopmost = ((exStyle & WS_EX_TOPMOST) != 0);
    return TRUE;
}

static void LogTopmostDiagnostics(HWND hwnd, const char* phase, BOOL requestedTopmost) {
    SetLastError(0);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    DWORD exStyleErr = GetLastError();
    HWND owner = (HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
    BOOL actualTopmost = FALSE;
    BOOL hasActual = GetWindowTopmostState(hwnd, &actualTopmost);

    LOG_WARNING("Topmost diagnostics [%s]: requested=%d preference=%d runtimeTarget=%d actualKnown=%d actual=%d exStyle=0x%08lX exErr=%lu owner=0x%p editMode=%d visible=%d iconic=%d",
                phase ? phase : "unknown",
                requestedTopmost,
                CLOCK_WINDOW_TOPMOST,
                CLOCK_WINDOW_EFFECTIVE_TOPMOST,
                hasActual,
                actualTopmost,
                (unsigned long)exStyle,
                exStyleErr,
                owner,
                CLOCK_EDIT_MODE,
                IsWindowVisible(hwnd),
                IsIconic(hwnd));
}

static BOOL ScheduleTopmostApplyRetry(HWND hwnd, BOOL targetTopmost) {
    if (!IsWindow(hwnd)) return FALSE;

    BOOL targetChanged = (s_topmostApplyRetryTarget != targetTopmost);

    if (!targetChanged && IsTopmostRetryCoolingDown(targetTopmost)) {
        return FALSE;
    }

    if (!s_topmostApplyRetryActive || s_topmostApplyRetryTarget != targetTopmost) {
        s_topmostApplyRetriesRemaining = TOPMOST_APPLY_MAX_RETRIES;
        s_topmostApplyRetryTarget = targetTopmost;
        s_topmostApplyRetryCooldownUntil = 0;
    }

    if (!SetTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY, TOPMOST_APPLY_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("Failed to schedule topmost apply retry (err=%lu)", GetLastError());
        s_topmostApplyRetriesRemaining = 0;
        s_topmostApplyRetryActive = FALSE;
        return FALSE;
    }
    s_topmostApplyRetryActive = TRUE;
    return TRUE;
}

static void CancelTopmostVisibilityRestore(HWND hwnd) {
    if (s_topmostVisibilityRestoreActive && hwnd && IsWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE);
    }
    s_topmostVisibilityRestoreActive = FALSE;
}

static BOOL WindowShouldBeVisibleForCurrentMode(void) {
    if (CLOCK_EDIT_MODE || CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP ||
        PluginData_IsActive()) {
        return TRUE;
    }

    return (CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME);
}

static BOOL ShouldRecoverTopmostVisibility(HWND hwnd) {
    return IsValidWindowHandle(hwnd, "ShouldRecoverTopmostVisibility") &&
           CLOCK_WINDOW_EFFECTIVE_TOPMOST &&
           !s_windowIntentionallyHidden &&
           WindowShouldBeVisibleForCurrentMode();
}

static BOOL ScheduleTopmostVisibilityRestore(HWND hwnd, const char* reason) {
    if (!ShouldRecoverTopmostVisibility(hwnd)) {
        return FALSE;
    }

    if (s_topmostVisibilityRestoreActive) {
        return TRUE;
    }

    LOG_WARNING("Topmost window visibility changed externally; scheduling restore (%s)",
                reason ? reason : "unknown");
    if (!SetTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE, 100, NULL)) {
        LOG_WARNING("Failed to schedule topmost visibility restore (err=%lu)", GetLastError());
        s_topmostVisibilityRestoreActive = FALSE;
        return FALSE;
    }
    s_topmostVisibilityRestoreActive = TRUE;
    return TRUE;
}

static BOOL ApplyWindowTopmostStateInternal(HWND hwnd, BOOL topmost, BOOL persistConfig, BOOL updatePreference, BOOL updateRuntimeTarget, BOOL scheduleRetry) {
    BOOL ownerApplied = TRUE;
    BOOL styleApplied = TRUE;
    BOOL zOrderApplied = FALSE;
    DWORD zOrderError = ERROR_SUCCESS;
    BOOL actualTopmost = FALSE;
    BOOL hasActualTopmost = FALSE;
    BOOL suppressFailureDiagnostics = !updatePreference && !persistConfig && !updateRuntimeTarget &&
                                      IsTopmostRetryCoolingDown(topmost);

    if (suppressFailureDiagnostics) {
        return FALSE;
    }

    if (persistConfig) {
        BOOL persisted = WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
        if (!persisted) {
            LOG_WARNING("Topmost preference change was not applied because config persistence failed");
            return FALSE;
        }
    }
    if (updatePreference) {
        CLOCK_WINDOW_TOPMOST = topmost;
    }
    if (updateRuntimeTarget) {
        CLOCK_WINDOW_EFFECTIVE_TOPMOST = topmost;
    }

    if (topmost) {
        ownerApplied = TrySetWindowOwner(hwnd, NULL);
        zOrderApplied = SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                                     SWP_FRAMECHANGED);
        if (!zOrderApplied) zOrderError = GetLastError();
        styleApplied = TrySetWindowNoActivate(hwnd, FALSE);
    } else {
        HWND hDesktop = FindDesktopWorkerWindow();
        if (hDesktop) {
            ownerApplied = TrySetWindowOwner(hwnd, hDesktop);
        } else {
            LOG_WARNING("Desktop anchor not found, clearing parent");
            ownerApplied = TrySetWindowOwner(hwnd, NULL);
        }

        zOrderApplied = SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (!zOrderApplied) zOrderError = GetLastError();
        if (!SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
            LOG_WARNING("SetWindowPos(HWND_TOP) failed while applying non-topmost (err=%lu)",
                        GetLastError());
            zOrderApplied = FALSE;
        }
        styleApplied = TrySetWindowNoActivate(hwnd, TRUE);
    }

    if (!SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED)) {
        LOG_WARNING("SetWindowPos(SWP_FRAMECHANGED) failed after topmost apply (err=%lu)",
                    GetLastError());
        styleApplied = FALSE;
    }

    hasActualTopmost = GetWindowTopmostState(hwnd, &actualTopmost);
    if (hasActualTopmost && actualTopmost != topmost) {
        LOG_WARNING("Topmost actual state mismatch after apply: requested=%d actual=%d",
                    topmost, actualTopmost);
        zOrderApplied = FALSE;
    }

    if (zOrderApplied && ownerApplied && styleApplied && hasActualTopmost && actualTopmost == topmost) {
        s_topmostApplyRetriesRemaining = 0;
        s_topmostApplyRetryActive = FALSE;
        s_topmostApplyRetryCooldownUntil = 0;
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
    } else {
        LOG_WARNING("Topmost apply incomplete: requested=%d zOrder=%d owner=%d style=%d actualKnown=%d actual=%d (zErr=%lu)",
                    topmost, zOrderApplied, ownerApplied, styleApplied,
                    hasActualTopmost, actualTopmost, zOrderError);
        LogTopmostDiagnostics(hwnd, "after-apply-failure", topmost);
        if (scheduleRetry) {
            if (updatePreference || persistConfig) {
                s_topmostApplyRetryActive = FALSE;
                s_topmostApplyRetryCooldownUntil = 0;
            }
            ScheduleTopmostApplyRetry(hwnd, topmost);
        }
    }

    return (zOrderApplied && ownerApplied && styleApplied && hasActualTopmost && actualTopmost == topmost);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

BOOL SetWindowTopmost(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmost")) return FALSE;

    return ApplyWindowTopmostStateInternal(hwnd, topmost, TRUE, TRUE, TRUE, TRUE);
}

BOOL SetWindowTopmostFromConfig(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmostFromConfig")) return FALSE;

    return ApplyWindowTopmostStateInternal(hwnd, topmost, FALSE, TRUE, TRUE, TRUE);
}

BOOL SetWindowTopmostTransient(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmostTransient")) return FALSE;

    return ApplyWindowTopmostStateInternal(hwnd, topmost, FALSE, FALSE, TRUE, TRUE);
}

BOOL RefreshWindowTopmostState(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "RefreshWindowTopmostState")) return FALSE;
    return ApplyWindowTopmostStateInternal(hwnd, CLOCK_WINDOW_EFFECTIVE_TOPMOST, FALSE, FALSE, FALSE, TRUE);
}

void EnsureWindowVisibleWithTopmostState(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "EnsureWindowVisibleWithTopmostState")) return;

    s_windowIntentionallyHidden = FALSE;
    CancelTopmostVisibilityRestore(hwnd);
    ShowWindowNoActivateIfNeeded(hwnd);

    RefreshWindowTopmostState(hwnd);

    if (CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

void HideWindowIntentionally(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "HideWindowIntentionally")) return;

    s_windowIntentionallyHidden = TRUE;
    CancelTopmostVisibilityRestore(hwnd);
    StopDrawingRenderAnimationTimer(hwnd);
    ShowWindow(hwnd, SW_HIDE);
}

BOOL HandleTopmostVisibilityChange(HWND hwnd, const WINDOWPOS* pwp) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostVisibilityChange")) return FALSE;

    if (!pwp) {
        s_topmostVisibilityRestoreActive = FALSE;
    }

    if (pwp && (pwp->flags & SWP_SHOWWINDOW)) {
        s_windowIntentionallyHidden = FALSE;
        CancelTopmostVisibilityRestore(hwnd);
        return FALSE;
    }

    BOOL hiddenByWindowPos = pwp && (pwp->flags & SWP_HIDEWINDOW);
    BOOL hiddenNow = !IsWindowVisible(hwnd) || IsIconic(hwnd);

    if (pwp && !hiddenByWindowPos) {
        return FALSE;
    }

    if (!ShouldRecoverTopmostVisibility(hwnd)) {
        return FALSE;
    }

    if (!hiddenByWindowPos && !hiddenNow) {
        return FALSE;
    }

    if (pwp) {
        return ScheduleTopmostVisibilityRestore(hwnd, "window-pos-hide");
    }

    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

BOOL HandleTopmostHiddenEvent(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostHiddenEvent")) return FALSE;
    return ScheduleTopmostVisibilityRestore(hwnd, "show-window-hide");
}

void HandleTopmostShownEvent(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostShownEvent")) return;
    s_windowIntentionallyHidden = FALSE;
    CancelTopmostVisibilityRestore(hwnd);
}

BOOL HandleTopmostMinimizeCommand(HWND hwnd, UINT sysCommand) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostMinimizeCommand")) return FALSE;

    UINT cmd = sysCommand & 0xFFF0;
    if (cmd != SC_MINIMIZE || !CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        return FALSE;
    }

    EnsureWindowVisibleWithTopmostState(hwnd);
    return TRUE;
}

BOOL HandleTopmostSizeEvent(HWND hwnd, WPARAM sizeType) {
    static BOOL s_restoringTopmostMinimize = FALSE;

    if (!IsValidWindowHandle(hwnd, "HandleTopmostSizeEvent")) return FALSE;
    if (sizeType != SIZE_MINIMIZED || !CLOCK_WINDOW_EFFECTIVE_TOPMOST) return FALSE;
    if (s_restoringTopmostMinimize) return TRUE;

    s_restoringTopmostMinimize = TRUE;
    EnsureWindowVisibleWithTopmostState(hwnd);
    s_restoringTopmostMinimize = FALSE;
    return TRUE;
}

void ReattachToDesktop(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "ReattachToDesktop")) return;

    HWND hDesktop = FindDesktopWorkerWindow();
    
    if (hDesktop) {
        TrySetWindowOwner(hwnd, hDesktop);
    } else {
        TrySetWindowOwner(hwnd, NULL);
        LOG_WARNING("Desktop worker not found, owner cleared");
    }
    
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

BOOL EnforceTopmostOverTaskbar(HWND hwnd) {
    static HWND s_taskbarHwnd = NULL;

    if (!IsValidWindowHandle(hwnd, "EnforceTopmostOverTaskbar")) return FALSE;

    /* Only enforce if topmost mode is enabled */
    if (!CLOCK_WINDOW_EFFECTIVE_TOPMOST) return FALSE;

    if ((!IsWindowVisible(hwnd) || IsIconic(hwnd)) &&
        ScheduleTopmostVisibilityRestore(hwnd, "topmost-enforce-hidden")) {
        return TRUE;
    }

    /* Get our window position */
    RECT rcWindow;
    if (!GetWindowRect(hwnd, &rcWindow)) return FALSE;
    
    /* Get taskbar position */
    if (!IsWindowOfClass(s_taskbarHwnd, TASKBAR_CLASS)) {
        s_taskbarHwnd = FindWindowW(TASKBAR_CLASS, NULL);
    }
    if (!s_taskbarHwnd) return FALSE;
    
    RECT rcTaskbar;
    if (!GetWindowRect(s_taskbarHwnd, &rcTaskbar)) {
        s_taskbarHwnd = NULL;
        return FALSE;
    }
    
    /* Check if our window overlaps with taskbar area */
    BOOL overlaps = !(rcWindow.right < rcTaskbar.left ||
                      rcWindow.left > rcTaskbar.right ||
                      rcWindow.bottom < rcTaskbar.top ||
                      rcWindow.top > rcTaskbar.bottom);

    BOOL styleTopmost = FALSE;
    BOOL hasTopmostState = GetWindowTopmostState(hwnd, &styleTopmost);
    BOOL needReassert = overlaps || !hasTopmostState || !styleTopmost;

    if (needReassert) {
        if (SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
            LOG_DEBUG("Topmost state reasserted (overlapsTaskbar=%d)", overlaps);
            if (!GetWindowTopmostState(hwnd, &styleTopmost) || !styleTopmost) {
                if (ScheduleTopmostApplyRetry(hwnd, TRUE)) {
                    LOG_WARNING("Topmost reassert did not produce WS_EX_TOPMOST; scheduling retry");
                    LogTopmostDiagnostics(hwnd, "reassert-mismatch", TRUE);
                }
            }
        } else {
            DWORD err = GetLastError();
            if (ScheduleTopmostApplyRetry(hwnd, TRUE)) {
                LOG_WARNING("Topmost reassert failed (overlapsTaskbar=%d err=%lu)",
                            overlaps, err);
                LogTopmostDiagnostics(hwnd, "reassert-failure", TRUE);
            }
        }
    }
    
    return overlaps;
}

BOOL HandleTopmostApplyRetry(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostApplyRetry")) return TRUE;

    if (s_topmostApplyRetriesRemaining <= 0) {
        s_topmostApplyRetryActive = FALSE;
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
        return TRUE;
    }

    s_topmostApplyRetriesRemaining--;

    if (ApplyWindowTopmostStateInternal(hwnd, CLOCK_WINDOW_EFFECTIVE_TOPMOST,
                                        FALSE, FALSE, FALSE, FALSE)) {
        s_topmostApplyRetriesRemaining = 0;
        s_topmostApplyRetryActive = FALSE;
        s_topmostApplyRetryCooldownUntil = 0;
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
    } else if (s_topmostApplyRetriesRemaining <= 0) {
        LOG_WARNING("Topmost apply retry exhausted; keeping requested runtime target for future recovery");
        LogTopmostDiagnostics(hwnd, "retry-exhausted", CLOCK_WINDOW_EFFECTIVE_TOPMOST);
        s_topmostApplyRetryActive = FALSE;
        s_topmostApplyRetryCooldownUntil = GetTickCount() + TOPMOST_APPLY_RETRY_COOLDOWN_MS;
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
    } else {
        if (!SetTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY, TOPMOST_APPLY_RETRY_INTERVAL_MS, NULL)) {
            LOG_WARNING("Failed to continue topmost apply retry timer (err=%lu)", GetLastError());
            s_topmostApplyRetriesRemaining = 0;
            s_topmostApplyRetryActive = FALSE;
        }
    }

    return TRUE;
}

void CleanupWindowDesktopIntegrationState(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY);
        KillTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE);
    }

    s_topmostApplyRetriesRemaining = 0;
    s_topmostApplyRetryActive = FALSE;
    s_topmostApplyRetryCooldownUntil = 0;
    s_topmostVisibilityRestoreActive = FALSE;
}
