/**
 * @file notification_window.c
 * @brief Notification window state, animation, and lifecycle helpers.
 */

#include "notification_internal.h"

int NotificationClampOpacityPercent(int opacityPercent) {
    if (opacityPercent < MIN_VISIBLE_OPACITY) return MIN_VISIBLE_OPACITY;
    if (opacityPercent > 100) return 100;
    return opacityPercent;
}

int NotificationClampWindowWidth(int width) {
    if (width < NOTIFICATION_MIN_WIDTH) return NOTIFICATION_MIN_WIDTH;
    if (width > NOTIFICATION_MAX_WIDTH) return NOTIFICATION_MAX_WIDTH;
    return width;
}

int NotificationClampWindowHeight(int height) {
    if (height < NOTIFICATION_MIN_HEIGHT) return NOTIFICATION_MIN_HEIGHT;
    if (height > NOTIFICATION_MAX_HEIGHT) return NOTIFICATION_MAX_HEIGHT;
    return height;
}

BYTE NotificationOpacityPercentToAlpha(int opacityPercent) {
    return (BYTE)((NotificationClampOpacityPercent(opacityPercent) * 255) / 100);
}

BOOL NotificationIsWindow(HWND hwnd) {
    if (!NotificationIsCurrentProcessWindow(hwnd) || !IsWindow(hwnd)) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, NOTIFICATION_CLASS_NAME) == 0;
}

void NotificationLoadConfigs(void) {
    /* Config is already loaded via ReadConfig() into g_AppConfig */
}

BOOL NotificationStartGradientTimer(HWND hwnd, NotificationData* data) {
    if (!hwnd || !data || !data->hasAnimatedGradient || data->isInSizeMove) {
        return FALSE;
    }

    if (SetTimer(hwnd, NOTIFICATION_GRADIENT_TIMER_ID,
                 NOTIFICATION_GRADIENT_INTERVAL_MS, NULL) == 0) {
        data->hasAnimatedGradient = FALSE;
        LOG_WARNING("Failed to start notification gradient animation timer");
        return FALSE;
    }

    return TRUE;
}

void NotificationStopGradientTimer(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, NOTIFICATION_GRADIENT_TIMER_ID);
    }
}

void CleanupNotificationResources(void) {
    NotificationDestroyAll();
    NotificationCleanupRenderResources();
}

/** Position in bottom-right of work area */
void NotificationCalculatePosition(int width, int height, int* x, int* y) {
    if (!x || !y) return;

    RECT workArea = {0};
    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0)) {
        workArea.right = GetSystemMetrics(SM_CXSCREEN);
        workArea.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    if (workArea.right <= workArea.left || workArea.bottom <= workArea.top) {
        *x = 0;
        *y = 0;
        return;
    }

    *x = workArea.right - width - NOTIFICATION_RIGHT_MARGIN;
    *y = workArea.bottom - height - NOTIFICATION_BOTTOM_MARGIN;
}

BOOL NotificationConstrainPosition(int width, int height, int* x, int* y) {
    if (width <= 0 || height <= 0 || !x || !y) return FALSE;

    POINT origin = {*x, *y};
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    RECT bounds = {0};

    if (monitor && GetMonitorInfoW(monitor, &info)) {
        bounds = info.rcWork;
        if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
            bounds = info.rcMonitor;
        }
    }

    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &bounds, 0) ||
            bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
            bounds.left = 0;
            bounds.top = 0;
            bounds.right = GetSystemMetrics(SM_CXSCREEN);
            bounds.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    return WindowPlacement_ClampFullyVisible(
        &bounds, width, height, x, y);
}

/** Centralized opacity calculation for fade animations */
BYTE NotificationUpdateAnimationOpacity(AnimationState state, BYTE currentOpacity,
                                   BYTE maxOpacity, BOOL* shouldDestroy) {
    *shouldDestroy = FALSE;

    switch (state) {
        case ANIM_FADE_IN:
            if (currentOpacity >= maxOpacity - ANIMATION_STEP) {
                return maxOpacity;
            }
            return currentOpacity + ANIMATION_STEP;

        case ANIM_FADE_OUT:
            if (currentOpacity <= ANIMATION_STEP) {
                *shouldDestroy = TRUE;
                return 0;
            }
            return currentOpacity - ANIMATION_STEP;

        case ANIM_VISIBLE:
        default:
            return currentOpacity;
    }
}

