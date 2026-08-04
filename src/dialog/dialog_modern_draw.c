/**
 * @file dialog_modern_draw.c
 * @brief Finalization and dialog/button/combo drawing.
 */

#include "dialog_modern_internal.h"

BOOL ModernFinalize(ModernDialogState* state) {
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

ModernControl* ModernFindControl(ModernDialogState* state, HWND hwnd) {
    if (!state || !hwnd) return NULL;
    for (size_t i = 0; i < state->controlCount; i++) {
        if (state->controls[i].hwnd == hwnd) return &state->controls[i];
    }
    return NULL;
}

void ModernDrawButton(ModernDialogState* state,
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

void ModernDrawComboItemContent(ModernControl* control, HDC hdc,
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
            : selectionFill;
        DialogModern_DrawRoundedRect(hdc, &selection,
                                     DialogModern_Scale(state->dpi, 12),
                                     selectionFill, selectionBorder,
                                     selected ? 1 : 0);
    }

    wchar_t text[512] = {0};
    SendMessageW(control->hwnd, CB_GETLBTEXT, itemId, (LPARAM)text);
    text[_countof(text) - 1] = L'\0';
    RECT textRect = rect;
    textRect.left += DialogModern_Scale(state->dpi, 16);
    textRect.right -= DialogModern_Scale(state->dpi, chosen ? 34 : 24);
    COLORREF textColor = disabled ? state->palette.mutedText :
        (selected || chosen ? state->palette.accent : state->palette.text);
    DialogModern_DrawText(hdc, state->editFont, textColor,
                          &textRect, text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);

    if (chosen) {
        int centerX = rect.right - DialogModern_Scale(state->dpi, 18);
        int arm = max(2, DialogModern_Scale(state->dpi, 3));
        LOGBRUSH penBrush = {BS_SOLID, state->palette.accent, 0};
        HPEN checkPen = ExtCreatePen(
            PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
            (DWORD)max(1, DialogModern_Scale(state->dpi, 2)),
            &penBrush, 0, NULL);
        HGDIOBJ oldPen = checkPen ? SelectObject(hdc, checkPen) : NULL;
        if (checkPen) {
            int centerY = (rect.top + rect.bottom) / 2;
            MoveToEx(hdc, centerX - arm, centerY, NULL);
            LineTo(hdc, centerX - arm / 3, centerY + arm);
            LineTo(hdc, centerX + arm, centerY - arm);
        }
        if (oldPen) SelectObject(hdc, oldPen);
        if (checkPen) DeleteObject(checkPen);
    }
}

void ModernDrawComboItem(ModernDialogState* state,
                                const DRAWITEMSTRUCT* item) {
    ModernControl* control = ModernFindControl(state, item->hwndItem);
    if (!control && item->CtlID != 0) {
        HWND combo = GetDlgItem(state->hwnd, (int)item->CtlID);
        control = ModernFindControl(state, combo);
    }
    if (!control || control->kind != MODERN_CONTROL_COMBO ||
        ModernIsDateTimeControl(control)) {
        return;
    }

    if (item->itemID == (UINT)-1) {
        COLORREF popupSurface = state->palette.darkMode
            ? ModernBlendColor(state->palette.surface,
                               state->palette.field, 58)
            : state->palette.surface;
        HBRUSH popupBrush = CreateSolidBrush(popupSurface);
        FillRect(item->hDC, &item->rcItem, popupBrush);
        DeleteObject(popupBrush);
        return;
    }
    ModernDrawComboItemContent(control, item->hDC, &item->rcItem,
                               item->itemID, item->itemState);
}

void ModernDrawDialog(ModernDialogState* state, HDC hdc) {
    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    FillRect(hdc, &client, state->backgroundBrush);

    RECT surface = {DialogModern_Scale(state->dpi, 8),
                    DialogModern_Scale(state->dpi, 8),
                    client.right - DialogModern_Scale(state->dpi, 8),
                    client.bottom - DialogModern_Scale(state->dpi, 8)};
    COLORREF shadow = state->palette.darkMode ? RGB(0x0D, 0x0E, 0x11)
                                              : RGB(0xD6, 0xDB, 0xE5);
    RECT shadowRect = surface;
    OffsetRect(&shadowRect, DialogModern_Scale(state->dpi, 1),
               DialogModern_Scale(state->dpi, 3));
    DialogModern_DrawRoundedRect(hdc, &shadowRect,
                                 DialogModern_Scale(state->dpi, 42),
                                 shadow, shadow, 0);
    DialogModern_DrawRoundedRect(hdc, &surface,
                                 DialogModern_Scale(state->dpi, 42),
                                 state->palette.surface,
                                 state->palette.highContrast ?
                                 state->palette.border : state->palette.surface,
                                 state->palette.highContrast ? 1 : 0);

    int side = DialogModern_Scale(state->dpi, state->sidePadding96);
    COLORREF accentColor = state->palette.accent;
    if (state->dialogType == DIALOG_INSTANCE_ERROR ||
        state->dialogType == DIALOG_INSTANCE_MESSAGE_ERROR ||
        state->dialogType == DIALOG_INSTANCE_UPDATE_ERROR) {
        accentColor = state->palette.danger;
    } else if (state->dialogType == DIALOG_INSTANCE_MESSAGE_WARNING) {
        accentColor = state->palette.warning;
    }
    wchar_t title[256] = {0};
    GetWindowTextW(state->hwnd, title, (int)_countof(title));
    RECT titleRect = {surface.left + side,
                      surface.top + DialogModern_Scale(state->dpi, 12),
                      client.right - side - DialogModern_Scale(state->dpi, 52),
                      DialogModern_Scale(state->dpi,
                                         state->headerHeight96 - 18)};
    SIZE titleSize = {0};
    HGDIOBJ oldTitleFont = state->titleFont
        ? SelectObject(hdc, state->titleFont) : NULL;
    GetTextExtentPoint32W(hdc, title, (int)wcslen(title), &titleSize);
    if (oldTitleFont) SelectObject(hdc, oldTitleFont);
    state->titleFrame.left = titleRect.left;
    state->titleFrame.top = titleRect.top +
        ((titleRect.bottom - titleRect.top) - titleSize.cy) / 2;
    state->titleFrame.right = state->titleFrame.left + titleSize.cx;
    if (state->titleFrame.right > titleRect.right) {
        state->titleFrame.right = titleRect.right;
    }
    state->titleFrame.bottom = state->titleFrame.top + titleSize.cy;
    RECT signatureRect = titleRect;
    signatureRect.bottom = DialogModern_Scale(
        state->dpi, state->headerHeight96 - 17);
    DialogModern_DrawTitleSignature(
        hdc, &signatureRect, state->dpi, titleSize.cx, accentColor,
        state->palette.surface, state->palette.darkMode,
        state->palette.highContrast);
    COLORREF titleColor = state->palette.highContrast
        ? state->palette.text
        : (state->titleHovered ? MODERN_TITLE_HOVER_COLOR
                               : state->palette.accent);
    DialogModern_DrawText(hdc, state->titleFont, titleColor,
                          &titleRect, title,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);
    ModernDrawBodyScrollbar(state, hdc);
}
