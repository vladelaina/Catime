/**
 * @file notification.c
 * @brief Multi-modal notifications with fade animations
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "language.h"
#include "notification.h"
#include "notification_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "dialog/dialog_notification.h"
#include "log.h"
#include <windowsx.h>

/** Notification config now in g_AppConfig.notification */

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL RegisterNotificationClass(HINSTANCE hInstance);

static void LoadNotificationConfigs(void);
static void CalculateNotificationPosition(int width, int height, int* x, int* y);
static BYTE UpdateAnimationOpacity(AnimationState state, BYTE currentOpacity, BYTE maxOpacity, BOOL* shouldDestroy);
static NotificationData* GetNotificationData(HWND hwnd);
static void SetNotificationData(HWND hwnd, NotificationData* data);
static void FreeNotificationData(HWND hwnd, NotificationData* data);
static void BeginNotificationFadeOut(HWND hwnd, NotificationData* data);
static void DestroyAllNotifications(void);
static void FlushPendingNotificationOpacity(HWND hwnd, NotificationData* data, BOOL allowRetry);
static int ClampOpacityPercent(int opacityPercent);
static BOOL StartNotificationGradientTimer(HWND hwnd, NotificationData* data);
static void StopNotificationGradientTimer(HWND hwnd);
static BYTE OpacityPercentToAlpha(int opacityPercent);
static void ShowToastNotificationInternal(HWND hwnd, const wchar_t* message,
                                          BOOL isPreview, int timeoutMs,
                                          int opacityPercent, BOOL animate,
                                          BOOL requireTimeout);

#define NOTIFICATION_OPACITY_SAVE_TIMER_ID 1003
#define NOTIFICATION_OPACITY_SAVE_DELAY_MS 300
#define NOTIFICATION_OPACITY_SAVE_MAX_RETRIES 3

static int ClampOpacityPercent(int opacityPercent) {
    if (opacityPercent < MIN_VISIBLE_OPACITY) return MIN_VISIBLE_OPACITY;
    if (opacityPercent > 100) return 100;
    return opacityPercent;
}

static int ClampNotificationWidth(int width) {
    if (width < NOTIFICATION_MIN_WIDTH) return NOTIFICATION_MIN_WIDTH;
    if (width > NOTIFICATION_MAX_WIDTH) return NOTIFICATION_MAX_WIDTH;
    return width;
}

static int ClampNotificationHeight(int height) {
    if (height < NOTIFICATION_MIN_HEIGHT) return NOTIFICATION_MIN_HEIGHT;
    if (height > NOTIFICATION_MAX_HEIGHT) return NOTIFICATION_MAX_HEIGHT;
    return height;
}

static BYTE OpacityPercentToAlpha(int opacityPercent) {
    return (BYTE)((ClampOpacityPercent(opacityPercent) * 255) / 100);
}

static BOOL IsNotificationWindow(HWND hwnd) {
    if (!NotificationIsCurrentProcessWindow(hwnd) || !IsWindow(hwnd)) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, NOTIFICATION_CLASS_NAME) == 0;
}

static void LoadNotificationConfigs(void) {
    /* Config is already loaded via ReadConfig() into g_AppConfig */
}

static BOOL StartNotificationGradientTimer(HWND hwnd, NotificationData* data) {
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

static void StopNotificationGradientTimer(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, NOTIFICATION_GRADIENT_TIMER_ID);
    }
}

void CleanupNotificationResources(void) {
    DestroyAllNotifications();
    NotificationCleanupRenderResources();
}

