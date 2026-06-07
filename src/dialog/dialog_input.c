/**
 * @file dialog_input.c
 * @brief Generic input dialog and time parsing implementation (modeless version)
 */

#include "dialog/dialog_input.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "language.h"
#include "timer/timer.h"
#include "config.h"
#include "dialog/dialog_language.h"
#include "utils/time_parser.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strsafe.h>
#include <wchar.h>

/* ============================================================================
 * Global State (declared in main.c and header files)
 * ============================================================================ */

/* Note: inputText is defined in main.c */
extern wchar_t inputText[256];

/* g_hwndInputDialog is defined here for dialog management */
HWND g_hwndInputDialog = NULL;

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define INPUT_FOCUS_TIMER_ID 9999
#define INPUT_FOCUS_TIMER_DELAY_MS 50
#define QUICK_TIME_OPTIONS_MAX_INPUT_CHARS 2048
#define QUICK_TIME_OPTIONS_MAX_INPUT_BYTES ((QUICK_TIME_OPTIONS_MAX_INPUT_CHARS * 4) + 1)
#define QUICK_TIME_OPTIONS_TOKEN_DELIMITERS " \t\r\n"

typedef struct {
    DWORD dialogId;
    int pomodoroTimeIndex;
} InputDialogState;

static BOOL IsValidInputDialogParentWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static HWND GetInputDialogParent(HWND hwndDlg) {
    HWND hwndParent = hwndDlg ? GetParent(hwndDlg) : NULL;
    return IsValidInputDialogParentWindow(hwndParent) ? hwndParent : NULL;
}

static BOOL ConvertDialogInputToUtf8(const wchar_t* source, char* dest, size_t destSize) {
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

static char* ConvertDialogInputToUtf8Alloc(const wchar_t* source, size_t maxBytes) {
    if (!source || maxBytes == 0 || maxBytes > INT_MAX) {
        return NULL;
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > maxBytes) {
        return NULL;
    }

    char* dest = (char*)calloc((size_t)required, sizeof(char));
    if (!dest) {
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, source, -1, dest,
                            required, NULL, NULL) <= 0) {
        free(dest);
        return NULL;
    }

    return dest;
}

static wchar_t* ReadDialogTextAlloc(HWND hwndDlg, int controlId, size_t maxChars) {
    HWND hwndEdit = GetDlgItem(hwndDlg, controlId);
    if (!hwndEdit || !IsWindow(hwndEdit) || maxChars > (size_t)INT_MAX) {
        return NULL;
    }

    int textLength = GetWindowTextLengthW(hwndEdit);
    if (textLength < 0 || (size_t)textLength > maxChars) {
        return NULL;
    }

    wchar_t* text = (wchar_t*)calloc((size_t)textLength + 1, sizeof(wchar_t));
    if (!text) {
        return NULL;
    }

    UINT copied = GetDlgItemTextW(hwndDlg, controlId, text, textLength + 1);
    if (copied != (UINT)textLength) {
        free(text);
        return NULL;
    }

    return text;
}

static BOOL AppendTextA(char* dest, size_t destSize, const char* suffix) {
    if (!dest || destSize == 0 || !suffix) {
        return FALSE;
    }

    return SUCCEEDED(StringCbCatA(dest, destSize, suffix));
}

static BOOL BuildQuickTimeOptionsDisplay(char* dest, size_t destSize) {
    if (!dest || destSize == 0) {
        return FALSE;
    }

    dest[0] = '\0';
    int timeOptionsCount = time_options_count;
    if (timeOptionsCount < 0) timeOptionsCount = 0;
    if (timeOptionsCount > MAX_TIME_OPTIONS) timeOptionsCount = MAX_TIME_OPTIONS;

    int appendedCount = 0;
    for (int i = 0; i < timeOptionsCount; i++) {
        if (time_options[i] <= 0 || time_options[i] > MAX_TIME_OPTION_SECONDS) {
            return FALSE;
        }

        char timeStr[32] = {0};
        Dialog_FormatSecondsToString(time_options[i], timeStr, sizeof(timeStr));
        if (timeStr[0] == '\0') {
            return FALSE;
        }

        if (appendedCount > 0 && !AppendTextA(dest, destSize, " ")) {
            return FALSE;
        }
        if (!AppendTextA(dest, destSize, timeStr)) {
            return FALSE;
        }
        appendedCount++;
    }

    return TRUE;
}

