/**
 * @file dialog_countdown_layout.c
 * @brief Geometry, hover tracking, and control layout.
 */

#include "dialog_countdown_internal.h"

int CountdownMeasureTextPixels(HDC hdc, HFONT font,
                                      const wchar_t* text) {
    if (!hdc || !text || !text[0]) {
        return 0;
    }

    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    SIZE size = {0};
    BOOL measured = GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);
    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
    return measured && size.cx > 0 ? size.cx : 0;
}

BOOL CountdownUpdateTitleHover(HWND hwnd,
                                      CountdownDialogState* state,
                                      POINT point) {
    if (!hwnd || !state) return FALSE;
    BOOL hovered = PtInRect(&state->titleFrame, point);
    if (hovered == state->titleHovered) return FALSE;
    state->titleHovered = hovered;
    InvalidateRect(hwnd, &state->titleFrame, FALSE);
    return TRUE;
}

void CountdownRefreshTitleHoverFromCursor(
    HWND hwnd, CountdownDialogState* state) {
    POINT point = {0};
    if (!hwnd || !state || !GetCursorPos(&point)) return;
    ScreenToClient(hwnd, &point);
    CountdownUpdateTitleHover(hwnd, state, point);
}

int CountdownMeasureButtonWidth96(HWND hwnd, HFONT font,
                                         const wchar_t* text, int minimum96) {
    if (!hwnd || !text) {
        return minimum96;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        return minimum96;
    }
    int measuredPixels = CountdownMeasureTextPixels(hdc, font, text);
    ReleaseDC(hwnd, hdc);
    /* Button padding is deliberately expressed in logical pixels. */
    UINT dpi = CountdownGetDpi(hwnd);
    int desired96 = MulDiv(measuredPixels, 96, (int)(dpi ? dpi : 96u)) + 32;
    if (desired96 < minimum96) {
        desired96 = minimum96;
    }
    return desired96;
}

CountdownHoverPart CountdownPartForButton(const CountdownDialogState* state,
                                                 HWND hwndButton) {
    if (!state || !hwndButton) {
        return COUNTDOWN_HOVER_NONE;
    }
    if (hwndButton == state->hwndClose) return COUNTDOWN_HOVER_CLOSE;
    if (hwndButton == state->hwndCancel) return COUNTDOWN_HOVER_CANCEL;
    if (hwndButton == state->hwndStart) return COUNTDOWN_HOVER_START;
    return COUNTDOWN_HOVER_NONE;
}

void CountdownTrackMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

void CountdownTrackNonClientMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE | TME_NONCLIENT;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

void CountdownApplyShape(HWND hwnd, const CountdownDialogState* state) {
    if (!hwnd || !state) {
        return;
    }
    DialogModern_ApplyWindowShape(hwnd, state->dpi, 20);
}

