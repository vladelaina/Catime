#ifndef DIALOG_INPUT_STATE_H
#define DIALOG_INPUT_STATE_H

#include "dialog/dialog_common.h"

#define INPUT_FOCUS_TIMER_ID 9999
#define INPUT_FOCUS_TIMER_DELAY_MS 50

typedef struct {
    DWORD dialogId;
    int pomodoroTimeIndex;
} InputDialogState;

InputDialogState* DialogInput_GetState(DialogContext* ctx);
DWORD DialogInput_GetDialogId(DialogContext* ctx);
void DialogInput_FreeState(DialogContext* ctx);
DialogInstanceType DialogInput_GetInstanceType(DWORD dialogId);
INT_PTR DialogInput_HandleInit(HWND hwndDlg, LPARAM parameter);
BOOL DialogInput_HandleCommand(HWND hwndDlg, DialogContext* ctx, WPARAM wParam);

#endif
