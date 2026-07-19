/**
 * @file dialog_countdown.c
 * @brief Custom-drawn modern countdown input window.
 */

#include "dialog/dialog_input.h"
#include "dialog/dialog_input_internal.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_language.h"
#include "language.h"
#include "timer/timer.h"
#include "utils/string_convert.h"
#include "log.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <stdlib.h>
#include <strsafe.h>
#include <wchar.h>

#define INPUT_FOCUS_TIMER_ID 9999
#define INPUT_FOCUS_TIMER_DELAY_MS 50
#define COUNTDOWN_WINDOW_CLASS_NAME L"CatimeModernCountdownDialog"
#define COUNTDOWN_CLOSE_BUTTON_ID 0x7F01
#define COUNTDOWN_BUTTON_SUBCLASS_ID 0xC710
#define COUNTDOWN_EDIT_SUBCLASS_ID 0xC711
#define COUNTDOWN_BASE_WIDTH 500
#define COUNTDOWN_BASE_HEIGHT 390

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (WM_USER + 1)
#endif

typedef struct {
    DWORD dialogId;
    int pomodoroTimeIndex;
} CountdownInputState;

typedef enum {
    COUNTDOWN_HOVER_NONE = 0,
    COUNTDOWN_HOVER_CLOSE,
    COUNTDOWN_HOVER_CANCEL,
    COUNTDOWN_HOVER_START,
    COUNTDOWN_HOVER_EDIT
} CountdownHoverPart;

typedef struct {
    CountdownInputState input;
    HWND hwndEdit;
    HWND hwndStart;
    HWND hwndCancel;
    HWND hwndClose;
    HFONT titleFont;
    HFONT bodyFont;
    HFONT smallFont;
    HFONT editFont;
    HFONT buttonFont;
    HBRUSH editBrush;
    UINT dpi;
    BOOL darkMode;
    BOOL highContrast;
    BOOL inputValid;
    BOOL showValidationError;
    BOOL selectAllOnNextFocus;
    CountdownHoverPart hoverPart;
    CountdownHoverPart pressedPart;
    RECT editFrame;
    RECT closeFrame;
    COLORREF backgroundColor;
    COLORREF cardColor;
    COLORREF fieldColor;
    COLORREF borderColor;
    COLORREF textColor;
    COLORREF mutedColor;
    COLORREF accentColor;
    COLORREF accentHoverColor;
    COLORREF dangerColor;
    COLORREF dangerBackgroundColor;
    wchar_t title[128];
    wchar_t formatLabel[64];
    wchar_t fieldLabel[64];
    wchar_t examples[2048];
    wchar_t startText[64];
    wchar_t cancelText[64];
    wchar_t invalidText[128];
    wchar_t previewText[128];
    int exampleColumns;
    int exampleColumnWidths[2];
    int exampleTokenWidths[2];
    int desiredWidth96;
    int exampleVisibleCount;
    BOOL compactLayout;
    BOOL ultraCompactLayout;
} CountdownDialogState;

static LRESULT CALLBACK CountdownDialogProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam);
static HWND CreateCustomCountdownDialog(HWND hwndParent);
static int CountdownSplitExampleLines(const wchar_t* text,
                                      wchar_t lines[][256], int maxLines);
static void CountdownUpdateResponsiveMode(HWND hwnd,
                                          CountdownDialogState* state);
static void CountdownUpdateTextMetrics(HWND hwnd,
                                       CountdownDialogState* state);
static void CountdownEnsureContentWidth(HWND hwnd,
                                        CountdownDialogState* state);

static int CountdownScaleValue(const CountdownDialogState* state, int value) {
    return DialogModern_Scale(state && state->dpi ? state->dpi : 96, value);
}

static UINT CountdownGetDpi(HWND hwnd) {
    return DialogModern_GetDpi(hwnd);
}