void CountdownCenterEditText(const CountdownDialogState* state) {
    if (!state || !state->hwndEdit) return;

    RECT client = {0};
    if (!GetClientRect(state->hwndEdit, &client)) return;
    HDC hdc = GetDC(state->hwndEdit);
    if (!hdc) return;

    HFONT font = (HFONT)SendMessageW(state->hwndEdit, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    TEXTMETRICW metrics = {0};
    if (GetTextMetricsW(hdc, &metrics)) {
        int height = client.bottom - client.top;
        int paddingY = (height - metrics.tmHeight) / 2;
        if (paddingY < 0) paddingY = 0;
        int paddingX = CountdownScaleValue(state, 10);
        RECT formatRect = {
            paddingX,
            paddingY,
            max(paddingX + 1, client.right - paddingX),
            max(paddingY + 1, height - paddingY)
        };
        SendMessageW(state->hwndEdit, EM_SETRECTNP, 0,
                     (LPARAM)&formatRect);
    }
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(state->hwndEdit, hdc);
}

void CountdownLayout(HWND hwnd, CountdownDialogState* state) {
    if (!hwnd || !state) {
        return;
    }

    CountdownUpdateResponsiveMode(hwnd, state);
    RECT client = {0};
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    int margin96 = state->compactLayout ? 24 : 32;
    int closeSize96 = state->compactLayout ? 28 : 32;
    int closeTop96 = state->compactLayout ? 14 : 20;
    int footerHeight96 = state->ultraCompactLayout ? 36 : 38;
    int footerBottom96 = state->compactLayout ? 12 : 20;
    int fieldHeight96 = state->ultraCompactLayout ? 50 :
                        (state->compactLayout ? 54 : 58);
    int fieldGapToFooter96 = state->ultraCompactLayout ? 84 :
                             (state->compactLayout ? 96 : 104);
    int margin = CountdownScaleValue(state, margin96);
    int closeSize = CountdownScaleValue(state, closeSize96);
    int closeTop = CountdownScaleValue(state, closeTop96);
    int footerHeight = CountdownScaleValue(state, footerHeight96);
    int footerBottom = CountdownScaleValue(state, footerBottom96);
    int fieldHeight = CountdownScaleValue(state, fieldHeight96);
    int fieldY = height - footerHeight - footerBottom -
                 CountdownScaleValue(state, fieldGapToFooter96);

    if (width < margin * 2 + CountdownScaleValue(state, 180)) {
        margin = CountdownScaleValue(state, 20);
    }
    int minimumFieldY96 = state->ultraCompactLayout ? 132 :
                          (state->compactLayout ? 165 : 150);
    if (fieldY < CountdownScaleValue(state, minimumFieldY96)) {
        fieldY = CountdownScaleValue(state, minimumFieldY96);
    }

    state->closeFrame.left = width - margin - closeSize;
    state->closeFrame.top = closeTop;
    state->closeFrame.right = state->closeFrame.left + closeSize;
    state->closeFrame.bottom = closeTop + closeSize;

    state->editFrame.left = margin;
    state->editFrame.top = fieldY;
    state->editFrame.right = width - margin;
    state->editFrame.bottom = fieldY + fieldHeight;

    int editLeft = state->editFrame.left + CountdownScaleValue(state, 48);
    int editTop = state->editFrame.top + CountdownScaleValue(state, 7);
    int editWidth = state->editFrame.right - editLeft - CountdownScaleValue(state, 14);
    int editHeight = fieldHeight - CountdownScaleValue(state, 14);
    if (editWidth < CountdownScaleValue(state, 40)) {
        editWidth = CountdownScaleValue(state, 40);
    }
    if (editHeight < CountdownScaleValue(state, 24)) {
        editHeight = CountdownScaleValue(state, 24);
    }

    if (state->hwndEdit) {
        HDC editDc = GetDC(state->hwndEdit);
        if (editDc) {
            HFONT editFont = (HFONT)SendMessageW(
                state->hwndEdit, WM_GETFONT, 0, 0);
            HGDIOBJ oldFont = editFont ? SelectObject(editDc, editFont) : NULL;
            TEXTMETRICW metrics = {0};
            if (GetTextMetricsW(editDc, &metrics)) {
                int measuredHeight = metrics.tmHeight +
                                     CountdownScaleValue(state, 8);
                if (measuredHeight >= CountdownScaleValue(state, 24) &&
                    measuredHeight < editHeight) {
                    editHeight = measuredHeight;
                    editTop = state->editFrame.top +
                              (fieldHeight - editHeight) / 2;
                }
            }
            if (oldFont) SelectObject(editDc, oldFont);
            ReleaseDC(state->hwndEdit, editDc);
        }
    }

    if (state->hwndEdit) {
        SetWindowPos(state->hwndEdit, NULL, editLeft, editTop,
                     editWidth, editHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        CountdownCenterEditText(state);
    }
    if (state->hwndClose) {
        SetWindowPos(state->hwndClose, NULL,
                     state->closeFrame.left, state->closeFrame.top,
                     closeSize, closeSize,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    int buttonGap = CountdownScaleValue(state, 10);
    int startWidth = CountdownScaleValue(state, 106);
    int cancelWidth = CountdownScaleValue(state, 88);
    if (state->buttonFont) {
        startWidth = CountdownScaleValue(
            state, CountdownMeasureButtonWidth96(hwnd, state->buttonFont,
                                                 state->startText, 106));
        cancelWidth = CountdownScaleValue(
            state, CountdownMeasureButtonWidth96(hwnd, state->buttonFont,
                                                 state->cancelText, 88));
    }
    int buttonY = height - footerBottom - footerHeight;
    int startX = width - margin - startWidth;
    int cancelX = startX - buttonGap - cancelWidth;

    if (cancelX < margin) {
        cancelX = margin;
        startX = cancelX + cancelWidth + buttonGap;
    }

    if (state->hwndCancel) {
        SetWindowPos(state->hwndCancel, NULL, cancelX, buttonY,
                     cancelWidth, footerHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (state->hwndStart) {
        SetWindowPos(state->hwndStart, NULL, startX, buttonY,
                     startWidth, footerHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    CountdownApplyShape(hwnd, state);
    InvalidateRect(hwnd, NULL, FALSE);
}
