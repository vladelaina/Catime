    }
    if (footerCount > 1) footerWidth += (int)(footerCount - 1) * 10;

    int bodyMaxY = minY;
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->sourceVisible || control->footer) continue;
        if (control->source96.bottom > bodyMaxY) bodyMaxY = control->source96.bottom;
    }
    if (bodyMaxY <= minY) bodyMaxY = maxY;

    state->contentMinX96 = minX;
    state->contentMinY96 = minY;
    state->contentWidth96 = maxX - minX;
    state->bodyHeight96 = bodyMaxY - minY;
    state->headerHeight96 = state->bodyHeight96 > 620 ? 66 : 72;
    state->sidePadding96 = state->bodyHeight96 > 620 ? 18 : 24;
    state->bottomPadding96 = state->bodyHeight96 > 620 ? 14 : 20;
    state->footerHeight96 = footerCount ? 36 : 0;
    state->hasFooter = footerCount > 0;

    int desiredWidth = state->contentWidth96 + state->sidePadding96 * 2;
    int footerDesiredWidth = footerWidth + state->sidePadding96 * 2;
    if (desiredWidth < footerDesiredWidth) desiredWidth = footerDesiredWidth;

    SIZE titleSize = {0};
    ModernMeasureText(state->hwnd, state->titleFont, &titleSize);
    int titleDesiredWidth = ModernTo96(state->dpi, titleSize.cx) +
                            state->sidePadding96 * 2 + 64;
    if (desiredWidth < titleDesiredWidth) desiredWidth = titleDesiredWidth;
    if (desiredWidth < 360) desiredWidth = 360;
    state->desiredClientWidth96 = desiredWidth;
    state->clientWidth96 = desiredWidth;

    int footerSpace = footerCount ? 24 + state->footerHeight96 : 0;
    state->desiredClientHeight96 = state->headerHeight96 +
                                   state->bodyHeight96 + footerSpace +
                                   state->bottomPadding96;
    state->clientHeight96 = state->desiredClientHeight96;
    state->footerY96 = state->clientHeight96 - state->bottomPadding96 -
                       state->footerHeight96;
}

static void ModernSetControlFont(const ModernDialogState* state,
                                 const ModernControl* control) {
    HFONT font = state->bodyFont;
    if (control->kind == MODERN_CONTROL_FIELD ||
        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO) {
        font = state->editFont;
    } else if (control->kind == MODERN_CONTROL_GROUP) {
        font = state->labelFont;
    } else if (control->kind == MODERN_CONTROL_PUSH ||
               control->kind == MODERN_CONTROL_CLOSE) {
        font = state->buttonFont;
    }
    if (font) SendMessageW(control->hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    if (control->kind == MODERN_CONTROL_FIELD ||
        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO ||
        control->kind == MODERN_CONTROL_SLIDER) {
        DialogModern_ApplyTheme(control->hwnd, state->palette.darkMode);
    }
    if (control->kind == MODERN_CONTROL_COMBO &&
        !ModernIsDateTimeControl(control)) {
        ModernAttachComboList((ModernControl*)control);
    }
}

static BOOL ModernApplyFieldRegionRaw(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return FALSE;
    if (control->kind != MODERN_CONTROL_FIELD &&
        control->kind != MODERN_CONTROL_LIST &&
        control->kind != MODERN_CONTROL_COMBO) return FALSE;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int radius = DialogModern_Scale(control->owner->dpi, 9);
    HRGN region = CreateRoundRectRgn(client.left, client.top,
                                     client.right + 1, client.bottom + 1,
                                     radius * 2, radius * 2);
    if (!region) return FALSE;
    if (!SetWindowRgn(control->hwnd, region, redraw)) {
        DeleteObject(region);
        return FALSE;
    }
    return TRUE;
}

static void ModernApplyFieldRegion(ModernControl* control) {
    if (!control) return;
    control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
    ModernApplyFieldRegionRaw(control, TRUE);
}

/* Compact visual single-line edits use the multiline formatting rectangle so
 * Win32 can center text without replacing its native editing behavior. */
static BOOL ModernIsCompactEdit(const ModernControl* control) {
    if (!control || control->kind != MODERN_CONTROL_FIELD ||
        !ModernWindowHasClass(control->hwnd, L"Edit")) {
        return FALSE;
    }

    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    if ((style & (ES_MULTILINE | ES_AUTOHSCROLL)) !=
            (ES_MULTILINE | ES_AUTOHSCROLL) ||
        (style & (ES_WANTRETURN | WS_VSCROLL)) != 0) {
        return FALSE;
    }

    RECT client = {0};
    if (!GetClientRect(control->hwnd, &client)) return FALSE;
    return client.bottom - client.top <=
           DialogModern_Scale(control->owner->dpi, 56);
}

static void ModernApplyEditLayout(ModernControl* control) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || control->kind != MODERN_CONTROL_FIELD ||
        !ModernWindowHasClass(control->hwnd, L"Edit")) {
        return;
    }

    int horizontalInset = DialogModern_Scale(state->dpi, 12);
    if (!ModernIsCompactEdit(control)) {
        SendMessageW(control->hwnd, EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELONG(horizontalInset, horizontalInset));
        return;
    }

    RECT client = {0};
    if (!GetClientRect(control->hwnd, &client)) return;
    HDC hdc = GetDC(control->hwnd);
    if (!hdc) return;

    HFONT font = (HFONT)SendMessageW(control->hwnd, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    TEXTMETRICW metrics = {0};
    if (GetTextMetricsW(hdc, &metrics)) {
        int height = client.bottom - client.top;
        int lineHeight = metrics.tmHeight + metrics.tmExternalLeading;
        int verticalInset = max(DialogModern_Scale(state->dpi, 3),
                                (height - lineHeight) / 2);
        RECT formatRect = {
            horizontalInset,
            verticalInset,
            max(horizontalInset + 1, client.right - horizontalInset),
            max(verticalInset + 1, client.bottom - verticalInset)
        };
        SendMessageW(control->hwnd, EM_SETRECTNP, 0,
                     (LPARAM)&formatRect);
    }
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(control->hwnd, hdc);
}

