/**
 * @file tray_diagnostics.c
 * @brief Rate-limited diagnostics for tray callback and menu failures.
 */

#include "tray_internal.h"
#include "tray/tray_event_protocol.h"
#include "timer/main_timer.h"
#include "timer/timer.h"
#include "log.h"
#include <limits.h>
#include <string.h>

#define TRAY_DIAGNOSTIC_REPEAT_INTERVAL_MS (60u * 1000u)
#define TRAY_DIAGNOSTIC_REASON_CAPACITY 24
#define TRAY_DIAGNOSTIC_REASON_LENGTH 63

static DWORD g_lastRejectedTrayCallbackLogTick = 0;
static UINT g_rejectedTrayCallbackSuppressedCount = 0;

typedef struct {
    char reason[TRAY_DIAGNOSTIC_REASON_LENGTH + 1];
    DWORD lastLogTick;
    UINT suppressedCount;
} TrayDiagnosticReasonState;

static TrayDiagnosticReasonState g_trayDiagnosticReasons[
    TRAY_DIAGNOSTIC_REASON_CAPACITY];
static UINT g_nextTrayDiagnosticReasonSlot = 0;

static TrayDiagnosticReasonState* FindTrayDiagnosticReason(
    const char* reason) {
    const char* value = reason ? reason : "unspecified";
    TrayDiagnosticReasonState* empty = NULL;
    for (UINT i = 0; i < TRAY_DIAGNOSTIC_REASON_CAPACITY; i++) {
        TrayDiagnosticReasonState* state = &g_trayDiagnosticReasons[i];
        if (state->reason[0] == '\0') {
            if (!empty) empty = state;
            continue;
        }
        if (strcmp(state->reason, value) == 0) {
            return state;
        }
    }

    TrayDiagnosticReasonState* state = empty;
    if (!state) {
        state = &g_trayDiagnosticReasons[
            g_nextTrayDiagnosticReasonSlot % TRAY_DIAGNOSTIC_REASON_CAPACITY];
        g_nextTrayDiagnosticReasonSlot++;
    }
    strncpy_s(state->reason, sizeof(state->reason), value, _TRUNCATE);
    state->lastLogTick = 0;
    state->suppressedCount = 0;
    return state;
}

void Tray_LogDiagnosticSnapshot(const char* reason, HWND hwnd) {
    DWORD now = GetTickCount();
    TrayDiagnosticReasonState* reasonState =
        FindTrayDiagnosticReason(reason);
    if (reasonState->lastLogTick != 0 &&
        (DWORD)(now - reasonState->lastLogTick) <
            TRAY_DIAGNOSTIC_REPEAT_INTERVAL_MS) {
        if (reasonState->suppressedCount < UINT_MAX) {
            reasonState->suppressedCount++;
        }
        return;
    }

    UINT suppressedCount = reasonState->suppressedCount;
    reasonState->lastLogTick = now ? now : 1u;
    reasonState->suppressedCount = 0;
    HWND foreground = GetForegroundWindow();
    BOOL validWindow = IsValidTrayMainWindow(hwnd);
    BOOL iconActive = IsTrayIconActiveForWindow(hwnd);
    BOOL visible = validWindow && IsWindowVisible(hwnd);
    BOOL enabled = validWindow && IsWindowEnabled(hwnd);
    LONG_PTR exStyle = validWindow ? GetWindowLongPtrW(hwnd, GWL_EXSTYLE) : 0;
    DWORD windowThread = validWindow
        ? GetWindowThreadProcessId(hwnd, NULL) : 0;
    DWORD foregroundThread = foreground
        ? GetWindowThreadProcessId(foreground, NULL) : 0;

    LOG_INFO(
        "Tray diagnostic [%s]: thread=%lu hwnd=0x%p main=0x%p nidHwnd=0x%p "
        "valid=%d visible=%d enabled=%d exStyle=0x%Ix foreground=0x%p "
        "windowThread=%lu foregroundThread=%lu iconActive=%d gIcon=%d "
        "nidSize=%u nidFlags=0x%X nidId=%u callback=0x%X "
        "expectedCallback=0x%X callbackVersion=%u "
        "suspended=%d shuttingDown=%d retryCount=%u healthFailures=%u "
        "tooltipActive=%d tipTimer=%d background=%d systemMonitor=%d "
        "timerRunning=%d highPrecision=%d showTime=%d countUp=%d paused=%d "
        "total=%d elapsed=%d timeoutAction=%d tick=%lu suppressed=%u",
        reason ? reason : "unspecified", GetCurrentThreadId(), hwnd, g_mainHwnd,
        nid.hWnd, validWindow, visible, enabled, exStyle, foreground,
        windowThread, foregroundThread, iconActive, g_trayIconActive,
        nid.cbSize, nid.uFlags, nid.uID, nid.uCallbackMessage,
        CLOCK_WM_TRAYICON, g_trayCallbackVersion,
        IsTrayInteractionSuspended(), g_trayShuttingDown,
        g_trayRecreateRetryCount,
        g_trayRecoveryPolicyState.consecutiveFailures,
        IsTrayTooltipActive(), g_trayTipTimerActive,
        g_trayBackgroundWorkEnabled, g_traySystemMonitorActive,
        MainTimer_IsRunning(), MainTimer_IsHighPrecision(),
        CLOCK_SHOW_CURRENT_TIME, CLOCK_COUNT_UP, CLOCK_IS_PAUSED,
        CLOCK_TOTAL_TIME, countdown_elapsed_time, CLOCK_TIMEOUT_ACTION,
        now, suppressedCount);
}

static BOOL IsTrayMenuCallbackRaw(BOOL version4, WPARAM wParam,
                                  LPARAM lParam) {
    (void)wParam;
    UINT message = version4 ? LOWORD((DWORD_PTR)lParam) : (UINT)lParam;
    return message == WM_LBUTTONUP || message == WM_RBUTTONUP ||
           message == WM_CONTEXTMENU || message == NIN_SELECT ||
           message == NIN_KEYSELECT;
}

void Tray_LogRejectedCallback(HWND hwnd, BOOL version4, WPARAM wParam, LPARAM lParam,
                              const char* reason) {
    DWORD now = GetTickCount();
    BOOL menuLike = IsTrayMenuCallbackRaw(version4, wParam, lParam);
    if (g_lastRejectedTrayCallbackLogTick != 0 &&
        (DWORD)(now - g_lastRejectedTrayCallbackLogTick) < 10000u) {
        if (g_rejectedTrayCallbackSuppressedCount < UINT_MAX) {
            g_rejectedTrayCallbackSuppressedCount++;
        }
        return;
    }
    UINT suppressedCount = g_rejectedTrayCallbackSuppressedCount;
    g_lastRejectedTrayCallbackLogTick = now ? now : 1u;
    g_rejectedTrayCallbackSuppressedCount = 0;
    LOG_WARNING(
        "Tray callback rejected (%s): preferredV4=%d wParam=0x%p lParam=0x%p "
        "iconActive=%d callbackVersion=%u hwnd=0x%p main=0x%p suppressed=%u",
        reason ? reason : "unknown", version4, (void*)(ULONG_PTR)wParam,
        (void*)(ULONG_PTR)lParam, g_trayIconActive, g_trayCallbackVersion,
        hwnd, g_mainHwnd, suppressedCount);
    if (menuLike) {
        Tray_LogDiagnosticSnapshot("callback-rejected", hwnd);
    }
}
