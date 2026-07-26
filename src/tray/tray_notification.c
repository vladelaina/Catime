/**
 * @file tray_notification.c
 * @brief UTF-8 conversion and system tray balloon notifications.
 */

#include "tray_internal.h"
#include <limits.h>
#include <shellapi.h>

static BOOL IsUtf8ContinuationByte(unsigned char ch) {
    return (ch & 0xC0u) == 0x80u;
}

static size_t FindUtf8PrefixForWideCapacity(const char* text,
                                             size_t wideCapacity) {
    if (!text || wideCapacity <= 1) {
        return 0;
    }

    size_t bytes = 0;
    size_t chars = 0;
    while (text[bytes] && chars < wideCapacity - 1) {
        size_t charBytes = 1;
        unsigned char ch = (unsigned char)text[bytes];
        if ((ch & 0x80u) == 0) {
            charBytes = 1;
        } else if ((ch & 0xE0u) == 0xC0u) {
            charBytes = 2;
        } else if ((ch & 0xF0u) == 0xE0u) {
            charBytes = 3;
        } else if ((ch & 0xF8u) == 0xF0u) {
            charBytes = 4;
        } else {
            break;
        }

        for (size_t i = 1; i < charBytes; i++) {
            if (!IsUtf8ContinuationByte((unsigned char)text[bytes + i])) {
                return bytes;
            }
        }

        bytes += charBytes;
        chars++;
    }
    return bytes;
}

void ShowTrayNotification(HWND hwnd, const char* message) {
    if (!message) {
        return;
    }

    HWND owner = IsValidTrayMainWindow(hwnd) ? hwnd
                                             : GetValidTrayMainWindow();
    if (!owner || !IsTrayIconActiveForWindow(owner)) {
        return;
    }

    NOTIFYICONDATAW notification = {0};
    notification.cbSize = sizeof(notification);
    notification.hWnd = owner;
    notification.uID = CLOCK_ID_TRAY_APP_ICON;
    notification.uFlags = NIF_INFO;
    notification.dwInfoFlags = NIIF_NONE;
    notification.uTimeout = 3000;

    int converted = MultiByteToWideChar(
        CP_UTF8, 0, message, -1, notification.szInfo,
        (int)_countof(notification.szInfo));
    if (converted <= 0) {
        size_t bytes = FindUtf8PrefixForWideCapacity(
            message, _countof(notification.szInfo));
        if (bytes == 0 || bytes > (size_t)INT_MAX) {
            return;
        }
        converted = MultiByteToWideChar(
            CP_UTF8, 0, message, (int)bytes, notification.szInfo,
            (int)_countof(notification.szInfo) - 1);
        if (converted <= 0) {
            return;
        }
        notification.szInfo[converted] = L'\0';
    }
    notification.szInfoTitle[0] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &notification);
}
