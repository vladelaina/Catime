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
#include "utils/localized_duration.h"
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

#define COUNTDOWN_TITLE_HOVER_COLOR RGB(0xF7, 0x7D, 0xAA)

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
    BOOL sanitizingInput;
    BOOL titleHovered;
    CountdownHoverPart hoverPart;
    CountdownHoverPart pressedPart;
    RECT titleFrame;
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
    wchar_t invalidText[256];
    wchar_t previewText[512];
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

    if (state->hwndEdit) {
        DialogModern_ApplyTheme(GetParent(state->hwndEdit), state->darkMode);
        DialogModern_ApplyTheme(state->hwndEdit, state->darkMode);
    }

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
