#include "dialog/dialog_input.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_input_internal.h"
#include "dialog_input_state.h"
#include "../resource/resource.h"
#include <stdlib.h>
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

BOOL DialogInput_IsValidParentWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;
    wchar_t className[64] = {0};
    return GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

HWND DialogInput_GetParent(HWND hwndDlg) {
    HWND parent = hwndDlg ? GetParent(hwndDlg) : NULL;
    return DialogInput_IsValidParentWindow(parent) ? parent : NULL;
}

static InputDialogState* CreateInputDialogState(DWORD dialogId,
                                                int pomodoroTimeIndex) {
    InputDialogState* state = (InputDialogState*)calloc(1, sizeof(*state));
    if (state) {
        state->dialogId = dialogId;
        state->pomodoroTimeIndex = pomodoroTimeIndex;
    }
    return state;
}

InputDialogState* DialogInput_GetState(DialogContext* ctx) {
    return ctx ? (InputDialogState*)ctx->userData : NULL;
}
DWORD DialogInput_GetDialogId(DialogContext* ctx) {
    InputDialogState* state = DialogInput_GetState(ctx);
    return state ? state->dialogId : 0;
}
void DialogInput_FreeState(DialogContext* ctx) {
    InputDialogState* state = DialogInput_GetState(ctx);
    if (!state) return;
    ctx->userData = NULL;
    free(state);
}

DialogInstanceType DialogInput_GetInstanceType(DWORD dialogId) {
    switch (dialogId) {
        case CLOCK_IDD_SHORTCUT_DIALOG: return DIALOG_INSTANCE_SHORTCUT;
        case CLOCK_IDD_STARTUP_DIALOG: return DIALOG_INSTANCE_STARTUP;
        case CLOCK_IDD_POMODORO_TIME_DIALOG: return DIALOG_INSTANCE_POMODORO_TIME;
        default: return DIALOG_INSTANCE_INPUT;
    }
}

HWND DialogInput_CreateResourceDialog(HWND parent, int resourceId,
                                      DWORD dialogId, int pomodoroTimeIndex) {
    InputDialogState* state = CreateInputDialogState(dialogId, pomodoroTimeIndex);
    if (!state) return NULL;
    HWND dialog = CreateDialogParamW(GetModuleHandle(NULL),
                                     MAKEINTRESOURCEW(resourceId), parent,
                                     DlgProc, (LPARAM)state);
    if (!dialog) free(state);
    return dialog;
}

static void ShowInputDialog(HWND parent, DialogInstanceType type,
                            int resourceId, DWORD dialogId, int index) {
    HWND existing = Dialog_GetInstance(type);
    if (existing) {
        SetForegroundWindow(existing);
        return;
    }
    if (!DialogInput_IsValidParentWindow(parent)) return;
    HWND dialog = DialogInput_CreateResourceDialog(parent, resourceId,
                                                    dialogId, index);
    if (dialog) ShowWindow(dialog, SW_SHOW);
}

void ShowShortcutTimeDialog(HWND parent) {
    ShowInputDialog(parent, DIALOG_INSTANCE_SHORTCUT,
                    CLOCK_IDD_SHORTCUT_DIALOG, CLOCK_IDD_SHORTCUT_DIALOG, -1);
}
void ShowStartupTimeDialog(HWND parent) {
    ShowInputDialog(parent, DIALOG_INSTANCE_STARTUP,
                    CLOCK_IDD_STARTUP_DIALOG, CLOCK_IDD_STARTUP_DIALOG, -1);
}
void ShowPomodoroTimeEditDialog(HWND parent, int timeIndex) {
    ShowInputDialog(parent, DIALOG_INSTANCE_POMODORO_TIME,
                    CLOCK_IDD_POMODORO_TIME_DIALOG,
                    CLOCK_IDD_POMODORO_TIME_DIALOG, timeIndex);
}
