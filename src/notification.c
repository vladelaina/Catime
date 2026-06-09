/**
 * @file notification.c
 * @brief Multi-modal notifications with fade animations
 */
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include "tray/tray.h"
#include "language.h"
#include "notification.h"
#include "config.h"
#include "dialog/dialog_notification.h"
#include "log.h"
#include "../resource/resource.h"
#include <windowsx.h>

/** Notification config now in g_AppConfig.notification */

/** Type-safe replacement for SetPropW/GetPropW */
typedef struct {
    wchar_t* messageText;
    int windowWidth;
    AnimationState animState;
    BYTE opacity;
    BYTE maxOpacity;
    int opacityPercent;
    BOOL isPreview;
    BOOL opacitySavePending;
    int pendingOpacity;
    int opacitySaveRetryCount;
    HDC paintDC;
    HBITMAP paintBitmap;
    HBITMAP oldPaintBitmap;
    int paintWidth;
    int paintHeight;
} NotificationData;

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL RegisterNotificationClass(HINSTANCE hInstance);

static void FallbackToTrayNotification(HWND hwnd, const wchar_t* message);
static void LoadNotificationConfigs(void);
static HFONT CreateNotificationFont(int size, int weight);
static HFONT GetNotificationTitleFont(void);
static HFONT GetNotificationContentFont(void);
static HBRUSH GetNotificationBgBrush(void);
static HPEN GetNotificationBorderPen(void);
static int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font);
static void CalculateNotificationPosition(int width, int height, int* x, int* y);
static void DrawNotificationBorder(HDC hdc, RECT rect);
static void DrawNotificationText(HDC memDC, const wchar_t* text, RECT rect, HFONT font, COLORREF color, DWORD flags);
static BYTE UpdateAnimationOpacity(AnimationState state, BYTE currentOpacity, BYTE maxOpacity, BOOL* shouldDestroy);
static NotificationData* GetNotificationData(HWND hwnd);
static void SetNotificationData(HWND hwnd, NotificationData* data);
static void FreeNotificationData(HWND hwnd, NotificationData* data);
static void BeginNotificationFadeOut(HWND hwnd, NotificationData* data);
static void ReleaseNotificationPaintBuffer(NotificationData* data);
static BOOL EnsureNotificationPaintBuffer(HDC hdc, NotificationData* data,
                                          int width, int height, HDC* outMemDC);
static void DestroyAllNotifications(void);
static void FlushPendingNotificationOpacity(HWND hwnd, NotificationData* data, BOOL allowRetry);
static int ClampOpacityPercent(int opacityPercent);
static BYTE OpacityPercentToAlpha(int opacityPercent);
static BOOL IsCurrentProcessWindow(HWND hwnd);
static void ShowToastNotificationInternal(HWND hwnd, const wchar_t* message,
                                          BOOL isPreview, int timeoutMs,
                                          int opacityPercent, BOOL animate,
                                          BOOL requireTimeout);

#define NOTIFICATION_OPACITY_SAVE_TIMER_ID 1003
#define NOTIFICATION_OPACITY_SAVE_DELAY_MS 300
#define NOTIFICATION_OPACITY_SAVE_MAX_RETRIES 3
#define NOTIFICATION_MAX_HEIGHT 600
#define NOTIFICATION_MAX_PAINT_PIXELS (NOTIFICATION_MAX_WIDTH * NOTIFICATION_MAX_HEIGHT)
#define NOTIFICATION_PAINT_SHRINK_THRESHOLD_MULTIPLIER 4u
#define MODAL_NOTIFICATION_START_FAILURE_COOLDOWN_MS 2000
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

typedef struct {
    HWND hwnd;
    wchar_t* message;
} DialogThreadParams;

static HFONT g_notificationTitleFont = NULL;
static HFONT g_notificationContentFont = NULL;
static HBRUSH g_notificationBgBrush = NULL;
static HPEN g_notificationBorderPen = NULL;
static SRWLOCK g_notificationResourceLock = SRWLOCK_INIT;
static volatile LONG g_modalNotificationActive = 0;
static DWORD g_modalNotificationStartFailureCooldownUntil = 0;

static int ClampOpacityPercent(int opacityPercent) {
    if (opacityPercent < 1) return 1;
    if (opacityPercent > 100) return 100;
    return opacityPercent;
}