static void CountdownCopyText(wchar_t* destination, size_t destinationCount,
                              const wchar_t* source, const wchar_t* fallback) {
    if (!destination || destinationCount == 0) {
        return;
    }

    const wchar_t* text = source && source[0] ? source : fallback;
    if (!text) {
        text = L"";
    }
    if (FAILED(StringCchCopyW(destination, destinationCount, text))) {
        destination[0] = L'\0';
    }
}

static void CountdownCopyEscapedText(wchar_t* destination, size_t destinationCount,
                                     const wchar_t* source, const wchar_t* fallback) {
    if (!destination || destinationCount == 0) {
        return;
    }

    const wchar_t* text = source && source[0] ? source : fallback;
    if (!text) {
        text = L"";
    }

    size_t out = 0;
    while (*text && out + 1 < destinationCount) {
        if (text[0] == L'\\' && text[1] == L'n') {
            destination[out++] = L'\n';
            text += 2;
        } else {
            destination[out++] = *text++;
        }
    }
    destination[out] = L'\0';
}

static void CountdownLoadTexts(CountdownDialogState* state) {
    static const wchar_t fallbackExamples[] =
        L"25 = 25 minutes\\n"
        L"25h = 25 hours\\n"
        L"25s = 25 seconds\\n"
        L"25 30 = 25 minutes 30 seconds\\n"
        L"25 30m = 25 hours 30 minutes\\n"
        L"1 30 20 = 1 hour 30 minutes 20 seconds\\n"
        L"17 20t = Countdown to 17:20\\n"
        L"9 9 9t = Countdown to 9:09:09";

    const wchar_t* title = GetDialogLocalizedString(CLOCK_IDD_DIALOG1, -1);
    const wchar_t* format = GetLocalizedString(NULL, L"Format");
    const wchar_t* field = GetLocalizedString(NULL, L"Countdown");
    const wchar_t* start = GetLocalizedString(NULL, L"Start");
    const wchar_t* cancel = GetLocalizedString(NULL, L"Cancel");
    const wchar_t* invalid = GetLocalizedString(NULL, L"Invalid input format");
    const wchar_t* examples =
        GetDialogLocalizedString(CLOCK_IDD_DIALOG1, CLOCK_IDC_STATIC);

    CountdownCopyText(state->title, _countof(state->title), title,
                      L"Set Countdown");
    CountdownCopyText(state->formatLabel, _countof(state->formatLabel), format,
                      L"Format");
    CountdownCopyText(state->fieldLabel, _countof(state->fieldLabel), field,
                      L"Countdown");
    CountdownCopyText(state->startText, _countof(state->startText), start,
                      L"Start");
    CountdownCopyText(state->cancelText, _countof(state->cancelText), cancel,
                      L"Cancel");
    CountdownCopyText(state->invalidText, _countof(state->invalidText), invalid,
                      L"Invalid input format");
    CountdownCopyEscapedText(state->examples, _countof(state->examples),
                             examples, fallbackExamples);
}

static void CountdownRefreshPalette(CountdownDialogState* state) {
    if (!state) {
        return;
    }

    DialogModernPalette palette;
    DialogModern_ResolvePalette(&palette);
    state->darkMode = palette.darkMode;
    state->highContrast = palette.highContrast;
    state->backgroundColor = palette.background;
    state->cardColor = palette.surface;
    state->fieldColor = palette.field;
    state->borderColor = palette.border;
    state->textColor = palette.text;
    state->mutedColor = palette.mutedText;
    state->accentColor = palette.accent;
    state->accentHoverColor = palette.accentHover;
    state->dangerColor = palette.danger;
    state->dangerBackgroundColor = palette.dangerBackground;

    if (state->editBrush) {
        DeleteObject(state->editBrush);
    }
    state->editBrush = CreateSolidBrush(state->fieldColor);
}

static HFONT CountdownCreateFont(UINT dpi, int pixelSize, LONG weight) {
    return DialogModern_CreateFont(dpi, pixelSize, weight);
}

