/**
 * @file color_picker_dialog.c
 * @brief Lightweight modern HSV color picker implemented with Win32/GDI.
 */

#include "color/color_picker_dialog.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "menu_preview.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>
#include <windowsx.h>

#define COLOR_PICKER_CANVAS_SUBCLASS_ID 0xC920
#define COLOR_PICKER_MAX_CANVAS_DIMENSION 2048

typedef struct {
    HWND hwnd;
    HWND hwndParent;
    COLORREF selectedColor;
    COLORREF eyedropperOriginalColor;
    COLORREF* customColors;
    size_t customColorCapacity;
    size_t customColorCount;
    double hue;
    double saturation;
    double value;
    BOOL updatingFields;
    BOOL eyedropperActive;
    int savedFocusIndex;
    DWORD* svPixels;
    int svPixelWidth;
    int svPixelHeight;
    int svCachedHue;
    DWORD* huePixels;
    int huePixelWidth;
    int huePixelHeight;
} ModernColorPickerState;

static INT_PTR CALLBACK ModernColorPickerDlgProc(HWND hwndDlg, UINT msg,
                                                 WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ColorPickerCanvasSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData);

static double PickerClampUnit(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static int PickerClampByte(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}

static double PickerNormalizeHue(double hue) {
    while (hue < 0.0) hue += 360.0;
    while (hue >= 360.0) hue -= 360.0;
    return hue;
}

static COLORREF PickerHsvToColor(double hue, double saturation, double value) {
    hue = PickerNormalizeHue(hue);
    saturation = PickerClampUnit(saturation);
    value = PickerClampUnit(value);

    double chroma = value * saturation;
    double hueSector = hue / 60.0;
    int sector = (int)hueSector;
    double fraction = hueSector - sector;
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    switch (sector) {
        case 0:
            red = chroma;
            green = chroma * fraction;
            break;
        case 1:
            red = chroma * (1.0 - fraction);
            green = chroma;
            break;
        case 2:
            green = chroma;
            blue = chroma * fraction;
            break;
        case 3:
            green = chroma * (1.0 - fraction);
            blue = chroma;
            break;
        case 4:
            red = chroma * fraction;
            blue = chroma;
            break;
        default:
            red = chroma;
            blue = chroma * (1.0 - fraction);
            break;
    }

    double match = value - chroma;
    int redByte = PickerClampByte((int)((red + match) * 255.0 + 0.5));
    int greenByte = PickerClampByte((int)((green + match) * 255.0 + 0.5));
    int blueByte = PickerClampByte((int)((blue + match) * 255.0 + 0.5));
    return RGB(redByte, greenByte, blueByte);
}

static void PickerColorToHsv(COLORREF color, double previousHue,
                             double* hue, double* saturation, double* value) {
    double red = GetRValue(color) / 255.0;
    double green = GetGValue(color) / 255.0;
    double blue = GetBValue(color) / 255.0;
    double maximum = red;
    if (green > maximum) maximum = green;
    if (blue > maximum) maximum = blue;
    double minimum = red;
    if (green < minimum) minimum = green;
    if (blue < minimum) minimum = blue;
    double delta = maximum - minimum;
    double resolvedHue = previousHue;

    if (delta > 0.0) {
        if (maximum == red) {
            resolvedHue = 60.0 * ((green - blue) / delta);
            if (resolvedHue < 0.0) resolvedHue += 360.0;
        } else if (maximum == green) {
            resolvedHue = 60.0 * (((blue - red) / delta) + 2.0);
        } else {
            resolvedHue = 60.0 * (((red - green) / delta) + 4.0);
        }
    }

    if (hue) *hue = PickerNormalizeHue(resolvedHue);
    if (saturation) *saturation = maximum > 0.0 ? delta / maximum : 0.0;
    if (value) *value = maximum;
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

static void PickerDrawOutline(HDC hdc, const RECT* rect, int radius,
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
    int hueKey = (int)(PickerNormalizeHue(state->hue) * 10.0 + 0.5);
    if (state->svCachedHue == hueKey) return TRUE;

    for (int y = 0; y < height; y++) {
        double value = height > 1 ? 1.0 - (double)y / (height - 1) : 1.0;
        for (int x = 0; x < width; x++) {
            double saturation = width > 1 ? (double)x / (width - 1) : 0.0;
            state->svPixels[(size_t)y * (size_t)width + (size_t)x] =
                PickerColorToDibPixel(
                    PickerHsvToColor(state->hue, saturation, value));
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
        DWORD pixel = PickerColorToDibPixel(PickerHsvToColor(hue, 1.0, 1.0));
        for (int x = 0; x < width; x++) {
            state->huePixels[(size_t)y * (size_t)width + (size_t)x] = pixel;
        }
    }
    return TRUE;
}

static void PickerRefreshPalette(ModernColorPickerState* state,
                                 DialogModernPalette* palette) {
    if (!palette) return;
    DialogModern_CopyPalette(state ? state->hwnd : NULL, palette);
}

static void PickerPaintSv(ModernColorPickerState* state,
                          const DRAWITEMSTRUCT* item) {
    RECT rect = item->rcItem;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    DialogModernPalette palette;
    PickerRefreshPalette(state, &palette);
    UINT dpi = DialogModern_GetDpi(state->hwnd);
    int canvasRadius = DialogModern_Scale(dpi, 11);
    HBRUSH surface = CreateSolidBrush(palette.surface);
    if (surface) {
        FillRect(item->hDC, &rect, surface);
