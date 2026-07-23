/**
 * @file notification.c
 * @brief Notification mode dispatch.
 */

#include "notification_internal.h"

void ShowNotification(HWND hwnd, const wchar_t* message) {
    if (!message) return;
    if (!NotificationGetOwnerWindow(hwnd)) return;

    NotificationLoadConfigs();

    if (g_AppConfig.notification.display.disabled || g_AppConfig.notification.display.timeout_ms == 0) {
        return;
    }

    switch (g_AppConfig.notification.display.type) {
        case NOTIFICATION_TYPE_CATIME:
            ShowToastNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            ShowModalNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_OS:
            NotificationFallbackToTray(hwnd, message);
            break;
        default:
            ShowToastNotification(hwnd, message);
            break;
    }
}
