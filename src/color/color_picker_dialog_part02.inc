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
