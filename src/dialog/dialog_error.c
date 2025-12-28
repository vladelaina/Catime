/**
 * @file dialog_error.c
 * @brief Simple error dialog implementation (modeless)
 */

#include "dialog/dialog_error.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "../resource/resource.h"

/* Global handle for error dialog */
static HWND g_hwndErrorDialog = NULL;

/* Parent window and edit control to refocus after dialog closes */
static HWND g_hwndErrorParent = NULL;
static int g_errorEditControlId = 0;

/* Original button window procedure for subclassing */
static WNDPROC g_origButtonProc = NULL;

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
    return CallWindowProc(g_origButtonProc, hwnd, msg, wParam, lParam);
}

void ShowErrorDialog(HWND hwndParent) {
    /* Close existing error dialog if open */
    if (g_hwndErrorDialog && IsWindow(g_hwndErrorDialog)) {
        DestroyWindow(g_hwndErrorDialog);
        g_hwndErrorDialog = NULL;
    }
    
    g_hwndErrorParent = hwndParent;
    g_errorEditControlId = 0;
    
    /* Create modeless dialog */
    g_hwndErrorDialog = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_ERROR_DIALOG),
        hwndParent,
        ErrorDlgProc
    );
    
    if (g_hwndErrorDialog) {
        ShowWindow(g_hwndErrorDialog, SW_SHOW);
    }
}

void ShowErrorDialogWithRefocus(HWND hwndParent, int editControlId) {
    /* Close existing error dialog if open */
    if (g_hwndErrorDialog && IsWindow(g_hwndErrorDialog)) {
        DestroyWindow(g_hwndErrorDialog);
        g_hwndErrorDialog = NULL;
    }
    
    g_hwndErrorParent = hwndParent;
    g_errorEditControlId = editControlId;
    
    /* Create modeless dialog */
    g_hwndErrorDialog = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_ERROR_DIALOG),
        hwndParent,
        ErrorDlgProc
    );
    
    if (g_hwndErrorDialog) {
        ShowWindow(g_hwndErrorDialog, SW_SHOW);
    }
}

INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    
    switch (msg) {
        case WM_INITDIALOG:
            Dialog_RegisterInstance(DIALOG_INSTANCE_ERROR, hwndDlg);
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT,
                GetLocalizedString(NULL, 
                                 L"Invalid input format, please try again."));

            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Error"));
            
            /* Localize OK button */
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            
            Dialog_CenterOnPrimaryScreen(hwndDlg);
            
            /* Subclass OK button to handle Enter/Escape keys */
            {
                HWND hwndOK = GetDlgItem(hwndDlg, IDOK);
                if (hwndOK) {
                    g_origButtonProc = (WNDPROC)SetWindowLongPtr(hwndOK, GWLP_WNDPROC, 
                                                                  (LONG_PTR)ErrorButtonSubclassProc);
                    SetFocus(hwndOK);
                }
            }
            
            return FALSE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY:
            /* Restore original button procedure before destruction */
            {
                HWND hwndOK = GetDlgItem(hwndDlg, IDOK);
                if (hwndOK && g_origButtonProc) {
                    SetWindowLongPtr(hwndOK, GWLP_WNDPROC, (LONG_PTR)g_origButtonProc);
                    g_origButtonProc = NULL;
                }
            }
            
            Dialog_UnregisterInstance(DIALOG_INSTANCE_ERROR);
            g_hwndErrorDialog = NULL;
            
            /* Refocus to parent edit control if specified */
            if (g_hwndErrorParent && IsWindow(g_hwndErrorParent) && g_errorEditControlId > 0) {
                HWND hwndEdit = GetDlgItem(g_hwndErrorParent, g_errorEditControlId);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
            }
            
            g_hwndErrorParent = NULL;
            g_errorEditControlId = 0;
            break;
    }
    return FALSE;
}
