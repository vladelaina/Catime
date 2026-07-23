#include "update_ui_state.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_language.h"
#include "language.h"
#include "../../resource/resource.h"

#include <strsafe.h>

INT_PTR CALLBACK ExitMsgDlgProc(HWND dialog, UINT message,
                                WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            Dialog_InitializeInstance(DIALOG_INSTANCE_EXIT_MSG, dialog);
            g_hwndExitMsgDialog = dialog;
            UpdateUi_InitializeDialog(dialog, IDD_EXIT_DIALOG);
            SetDlgItemTextW(
                dialog, IDC_EXIT_TEXT,
                GetLocalizedString(NULL, L"The application will exit now"));
            SetDlgItemTextW(dialog, IDOK, GetLocalizedString(NULL, L"OK"));
            SetWindowTextW(
                dialog, GetLocalizedString(NULL, L"Catime - Update Notice"));
            return TRUE;
        case WM_KEYDOWN:
            if (wParam != VK_ESCAPE) break;
            DestroyWindow(dialog);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) != IDOK) break;
            DestroyWindow(dialog);
            return TRUE;
        case WM_CLOSE:
            DestroyWindow(dialog);
            return TRUE;
        case WM_DESTROY:
            Dialog_UnregisterInstanceForWindow(
                DIALOG_INSTANCE_EXIT_MSG, dialog);
            g_hwndExitMsgDialog = NULL;
            if (g_shouldExitAfterDialog) {
                g_shouldExitAfterDialog = FALSE;
                PostQuitMessage(0);
            }
            break;
        default:
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK UpdateErrorDlgProc(HWND dialog, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            Dialog_InitializeInstance(DIALOG_INSTANCE_UPDATE_ERROR, dialog);
            UpdateUi_InitializeDialog(dialog, IDD_UPDATE_ERROR_DIALOG);
            if (lParam) {
                SetDlgItemTextW(
                    dialog, IDC_UPDATE_ERROR_TEXT, (const wchar_t*)lParam);
            }
            return TRUE;
        case WM_KEYDOWN:
            if (wParam != VK_ESCAPE) break;
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) != IDOK) break;
            EndDialog(dialog, IDOK);
            return TRUE;
        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        case WM_DESTROY:
            Dialog_UnregisterInstanceForWindow(
                DIALOG_INSTANCE_UPDATE_ERROR, dialog);
            break;
        default:
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK NoUpdateDlgProc(HWND dialog, UINT message,
                                 WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_NO_UPDATE, dialog);
            g_hwndNoUpdateDialog = dialog;
            UpdateUi_InitializeDialog(dialog, IDD_NO_UPDATE_DIALOG);
            SetWindowTextW(
                dialog, GetLocalizedString(NULL, L"Catime - Update Notice"));
            if (g_noUpdateVersion[0]) {
                const wchar_t* baseText = GetDialogLocalizedString(
                    IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) {
                    baseText = L"You are already using the latest version!";
                }
                wchar_t text[256];
                StringCbPrintfW(
                    text, sizeof(text), L"%s\n%s %hs", baseText,
                    GetLocalizedString(NULL, L"Current version:"),
                    g_noUpdateVersion);
                SetDlgItemTextW(dialog, IDC_NO_UPDATE_TEXT, text);
            }
            return TRUE;
        }
        case WM_KEYDOWN:
            if (wParam != VK_ESCAPE) break;
            DestroyWindow(dialog);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) != IDOK) break;
            DestroyWindow(dialog);
            return TRUE;
        case WM_CLOSE:
            DestroyWindow(dialog);
            return TRUE;
        case WM_DESTROY:
            Dialog_UnregisterInstanceForWindow(
                DIALOG_INSTANCE_NO_UPDATE, dialog);
            g_hwndNoUpdateDialog = NULL;
            break;
        default:
            break;
    }
    return FALSE;
}
