/**
 * @file dialog_input_internal.h
 * @brief Private bridge between the resource input and countdown modules.
 */

#ifndef DIALOG_INPUT_INTERNAL_H
#define DIALOG_INPUT_INTERNAL_H

#include "dialog/dialog_registry.h"
#include <windows.h>

BOOL DialogInput_IsValidParentWindow(HWND hwnd);
HWND DialogInput_GetParent(HWND hwndDlg);
HWND DialogInput_CreateResourceDialog(HWND hwndParent, int resourceId,
                                      DWORD dialogId, int pomodoroTimeIndex);
DialogInstanceType DialogInput_GetInstanceType(DWORD dialogId);
int DialogInput_GetInitialSeconds(DWORD dialogId, int pomodoroTimeIndex);
BOOL DialogInput_PersistParsedTime(HWND dialog, DWORD dialogId,
                                   int pomodoroTimeIndex, int seconds);

#endif /* DIALOG_INPUT_INTERNAL_H */