static int ClampNotificationWidth(int width) {
    if (width < NOTIFICATION_MIN_WIDTH) return NOTIFICATION_MIN_WIDTH;
    if (width > NOTIFICATION_MAX_WIDTH) return NOTIFICATION_MAX_WIDTH;
    return width;
}

static int ClampNotificationHeight(int height) {
    if (height < NOTIFICATION_HEIGHT) return NOTIFICATION_HEIGHT;
    if (height > NOTIFICATION_MAX_HEIGHT) return NOTIFICATION_MAX_HEIGHT;
    return height;
}

static BYTE OpacityPercentToAlpha(int opacityPercent) {
    return (BYTE)((ClampOpacityPercent(opacityPercent) * 255) / 100);
}

static BOOL IsCurrentProcessWindow(HWND hwnd) {
    DWORD processId = 0;
    if (!hwnd) return FALSE;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

static BOOL IsModalNotificationStartFailureCoolingDown(DWORD now) {
    return g_modalNotificationStartFailureCooldownUntil != 0 &&
           (LONG)(g_modalNotificationStartFailureCooldownUntil - now) > 0;
}

static void MarkModalNotificationStartFailure(DWORD now) {
    DWORD cooldownUntil = now + MODAL_NOTIFICATION_START_FAILURE_COOLDOWN_MS;
    g_modalNotificationStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

static BOOL IsNotificationWindow(HWND hwnd) {
    if (!IsCurrentProcessWindow(hwnd) || !IsWindow(hwnd)) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, NOTIFICATION_CLASS_NAME) == 0;
}

static BOOL IsValidNotificationOwnerWindow(HWND hwnd) {
    if (!IsCurrentProcessWindow(hwnd) || !IsWindow(hwnd)) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static HWND GetNotificationOwnerWindow(HWND hwnd) {
    return IsValidNotificationOwnerWindow(hwnd) ? hwnd : NULL;
}

/** Universal fallback for all notification failures */
static void FallbackToTrayNotification(HWND hwnd, const wchar_t* message) {
    if (!message) message = L"";
    HWND owner = GetNotificationOwnerWindow(hwnd);
    if (!owner) return;

    wchar_t boundedMessage[sizeof(((NOTIFYICONDATAW*)0)->szInfo) / sizeof(wchar_t)] = {0};
    wcsncpy_s(boundedMessage, _countof(boundedMessage), message, _TRUNCATE);

    int len = WideCharToMultiByte(CP_UTF8, 0, boundedMessage, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        char* ansiMessage = (char*)malloc(len);
        if (ansiMessage) {
            WideCharToMultiByte(CP_UTF8, 0, boundedMessage, -1, ansiMessage, len, NULL, NULL);
            ShowTrayNotification(owner, ansiMessage);
            free(ansiMessage);
        }
    }
}

static void LoadNotificationConfigs(void) {
    /* Config is already loaded via ReadConfig() into g_AppConfig */
}

static HFONT CreateNotificationFont(int size, int weight) {
    return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                      NOTIFICATION_FONT_NAME);
}

static HFONT GetNotificationTitleFont(void) {
    if (!g_notificationTitleFont) {
        g_notificationTitleFont = CreateNotificationFont(NOTIFICATION_TITLE_FONT_SIZE, FW_BOLD);
    }
    return g_notificationTitleFont;
}

static HFONT GetNotificationContentFont(void) {
    if (!g_notificationContentFont) {
        g_notificationContentFont = CreateNotificationFont(NOTIFICATION_CONTENT_FONT_SIZE, FW_NORMAL);
    }
    return g_notificationContentFont;
}

static HBRUSH GetNotificationBgBrush(void) {
    if (!g_notificationBgBrush) {
        g_notificationBgBrush = CreateSolidBrush(NOTIFICATION_BG_COLOR);
    }
    return g_notificationBgBrush;
}

static HPEN GetNotificationBorderPen(void) {
    if (!g_notificationBorderPen) {
        g_notificationBorderPen = CreatePen(PS_SOLID, 1, NOTIFICATION_BORDER_COLOR);
    }
    return g_notificationBorderPen;
}

void CleanupNotificationResources(void) {
    DestroyAllNotifications();

    AcquireSRWLockExclusive(&g_notificationResourceLock);
    if (g_notificationTitleFont) {
        DeleteObject(g_notificationTitleFont);
        g_notificationTitleFont = NULL;
    }
    if (g_notificationContentFont) {
        DeleteObject(g_notificationContentFont);
        g_notificationContentFont = NULL;
    }
    if (g_notificationBgBrush) {
        DeleteObject(g_notificationBgBrush);
        g_notificationBgBrush = NULL;
    }
    if (g_notificationBorderPen) {
        DeleteObject(g_notificationBorderPen);
        g_notificationBorderPen = NULL;
    }
    ReleaseSRWLockExclusive(&g_notificationResourceLock);
}

static int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font) {
    HFONT oldFont = NULL;
    if (font) {
        oldFont = (HFONT)SelectObject(hdc, font);
    }

    SIZE textSize = {0};
    if (!GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &textSize)) {
        textSize.cx = 0;
    }

    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
    return textSize.cx;
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

