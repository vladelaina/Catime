/**
 * @file color_picker_canvas.c
 * @brief Builds and paints saturation/value and hue canvases.
 */

#include "color/color_picker_internal.h"
#include "../resource/resource.h"

#include <stdlib.h>

#define COLOR_PICKER_MAX_CANVAS_DIMENSION 2048

typedef struct {
    HDC target;
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ oldBitmap;
    RECT rect;
    POINT targetOrigin;
    int width;
    int height;
} PickerPaintBuffer;

static void PickerBeginPaintBuffer(const DRAWITEMSTRUCT* item,
                                   PickerPaintBuffer* paint) {
    ZeroMemory(paint, sizeof(*paint));
    paint->target = item->hDC;
    paint->dc = item->hDC;
    paint->rect = item->rcItem;
    paint->targetOrigin.x = item->rcItem.left;
    paint->targetOrigin.y = item->rcItem.top;
    paint->width = item->rcItem.right - item->rcItem.left;
    paint->height = item->rcItem.bottom - item->rcItem.top;
    if (paint->width <= 0 || paint->height <= 0) return;
    HDC buffer = CreateCompatibleDC(item->hDC);
    HBITMAP bitmap = buffer
        ? CreateCompatibleBitmap(item->hDC, paint->width, paint->height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    if (!buffer || !bitmap || !oldBitmap || oldBitmap == HGDI_ERROR) {
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
        return;
    }
    paint->dc = buffer;
    paint->bitmap = bitmap;
    paint->oldBitmap = oldBitmap;
    SetRect(&paint->rect, 0, 0, paint->width, paint->height);
}

static void PickerEndPaintBuffer(PickerPaintBuffer* paint) {
    if (!paint || !paint->bitmap) return;
    BitBlt(paint->target, paint->targetOrigin.x, paint->targetOrigin.y,
           paint->width, paint->height,
           paint->dc, 0, 0, SRCCOPY);
    SelectObject(paint->dc, paint->oldBitmap);
    DeleteObject(paint->bitmap);
    DeleteDC(paint->dc);
}

static DWORD PickerColorToDibPixel(COLORREF color) {
    return (DWORD)GetBValue(color) |
           ((DWORD)GetGValue(color) << 8) |
           ((DWORD)GetRValue(color) << 16);
}

static BOOL PickerEnsurePixelBuffer(DWORD** pixels, int* currentWidth,
                                    int* currentHeight, int width, int height) {
    if (!pixels || !currentWidth || !currentHeight || width <= 0 || height <= 0 ||
        width > COLOR_PICKER_MAX_CANVAS_DIMENSION ||
        height > COLOR_PICKER_MAX_CANVAS_DIMENSION) {
        return FALSE;
    }
    if (*pixels && *currentWidth == width && *currentHeight == height) {
        return TRUE;
    }

    size_t pixelCount = (size_t)width * (size_t)height;
    if (pixelCount > ((size_t)-1) / sizeof(DWORD)) return FALSE;
    DWORD* resized = (DWORD*)realloc(*pixels, pixelCount * sizeof(DWORD));
    if (!resized) return FALSE;
    *pixels = resized;
    *currentWidth = width;
    *currentHeight = height;
    return TRUE;
}

static void PickerDrawPixels(HDC hdc, const RECT* rect, const DWORD* pixels,
                             int width, int height) {
    if (!hdc || !rect || !pixels || width <= 0 || height <= 0) return;
    BITMAPINFO bitmapInfo = {0};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, rect->left, rect->top,
                  rect->right - rect->left, rect->bottom - rect->top,
                  0, 0, width, height, pixels, &bitmapInfo,
                  DIB_RGB_COLORS, SRCCOPY);
}

