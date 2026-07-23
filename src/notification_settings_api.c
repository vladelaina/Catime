/**
 * @file notification_settings_api.c
 * @brief Live notification preview and appearance updates.
 */

#include "notification_internal.h"

void ShowToastNotificationEx(HWND hwnd, const wchar_t* message, BOOL isPreview) {
    NotificationShowToastInternal(hwnd, message, isPreview,
                                  g_AppConfig.notification.display.timeout_ms,
                                  g_AppConfig.notification.display.max_opacity,
                                  TRUE, TRUE);
}

void ShowToastNotificationWithTimeout(HWND hwnd, const wchar_t* message, int timeoutMs) {
    NotificationShowToastInternal(hwnd, message, FALSE, timeoutMs,
                                  g_AppConfig.notification.display.max_opacity,
                                  TRUE, TRUE);
}

void ShowToastNotificationPreview(HWND hwnd, const wchar_t* message, int opacityPercent) {
    NotificationShowToastInternal(hwnd, message, TRUE, 0, opacityPercent, FALSE, FALSE);
}

void SetToastNotificationOpacity(HWND hwnd, int opacityPercent) {
    if (!NotificationIsWindow(hwnd)) return;

    NotificationData* data = NotificationGetData(hwnd);
    int clampedOpacity = NotificationClampOpacityPercent(opacityPercent);
    BYTE alphaValue = NotificationOpacityPercentToAlpha(opacityPercent);

    if (data) {
        if (data->opacityPercent == clampedOpacity &&
            data->maxOpacity == alphaValue && data->opacity == alphaValue &&
            data->animState == ANIM_VISIBLE) {
            return;
        }
        data->opacityPercent = clampedOpacity;
        data->maxOpacity = alphaValue;
        data->opacity = alphaValue;
        data->animState = ANIM_VISIBLE;
        NotificationRenderWithRecovery(hwnd, data);
    }
}

void SetToastNotificationCornerRadius(HWND hwnd, int cornerRadius) {
    if (!NotificationIsWindow(hwnd)) return;

    NotificationData* data = NotificationGetData(hwnd);
    int clampedRadius = NotificationClampCornerRadius(cornerRadius);
    if (data) {
        if (data->cornerRadius == clampedRadius) return;
        data->cornerRadius = clampedRadius;
        NotificationRenderWithRecovery(hwnd, data);
    }
}

void SetToastNotificationFontPercent(HWND hwnd, int fontPercent) {
    if (!NotificationIsWindow(hwnd)) return;

    NotificationData* data = NotificationGetData(hwnd);
    if (data) {
        int clampedFontPercent = NotificationClampFontPercent(fontPercent);
        if (data->fontPercent == clampedFontPercent) return;
        data->fontPercent = clampedFontPercent;
        NotificationRenderWithRecovery(hwnd, data);
    }
}

void SetToastNotificationAppearance(HWND hwnd, int opacityPercent,
                                    int cornerRadius, int fontPercent) {
    if (!NotificationIsWindow(hwnd)) return;

    NotificationData* data = NotificationGetData(hwnd);
    if (!data) return;

    int clampedOpacity = NotificationClampOpacityPercent(opacityPercent);
    BYTE alphaValue = NotificationOpacityPercentToAlpha(clampedOpacity);
    int clampedRadius = NotificationClampCornerRadius(cornerRadius);
    int clampedFontPercent = NotificationClampFontPercent(fontPercent);
    BOOL changed = data->opacityPercent != clampedOpacity ||
                   data->maxOpacity != alphaValue ||
                   data->opacity != alphaValue ||
                   data->animState != ANIM_VISIBLE ||
                   data->cornerRadius != clampedRadius ||
                   data->fontPercent != clampedFontPercent;
    if (!changed) return;

    data->opacityPercent = clampedOpacity;
    data->maxOpacity = alphaValue;
    data->opacity = alphaValue;
    data->animState = ANIM_VISIBLE;
    data->cornerRadius = clampedRadius;
    data->fontPercent = clampedFontPercent;
    NotificationRenderWithRecovery(hwnd, data);
}

BOOL SetToastNotificationMessage(HWND hwnd, const wchar_t* message) {
    if (!NotificationIsWindow(hwnd) || !message) return FALSE;

    NotificationData* data = NotificationGetData(hwnd);
    if (!data) return FALSE;

    size_t messageLen = wcslen(message);
    if (messageLen > (SIZE_MAX / sizeof(wchar_t)) - 1) {
        return FALSE;
    }
    messageLen++;

    wchar_t* newBuffer = (wchar_t*)realloc(data->messageText, messageLen * sizeof(wchar_t));
    if (!newBuffer) {
        return FALSE;
    }

    data->messageText = newBuffer;
    wcscpy_s(data->messageText, messageLen, message);
    NotificationRenderWithRecovery(hwnd, data);
    return TRUE;
}

void RefreshToastNotificationColors(void) {
    BOOL activeColorIsAnimated = NotificationCurrentColorIsAnimatedGradient();
    HWND hwnd = FindWindowExW(NULL, NULL, NOTIFICATION_CLASS_NAME, NULL);

    while (hwnd) {
        HWND nextHwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, NULL);

        if (NotificationIsWindow(hwnd)) {
            NotificationData* data = NotificationGetData(hwnd);
            if (data) {
                data->hasAnimatedGradient = activeColorIsAnimated;
                NotificationStopGradientTimer(hwnd);
                NotificationStartGradientTimer(hwnd, data);
                NotificationRenderWithRecovery(hwnd, data);
            }
        }

        hwnd = nextHwnd;
    }
}

BOOL IsToastNotificationPreviewWindow(HWND hwnd) {
    if (!NotificationIsWindow(hwnd)) return FALSE;

    const NotificationData* data = NotificationGetData(hwnd);
    return data && data->isPreview;
}

void ShowToastNotification(HWND hwnd, const wchar_t* message) {
    ShowToastNotificationEx(hwnd, message, FALSE);
}