static BOOL BuildQuickTimeOptionsConfig(char* inputUtf8, char* options,
                                        size_t optionsSize, int* parsedSeconds,
                                        int* parsedCount) {
    if (!inputUtf8 || !options || optionsSize == 0 ||
        !parsedSeconds || !parsedCount) {
        return FALSE;
    }

    options[0] = '\0';
    *parsedCount = 0;

    const char* token = strtok(inputUtf8, QUICK_TIME_OPTIONS_TOKEN_DELIMITERS);
    while (token) {
        if (*parsedCount >= MAX_TIME_OPTIONS) {
            return FALSE;
        }

        int seconds = 0;
        if (!TimeParser_ParseBasic(token, &seconds) ||
            seconds <= 0 || seconds > MAX_TIME_OPTION_SECONDS) {
            return FALSE;
        }

        char secondsStr[32] = {0};
        int written = snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
        if (written < 0 || (size_t)written >= sizeof(secondsStr)) {
            return FALSE;
        }

        if (*parsedCount > 0 && !AppendTextA(options, optionsSize, ",")) {
            return FALSE;
        }
        if (!AppendTextA(options, optionsSize, secondsStr)) {
            return FALSE;
        }

        parsedSeconds[*parsedCount] = seconds;
        (*parsedCount)++;
        token = strtok(NULL, QUICK_TIME_OPTIONS_TOKEN_DELIMITERS);
    }

    return *parsedCount > 0;
}

static InputDialogState* CreateInputDialogState(DWORD dialogId, int pomodoroTimeIndex) {
    InputDialogState* state = (InputDialogState*)calloc(1, sizeof(*state));
    if (!state) {
        return NULL;
    }

    state->dialogId = dialogId;
    state->pomodoroTimeIndex = pomodoroTimeIndex;
    return state;
}

static InputDialogState* GetInputDialogState(DialogContext* ctx) {
    return ctx ? (InputDialogState*)ctx->userData : NULL;
}

static DWORD GetInputDialogId(DialogContext* ctx) {
    InputDialogState* state = GetInputDialogState(ctx);
    return state ? state->dialogId : 0;
}

static void FreeInputDialogState(DialogContext* ctx) {
    InputDialogState* state = GetInputDialogState(ctx);
    if (!state) {
        return;
    }

    ctx->userData = NULL;
    free(state);
}

static HWND CreateInputDialog(HWND hwndParent, int resourceId, DWORD dialogId,
                              int pomodoroTimeIndex) {
    InputDialogState* state = CreateInputDialogState(dialogId, pomodoroTimeIndex);
    if (!state) {
        return NULL;
    }

    HWND hwndDlg = CreateDialogParamW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(resourceId),
        hwndParent,
        DlgProc,
        (LPARAM)state
    );

    if (!hwndDlg) {
        free(state);
    }

    return hwndDlg;
}

/* ============================================================================
 * Modeless Dialog API
 * ============================================================================ */

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

    if (!IsValidInputDialogParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateInputDialog(hwndParent, CLOCK_IDD_DIALOG1,
                                    CLOCK_IDD_DIALOG1, -1);

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

/**
 * @brief Show shortcut time settings dialog (modeless)
 * @param hwndParent Parent window handle
 */
void ShowShortcutTimeDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_SHORTCUT)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_SHORTCUT);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidInputDialogParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateInputDialog(hwndParent, CLOCK_IDD_SHORTCUT_DIALOG,
                                    CLOCK_IDD_SHORTCUT_DIALOG, -1);

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

/**
 * @brief Show startup time settings dialog (modeless)
 * @param hwndParent Parent window handle
 */
void ShowStartupTimeDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_STARTUP)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_STARTUP);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidInputDialogParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateInputDialog(hwndParent, CLOCK_IDD_STARTUP_DIALOG,
                                    CLOCK_IDD_STARTUP_DIALOG, -1);

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

/**
 * @brief Show pomodoro time edit dialog (modeless)
 * @param hwndParent Parent window handle
 * @param timeIndex Index of pomodoro time to edit
 */
void ShowPomodoroTimeEditDialog(HWND hwndParent, int timeIndex) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_POMODORO_TIME)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_POMODORO_TIME);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidInputDialogParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateInputDialog(hwndParent, CLOCK_IDD_POMODORO_TIME_DIALOG,
                                    CLOCK_IDD_POMODORO_TIME_DIALOG, timeIndex);

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

/* ============================================================================
 * Helper: Get dialog instance type from dialog ID
 * ============================================================================ */

