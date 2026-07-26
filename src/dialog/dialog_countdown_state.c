/**
 * @file dialog_countdown_state.c
 * @brief State, text, palette, and font helpers.
 */

#include "dialog_countdown_internal.h"

int CountdownScaleValue(const CountdownDialogState* state, int value) {
    return DialogModern_Scale(state && state->dpi ? state->dpi : 96, value);
}

UINT CountdownGetDpi(HWND hwnd) {
    return DialogModern_GetDpi(hwnd);
}

void CountdownCopyText(wchar_t* destination, size_t destinationCount,
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

void CountdownCopyEscapedText(wchar_t* destination, size_t destinationCount,
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

void CountdownLoadTexts(CountdownDialogState* state) {
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

void CountdownRefreshPalette(CountdownDialogState* state) {
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

HFONT CountdownCreateFont(UINT dpi, int pixelSize, LONG weight) {
    return DialogModern_CreateFont(dpi, pixelSize, weight);
}

void CountdownDestroyFonts(CountdownDialogState* state) {
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

void CountdownBuildFonts(CountdownDialogState* state) {
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
