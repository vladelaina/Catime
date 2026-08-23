/**
 * @file dialog_input.c
 * @brief Generic input dialog and time parsing implementation (modeless version)
 */

#include "dialog/dialog_input.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_error.h"
#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_input_internal.h"
#include "dialog/dialog_input_options.h"
#include "dialog_input_state.h"
#include "language.h"
#include "timer/timer.h"
#include "config.h"
#include "dialog/dialog_language.h"
#include "utils/time_parser.h"
#include "utils/string_convert.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>

/* ============================================================================
 * Global State (declared in main.c and header files)
 * ============================================================================ */

/* Note: inputText is defined in main.c */
extern wchar_t inputText[256];

/* g_hwndInputDialog is defined here for dialog management */
HWND g_hwndInputDialog = NULL;


INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG:
            return DialogInput_HandleInit(hwndDlg, lParam);
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
            if (DialogInput_HandleCommand(hwndDlg, ctx, wParam)) return TRUE;
            break;
        case WM_TIMER:
            if (wParam == INPUT_FOCUS_TIMER_ID) {
                KillTimer(hwndDlg, INPUT_FOCUS_TIMER_ID);
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && IsWindow(hwndEdit) &&
                    !Dialog_HasFocusWithin(hwndDlg)) {
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

        case WM_DESTROY: {
            KillTimer(hwndDlg, INPUT_FOCUS_TIMER_ID);

            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }

                DWORD dlgId = DialogInput_GetDialogId(ctx);
                DialogInstanceType instanceType = DialogInput_GetInstanceType(dlgId);
                Dialog_UnregisterInstanceForWindow(instanceType, hwndDlg);

                DialogInput_FreeState(ctx);
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