static void PickerDrawRoundedPixels(HDC hdc, const RECT* rect,
                                    const DWORD* pixels, int width, int height,
                                    int radius) {
    if (!hdc || !rect || !pixels) return;
    int savedDc = SaveDC(hdc);
    HRGN clip = CreateRoundRectRgn(rect->left, rect->top,
                                   rect->right + 1, rect->bottom + 1,
                                   radius * 2, radius * 2);
    if (savedDc && clip) SelectClipRgn(hdc, clip);
    PickerDrawPixels(hdc, rect, pixels, width, height);
    if (savedDc) RestoreDC(hdc, savedDc);
    if (clip) DeleteObject(clip);
}

void ColorPickerInternal_DrawOutline(HDC hdc, const RECT* rect, int radius,
                              COLORREF color, int width) {
    if (!hdc || !rect) return;
    HPEN pen = CreatePen(PS_SOLID, width > 0 ? width : 1, color);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom,
              radius, radius);
    SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static BOOL PickerBuildSvPixels(ModernColorPickerState* state,
                                int width, int height) {
    if (!state) return FALSE;
    int previousWidth = state->svPixelWidth;
    int previousHeight = state->svPixelHeight;
    if (!PickerEnsurePixelBuffer(&state->svPixels, &state->svPixelWidth,
                                 &state->svPixelHeight, width, height)) {
        return FALSE;
    }
    if (previousWidth != width || previousHeight != height) {
        state->svCachedHue = -1;
    }
    int hueKey = (int)(ColorPickerInternal_NormalizeHue(state->hue) * 10.0 + 0.5);
    if (state->svCachedHue == hueKey) return TRUE;

    for (int y = 0; y < height; y++) {
        double value = height > 1 ? 1.0 - (double)y / (height - 1) : 1.0;
        for (int x = 0; x < width; x++) {
            double saturation = width > 1 ? (double)x / (width - 1) : 0.0;
            state->svPixels[(size_t)y * (size_t)width + (size_t)x] =
                PickerColorToDibPixel(
                    ColorPickerInternal_HsvToColor(state->hue, saturation, value));
        }
    }
    state->svCachedHue = hueKey;
    return TRUE;
}

static BOOL PickerBuildHuePixels(ModernColorPickerState* state,
                                 int width, int height) {
    if (!state ||
        !PickerEnsurePixelBuffer(&state->huePixels, &state->huePixelWidth,
                                 &state->huePixelHeight, width, height)) {
        return FALSE;
    }
    for (int y = 0; y < height; y++) {
        double hue = height > 1 ? (double)y * 359.999 / (height - 1) : 0.0;
        DWORD pixel = PickerColorToDibPixel(ColorPickerInternal_HsvToColor(hue, 1.0, 1.0));
        for (int x = 0; x < width; x++) {
            state->huePixels[(size_t)y * (size_t)width + (size_t)x] = pixel;
        }
    }
    return TRUE;
}

void ColorPickerInternal_RefreshPalette(ModernColorPickerState* state,
                                 DialogModernPalette* palette) {
    if (!palette) return;
    DialogModern_CopyPalette(state ? state->hwnd : NULL, palette);
}