static void CountdownDestroyFonts(CountdownDialogState* state) {
    if (!state) {
        return;
    }
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->bodyFont) DeleteObject(state->bodyFont);
    if (state->smallFont) DeleteObject(state->smallFont);
    if (state->editFont) DeleteObject(state->editFont);
    if (state->buttonFont) DeleteObject(state->buttonFont);
    state->titleFont = NULL;
    state->bodyFont = NULL;
    state->smallFont = NULL;
    state->editFont = NULL;
    state->buttonFont = NULL;
}

static void CountdownBuildFonts(CountdownDialogState* state) {
    if (!state) {
        return;
    }

    CountdownDestroyFonts(state);
    state->titleFont = CountdownCreateFont(state->dpi, 24, FW_SEMIBOLD);
    state->bodyFont = CountdownCreateFont(state->dpi, 12, FW_NORMAL);
    state->smallFont = CountdownCreateFont(state->dpi, 11, FW_SEMIBOLD);
    state->editFont = CountdownCreateFont(state->dpi, 20, FW_NORMAL);
    state->buttonFont = CountdownCreateFont(state->dpi, 13, FW_SEMIBOLD);

    if (state->hwndEdit && state->editFont) {
        SendMessageW(state->hwndEdit, WM_SETFONT, (WPARAM)state->editFont, TRUE);
    }
    if (state->hwndStart && state->buttonFont) {
        SendMessageW(state->hwndStart, WM_SETFONT, (WPARAM)state->buttonFont, TRUE);
    }
    if (state->hwndCancel && state->buttonFont) {
        SendMessageW(state->hwndCancel, WM_SETFONT, (WPARAM)state->buttonFont, TRUE);
    }
}

