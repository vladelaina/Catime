/**
 * @file dialog_pomodoro.c
 * @brief Pomodoro-specific dialogs implementation
 */

#include "dialog/dialog_pomodoro.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "dialog/dialog_input.h"
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

/* ============================================================================
 * Global State (defined in timer.c)
 * ============================================================================ */

/* Pomodoro configuration is now in g_AppConfig */

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

static BOOL ParsePomodoroLoopCount(const wchar_t* input, int* loopCount) {
    if (!input || !loopCount) return FALSE;

    int value = 0;
    BOOL hasDigit = FALSE;

    for (int i = 0; input[i]; i++) {
        if (iswspace(input[i])) {
            continue;
        }

        if (!iswdigit(input[i])) {
            return FALSE;
        }

        hasDigit = TRUE;
        value = value * 10 + (int)(input[i] - L'0');
        if (value > 100) {
            return FALSE;
        }
    }

    if (!hasDigit || value < 1) {
        return FALSE;
    }

    *loopCount = value;
    return TRUE;
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

/* ============================================================================
 * Pomodoro Loop Dialog Implementation
 * ============================================================================ */

void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_POMODORO_LOOP)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_POMODORO_LOOP);
        SetForegroundWindow(existing);
        return;
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LOOP_DIALOG),
        hwndParent,
        PomodoroLoopDialogProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

INT_PTR CALLBACK PomodoroLoopDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) {
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            Dialog_SetContext(hwndDlg, ctx);

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);

            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC,
                GetLocalizedString(NULL, 
                                 L"Please enter loop count (1-100):"));

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);

            /* Display current loop count value */
            if (g_AppConfig.pomodoro.loop_count > 0) {
                wchar_t loopCountStr[16];
                _snwprintf_s(loopCountStr, 16, _TRUNCATE, L"%d", g_AppConfig.pomodoro.loop_count);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, loopCountStr);
            }

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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));

                if (Dialog_IsEmptyOrWhitespace(input_str)) {
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                /* Range: 1-100 */
                int new_loop_count = 0;
                if (ParsePomodoroLoopCount(input_str, &new_loop_count)) {
                    extern BOOL WriteConfigPomodoroLoopCount(int loop_count);
                    if (!WriteConfigPomodoroLoopCount(new_loop_count)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                    DestroyWindow(hwndDlg);
                } else {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                }
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

        case WM_DESTROY:
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_DestroyContext(hwndDlg);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Pomodoro Combo Dialog Implementation
 * ============================================================================ */

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
            Dialog_RegisterInstance(DIALOG_INSTANCE_POMODORO_COMBO, hwndDlg);

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
