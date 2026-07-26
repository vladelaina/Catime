/**
 * @file dialog_modern_state.c
 * @brief State lookup, resources, capture, and basic measurements.
 */

#include "dialog_modern_internal.h"

COLORREF ModernBlendColor(COLORREF from, COLORREF to, int toPercent) {
    if (toPercent < 0) toPercent = 0;
    if (toPercent > 100) toPercent = 100;
    int fromPercent = 100 - toPercent;
    return RGB(
        (GetRValue(from) * fromPercent + GetRValue(to) * toPercent) / 100,
        (GetGValue(from) * fromPercent + GetGValue(to) * toPercent) / 100,
        (GetBValue(from) * fromPercent + GetBValue(to) * toPercent) / 100);
}

BOOL ModernUpdateTitleHover(ModernDialogState* state, POINT point) {
    if (!state || !state->finalized) return FALSE;
    BOOL hovered = PtInRect(&state->titleFrame, point);
    if (hovered == state->titleHovered) return FALSE;
    state->titleHovered = hovered;
    InvalidateRect(state->hwnd, &state->titleFrame, FALSE);
    return TRUE;
}

void ModernRefreshTitleHoverFromCursor(ModernDialogState* state) {
    POINT point = {0};
    if (!state || !GetCursorPos(&point)) return;
    ScreenToClient(state->hwnd, &point);
    ModernUpdateTitleHover(state, point);
}

ModernDialogState* ModernGetState(HWND hwnd) {
    return hwnd ? (ModernDialogState*)GetPropW(hwnd, MODERN_DIALOG_STATE_PROP)
                : NULL;
}
void ModernDeleteFonts(ModernDialogState* state) {
    if (!state) return;
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->bodyFont) DeleteObject(state->bodyFont);
    if (state->labelFont) DeleteObject(state->labelFont);
    if (state->editFont) DeleteObject(state->editFont);
    if (state->buttonFont) DeleteObject(state->buttonFont);
    state->titleFont = NULL;
    state->bodyFont = NULL;
    state->labelFont = NULL;
    state->editFont = NULL;
    state->buttonFont = NULL;
}

void ModernDeleteBrushes(ModernDialogState* state) {
    if (!state) return;
    if (state->backgroundBrush) DeleteObject(state->backgroundBrush);
    if (state->surfaceBrush) DeleteObject(state->surfaceBrush);
    if (state->fieldBrush) DeleteObject(state->fieldBrush);
    state->backgroundBrush = NULL;
    state->surfaceBrush = NULL;
    state->fieldBrush = NULL;
}

void ModernRebuildResources(ModernDialogState* state) {
    if (!state) return;
    ModernDeleteFonts(state);
    ModernDeleteBrushes(state);
    DialogModern_ResolvePalette(&state->palette);
    DialogModern_ApplyTheme(state->hwnd, state->palette.darkMode);
    state->backgroundBrush = CreateSolidBrush(state->palette.background);
    state->surfaceBrush = CreateSolidBrush(state->palette.surface);
    state->fieldBrush = CreateSolidBrush(state->palette.field);
    state->titleFont = DialogModern_CreateFont(state->dpi, 20, FW_SEMIBOLD);
    state->bodyFont = DialogModern_CreateFont(state->dpi, 12, FW_NORMAL);
    state->labelFont = DialogModern_CreateFont(state->dpi, 11, FW_SEMIBOLD);
    state->editFont = DialogModern_CreateFont(state->dpi, 14, FW_NORMAL);
    state->buttonFont = DialogModern_CreateFont(state->dpi, 12, FW_SEMIBOLD);
}

int ModernTo96(UINT dpi, int value) {
    return MulDiv(value, 96, (int)(dpi ? dpi : 96u));
}

BOOL ModernEnsureControlCapacity(ModernDialogState* state, size_t count) {
    if (state->controlCapacity >= count) return TRUE;
    size_t capacity = state->controlCapacity ? state->controlCapacity * 2 : 32;
    while (capacity < count) capacity *= 2;
    ModernControl* controls =
        (ModernControl*)realloc(state->controls, capacity * sizeof(*controls));
    if (!controls) return FALSE;
    state->controls = controls;
    state->controlCapacity = capacity;
    return TRUE;
}

