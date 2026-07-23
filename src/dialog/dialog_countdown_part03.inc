
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

static void CountdownUpdateResponsiveMode(HWND hwnd,
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

static void CountdownUpdateTextMetrics(HWND hwnd,
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

static void CountdownEnsureContentWidth(HWND hwnd,
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

static void CountdownDrawExamplesGrid(HDC hdc,
                                      const CountdownDialogState* state,
                                      const RECT* rect) {
    if (!hdc || !state || !rect) {
        return;
    }

    wchar_t lines[12][256] = {{0}};
    int fullCount = CountdownSplitExampleLines(state->examples, lines,
                                               (int)_countof(lines));
    int count = state->exampleVisibleCount > 0 ?
                min(fullCount, state->exampleVisibleCount) : fullCount;
    if (count <= 0) {
        return;
    }

    int columns = state->exampleColumns > 0 ? state->exampleColumns :
                  (count > 4 ? 2 : (state->compactLayout && count > 1 ? 2 : 1));
    int rows = (count + columns - 1) / columns;
    int gap = CountdownScaleValue(state, 16);
    int availableWidth = rect->right - rect->left - gap * (columns - 1);
    int columnWidths[2] = {availableWidth, 0};
    int columnLefts[2] = {rect->left, rect->left};
    if (columns == 2) {
        int desiredTotal = state->exampleColumnWidths[0] +
                           state->exampleColumnWidths[1];
        int firstWidth = desiredTotal > 0 ?
            MulDiv(availableWidth, state->exampleColumnWidths[0],
                   desiredTotal) : availableWidth / 2;
        int minimumColumnWidth = availableWidth / 3;
        if (firstWidth < minimumColumnWidth) firstWidth = minimumColumnWidth;
        if (firstWidth > availableWidth - minimumColumnWidth) {
            firstWidth = availableWidth - minimumColumnWidth;
        }
        columnWidths[0] = firstWidth;
        columnWidths[1] = availableWidth - firstWidth;
        columnLefts[1] = rect->left + firstWidth + gap;
    }
    int rowHeight = (rect->bottom - rect->top) / rows;

    if (columns == 2) {
        int dividerX = rect->left + columnWidths[0] + gap / 2;
        HPEN dividerPen = CreatePen(PS_SOLID, 1, state->borderColor);
        HGDIOBJ oldPen = SelectObject(hdc, dividerPen);
        MoveToEx(hdc, dividerX, rect->top, NULL);
        LineTo(hdc, dividerX, rect->bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(dividerPen);
    }

    for (int i = 0; i < count; i++) {
        int column = i / rows;
        int row = i % rows;
        RECT cell = {
            columnLefts[column],
            rect->top + row * rowHeight,
            columnLefts[column] + columnWidths[column],
            rect->top + (row + 1) * rowHeight
        };

        wchar_t* equals = wcschr(lines[i], L'=');
        if (!equals) {
            CountdownDrawText(hdc, state->bodyFont, state->mutedColor,
                              &cell, lines[i],
                              DT_LEFT | DT_SINGLELINE | DT_VCENTER |
                              DT_END_ELLIPSIS);
            continue;
        }

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
        int tokenWidth = state->exampleTokenWidths[column];
        if (tokenWidth <= 0) {
            tokenWidth = CountdownScaleValue(state, 64);
        }
        tokenWidth += CountdownScaleValue(state, 4);
        int maximumTokenWidth = columnWidths[column] -
                                CountdownScaleValue(state, 40);
        if (tokenWidth > maximumTokenWidth) tokenWidth = maximumTokenWidth;
        if (tokenWidth < CountdownScaleValue(state, 24)) {
            tokenWidth = CountdownScaleValue(state, 24);
        }
        RECT tokenRect = cell;
        tokenRect.right = tokenRect.left + tokenWidth;
