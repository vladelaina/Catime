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
        DeleteObject(surface);
    }
    DialogModern_DrawRoundedRect(item->hDC, &rect, canvasRadius * 2,
                                 palette.field, palette.field, 0);
    if (PickerBuildSvPixels(state, width, height)) {
        PickerDrawRoundedPixels(item->hDC, &rect, state->svPixels,
                                width, height, canvasRadius);
    }

    RECT canvasOutline = rect;
    InflateRect(&canvasOutline, -1, -1);
    BOOL focused = GetFocus() == item->hwndItem;
    PickerDrawOutline(item->hDC, &canvasOutline, canvasRadius * 2,
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
    PickerDrawOutline(item->hDC, &marker, markerRadius * 2,
                      RGB(0xFF, 0xFF, 0xFF), 3);
    InflateRect(&marker, -2, -2);
    PickerDrawOutline(item->hDC, &marker, markerRadius * 2,
                      RGB(0x18, 0x22, 0x30), 1);
}

static void PickerPaintHue(ModernColorPickerState* state,
                           const DRAWITEMSTRUCT* item) {
    RECT rect = item->rcItem;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    DialogModernPalette palette;
    PickerRefreshPalette(state, &palette);
    UINT dpi = DialogModern_GetDpi(state->hwnd);
    int canvasRadius = DialogModern_Scale(dpi, 7);
    HBRUSH surface = CreateSolidBrush(palette.surface);
    if (surface) {
        FillRect(item->hDC, &rect, surface);
        DeleteObject(surface);
    }
    DialogModern_DrawRoundedRect(item->hDC, &rect, canvasRadius * 2,
                                 palette.field, palette.field, 0);
    if (PickerBuildHuePixels(state, width, height)) {
        PickerDrawRoundedPixels(item->hDC, &rect, state->huePixels,
                                width, height, canvasRadius);
    }

    RECT canvasOutline = rect;
    InflateRect(&canvasOutline, -1, -1);
    BOOL focused = GetFocus() == item->hwndItem;
    PickerDrawOutline(item->hDC, &canvasOutline, canvasRadius * 2,
                      focused ? palette.accent : palette.border,
                      focused ? 2 : 1);
    int y = rect.top + (int)(PickerNormalizeHue(state->hue) *
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
    PickerDrawOutline(item->hDC, &marker, markerRadius,
                      RGB(0xFF, 0xFF, 0xFF),
                      DialogModern_Scale(dpi, 2));
    InflateRect(&marker, -1, -1);
    PickerDrawOutline(item->hDC, &marker, DialogModern_Scale(dpi, 4),
                      RGB(0x18, 0x22, 0x30),
                      DialogModern_Scale(dpi, 1));
}

static BOOL PickerGetSavedCellRect(const RECT* bounds, size_t index,
                                   RECT* cellRect) {
    if (!bounds || !cellRect || index >= 16) return FALSE;
    const int columns = 8;
    const int rows = 2;
    const int gap = 5;
    int availableWidth = bounds->right - bounds->left - gap * (columns - 1);
    int availableHeight = bounds->bottom - bounds->top - gap * (rows - 1);
    if (availableWidth <= 0 || availableHeight <= 0) return FALSE;
    int cellWidth = availableWidth / columns;
    int cellHeight = availableHeight / rows;
    int column = (int)(index % columns);
    int row = (int)(index / columns);
    cellRect->left = bounds->left + column * (cellWidth + gap);
    cellRect->top = bounds->top + row * (cellHeight + gap);
    cellRect->right = cellRect->left + cellWidth;
    cellRect->bottom = cellRect->top + cellHeight;
    return TRUE;
}

static void PickerDrawSavedCheck(HDC hdc, const RECT* rect,
                                 COLORREF fill, UINT dpi) {
    if (!hdc || !rect) return;
    int luminance = (GetRValue(fill) * 299 +
                     GetGValue(fill) * 587 +
                     GetBValue(fill) * 114) / 1000;
    COLORREF color = luminance > 165 ? RGB(0x18, 0x22, 0x30) :
                                      RGB(0xFF, 0xFF, 0xFF);
    int arm = DialogModern_Scale(dpi, 6);
    int centerX = (rect->left + rect->right) / 2;
    int centerY = (rect->top + rect->bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, DialogModern_Scale(dpi, 2), color);
    if (!pen) return;
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, centerX - arm, centerY, NULL);
    LineTo(hdc, centerX - DialogModern_Scale(dpi, 1),
           centerY + DialogModern_Scale(dpi, 4));
    LineTo(hdc, centerX + arm, centerY - arm);
    if (oldPen) SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void PickerPaintSavedColors(ModernColorPickerState* state,
                                   const DRAWITEMSTRUCT* item) {
    DialogModernPalette palette;
    PickerRefreshPalette(state, &palette);
    RECT rect = item->rcItem;
    HBRUSH surface = CreateSolidBrush(palette.surface);
    if (surface) {
        FillRect(item->hDC, &rect, surface);
        DeleteObject(surface);
    }
    UINT dpi = DialogModern_GetDpi(state->hwnd);

    for (size_t i = 0; i < 16; i++) {
        RECT cell;
        if (!PickerGetSavedCellRect(&rect, i, &cell)) continue;
        COLORREF fill = i < state->customColorCount ?
                        state->customColors[i] : palette.field;
        DialogModern_DrawRoundedRect(
            item->hDC, &cell,
            DialogModern_Scale(dpi, 7),
            fill, palette.border, 1);
        if (i < state->customColorCount &&
            state->customColors[i] == state->selectedColor) {
            RECT selected = cell;
            InflateRect(&selected, -2, -2);
            PickerDrawOutline(
                item->hDC, &selected,
                DialogModern_Scale(dpi, 6),
                palette.accent, 2);
            PickerDrawSavedCheck(item->hDC, &cell, fill, dpi);
        }
        if (GetFocus() == item->hwndItem &&
            (int)i == state->savedFocusIndex) {
            RECT focused = cell;
            InflateRect(&focused, 2, 2);
            PickerDrawOutline(
                item->hDC, &focused,
                DialogModern_Scale(dpi, 8),
                palette.accent, 2);
        }
    }
}

static void PickerPaintPreview(ModernColorPickerState* state,
                               const DRAWITEMSTRUCT* item) {
    DialogModernPalette palette;
    PickerRefreshPalette(state, &palette);
    RECT rect = item->rcItem;
    HBRUSH surface = CreateSolidBrush(palette.surface);
    if (surface) {
        FillRect(item->hDC, &rect, surface);
        DeleteObject(surface);
    }
    InflateRect(&rect, -1, -1);
    DialogModern_DrawRoundedRect(
        item->hDC, &rect,
        DialogModern_Scale(DialogModern_GetDpi(state->hwnd), 14),
        state->selectedColor, palette.border, 1);
}

static void PickerInvalidateVisuals(ModernColorPickerState* state) {
    if (!state || !state->hwnd) return;
    int controls[] = {
        IDC_MODERN_COLOR_SV,
        IDC_MODERN_COLOR_HUE,
        IDC_MODERN_COLOR_PREVIEW,
        IDC_MODERN_COLOR_SAVED
    };
    for (size_t i = 0; i < _countof(controls); i++) {
        HWND control = GetDlgItem(state->hwnd, controls[i]);
        if (control) InvalidateRect(control, NULL, FALSE);
    }
}

static void PickerUpdateFields(ModernColorPickerState* state) {
    if (!state || !state->hwnd) return;
    wchar_t hex[16] = {0};
    _snwprintf_s(hex, _countof(hex), _TRUNCATE, L"#%02X%02X%02X",
                 GetRValue(state->selectedColor),
                 GetGValue(state->selectedColor),
                 GetBValue(state->selectedColor));
    state->updatingFields = TRUE;
    SetDlgItemTextW(state->hwnd, IDC_MODERN_COLOR_HEX_EDIT, hex);
    SetDlgItemInt(state->hwnd, IDC_MODERN_COLOR_RED_EDIT,
                  GetRValue(state->selectedColor), FALSE);
    SetDlgItemInt(state->hwnd, IDC_MODERN_COLOR_GREEN_EDIT,
                  GetGValue(state->selectedColor), FALSE);
    SetDlgItemInt(state->hwnd, IDC_MODERN_COLOR_BLUE_EDIT,
                  GetBValue(state->selectedColor), FALSE);
    state->updatingFields = FALSE;
}

static void PickerApplyPreview(ModernColorPickerState* state) {
    if (!state || !state->hwndParent) return;
    char colorString[COLOR_HEX_BUFFER] = {0};
    char finalColor[COLOR_HEX_BUFFER] = {0};
    ColorRefToHex(state->selectedColor, colorString, sizeof(colorString));
    ReplaceBlackColor(colorString, finalColor, sizeof(finalColor));
    StartPreview(PREVIEW_TYPE_COLOR, finalColor, state->hwndParent);
}

static void PickerSetColor(ModernColorPickerState* state, COLORREF color,
                           BOOL updateHsv, BOOL preview) {
    if (!state) return;
    state->selectedColor = color;
    if (updateHsv) {
        PickerColorToHsv(color, state->hue, &state->hue,
                         &state->saturation, &state->value);
    }
    PickerUpdateFields(state);
    PickerInvalidateVisuals(state);
    if (preview) PickerApplyPreview(state);
}

static void PickerSetColorFromHsv(ModernColorPickerState* state, BOOL preview) {
    if (!state) return;
    state->hue = PickerNormalizeHue(state->hue);
    state->saturation = PickerClampUnit(state->saturation);
    state->value = PickerClampUnit(state->value);
    state->selectedColor =
        PickerHsvToColor(state->hue, state->saturation, state->value);
    PickerUpdateFields(state);
    PickerInvalidateVisuals(state);
    if (preview) PickerApplyPreview(state);
}

static BOOL PickerReadRgbFields(ModernColorPickerState* state,
                                COLORREF* color) {
    if (!state || !color) return FALSE;
    BOOL redValid = FALSE;
    BOOL greenValid = FALSE;
    BOOL blueValid = FALSE;
    UINT red = GetDlgItemInt(state->hwnd, IDC_MODERN_COLOR_RED_EDIT,
                             &redValid, FALSE);
    UINT green = GetDlgItemInt(state->hwnd, IDC_MODERN_COLOR_GREEN_EDIT,
                               &greenValid, FALSE);
    UINT blue = GetDlgItemInt(state->hwnd, IDC_MODERN_COLOR_BLUE_EDIT,
                              &blueValid, FALSE);
    if (!redValid || !greenValid || !blueValid ||
        red > 255 || green > 255 || blue > 255) {
        return FALSE;
    }
    *color = RGB(red, green, blue);
    return TRUE;
}

static void PickerHandleFieldChange(ModernColorPickerState* state,
                                    int controlId) {
    if (!state || state->updatingFields) return;
    COLORREF color = 0;
    if (controlId == IDC_MODERN_COLOR_HEX_EDIT) {
        wchar_t wideValue[16] = {0};
        char value[16] = {0};
        GetDlgItemTextW(state->hwnd, controlId, wideValue,
                        (int)_countof(wideValue));
        int copied = WideCharToMultiByte(CP_UTF8, 0, wideValue, -1, value,
                                         (int)sizeof(value), NULL, NULL);
        if (copied <= 0 || !ColorStringToColorRef(value, &color)) return;
    } else if (!PickerReadRgbFields(state, &color)) {
        return;
    }
    PickerSetColor(state, color, TRUE, TRUE);
}

static void PickerSaveCurrentColor(ModernColorPickerState* state) {
    if (!state || !state->customColors || state->customColorCapacity == 0) {
        return;
    }
    size_t existing = state->customColorCount;
    for (size_t i = 0; i < state->customColorCount; i++) {
        if (state->customColors[i] == state->selectedColor) {
            existing = i;
            break;
        }
    }
    if (existing < state->customColorCount) {
        for (size_t i = existing; i > 0; i--) {
            state->customColors[i] = state->customColors[i - 1];
        }
    } else {
        size_t count = state->customColorCount;
        if (count < state->customColorCapacity) count++;
        for (size_t i = count; i > 1; i--) {
            state->customColors[i - 1] = state->customColors[i - 2];
        }
        state->customColorCount = count;
    }
    state->customColors[0] = state->selectedColor;
    state->savedFocusIndex = 0;
    InvalidateRect(GetDlgItem(state->hwnd, IDC_MODERN_COLOR_SAVED),
                   NULL, FALSE);
}

static void PickerUpdateSvFromPoint(ModernColorPickerState* state,
                                    HWND control, int x, int y) {
    RECT rect = {0};
    GetClientRect(control, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 1 || height <= 1) return;
    state->saturation = PickerClampUnit((double)x / (width - 1));
    state->value = PickerClampUnit(1.0 - (double)y / (height - 1));
    PickerSetColorFromHsv(state, TRUE);
}

static void PickerUpdateHueFromPoint(ModernColorPickerState* state,
                                     HWND control, int y) {
    RECT rect = {0};
    GetClientRect(control, &rect);
    int height = rect.bottom - rect.top;
    if (height <= 1) return;
    double clamped = PickerClampUnit((double)y / (height - 1));
    state->hue = clamped * 359.999;
    state->svCachedHue = -1;
    PickerSetColorFromHsv(state, TRUE);
}

static int PickerSavedIndexFromPoint(HWND control, int x, int y) {
    RECT bounds = {0};
    GetClientRect(control, &bounds);
    POINT point = {x, y};
    for (int i = 0; i < 16; i++) {
        RECT cell;
        if (PickerGetSavedCellRect(&bounds, (size_t)i, &cell) &&
            PtInRect(&cell, point)) {
            return i;
        }
    }
    return -1;
}

static void PickerSelectSavedColor(ModernColorPickerState* state, int index) {
    if (!state || index < 0 || (size_t)index >= state->customColorCount) {
        return;
    }
    state->savedFocusIndex = index;
    PickerSetColor(state, state->customColors[index], TRUE, TRUE);
}

static BOOL PickerSampleScreenColor(COLORREF* color) {
    if (!color) return FALSE;
    POINT point;
    if (!GetCursorPos(&point)) return FALSE;
    HDC screen = GetDC(NULL);
    if (!screen) return FALSE;
    COLORREF sampled = GetPixel(screen, point.x, point.y);
    ReleaseDC(NULL, screen);
    if (sampled == CLR_INVALID) return FALSE;
    *color = sampled;
    return TRUE;
}

static void PickerBeginEyedropper(ModernColorPickerState* state) {
    if (!state || state->eyedropperActive) return;
    state->eyedropperOriginalColor = state->selectedColor;
    state->eyedropperActive = TRUE;
    SetCapture(state->hwnd);
    SetCursor(LoadCursorW(NULL, IDC_CROSS));
}

static void PickerEndEyedropper(ModernColorPickerState* state, BOOL keepColor) {
    if (!state || !state->eyedropperActive) return;
    state->eyedropperActive = FALSE;
    if (GetCapture() == state->hwnd) ReleaseCapture();
    if (!keepColor) {
        PickerSetColor(state, state->eyedropperOriginalColor, TRUE, TRUE);
    }
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

static void PickerLayoutControls(HWND hwndDlg) {
    UINT dpi = DialogModern_GetDpi(hwndDlg);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_SV, dpi,
                                0, 0, 330, 220);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_HUE, dpi,
                                342, 0, 24, 220);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_PREVIEW, dpi,
                                386, 0, 174, 46);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_HEX_LABEL, dpi,
                                386, 62, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_HEX_EDIT, dpi,
                                426, 56, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_RED_LABEL, dpi,
                                386, 106, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_RED_EDIT, dpi,
                                426, 100, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_GREEN_LABEL, dpi,
                                386, 148, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_GREEN_EDIT, dpi,
                                426, 142, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_BLUE_LABEL, dpi,
                                386, 190, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_BLUE_EDIT, dpi,
                                426, 184, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_SAVED, dpi,
                                0, 240, 366, 54);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_PICK_BUTTON, dpi,
                                386, 240, 82, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_SAVE_BUTTON, dpi,
                                478, 240, 82, 36);
    DialogModern_SetChildRect96(hwndDlg, IDCANCEL, dpi,
                                384, 316, 84, 36);
    DialogModern_SetChildRect96(hwndDlg, IDOK, dpi,
                                478, 316, 82, 36);
}