static int CountdownMeasureTextPixels(HDC hdc, HFONT font,
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

static int CountdownMeasureButtonWidth96(HWND hwnd, HFONT font,
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

static CountdownHoverPart CountdownPartForButton(const CountdownDialogState* state,
                                                 HWND hwndButton) {
    if (!state || !hwndButton) {
        return COUNTDOWN_HOVER_NONE;
    }
    if (hwndButton == state->hwndClose) return COUNTDOWN_HOVER_CLOSE;
    if (hwndButton == state->hwndCancel) return COUNTDOWN_HOVER_CANCEL;
    if (hwndButton == state->hwndStart) return COUNTDOWN_HOVER_START;
    return COUNTDOWN_HOVER_NONE;
}

static void CountdownTrackMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

static void CountdownApplyShape(HWND hwnd, const CountdownDialogState* state) {
    if (!hwnd || !state) {
        return;
    }
    DialogModern_ApplyWindowShape(hwnd, state->dpi, 20);
}

static void CountdownLayout(HWND hwnd, CountdownDialogState* state) {
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
    int fieldGapToFooter96 = state->ultraCompactLayout ? 76 :
                             (state->compactLayout ? 88 : 95);
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
        SetWindowPos(state->hwndEdit, NULL, editLeft, editTop,
                     editWidth, editHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
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

static void CountdownDrawRounded(HDC hdc, const RECT* rect, int radius,
                                 COLORREF fill, COLORREF border,
                                 int borderWidth) {
    DialogModern_DrawRoundedRect(hdc, rect, radius, fill, border, borderWidth);
}

static void CountdownDrawText(HDC hdc, HFONT font, COLORREF color,
                              const RECT* rect, const wchar_t* text,
                              UINT format) {
    DialogModern_DrawText(hdc, font, color, rect, text, format);
}

static int CountdownSplitExampleLines(const wchar_t* text,
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

static void CountdownDrawClockIcon(HDC hdc, int centerX, int centerY,
                                   int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, radius > 8 ? 2 : 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(hdc, centerX - radius, centerY - radius,
            centerX + radius, centerY + radius);
    MoveToEx(hdc, centerX, centerY, NULL);
    LineTo(hdc, centerX, centerY - radius / 2);
    MoveToEx(hdc, centerX, centerY, NULL);
    LineTo(hdc, centerX + radius / 2, centerY + radius / 3);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void CountdownDrawCheckIcon(HDC hdc, int centerX, int centerY,
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

static void CountdownDrawWarningIcon(HDC hdc, int centerX, int centerY,
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

static void CountdownUpdatePreview(HWND hwnd, CountdownDialogState* state) {
    if (!hwnd || !state || !state->hwndEdit) {
        return;
    }

    wchar_t text[256] = {0};
    GetWindowTextW(state->hwndEdit, text, (int)_countof(text));
    BOOL inputHasText = !Dialog_IsEmptyOrWhitespace(text);
    state->inputValid = FALSE;
    state->previewText[0] = L'\0';

    if (!inputHasText) {
        state->showValidationError = FALSE;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    char inputUtf8[256] = {0};
    int totalSeconds = 0;
    if (WideToUtf8(text, inputUtf8, sizeof(inputUtf8)) &&
        ParseInput(inputUtf8, &totalSeconds)) {
        char formatted[64] = {0};
        Dialog_FormatSecondsToString(totalSeconds, formatted, sizeof(formatted));
        if (MultiByteToWideChar(CP_UTF8, 0, formatted, -1,
                                state->previewText,
                                (int)_countof(state->previewText)) > 0) {
            state->inputValid = TRUE;
            state->showValidationError = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }

    state->showValidationError = TRUE;
    InvalidateRect(hwnd, NULL, FALSE);
}

static BOOL CountdownSubmit(HWND hwnd, CountdownDialogState* state) {
    if (!hwnd || !state || !state->hwndEdit) {
        return FALSE;
    }

    GetWindowTextW(state->hwndEdit, inputText, (int)_countof(inputText));
    if (Dialog_IsEmptyOrWhitespace(inputText)) {
        DestroyWindow(hwnd);
        return TRUE;
    }

    char inputUtf8[256] = {0};
    int totalSeconds = 0;
    if (!WideToUtf8(inputText, inputUtf8, sizeof(inputUtf8)) ||
        !ParseInput(inputUtf8, &totalSeconds)) {
        state->showValidationError = TRUE;
        state->inputValid = FALSE;
        CountdownUpdatePreview(hwnd, state);
        SetFocus(state->hwndEdit);
        SendMessageW(state->hwndEdit, EM_SETSEL, 0, -1);
        MessageBeep(MB_ICONWARNING);
        return FALSE;
    }

    HWND hwndParent = DialogInput_GetParent(hwnd);
    if (hwndParent) {
        PostMessage(hwndParent, WM_DIALOG_COUNTDOWN,
                    (WPARAM)totalSeconds, 0);
    }
    DestroyWindow(hwnd);
    return TRUE;
}

static void CountdownPaint(HWND hwnd, CountdownDialogState* state, HDC target) {
    if (!hwnd || !state || !target) {
        return;
    }

    RECT client = {0};
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    int margin = CountdownScaleValue(state, state->compactLayout ? 24 : 32);
    int smallGap = CountdownScaleValue(
        state, state->ultraCompactLayout ? 4 :
               (state->compactLayout ? 6 : 10));
    int radius = CountdownScaleValue(state, 18);
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

    int accentTop96 = state->compactLayout ? 12 : 18;
    RECT accent = {card.left + margin,
                   card.top + CountdownScaleValue(state, accentTop96),
                   card.left + margin + CountdownScaleValue(state, 44),
                   card.top + CountdownScaleValue(state, accentTop96 + 3)};
    CountdownDrawRounded(target, &accent, CountdownScaleValue(state, 2),
                         state->accentColor, state->accentColor, 0);

    int titleTop96 = state->compactLayout ? 20 : 30;
    int titleBottom96 = state->compactLayout ? 55 : 66;
    RECT titleRect = {margin, CountdownScaleValue(state, titleTop96),
                      width - margin - CountdownScaleValue(state, 64),
                      CountdownScaleValue(state, titleBottom96)};
    CountdownDrawText(target, state->titleFont, state->textColor, &titleRect,
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
    int previewHeight96 = state->ultraCompactLayout ? 18 :
                          (state->compactLayout ? 20 : 24);
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
        CountdownDrawText(target, state->smallFont, state->dangerColor,
                          &previewRect, state->invalidText,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    } else if (state->inputValid) {
        CountdownDrawCheckIcon(target,
                               previewRect.left - CountdownScaleValue(state, 9),
                               previewTop + CountdownScaleValue(
                                   state, previewHeight96 / 2),
                               CountdownScaleValue(state, 7),
                               state->accentColor);
        CountdownDrawText(target, state->smallFont, state->accentColor,
                          &previewRect, state->previewText,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
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
                                     state->accentColor, state->mutedColor,
                                     state->borderColor);
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
    if (pressed) {
        fill = primary ? state->accentHoverColor : state->borderColor;
    }

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
            if (state) {
                state->hoverPart = COUNTDOWN_HOVER_EDIT;
                CountdownTrackMouse(hwnd);
                InvalidateRect(parent, NULL, FALSE);
            }
            break;

        case WM_MOUSELEAVE:
            if (state && GetFocus() != hwnd &&
                state->hoverPart == COUNTDOWN_HOVER_EDIT) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(parent, NULL, FALSE);
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessageW(parent, WM_COMMAND,
                             MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED),
                             (LPARAM)hwnd);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                SendMessageW(parent, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wParam == VK_TAB) {
                CountdownMoveFocus(state, hwnd, GetKeyState(VK_SHIFT) < 0);
                return 0;
            }
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == VK_RETURN) {
                return 0;
            }
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, CountdownEditSubclassProc,
                                 COUNTDOWN_EDIT_SUBCLASS_ID);
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK CountdownButtonSubclassProc(HWND hwnd, UINT msg,
                                                    WPARAM wParam, LPARAM lParam,
                                                    UINT_PTR subclassId,
                                                    DWORD_PTR refData) {
    (void)subclassId;
    CountdownDialogState* state = (CountdownDialogState*)refData;
    CountdownHoverPart part = CountdownPartForButton(state, hwnd);
    HWND parent = GetParent(hwnd);

    switch (msg) {
        case WM_MOUSEMOVE:
            if (state) {
                state->hoverPart = part;
                CountdownTrackMouse(hwnd);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_MOUSELEAVE:
            if (state && state->hoverPart == part && GetCapture() != hwnd) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_LBUTTONDOWN:
            if (state) {
                state->pressedPart = part;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_LBUTTONUP:
            if (state) {
                state->pressedPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_CAPTURECHANGED:
            if (state && state->pressedPart == part) {
                state->pressedPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_SPACE) {
                if (part == COUNTDOWN_HOVER_CLOSE) {
                    SendMessageW(parent, WM_CLOSE, 0, 0);
                } else if (part == COUNTDOWN_HOVER_START) {
                    SendMessageW(parent, WM_COMMAND,
                                 MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED),
                                 (LPARAM)hwnd);
                } else if (part == COUNTDOWN_HOVER_CANCEL) {
                    SendMessageW(parent, WM_COMMAND,
                                 MAKEWPARAM(IDCANCEL, BN_CLICKED),
                                 (LPARAM)hwnd);
                }
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                SendMessageW(parent, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wParam == VK_TAB) {
                CountdownMoveFocus(state, hwnd, GetKeyState(VK_SHIFT) < 0);
                return 0;
            }
            break;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(NULL, part == COUNTDOWN_HOVER_CLOSE ?
                                         IDC_HAND : IDC_ARROW));
            return TRUE;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, CountdownButtonSubclassProc,
                                 COUNTDOWN_BUTTON_SUBCLASS_ID);
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void CountdownReleaseModifiers(void) {
    INPUT inputs[8] = {0};
    WORD keys[8] = {
        VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL,
        VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN
    };

    for (int i = 0; i < (int)_countof(keys); i++) {
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = keys[i];
        inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput((UINT)_countof(inputs), inputs, sizeof(INPUT));
}

static BOOL CountdownRegisterWindowClass(void) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW existing = {0};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, COUNTDOWN_WINDOW_CLASS_NAME, &existing)) {
        return TRUE;
    }

    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    windowClass.lpfnWndProc = CountdownDialogProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = NULL;
    windowClass.lpszClassName = COUNTDOWN_WINDOW_CLASS_NAME;
    if (RegisterClassExW(&windowClass)) {
        return TRUE;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static BOOL CountdownCreateControls(HWND hwnd, CountdownDialogState* state) {
    if (!hwnd || !state) {
        return FALSE;
    }

    CountdownLoadTexts(state);
    CountdownRefreshPalette(state);

    DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                       ES_AUTOHSCROLL | ES_LEFT | ES_NOHIDESEL;
    state->hwndEdit = CreateWindowExW(
        0, L"EDIT", L"", editStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)CLOCK_IDC_EDIT,
        GetModuleHandleW(NULL), NULL);

    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                         BS_OWNERDRAW | BS_NOTIFY;
    state->hwndCancel = CreateWindowExW(
        0, L"BUTTON", state->cancelText, buttonStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDCANCEL,
        GetModuleHandleW(NULL), NULL);
    state->hwndStart = CreateWindowExW(
        0, L"BUTTON", state->startText, buttonStyle | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)CLOCK_IDC_BUTTON_OK,
        GetModuleHandleW(NULL), NULL);
    state->hwndClose = CreateWindowExW(
        0, L"BUTTON", state->cancelText, buttonStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)COUNTDOWN_CLOSE_BUTTON_ID,
        GetModuleHandleW(NULL), NULL);

    if (!state->hwndEdit || !state->hwndCancel || !state->hwndStart ||
        !state->hwndClose) {
        return FALSE;
    }

    SendMessageW(state->hwndEdit, EM_SETLIMITTEXT,
                 (WPARAM)(_countof(inputText) - 1), 0);
    SendMessageW(state->hwndEdit, EM_SETMARGINS,
                 EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELONG(CountdownScaleValue(state, 2),
                          CountdownScaleValue(state, 2)));
    SendMessageW(state->hwndEdit, EM_SETCUEBANNER, TRUE,
                 (LPARAM)L"25m 30s");

    SetWindowSubclass(state->hwndEdit, CountdownEditSubclassProc,
                      COUNTDOWN_EDIT_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndCancel, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndStart, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndClose, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);

    CountdownBuildFonts(state);
    CountdownUpdateTextMetrics(hwnd, state);
    CountdownEnsureContentWidth(hwnd, state);
    CountdownLayout(hwnd, state);
    CountdownUpdatePreview(hwnd, state);

    SetWindowTextW(hwnd, state->title);
    Dialog_InitializeInstance(DIALOG_INSTANCE_INPUT, hwnd);
    g_hwndInputDialog = hwnd;

    SetFocus(state->hwndEdit);
    PostMessageW(hwnd, WM_APP + 200, 0, 0);
    PostMessageW(hwnd, WM_APP + 103, 0, 0);
    if (!SetTimer(hwnd, INPUT_FOCUS_TIMER_ID,
                  INPUT_FOCUS_TIMER_DELAY_MS, NULL)) {
        LOG_WARNING("CountdownDialog: failed to start focus timer (error=%lu)",
                    GetLastError());
    }
    return TRUE;
}

static HWND CreateCustomCountdownDialog(HWND hwndParent) {
    if (!CountdownRegisterWindowClass()) {
        return NULL;
    }

    UINT dpi = CountdownGetDpi(hwndParent);
    int width = MulDiv(COUNTDOWN_BASE_WIDTH, (int)dpi, 96);
    int height = MulDiv(COUNTDOWN_BASE_HEIGHT, (int)dpi, 96);
    HMONITOR monitor = MonitorFromWindow(hwndParent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {0};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        int availableWidth = workWidth - MulDiv(24, (int)dpi, 96);
        int availableHeight = workHeight - MulDiv(24, (int)dpi, 96);
        int minimumWidth = MulDiv(360, (int)dpi, 96);
        int minimumHeight = MulDiv(330, (int)dpi, 96);
        if (availableWidth < MulDiv(260, (int)dpi, 96)) {
            availableWidth = MulDiv(260, (int)dpi, 96);
        }
        if (availableHeight < MulDiv(280, (int)dpi, 96)) {
            availableHeight = MulDiv(280, (int)dpi, 96);
        }
        if (width < minimumWidth) width = minimumWidth;
        if (height < minimumHeight) height = minimumHeight;
        if (width > availableWidth) width = availableWidth;
        if (height > availableHeight) height = availableHeight;

        int x = monitorInfo.rcWork.left +
                ((monitorInfo.rcWork.right - monitorInfo.rcWork.left) - width) / 2;
        int y = monitorInfo.rcWork.top +
                ((monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) - height) / 2;
        return CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
            COUNTDOWN_WINDOW_CLASS_NAME, L"Set Countdown",
            WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            x, y, width, height, hwndParent, NULL,
            GetModuleHandleW(NULL), NULL);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    return CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        COUNTDOWN_WINDOW_CLASS_NAME, L"Set Countdown",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, width, height, hwndParent, NULL,
        GetModuleHandleW(NULL), NULL);
}

static LRESULT CALLBACK CountdownDialogProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam) {
    CountdownDialogState* state =
        (CountdownDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            CountdownDialogState* newState =
                (CountdownDialogState*)calloc(1, sizeof(*newState));
            if (!newState) {
                return FALSE;
            }
            newState->input.dialogId = CLOCK_IDD_DIALOG1;
            newState->input.pomodoroTimeIndex = -1;
            newState->dpi = CountdownGetDpi(hwnd);
            newState->selectAllOnNextFocus = TRUE;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)newState);
            return TRUE;
        }

        case WM_CREATE:
            state = (CountdownDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (!state || !CountdownCreateControls(hwnd, state)) {
                return -1;
            }
            return 0;

        case WM_PAINT: {
            if (!state) break;
            PAINTSTRUCT paint = {0};
            HDC target = BeginPaint(hwnd, &paint);
            RECT client = {0};
            GetClientRect(hwnd, &client);
            int width = client.right - client.left;
            int height = client.bottom - client.top;
            HDC buffer = CreateCompatibleDC(target);
            HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
            HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
            if (buffer && bitmap) {
                CountdownPaint(hwnd, state, buffer);
                BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, oldBitmap);
                DeleteObject(bitmap);
                DeleteDC(buffer);
            } else {
                CountdownPaint(hwnd, state, target);
                if (bitmap) DeleteObject(bitmap);
                if (buffer) DeleteDC(buffer);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_PRINTCLIENT:
            if (state) {
                CountdownPaint(hwnd, state, (HDC)wParam);
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_DRAWITEM:
            if (state && lParam) {
                CountdownDrawButton((const DRAWITEMSTRUCT*)lParam, state);
                return TRUE;
            }
            break;

        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            int notification = HIWORD(wParam);
            if (!state) break;
            if (controlId == CLOCK_IDC_BUTTON_OK &&
                (notification == BN_CLICKED || notification == 0)) {
                CountdownSubmit(hwnd, state);
                return 0;
            }
            if (controlId == IDCANCEL &&
                (notification == BN_CLICKED || notification == 0)) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (controlId == COUNTDOWN_CLOSE_BUTTON_ID &&
                (notification == BN_CLICKED || notification == 0)) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (controlId == CLOCK_IDC_EDIT) {
                if (notification == EN_CHANGE) {
                    state->showValidationError = FALSE;
                    CountdownUpdatePreview(hwnd, state);
                    return 0;
                }
                if (notification == EN_SETFOCUS || notification == EN_KILLFOCUS) {
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
            if (state) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->textColor);
                return (LRESULT)state->editBrush;
            }
            break;

        case WM_CTLCOLOREDIT:
            if (state) {
                SetTextColor((HDC)wParam, state->textColor);
                SetBkColor((HDC)wParam, state->fieldColor);
                SetBkMode((HDC)wParam, OPAQUE);
                return (LRESULT)state->editBrush;
            }
            break;

        case WM_CTLCOLORBTN:
            if (state) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                return (LRESULT)state->editBrush;
            }
            break;

        case WM_TIMER:
            if (wParam == INPUT_FOCUS_TIMER_ID && state) {
                KillTimer(hwnd, INPUT_FOCUS_TIMER_ID);
                SetForegroundWindow(hwnd);
                SetFocus(state->hwndEdit);
                SendMessageW(state->hwndEdit, EM_SETSEL, 0, -1);
                return 0;
            }
            break;

        case WM_APP + 200:
            if (state && state->hwndEdit && IsWindow(state->hwndEdit)) {
                SetForegroundWindow(hwnd);
                SetFocus(state->hwndEdit);
                SendMessageW(state->hwndEdit, EM_SETSEL, 0, -1);
            }
            return 0;

        case WM_APP + 103:
            CountdownReleaseModifiers();
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == VK_RETURN && state) {
                CountdownSubmit(hwnd, state);
                return 0;
            }
            break;

        case WM_NCHITTEST: {
            if (state) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                if (PtInRect(&state->closeFrame, point)) {
                    return HTCLIENT;
                }
                if (point.y < CountdownScaleValue(state, 86)) {
                    return HTCAPTION;
                }
            }
            return HTCLIENT;
        }

        case WM_MOUSEMOVE:
            if (state) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                state->hoverPart = PtInRect(&state->closeFrame, point) ?
                                    COUNTDOWN_HOVER_CLOSE :
                                    COUNTDOWN_HOVER_NONE;
                CountdownTrackMouse(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_MOUSELEAVE:
            if (state && state->hoverPart == COUNTDOWN_HOVER_CLOSE) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_DPICHANGED:
            if (state) {
                state->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96;
                RECT* suggested = (RECT*)lParam;
                if (suggested) {
                    SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                CountdownRefreshPalette(state);
                CountdownBuildFonts(state);
                CountdownUpdateTextMetrics(hwnd, state);
                CountdownEnsureContentWidth(hwnd, state);
                CountdownLayout(hwnd, state);
                return 0;
            }
            break;

        case WM_SIZE:
            if (state) {
                CountdownUpdateTextMetrics(hwnd, state);
                CountdownLayout(hwnd, state);
                return 0;
            }
            break;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (state) {
                CountdownRefreshPalette(state);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_NCDESTROY:
            if (state) {
                KillTimer(hwnd, INPUT_FOCUS_TIMER_ID);
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_INPUT, hwnd);
                if (g_hwndInputDialog == hwnd) {
                    g_hwndInputDialog = NULL;
                }
                if (state->editBrush) DeleteObject(state->editBrush);
                CountdownDestroyFonts(state);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                free(state);
            }
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}


/**
 * @brief Show countdown input dialog (modeless)
 * @param hwndParent Parent window handle
 */
void ShowCountdownInputDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_INPUT)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_INPUT);
        SetForegroundWindow(existing);
        return;
    }

    if (!DialogInput_IsValidParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateCustomCountdownDialog(hwndParent);
    if (!hwndDlg) {
        LOG_WARNING("CountdownDialog: custom window creation failed; using resource fallback");
        hwndDlg = DialogInput_CreateResourceDialog(hwndParent, CLOCK_IDD_DIALOG1,
                                            CLOCK_IDD_DIALOG1, -1);
    }

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOWNORMAL);
        UpdateWindow(hwndDlg);
        SetForegroundWindow(hwndDlg);
    }
}
