/**
 * @file notification_render.c
 * @brief Rendering helpers for custom toast notification windows
 */
#include "notification_internal.h"
#include "notification_render_internal.h"

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

BOOL EnsureNotificationTextMaskBuffer(HDC hdc, NotificationData* data,
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
