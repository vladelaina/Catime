/**
 * @file notification_delivery.c
 * @brief Non-toast notification delivery and fallback helpers
 */
#include "notification_internal.h"

#include <stdlib.h>
#include <wchar.h>

#include "dialog/dialog_message.h"
#include "tray/tray.h"

#define MODAL_NOTIFICATION_START_FAILURE_COOLDOWN_MS 2000

typedef struct {
    HWND hwnd;
    wchar_t message[NOTIFICATION_MESSAGE_BUFFER_SIZE];
} DialogThreadParams;

static volatile LONG g_modalNotificationActive = 0;
static DWORD g_modalNotificationStartFailureCooldownUntil = 0;
/* A single static payload is safe because modal notifications are serialized. */
static DialogThreadParams g_modalDialogParams = {0};

static BOOL IsModalNotificationStartFailureCoolingDown(DWORD now) {
    return g_modalNotificationStartFailureCooldownUntil != 0 &&
           (LONG)(g_modalNotificationStartFailureCooldownUntil - now) > 0;
}

static void MarkModalNotificationStartFailure(DWORD now) {
    DWORD cooldownUntil = now + MODAL_NOTIFICATION_START_FAILURE_COOLDOWN_MS;
    g_modalNotificationStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

void NotificationFallbackToTray(HWND hwnd, const wchar_t* message) {
    if (!message) message = L"";
    HWND owner = NotificationGetOwnerWindow(hwnd);
    if (!owner) return;

    wchar_t boundedMessage[sizeof(((NOTIFYICONDATAW*)0)->szInfo) / sizeof(wchar_t)] = {0};
    wcsncpy_s(boundedMessage, _countof(boundedMessage), message, _TRUNCATE);

    int len = WideCharToMultiByte(CP_UTF8, 0, boundedMessage, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        char* ansiMessage = (char*)malloc((size_t)len);
        if (ansiMessage) {
            WideCharToMultiByte(CP_UTF8, 0, boundedMessage, -1, ansiMessage, len, NULL, NULL);
            ShowTrayNotification(owner, ansiMessage);
            free(ansiMessage);
        }
    }
}

static DWORD WINAPI ShowModalDialogThread(LPVOID lpParam) {
    (void)lpParam;

    HWND owner = NotificationGetOwnerWindow(g_modalDialogParams.hwnd);
    if (owner) {
        if (!DialogMessage_ShowNotification(
                owner, L"Catime", g_modalDialogParams.message)) {
            NotificationFallbackToTray(owner, g_modalDialogParams.message);
        }
    }

    g_modalDialogParams.hwnd = NULL;
    g_modalDialogParams.message[0] = L'\0';
    InterlockedExchange(&g_modalNotificationActive, 0);

    return 0;
}

/** Background thread avoids blocking main UI */
void ShowModalNotification(HWND hwnd, const wchar_t* message) {
    if (!message) return;
    HWND owner = NotificationGetOwnerWindow(hwnd);
    if (!owner) return;

    DWORD now = GetTickCount();
    if (IsModalNotificationStartFailureCoolingDown(now)) {
        NotificationFallbackToTray(owner, message);
        return;
    }

    if (InterlockedCompareExchange(&g_modalNotificationActive, 1, 0) != 0) {
        NotificationFallbackToTray(owner, message);
        return;
    }

    g_modalDialogParams.hwnd = owner;
    wcsncpy_s(g_modalDialogParams.message, _countof(g_modalDialogParams.message),
              message, _TRUNCATE);

    HANDLE hThread = CreateThread(NULL, 0, ShowModalDialogThread, NULL, 0, NULL);

    if (hThread == NULL) {
        g_modalDialogParams.hwnd = NULL;
        g_modalDialogParams.message[0] = L'\0';
        MarkModalNotificationStartFailure(now);
        InterlockedExchange(&g_modalNotificationActive, 0);
        MessageBeep(MB_OK);
        NotificationFallbackToTray(owner, message);
        return;
    }

    g_modalNotificationStartFailureCooldownUntil = 0;
    CloseHandle(hThread);
}
