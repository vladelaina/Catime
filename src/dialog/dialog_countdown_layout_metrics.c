/**
 * @file dialog_countdown_layout_metrics.c
 * @brief Responsive metrics and drawing primitives.
 */

#include "dialog_countdown_internal.h"

void CountdownDrawRounded(HDC hdc, const RECT* rect, int radius,
                                 COLORREF fill, COLORREF border,
                                 int borderWidth) {
    DialogModern_DrawRoundedRect(hdc, rect, radius, fill, border, borderWidth);
}

void CountdownDrawText(HDC hdc, HFONT font, COLORREF color,
                              const RECT* rect, const wchar_t* text,
                              UINT format) {
    DialogModern_DrawText(hdc, font, color, rect, text, format);
}

void CountdownDrawWrappedText(HDC hdc, HFONT font, COLORREF color,
                                     const RECT* rect,
                                     const wchar_t* text) {
    if (!hdc || !rect || !text) return;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    int oldMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, color);

    RECT measure = {0, 0, rect->right - rect->left,
                    rect->bottom - rect->top};
    DrawTextW(hdc, text, -1, &measure,
              DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    int availableHeight = rect->bottom - rect->top;
    int measuredHeight = measure.bottom - measure.top;
    RECT drawRect = *rect;
    if (measuredHeight > 0 && measuredHeight < availableHeight) {
        drawRect.top += (availableHeight - measuredHeight) / 2;
        drawRect.bottom = drawRect.top + measuredHeight;
    }
    DrawTextW(hdc, text, -1, &drawRect,
              DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldMode);
    if (oldFont) SelectObject(hdc, oldFont);
}

int CountdownSplitExampleLines(const wchar_t* text,
                                      wchar_t lines[][256], int maxLines) {
    if (!text || !lines || maxLines <= 0) {
        return 0;
    }

    int count = 0;
    const wchar_t* cursor = text;
    while (*cursor && count < maxLines) {
        const wchar_t* end = wcschr(cursor, L'\n');
        size_t length = end ? (size_t)(end - cursor) : wcslen(cursor);
        if (length >= 256) length = 255;
        wmemcpy(lines[count], cursor, length);
        lines[count][length] = L'\0';
        count++;
        if (!end) break;
        cursor = end + 1;
    }
    return count;
}

void CountdownUpdateResponsiveMode(HWND hwnd,
                                          CountdownDialogState* state) {
    if (!hwnd || !state) {
        return;
    }

    RECT client = {0};
    GetClientRect(hwnd, &client);
    int height = client.bottom - client.top;
    UINT dpi = state->dpi ? state->dpi : CountdownGetDpi(hwnd);
    int logicalHeight = MulDiv(height, 96, (int)(dpi ? dpi : 96u));
    state->ultraCompactLayout = logicalHeight < 305;
    state->compactLayout = logicalHeight < 360;
}