/** Position in bottom-right of work area */
static void CalculateNotificationPosition(int width, int height, int* x, int* y) {
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

/** Centralized opacity calculation for fade animations */
static BYTE UpdateAnimationOpacity(AnimationState state, BYTE currentOpacity, 
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

static NotificationData* GetNotificationData(HWND hwnd) {
    if (!IsNotificationWindow(hwnd)) {
        return NULL;
    }

    return (NotificationData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static void SetNotificationData(HWND hwnd, NotificationData* data) {
    if (!IsNotificationWindow(hwnd)) {
        return;
    }

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
}

static void FreeNotificationData(HWND hwnd, NotificationData* data) {
    if (!data) return;

    if (hwnd && GetNotificationData(hwnd) == data) {
        SetNotificationData(hwnd, NULL);
    }

    NotificationReleaseRenderBuffers(data);
    free(data->messageText);
    data->messageText = NULL;
    free(data);
}

static void BeginNotificationFadeOut(HWND hwnd, NotificationData* data) {
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

static BOOL TrySavePendingNotificationOpacity(NotificationData* data) {
    if (!data || !data->opacitySavePending) return TRUE;

    if (!WriteConfigNotificationOpacity(data->pendingOpacity)) {
        return FALSE;
    }

    data->opacitySavePending = FALSE;
    data->opacitySaveRetryCount = 0;
    return TRUE;
}

static void FlushPendingNotificationOpacity(HWND hwnd, NotificationData* data, BOOL allowRetry) {
    if (!data || !data->opacitySavePending) return;

    KillTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID);
    if (TrySavePendingNotificationOpacity(data)) {
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

void ShowNotification(HWND hwnd, const wchar_t* message) {
    if (!message) return;
    if (!NotificationGetOwnerWindow(hwnd)) return;

    LoadNotificationConfigs();
    
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

/** Auto-sizes based on text, positioned in bottom-right */
static void ShowToastNotificationInternal(HWND hwnd, const wchar_t* message,
                                          BOOL isPreview, int timeoutMs,
                                          int opacityPercent, BOOL animate,
                                          BOOL requireTimeout) {
    if (!message) return;
    HWND owner = NotificationGetOwnerWindow(hwnd);
    if (!owner) return;

    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(owner, GWLP_HINSTANCE);
    
    LoadNotificationConfigs();
    
    if (g_AppConfig.notification.display.disabled ||
        (requireTimeout && timeoutMs <= 0)) {
        return;
    }
    
    DestroyAllNotifications();
    
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
        CalculateNotificationPosition(notificationWidth, NOTIFICATION_HEIGHT, &x, &y);
    }
    
    /* Use saved size if valid (> 0), otherwise auto-calculate */
    if (g_AppConfig.notification.display.window_width > 0 && 
        g_AppConfig.notification.display.window_height > 0) {
        width = ClampNotificationWidth(g_AppConfig.notification.display.window_width);
        height = ClampNotificationHeight(g_AppConfig.notification.display.window_height);
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
    
    notifData->maxOpacity = OpacityPercentToAlpha(opacityPercent);
    notifData->opacityPercent = ClampOpacityPercent(opacityPercent);
    notifData->animState = animate ? ANIM_FADE_IN : ANIM_VISIBLE;
    notifData->opacity = animate ? 0 : notifData->maxOpacity;
    notifData->cornerRadius = NotificationClampCornerRadius(g_AppConfig.notification.display.corner_radius);
    notifData->hasAnimatedGradient = NotificationCurrentColorIsAnimatedGradient();
    notifData->isPreview = isPreview;  /* Controls interactivity and position saving */
    
    SetNotificationData(hNotification, notifData);
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

    StartNotificationGradientTimer(hNotification, notifData);
}

void ShowToastNotificationEx(HWND hwnd, const wchar_t* message, BOOL isPreview) {
    ShowToastNotificationInternal(hwnd, message, isPreview,
                                  g_AppConfig.notification.display.timeout_ms,
                                  g_AppConfig.notification.display.max_opacity,
                                  TRUE, TRUE);
}

void ShowToastNotificationWithTimeout(HWND hwnd, const wchar_t* message, int timeoutMs) {
    ShowToastNotificationInternal(hwnd, message, FALSE, timeoutMs,
                                  g_AppConfig.notification.display.max_opacity,
                                  TRUE, TRUE);
}

void ShowToastNotificationPreview(HWND hwnd, const wchar_t* message, int opacityPercent) {
    ShowToastNotificationInternal(hwnd, message, TRUE, 0, opacityPercent, FALSE, FALSE);
}

void SetToastNotificationOpacity(HWND hwnd, int opacityPercent) {
    if (!IsNotificationWindow(hwnd)) return;

    NotificationData* data = GetNotificationData(hwnd);
    BYTE alphaValue = OpacityPercentToAlpha(opacityPercent);

    if (data) {
        data->opacityPercent = ClampOpacityPercent(opacityPercent);
        data->maxOpacity = alphaValue;
        data->opacity = alphaValue;
        data->animState = ANIM_VISIBLE;
        NotificationRenderLayeredWindow(hwnd, data);
    }
}

void SetToastNotificationCornerRadius(HWND hwnd, int cornerRadius) {
    if (!IsNotificationWindow(hwnd)) return;

    NotificationData* data = GetNotificationData(hwnd);
    int clampedRadius = NotificationClampCornerRadius(cornerRadius);
    if (data) {
        data->cornerRadius = clampedRadius;
        NotificationRenderLayeredWindow(hwnd, data);
    }
}

void SetToastNotificationFontPercent(HWND hwnd, int fontPercent) {
    if (!IsNotificationWindow(hwnd)) return;

    NotificationData* data = GetNotificationData(hwnd);
    if (data) {
        data->fontPercent = NotificationClampFontPercent(fontPercent);
        NotificationRenderLayeredWindow(hwnd, data);
    }
}

BOOL SetToastNotificationMessage(HWND hwnd, const wchar_t* message) {
    if (!IsNotificationWindow(hwnd) || !message) return FALSE;

    NotificationData* data = GetNotificationData(hwnd);
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
    NotificationRenderLayeredWindow(hwnd, data);
    return TRUE;
}

void RefreshToastNotificationColors(void) {
    BOOL activeColorIsAnimated = NotificationCurrentColorIsAnimatedGradient();
    HWND hwnd = FindWindowExW(NULL, NULL, NOTIFICATION_CLASS_NAME, NULL);

    while (hwnd) {
        HWND nextHwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, NULL);

        if (IsNotificationWindow(hwnd)) {
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                data->hasAnimatedGradient = activeColorIsAnimated;
                StopNotificationGradientTimer(hwnd);
                StartNotificationGradientTimer(hwnd, data);
                NotificationRenderLayeredWindow(hwnd, data);
            }
        }

        hwnd = nextHwnd;
    }
}

BOOL IsToastNotificationPreviewWindow(HWND hwnd) {
    if (!IsNotificationWindow(hwnd)) return FALSE;

    const NotificationData* data = GetNotificationData(hwnd);
    return data && data->isPreview;
}

void ShowToastNotification(HWND hwnd, const wchar_t* message) {
    ShowToastNotificationEx(hwnd, message, FALSE);
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

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            /* The layered-window frame is redrawn as a complete buffer. */
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!hdc) {
                return 0;
            }
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                NotificationRenderLayeredWindow(hwnd, data);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER: {
            NotificationData* data = GetNotificationData(hwnd);
            if (!data) break;
            
            if (wParam == NOTIFICATION_TIMER_ID) {
                BeginNotificationFadeOut(hwnd, data);
                return 0;
            }
            else if (wParam == NOTIFICATION_OPACITY_SAVE_TIMER_ID) {
                FlushPendingNotificationOpacity(hwnd, data, TRUE);
                return 0;
            }
            else if (wParam == NOTIFICATION_GRADIENT_TIMER_ID) {
                if (data->hasAnimatedGradient && !data->isInSizeMove) {
                    NotificationRenderLayeredWindow(hwnd, data);
                }
                return 0;
            }
            else if (wParam == ANIMATION_TIMER_ID) {
                BOOL shouldDestroy = FALSE;
                
                BYTE newOpacity = UpdateAnimationOpacity(data->animState, data->opacity, 
                                                        data->maxOpacity, &shouldDestroy);
                
                if (shouldDestroy) {
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                    DestroyWindow(hwnd);
                    return 0;
                }
                
                data->opacity = newOpacity;
                NotificationRenderLayeredWindow(hwnd, data);
                
                if (data->animState == ANIM_FADE_IN && newOpacity >= data->maxOpacity) {
                    data->animState = ANIM_VISIBLE;
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                }
                
                return 0;
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            const NotificationData* data = GetNotificationData(hwnd);
            if (data && data->isPreview && lParam) {
                MINMAXINFO* minMaxInfo = (MINMAXINFO*)lParam;
                minMaxInfo->ptMinTrackSize.x = NOTIFICATION_MIN_WIDTH;
                minMaxInfo->ptMinTrackSize.y = NOTIFICATION_MIN_HEIGHT;
                minMaxInfo->ptMaxTrackSize.x = NOTIFICATION_MAX_WIDTH;
                minMaxInfo->ptMaxTrackSize.y = NOTIFICATION_MAX_HEIGHT;
                return 0;
            }
            break;
        }
        
        case WM_NCHITTEST: {
            const NotificationData* data = GetNotificationData(hwnd);
            /* Normal notifications are non-interactive */
            if (!data || !data->isPreview) {
                return HTCLIENT;
            }
            
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            
            RECT rect;
            GetWindowRect(hwnd, &rect);
            
            int borderSize = 8;
            int cornerSize = 16;
            
            int relX = pt.x - rect.left;
            int relY = pt.y - rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            BOOL atLeft = (relX < borderSize);
            BOOL atRight = (relX >= width - borderSize);
            BOOL atTop = (relY < borderSize);
            BOOL atBottom = (relY >= height - borderSize);
            
            BOOL atTopLeft = (relX < cornerSize && relY < cornerSize);
            BOOL atTopRight = (relX >= width - cornerSize && relY < cornerSize);
            BOOL atBottomLeft = (relX < cornerSize && relY >= height - cornerSize);
            BOOL atBottomRight = (relX >= width - cornerSize && relY >= height - cornerSize);
            
            if (atTopLeft) return HTTOPLEFT;
            if (atTopRight) return HTTOPRIGHT;
            if (atBottomLeft) return HTBOTTOMLEFT;
            if (atBottomRight) return HTBOTTOMRIGHT;
            if (atTop) return HTTOP;
            if (atBottom) return HTBOTTOM;
            if (atLeft) return HTLEFT;
            if (atRight) return HTRIGHT;
            
            return HTCAPTION;
        }
        
        case WM_SETCURSOR: {
            const NotificationData* data = GetNotificationData(hwnd);
            /* Preview windows show crosshair cursor for dragging */
            if (data && data->isPreview) {
                WORD hitTest = LOWORD(lParam);
                if (hitTest == HTCAPTION) {
                    SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                    return TRUE;
                }
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        
        case WM_LBUTTONDOWN: {
            NotificationData* data = GetNotificationData(hwnd);
            /* Normal notifications: left-click to dismiss */
            if (data && !data->isPreview) {
                BeginNotificationFadeOut(hwnd, data);
            }
            return 0;
        }
        
        case WM_NCLBUTTONDBLCLK: {
            /* Preview windows: double-click title to dismiss */
            if (wParam == HTCAPTION) {
                NotificationData* data = GetNotificationData(hwnd);
                if (data && data->isPreview) {
                    BeginNotificationFadeOut(hwnd, data);
                }
            }
            return 0;
        }

        case WM_ENTERSIZEMOVE: {
            NotificationData* data = GetNotificationData(hwnd);
            if (data && data->isPreview) {
                data->isInSizeMove = TRUE;
                StopNotificationGradientTimer(hwnd);
            }
            return 0;
        }

        case WM_SIZING: {
            return TRUE;
        }

        case WM_SIZE: {
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                NotificationRenderLayeredWindow(hwnd, data);
            }
            return 0;
        }
        
        case WM_EXITSIZEMOVE: {
            NotificationData* data = GetNotificationData(hwnd);
            /* Save position/size only for preview windows */
            if (data && data->isPreview) {
                data->isInSizeMove = FALSE;
                StartNotificationGradientTimer(hwnd, data);

                RECT rect;
                if (GetWindowRect(hwnd, &rect)) {
                    if (!WriteConfigNotificationWindow(rect.left, rect.top,
                                                       rect.right - rect.left,
                                                       rect.bottom - rect.top)) {
                        LOG_WARNING("Failed to save notification preview window placement");
                    }
                }
            }
            if (data) {
                NotificationRenderLayeredWindow(hwnd, data);
            }
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            NotificationData* data = GetNotificationData(hwnd);
            /* Preview windows: right-click to dismiss */
            if (data && data->isPreview) {
                BeginNotificationFadeOut(hwnd, data);
            }
            return 0;
        }
        
        case WM_MOUSEWHEEL: {
            NotificationData* data = GetNotificationData(hwnd);
            /* Preview windows support quick appearance tuning via scroll wheel. */
            if (data && data->isPreview) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                BOOL ctrlDown = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;

                if (ctrlDown) {
                    int currentFontPercent = data->fontPercent;

                    if (delta > 0) {
                        currentFontPercent += 1;
                    } else {
                        currentFontPercent -= 1;
                    }

                    currentFontPercent = NotificationClampFontPercent(currentFontPercent);
                    SetToastNotificationFontPercent(hwnd, currentFontPercent);
                    UpdateNotificationFontPercentControls(currentFontPercent);
                    return 0;
                }

                int currentOpacity = data->opacityPercent;
                int step = 5;
                
                /* Scroll up increases opacity, scroll down decreases */
                if (delta > 0) {
                    currentOpacity += step;
                } else {
                    currentOpacity -= step;
                }
                
                currentOpacity = ClampOpacityPercent(currentOpacity);
                SetToastNotificationOpacity(hwnd, currentOpacity);
                
                /* Sync settings dialog controls if dialog is open */
                UpdateNotificationOpacityControls(currentOpacity);

                data->pendingOpacity = currentOpacity;
                data->opacitySavePending = TRUE;
                data->opacitySaveRetryCount = 0;
                if (SetTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID,
                             NOTIFICATION_OPACITY_SAVE_DELAY_MS, NULL) == 0) {
                    TrySavePendingNotificationOpacity(data);
                }
            }
            return 0;
        }
        
        case WM_DESTROY: {
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                FlushPendingNotificationOpacity(hwnd, data, FALSE);
            }
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            KillTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID);
            KillTimer(hwnd, NOTIFICATION_GRADIENT_TIMER_ID);

            if (data) {
                FreeNotificationData(hwnd, data);
            }
            
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/** Immediately destroy all notification windows */
static void DestroyAllNotifications(void) {
    HWND hwnd = FindWindowExW(NULL, NULL, NOTIFICATION_CLASS_NAME, NULL);

    while (hwnd) {
        HWND nextHwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, NULL);
        if (IsNotificationWindow(hwnd)) {
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
        NotificationData* data = GetNotificationData(hwnd);

        if (data) {
            if (data->animState != ANIM_FADE_OUT) {
                BeginNotificationFadeOut(hwnd, data);
            }
        }

        hwnd = nextHwnd;
    }
}
