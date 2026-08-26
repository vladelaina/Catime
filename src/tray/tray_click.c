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
#include <shellapi.h>

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
        return FALSE;
    }
    if (!IsValidTrayMainWindow(hwnd)) {
        return FALSE;
    }
    if (IsTrayInteractionSuspended()) {
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
        return FALSE;
    }
    return ShowTrayMenu(hwnd, mouseMessage, anchor, TRUE);
}
