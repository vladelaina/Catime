#include "dialog_input_state.h"
#include "dialog/dialog_input_internal.h"
#include "dialog/dialog_error.h"
#include "dialog/dialog_input_options.h"
#include "config.h"
#include "timer/timer.h"
#include "utils/string_convert.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

extern wchar_t inputText[256];
BOOL WriteConfigPomodoroLoopCount(int loopCount);

static wchar_t* ReadDialogText(HWND dialog, size_t maxChars) {
    HWND edit = GetDlgItem(dialog, CLOCK_IDC_EDIT);
    if (!edit || maxChars > INT_MAX) return NULL;
    int length = GetWindowTextLengthW(edit);
    if (length < 0 || (size_t)length > maxChars) return NULL;
    wchar_t* text = (wchar_t*)calloc((size_t)length + 1, sizeof(wchar_t));
    if (!text) return NULL;
    if (GetWindowTextW(edit, text, length + 1) != length) {
        free(text);
        return NULL;
    }
    return text;
}

static char* ConvertInputToUtf8(const wchar_t* source, size_t maxBytes) {
    if (!source || !maxBytes || maxBytes > INT_MAX) return NULL;
    int required = WideCharToMultiByte(CP_UTF8, 0, source, -1,
                                       NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > maxBytes) return NULL;
    char* output = (char*)calloc((size_t)required, 1);
    if (!output) return NULL;
    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, output,
                             required, NULL, NULL)) {
        free(output);
        return NULL;
    }
    return output;
}

static BOOL HandleShortcutSubmit(HWND dialog) {
    wchar_t* wideInput = ReadDialogText(dialog,
                                        QUICK_TIME_OPTIONS_MAX_INPUT_CHARS);
    if (!wideInput) return FALSE;
    if (Dialog_IsEmptyOrWhitespace(wideInput)) {
        free(wideInput);
        DestroyWindow(dialog);
        return TRUE;
    }
    char* input = ConvertInputToUtf8(wideInput,
                                     QUICK_TIME_OPTIONS_MAX_INPUT_BYTES);
    free(wideInput);
    if (!input) return FALSE;
    char options[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    int parsedSeconds[MAX_TIME_OPTIONS] = {0};
    int count = 0;
    BOOL parsed = DialogInputOptions_ParseConfig(input, options, sizeof(options),
                                                  parsedSeconds, &count);
    free(input);
    if (!parsed || !WriteConfigTimeOptions(options)) return FALSE;
    time_options_count = count;
    memcpy(time_options, parsedSeconds, (size_t)count * sizeof(time_options[0]));
    HWND parent = DialogInput_GetParent(dialog);
    if (parent) PostMessage(parent, WM_DIALOG_SHORTCUT, 0, 0);
    DestroyWindow(dialog);
    return TRUE;
}

BOOL DialogInput_PersistParsedTime(HWND dialog, DWORD dialogId,
                                   int pomodoroTimeIndex, int seconds) {
    if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
        int count = g_AppConfig.pomodoro.times_count;
        if (count < 0) count = 0;
        if (count > (int)_countof(g_AppConfig.pomodoro.times))
            count = (int)_countof(g_AppConfig.pomodoro.times);
        if (pomodoroTimeIndex < 0 || pomodoroTimeIndex >= count)
            return FALSE;
        int updated[_countof(g_AppConfig.pomodoro.times)] = {0};
        memcpy(updated, g_AppConfig.pomodoro.times, (size_t)count * sizeof(int));
        updated[pomodoroTimeIndex] = seconds;
        return WriteConfigPomodoroTimeOptions(updated, count);
    }
    if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG)
        return WriteConfigPomodoroLoopCount(seconds);
    if (dialogId == CLOCK_IDD_STARTUP_DIALOG)
        return WriteConfigDefaultCountdownStartup(seconds);
    HWND parent = DialogInput_GetParent(dialog);
    if (parent) PostMessage(parent, WM_DIALOG_COUNTDOWN, (WPARAM)seconds, 0);
    return TRUE;
}

static BOOL HandleTimeSubmit(HWND dialog, const InputDialogState* state) {
    GetDlgItemTextW(dialog, CLOCK_IDC_EDIT, inputText, _countof(inputText));
    if (Dialog_IsEmptyOrWhitespace(inputText)) {
        DestroyWindow(dialog);
        return TRUE;
    }
    char utf8[256] = {0};
    int seconds = 0;
    if (!WideToUtf8(inputText, utf8, sizeof(utf8)) ||
        !ParseInput(utf8, &seconds) ||
        !DialogInput_PersistParsedTime(dialog, state->dialogId,
                                       state->pomodoroTimeIndex, seconds))
        return FALSE;
    DestroyWindow(dialog);
    return TRUE;
}

BOOL DialogInput_HandleCommand(HWND hwndDlg, DialogContext* ctx,
                               WPARAM wParam) {
    int controlId = LOWORD(wParam);
    if (controlId == IDCANCEL) {
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    if (controlId != CLOCK_IDC_BUTTON_OK) return FALSE;
    const InputDialogState* state = DialogInput_GetState(ctx);
    if (!state) return TRUE;
    BOOL success = state->dialogId == CLOCK_IDD_SHORTCUT_DIALOG
        ? HandleShortcutSubmit(hwndDlg) : HandleTimeSubmit(hwndDlg, state);
    if (!success) Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
    return TRUE;
}
