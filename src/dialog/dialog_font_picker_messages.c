/**
 * @file dialog_font_picker_messages.c
 * @brief Message handlers for the system font picker dialog.
 */

#include "dialog_font_picker_internal.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_procedure.h"
#include "font.h"
#include "language.h"
#include "log.h"
#include "window/window_core.h"
#include "../../resource/resource.h"

INT_PTR DialogFontPickerInternal_OnInit(HWND hdlg) {
    Dialog_InitializeInstance(DIALOG_INSTANCE_FONT_PICKER, hdlg);
    if (!SetTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID, 500, NULL)) {
        LOG_WARNING("FontPicker: Failed to start topmost timer (error=%lu)",
                    GetLastError());
    }

    g_currentFontIndex = -1;
    g_previewFontIndex = -1;
    SetWindowTextW(hdlg, GetLocalizedString(NULL, L"Select Font"));
    SetDlgItemTextW(hdlg, IDOK, GetLocalizedString(NULL, L"OK"));
    SetDlgItemTextW(hdlg, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Font families (variants filtered):"));
    MoveDialogToPrimaryScreen(hdlg);
    DialogFontPickerInternal_SaveOriginalFont();

    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (!hwndList) {
        LOG_ERROR("FontPicker: Failed to get list control");
        return TRUE;
    }

    if (!DialogFontPickerInternal_CleanupCompletedEnumeration()) {
        DialogFontPickerInternal_StopEnumeration(FONT_ENUM_STOP_WAIT_MS);
    }
    if (!DialogFontPickerInternal_CleanupCompletedEnumeration()) {
        g_fontEnumRestartAfterCleanup = TRUE;
        SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                        GetLocalizedString(NULL,
                            L"Font families (variants filtered):"));
        DialogFontPickerInternal_StartPollTimer(hdlg);
        return TRUE;
    }

    g_fontEnumRestartAfterCleanup = FALSE;
    DialogFontPickerInternal_ResetFontMap();
    g_fontListReady = FALSE;
    EnableWindow(hwndList, FALSE);
    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Font families (variants filtered):"));
    DialogFontPickerInternal_StartEnumeration(hdlg);
    return TRUE;
}

INT_PTR DialogFontPickerInternal_OnEnumerationComplete(HWND hdlg, WPARAM wp) {
    if ((LONG)wp != InterlockedCompareExchange(&g_fontEnumGeneration, 0, 0) ||
        Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER) != hdlg) {
        return TRUE;
    }
    KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
    if (g_fontListReady) {
        return TRUE;
    }
    if (!DialogFontPickerInternal_CleanupCompletedEnumeration()) {
        g_fontEnumRestartAfterCleanup = FALSE;
        DialogFontPickerInternal_StartPollTimer(hdlg);
        return TRUE;
    }
    g_fontListReady = TRUE;
    DialogFontPickerInternal_PopulateFontList(hdlg);
    return TRUE;
}

static void RestartFontEnumerationIfNeeded(HWND hdlg) {
    if (!g_fontEnumRestartAfterCleanup) {
        if (!g_fontListReady) {
            g_fontListReady = TRUE;
            DialogFontPickerInternal_PopulateFontList(hdlg);
        }
        return;
    }

    g_fontEnumRestartAfterCleanup = FALSE;
    DialogFontPickerInternal_ResetFontMap();
    g_fontListReady = FALSE;
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (hwndList) {
        EnableWindow(hwndList, FALSE);
    }
    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
    DialogFontPickerInternal_StartEnumeration(hdlg);
}

INT_PTR DialogFontPickerInternal_OnTimer(HWND hdlg, WPARAM wp) {
    if (wp == FONT_PICKER_TOPMOST_TIMER_ID) {
        Dialog_ApplyTopmost(hdlg);
        return TRUE;
    }
    if (wp == FONT_ENUM_POLL_TIMER_ID) {
        if (DialogFontPickerInternal_CleanupCompletedEnumeration()) {
            KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
            RestartFontEnumerationIfNeeded(hdlg);
        }
        return TRUE;
    }
    if (wp == FONT_ENUM_START_RETRY_TIMER_ID) {
        if (!g_fontEnumThread) {
            DialogFontPickerInternal_StartEnumeration(hdlg);
        }
        return TRUE;
    }
    return FALSE;
}

