/**
 * @file dialog_font_picker.c
 * @brief System font picker entry point and message dispatch.
 */

#include "dialog_font_picker_internal.h"
#include "dialog/dialog_common.h"
#include "../../resource/resource.h"

static INT_PTR CALLBACK SimpleFontPickerProc(HWND hdlg, UINT msg,
                                             WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MEASUREITEM:
            return DialogFontPickerInternal_OnMeasureItem(hdlg, lp);

        case WM_DRAWITEM:
            return DialogFontPickerInternal_OnDrawItem(hdlg, lp);

        case WM_INITDIALOG:
            return DialogFontPickerInternal_OnInit(hdlg);

        case WM_APP_FONT_ENUM_COMPLETE:
            return DialogFontPickerInternal_OnEnumerationComplete(hdlg, wp);

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
                return TRUE;
            }
            break;

        case WM_TIMER:
            return DialogFontPickerInternal_OnTimer(hdlg, wp);

        case WM_COMMAND:
            return DialogFontPickerInternal_OnCommand(hdlg, wp);

        case WM_CLOSE:
            SendMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
            return TRUE;

        case WM_DESTROY:
            return DialogFontPickerInternal_OnDestroy(hdlg);
    }
    return FALSE;
}

BOOL ShowSystemFontDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER);
        SetForegroundWindow(existing);
        return TRUE;
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_FONT_PICKER_SIMPLE),
        hwndParent, SimpleFontPickerProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}