ModernControlKind ModernClassifyControl(HWND hwnd) {
    wchar_t className[64] = {0};
    GetClassNameW(hwnd, className, _countof(className));
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    if (_wcsicmp(className, L"Button") == 0) {
        UINT type = (UINT)style & BS_TYPEMASK;
        if (type == BS_GROUPBOX) return MODERN_CONTROL_GROUP;
        if (type == BS_AUTOCHECKBOX || type == BS_CHECKBOX ||
            type == BS_3STATE || type == BS_AUTO3STATE) {
            return MODERN_CONTROL_CHECK;
        }
        if (type == BS_AUTORADIOBUTTON || type == BS_RADIOBUTTON) {
            return MODERN_CONTROL_RADIO;
        }
        if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON ||
            type == BS_OWNERDRAW) {
            return MODERN_CONTROL_PUSH;
        }
    }
    if (_wcsicmp(className, L"Edit") == 0 ||
        _wcsicmp(className, L"msctls_hotkey32") == 0) {
        return MODERN_CONTROL_FIELD;
    }
    if (_wcsicmp(className, L"ListBox") == 0) return MODERN_CONTROL_LIST;
    if (_wcsicmp(className, L"ComboBox") == 0 ||
        _wcsicmp(className, L"SysDateTimePick32") == 0) {
        return MODERN_CONTROL_COMBO;
    }
    if (_wcsicmp(className, L"msctls_trackbar32") == 0) {
        return MODERN_CONTROL_SLIDER;
    }
    return MODERN_CONTROL_OTHER;
}

BOOL ModernWindowHasClass(HWND hwnd, const wchar_t* expected) {
    wchar_t className[64] = {0};
    return hwnd && expected &&
           GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           _wcsicmp(className, expected) == 0;
}

BOOL ModernIsDateTimeControl(const ModernControl* control) {
    return control && control->kind == MODERN_CONTROL_COMBO &&
           ModernWindowHasClass(control->hwnd, L"SysDateTimePick32");
}

BOOL CALLBACK ModernAttachDateTimeChild(HWND child, LPARAM data) {
    ModernControl* control = (ModernControl*)data;
    if (ModernWindowHasClass(child, L"msctls_updown32")) {
        SetWindowSubclass(child, ModernDateTimeChildSubclassProc,
                          MODERN_DATETIME_CHILD_SUBCLASS_ID,
                          (DWORD_PTR)control);
        ShowWindow(child, SW_HIDE);
    }
    return TRUE;
}

void ModernHideDateTimeSpinner(ModernControl* control) {
    if (!ModernIsDateTimeControl(control)) return;
    EnumChildWindows(control->hwnd, ModernAttachDateTimeChild,
                     (LPARAM)control);
}

BOOL ModernIsPrimaryButton(int id) {
    return id == IDOK || id == IDYES || id == CLOCK_IDC_BUTTON_OK ||
           id == IDC_PLUGIN_SECURITY_TRUST_BTN ||
           id == IDC_FONT_LICENSE_AGREE_BTN;
}

BOOL CALLBACK ModernCaptureChild(HWND child, LPARAM lParam) {
    ModernDialogState* state = (ModernDialogState*)lParam;
    if (!state || GetParent(child) != state->hwnd) {
        return TRUE;
    }
    if (!ModernEnsureControlCapacity(state, state->controlCount + 1)) {
        return FALSE;
    }

    ModernControl* control = &state->controls[state->controlCount++];
    ZeroMemory(control, sizeof(*control));
    control->owner = state;
    control->hwnd = child;
    control->comboHotItem = -1;
    control->dateTimeHotPart = MODERN_DATETIME_HIT_NONE;
    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
    control->id = GetDlgCtrlID(child);
    control->kind = ModernClassifyControl(child);
    control->primary = ModernIsPrimaryButton(control->id);
    control->sourceVisible =
        (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0;

    RECT rect = {0};
    GetWindowRect(child, &rect);
    MapWindowPoints(NULL, state->hwnd, (POINT*)&rect, 2);
    control->source96.left = ModernTo96(state->dpi, rect.left);
    control->source96.top = ModernTo96(state->dpi, rect.top);
    control->source96.right = ModernTo96(state->dpi, rect.right);
    control->source96.bottom = ModernTo96(state->dpi, rect.bottom);
    return TRUE;
}

int ModernControlCompareX(const void* left, const void* right) {
    const ModernControl* const* a = (const ModernControl* const*)left;
    const ModernControl* const* b = (const ModernControl* const*)right;
    return (*a)->source96.left - (*b)->source96.left;
}

void ModernMeasureText(HWND hwnd, HFONT font, SIZE* size) {
    if (!size) return;
    size->cx = 0;
    size->cy = 0;
    wchar_t text[256] = {0};
    GetWindowTextW(hwnd, text, (int)_countof(text));
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), size);
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(hwnd, hdc);
}
