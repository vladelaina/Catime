/**
 * @file notification_create.c
 * @brief Creation and registration of layered notification windows.
 */

#include "notification_internal.h"

static BOOL RegisterNotificationClass(HINSTANCE hInstance);

/** Auto-sizes based on text, positioned in bottom-right */
void NotificationShowToastInternal(HWND hwnd, const wchar_t* message,
                                          BOOL isPreview, int timeoutMs,
                                          int opacityPercent, BOOL animate,
                                          BOOL requireTimeout) {
    if (!message) return;
    HWND owner = NotificationGetOwnerWindow(hwnd);
    if (!owner) return;

    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(owner, GWLP_HINSTANCE);

    NotificationLoadConfigs();

    if (g_AppConfig.notification.display.disabled ||
        (requireTimeout && timeoutMs <= 0)) {
        return;
    }

    NotificationDestroyAll();

    if (!isClassRegistered) {
        isClassRegistered = RegisterNotificationClass(hInstance);
        if (!isClassRegistered) {
            NotificationFallbackToTray(owner, message);
            return;
        }
    }

    NotificationData* notifData = (NotificationData*)calloc(1, sizeof(NotificationData));
    if (!notifData) {
        NotificationFallbackToTray(owner, message);
        return;
    }

    size_t messageLen = wcslen(message) + 1;
    if (messageLen > SIZE_MAX / sizeof(wchar_t)) {
        free(notifData);
        NotificationFallbackToTray(owner, message);
        return;
    }
    notifData->messageText = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!notifData->messageText) {
        free(notifData);
        NotificationFallbackToTray(owner, message);
        return;
    }
    wcscpy_s(notifData->messageText, messageLen, message);

    int notificationFontPercent = NotificationClampFontPercent(g_AppConfig.notification.display.font_size);
    notifData->fontPercent = notificationFontPercent;

    HDC hdc = GetDC(owner);
    if (!hdc) {
        free(notifData->messageText);
        free(notifData);
        NotificationFallbackToTray(owner, message);
        return;
    }
    int textWidth = NotificationMeasureTextWidth(hdc, message,
                                                 NOTIFICATION_HEIGHT,
                                                 notificationFontPercent);

    int notificationWidth = textWidth + NOTIFICATION_TEXT_PADDING;

    if (notificationWidth < NOTIFICATION_MIN_WIDTH)
        notificationWidth = NOTIFICATION_MIN_WIDTH;
    if (notificationWidth > NOTIFICATION_MAX_WIDTH)
        notificationWidth = NOTIFICATION_MAX_WIDTH;

    ReleaseDC(owner, hdc);

    notifData->windowWidth = notificationWidth;

    int x, y, width, height;

    /* Use saved position if valid (>= 0), otherwise auto-calculate */
    if (g_AppConfig.notification.display.window_x >= 0 &&
        g_AppConfig.notification.display.window_y >= 0) {
        x = g_AppConfig.notification.display.window_x;
        y = g_AppConfig.notification.display.window_y;
    } else {
        NotificationCalculatePosition(notificationWidth, NOTIFICATION_HEIGHT, &x, &y);
    }

    /* Use saved size if valid (> 0), otherwise auto-calculate */
    if (g_AppConfig.notification.display.window_width > 0 &&
        g_AppConfig.notification.display.window_height > 0) {
        width = NotificationClampWindowWidth(g_AppConfig.notification.display.window_width);
        height = NotificationClampWindowHeight(g_AppConfig.notification.display.window_height);
    } else {
        width = notificationWidth;
        height = NOTIFICATION_HEIGHT;
    }

    HWND hNotification = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        NOTIFICATION_CLASS_NAME,
        L"Catime Notification",
        WS_POPUP,
        x, y,
        width, height,
        NULL, NULL, hInstance, NULL
    );

    if (!hNotification) {
        free(notifData->messageText);
        free(notifData);
        NotificationFallbackToTray(owner, message);
        return;
    }

    notifData->maxOpacity = NotificationOpacityPercentToAlpha(opacityPercent);
    notifData->opacityPercent = NotificationClampOpacityPercent(opacityPercent);
    notifData->animState = animate ? ANIM_FADE_IN : ANIM_VISIBLE;
    notifData->opacity = animate ? 0 : notifData->maxOpacity;
    notifData->cornerRadius = NotificationClampCornerRadius(g_AppConfig.notification.display.corner_radius);
    notifData->hasAnimatedGradient = NotificationCurrentColorIsAnimatedGradient();
    notifData->isPreview = isPreview;  /* Controls interactivity and position saving */

    NotificationSetData(hNotification, notifData);
    if (!NotificationRenderLayeredWindow(hNotification, notifData)) {
        DestroyWindow(hNotification);
        NotificationFallbackToTray(owner, message);
        return;
    }

    ShowWindow(hNotification, SW_SHOWNOACTIVATE);

    if (animate && SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL) == 0) {
        DestroyWindow(hNotification);
        NotificationFallbackToTray(owner, message);
        return;
    }

    if (timeoutMs > 0 && SetTimer(hNotification, NOTIFICATION_TIMER_ID, timeoutMs, NULL) == 0) {
        DestroyWindow(hNotification);
        NotificationFallbackToTray(owner, message);
        return;
    }

    NotificationStartGradientTimer(hNotification, notifData);
}

static BOOL RegisterNotificationClass(HINSTANCE hInstance) {
    if (!hInstance) {
        hInstance = GetModuleHandleW(NULL);
    }
    if (!hInstance) {
        return FALSE;
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = NotificationWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = NOTIFICATION_CLASS_NAME;
    wc.style = CS_DBLCLKS;

    if (RegisterClassExW(&wc) != 0) {
        return TRUE;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