static void DrawNotificationBorder(HDC hdc, RECT rect) {
    HPEN pen = GetNotificationBorderPen();
    HPEN oldPen = pen ? (HPEN)SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

    if (oldBrush) {
        SelectObject(hdc, oldBrush);
    }
    if (oldPen) {
        SelectObject(hdc, oldPen);
    }
}

static void DrawNotificationText(HDC memDC, const wchar_t* text, RECT rect, 
                                 HFONT font, COLORREF color, DWORD flags) {
    HFONT oldFont = NULL;
    if (font) {
        oldFont = (HFONT)SelectObject(memDC, font);
    }
    SetTextColor(memDC, color);
    DrawTextW(memDC, text, -1, &rect, flags);
    if (oldFont) {
        SelectObject(memDC, oldFont);
    }
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

    ReleaseNotificationPaintBuffer(data);
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

static void FlushPendingNotificationOpacity(HWND hwnd, NotificationData* data, BOOL allowRetry) {
    if (!data || !data->opacitySavePending) return;

    KillTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID);
    if (WriteConfigNotificationOpacity(data->pendingOpacity)) {
        data->opacitySavePending = FALSE;
        data->opacitySaveRetryCount = 0;
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

static void ReleaseNotificationPaintBuffer(NotificationData* data) {
    if (!data) return;

    if (data->paintDC && data->oldPaintBitmap) {
        SelectObject(data->paintDC, data->oldPaintBitmap);
    }
    if (data->paintBitmap) {
        DeleteObject(data->paintBitmap);
    }
    if (data->paintDC) {
        DeleteDC(data->paintDC);
    }

    data->paintDC = NULL;
    data->paintBitmap = NULL;
    data->oldPaintBitmap = NULL;
    data->paintWidth = 0;
    data->paintHeight = 0;
}

static BOOL EnsureNotificationPaintBuffer(HDC hdc, NotificationData* data,
                                          int width, int height, HDC* outMemDC) {
    if (!hdc || !data || !outMemDC || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > (size_t)NOTIFICATION_MAX_PAINT_PIXELS / (size_t)height) {
        ReleaseNotificationPaintBuffer(data);
        return FALSE;
    }

    if (data->paintDC && data->paintWidth >= width && data->paintHeight >= height) {
        size_t requestedPixels = (size_t)width * (size_t)height;
        size_t cachedPixels = (size_t)data->paintWidth * (size_t)data->paintHeight;
        if (requestedPixels > 0 &&
            cachedPixels / NOTIFICATION_PAINT_SHRINK_THRESHOLD_MULTIPLIER <= requestedPixels) {
            *outMemDC = data->paintDC;
            return TRUE;
        }
    }

    if (data->paintDC &&
        data->paintWidth == width &&
        data->paintHeight == height) {
        *outMemDC = data->paintDC;
        return TRUE;
    }

    ReleaseNotificationPaintBuffer(data);

    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) {
        return FALSE;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!bitmap) {
        DeleteDC(memDC);
        return FALSE;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);
    if (!oldBitmap) {
        DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }

    data->paintDC = memDC;
    data->paintBitmap = bitmap;
    data->oldPaintBitmap = oldBitmap;
    data->paintWidth = width;
    data->paintHeight = height;

    *outMemDC = memDC;
    return TRUE;
}

/** Fallback chain: toast → modal → tray */
void ShowNotification(HWND hwnd, const wchar_t* message) {
    if (!message) return;
    if (!GetNotificationOwnerWindow(hwnd)) return;

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
            FallbackToTrayNotification(hwnd, message);
            break;
        default:
            ShowToastNotification(hwnd, message);
            break;
    }
}

