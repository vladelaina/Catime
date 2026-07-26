/**
 * @file dialog_countdown_input.c
 * @brief Input filtering, preview, and submission.
 */

#include "dialog_countdown_internal.h"

BOOL CountdownIsAllowedInputChar(wchar_t value) {
    return (value >= L'A' && value <= L'Z') ||
           (value >= L'a' && value <= L'z') ||
           (value >= L'0' && value <= L'9') ||
           value == L' ';
}

size_t CountdownCopyAllowedInput(wchar_t* destination,
                                        size_t destinationCount,
                                        const wchar_t* source) {
    if (!destination || destinationCount == 0) return 0;
    destination[0] = L'\0';
    if (!source) return 0;

    size_t output = 0;
    while (*source && output + 1 < destinationCount) {
        if (CountdownIsAllowedInputChar(*source)) {
            destination[output++] = *source;
        }
        source++;
    }
    destination[output] = L'\0';
    return output;
}

DWORD CountdownCountAllowedInput(const wchar_t* source, DWORD limit) {
    if (!source) return 0;
    DWORD count = 0;
    for (DWORD i = 0; i < limit && source[i]; i++) {
        if (CountdownIsAllowedInputChar(source[i])) count++;
    }
    return count;
}

void CountdownNormalizeInputKey(const wchar_t* begin,
                                       const wchar_t* end,
                                       wchar_t* destination,
                                       size_t destinationCount) {
    if (!destination || destinationCount == 0) return;
    destination[0] = L'\0';
    if (!begin) return;

    size_t output = 0;
    const wchar_t* cursor = begin;
    while (*cursor && (!end || cursor < end) &&
           output + 1 < destinationCount) {
        wchar_t value = *cursor++;
        if ((value >= L'A' && value <= L'Z') ||
            (value >= L'a' && value <= L'z') ||
            (value >= L'0' && value <= L'9')) {
            if (value >= L'A' && value <= L'Z') {
                value = (wchar_t)(value - L'A' + L'a');
            }
            destination[output++] = value;
        }
    }
    destination[output] = L'\0';
}

BOOL CountdownBuildExamplePreview(const CountdownDialogState* state,
                                         const wchar_t* input,
                                         wchar_t* destination,
                                         size_t destinationCount) {
    if (!state || !input || !destination || destinationCount == 0) {
        return FALSE;
    }

    wchar_t inputKey[256] = {0};
    CountdownNormalizeInputKey(input, NULL, inputKey, _countof(inputKey));

    wchar_t lines[12][256] = {{0}};
    int lineCount = CountdownSplitExampleLines(
        state->examples, lines, (int)_countof(lines));
    for (int i = 0; i < lineCount; i++) {
        wchar_t* equals = wcschr(lines[i], L'=');
        if (!equals) continue;

        wchar_t lineKey[256] = {0};
        CountdownNormalizeInputKey(lines[i], equals, lineKey,
                                   _countof(lineKey));
        if (!inputKey[0] || wcscmp(inputKey, lineKey) != 0) continue;

        const wchar_t* explanation = equals + 1;
        while (*explanation == L' ' || *explanation == L'\t') {
            explanation++;
        }
        return SUCCEEDED(StringCchCopyW(destination, destinationCount,
                                        explanation));
    }
    return FALSE;
}

void CountdownBuildPreviewText(const CountdownDialogState* state,
                                      const wchar_t* input,
                                      int totalSeconds,
                                      wchar_t* destination,
                                      size_t destinationCount) {
    if (!destination || destinationCount == 0) return;
    destination[0] = L'\0';
    if (CountdownBuildExamplePreview(state, input,
                                     destination, destinationCount)) {
        return;
    }
    LocalizedDuration_Format(totalSeconds, destination, destinationCount);
}

void CountdownSanitizeEditText(HWND hwnd,
                                      CountdownDialogState* state) {
    if (!hwnd || !state || state->sanitizingInput) return;

    wchar_t source[256] = {0};
    GetWindowTextW(hwnd, source, (int)_countof(source));
    wchar_t filtered[256] = {0};
    CountdownCopyAllowedInput(filtered, _countof(filtered), source);
    if (wcscmp(source, filtered) == 0) return;

    DWORD selection = (DWORD)SendMessageW(hwnd, EM_GETSEL, 0, 0);
    DWORD selectionStart = LOWORD(selection);
    DWORD selectionEnd = HIWORD(selection);
    size_t sourceLength = wcslen(source);
    if (selectionStart > sourceLength) selectionStart = (DWORD)sourceLength;
    if (selectionEnd > sourceLength) selectionEnd = (DWORD)sourceLength;

    DWORD mappedStart = CountdownCountAllowedInput(source, selectionStart);
    DWORD mappedEnd = CountdownCountAllowedInput(source, selectionEnd);

    state->sanitizingInput = TRUE;
    SetWindowTextW(hwnd, filtered);
    SendMessageW(hwnd, EM_SETSEL, mappedStart, mappedEnd);
    state->sanitizingInput = FALSE;
}

void CountdownUpdatePreview(HWND hwnd, CountdownDialogState* state) {
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
        CountdownBuildPreviewText(state, text, totalSeconds,
                                  state->previewText,
                                  _countof(state->previewText));
        if (state->previewText[0]) {
            state->inputValid = TRUE;
            state->showValidationError = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }

    state->showValidationError = TRUE;
    InvalidateRect(hwnd, NULL, FALSE);
}

BOOL CountdownSubmit(HWND hwnd, CountdownDialogState* state) {
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