void CountdownUpdateTextMetrics(HWND hwnd,
                                       CountdownDialogState* state) {
    if (!hwnd || !state) {
        return;
    }

    CountdownUpdateResponsiveMode(hwnd, state);
    state->exampleColumns = 1;
    state->exampleColumnWidths[0] = 0;
    state->exampleColumnWidths[1] = 0;
    state->exampleTokenWidths[0] = 0;
    state->exampleTokenWidths[1] = 0;

    wchar_t lines[12][256] = {{0}};
    int fullCount = CountdownSplitExampleLines(state->examples, lines,
                                               (int)_countof(lines));
    if (fullCount <= 0) {
        state->exampleVisibleCount = 0;
        state->desiredWidth96 = COUNTDOWN_BASE_WIDTH;
        return;
    }

    int count = fullCount;
    if (state->ultraCompactLayout) {
        count = min(count, 1);
    } else if (state->compactLayout) {
        count = min(count, 4);
    }
    state->exampleVisibleCount = count;
    state->exampleColumns = count > 4 ? 2 :
                            (state->compactLayout && count > 1 ? 2 : 1);
    int rows = (count + state->exampleColumns - 1) / state->exampleColumns;
    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        state->desiredWidth96 = COUNTDOWN_BASE_WIDTH;
        return;
    }

    int gapPixels = CountdownScaleValue(state, 4);
    int explanationWidths[2] = {0, 0};
    for (int i = 0; i < count; i++) {
        int column = i / rows;
        wchar_t* equals = wcschr(lines[i], L'=');
        int lineWidth = 0;
        if (equals) {
            *equals = L'\0';
            wchar_t* tokenEnd = equals;
            while (tokenEnd > lines[i] &&
                   (tokenEnd[-1] == L' ' || tokenEnd[-1] == L'\t')) {
                *--tokenEnd = L'\0';
            }
            const wchar_t* explanation = equals + 1;
            while (*explanation == L' ' || *explanation == L'\t') {
                explanation++;
            }
            int tokenWidth = CountdownMeasureTextPixels(hdc, state->smallFont,
                                                         lines[i]);
            int explanationWidth = CountdownMeasureTextPixels(
                hdc, state->bodyFont, explanation);
            state->exampleTokenWidths[column] =
                max(state->exampleTokenWidths[column], tokenWidth);
            explanationWidths[column] =
                max(explanationWidths[column], explanationWidth);
            lineWidth = tokenWidth + gapPixels + explanationWidth;
        } else {
            lineWidth = CountdownMeasureTextPixels(hdc, state->bodyFont,
                                                   lines[i]);
        }
        state->exampleColumnWidths[column] =
            max(state->exampleColumnWidths[column], lineWidth);
    }
    ReleaseDC(hwnd, hdc);

    for (int column = 0; column < state->exampleColumns; column++) {
        int alignedWidth = state->exampleTokenWidths[column] + gapPixels +
                           explanationWidths[column] +
                           CountdownScaleValue(state, 16);
        state->exampleColumnWidths[column] =
            max(state->exampleColumnWidths[column], alignedWidth);
    }

    int totalPixels = state->exampleColumnWidths[0] +
                      CountdownScaleValue(state, 96);
    if (state->exampleColumns == 2) {
        totalPixels += state->exampleColumnWidths[1] +
                       CountdownScaleValue(state, 16);
    }
    int examplesWidth96 = MulDiv(totalPixels, 96,
                                 (int)(state->dpi ? state->dpi : 96u));

    HDC measureDc = GetDC(hwnd);
    int titlePixels = measureDc ?
        CountdownMeasureTextPixels(measureDc, state->titleFont, state->title) : 0;
    int formatPixels = measureDc ?
        CountdownMeasureTextPixels(measureDc, state->smallFont,
                                   state->formatLabel) : 0;
    if (measureDc) ReleaseDC(hwnd, measureDc);

    int titleWidth96 = MulDiv(titlePixels, 96,
                              (int)(state->dpi ? state->dpi : 96u)) + 96;
    int formatWidth96 = MulDiv(formatPixels, 96,
                               (int)(state->dpi ? state->dpi : 96u)) + 96;
    state->desiredWidth96 = COUNTDOWN_BASE_WIDTH;
    if (examplesWidth96 > state->desiredWidth96) {
        state->desiredWidth96 = examplesWidth96;
    }
    if (titleWidth96 > state->desiredWidth96) {
        state->desiredWidth96 = titleWidth96;
    }
    if (formatWidth96 > state->desiredWidth96) {
        state->desiredWidth96 = formatWidth96;
    }
}

void CountdownEnsureContentWidth(HWND hwnd,
                                        CountdownDialogState* state) {
    if (!hwnd || !state) {
        return;
    }

    int desired96 = state->desiredWidth96;
    if (desired96 < COUNTDOWN_BASE_WIDTH) {
        desired96 = COUNTDOWN_BASE_WIDTH;
    }

    UINT dpi = state->dpi ? state->dpi : CountdownGetDpi(hwnd);
    int desiredPixels = DialogModern_Scale(dpi, desired96);
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {0};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        int maxPixels = (monitorInfo.rcWork.right - monitorInfo.rcWork.left) -
                        DialogModern_Scale(dpi, 24);
        if (desiredPixels > maxPixels) {
            desiredPixels = maxPixels;
        }

        RECT current = {0};
        GetWindowRect(hwnd, &current);
        int height = current.bottom - current.top;
        int x = monitorInfo.rcWork.left +
                ((monitorInfo.rcWork.right - monitorInfo.rcWork.left) -
                 desiredPixels) / 2;
        int y = monitorInfo.rcWork.top +
                ((monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) - height) / 2;
        SetWindowPos(hwnd, NULL, x, y, desiredPixels, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    RECT current = {0};
    GetWindowRect(hwnd, &current);
    SetWindowPos(hwnd, NULL, current.left, current.top, desiredPixels,
                 current.bottom - current.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}
