/**
 * @file dialog_error.c
 * @brief Simple error dialog implementation
 */

#include "dialog/dialog_error.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "log.h"
#include "../resource/resource.h"

void ShowErrorDialog(HWND hwndParent) {
    /* Don't show blocking error dialog - use audio feedback and logging instead
     * This provides much better UX by not interrupting user workflow with modal dialogs
     * User gets immediate audio feedback and can check logs if needed */
    (void)hwndParent;
    
    /* Audio feedback - non-blocking */
    MessageBeep(MB_ICONERROR);
    
    /* Log the error for troubleshooting */
    LOG_WARNING("Invalid input format detected - user input validation failed");
    
    /* Original modal dialog implementation commented out for reference:
    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(IDD_ERROR_DIALOG),
              hwndParent,
              ErrorDlgProc);
    */
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
            
            Dialog_CenterOnPrimaryScreen(hwndDlg);
            
            return TRUE;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstance(DIALOG_INSTANCE_ERROR);
            break;
    }
    return FALSE;
}

