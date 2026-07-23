static BOOL ModernUpdateTitleHover(ModernDialogState* state, POINT point) {
    if (!state || !state->finalized) return FALSE;
    BOOL hovered = PtInRect(&state->titleFrame, point);
    if (hovered == state->titleHovered) return FALSE;
    state->titleHovered = hovered;
    InvalidateRect(state->hwnd, &state->titleFrame, FALSE);
    return TRUE;
}

static void ModernRefreshTitleHoverFromCursor(ModernDialogState* state) {
    POINT point = {0};
    if (!state || !GetCursorPos(&point)) return;
    ScreenToClient(state->hwnd, &point);
    ModernUpdateTitleHover(state, point);
}

static ModernDialogState* ModernGetState(HWND hwnd) {
    return hwnd ? (ModernDialogState*)GetPropW(hwnd, MODERN_DIALOG_STATE_PROP)
                : NULL;
}

BOOL DialogModern_CopyPalette(HWND hwnd, DialogModernPalette* palette) {
    if (!palette) return FALSE;
    const ModernDialogState* state = ModernGetState(hwnd);
    if (!state && hwnd) state = ModernGetState(GetParent(hwnd));
    if (state && state->surfaceBrush) {
        *palette = state->palette;
        return TRUE;
    }
    DialogModern_ResolvePalette(palette);
    return TRUE;
}

BOOL DialogModern_IsAttached(HWND hwndDlg) {
    return ModernGetState(hwndDlg) != NULL;
}

static void ModernDeleteFonts(ModernDialogState* state) {
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

static void ModernDeleteBrushes(ModernDialogState* state) {
    if (!state) return;
    if (state->backgroundBrush) DeleteObject(state->backgroundBrush);
    if (state->surfaceBrush) DeleteObject(state->surfaceBrush);
    if (state->fieldBrush) DeleteObject(state->fieldBrush);
    state->backgroundBrush = NULL;
    state->surfaceBrush = NULL;
    state->fieldBrush = NULL;
}

static void ModernRebuildResources(ModernDialogState* state) {
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

static int ModernTo96(UINT dpi, int value) {
    return MulDiv(value, 96, (int)(dpi ? dpi : 96u));
}

static BOOL ModernEnsureControlCapacity(ModernDialogState* state, size_t count) {
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

static ModernControlKind ModernClassifyControl(HWND hwnd) {
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

static BOOL ModernWindowHasClass(HWND hwnd, const wchar_t* expected) {
    wchar_t className[64] = {0};
    return hwnd && expected &&
           GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           _wcsicmp(className, expected) == 0;
}

static BOOL ModernIsDateTimeControl(const ModernControl* control) {
    return control && control->kind == MODERN_CONTROL_COMBO &&
           ModernWindowHasClass(control->hwnd, L"SysDateTimePick32");
}

static BOOL CALLBACK ModernAttachDateTimeChild(HWND child, LPARAM data) {
    ModernControl* control = (ModernControl*)data;
    if (ModernWindowHasClass(child, L"msctls_updown32")) {
        SetWindowSubclass(child, ModernDateTimeChildSubclassProc,
                          MODERN_DATETIME_CHILD_SUBCLASS_ID,
                          (DWORD_PTR)control);
        ShowWindow(child, SW_HIDE);
    }
    return TRUE;
}

static void ModernHideDateTimeSpinner(ModernControl* control) {
    if (!ModernIsDateTimeControl(control)) return;
    EnumChildWindows(control->hwnd, ModernAttachDateTimeChild,
                     (LPARAM)control);
}

static BOOL ModernIsPrimaryButton(int id) {
    return id == IDOK || id == IDYES || id == CLOCK_IDC_BUTTON_OK ||
           id == IDC_PLUGIN_SECURITY_TRUST_BTN ||
           id == IDC_FONT_LICENSE_AGREE_BTN;
}

static BOOL CALLBACK ModernCaptureChild(HWND child, LPARAM lParam) {
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

static int ModernControlCompareX(const void* left, const void* right) {
    const ModernControl* const* a = (const ModernControl* const*)left;
    const ModernControl* const* b = (const ModernControl* const*)right;
    return (*a)->source96.left - (*b)->source96.left;
}

static void ModernMeasureText(HWND hwnd, HFONT font, SIZE* size) {
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

static void ModernAnalyzeLayout(ModernDialogState* state) {
    int minX = INT_MAX;
    int minY = INT_MAX;
    int maxX = INT_MIN;
    int maxY = INT_MIN;

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->sourceVisible) continue;
        if (control->source96.left < minX) minX = control->source96.left;
        if (control->source96.top < minY) minY = control->source96.top;
        if (control->source96.right > maxX) maxX = control->source96.right;
        if (control->source96.bottom > maxY) maxY = control->source96.bottom;
    }

    if (minX == INT_MAX) {
        RECT client = {0};
        GetClientRect(state->hwnd, &client);
        minX = minY = 0;
        maxX = ModernTo96(state->dpi, client.right);
        maxY = ModernTo96(state->dpi, client.bottom);
    }

    int totalHeight = maxY - minY;
    int footerThreshold = maxY - 34;
    size_t footerCount = 0;
    int footerWidth = 0;
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->sourceVisible ||
            control->kind != MODERN_CONTROL_PUSH) continue;
        if (control->source96.bottom >= footerThreshold ||
            control->source96.top >= minY + totalHeight * 3 / 4) {
            control->footer = TRUE;
            SIZE textSize = {0};
            ModernMeasureText(control->hwnd, state->buttonFont, &textSize);
            int textWidth96 = ModernTo96(state->dpi, textSize.cx);
            int originalWidth = control->source96.right - control->source96.left;
            int desiredWidth = textWidth96 + 30;
            if (desiredWidth < 80) desiredWidth = 80;
            if (desiredWidth < originalWidth) desiredWidth = originalWidth;
            control->source96.right = control->source96.left + desiredWidth;
            footerWidth += desiredWidth;
            footerCount++;
        }