void ColorPickerInternal_PaintSv(ModernColorPickerState* state,
                          const DRAWITEMSTRUCT* item) {
    PickerPaintBuffer paint;
    PickerBeginPaintBuffer(item, &paint);
    RECT rect = paint.rect;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    DialogModernPalette palette;
    ColorPickerInternal_RefreshPalette(state, &palette);
    UINT dpi = DialogModern_GetDpi(state->hwnd);
    int canvasRadius = DialogModern_Scale(dpi, 11);
    HBRUSH surface = CreateSolidBrush(palette.surface);
    if (surface) {
        FillRect(paint.dc, &rect, surface);
        DeleteObject(surface);
    }
    DialogModern_DrawRoundedRect(paint.dc, &rect, canvasRadius * 2,
                                 palette.field, palette.field, 0);
    if (PickerBuildSvPixels(state, width, height)) {
        PickerDrawRoundedPixels(paint.dc, &rect, state->svPixels,
                                width, height, canvasRadius);
    }

    RECT canvasOutline = rect;
    InflateRect(&canvasOutline, -1, -1);
    BOOL focused = GetFocus() == item->hwndItem;
    ColorPickerInternal_DrawOutline(paint.dc, &canvasOutline, canvasRadius * 2,
                      focused ? palette.accent : palette.border,
                      focused ? 2 : 1);

    int x = rect.left + (int)(state->saturation * (width - 1) + 0.5);
    int y = rect.top + (int)((1.0 - state->value) * (height - 1) + 0.5);
    int markerRadius = DialogModern_Scale(dpi, 7);
    int markerInset = markerRadius + DialogModern_Scale(dpi, 2);
    if (x < rect.left + markerInset) x = rect.left + markerInset;
    if (x > rect.right - markerInset - 1) {
        x = rect.right - markerInset - 1;
    }
    if (y < rect.top + markerInset) y = rect.top + markerInset;
    if (y > rect.bottom - markerInset - 1) {
        y = rect.bottom - markerInset - 1;
    }
    RECT marker = {x - markerRadius, y - markerRadius,
                   x + markerRadius + 1, y + markerRadius + 1};
    ColorPickerInternal_DrawOutline(paint.dc, &marker, markerRadius * 2,
                      RGB(0xFF, 0xFF, 0xFF), 3);
    InflateRect(&marker, -2, -2);
    ColorPickerInternal_DrawOutline(paint.dc, &marker, markerRadius * 2,
                      RGB(0x18, 0x22, 0x30), 1);
    PickerEndPaintBuffer(&paint);
}

void ColorPickerInternal_PaintHue(ModernColorPickerState* state,
                           const DRAWITEMSTRUCT* item) {
    PickerPaintBuffer paint;
    PickerBeginPaintBuffer(item, &paint);
    RECT rect = paint.rect;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    DialogModernPalette palette;
    ColorPickerInternal_RefreshPalette(state, &palette);
    UINT dpi = DialogModern_GetDpi(state->hwnd);
    int canvasRadius = DialogModern_Scale(dpi, 7);
    HBRUSH surface = CreateSolidBrush(palette.surface);
    if (surface) {
        FillRect(paint.dc, &rect, surface);
        DeleteObject(surface);
    }
    DialogModern_DrawRoundedRect(paint.dc, &rect, canvasRadius * 2,
                                 palette.field, palette.field, 0);
    if (PickerBuildHuePixels(state, width, height)) {
        PickerDrawRoundedPixels(paint.dc, &rect, state->huePixels,
                                width, height, canvasRadius);
    }

    RECT canvasOutline = rect;
    InflateRect(&canvasOutline, -1, -1);
    BOOL focused = GetFocus() == item->hwndItem;
    ColorPickerInternal_DrawOutline(paint.dc, &canvasOutline, canvasRadius * 2,
                      focused ? palette.accent : palette.border,
                      focused ? 2 : 1);
    int y = rect.top + (int)(ColorPickerInternal_NormalizeHue(state->hue) *
                             (height - 1) / 359.999 + 0.5);
    int markerHalfHeight = DialogModern_Scale(dpi, 4);
    if (y < rect.top + markerHalfHeight + 1) {
        y = rect.top + markerHalfHeight + 1;
    }
    if (y > rect.bottom - markerHalfHeight - 2) {
        y = rect.bottom - markerHalfHeight - 2;
    }
    RECT marker = {rect.left + 1, y - markerHalfHeight,
                   rect.right - 1, y + markerHalfHeight + 1};
    int markerRadius = DialogModern_Scale(dpi, 5);
    ColorPickerInternal_DrawOutline(paint.dc, &marker, markerRadius,
                      RGB(0xFF, 0xFF, 0xFF),
                      DialogModern_Scale(dpi, 2));
    InflateRect(&marker, -1, -1);
    ColorPickerInternal_DrawOutline(paint.dc, &marker, DialogModern_Scale(dpi, 4),
                      RGB(0x18, 0x22, 0x30),
                      DialogModern_Scale(dpi, 1));
    PickerEndPaintBuffer(&paint);
}