static void PickerSetupCanvas(HWND hwndDlg, int controlId,
                              ModernColorPickerState* state) {
    HWND control = GetDlgItem(hwndDlg, controlId);
    if (!control) return;
    SetWindowSubclass(control, ColorPickerCanvasSubclassProc,
                      (UINT_PTR)controlId, (DWORD_PTR)state);
}

static BOOL PickerHandleCanvasKey(ModernColorPickerState* state,
                                  UINT_PTR controlId, WPARAM key) {
    if (!state) return FALSE;
    double smallStep = GetKeyState(VK_SHIFT) < 0 ? 0.05 : 0.01;
    if (controlId == IDC_MODERN_COLOR_SV) {
        if (key == VK_LEFT) state->saturation -= smallStep;
        else if (key == VK_RIGHT) state->saturation += smallStep;
        else if (key == VK_UP) state->value += smallStep;
        else if (key == VK_DOWN) state->value -= smallStep;
        else return FALSE;
        PickerSetColorFromHsv(state, TRUE);
        return TRUE;
    }
    if (controlId == IDC_MODERN_COLOR_HUE) {
        double hueStep = GetKeyState(VK_SHIFT) < 0 ? 10.0 : 2.0;
        if (key == VK_LEFT || key == VK_UP) state->hue -= hueStep;
        else if (key == VK_RIGHT || key == VK_DOWN) state->hue += hueStep;
        else return FALSE;
        state->svCachedHue = -1;
        PickerSetColorFromHsv(state, TRUE);
        return TRUE;
    }
    if (controlId == IDC_MODERN_COLOR_SAVED &&
        state->customColorCount > 0) {
        int index = state->savedFocusIndex;
        if (index < 0 || (size_t)index >= state->customColorCount) index = 0;
        if (key == VK_LEFT) index--;
        else if (key == VK_RIGHT) index++;
        else if (key == VK_UP) index -= 8;
        else if (key == VK_DOWN) index += 8;
        else if (key == VK_RETURN || key == VK_SPACE) {
            PickerSelectSavedColor(state, index);
            return TRUE;
        } else {
            return FALSE;
        }
        if (index < 0) index = 0;
        if ((size_t)index >= state->customColorCount) {
            index = (int)state->customColorCount - 1;
        }
        state->savedFocusIndex = index;
        InvalidateRect(GetDlgItem(state->hwnd, IDC_MODERN_COLOR_SAVED),
                       NULL, FALSE);
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK ColorPickerCanvasSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    ModernColorPickerState* state = (ModernColorPickerState*)refData;
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            SetCapture(hwnd);
            if (subclassId == IDC_MODERN_COLOR_SV) {
                PickerUpdateSvFromPoint(state, hwnd,
                                        GET_X_LPARAM(lParam),
                                        GET_Y_LPARAM(lParam));
            } else if (subclassId == IDC_MODERN_COLOR_HUE) {
                PickerUpdateHueFromPoint(state, hwnd, GET_Y_LPARAM(lParam));
            } else if (subclassId == IDC_MODERN_COLOR_SAVED) {
                PickerSelectSavedColor(
                    state, PickerSavedIndexFromPoint(
                               hwnd, GET_X_LPARAM(lParam),
                               GET_Y_LPARAM(lParam)));
            }
            return 0;
        case WM_MOUSEMOVE:
            if ((wParam & MK_LBUTTON) && GetCapture() == hwnd) {
                if (subclassId == IDC_MODERN_COLOR_SV) {
                    PickerUpdateSvFromPoint(state, hwnd,
                                            GET_X_LPARAM(lParam),
                                            GET_Y_LPARAM(lParam));
                } else if (subclassId == IDC_MODERN_COLOR_HUE) {
                    PickerUpdateHueFromPoint(state, hwnd,
                                             GET_Y_LPARAM(lParam));
                }
            }
            break;
        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) ReleaseCapture();
            return 0;
        case WM_KEYDOWN:
            if (PickerHandleCanvasKey(state, subclassId, wParam)) return 0;
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(NULL,
                subclassId == IDC_MODERN_COLOR_SAVED ? IDC_HAND : IDC_CROSS));
            return TRUE;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ColorPickerCanvasSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static INT_PTR CALLBACK ModernColorPickerDlgProc(HWND hwndDlg, UINT msg,
                                                 WPARAM wParam, LPARAM lParam) {
    ModernColorPickerState* state =
        (ModernColorPickerState*)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);
    switch (msg) {
        case WM_INITDIALOG: {
            state = (ModernColorPickerState*)lParam;
            if (!state) return FALSE;
            state->hwnd = hwndDlg;
            state->svCachedHue = -1;
            state->savedFocusIndex =
                state->customColorCount > 0 ? 0 : -1;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);
            Dialog_InitializeInstance(DIALOG_INSTANCE_COLOR_PICKER, hwndDlg);
            PickerLayoutControls(hwndDlg);

            SetWindowTextW(hwndDlg,
                           GetLocalizedString(NULL, L"Set Color Value"));
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL,
                            GetLocalizedString(NULL, L"Cancel"));
            SetDlgItemTextW(hwndDlg, IDC_MODERN_COLOR_PICK_BUTTON,
                            GetLocalizedString(NULL, L"Pick"));
            SetDlgItemTextW(hwndDlg, IDC_MODERN_COLOR_SAVE_BUTTON,
                            GetLocalizedString(NULL, L"Save"));

            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_HEX_EDIT,
                                EM_SETLIMITTEXT, 7, 0);
            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_RED_EDIT,
                                EM_SETLIMITTEXT, 3, 0);
            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_GREEN_EDIT,
                                EM_SETLIMITTEXT, 3, 0);
            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_BLUE_EDIT,
                                EM_SETLIMITTEXT, 3, 0);

            PickerSetupCanvas(hwndDlg, IDC_MODERN_COLOR_SV, state);
            PickerSetupCanvas(hwndDlg, IDC_MODERN_COLOR_HUE, state);
            PickerSetupCanvas(hwndDlg, IDC_MODERN_COLOR_SAVED, state);
            PickerColorToHsv(state->selectedColor, 0.0, &state->hue,
                             &state->saturation, &state->value);
            PickerUpdateFields(state);
            SetFocus(GetDlgItem(hwndDlg, IDC_MODERN_COLOR_SV));
            return FALSE;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
            if (!state || !item) break;
            if (item->CtlID == IDC_MODERN_COLOR_SV) {
                PickerPaintSv(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_HUE) {
                PickerPaintHue(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_SAVED) {
                PickerPaintSavedColors(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_PREVIEW) {
                PickerPaintPreview(state, item);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            int notification = HIWORD(wParam);
            if (controlId == IDOK) {
                PickerEndEyedropper(state, TRUE);
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            if (controlId == IDCANCEL) {
                if (state && state->eyedropperActive) {
                    PickerEndEyedropper(state, FALSE);
                    return TRUE;
                }
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            if (controlId == IDC_MODERN_COLOR_PICK_BUTTON &&
                notification == BN_CLICKED) {
                PickerBeginEyedropper(state);
                return TRUE;
            }
            if (controlId == IDC_MODERN_COLOR_SAVE_BUTTON &&
                notification == BN_CLICKED) {
                PickerSaveCurrentColor(state);
                return TRUE;
            }
            if (notification == EN_CHANGE &&
                (controlId == IDC_MODERN_COLOR_HEX_EDIT ||
                 controlId == IDC_MODERN_COLOR_RED_EDIT ||
                 controlId == IDC_MODERN_COLOR_GREEN_EDIT ||
                 controlId == IDC_MODERN_COLOR_BLUE_EDIT)) {
                PickerHandleFieldChange(state, controlId);
                return TRUE;
            }
            if (notification == EN_KILLFOCUS &&
                (controlId == IDC_MODERN_COLOR_HEX_EDIT ||
                 controlId == IDC_MODERN_COLOR_RED_EDIT ||
                 controlId == IDC_MODERN_COLOR_GREEN_EDIT ||
                 controlId == IDC_MODERN_COLOR_BLUE_EDIT)) {
                PickerUpdateFields(state);
                return TRUE;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (state && state->eyedropperActive) {
                COLORREF sampled = 0;
                if (PickerSampleScreenColor(&sampled)) {
                    PickerSetColor(state, sampled, TRUE, TRUE);
                }
                return TRUE;
            }
            break;
        case WM_LBUTTONUP:
            if (state && state->eyedropperActive) {
                COLORREF sampled = 0;
                if (PickerSampleScreenColor(&sampled)) {
                    PickerSetColor(state, sampled, TRUE, TRUE);
                }
                PickerEndEyedropper(state, TRUE);
                return TRUE;
            }
            break;
        case WM_RBUTTONUP:
            if (state && state->eyedropperActive) {
                PickerEndEyedropper(state, FALSE);
                return TRUE;
            }
            break;
        case WM_CANCELMODE:
            if (state && state->eyedropperActive) {
                PickerEndEyedropper(state, FALSE);
                return TRUE;
            }
            break;
        case WM_CAPTURECHANGED:
            if (state && state->eyedropperActive &&
                (HWND)lParam != hwndDlg) {
                PickerEndEyedropper(state, FALSE);
            }
            break;
        case WM_SETCURSOR:
            if (state && state->eyedropperActive) {
                SetCursor(LoadCursorW(NULL, IDC_CROSS));
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (state && state->eyedropperActive) {
                    PickerEndEyedropper(state, FALSE);
                } else {
                    EndDialog(hwndDlg, IDCANCEL);
                }
                return TRUE;
            }
            break;
        case WM_DPICHANGED:
            PickerLayoutControls(hwndDlg);
            break;
        case WM_CLOSE:
            if (state) PickerEndEyedropper(state, FALSE);
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        case WM_DESTROY:
            if (state) {
                if (GetCapture() == hwndDlg) ReleaseCapture();
                free(state->svPixels);
                free(state->huePixels);
                state->svPixels = NULL;
                state->huePixels = NULL;
            }
            Dialog_UnregisterInstanceForWindow(
                DIALOG_INSTANCE_COLOR_PICKER, hwndDlg);
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            break;
    }
    return FALSE;
}

BOOL ModernColorPicker_Show(HWND hwndParent,
                            COLORREF initialColor,
                            COLORREF* customColors,
                            size_t customColorCapacity,
                            size_t* customColorCount,
                            COLORREF* selectedColor) {
    if (!hwndParent || !IsWindow(hwndParent) || !customColors ||
        customColorCapacity == 0 || !customColorCount || !selectedColor) {
        return FALSE;
    }
    if (Dialog_IsOpen(DIALOG_INSTANCE_COLOR_PICKER)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_COLOR_PICKER);
        SetForegroundWindow(existing);
        return FALSE;
    }

    COLORREF* workingColors =
        (COLORREF*)calloc(customColorCapacity, sizeof(*workingColors));
    if (!workingColors) return FALSE;
    size_t count = *customColorCount;
    if (count > customColorCapacity) count = customColorCapacity;
    memcpy(workingColors, customColors, count * sizeof(*workingColors));

    ModernColorPickerState state = {0};
    state.hwndParent = hwndParent;
    state.selectedColor = initialColor;
    state.customColors = workingColors;
    state.customColorCapacity = customColorCapacity;
    state.customColorCount = count;

    INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(NULL),
        MAKEINTRESOURCEW(IDD_MODERN_COLOR_PICKER_DIALOG),
        hwndParent, ModernColorPickerDlgProc, (LPARAM)&state);
    BOOL accepted = result == IDOK;
    if (accepted) {
        memcpy(customColors, workingColors,
               state.customColorCount * sizeof(*customColors));
        *customColorCount = state.customColorCount;
        *selectedColor = state.selectedColor;
    }
    free(workingColors);
    return accepted;
}
