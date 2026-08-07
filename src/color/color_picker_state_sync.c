/**
 * @file color_picker_state_sync.c
 * @brief Synchronizes picker fields, HSV state, and live preview.
 */

#include "color/color_picker_internal.h"
#include "color/color_parser.h"
#include "color/color_state.h"
#include "menu_preview.h"
#include "../resource/resource.h"

#include <stdio.h>

static void PickerInvalidateVisuals(ModernColorPickerState* state) {
    if (!state || !state->hwnd) return;
    int controls[] = {
        IDC_MODERN_COLOR_SV,
        IDC_MODERN_COLOR_HUE,
        IDC_MODERN_COLOR_PREVIEW
    };
    /* Saved swatches change only when explicitly saved, not while dragging. */
    for (size_t i = 0; i < _countof(controls); i++) {
        HWND control = GetDlgItem(state->hwnd, controls[i]);
        if (control) InvalidateRect(control, NULL, FALSE);
    }
}

void ColorPickerInternal_UpdateFields(ModernColorPickerState* state) {
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

void ColorPickerInternal_SetColor(ModernColorPickerState* state, COLORREF color,
                           BOOL updateHsv, BOOL preview) {
    if (!state) return;
    state->selectedColor = color;
    if (updateHsv) {
        ColorPickerInternal_ColorToHsv(color, state->hue, &state->hue,
                         &state->saturation, &state->value);
    }
    ColorPickerInternal_UpdateFields(state);
    PickerInvalidateVisuals(state);
    if (preview) PickerApplyPreview(state);
}

void ColorPickerInternal_SetColorFromHsv(ModernColorPickerState* state, BOOL preview) {
    if (!state) return;
    state->hue = ColorPickerInternal_NormalizeHue(state->hue);
    state->saturation = ColorPickerInternal_ClampUnit(state->saturation);
    state->value = ColorPickerInternal_ClampUnit(state->value);
    state->selectedColor =
        ColorPickerInternal_HsvToColor(state->hue, state->saturation, state->value);
    ColorPickerInternal_UpdateFields(state);
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

void ColorPickerInternal_HandleFieldChange(ModernColorPickerState* state,
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
    ColorPickerInternal_SetColor(state, color, TRUE, TRUE);
}
