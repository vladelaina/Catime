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
#include "dialog/dialog_language.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <strsafe.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global State (defined in timer.c)
 * ============================================================================ */

/* Pomodoro configuration is now in g_AppConfig */

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
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_POMODORO_LOOP, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;
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
            Dialog_ApplyTopmost(hwndDlg);

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

                if (!Dialog_IsValidNumberInput(input_str)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }

                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }

                /* Range: 1-100 */
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {
                    extern void WriteConfigPomodoroLoopCount(int loop_count);
                    WriteConfigPomodoroLoopCount(new_loop_count);
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

        case WM_DESTROY:
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_FreeContext(ctx);
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_POMODORO_LOOP);
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
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_POMODORO_COMBO, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;
            Dialog_SetContext(hwndDlg, ctx);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);

            wchar_t currentOptions[256] = {0};
            for (int i = 0; i < g_AppConfig.pomodoro.times_count; i++) {
                char timeStrA[32];
                wchar_t timeStr[32];
                Dialog_FormatSecondsToString(g_AppConfig.pomodoro.times[i], timeStrA, sizeof(timeStrA));
                MultiByteToWideChar(CP_UTF8, 0, timeStrA, -1, timeStr, 32);
                StringCbCatW(currentOptions, sizeof(currentOptions), timeStr);
                StringCbCatW(currentOptions, sizeof(currentOptions), L" ");
            }

            if (wcslen(currentOptions) > 0 && currentOptions[wcslen(currentOptions) - 1] == L' ') {
                currentOptions[wcslen(currentOptions) - 1] = L'\0';
            }

            SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, currentOptions);

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);

            Dialog_CenterOnPrimaryScreen(hwndDlg);
            Dialog_ApplyTopmost(hwndDlg);

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
                char input[256] = {0};

                wchar_t winput[256];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, winput, sizeof(winput)/sizeof(wchar_t));
                WideCharToMultiByte(CP_UTF8, 0, winput, -1, input, sizeof(input), NULL, NULL);

                if (Dialog_IsEmptyOrWhitespaceA(input)) {
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }

                char *token;
                char input_copy[256];
                StringCbCopyA(input_copy, sizeof(input_copy), input);

                int times[MAX_POMODORO_TIMES] = {0};
                int times_count = 0;
                BOOL hasInvalidInput = FALSE;

                token = strtok(input_copy, " ");
                while (token && times_count < MAX_POMODORO_TIMES) {
                    int seconds = 0;
                    if (TimeParser_ParseBasic(token, &seconds)) {
                        times[times_count++] = seconds;
                    } else {
                        hasInvalidInput = TRUE;
                        break;
                    }
                    token = strtok(NULL, " ");
                }

                if (hasInvalidInput || times_count == 0) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }

                g_AppConfig.pomodoro.times_count = times_count;
                for (int i = 0; i < times_count; i++) {
                    g_AppConfig.pomodoro.times[i] = times[i];
                }

                if (times_count > 0) g_AppConfig.pomodoro.work_time = times[0];
                if (times_count > 1) g_AppConfig.pomodoro.short_break = times[1];
                if (times_count > 2) g_AppConfig.pomodoro.long_break = times[2];

                extern void WriteConfigPomodoroTimeOptions(int* times, int count);
                WriteConfigPomodoroTimeOptions(times, times_count);

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
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
                Dialog_FreeContext(ctx);
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_POMODORO_COMBO);
            break;
    }

    return FALSE;
}

