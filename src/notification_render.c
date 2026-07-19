/**
 * @file notification_render.c
 * @brief Rendering helpers for custom toast notification windows
 */
#include "notification_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "color/color_state.h"
#include "color/gradient.h"
#include "config/config_defaults.h"
#include "menu_preview.h"

static HFONT g_notificationContentFont = NULL;
static int g_notificationContentFontSize = 0;
static HBRUSH g_notificationBgBrush = NULL;
static SRWLOCK g_notificationResourceLock = SRWLOCK_INIT;

static BOOL EnsureNotificationTextMaskBuffer(HDC hdc, NotificationData* data,
                                             int width, int height, HDC* outMemDC);

int NotificationClampCornerRadius(int cornerRadius) {
    if (cornerRadius < MIN_NOTIFICATION_CORNER_RADIUS) {
        return MIN_NOTIFICATION_CORNER_RADIUS;
    }
    if (cornerRadius > MAX_NOTIFICATION_CORNER_RADIUS) {
        return MAX_NOTIFICATION_CORNER_RADIUS;
    }
    return cornerRadius;
}

int NotificationClampFontPercent(int fontPercent) {
    if (fontPercent <= 0) {
        return DEFAULT_NOTIFICATION_FONT_SIZE;
    }
    if (fontPercent < MIN_NOTIFICATION_FONT_SIZE) {
        return MIN_NOTIFICATION_FONT_SIZE;
    }
    if (fontPercent > MAX_NOTIFICATION_FONT_SIZE) {
        return MAX_NOTIFICATION_FONT_SIZE;
    }
    return fontPercent;
}

static int CalculateNotificationFontPixelSize(int windowHeight, int fontPercent) {
    if (windowHeight <= 0) {
        windowHeight = NOTIFICATION_HEIGHT;
    }

    int fontSize = MulDiv(windowHeight, NotificationClampFontPercent(fontPercent), 100);
    int maxFontSize = windowHeight - (NOTIFICATION_PADDING_V * 2);

    if (maxFontSize < NOTIFICATION_MIN_FONT_PIXEL_SIZE) {
        maxFontSize = NOTIFICATION_MIN_FONT_PIXEL_SIZE;
    }
    if (fontSize < NOTIFICATION_MIN_FONT_PIXEL_SIZE) {
        return NOTIFICATION_MIN_FONT_PIXEL_SIZE;
    }
    if (fontSize > maxFontSize) {
        return maxFontSize;
    }
    return fontSize;
}

static BYTE CalculateNotificationCornerAlpha(int x, int y,
                                             int width, int height,
                                             int radius,
                                             BYTE opacity) {
    const int sampleCount = 4;
    const int coordinateScale = sampleCount * 2;
    int centerX = (x < radius) ? radius : width - radius;
    int centerY = (y < radius) ? radius : height - radius;
    int centerXScaled = centerX * coordinateScale;
    int centerYScaled = centerY * coordinateScale;
    int radiusScaled = radius * coordinateScale;
    int radiusSquared = radiusScaled * radiusScaled;
    int insideSamples = 0;

    for (int sy = 0; sy < sampleCount; sy++) {
        int sampleY = y * coordinateScale + sy * 2 + 1;
        int dy = sampleY - centerYScaled;
        for (int sx = 0; sx < sampleCount; sx++) {
            int sampleX = x * coordinateScale + sx * 2 + 1;
            int dx = sampleX - centerXScaled;
            if (dx * dx + dy * dy <= radiusSquared) {
                insideSamples++;
            }
        }
    }

    return (BYTE)(((int)opacity * insideSamples + 8) / 16);
}

static void PremultiplyNotificationPixel(unsigned char* pixel, BYTE alpha) {
    pixel[3] = alpha;
    if (alpha == 0) {
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        return;
    }

    pixel[0] = (unsigned char)(((int)pixel[0] * alpha + 127) / 255);
    pixel[1] = (unsigned char)(((int)pixel[1] * alpha + 127) / 255);
    pixel[2] = (unsigned char)(((int)pixel[2] * alpha + 127) / 255);
}

