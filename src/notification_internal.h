/**
 * @file notification_internal.h
 * @brief Private notification state shared by notification modules
 */
#ifndef CATIME_NOTIFICATION_INTERNAL_H
#define CATIME_NOTIFICATION_INTERNAL_H

#include <windows.h>
#include <wchar.h>
#include "notification.h"
#include "../resource/resource.h"

#define NOTIFICATION_GRADIENT_TIMER_ID 1004
#define NOTIFICATION_GRADIENT_INTERVAL_MS 33
#define NOTIFICATION_GRADIENT_CYCLE_MS 512u
#define NOTIFICATION_MAX_HEIGHT 900
#define NOTIFICATION_MAX_PAINT_PIXELS (NOTIFICATION_MAX_WIDTH * NOTIFICATION_MAX_HEIGHT)
#define NOTIFICATION_MIN_FONT_PIXEL_SIZE 8
#define NOTIFICATION_PAINT_SHRINK_THRESHOLD_MULTIPLIER 4u
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

typedef struct {
    wchar_t* messageText;
    int windowWidth;
    AnimationState animState;
    BYTE opacity;
    BYTE maxOpacity;
    int opacityPercent;
    int cornerRadius;
    int fontPercent;
    BOOL hasAnimatedGradient;
    BOOL isInSizeMove;
    BOOL isPreview;
    BOOL opacitySavePending;
    int pendingOpacity;
    int opacitySaveRetryCount;
    HDC paintDC;
    HBITMAP paintBitmap;
    HBITMAP oldPaintBitmap;
    void* paintBits;
    int paintWidth;
    int paintHeight;
    HDC textMaskDC;
    HBITMAP textMaskBitmap;
    HBITMAP oldTextMaskBitmap;
    void* textMaskBits;
    int textMaskWidth;
    int textMaskHeight;
} NotificationData;

int NotificationClampCornerRadius(int cornerRadius);
int NotificationClampFontPercent(int fontPercent);
int NotificationMeasureTextWidth(HDC hdc, const wchar_t* text,
                                 int windowHeight, int fontPercent);
BOOL NotificationCurrentColorIsAnimatedGradient(void);
BOOL NotificationRenderLayeredWindow(HWND hwnd, NotificationData* data);
void NotificationReleaseRenderBuffers(NotificationData* data);
void NotificationCleanupRenderResources(void);
void NotificationFallbackToTray(HWND hwnd, const wchar_t* message);

static inline BOOL NotificationIsCurrentProcessWindow(HWND hwnd) {
    DWORD processId = 0;
    if (!hwnd) return FALSE;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

static inline HWND NotificationGetOwnerWindow(HWND hwnd) {
    if (!NotificationIsCurrentProcessWindow(hwnd) || !IsWindow(hwnd)) {
        return NULL;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return NULL;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0 ? hwnd : NULL;
}

#endif /* CATIME_NOTIFICATION_INTERNAL_H */
