/**
 * @file dialog_error.c
 * @brief Simple error dialog implementation
 */

#include "dialog/dialog_error.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "../resource/resource.h"

void ShowErrorDialog(HWND hwndParent) {
    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(IDD_ERROR_DIALOG),
              hwndParent,
              ErrorDlgProc);
}

INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT,
                GetLocalizedString(NULL, 
                                 L"Invalid input format, please try again."));

            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Error"));
            
            Dialog_CenterOnPrimaryScreen(hwndDlg);
            
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

