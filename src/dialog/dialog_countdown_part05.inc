    int cardRadius = CountdownScaleValue(state, 22);

    HBRUSH backgroundBrush = CreateSolidBrush(state->backgroundColor);
    FillRect(target, &client, backgroundBrush);
    DeleteObject(backgroundBrush);

    RECT card = {CountdownScaleValue(state, 8), CountdownScaleValue(state, 8),
                 width - CountdownScaleValue(state, 8),
                 height - CountdownScaleValue(state, 8)};
    COLORREF shadowColor = state->darkMode ? RGB(0x0D, 0x0E, 0x11)
                                           : RGB(0xD6, 0xDB, 0xE5);
    RECT shadow = card;
    OffsetRect(&shadow, CountdownScaleValue(state, 1),
               CountdownScaleValue(state, 3));
    CountdownDrawRounded(target, &shadow, cardRadius, shadowColor,
                         shadowColor, 0);
    CountdownDrawRounded(target, &card, cardRadius, state->cardColor,
                         state->borderColor, state->highContrast ? 1 : 0);

    int titleTop96 = state->compactLayout ? 16 : 22;
    int titleBottom96 = state->ultraCompactLayout ? 50 :
                       (state->compactLayout ? 55 : 66);
    RECT titleRect = {margin, CountdownScaleValue(state, titleTop96),
                      width - margin - CountdownScaleValue(state, 64),
                      CountdownScaleValue(state, titleBottom96)};
    SIZE titleSize = {0};
    HGDIOBJ oldTitleFont = state->titleFont
        ? SelectObject(target, state->titleFont) : NULL;
    GetTextExtentPoint32W(target, state->title, (int)wcslen(state->title),
                          &titleSize);
    if (oldTitleFont) SelectObject(target, oldTitleFont);
    state->titleFrame.left = titleRect.left;
    state->titleFrame.top = titleRect.top +
        ((titleRect.bottom - titleRect.top) - titleSize.cy) / 2;
    state->titleFrame.right = state->titleFrame.left + titleSize.cx;
    if (state->titleFrame.right > titleRect.right) {
        state->titleFrame.right = titleRect.right;
    }
    state->titleFrame.bottom = state->titleFrame.top + titleSize.cy;
    int signatureBottom96 = state->ultraCompactLayout ? 52 :
                            (state->compactLayout ? 57 : 68);
    RECT signatureRect = titleRect;
    signatureRect.bottom = CountdownScaleValue(state, signatureBottom96);
    DialogModern_DrawTitleSignature(
        target, &signatureRect, state->dpi, titleSize.cx,
        state->accentColor, state->cardColor, state->darkMode,
        state->highContrast);
    COLORREF titleColor = state->highContrast
        ? state->textColor
        : (state->titleHovered ? COUNTDOWN_TITLE_HOVER_COLOR
                               : state->accentColor);
    CountdownDrawText(target, state->titleFont, titleColor, &titleRect,
                      state->title, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    int panelTop96 = state->ultraCompactLayout ? 68 :
                     (state->compactLayout ? 76 : 92);
    int panelGap96 = state->ultraCompactLayout ? 26 :
                     (state->compactLayout ? 26 : 30);
    int panelMinimumHeight96 = state->ultraCompactLayout ? 42 :
                               (state->compactLayout ? 72 : 52);
    RECT formatPanel = {
        margin, CountdownScaleValue(state, panelTop96), width - margin,
        state->editFrame.top - CountdownScaleValue(state, panelGap96)
    };
    if (formatPanel.bottom <
        formatPanel.top + CountdownScaleValue(state, panelMinimumHeight96)) {
        formatPanel.bottom = formatPanel.top +
                             CountdownScaleValue(state, panelMinimumHeight96);
    }
    CountdownDrawRounded(target, &formatPanel, radius, state->fieldColor,
                         state->highContrast ? state->borderColor : state->fieldColor,
                         state->highContrast ? 1 : 0);

    int panelInset96 = state->compactLayout ? 12 : 16;
    int labelTop96 = state->compactLayout ? 6 : 10;
    int labelBottom96 = state->compactLayout ? 22 : 28;
    RECT formatLabelRect = {
        formatPanel.left + CountdownScaleValue(state, panelInset96),
        formatPanel.top + CountdownScaleValue(state, labelTop96),
                            formatPanel.right - CountdownScaleValue(state, 16),
        formatPanel.top + CountdownScaleValue(state, labelBottom96)
    };
    CountdownDrawText(target, state->smallFont, state->mutedColor,
                      &formatLabelRect, state->formatLabel,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    int examplesTop96 = state->compactLayout ? 23 : 31;
    int examplesBottomInset96 = state->compactLayout ? 5 : 9;
    RECT examplesRect = {
        formatPanel.left + CountdownScaleValue(state, panelInset96),
        formatPanel.top + CountdownScaleValue(state, examplesTop96),
        formatPanel.right - CountdownScaleValue(state, panelInset96),
        formatPanel.bottom - CountdownScaleValue(state, examplesBottomInset96)
    };
    CountdownDrawExamplesGrid(target, state, &examplesRect);

    int fieldLabelOffset96 = state->compactLayout ? 20 : 25;
    RECT fieldLabelRect = {state->editFrame.left,
                           state->editFrame.top -
                               CountdownScaleValue(state, fieldLabelOffset96),
                           state->editFrame.right,
                           state->editFrame.top - CountdownScaleValue(state, 4)};
    CountdownDrawText(target, state->smallFont, state->mutedColor,
                      &fieldLabelRect, state->fieldLabel,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    BOOL editFocused = GetFocus() == state->hwndEdit;
    COLORREF fieldBorder = state->borderColor;
    if (state->showValidationError) {
        fieldBorder = state->dangerColor;
    } else if (editFocused) {
        fieldBorder = state->accentColor;
    }
    CountdownDrawRounded(target, &state->editFrame, radius, state->fieldColor,
                         fieldBorder, editFocused || state->showValidationError ?
                         CountdownScaleValue(state, 2) : 1);
    CountdownDrawClockIcon(target,
                           state->editFrame.left + CountdownScaleValue(state, 25),
                           (state->editFrame.top + state->editFrame.bottom) / 2,
                           CountdownScaleValue(state, 9),
                           editFocused ? state->accentColor : state->mutedColor);

    int previewTop = state->editFrame.bottom + smallGap;
    int previewHeight96 = state->ultraCompactLayout ? 30 :
                          (state->compactLayout ? 38 : 46);
    RECT previewRect = {state->editFrame.left + CountdownScaleValue(state, 18),
                        previewTop,
                        state->editFrame.right - CountdownScaleValue(state, 8),
                        previewTop + CountdownScaleValue(state, previewHeight96)};
    if (state->showValidationError) {
        CountdownDrawWarningIcon(target,
                                 previewRect.left - CountdownScaleValue(state, 9),
                                 previewTop + CountdownScaleValue(
                                     state, previewHeight96 / 2),
                                 CountdownScaleValue(state, 7),
                                 state->dangerColor);
        CountdownDrawWrappedText(target, state->smallFont, state->dangerColor,
                                 &previewRect, state->invalidText);
    } else if (state->inputValid) {
        CountdownDrawCheckIcon(target,
                               previewRect.left - CountdownScaleValue(state, 9),
                               previewTop + CountdownScaleValue(
                                   state, previewHeight96 / 2),
                               CountdownScaleValue(state, 7),
                               state->accentColor);
        CountdownDrawWrappedText(target, state->smallFont, state->accentColor,
                                 &previewRect, state->previewText);
    }

}

static void CountdownDrawButton(const DRAWITEMSTRUCT* drawItem,
                                CountdownDialogState* state) {
    if (!drawItem || !state || !drawItem->hDC) {
        return;
    }

    CountdownHoverPart part = CountdownPartForButton(state, drawItem->hwndItem);
    BOOL primary = part == COUNTDOWN_HOVER_START;
    BOOL close = part == COUNTDOWN_HOVER_CLOSE;
    BOOL hovered = state->hoverPart == part;
    BOOL pressed = state->pressedPart == part ||
                   (drawItem->itemState & ODS_SELECTED) != 0;
    BOOL focused = (drawItem->itemState & ODS_FOCUS) != 0;
    RECT rect = drawItem->rcItem;
    HBRUSH backgroundBrush = CreateSolidBrush(state->cardColor);
    FillRect(drawItem->hDC, &rect, backgroundBrush);
    DeleteObject(backgroundBrush);

    if (close) {
        DialogModern_DrawCloseButton(drawItem->hDC, &rect, state->dpi,
                                     hovered, focused, state->highContrast,
                                     state->accentColor, state->mutedColor);
        return;
    }

    COLORREF fill = primary ? state->accentColor : state->fieldColor;
    COLORREF text = primary ?
        (state->highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
                               RGB(0xFF, 0xFF, 0xFF)) :
        state->textColor;
    COLORREF border = primary ? state->accentColor : state->borderColor;
    if (hovered) {
        fill = primary ? state->accentHoverColor : state->cardColor;
    }
    /* Keep the secondary Cancel action visually still while pressed. */
    if (pressed && primary) fill = state->accentHoverColor;

    CountdownDrawRounded(drawItem->hDC, &rect,
                         CountdownScaleValue(state, 11), fill, border,
                         state->highContrast ? 1 : 0);

    wchar_t buttonText[128] = {0};
    GetWindowTextW(drawItem->hwndItem, buttonText, (int)_countof(buttonText));
    if (focused) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -CountdownScaleValue(state, 4),
                    -CountdownScaleValue(state, 4));
        CountdownDrawRounded(drawItem->hDC, &focusRect,
                             CountdownScaleValue(state, 8),
                             fill, state->accentColor,
                             CountdownScaleValue(state, 1));
    }
    CountdownDrawText(drawItem->hDC, state->buttonFont, text, &rect, buttonText,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void CountdownMoveFocus(CountdownDialogState* state, HWND current,
                               BOOL reverse) {
    if (!state) {
        return;
    }

    HWND controls[4] = {
        state->hwndEdit,
        state->hwndStart,
        state->hwndCancel,
        state->hwndClose
    };
    int currentIndex = 0;
    for (int i = 0; i < (int)_countof(controls); i++) {
        if (controls[i] == current) {
            currentIndex = i;
            break;
        }
    }

    int step = reverse ? -1 : 1;
    int nextIndex = currentIndex;
    for (int i = 0; i < (int)_countof(controls); i++) {
        nextIndex = (nextIndex + step + (int)_countof(controls)) %
                    (int)_countof(controls);
        if (controls[nextIndex] && IsWindowVisible(controls[nextIndex]) &&
            IsWindowEnabled(controls[nextIndex])) {
            SetFocus(controls[nextIndex]);
            return;
        }
    }
}

static void CountdownClearChildFocus(HWND hwnd) {
    HWND focused = GetFocus();
    if (focused && focused != hwnd && IsChild(hwnd, focused)) {
        SetFocus(hwnd);
    }
}

static LRESULT CALLBACK CountdownEditSubclassProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
                                                  DWORD_PTR refData) {
    (void)subclassId;
    CountdownDialogState* state = (CountdownDialogState*)refData;
    HWND parent = GetParent(hwnd);

    switch (msg) {
        case WM_SETFOCUS:
            if (state) {
                state->hoverPart = COUNTDOWN_HOVER_EDIT;
                InvalidateRect(parent, NULL, FALSE);
                if (state->selectAllOnNextFocus) {
                    state->selectAllOnNextFocus = FALSE;
                    PostMessageW(hwnd, EM_SETSEL, 0, -1);
                }
            }
            break;

        case WM_KILLFOCUS:
            if (state && state->hoverPart == COUNTDOWN_HOVER_EDIT) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(parent, NULL, FALSE);
            }
            break;

        case WM_MOUSEMOVE:
            if (state && state->hoverPart != COUNTDOWN_HOVER_EDIT) {
                state->hoverPart = COUNTDOWN_HOVER_EDIT;
                CountdownTrackMouse(hwnd);
                InvalidateRect(parent, NULL, FALSE);
            }
