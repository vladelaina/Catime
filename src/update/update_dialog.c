#include "update_notes_internal.h"
#include "update_ui_state.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "utils/string_convert.h"
#include "../../resource/resource.h"

#include <stdlib.h>
#include <strsafe.h>

#define UPDATE_NOTES_RELAYOUT_MESSAGE (WM_APP + 731)

static void SetVersionText(HWND dialog, const VersionInfo* versionInfo) {
    wchar_t* current = Utf8ToWideAlloc(versionInfo->currentVersion);
    wchar_t* latest = Utf8ToWideAlloc(versionInfo->latestVersion);
    if (current && latest) {
        wchar_t text[256];
        StringCbPrintfW(
            text, sizeof(text), L"%s %s\n%s %s",
            GetLocalizedString(NULL, L"Current version:"), current,
            GetLocalizedString(NULL, L"New version:"), latest);
        SetDlgItemTextW(dialog, IDC_UPDATE_TEXT, text);
    }
    free(current);
    free(latest);
}

static void InitializeUpdateDialog(HWND dialog) {
    Dialog_InitializeInstance(DIALOG_INSTANCE_UPDATE, dialog);
    g_hwndUpdateDialog = dialog;
    UpdateUi_InitializeDialog(dialog, IDD_UPDATE_DIALOG);

    const VersionInfo* versionInfo = &g_updateVersionInfo;
    if (!versionInfo->currentVersion) return;
    SetVersionText(dialog, versionInfo);
    UpdateNotes_Initialize(dialog, versionInfo->releaseNotes);
    SetDlgItemTextW(
        dialog, IDYES, GetLocalizedString(NULL, L"Update Now"));
    SetDlgItemTextW(dialog, IDNO, GetLocalizedString(NULL, L"Later"));
    SetWindowTextW(
        dialog, GetLocalizedString(NULL, L"Catime - Update Notice"));
    ShowWindow(GetDlgItem(dialog, IDYES), SW_SHOW);
    ShowWindow(GetDlgItem(dialog, IDNO), SW_SHOW);
    ShowWindow(GetDlgItem(dialog, IDOK), SW_HIDE);
    PostMessageW(dialog, UPDATE_NOTES_RELAYOUT_MESSAGE, 0, 0);
}

INT_PTR CALLBACK UpdateDlgProc(HWND dialog, UINT message,
                               WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            InitializeUpdateDialog(dialog);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                HWND parent = UpdateUi_GetDialogParent(dialog);
                int result = LOWORD(wParam);
                UpdateNotes_Cleanup(dialog);
                DestroyWindow(dialog);
                if (result == IDYES && parent) {
                    PostMessageW(parent, WM_DIALOG_UPDATE, IDYES, 0);
                }
                return TRUE;
            }
            break;
        case WM_SIZE:
            if (g_notesDisplayText) {
                PostMessageW(dialog, UPDATE_NOTES_RELAYOUT_MESSAGE, 0, 0);
            }
            break;
        case UPDATE_NOTES_RELAYOUT_MESSAGE:
            UpdateNotes_Recalculate(dialog);
            return TRUE;
        case WM_DRAWITEM:
            if (UpdateNotes_Paint(
                    dialog, (const DRAWITEMSTRUCT*)lParam)) {
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (wParam != VK_ESCAPE) break;
            UpdateNotes_Cleanup(dialog);
            DestroyWindow(dialog);
            return TRUE;
        case WM_CLOSE:
            UpdateNotes_Cleanup(dialog);
            DestroyWindow(dialog);
            return TRUE;
        case WM_DESTROY:
            UpdateNotes_Cleanup(dialog);
            Dialog_UnregisterInstanceForWindow(
                DIALOG_INSTANCE_UPDATE, dialog);
            g_hwndUpdateDialog = NULL;
            break;
        default:
            break;
    }
    return FALSE;
}
