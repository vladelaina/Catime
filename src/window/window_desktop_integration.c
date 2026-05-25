/**
 * @file window_desktop_integration.c
 * @brief Desktop integration and Z-order management
 */

#include "window/window_desktop_integration.h"
#include "window/window_core.h"
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PROGMAN_CLASS L"Progman"
#define WORKERW_CLASS L"WorkerW"
#define SHELLDLL_CLASS L"SHELLDLL_DefView"
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

static BOOL IsTopmostRetryCoolingDown(BOOL targetTopmost) {
    DWORD now = GetTickCount();
    return s_topmostApplyRetryCooldownUntil != 0 &&
           s_topmostApplyRetryTarget == targetTopmost &&
           (LONG)(now - s_topmostApplyRetryCooldownUntil) < 0;
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
            LOG_INFO("Found desktop WorkerW window");
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

    LOG_INFO("Topmost diagnostics [%s]: requested=%d preference=%d runtimeTarget=%d actualKnown=%d actual=%d exStyle=0x%08lX exErr=%lu owner=0x%p editMode=%d visible=%d iconic=%d",
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
        s_topmostApplyRetryActive = TRUE;
        s_topmostApplyRetryCooldownUntil = 0;
    }

    if (!SetTimer(hwnd, TIMER_ID_TOPMOST_APPLY_RETRY, TOPMOST_APPLY_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("Failed to schedule topmost apply retry (err=%lu)", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL ApplyWindowTopmostStateInternal(HWND hwnd, BOOL topmost, BOOL persistConfig, BOOL updatePreference, BOOL updateRuntimeTarget, BOOL scheduleRetry) {
    BOOL ownerApplied = TRUE;
    BOOL styleApplied = TRUE;
    BOOL zOrderApplied = FALSE;
    DWORD zOrderError = ERROR_SUCCESS;
    BOOL actualTopmost = FALSE;
    BOOL hasActualTopmost = FALSE;
    BOOL persisted = TRUE;
    BOOL suppressFailureDiagnostics = !updatePreference && !persistConfig && !updateRuntimeTarget &&
                                      IsTopmostRetryCoolingDown(topmost);

    if (suppressFailureDiagnostics) {
        return FALSE;
    }

    if (updatePreference) {
        CLOCK_WINDOW_TOPMOST = topmost;
    }
    if (updateRuntimeTarget) {
        CLOCK_WINDOW_EFFECTIVE_TOPMOST = topmost;
    }
    if (persistConfig) {
        persisted = WriteConfigTopmost(topmost ? "TRUE" : "FALSE");
        if (!persisted) {
            LOG_WARNING("Topmost preference updated in memory but failed to persist config");
        }
    }

    LogTopmostDiagnostics(hwnd, "before-apply", topmost);

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
            LOG_INFO("Window parented to desktop anchor for Win+D protection");
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
        LOG_INFO("Window topmost state applied%s",
                 persistConfig ? (persisted ? " and saved" : " but config save failed") : "");
        LogTopmostDiagnostics(hwnd, "after-apply-success", topmost);
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

    LOG_INFO("Setting window topmost: %s", topmost ? "true" : "false");

    return ApplyWindowTopmostStateInternal(hwnd, topmost, TRUE, TRUE, TRUE, TRUE);
}

BOOL SetWindowTopmostFromConfig(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmostFromConfig")) return FALSE;

    LOG_INFO("Applying configured window topmost: %s", topmost ? "true" : "false");

    return ApplyWindowTopmostStateInternal(hwnd, topmost, FALSE, TRUE, TRUE, TRUE);
}

BOOL SetWindowTopmostTransient(HWND hwnd, BOOL topmost) {
    if (!IsValidWindowHandle(hwnd, "SetWindowTopmostTransient")) return FALSE;

    LOG_INFO("Applying transient window topmost: %s", topmost ? "true" : "false");

    return ApplyWindowTopmostStateInternal(hwnd, topmost, FALSE, FALSE, TRUE, TRUE);
}

BOOL RefreshWindowTopmostState(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "RefreshWindowTopmostState")) return FALSE;
    return ApplyWindowTopmostStateInternal(hwnd, CLOCK_WINDOW_EFFECTIVE_TOPMOST, FALSE, FALSE, FALSE, TRUE);
}

void EnsureWindowVisibleWithTopmostState(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "EnsureWindowVisibleWithTopmostState")) return;

    ShowWindowNoActivateIfNeeded(hwnd);

    RefreshWindowTopmostState(hwnd);

    if (CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

BOOL HandleTopmostMinimizeCommand(HWND hwnd, UINT sysCommand) {
    if (!IsValidWindowHandle(hwnd, "HandleTopmostMinimizeCommand")) return FALSE;

    UINT cmd = sysCommand & 0xFFF0;
    if (cmd != SC_MINIMIZE || !CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        return FALSE;
    }

    LOG_INFO("Blocking minimize request while topmost mode is enabled");
    EnsureWindowVisibleWithTopmostState(hwnd);
    return TRUE;
}

BOOL HandleTopmostSizeEvent(HWND hwnd, WPARAM sizeType) {
    static BOOL s_restoringTopmostMinimize = FALSE;

    if (!IsValidWindowHandle(hwnd, "HandleTopmostSizeEvent")) return FALSE;
    if (sizeType != SIZE_MINIMIZED || !CLOCK_WINDOW_EFFECTIVE_TOPMOST) return FALSE;
    if (s_restoringTopmostMinimize) return TRUE;

    s_restoringTopmostMinimize = TRUE;
    LOG_INFO("Topmost window was minimized, restoring visibility");
    EnsureWindowVisibleWithTopmostState(hwnd);
    s_restoringTopmostMinimize = FALSE;
    return TRUE;
}

void ReattachToDesktop(HWND hwnd) {
    if (!IsValidWindowHandle(hwnd, "ReattachToDesktop")) return;

    LOG_INFO("Reattaching window to desktop level");
    
    HWND hDesktop = FindDesktopWorkerWindow();
    
    if (hDesktop) {
        TrySetWindowOwner(hwnd, hDesktop);
        LOG_INFO("Window owner set to desktop anchor");
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
    
    /* Get our window position */
    RECT rcWindow;
    if (!GetWindowRect(hwnd, &rcWindow)) return FALSE;
    
    /* Get taskbar position */
    if (!s_taskbarHwnd || !IsWindow(s_taskbarHwnd)) {
        s_taskbarHwnd = FindWindowW(L"Shell_TrayWnd", NULL);
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

    LOG_INFO("Retrying topmost apply: runtimeTarget=%d attemptsRemaining=%d",
             CLOCK_WINDOW_EFFECTIVE_TOPMOST, s_topmostApplyRetriesRemaining);
    s_topmostApplyRetriesRemaining--;

    if (ApplyWindowTopmostStateInternal(hwnd, CLOCK_WINDOW_EFFECTIVE_TOPMOST,
                                        FALSE, FALSE, FALSE, FALSE)) {
        LOG_INFO("Topmost apply retry succeeded");
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
            s_topmostApplyRetryActive = FALSE;
        }
    }

    return TRUE;
}

