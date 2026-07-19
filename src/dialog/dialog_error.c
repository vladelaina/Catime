/**
 * @file dialog_error.c
 * @brief Simple error dialog implementation (modeless)
 */

#include "dialog/dialog_error.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "../resource/resource.h"
#include <stdlib.h>

/* Global handle for error dialog */
static HWND g_hwndErrorDialog = NULL;

typedef struct {
    HWND parent;
    int editControlId;
} ErrorDialogState;

#define ERROR_BUTTON_ORIG_PROC_PROP L"Catime.ErrorDialog.OrigButtonProc"

static LRESULT CALLBACK ErrorButtonSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static WNDPROC GetErrorButtonOrigProc(HWND hwndButton) {
    return (WNDPROC)(LONG_PTR)GetPropW(hwndButton, ERROR_BUTTON_ORIG_PROC_PROP);
}

static BOOL SubclassErrorButton(HWND hwndButton) {
    if (!hwndButton) {
        return FALSE;
    }

    if (GetErrorButtonOrigProc(hwndButton)) {
        return TRUE;
    }

    SetLastError(0);
    WNDPROC origProc = (WNDPROC)SetWindowLongPtr(hwndButton, GWLP_WNDPROC,
                                                 (LONG_PTR)ErrorButtonSubclassProc);
    if (!origProc) {
        DWORD error = GetLastError();
        if (error != 0) {
            return FALSE;
        }
        return FALSE;
    }

    if (!SetPropW(hwndButton, ERROR_BUTTON_ORIG_PROC_PROP,
                  (HANDLE)(LONG_PTR)origProc)) {
        SetWindowLongPtr(hwndButton, GWLP_WNDPROC, (LONG_PTR)origProc);
        return FALSE;
    }

    return TRUE;
}

static void UnsubclassErrorButton(HWND hwndButton) {
    if (!hwndButton) {
        return;
    }

    WNDPROC origProc = GetErrorButtonOrigProc(hwndButton);
    if (!origProc) {
        return;
    }

    SetWindowLongPtr(hwndButton, GWLP_WNDPROC, (LONG_PTR)origProc);
    RemovePropW(hwndButton, ERROR_BUTTON_ORIG_PROC_PROP);
}

/* Subclass procedure for OK button to handle Enter key */
static LRESULT CALLBACK ErrorButtonSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
            HWND hwndDlg = GetParent(hwnd);
            if (hwndDlg) {
                DestroyWindow(hwndDlg);
            }
            return 0;
        }
    }

    WNDPROC origProc = GetErrorButtonOrigProc(hwnd);
    if (!origProc) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return CallWindowProc(origProc, hwnd, msg, wParam, lParam);
}

void ShowErrorDialog(HWND hwndParent) {
    /* Close existing error dialog if open */
    if (g_hwndErrorDialog && IsWindow(g_hwndErrorDialog)) {
        DestroyWindow(g_hwndErrorDialog);
        g_hwndErrorDialog = NULL;
    }
    
    ErrorDialogState* state = (ErrorDialogState*)calloc(1, sizeof(*state));
    if (!state) return;
    state->parent = hwndParent;

    g_hwndErrorDialog = CreateDialogParamW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_ERROR_DIALOG),
        hwndParent,
        ErrorDlgProc,
        (LPARAM)state
    );
    
    if (g_hwndErrorDialog) {
        ShowWindow(g_hwndErrorDialog, SW_SHOW);
    } else {
        free(state);
    }
}

void ShowErrorDialogWithRefocus(HWND hwndParent, int editControlId) {
    /* Close existing error dialog if open */
    if (g_hwndErrorDialog && IsWindow(g_hwndErrorDialog)) {
        DestroyWindow(g_hwndErrorDialog);
        g_hwndErrorDialog = NULL;
    }
    
    ErrorDialogState* state = (ErrorDialogState*)calloc(1, sizeof(*state));
    if (!state) return;
    state->parent = hwndParent;
    state->editControlId = editControlId;

    g_hwndErrorDialog = CreateDialogParamW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_ERROR_DIALOG),
        hwndParent,
        ErrorDlgProc,
        (LPARAM)state
    );
    
    if (g_hwndErrorDialog) {
        ShowWindow(g_hwndErrorDialog, SW_SHOW);
    } else {
        free(state);
    }
}

INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            ErrorDialogState* state = (ErrorDialogState*)lParam;
            if (!state) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);
            Dialog_InitializeInstance(DIALOG_INSTANCE_ERROR, hwndDlg);
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT,
                GetLocalizedString(NULL, 
                                 L"Invalid input format, please try again."));

            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Error"));
            
            /* Localize OK button */
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));

            {
                UINT dpi = DialogModern_GetDpi(hwndDlg);
                DialogModern_SetChildRect96(hwndDlg, IDC_ERROR_TEXT, dpi,
                                            0, 0, 360, 36);
                DialogModern_SetChildRect96(hwndDlg, IDOK, dpi,
                                            280, 60, 80, 36);
            }

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            /* Subclass OK button to handle Enter/Escape keys */
            {
                HWND hwndOK = GetDlgItem(hwndDlg, IDOK);
                if (hwndOK) {
                    SubclassErrorButton(hwndOK);
                    SetFocus(hwndOK);
                }
            }
            
            return FALSE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY: {
            ErrorDialogState* state = (ErrorDialogState*)GetWindowLongPtrW(
                hwndDlg, GWLP_USERDATA);
            /* Restore original button procedure before destruction */
            UnsubclassErrorButton(GetDlgItem(hwndDlg, IDOK));
            
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_ERROR, hwndDlg);
            g_hwndErrorDialog = NULL;
            
            /* Refocus to parent edit control if specified */
            if (state && state->parent && IsWindow(state->parent) &&
                state->editControlId > 0) {
                HWND hwndEdit = GetDlgItem(state->parent,
                                          state->editControlId);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
            }
            
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            free(state);
            break;
        }
    }
    return FALSE;
}