static void HandleFontListSelection(HWND hdlg) {
    if (!g_fontListReady) {
        return;
    }

    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    int sel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        LOG_WARNING("FontPicker: LBN_SELCHANGE but GETCURSEL returned LB_ERR!");
        return;
    }

    LRESULT itemData = SendMessageW(hwndList, LB_GETITEMDATA,
                                    (WPARAM)sel, 0);
    if (sel == g_previewFontIndex) {
        return;
    }
    if (itemData >= 0 && itemData < g_fontMapCount) {
        int fontIndex = (int)itemData;
        if (DialogFontPickerInternal_PreviewFont(
                g_fontMap[fontIndex].fontName,
                g_fontMap[fontIndex].fontPath, hdlg, hwndList)) {
            g_previewFontIndex = sel;
        }
    } else {
        wchar_t fontName[LF_FACESIZE];
        ZeroMemory(fontName, sizeof(fontName));
        if (SendMessageW(hwndList, LB_GETTEXT, (WPARAM)sel,
                         (LPARAM)fontName) != LB_ERR &&
            DialogFontPickerInternal_PreviewFont(fontName, NULL,
                                                 hdlg, hwndList)) {
            g_previewFontIndex = sel;
        }
    }

    int newSel = (int)SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
    if (newSel != sel) {
        LOG_ERROR("FontPicker: ✗✗✗ SELECTION LOST! Was %d, now %d. Re-selecting.",
                  sel, newSel);
        SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)sel, 0);
        SetFocus(hwndList);
    }
}

static void CloseFontPickerWithCancel(HWND hdlg) {
    DialogFontPickerInternal_RestoreOriginalFont();
    g_fontState.closeHandled = TRUE;
    KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
    DestroyWindow(hdlg);
}

INT_PTR DialogFontPickerInternal_OnCommand(HWND hdlg, WPARAM wp) {
    if (LOWORD(wp) == IDOK) {
        HWND hwndMain = FindCurrentProcessMainWindow();
        if (!DialogFontPickerInternal_CommitSelection(hwndMain)) {
            DialogFontPickerInternal_RestoreOriginalFont();
            return TRUE;
        }
        g_fontState.closeHandled = TRUE;
        KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
        DestroyWindow(hdlg);
        return TRUE;
    }
    if (LOWORD(wp) == IDCANCEL) {
        CloseFontPickerWithCancel(hdlg);
        return TRUE;
    }
    if (LOWORD(wp) == IDC_FONT_LIST_SIMPLE) {
        if (HIWORD(wp) == LBN_SELCHANGE) {
            HandleFontListSelection(hdlg);
            return TRUE;
        }
        if (HIWORD(wp) == LBN_DBLCLK) {
            SendMessageW(hdlg, WM_COMMAND, IDOK, 0);
            return TRUE;
        }
    }
    return FALSE;
}

INT_PTR DialogFontPickerInternal_OnDestroy(HWND hdlg) {
    if (!g_fontState.closeHandled) {
        DialogFontPickerInternal_RestoreOriginalFont();
        g_fontState.closeHandled = TRUE;
    }
    KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
    KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
    KillTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID);
    Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_FONT_PICKER, hdlg);

    BOOL enumStopped = DialogFontPickerInternal_StopEnumeration(
        FONT_ENUM_STOP_WAIT_MS);
    if (!enumStopped) {
        LOG_WARNING("FontPicker: Leaving slow enumeration thread to finish asynchronously");
        DialogFontPickerInternal_ScheduleDeferredCleanup();
    }

    g_currentFontIndex = -1;
    g_previewFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontState.closeHandled = FALSE;
    if (enumStopped) {
        DialogFontPickerInternal_ResetFontMap();
    }
    return TRUE;
}