static void ApplyNotificationLayerAlpha(NotificationData* data,
                                        int width, int height) {
    if (!data || !data->paintBits || width <= 0 || height <= 0 ||
        data->paintWidth < width) {
        return;
    }

    BYTE opacity = data->opacity;
    int radius = NotificationClampCornerRadius(data->cornerRadius);
    int maxRadius = width < height ? width / 2 : height / 2;
    if (radius > maxRadius) {
        radius = maxRadius;
    }

    unsigned char* pixels = (unsigned char*)data->paintBits;
    int strideWidth = data->paintWidth;
    for (int y = 0; y < height; y++) {
        BOOL yInsideMiddle = radius <= 0 || (y >= radius && y < height - radius);
        for (int x = 0; x < width; x++) {
            unsigned char* pixel =
                pixels + ((size_t)y * (size_t)strideWidth + (size_t)x) * 4;
            BYTE alpha = (yInsideMiddle || (x >= radius && x < width - radius))
                ? opacity
                : CalculateNotificationCornerAlpha(x, y, width, height, radius, opacity);
            PremultiplyNotificationPixel(pixel, alpha);
        }
    }
}

static HFONT CreateNotificationFont(int size, int weight) {
    return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       NOTIFICATION_FONT_NAME);
}

static HFONT GetNotificationContentFont(int fontSize) {
    if (g_notificationContentFont && g_notificationContentFontSize != fontSize) {
        DeleteObject(g_notificationContentFont);
        g_notificationContentFont = NULL;
        g_notificationContentFontSize = 0;
    }

    if (!g_notificationContentFont) {
        g_notificationContentFont = CreateNotificationFont(fontSize, FW_NORMAL);
        if (g_notificationContentFont) {
            g_notificationContentFontSize = fontSize;
        }
    }
    return g_notificationContentFont;
}

static HBRUSH GetNotificationBgBrush(void) {
    if (!g_notificationBgBrush) {
        g_notificationBgBrush = CreateSolidBrush(NOTIFICATION_BG_COLOR);
    }
    return g_notificationBgBrush;
}

static COLORREF GetNotificationSolidTextColor(const char* activeColor) {
    COLORREF color = NOTIFICATION_CONTENT_COLOR;
    if (ColorStringToColorRef(activeColor, &color)) {
        return color;
    }
    return NOTIFICATION_CONTENT_COLOR;
}

BOOL NotificationCurrentColorIsAnimatedGradient(void) {
    char activeColor[COLOR_HEX_BUFFER] = {0};
    GetActiveColor(activeColor, sizeof(activeColor));

    GradientInfoSnapshot gradientSnapshot;
    return GetGradientInfoSnapshotByName(activeColor, &gradientSnapshot) != GRADIENT_NONE &&
           gradientSnapshot.info.isAnimated;
}

static void BlendNotificationDibPixel(unsigned char* pixel, COLORREF foreground, BYTE alpha) {
    if (!pixel || alpha == 0) return;

    int bgB = pixel[0];
    int bgG = pixel[1];
    int bgR = pixel[2];

    pixel[0] = (unsigned char)((GetBValue(foreground) * alpha + bgB * (255 - alpha)) / 255);
    pixel[1] = (unsigned char)((GetGValue(foreground) * alpha + bgG * (255 - alpha)) / 255);
    pixel[2] = (unsigned char)((GetRValue(foreground) * alpha + bgR * (255 - alpha)) / 255);
}

