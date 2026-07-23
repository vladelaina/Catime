#include "notification_render_internal.h"
#include <stdint.h>

static void ReleaseNotificationPaintBuffer(NotificationData* data) {
    if (!data) return;
    if (data->paintDC && data->oldPaintBitmap)
        SelectObject(data->paintDC, data->oldPaintBitmap);
    if (data->paintBitmap) DeleteObject(data->paintBitmap);
    if (data->paintDC) DeleteDC(data->paintDC);
    data->paintDC = NULL;
    data->paintBitmap = NULL;
    data->oldPaintBitmap = NULL;
    data->paintBits = NULL;
    data->paintWidth = 0;
    data->paintHeight = 0;
}

static void ReleaseNotificationTextMaskBuffer(NotificationData* data) {
    if (!data) return;
    if (data->textMaskDC && data->oldTextMaskBitmap)
        SelectObject(data->textMaskDC, data->oldTextMaskBitmap);
    if (data->textMaskBitmap) DeleteObject(data->textMaskBitmap);
    if (data->textMaskDC) DeleteDC(data->textMaskDC);
    data->textMaskDC = NULL;
    data->textMaskBitmap = NULL;
    data->oldTextMaskBitmap = NULL;
    data->textMaskBits = NULL;
    data->textMaskWidth = 0;
    data->textMaskHeight = 0;
}

void NotificationReleaseRenderBuffers(NotificationData* data) {
    ReleaseNotificationPaintBuffer(data);
    ReleaseNotificationTextMaskBuffer(data);
}

static BOOL CanReuseBuffer(int cachedWidth, int cachedHeight,
                           int width, int height) {
    if (cachedWidth < width || cachedHeight < height) return FALSE;
    size_t requested = (size_t)width * (size_t)height;
    size_t cached = (size_t)cachedWidth * (size_t)cachedHeight;
    return requested > 0 &&
           cached / NOTIFICATION_PAINT_SHRINK_THRESHOLD_MULTIPLIER <= requested;
}

static HBITMAP CreateNotificationDib(HDC hdc, int width, int height,
                                     void** bits) {
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(hdc, &info, DIB_RGB_COLORS, bits, NULL, 0);
}

BOOL EnsureNotificationPaintBuffer(HDC hdc, NotificationData* data,
                                   int width, int height, HDC* outMemDC) {
    if (!hdc || !data || !outMemDC || width <= 0 || height <= 0 ||
        (size_t)width > (size_t)NOTIFICATION_MAX_PAINT_PIXELS / (size_t)height)
        return FALSE;
    if (data->paintDC && CanReuseBuffer(data->paintWidth, data->paintHeight,
                                        width, height)) {
        *outMemDC = data->paintDC;
        return TRUE;
    }
    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) return FALSE;
    void* bits = NULL;
    HBITMAP bitmap = CreateNotificationDib(hdc, width, height, &bits);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);
    if (!oldBitmap) {
        DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }
    ReleaseNotificationPaintBuffer(data);
    data->paintDC = memDC;
    data->paintBitmap = bitmap;
    data->oldPaintBitmap = oldBitmap;
    data->paintBits = bits;
    data->paintWidth = width;
    data->paintHeight = height;
    *outMemDC = memDC;
    return TRUE;
}

BOOL EnsureNotificationTextMaskBuffer(HDC hdc, NotificationData* data,
                                      int width, int height, HDC* outMemDC) {
    if (!hdc || !data || !outMemDC || width <= 0 || height <= 0 ||
        (size_t)width > (size_t)NOTIFICATION_MAX_PAINT_PIXELS / (size_t)height)
        return FALSE;
    if (data->textMaskDC &&
        CanReuseBuffer(data->textMaskWidth, data->textMaskHeight, width, height)) {
        *outMemDC = data->textMaskDC;
        return TRUE;
    }
    HDC maskDC = CreateCompatibleDC(hdc);
    if (!maskDC) return FALSE;
    void* bits = NULL;
    HBITMAP bitmap = CreateNotificationDib(hdc, width, height, &bits);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(maskDC);
        return FALSE;
    }
    HBITMAP oldBitmap = (HBITMAP)SelectObject(maskDC, bitmap);
    if (!oldBitmap) {
        DeleteObject(bitmap);
        DeleteDC(maskDC);
        return FALSE;
    }
    ReleaseNotificationTextMaskBuffer(data);
    data->textMaskDC = maskDC;
    data->textMaskBitmap = bitmap;
    data->oldTextMaskBitmap = oldBitmap;
    data->textMaskBits = bits;
    data->textMaskWidth = width;
    data->textMaskHeight = height;
    *outMemDC = maskDC;
    return TRUE;
}