static DialogInstanceType GetInstanceTypeFromDialogId(DWORD dlgId) {
    switch (dlgId) {
        case CLOCK_IDD_SHORTCUT_DIALOG:
            return DIALOG_INSTANCE_SHORTCUT;
        case CLOCK_IDD_STARTUP_DIALOG:
            return DIALOG_INSTANCE_STARTUP;
        case CLOCK_IDD_POMODORO_TIME_DIALOG:
            return DIALOG_INSTANCE_POMODORO_TIME;
        default:
            return DIALOG_INSTANCE_INPUT;
    }
}

/* ============================================================================
 * Dialog Procedure
 * ============================================================================ */

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            InputDialogState* state = (InputDialogState*)lParam;
            if (!state) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            ctx = Dialog_CreateContext();
            if (!ctx) {
                free(state);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            
            ctx->userData = state;
            Dialog_SetContext(hwndDlg, ctx);

            DWORD dlgId = state->dialogId;
            DialogInstanceType instanceType = GetInstanceTypeFromDialogId(dlgId);
            Dialog_RegisterInstance(instanceType, hwndDlg);

            if (dlgId == CLOCK_IDD_DIALOG1) {
                g_hwndInputDialog = hwndDlg;
            }

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);
            if (hwndEdit) {
                SendMessageW(hwndEdit, EM_SETLIMITTEXT,
                             dlgId == CLOCK_IDD_SHORTCUT_DIALOG ?
                                 QUICK_TIME_OPTIONS_MAX_INPUT_CHARS :
                                 _countof(inputText) - 1,
                             0);
            }

            /* Populate edit with current values */
            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) {
                char currentOptions[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
                wchar_t wcurrentOptions[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
                if (BuildQuickTimeOptionsDisplay(currentOptions, sizeof(currentOptions)) &&
                    MultiByteToWideChar(CP_UTF8, 0, currentOptions, -1,
                                        wcurrentOptions,
                                        (int)_countof(wcurrentOptions)) > 0) {
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcurrentOptions);
                } else {
                    LOG_WARNING("InputDialog: Failed to build shortcut options display text");
                }
            } else if (dlgId == CLOCK_IDD_STARTUP_DIALOG) {
                if (g_AppConfig.timer.default_start_time > 0) {
                    char timeStr[64];
                    Dialog_FormatSecondsToString(g_AppConfig.timer.default_start_time, timeStr, sizeof(timeStr));

                    wchar_t wtimeStr[64] = {0};
                    if (MultiByteToWideChar(CP_UTF8, 0, timeStr, -1, wtimeStr, 64) > 0) {
                        SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                    }
                }
            } else if (dlgId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                int pomodoroIndex = state->pomodoroTimeIndex;
                int pomodoroTimesCount = g_AppConfig.pomodoro.times_count;
                if (pomodoroTimesCount < 0) pomodoroTimesCount = 0;
                if (pomodoroTimesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
                    pomodoroTimesCount = (int)_countof(g_AppConfig.pomodoro.times);
                }
                if (pomodoroIndex >= 0 && pomodoroIndex < pomodoroTimesCount &&
                    g_AppConfig.pomodoro.times[pomodoroIndex] > 0) {
                    char timeStr[64];
                    Dialog_FormatSecondsToString(g_AppConfig.pomodoro.times[pomodoroIndex],
                                                 timeStr, sizeof(timeStr));

                    wchar_t wtimeStr[64] = {0};
                    if (MultiByteToWideChar(CP_UTF8, 0, timeStr, -1, wtimeStr, 64) > 0) {
                        SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                    }
                }
            }

            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            SetFocus(hwndEdit);

            /* Aggressive focus handling for topmost dialogs */
            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);

            Dialog_SelectAllText(hwndEdit);
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);
            if (!SetTimer(hwndDlg, INPUT_FOCUS_TIMER_ID,
                          INPUT_FOCUS_TIMER_DELAY_MS, NULL)) {
                LOG_WARNING("InputDialog: Failed to start focus timer (error=%lu)",
                            GetLastError());
            }

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
            DestroyWindow(hwndDlg);
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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                const InputDialogState* state = GetInputDialogState(ctx);
                if (!state) {
                    return TRUE;
                }
                int dialogId = (int)state->dialogId;
                
                if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
                    wchar_t* shortcutInput = ReadDialogTextAlloc(
                        hwndDlg, CLOCK_IDC_EDIT, QUICK_TIME_OPTIONS_MAX_INPUT_CHARS);
                    if (!shortcutInput) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    if (Dialog_IsEmptyOrWhitespace(shortcutInput)) {
                        free(shortcutInput);
                        DestroyWindow(hwndDlg);
                        return TRUE;
                    }

                    char* inputCopy = ConvertDialogInputToUtf8Alloc(
                        shortcutInput, QUICK_TIME_OPTIONS_MAX_INPUT_BYTES);
                    free(shortcutInput);
                    if (!inputCopy) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    char options[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
                    int parsedSeconds[MAX_TIME_OPTIONS] = {0};
                    int count = 0;

                    BOOL parsed = BuildQuickTimeOptionsConfig(inputCopy, options,
                                                              sizeof(options),
                                                              parsedSeconds,
                                                              &count);
                    free(inputCopy);
                    if (!parsed) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    if (!WriteConfigTimeOptions(options)) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }

                    time_options_count = count;
                    memcpy(time_options, parsedSeconds, (size_t)count * sizeof(time_options[0]));

                    HWND hwndParent = GetInputDialogParent(hwndDlg);
                    if (hwndParent) {
                        PostMessage(hwndParent, WM_DIALOG_SHORTCUT, 0, 0);
                    }
                    DestroyWindow(hwndDlg);
                } else {
                    GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, inputText,
                                    sizeof(inputText) / sizeof(wchar_t));

                    if (Dialog_IsEmptyOrWhitespace(inputText)) {
                        DestroyWindow(hwndDlg);
                        return TRUE;
                    }

                    char inputUtf8[256] = {0};
                    if (!ConvertDialogInputToUtf8(inputText, inputUtf8, sizeof(inputUtf8))) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                    
                    int total_seconds;
                    if (ParseInput(inputUtf8, &total_seconds)) {
                        if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                            int pomodoroIndex = state->pomodoroTimeIndex;
                            int pomodoroTimesCount = g_AppConfig.pomodoro.times_count;
                            if (pomodoroTimesCount < 0) pomodoroTimesCount = 0;
                            if (pomodoroTimesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
                                pomodoroTimesCount = (int)_countof(g_AppConfig.pomodoro.times);
                            }
                            if (pomodoroIndex >= 0 && pomodoroIndex < pomodoroTimesCount) {
                                int updatedTimes[sizeof(g_AppConfig.pomodoro.times) /
                                                 sizeof(g_AppConfig.pomodoro.times[0])] = {0};
                                memcpy(updatedTimes, g_AppConfig.pomodoro.times,
                                       sizeof(int) * (size_t)pomodoroTimesCount);
                                updatedTimes[pomodoroIndex] = total_seconds;
                                if (!WriteConfigPomodoroTimeOptions(updatedTimes, pomodoroTimesCount)) {
                                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                                    return TRUE;
                                }
                            }
                            DestroyWindow(hwndDlg);
                        } else if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
                            extern BOOL WriteConfigPomodoroLoopCount(int loop_count);
                            if (!WriteConfigPomodoroLoopCount(total_seconds)) {
                                Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                                return TRUE;
                            }
                            DestroyWindow(hwndDlg);
                        } else if (dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                            if (!WriteConfigDefaultCountdownStartup(total_seconds)) {
                                Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                                return TRUE;
                            }
                            DestroyWindow(hwndDlg);
                        } else {
                            HWND hwndParent = GetInputDialogParent(hwndDlg);
                            if (hwndParent) {
                                PostMessage(hwndParent, WM_DIALOG_COUNTDOWN, (WPARAM)total_seconds, 0);
                            }
                            DestroyWindow(hwndDlg);
                        }
                    } else {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wParam == INPUT_FOCUS_TIMER_ID) {
                KillTimer(hwndDlg, INPUT_FOCUS_TIMER_ID);
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    SetForegroundWindow(hwndDlg);
                    SetFocus(hwndEdit);
                    Dialog_SelectAllText(hwndEdit);
                }
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                return TRUE;
            } else if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
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
                    Dialog_SelectAllText(hwndEdit);
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

        case WM_DESTROY: {
            KillTimer(hwndDlg, INPUT_FOCUS_TIMER_ID);

            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                
                DWORD dlgId = GetInputDialogId(ctx);
                DialogInstanceType instanceType = GetInstanceTypeFromDialogId(dlgId);
                Dialog_UnregisterInstanceForWindow(instanceType, hwndDlg);
                
                FreeInputDialogState(ctx);
                Dialog_DestroyContext(hwndDlg);
            }
            if (g_hwndInputDialog == hwndDlg) {
                g_hwndInputDialog = NULL;
            }
            break;
        }
    }
    return FALSE;
}