static BOOL DrawGradientNotificationText(NotificationData* data,
                                         HDC memDC, void* destBits,
                                         int destStrideWidth,
                                         int destWidth, int destHeight,
                                         const wchar_t* text, RECT rect,
                                         HFONT font, const GradientInfo* gradientInfo,
                                         DWORD flags) {
    if (!data || !memDC || !destBits || !text || !font || !gradientInfo) return FALSE;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0 ||
        width > NOTIFICATION_MAX_WIDTH || height > NOTIFICATION_MAX_HEIGHT ||
        destStrideWidth <= 0 || destWidth <= 0 || destHeight <= 0 ||
        destStrideWidth < destWidth) {
        return FALSE;
    }

    HFONT oldMetricsFont = (HFONT)SelectObject(memDC, font);
    TEXTMETRICW textMetrics;
    ZeroMemory(&textMetrics, sizeof(textMetrics));
    if (!GetTextMetricsW(memDC, &textMetrics)) {
        if (oldMetricsFont) {
            SelectObject(memDC, oldMetricsFont);
        }
        return FALSE;
    }
    if (oldMetricsFont) {
        SelectObject(memDC, oldMetricsFont);
    }

    int maskHeight = textMetrics.tmHeight + textMetrics.tmExternalLeading + 4;
    if (maskHeight < NOTIFICATION_MIN_FONT_PIXEL_SIZE) {
        maskHeight = NOTIFICATION_MIN_FONT_PIXEL_SIZE;
    }
    if (maskHeight > height) {
        maskHeight = height;
    }
    int yOffset = (height - maskHeight) / 2;

    HDC maskDC = NULL;
    if (!EnsureNotificationTextMaskBuffer(memDC, data, width, maskHeight, &maskDC)) {
        return FALSE;
    }

    HFONT oldMaskFont = (HFONT)SelectObject(maskDC, font);
    RECT localRect = {0, 0, width, maskHeight};
    FillRect(maskDC, &localRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(maskDC, TRANSPARENT);
    SetTextColor(maskDC, RGB(255, 255, 255));
    DrawTextW(maskDC, text, -1, &localRect, flags & ~DT_VCENTER);

    float animationProgress = gradientInfo->isAnimated
        ? (float)(GetTickCount() % NOTIFICATION_GRADIENT_CYCLE_MS) /
          (float)NOTIFICATION_GRADIENT_CYCLE_MS
        : 0.0f;

    COLORREF gradientColors[NOTIFICATION_MAX_WIDTH];

    for (int x = 0; x < width; x++) {
        float t = (width > 1) ? (float)x / (float)(width - 1) : 0.0f;
        if (gradientInfo->isAnimated) {
            t -= animationProgress;
            if (t < 0.0f) {
                t += 1.0f;
            }
        }
        gradientColors[x] = GetGradientColorAt(gradientInfo, t);
    }

    unsigned char* pixels = (unsigned char*)data->textMaskBits;
    unsigned char* destPixels = (unsigned char*)destBits;
    int maskStrideWidth = data->textMaskWidth;
    for (int y = 0; y < maskHeight; y++) {
        int destY = rect.top + yOffset + y;
        if (destY < 0 || destY >= destHeight) {
            continue;
        }

        for (int x = 0; x < width; x++) {
            unsigned char* pixel =
                pixels + ((size_t)y * (size_t)maskStrideWidth + (size_t)x) * 4;
            BYTE alpha = (BYTE)(((int)pixel[0] + (int)pixel[1] + (int)pixel[2]) / 3);
            if (alpha == 0) {
                continue;
            }

            int destX = rect.left + x;
            if (destX < 0 || destX >= destWidth) {
                continue;
            }

            unsigned char* destPixel =
                destPixels + ((size_t)destY * (size_t)destStrideWidth + (size_t)destX) * 4;
            BlendNotificationDibPixel(destPixel, gradientColors[x], alpha);
        }
    }

    if (oldMaskFont) {
        SelectObject(maskDC, oldMaskFont);
    }
    return TRUE;
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

static void DrawNotificationTextWithCurrentColor(NotificationData* data,
                                                 HDC memDC, void* destBits,
                                                 int destStrideWidth,
                                                 int destWidth, int destHeight,
                                                 const wchar_t* text, RECT rect,
                                                 HFONT font, DWORD flags) {
    char activeColor[COLOR_HEX_BUFFER] = {0};
    GetActiveColor(activeColor, sizeof(activeColor));

    GradientInfoSnapshot gradientSnapshot;
    if (GetGradientInfoSnapshotByName(activeColor, &gradientSnapshot) != GRADIENT_NONE) {
        if (DrawGradientNotificationText(data, memDC, destBits, destStrideWidth,
                                         destWidth, destHeight,
                                         text, rect, font,
                                         &gradientSnapshot.info, flags)) {
            return;
        }
    }

    COLORREF textColor = GetNotificationSolidTextColor(activeColor);
    DrawNotificationText(memDC, text, rect, font, textColor, flags);
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

int NotificationMeasureTextWidth(HDC hdc, const wchar_t* text,
                                 int windowHeight, int fontPercent) {
    if (!hdc || !text) return 0;

    int fontSize = CalculateNotificationFontPixelSize(windowHeight, fontPercent);

    AcquireSRWLockExclusive(&g_notificationResourceLock);
    HFONT contentFont = GetNotificationContentFont(fontSize);
    int textWidth = CalculateTextWidth(hdc, text, contentFont);
    ReleaseSRWLockExclusive(&g_notificationResourceLock);

    return textWidth;
}

void NotificationCleanupRenderResources(void) {
    AcquireSRWLockExclusive(&g_notificationResourceLock);
    if (g_notificationContentFont) {
        DeleteObject(g_notificationContentFont);
        g_notificationContentFont = NULL;
        g_notificationContentFontSize = 0;
    }
    if (g_notificationBgBrush) {
        DeleteObject(g_notificationBgBrush);
        g_notificationBgBrush = NULL;
    }
    ReleaseSRWLockExclusive(&g_notificationResourceLock);
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
    data->paintBits = NULL;
    data->paintWidth = 0;
    data->paintHeight = 0;
}

static void ReleaseNotificationTextMaskBuffer(NotificationData* data) {
    if (!data) return;

    if (data->textMaskDC && data->oldTextMaskBitmap) {
        SelectObject(data->textMaskDC, data->oldTextMaskBitmap);
    }
    if (data->textMaskBitmap) {
        DeleteObject(data->textMaskBitmap);
    }
    if (data->textMaskDC) {
        DeleteDC(data->textMaskDC);
    }

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

static BOOL EnsureNotificationPaintBuffer(HDC hdc, NotificationData* data,
                                          int width, int height, HDC* outMemDC) {
    if (!hdc || !data || !outMemDC || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > (size_t)NOTIFICATION_MAX_PAINT_PIXELS / (size_t)height) {
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

    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) {
        return FALSE;
    }

    BITMAPINFO bitmapInfo;
    ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapBits = NULL;
    HBITMAP bitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS,
                                      &bitmapBits, NULL, 0);
    if (!bitmap || !bitmapBits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
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
    data->paintBits = bitmapBits;
    data->paintWidth = width;
    data->paintHeight = height;

    *outMemDC = memDC;
    return TRUE;
}

static BOOL EnsureNotificationTextMaskBuffer(HDC hdc, NotificationData* data,
                                             int width, int height, HDC* outMemDC) {
    if (!hdc || !data || !outMemDC || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > (size_t)NOTIFICATION_MAX_PAINT_PIXELS / (size_t)height) {
        return FALSE;
    }

    if (data->textMaskDC &&
        data->textMaskWidth >= width &&
        data->textMaskHeight >= height) {
        size_t requestedPixels = (size_t)width * (size_t)height;
        size_t cachedPixels =
            (size_t)data->textMaskWidth * (size_t)data->textMaskHeight;
        if (requestedPixels > 0 &&
            cachedPixels / NOTIFICATION_PAINT_SHRINK_THRESHOLD_MULTIPLIER <= requestedPixels) {
            *outMemDC = data->textMaskDC;
            return TRUE;
        }
    }

    HDC maskDC = CreateCompatibleDC(hdc);
    if (!maskDC) {
        return FALSE;
    }

    BITMAPINFO bitmapInfo;
    ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapBits = NULL;
    HBITMAP bitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS,
                                      &bitmapBits, NULL, 0);
    if (!bitmap || !bitmapBits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
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
    data->textMaskBits = bitmapBits;
    data->textMaskWidth = width;
    data->textMaskHeight = height;

    *outMemDC = maskDC;
    return TRUE;
}

BOOL NotificationRenderLayeredWindow(HWND hwnd, NotificationData* data) {
    if (!hwnd || !data) {
        return FALSE;
    }

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int paintWidth = clientRect.right - clientRect.left;
    int paintHeight = clientRect.bottom - clientRect.top;
    if (paintWidth <= 0 || paintHeight <= 0) {
        return FALSE;
    }

    HDC screenDC = GetDC(NULL);
    if (!screenDC) {
        return FALSE;
    }

    HDC memDC = NULL;
    if (!EnsureNotificationPaintBuffer(screenDC, data, paintWidth, paintHeight, &memDC)) {
        ReleaseDC(NULL, screenDC);
        return FALSE;
    }

    AcquireSRWLockExclusive(&g_notificationResourceLock);

    HBRUSH bgBrush = GetNotificationBgBrush();
    FillRect(memDC, &clientRect, bgBrush ? bgBrush : (HBRUSH)(COLOR_WINDOW + 1));
    SetBkMode(memDC, TRANSPARENT);

    int fontSize = CalculateNotificationFontPixelSize(paintHeight, data->fontPercent);
    HFONT contentFont = GetNotificationContentFont(fontSize);

    if (data->messageText) {
        RECT textRect = {
            NOTIFICATION_PADDING_H,
            NOTIFICATION_PADDING_V,
            clientRect.right - NOTIFICATION_PADDING_H,
            clientRect.bottom - NOTIFICATION_PADDING_V
        };
        DrawNotificationTextWithCurrentColor(data, memDC, data->paintBits,
                                             data->paintWidth,
                                             paintWidth, paintHeight,
                                             data->messageText, textRect,
                                             contentFont,
                                             DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    ReleaseSRWLockExclusive(&g_notificationResourceLock);

    ApplyNotificationLayerAlpha(data, paintWidth, paintHeight);

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        ReleaseDC(NULL, screenDC);
        return FALSE;
    }

    POINT dst = {windowRect.left, windowRect.top};
    POINT src = {0, 0};
    SIZE size = {paintWidth, paintHeight};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    BOOL updated = UpdateLayeredWindow(hwnd, screenDC, &dst, &size,
                                       memDC, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(NULL, screenDC);
    return updated;
}