NotificationData* NotificationGetData(HWND hwnd) {
    if (!NotificationIsWindow(hwnd)) {
        return NULL;
    }

    return (NotificationData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

void NotificationSetData(HWND hwnd, NotificationData* data) {
    if (!NotificationIsWindow(hwnd)) {
        return;
    }

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
}

static void ResetNotificationRenderRetry(HWND hwnd, NotificationData* data) {
    if (hwnd) {
        KillTimer(hwnd, NOTIFICATION_RENDER_RETRY_TIMER_ID);
    }
    if (data) {
        RenderRetry_Reset(&data->renderRetry);
    }
}

static BOOL ArmNotificationRenderRetry(HWND hwnd, NotificationData* data,
                                       UINT delayMs) {
    if (!hwnd || !data || !NotificationIsWindow(hwnd)) return FALSE;
    if (RenderRetry_IsTimerArmed(&data->renderRetry)) return TRUE;

    if (SetTimer(hwnd, NOTIFICATION_RENDER_RETRY_TIMER_ID,
                 delayMs > 0 ? delayMs : 1u, NULL) == 0) {
        LOG_WARNING("Failed to schedule notification render retry (delay=%u, error=%lu)",
                    delayMs, GetLastError());
        return FALSE;
    }

    RenderRetry_MarkTimerArmed(&data->renderRetry);
    return TRUE;
}

BOOL NotificationRenderWithRecovery(HWND hwnd, NotificationData* data) {
    if (!hwnd || !data) return FALSE;

    if (NotificationRenderLayeredWindow(hwnd, data)) {
        ResetNotificationRenderRetry(hwnd, data);
        return TRUE;
    }

    UINT delay = RenderRetry_RecordFailure(&data->renderRetry,
                                           NOTIFICATION_RENDER_RETRY_BASE_MS,
                                           NOTIFICATION_RENDER_RETRY_MAX_MS);
    ArmNotificationRenderRetry(hwnd, data, delay);
    return FALSE;
}

void NotificationFreeData(HWND hwnd, NotificationData* data) {
    if (!data) return;

    ResetNotificationRenderRetry(hwnd, data);

    if (hwnd && NotificationGetData(hwnd) == data) {
        NotificationSetData(hwnd, NULL);
    }

    NotificationReleaseRenderBuffers(data);
    free(data->messageText);
    data->messageText = NULL;
    free(data);
}

void NotificationBeginFadeOut(HWND hwnd, NotificationData* data) {
    if (!data) {
        DestroyWindow(hwnd);
        return;
    }

    KillTimer(hwnd, NOTIFICATION_TIMER_ID);
    data->animState = ANIM_FADE_OUT;

    if (SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL) == 0) {
        DestroyWindow(hwnd);
    }
}

BOOL NotificationTrySavePendingOpacity(NotificationData* data) {
    if (!data || !data->opacitySavePending) return TRUE;

    if (!WriteConfigNotificationOpacity(data->pendingOpacity)) {
        return FALSE;
    }

    data->opacitySavePending = FALSE;
    data->opacitySaveRetryCount = 0;
    return TRUE;
}

void NotificationFlushPendingOpacity(HWND hwnd, NotificationData* data, BOOL allowRetry) {
    if (!data || !data->opacitySavePending) return;

    KillTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID);
    if (NotificationTrySavePendingOpacity(data)) {
        return;
    }

    if (allowRetry &&
        data->opacitySaveRetryCount < NOTIFICATION_OPACITY_SAVE_MAX_RETRIES) {
        data->opacitySaveRetryCount++;
        if (SetTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID,
                     NOTIFICATION_OPACITY_SAVE_DELAY_MS, NULL) != 0) {
            return;
        }
    }

    data->opacitySavePending = FALSE;
    data->opacitySaveRetryCount = 0;
}

/** Immediately destroy all notification windows */
void NotificationDestroyAll(void) {
    HWND hwnd = FindWindowExW(NULL, NULL, NOTIFICATION_CLASS_NAME, NULL);

    while (hwnd) {
        HWND nextHwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, NULL);
        if (NotificationIsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
        hwnd = nextHwnd;
    }
}

/** Trigger fade-out for all visible notifications */
void CloseAllNotifications(void) {
    HWND hwnd = FindWindowExW(NULL, NULL, NOTIFICATION_CLASS_NAME, NULL);

    while (hwnd) {
        HWND nextHwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, NULL);
        NotificationData* data = NotificationGetData(hwnd);

        if (data) {
            if (data->animState != ANIM_FADE_OUT) {
                NotificationBeginFadeOut(hwnd, data);
            }
        }

        hwnd = nextHwnd;
    }
}
