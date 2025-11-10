/**
 * @file dialog_input.c
 * @brief Generic input dialog and time parsing implementation
 */

#include "dialog/dialog_input.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "language.h"
#include "timer/timer.h"
#include "config.h"
#include "dialog/dialog_language.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strsafe.h>

/* ============================================================================
 * Global State (declared in main.c and header files)
 * ============================================================================ */

/* Note: inputText is defined in main.c */
extern wchar_t inputText[256];

/* g_hwndInputDialog is defined here for dialog management */
HWND g_hwndInputDialog = NULL;

/* Global variable to pass the selected pomodoro time index */
int g_pomodoroSelectedIndex = -1;

/* ============================================================================
 * Dialog Procedure
 * ============================================================================ */

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;
            
            ctx->userData = (void*)lParam;
            Dialog_SetContext(hwndDlg, ctx);

            g_hwndInputDialog = hwndDlg;

            Dialog_ApplyTopmost(hwndDlg);
            Dialog_CenterOnPrimaryScreen(hwndDlg);

            DWORD dlgId = (DWORD)(LONG_PTR)ctx->userData;

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);

            /* Populate edit with current values */
            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) {
                extern int time_options[];
                extern int time_options_count;

                char currentOptions[256] = {0};
                for (int i = 0; i < time_options_count; i++) {
                    char timeStr[32];
                    Dialog_FormatSecondsToString(time_options[i], timeStr, sizeof(timeStr));

                    if (i > 0) {
                        StringCbCatA(currentOptions, sizeof(currentOptions), " ");
                    }
                    StringCbCatA(currentOptions, sizeof(currentOptions), timeStr);
                }

                wchar_t wcurrentOptions[256];
                MultiByteToWideChar(CP_UTF8, 0, currentOptions, -1, wcurrentOptions, 256);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcurrentOptions);
            } else if (dlgId == CLOCK_IDD_STARTUP_DIALOG) {
                if (g_AppConfig.timer.default_start_time > 0) {
                    char timeStr[64];
                    Dialog_FormatSecondsToString(g_AppConfig.timer.default_start_time, timeStr, sizeof(timeStr));

                    wchar_t wtimeStr[64];
                    MultiByteToWideChar(CP_UTF8, 0, timeStr, -1, wtimeStr, 64);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                }
            } else if (dlgId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                /* Display current pomodoro time value */
                if (g_pomodoroSelectedIndex >= 0 && g_pomodoroSelectedIndex < g_AppConfig.pomodoro.times_count) {
                    char timeStr[64];
                    Dialog_FormatSecondsToString(g_AppConfig.pomodoro.times[g_pomodoroSelectedIndex],
                                                 timeStr, sizeof(timeStr));

                    wchar_t wtimeStr[64];
                    MultiByteToWideChar(CP_UTF8, 0, timeStr, -1, wtimeStr, 64);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                }
            }

            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            SetFocus(hwndEdit);

            /* Aggressive focus handling for topmost dialogs */
            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);

            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);
            SetTimer(hwndDlg, 9999, 50, NULL);

            /* Release stuck modifier keys (hotkey aftermath) */
            PostMessage(hwndDlg, WM_APP+103, 0, 0);

            /* Show build date */
            char month[4];
            int day, year, hour, min, sec;

            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), 
                          L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                          year, month_num, day, hour, min, sec);

            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            return FALSE;
        }

        case WM_CLOSE: {
            g_hwndInputDialog = NULL;
            EndDialog(hwndDlg, 0);
            return TRUE;
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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText)/sizeof(wchar_t));

                if (Dialog_IsEmptyOrWhitespace(inputText)) {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }

                int dialogId = (int)(LONG_PTR)ctx->userData;
                
                if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
                    extern int time_options[];
                    extern int time_options_count;
                    extern void WriteConfigTimeOptions(const char* options);
                    
                    char inputCopy[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputCopy, sizeof(inputCopy), NULL, NULL);
                    
                    char* token = strtok(inputCopy, " ");
                    char options[256] = {0};
                    int valid = 1;
                    int count = 0;
                    
                    while (token && count < MAX_TIME_OPTIONS) {
                        int seconds = 0;
                        if (!TimeParser_ParseBasic(token, &seconds) || seconds <= 0) {
                            valid = 0;
                            break;
                        }
                        
                        if (count > 0) {
                            StringCbCatA(options, sizeof(options), ",");
                        }
                        
                        char secondsStr[32];
                        snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
                        StringCbCatA(options, sizeof(options), secondsStr);
                        count++;
                        token = strtok(NULL, " ");
                    }
                    
                    if (valid && count > 0) {
                        WriteConfigTimeOptions(options);
                        time_options_count = 0;
                        char optionsCopy[256];
                        strncpy(optionsCopy, options, sizeof(optionsCopy) - 1);
                        optionsCopy[sizeof(optionsCopy) - 1] = '\0';
                        char* tok = strtok(optionsCopy, ",");
                        while (tok && time_options_count < 10) {
                            while (*tok == ' ') tok++;
                            time_options[time_options_count++] = atoi(tok);
                            tok = strtok(NULL, ",");
                        }
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                } else {
                    char inputUtf8[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputUtf8, sizeof(inputUtf8), NULL, NULL);
                    
                    int total_seconds;
                    if (ParseInput(inputUtf8, &total_seconds)) {
                        if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        } else if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
                            extern void WriteConfigPomodoroLoopCount(int loop_count);
                            WriteConfigPomodoroLoopCount(total_seconds);
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        } else if (dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                            extern void WriteConfigDefaultStartTime(int seconds);
                            WriteConfigDefaultStartTime(total_seconds);
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        } else {
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        }
                    } else {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                }
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wParam == 9999) {
                KillTimer(hwndDlg, 9999);
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    SetForegroundWindow(hwndDlg);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                return TRUE;
            }
            break;

        case WM_APP+100:
        case WM_APP+101:
        case WM_APP+102:
            if (lParam) {
                HWND hwndEdit = (HWND)lParam;
                if (IsWindow(hwndEdit) && IsWindowVisible(hwndEdit)) {
                    SetForegroundWindow(hwndDlg);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
            }
            return TRUE;

        case WM_APP+103: {
            /* Release stuck modifier keys */
            INPUT inputs[8] = {0};
            int inputCount = 0;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_LSHIFT;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_RSHIFT;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_LCONTROL;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_RCONTROL;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_LMENU;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_RMENU;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_LWIN;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_RWIN;
            inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
            inputCount++;

            SendInput(inputCount, inputs, sizeof(INPUT));
            return TRUE;
        }

        case WM_DESTROY:
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_FreeContext(ctx);
            }
            g_hwndInputDialog = NULL;
            break;
    }
    return FALSE;
}

