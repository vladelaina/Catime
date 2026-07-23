            : selectionFill;
        DialogModern_DrawRoundedRect(hdc, &selection,
                                     DialogModern_Scale(state->dpi, 12),
                                     selectionFill, selectionBorder,
                                     selected ? 1 : 0);
    }

    wchar_t text[512] = {0};
    SendMessageW(control->hwnd, CB_GETLBTEXT, itemId, (LPARAM)text);
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
        int centerY = (rect.top + rect.bottom) / 2;
        int arm = max(2, DialogModern_Scale(state->dpi, 3));
        LOGBRUSH penBrush = {BS_SOLID, state->palette.accent, 0};
        HPEN checkPen = ExtCreatePen(
            PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
            (DWORD)max(1, DialogModern_Scale(state->dpi, 2)),
            &penBrush, 0, NULL);
        HGDIOBJ oldPen = checkPen ? SelectObject(hdc, checkPen) : NULL;
        if (checkPen) {
            MoveToEx(hdc, centerX - arm, centerY, NULL);
            LineTo(hdc, centerX - arm / 3, centerY + arm);
            LineTo(hdc, centerX + arm, centerY - arm);
        }
        if (oldPen) SelectObject(hdc, oldPen);
        if (checkPen) DeleteObject(checkPen);
    }
}

static void ModernDrawComboItem(ModernDialogState* state,
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

static void ModernDrawDialog(ModernDialogState* state, HDC hdc) {
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

static void ModernPaintBuffered(ModernDialogState* state, HDC target) {
    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    if (buffer && bitmap) {
        ModernDrawDialog(state, buffer);
        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
    } else {
        ModernDrawDialog(state, target);
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
    }
}

static void ModernDrawFieldOutlineToDc(ModernControl* control, HDC hdc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd || !hdc) return;
    RECT rect = {0};
    GetClientRect(control->hwnd, &rect);
    InflateRect(&rect, -1, -1);
    COLORREF border = control->focused
        ? state->palette.accent
        : (control->hovered
               ? ModernBlendColor(state->palette.border,
                                  state->palette.accent, 38)
               : state->palette.border);
    HPEN pen = CreatePen(PS_SOLID,
                         control->focused ? DialogModern_Scale(state->dpi, 2) : 1,
                         border);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom,
              DialogModern_Scale(state->dpi, 18),
              DialogModern_Scale(state->dpi, 18));
    SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static void ModernDrawFieldOutline(ModernControl* control) {
    if (!control || !control->hwnd) return;
    HDC hdc = GetDC(control->hwnd);
    if (!hdc) return;
    ModernDrawFieldOutlineToDc(control, hdc);
    ReleaseDC(control->hwnd, hdc);
}

static void ModernPaintChoiceControl(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state) return;
    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->surfaceBrush);

    wchar_t text[512] = {0};
    GetWindowTextW(control->hwnd, text, (int)_countof(text));
    BOOL enabled = IsWindowEnabled(control->hwnd);
    COLORREF textColor = enabled ? state->palette.text : state->palette.mutedText;

    if (control->kind == MODERN_CONTROL_GROUP) {
        int labelHeight = DialogModern_Scale(state->dpi, 18);
        RECT borderRect = client;
        borderRect.top += labelHeight / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, state->palette.border);
        HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(hdc, borderRect.left, borderRect.top,
                  borderRect.right, borderRect.bottom,
                  DialogModern_Scale(state->dpi, 16),
                  DialogModern_Scale(state->dpi, 16));
        SelectObject(hdc, oldBrush);
        if (oldPen) SelectObject(hdc, oldPen);
        if (pen) DeleteObject(pen);
        RECT label = {DialogModern_Scale(state->dpi, 12), 0,
                      client.right - DialogModern_Scale(state->dpi, 12),
                      labelHeight};
        if (text[0]) {
            SIZE textSize = {0};
            ModernMeasureText(control->hwnd, state->labelFont, &textSize);
            RECT labelBackdrop = {
                label.left - DialogModern_Scale(state->dpi, 4),
                0,
                label.left + textSize.cx + DialogModern_Scale(state->dpi, 4),
                labelHeight
            };
            if (labelBackdrop.right > label.right) {
                labelBackdrop.right = label.right;
            }
            FillRect(hdc, &labelBackdrop, state->surfaceBrush);
        }
        DialogModern_DrawText(hdc, state->labelFont, state->palette.mutedText,
                              &label, text,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);
    } else {
        int glyphSize = DialogModern_Scale(state->dpi, 16);
        int glyphY = (client.bottom - glyphSize) / 2;
        RECT glyph = {1, glyphY, 1 + glyphSize, glyphY + glyphSize};
        LRESULT checked = SendMessageW(control->hwnd, BM_GETCHECK, 0, 0);
        BOOL selected = checked == BST_CHECKED || checked == BST_INDETERMINATE;
        COLORREF selectionMark = state->palette.highContrast
            ? GetSysColor(COLOR_HIGHLIGHTTEXT)
            : RGB(0xFF, 0xFF, 0xFF);
        if (control->kind == MODERN_CONTROL_RADIO) {
            HBRUSH brush = CreateSolidBrush(selected ? state->palette.accent :
                                                       state->palette.surface);
            HPEN pen = CreatePen(PS_SOLID, 1,
                                 selected ? state->palette.accent :
                                            state->palette.border);
            HGDIOBJ oldBrush = brush ? SelectObject(hdc, brush) : NULL;
            HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
            Ellipse(hdc, glyph.left, glyph.top, glyph.right, glyph.bottom);
            if (selected) {
                RECT dot = glyph;
                InflateRect(&dot, -DialogModern_Scale(state->dpi, 5),
