#include "dialog_input_state.h"
#include "dialog/dialog_input.h"
#include "dialog/dialog_input_internal.h"
#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_input_options.h"
#include "dialog/dialog_language.h"
#include "config.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

int DialogInput_GetInitialSeconds(DWORD dialogId, int pomodoroTimeIndex) {
    if (dialogId == CLOCK_IDD_STARTUP_DIALOG) {
        return g_AppConfig.timer.default_start_time;
    }
    if (dialogId != CLOCK_IDD_POMODORO_TIME_DIALOG) {
        return 0;
    }

    int count = g_AppConfig.pomodoro.times_count;
    if (count < 0) count = 0;
    if (count > (int)_countof(g_AppConfig.pomodoro.times)) {
        count = (int)_countof(g_AppConfig.pomodoro.times);
    }
    if (pomodoroTimeIndex < 0 || pomodoroTimeIndex >= count) {
        return 0;
    }
    return g_AppConfig.pomodoro.times[pomodoroTimeIndex];
}

static void PopulateInitialInput(HWND dialog, const InputDialogState* state) {
    HWND edit = GetDlgItem(dialog, CLOCK_IDC_EDIT);
    if (!edit || !state) return;
    DWORD dialogId = state->dialogId;
    if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
        char options[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
        wchar_t wideOptions[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
        if (DialogInputOptions_BuildDisplay(options, sizeof(options)) &&
            MultiByteToWideChar(CP_UTF8, 0, options, -1, wideOptions,
                                _countof(wideOptions)) > 0)
            SetWindowTextW(edit, wideOptions);
        return;
    }
    int seconds = DialogInput_GetInitialSeconds(
        dialogId, state->pomodoroTimeIndex);
    if (seconds > 0) {
        char value[64];
        wchar_t wideValue[64] = {0};
        Dialog_FormatSecondsToString(seconds, value, sizeof(value));
        if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wideValue,
                                _countof(wideValue)) > 0)
            SetWindowTextW(edit, wideValue);
    }
}

static void ShowBuildDate(HWND dialog) {
    char month[4];
    int day = 0, year = 0, hour = 0, minute = 0, second = 0;
    sscanf(__DATE__, "%3s %d %d", month, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"};
    int monthNumber = 0;
    while (++monthNumber <= 12 && strcmp(month, months[monthNumber - 1])) {}
    wchar_t text[60];
    StringCbPrintfW(text, sizeof(text),
                    L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                    year, monthNumber, day, hour, minute, second);
    SetDlgItemTextW(dialog, IDC_BUILD_DATE, text);
}

INT_PTR DialogInput_HandleInit(HWND hwndDlg, LPARAM parameter) {
    InputDialogState* state = (InputDialogState*)parameter;
    if (!state) {
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    DialogContext* context = Dialog_CreateContext();
    if (!context) {
        free(state);
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    context->userData = state;
    Dialog_SetContext(hwndDlg, context);
    Dialog_InitializeInstance(DialogInput_GetInstanceType(state->dialogId), hwndDlg);
    if (state->dialogId == CLOCK_IDD_DIALOG1) g_hwndInputDialog = hwndDlg;
    Dialog_CenterOnPrimaryScreen(hwndDlg);
    HWND edit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
    Dialog_SubclassEdit(edit, context);
    if (edit) {
        SendMessageW(edit, EM_SETLIMITTEXT,
                     state->dialogId == CLOCK_IDD_SHORTCUT_DIALOG
                         ? QUICK_TIME_OPTIONS_MAX_INPUT_CHARS : 255, 0);
    }
    PopulateInitialInput(hwndDlg, state);
    ApplyDialogLanguage(hwndDlg, (int)state->dialogId);
    DialogFormLayout_ApplyInstruction(hwndDlg, CLOCK_IDC_STATIC,
                                      CLOCK_IDC_EDIT, CLOCK_IDC_BUTTON_OK);
    SetFocus(edit);
    PostMessage(hwndDlg, WM_APP + 100, 0, (LPARAM)edit);
    PostMessage(hwndDlg, WM_APP + 101, 0, (LPARAM)edit);
    PostMessage(hwndDlg, WM_APP + 102, 0, (LPARAM)edit);
    Dialog_SelectAllText(edit);
    SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);
    if (!SetTimer(hwndDlg, INPUT_FOCUS_TIMER_ID, INPUT_FOCUS_TIMER_DELAY_MS, NULL))
        LOG_WARNING("InputDialog: Failed to start focus timer (error=%lu)",
                    GetLastError());
    ShowBuildDate(hwndDlg);
    return FALSE;
}