static DWORD WINAPI ShowModalDialogThread(LPVOID lpParam) {
    DialogThreadParams* params = (DialogThreadParams*)lpParam;
    HWND owner = GetNotificationOwnerWindow(params->hwnd);
    if (owner) {
        MessageBoxW(owner, params->message, L"Catime", MB_OK);
    }
    
    free(params->message);
    free(params);
    InterlockedExchange(&g_modalNotificationActive, 0);
    
    return 0;
}

/** Background thread avoids blocking main UI */
void ShowModalNotification(HWND hwnd, const wchar_t* message) {
    if (!message) return;
    HWND owner = GetNotificationOwnerWindow(hwnd);
    if (!owner) return;

    DWORD now = GetTickCount();
    if (IsModalNotificationStartFailureCoolingDown(now)) {
        FallbackToTrayNotification(owner, message);
        return;
    }

    if (InterlockedCompareExchange(&g_modalNotificationActive, 1, 0) != 0) {
        FallbackToTrayNotification(owner, message);
        return;
    }

    DialogThreadParams* params = (DialogThreadParams*)malloc(sizeof(DialogThreadParams));
    if (!params) {
        MarkModalNotificationStartFailure(now);
        InterlockedExchange(&g_modalNotificationActive, 0);
        FallbackToTrayNotification(owner, message);
        return;
    }
    
    size_t messageLen = wcslen(message) + 1;
    if (messageLen > SIZE_MAX / sizeof(wchar_t)) {
        free(params);
        MarkModalNotificationStartFailure(now);
        InterlockedExchange(&g_modalNotificationActive, 0);
        FallbackToTrayNotification(owner, message);
        return;
    }
    params->message = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!params->message) {
        free(params);
        MarkModalNotificationStartFailure(now);
        InterlockedExchange(&g_modalNotificationActive, 0);
        FallbackToTrayNotification(owner, message);
        return;
    }
    
    params->hwnd = owner;
    wcscpy_s(params->message, messageLen, message);
    
    HANDLE hThread = CreateThread(NULL, 0, ShowModalDialogThread, params, 0, NULL);
    
    if (hThread == NULL) {
        free(params->message);
        free(params);
        MarkModalNotificationStartFailure(now);
        InterlockedExchange(&g_modalNotificationActive, 0);
        MessageBeep(MB_OK);
        FallbackToTrayNotification(owner, message);
        return;
    }
    
    g_modalNotificationStartFailureCooldownUntil = 0;
    CloseHandle(hThread);
}

