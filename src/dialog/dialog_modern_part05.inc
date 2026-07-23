        DialogModern_Scale(state->dpi, 4),
        thumbColor, thumbColor, 0);
}

static void ModernEnsureControlVisible(ModernControl* control) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control || control->footer || control->kind == MODERN_CONTROL_CLOSE ||
        state->bodyScrollMax96 <= 0) {
        return;
    }

    int top96 = control->source96.top - state->contentMinY96;
    int bottom96 = control->source96.bottom - state->contentMinY96;
    int offset = state->bodyScrollOffset96;
    if (top96 < offset) {
        offset = top96;
    } else if (bottom96 > offset + state->bodyViewportHeight96) {
        offset = bottom96 - state->bodyViewportHeight96;
    }
    ModernSetBodyScrollOffset(state, offset);
}

static BOOL ModernAppendCloseButton(ModernDialogState* state) {
    const wchar_t* cancel = GetLocalizedString(NULL, L"Cancel");
    state->closeButton = CreateWindowExW(
        0, L"BUTTON", cancel ? cancel : L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, state->hwnd,
        (HMENU)(INT_PTR)MODERN_DIALOG_CLOSE_ID,
        GetModuleHandleW(NULL), NULL);
    if (!state->closeButton) {
        return FALSE;
    }
    if (!ModernEnsureControlCapacity(state, state->controlCount + 1)) {
        DestroyWindow(state->closeButton);
        state->closeButton = NULL;
        return FALSE;
    }

    ModernControl* control = &state->controls[state->controlCount];
    ZeroMemory(control, sizeof(*control));
    control->owner = state;
    control->hwnd = state->closeButton;
    control->id = MODERN_DIALOG_CLOSE_ID;
    control->kind = MODERN_CONTROL_CLOSE;
    control->sourceVisible = TRUE;
    ModernSetControlFont(state, control);
    if (!SetWindowSubclass(control->hwnd, ModernControlSubclassProc,
                           MODERN_CONTROL_SUBCLASS_ID, (DWORD_PTR)control)) {
        DestroyWindow(state->closeButton);
        state->closeButton = NULL;
        ZeroMemory(control, sizeof(*control));
        return FALSE;
    }
    state->controlCount++;
    return TRUE;
}

static void ModernCommitClientSize(ModernDialogState* state,
                                   int width, int height) {
    state->clientWidth96 = ModernTo96(state->dpi, width);
    state->clientHeight96 = ModernTo96(state->dpi, height);
    if (state->hasFooter) {
        state->footerY96 = state->clientHeight96 - state->bottomPadding96 -
                           state->footerHeight96;
    }
}

/* A dialog can be constrained by the work area or resized by accessibility
 * tooling. Keep the layout model tied to the actual client region so the
 * footer remains anchored and the body can scroll into the remaining space. */
static void ModernSyncClientSizeFromWindow(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    RECT client = {0};
    if (!GetClientRect(state->hwnd, &client)) return;

    int width96 = ModernTo96(state->dpi, client.right - client.left);
    int height96 = ModernTo96(state->dpi, client.bottom - client.top);
    if (width96 > 0) state->clientWidth96 = width96;
    if (height96 > 0) state->clientHeight96 = height96;
    if (state->hasFooter) {
        state->footerY96 = state->clientHeight96 -
                           state->bottomPadding96 - state->footerHeight96;
    }
}

