/**
 * @file dialog_countdown_render.c
 * @brief Examples grid and status icons.
 */

#include "dialog_countdown_internal.h"

void CountdownDrawExamplesGrid(HDC hdc,
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
        RECT explanationRect = cell;
        explanationRect.left = tokenRect.right + CountdownScaleValue(state, 4);

        CountdownDrawText(hdc, state->smallFont, state->accentColor,
                          &tokenRect, lines[i],
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER |
                          DT_END_ELLIPSIS);
        CountdownDrawText(hdc, state->bodyFont, state->mutedColor,
                          &explanationRect, explanation,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER |
                          DT_END_ELLIPSIS);
    }
}

void CountdownDrawCheckIcon(HDC hdc, int centerX, int centerY,
                                   int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, radius > 8 ? 2 : 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(hdc, centerX - radius, centerY - radius,
            centerX + radius, centerY + radius);
    MoveToEx(hdc, centerX - radius / 2, centerY, NULL);
    LineTo(hdc, centerX - radius / 8, centerY + radius / 2);
    LineTo(hdc, centerX + radius * 2 / 3, centerY - radius / 2);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void CountdownDrawWarningIcon(HDC hdc, int centerX, int centerY,
                                     int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, radius > 8 ? 2 : 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    POINT triangle[3] = {
        {centerX, centerY - radius},
        {centerX - radius, centerY + radius},
        {centerX + radius, centerY + radius}
    };
    Polygon(hdc, triangle, 3);
    MoveToEx(hdc, centerX, centerY - radius / 3, NULL);
    LineTo(hdc, centerX, centerY + radius / 3);
    Ellipse(hdc, centerX - 1, centerY + radius / 2 - 1,
            centerX + 1, centerY + radius / 2 + 1);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
