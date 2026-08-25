/**
 * @file tray_click.c
 * @brief Reliable tray menu activation and notification-area focus recovery.
 */

#include "tray_internal.h"
#include "tray/tray_events.h"
#include "tray/tray_menu.h"
#include "audio_player.h"
#include "window/window_core.h"
#include "log.h"
#include <limits.h>
#include <shellapi.h>

static DWORD g_lastTrayMenuRejectionLogTick = 0;
static UINT g_trayMenuRejectionSuppressedCount = 0;

static void LogTrayMenuRejection(const char* reason, HWND hwnd,
                                 UINT mouseMessage, BOOL active) {
    DWORD now = GetTickCount();
    if (g_lastTrayMenuRejectionLogTick != 0 &&
        (DWORD)(now - g_lastTrayMenuRejectionLogTick) < 10000u) {
        if (g_trayMenuRejectionSuppressedCount < UINT_MAX) {
            g_trayMenuRejectionSuppressedCount++;
        }
        return;
    }

    UINT suppressedCount = g_trayMenuRejectionSuppressedCount;
    g_lastTrayMenuRejectionLogTick = now ? now : 1u;
    g_trayMenuRejectionSuppressedCount = 0;
    LOG_WARNING(
        "Tray menu rejected (%s): hwnd=0x%p message=0x%X active=%d "
        "suspended=%d suppressed=%u",
        reason ? reason : "unknown", hwnd, mouseMessage, active,
        IsTrayInteractionSuspended(), suppressedCount);
}

static void RestoreNotificationAreaFocus(HWND hwnd) {
    if (!IsValidTrayMainWindow(hwnd) || !IsTrayIconActiveForWindow(hwnd)) {
        return;
    }
    NOTIFYICONDATAW focusData = {0};
    focusData.cbSize = sizeof(focusData);
    focusData.hWnd = hwnd;
    focusData.uID = CLOCK_ID_TRAY_APP_ICON;
    if (!Shell_NotifyIconW(NIM_SETFOCUS, &focusData)) {
        LOG_WARNING("Failed to return focus to the tray notification area "
                    "(error=%lu)", GetLastError());
    }
}

static BOOL ShowTrayMenu(HWND hwnd, UINT mouseMessage,
                         const POINT* anchor, BOOL restoreTrayFocus) {
    if (mouseMessage != WM_LBUTTONUP && mouseMessage != WM_RBUTTONUP) {
        LogTrayMenuRejection("unsupported-message", hwnd, mouseMessage,
                             IsTrayIconActiveForWindow(hwnd));
        Tray_LogDiagnosticSnapshot("menu-rejected-unsupported-message", hwnd);
        return FALSE;
    }
    if (!IsValidTrayMainWindow(hwnd)) {
        LogTrayMenuRejection("invalid-owner-window", hwnd, mouseMessage,
                             FALSE);
        Tray_LogDiagnosticSnapshot("menu-rejected-invalid-window", hwnd);
        return FALSE;
    }
    if (IsTrayInteractionSuspended()) {
        LogTrayMenuRejection("interaction-suspended", hwnd, mouseMessage,
                             IsTrayIconActiveForWindow(hwnd));
        Tray_LogDiagnosticSnapshot("menu-rejected-suspended", hwnd);
        return FALSE;
    }

    StopNotificationSound();
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    TryRestorePendingWindowPosition(hwnd);
    SetTrayInteractionSuspended(TRUE);
    if (mouseMessage == WM_RBUTTONUP) {
        ShowColorMenu(hwnd, anchor);
    } else {
        ShowContextMenu(hwnd, anchor);
    }
    if (restoreTrayFocus) RestoreNotificationAreaFocus(hwnd);
    SetTrayInteractionSuspended(FALSE);
    return TRUE;
}

BOOL HandleTrayMenuClick(HWND hwnd, UINT mouseMessage) {
    return ShowTrayMenu(hwnd, mouseMessage, NULL, FALSE);
}

BOOL HandleTrayIconMenuActivation(HWND hwnd, UINT mouseMessage,
                                  const POINT* anchor) {
    if (!IsValidTrayMainWindow(hwnd) ||
        !IsTrayIconActiveForWindow(hwnd)) {
        LogTrayMenuRejection("inactive-tray", hwnd, mouseMessage,
                             IsTrayIconActiveForWindow(hwnd));
        Tray_LogDiagnosticSnapshot("activation-rejected-inactive-tray", hwnd);
        return FALSE;
    }
    return ShowTrayMenu(hwnd, mouseMessage, anchor, TRUE);
}