/** Auto-sizes based on text, positioned in bottom-right */
static void ShowToastNotificationInternal(HWND hwnd, const wchar_t* message,
                                          BOOL isPreview, int timeoutMs,
                                          int opacityPercent, BOOL animate,
                                          BOOL requireTimeout) {
    if (!message) return;
    HWND owner = GetNotificationOwnerWindow(hwnd);
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
            FallbackToTrayNotification(owner, message);
            return;
        }
    }
    
    NotificationData* notifData = (NotificationData*)calloc(1, sizeof(NotificationData));
    if (!notifData) {
        FallbackToTrayNotification(owner, message);
        return;
    }
    
    size_t messageLen = wcslen(message) + 1;
    if (messageLen > SIZE_MAX / sizeof(wchar_t)) {
        free(notifData);
        FallbackToTrayNotification(owner, message);
        return;
    }
    notifData->messageText = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!notifData->messageText) {
        free(notifData);
        FallbackToTrayNotification(owner, message);
        return;
    }
    wcscpy_s(notifData->messageText, messageLen, message);
    
    HDC hdc = GetDC(owner);
    if (!hdc) {
        free(notifData->messageText);
        free(notifData);
        FallbackToTrayNotification(owner, message);
        return;
    }
    AcquireSRWLockExclusive(&g_notificationResourceLock);
    HFONT contentFont = GetNotificationContentFont();
    int textWidth = CalculateTextWidth(hdc, message, contentFont);
    ReleaseSRWLockExclusive(&g_notificationResourceLock);

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
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_COMPOSITED,
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
        FallbackToTrayNotification(owner, message);
        return;
    }
    
    notifData->maxOpacity = OpacityPercentToAlpha(opacityPercent);
    notifData->opacityPercent = ClampOpacityPercent(opacityPercent);
    notifData->animState = animate ? ANIM_FADE_IN : ANIM_VISIBLE;
    notifData->opacity = animate ? 0 : notifData->maxOpacity;
    notifData->isPreview = isPreview;  /* Controls interactivity and position saving */
    
    SetNotificationData(hNotification, notifData);
    
    SetLayeredWindowAttributes(hNotification, 0, notifData->opacity, LWA_ALPHA);
    
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    
    if (animate && SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL) == 0) {
        DestroyWindow(hNotification);
        FallbackToTrayNotification(owner, message);
        return;
    }

    if (timeoutMs > 0 && SetTimer(hNotification, NOTIFICATION_TIMER_ID, timeoutMs, NULL) == 0) {
        DestroyWindow(hNotification);
        FallbackToTrayNotification(owner, message);
        return;
    }
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
    }

    SetLayeredWindowAttributes(hwnd, 0, alphaValue, LWA_ALPHA);
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
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    return TRUE;
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
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!hdc) {
                return 0;
            }

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            HFONT titleFont = NULL;
            HFONT contentFont = NULL;

            int paintWidth = clientRect.right - clientRect.left;
            int paintHeight = clientRect.bottom - clientRect.top;
            if (paintWidth <= 0 || paintHeight <= 0) {
                EndPaint(hwnd, &ps);
                return 0;
            }

            NotificationData* data = GetNotificationData(hwnd);
            HDC memDC = NULL;
            if (!EnsureNotificationPaintBuffer(hdc, data, paintWidth, paintHeight, &memDC)) {
                EndPaint(hwnd, &ps);
                return 0;
            }

            AcquireSRWLockExclusive(&g_notificationResourceLock);

            HBRUSH bgBrush = GetNotificationBgBrush();
            FillRect(memDC, &clientRect, bgBrush ? bgBrush : (HBRUSH)(COLOR_WINDOW + 1));

            DrawNotificationBorder(memDC, clientRect);

            SetBkMode(memDC, TRANSPARENT);

            titleFont = GetNotificationTitleFont();
            contentFont = GetNotificationContentFont();

            RECT titleRect = {
                NOTIFICATION_PADDING_H, 
                NOTIFICATION_PADDING_V, 
                clientRect.right - NOTIFICATION_PADDING_H, 
                NOTIFICATION_PADDING_V + NOTIFICATION_TITLE_HEIGHT
            };
            DrawNotificationText(memDC, L"Catime", titleRect, titleFont, 
                               NOTIFICATION_TITLE_COLOR, DT_SINGLELINE);

            if (data && data->messageText) {
                RECT textRect = {
                    NOTIFICATION_PADDING_H, 
                    NOTIFICATION_CONTENT_SPACING, 
                    clientRect.right - NOTIFICATION_PADDING_H, 
                    clientRect.bottom - NOTIFICATION_PADDING_V
                };
                DrawNotificationText(memDC, data->messageText, textRect, contentFont,
                                   NOTIFICATION_CONTENT_COLOR, DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            ReleaseSRWLockExclusive(&g_notificationResourceLock);

            BitBlt(hdc, 0, 0, paintWidth, paintHeight, memDC, 0, 0, SRCCOPY);

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
                SetLayeredWindowAttributes(hwnd, 0, newOpacity, LWA_ALPHA);
                
                if (data->animState == ANIM_FADE_IN && newOpacity >= data->maxOpacity) {
                    data->animState = ANIM_VISIBLE;
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                }
                
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
        
        case WM_SIZING: {
            InvalidateRect(hwnd, NULL, FALSE);
            return TRUE;
        }
        
        case WM_EXITSIZEMOVE: {
            const NotificationData* data = GetNotificationData(hwnd);
            /* Save position/size only for preview windows */
            if (data && data->isPreview) {
                RECT rect;
                if (GetWindowRect(hwnd, &rect)) {
                    if (!WriteConfigNotificationWindow(rect.left, rect.top,
                                                       rect.right - rect.left,
                                                       rect.bottom - rect.top)) {
                        LOG_WARNING("Failed to save notification preview window placement");
                    }
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
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
            /* Only preview windows support opacity adjustment via scroll wheel */
            if (data && data->isPreview) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
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
                KillTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID);
                if (SetTimer(hwnd, NOTIFICATION_OPACITY_SAVE_TIMER_ID,
                             NOTIFICATION_OPACITY_SAVE_DELAY_MS, NULL) == 0) {
                    FlushPendingNotificationOpacity(hwnd, data, TRUE);
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