static wchar_t* ModernCreateSingleLineText(const wchar_t* source,
                                           size_t length,
                                           BOOL* changed) {
    if (changed) *changed = FALSE;
    if (!source) return NULL;

    wchar_t* result = (wchar_t*)malloc((length + 1) * sizeof(*result));
    if (!result) return NULL;

    size_t output = 0;
    BOOL pendingSpace = FALSE;
    for (size_t i = 0; i < length; i++) {
        wchar_t ch = source[i];
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') {
            pendingSpace = output > 0;
            if (changed) *changed = TRUE;
            continue;
        }
        if (pendingSpace && result[output - 1] != L' ') {
            result[output++] = L' ';
        }
        pendingSpace = FALSE;
        result[output++] = ch;
    }
    result[output] = L'\0';
    return result;
}

static BOOL ModernPasteCompactEdit(HWND hwnd) {
    if (!hwnd || !IsClipboardFormatAvailable(CF_UNICODETEXT) ||
        !OpenClipboard(hwnd)) {
        return FALSE;
    }

    BOOL handled = FALSE;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const wchar_t* source = data ? (const wchar_t*)GlobalLock(data) : NULL;
    if (source) {
        SIZE_T capacity = GlobalSize(data) / sizeof(*source);
        size_t length = 0;
        while (length < capacity && source[length] != L'\0') length++;
        if (length < capacity) {
            BOOL changed = FALSE;
            wchar_t* text = ModernCreateSingleLineText(
                source, length, &changed);
            if (text && changed) {
                SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)text);
                handled = TRUE;
            }
            free(text);
        }
        GlobalUnlock(data);
    }
    CloseClipboard();
    return handled;
}

static void ModernStyleControl(ModernDialogState* state,
                               ModernControl* control) {
    if (!state || !control || !control->hwnd) return;
    ModernSetControlFont(state, control);

    if (control->kind == MODERN_CONTROL_PUSH) {
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        style = (style & ~(BS_TYPEMASK | BS_NOTIFY)) | BS_OWNERDRAW;
        SetWindowLongPtrW(control->hwnd, GWL_STYLE, style);
    } else if (control->kind == MODERN_CONTROL_FIELD ||
               control->kind == MODERN_CONTROL_LIST ||
               control->kind == MODERN_CONTROL_COMBO) {
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        LONG_PTR exStyle = GetWindowLongPtrW(control->hwnd, GWL_EXSTYLE);
        SetWindowLongPtrW(control->hwnd, GWL_STYLE, style & ~WS_BORDER);
        SetWindowLongPtrW(control->hwnd, GWL_EXSTYLE,
                          exStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        if (control->kind == MODERN_CONTROL_COMBO &&
            !ModernIsDateTimeControl(control)) {
            style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
            SetWindowLongPtrW(control->hwnd, GWL_STYLE,
                              style | CBS_HASSTRINGS);
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, (WPARAM)-1,
                         DialogModern_Scale(state->dpi, 30));
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, 0,
                         DialogModern_Scale(state->dpi, 34));
            SendMessageW(control->hwnd, CB_SETMINVISIBLE,
                         MODERN_COMBO_VISIBLE_ITEMS, 0);
            ModernAttachComboList(control);
        }
        ModernApplyFieldRegion(control);
        if (control->kind == MODERN_CONTROL_FIELD) {
            ModernApplyEditLayout(control);
        }
    }

    if (ModernIsDateTimeControl(control)) {
        SendMessageW(control->hwnd, DTM_SETFORMATW, 0,
                     (LPARAM)L"HH':'mm':'ss");
        ModernHideDateTimeSpinner(control);
    }

    if (control->kind == MODERN_CONTROL_PUSH ||
        control->kind == MODERN_CONTROL_CHECK ||
        control->kind == MODERN_CONTROL_RADIO ||
        control->kind == MODERN_CONTROL_GROUP ||
        control->kind == MODERN_CONTROL_FIELD ||