static void ModernCenterAndResize(ModernDialogState* state) {
    int width = DialogModern_Scale(state->dpi,
                                   state->desiredClientWidth96);
    int height = DialogModern_Scale(state->dpi,
                                    state->desiredClientHeight96);
    HMONITOR monitor = MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        ModernCommitClientSize(state, width, height);
        SetWindowPos(state->hwnd, NULL, 0, 0, width, height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
        return;
    }

    int maxWidth = info.rcWork.right - info.rcWork.left -
                   DialogModern_Scale(state->dpi, 24);
    int maxHeight = info.rcWork.bottom - info.rcWork.top -
                    DialogModern_Scale(state->dpi, 24);
    if (width > maxWidth) width = maxWidth;
    if (height > maxHeight) height = maxHeight;
    ModernCommitClientSize(state, width, height);
    int x = info.rcWork.left +
            ((info.rcWork.right - info.rcWork.left) - width) / 2;
    int y = info.rcWork.top +
            ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
    SetWindowPos(state->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static BOOL ModernFinalize(ModernDialogState* state) {
    if (!state || state->finalized || state->finalizing) return state != NULL;
    state->finalizing = TRUE;
    state->dpi = DialogModern_GetDpi(state->hwnd);
    ModernRebuildResources(state);

    state->controlCount = 0;
    if (!EnumChildWindows(state->hwnd, ModernCaptureChild, (LPARAM)state)) {
        state->finalizing = FALSE;
        return FALSE;
    }
    ModernAnalyzeLayout(state);

    if (!ModernAppendCloseButton(state)) {
        state->finalizing = FALSE;
        return FALSE;
    }

    LONG_PTR style = GetWindowLongPtrW(state->hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(state->hwnd, GWL_EXSTYLE);
    style &= ~(WS_CAPTION | WS_DLGFRAME | WS_THICKFRAME | WS_BORDER |
               WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    style |= WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    exStyle |= WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT;
    SetWindowLongPtrW(state->hwnd, GWL_STYLE, style);
    SetWindowLongPtrW(state->hwnd, GWL_EXSTYLE, exStyle);

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernStyleControl(state, &state->controls[i]);
    }
    ModernSetDefaultButton(state);

    state->finalized = TRUE;
    state->finalizing = FALSE;
    ModernCenterAndResize(state);
    ModernLayoutControls(state);
    DialogModern_ApplyWindowShape(state->hwnd, state->dpi, 20);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    return TRUE;
}

static ModernControl* ModernFindControl(ModernDialogState* state, HWND hwnd) {
    if (!state || !hwnd) return NULL;
    for (size_t i = 0; i < state->controlCount; i++) {
        if (state->controls[i].hwnd == hwnd) return &state->controls[i];
    }
    return NULL;
}

static void ModernDrawButton(ModernDialogState* state,
                             const DRAWITEMSTRUCT* item) {
    ModernControl* control = ModernFindControl(state, item->hwndItem);
    if (!control) return;

    RECT rect = item->rcItem;
    HBRUSH surface = CreateSolidBrush(state->palette.surface);
    FillRect(item->hDC, &rect, surface);
    DeleteObject(surface);
    BOOL disabled = (item->itemState & ODS_DISABLED) != 0;
    BOOL pressed = control->pressed || (item->itemState & ODS_SELECTED) != 0;
    BOOL focused = control->focused || (item->itemState & ODS_FOCUS) != 0;

    if (control->kind == MODERN_CONTROL_CLOSE) {
        DialogModern_DrawCloseButton(item->hDC, &rect, state->dpi,
                                     control->hovered, focused,
                                     state->palette.highContrast,
                                     state->palette.accent,
                                     state->palette.mutedText);
        return;
    }

    COLORREF fill = control->primary ? state->palette.accent
                                     : state->palette.field;
    COLORREF border = control->primary ? state->palette.accent
                                       : state->palette.border;
    COLORREF text = control->primary ?
        (state->palette.highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
                                       RGB(0xFF, 0xFF, 0xFF)) :
        state->palette.text;
    if (control->hovered) {
        fill = control->primary ? state->palette.accentHover
                                : state->palette.surface;
    }
    /* Secondary actions keep their current hover/rest appearance on press. */
    if (pressed && control->primary) fill = state->palette.accentHover;
    if (disabled) {
        fill = state->palette.field;
        text = state->palette.mutedText;
        border = state->palette.border;
    }

    DialogModern_DrawRoundedRect(item->hDC, &rect,
                                 DialogModern_Scale(state->dpi, 18),
                                 fill, border,
                                 state->palette.highContrast ? 1 : 0);
    wchar_t textBuffer[256] = {0};
    GetWindowTextW(control->hwnd, textBuffer, (int)_countof(textBuffer));
    if (focused) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -DialogModern_Scale(state->dpi, 4),
                    -DialogModern_Scale(state->dpi, 4));
        DialogModern_DrawRoundedRect(item->hDC, &focusRect,
                                     DialogModern_Scale(state->dpi, 14),
                                     fill, state->palette.accent, 1);
    }
    DialogModern_DrawText(item->hDC, state->buttonFont, text, &rect, textBuffer,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);
}

static void ModernDrawComboItemContent(ModernControl* control, HDC hdc,
                                       const RECT* itemRect, UINT itemId,
                                       UINT itemState) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !hdc || !itemRect || itemId == (UINT)-1) return;

    RECT rect = *itemRect;
    COLORREF popupSurface = state->palette.darkMode
        ? ModernBlendColor(state->palette.surface, state->palette.field, 58)
        : state->palette.surface;
    HBRUSH popupBrush = CreateSolidBrush(popupSurface);
    FillRect(hdc, &rect, popupBrush);
    DeleteObject(popupBrush);

    BOOL selected = (itemState & ODS_SELECTED) != 0 ||
                    control->comboHotItem == (int)itemId;
    BOOL chosen = (int)itemId ==
        (int)SendMessageW(control->hwnd, CB_GETCURSEL, 0, 0);
    BOOL disabled = (itemState & ODS_DISABLED) != 0;
    RECT selection = rect;
    int horizontalInset = DialogModern_Scale(state->dpi, 6);
    InflateRect(&selection, -horizontalInset,
                -DialogModern_Scale(state->dpi, 3));
    if (selected || chosen) {
        COLORREF selectionFill = selected
            ? ModernBlendColor(state->palette.accent, popupSurface,
                               state->palette.darkMode ? 69 : 84)
            : ModernBlendColor(state->palette.accent, popupSurface,
                               state->palette.darkMode ? 83 : 92);
        COLORREF selectionBorder = selected
            ? ModernBlendColor(state->palette.accent, popupSurface, 32)
