#include "notification_render_internal.h"
#include "color/color_state.h"
#include "color/gradient.h"
#include "menu_preview.h"

static COLORREF GetNotificationSolidTextColor(const char* activeColor) {
    COLORREF color = NOTIFICATION_CONTENT_COLOR;
    return ColorStringToColorRef(activeColor, &color) ? color
                                                       : NOTIFICATION_CONTENT_COLOR;
}

BOOL NotificationCurrentColorIsAnimatedGradient(void) {
    char activeColor[COLOR_HEX_BUFFER] = {0};
    GetActiveColor(activeColor, sizeof(activeColor));
    GradientInfoSnapshot snapshot;
    return GetGradientInfoSnapshotByName(activeColor, &snapshot) != GRADIENT_NONE &&
           snapshot.info.isAnimated;
}

static void BlendNotificationDibPixel(unsigned char* pixel, COLORREF foreground,
                                      BYTE alpha) {
    if (!pixel || alpha == 0) return;
    pixel[0] = (unsigned char)((GetBValue(foreground) * alpha +
                                pixel[0] * (255 - alpha)) / 255);
    pixel[1] = (unsigned char)((GetGValue(foreground) * alpha +
                                pixel[1] * (255 - alpha)) / 255);
    pixel[2] = (unsigned char)((GetRValue(foreground) * alpha +
                                pixel[2] * (255 - alpha)) / 255);
}

static BOOL DrawGradientNotificationText(NotificationData* data, HDC memDC,
                                         void* destBits, int destStrideWidth,
                                         int destWidth, int destHeight,
                                         const wchar_t* text, RECT rect, HFONT font,
                                         const GradientInfo* gradientInfo,
                                         DWORD flags) {
    if (!data || !memDC || !destBits || !text || !font || !gradientInfo) return FALSE;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0 || width > NOTIFICATION_MAX_WIDTH ||
        height > NOTIFICATION_MAX_HEIGHT || destStrideWidth <= 0 ||
        destWidth <= 0 || destHeight <= 0 || destStrideWidth < destWidth) return FALSE;
    HFONT oldMetricsFont = (HFONT)SelectObject(memDC, font);
    TEXTMETRICW textMetrics;
    ZeroMemory(&textMetrics, sizeof(textMetrics));
    BOOL measured = GetTextMetricsW(memDC, &textMetrics);
    if (oldMetricsFont) SelectObject(memDC, oldMetricsFont);
    if (!measured) return FALSE;
    int maskHeight = textMetrics.tmHeight + textMetrics.tmExternalLeading + 4;
    if (maskHeight < NOTIFICATION_MIN_FONT_PIXEL_SIZE) maskHeight = NOTIFICATION_MIN_FONT_PIXEL_SIZE;
    if (maskHeight > height) maskHeight = height;
    int yOffset = (height - maskHeight) / 2;
    HDC maskDC = NULL;
    if (!EnsureNotificationTextMaskBuffer(memDC, data, width, maskHeight, &maskDC)) return FALSE;
    HFONT oldMaskFont = (HFONT)SelectObject(maskDC, font);
    RECT localRect = {0, 0, width, maskHeight};
    FillRect(maskDC, &localRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(maskDC, TRANSPARENT);
    SetTextColor(maskDC, RGB(255, 255, 255));
    DrawTextW(maskDC, text, -1, &localRect, flags & ~DT_VCENTER);
    float progress = gradientInfo->isAnimated
        ? (float)(GetTickCount() % NOTIFICATION_GRADIENT_CYCLE_MS) /
          (float)NOTIFICATION_GRADIENT_CYCLE_MS : 0.0f;
    COLORREF gradientColors[NOTIFICATION_MAX_WIDTH];
    for (int x = 0; x < width; x++) {
        float t = width > 1 ? (float)x / (float)(width - 1) : 0.0f;
        if (gradientInfo->isAnimated) {
            t -= progress;
            if (t < 0.0f) t += 1.0f;
        }
        gradientColors[x] = GetGradientColorAt(gradientInfo, t);
    }
    unsigned char* pixels = (unsigned char*)data->textMaskBits;
    unsigned char* destPixels = (unsigned char*)destBits;
    for (int y = 0; y < maskHeight; y++) {
        int destY = rect.top + yOffset + y;
        if (destY < 0 || destY >= destHeight) continue;
        for (int x = 0; x < width; x++) {
            const unsigned char* pixel =
                pixels + ((size_t)y * data->textMaskWidth + x) * 4;
            BYTE alpha = (BYTE)(((int)pixel[0] + pixel[1] + pixel[2]) / 3);
            int destX = rect.left + x;
            if (!alpha || destX < 0 || destX >= destWidth) continue;
            unsigned char* destPixel = destPixels +
                ((size_t)destY * destStrideWidth + destX) * 4;
            BlendNotificationDibPixel(destPixel, gradientColors[x], alpha);
        }
    }
    if (oldMaskFont) SelectObject(maskDC, oldMaskFont);
    return TRUE;
}

static void DrawNotificationText(HDC memDC, const wchar_t* text, RECT rect,
                                 HFONT font, COLORREF color, DWORD flags) {
    HFONT oldFont = font ? (HFONT)SelectObject(memDC, font) : NULL;
    SetTextColor(memDC, color);
    DrawTextW(memDC, text, -1, &rect, flags);
    if (oldFont) SelectObject(memDC, oldFont);
}

void DrawNotificationTextWithCurrentColor(NotificationData* data, HDC memDC,
                                          void* destBits, int destStrideWidth,
                                          int destWidth, int destHeight,
                                          const wchar_t* text, RECT rect,
                                          HFONT font, DWORD flags) {
    char activeColor[COLOR_HEX_BUFFER] = {0};
    GetActiveColor(activeColor, sizeof(activeColor));
    GradientInfoSnapshot snapshot;
    if (GetGradientInfoSnapshotByName(activeColor, &snapshot) != GRADIENT_NONE &&
        DrawGradientNotificationText(data, memDC, destBits, destStrideWidth,
                                     destWidth, destHeight, text, rect, font,
                                     &snapshot.info, flags)) return;
    DrawNotificationText(memDC, text, rect, font,
                         GetNotificationSolidTextColor(activeColor), flags);
}
