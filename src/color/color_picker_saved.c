/**
 * @file color_picker_saved.c
 * @brief Paints, stores, and selects saved picker colors.
 */

#include "color/color_picker_internal.h"
#include "../resource/resource.h"

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

void ColorPickerInternal_PaintSavedColors(ModernColorPickerState* state,
                                   const DRAWITEMSTRUCT* item) {
    DialogModernPalette palette;
    ColorPickerInternal_RefreshPalette(state, &palette);
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
            ColorPickerInternal_DrawOutline(
                item->hDC, &selected,
                DialogModern_Scale(dpi, 6),
                palette.accent, 2);
            PickerDrawSavedCheck(item->hDC, &cell, fill, dpi);
        }
        if (GetFocus() == item->hwndItem &&
            (int)i == state->savedFocusIndex) {
            RECT focused = cell;
            InflateRect(&focused, 2, 2);
            ColorPickerInternal_DrawOutline(
                item->hDC, &focused,
                DialogModern_Scale(dpi, 8),
                palette.accent, 2);
        }
    }
}

void ColorPickerInternal_PaintPreview(ModernColorPickerState* state,
                               const DRAWITEMSTRUCT* item) {
    DialogModernPalette palette;
    ColorPickerInternal_RefreshPalette(state, &palette);
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

void ColorPickerInternal_SaveCurrentColor(ModernColorPickerState* state) {
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

int ColorPickerInternal_SavedIndexFromPoint(HWND control, int x, int y) {
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

void ColorPickerInternal_SelectSavedColor(ModernColorPickerState* state, int index) {
    if (!state || index < 0 || (size_t)index >= state->customColorCount) {
        return;
    }
    state->savedFocusIndex = index;
    ColorPickerInternal_SetColor(state, state->customColors[index], TRUE, TRUE);
}
