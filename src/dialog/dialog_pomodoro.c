#include "dialog/dialog_pomodoro.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_input.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "config.h"
#include "config/config_defaults.h"
#include "dialog/dialog_language.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <strsafe.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#define POMODORO_OPTIONS_MAX_INPUT_CHARS 512
#define POMODORO_OPTIONS_MAX_INPUT_BYTES ((POMODORO_OPTIONS_MAX_INPUT_CHARS * 4) + 1)
#define POMODORO_OPTIONS_TOKEN_DELIMITERS " \t\r\n"
static BOOL ConvertPomodoroInputToUtf8(const wchar_t* source, char* dest, size_t destSize) {
    if (!source || !dest || destSize == 0 || destSize > INT_MAX) {
        return FALSE;
    }
    dest[0] = '\0';
    int required = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > destSize) {
        return FALSE;
    }
    return WideCharToMultiByte(CP_UTF8, 0, source, -1, dest,
                               (int)destSize, NULL, NULL) > 0;
}
static BOOL AppendTextW(wchar_t* dest, size_t destBytes, const wchar_t* text) {
    if (!dest || destBytes == 0 || !text) {
        return FALSE;
    }
    return SUCCEEDED(StringCbCatW(dest, destBytes, text));
}
static BOOL BuildPomodoroOptionsDisplay(wchar_t* dest, size_t destBytes) {
    if (!dest || destBytes == 0) {
        return FALSE;
    }
    dest[0] = L'\0';
    int timesCount = g_AppConfig.pomodoro.times_count;
    if (timesCount <= 0 ||
        timesCount > MAX_POMODORO_TIMES ||
        timesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        return FALSE;
    }
    for (int i = 0; i < timesCount; i++) {
        if (g_AppConfig.pomodoro.times[i] <= 0 ||
            g_AppConfig.pomodoro.times[i] > MAX_POMODORO_OPTION_SECONDS) {
            return FALSE;
        }
        char timeStrA[32] = {0};
        wchar_t timeStr[32] = {0};
        Dialog_FormatSecondsToString(g_AppConfig.pomodoro.times[i],
                                     timeStrA, sizeof(timeStrA));
        if (MultiByteToWideChar(CP_UTF8, 0, timeStrA, -1,
                                timeStr, _countof(timeStr)) <= 0) {
            return FALSE;
        }
        if (i > 0 && !AppendTextW(dest, destBytes, L" ")) {
            return FALSE;
        }
        if (!AppendTextW(dest, destBytes, timeStr)) {
            return FALSE;
        }
    }
    return TRUE;
}
static BOOL BuildPomodoroOptionsFromInput(char* inputUtf8, int* times,
                                          int* count) {
    if (!inputUtf8 || !times || !count) {
        return FALSE;
    }
    *count = 0;
    const char* token = strtok(inputUtf8, POMODORO_OPTIONS_TOKEN_DELIMITERS);
    while (token) {
        if (*count >= MAX_POMODORO_TIMES) {
            return FALSE;
        }
        int seconds = 0;
        if (!TimeParser_ParseBasic(token, &seconds) ||
            seconds <= 0 || seconds > MAX_POMODORO_OPTION_SECONDS) {
            return FALSE;
        }
        times[*count] = seconds;
        (*count)++;
        token = strtok(NULL, POMODORO_OPTIONS_TOKEN_DELIMITERS);
    }
    return *count > 0;
}
void ShowPomodoroComboDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_POMODORO_COMBO)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_POMODORO_COMBO);
        SetForegroundWindow(existing);
        return;
    }
    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_POMODORO_COMBO_DIALOG),
        hwndParent,
        PomodoroComboDialogProc
    );
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    DialogContext* ctx = Dialog_GetContext(hwndDlg);
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_POMODORO_COMBO, hwndDlg);
            ctx = Dialog_CreateContext();
            if (!ctx) {
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_POMODORO_COMBO, hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            Dialog_SetContext(hwndDlg, ctx);
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);
            if (hwndEdit) {
                SendMessageW(hwndEdit, EM_SETLIMITTEXT,
                             POMODORO_OPTIONS_MAX_INPUT_CHARS, 0);
            }
            wchar_t currentOptions[POMODORO_OPTIONS_MAX_INPUT_CHARS + 1] = {0};
            if (BuildPomodoroOptionsDisplay(currentOptions, sizeof(currentOptions))) {
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            }
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            DialogFormLayout_ApplyInstruction(
                hwndDlg, CLOCK_IDC_STATIC, CLOCK_IDC_EDIT,
                CLOCK_IDC_BUTTON_OK);
            Dialog_CenterOnPrimaryScreen(hwndDlg);
            SetFocus(hwndEdit);
            Dialog_SelectAllText(hwndEdit);
            return FALSE;
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (Dialog_HandleColorMessages(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || LOWORD(wParam) == IDOK) {
                char input[POMODORO_OPTIONS_MAX_INPUT_BYTES] = {0};
                wchar_t winput[POMODORO_OPTIONS_MAX_INPUT_CHARS + 1] = {0};
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, winput, _countof(winput));
                if (!ConvertPomodoroInputToUtf8(winput, input, sizeof(input))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                if (Dialog_IsEmptyOrWhitespaceA(input)) {
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }
                char input_copy[POMODORO_OPTIONS_MAX_INPUT_BYTES] = {0};
                if (FAILED(StringCbCopyA(input_copy, sizeof(input_copy), input))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                int times[MAX_POMODORO_TIMES] = {0};
                int times_count = 0;
                if (!BuildPomodoroOptionsFromInput(input_copy, times, &times_count)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                if (!WriteConfigPomodoroTimeOptions(times, times_count)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
        case WM_DESTROY:
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_DestroyContext(hwndDlg);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_POMODORO_COMBO, hwndDlg);
            break;
    }
    return FALSE;
}
