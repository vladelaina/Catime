#include "dialog/dialog_common.h"
#include "../resource/resource.h"

#define DIALOG_EDIT_ORIG_PROC_PROP L"Catime.Dialog.OrigEditProc"

static int Dialog_GetDefaultCommandId(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return 0;
    LRESULT result = SendMessageW(hwndDlg, DM_GETDEFID, 0, 0);
    if (HIWORD(result) == DC_HASDEFID) {
        int controlId = LOWORD(result);
        HWND button = GetDlgItem(hwndDlg, controlId);
        if (button && IsWindowVisible(button) && IsWindowEnabled(button))
            return controlId;
    }
    const int fallbackIds[] = {CLOCK_IDC_BUTTON_OK, IDOK, IDYES};
    for (size_t i = 0; i < _countof(fallbackIds); i++) {
        HWND button = GetDlgItem(hwndDlg, fallbackIds[i]);
        if (button && IsWindowVisible(button) && IsWindowEnabled(button))
            return fallbackIds[i];
    }
    return 0;
}

static BOOL Dialog_InvokeDefaultCommand(HWND hwndControl) {
    HWND hwndDlg = hwndControl ? GetParent(hwndControl) : NULL;
    int controlId = Dialog_GetDefaultCommandId(hwndDlg);
    if (!controlId) return FALSE;
    HWND button = GetDlgItem(hwndDlg, controlId);
    SendMessageW(hwndDlg, WM_COMMAND, MAKEWPARAM(controlId, BN_CLICKED),
                 (LPARAM)button);
    return TRUE;
}

LRESULT APIENTRY Dialog_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam) {
    switch (msg) {
        case WM_SETFOCUS:
            PostMessage(hwnd, EM_SETSEL, 0, -1);
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN && Dialog_InvokeDefaultCommand(hwnd)) return 0;
            if (wParam == VK_ESCAPE) {
                SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
                return 0;
            }
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;
        case WM_CHAR:
            if (wParam == 1 || ((wParam == 'a' || wParam == 'A') &&
                                GetKeyState(VK_CONTROL) < 0)) return 0;
            if (wParam == VK_RETURN &&
                Dialog_GetDefaultCommandId(GetParent(hwnd)) != 0) return 0;
            if (wParam == VK_ESCAPE) return 0;
            break;
    }
    WNDPROC origProc = (WNDPROC)(LONG_PTR)GetPropW(hwnd,
                                                   DIALOG_EDIT_ORIG_PROC_PROP);
    return origProc ? CallWindowProc(origProc, hwnd, msg, wParam, lParam)
                    : DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Dialog_SubclassEdit(HWND hwndEdit, DialogContext* ctx) {
    if (!hwndEdit || !ctx) return FALSE;
    WNDPROC existing = (WNDPROC)(LONG_PTR)GetPropW(hwndEdit,
                                                   DIALOG_EDIT_ORIG_PROC_PROP);
    if (existing) {
        ctx->wpOrigEditProc = existing;
        return TRUE;
    }
    SetLastError(0);
    LONG_PTR previous = SetWindowLongPtr(hwndEdit, GWLP_WNDPROC,
                                         (LONG_PTR)Dialog_EditSubclassProc);
    if (!previous && GetLastError() != 0) return FALSE;
    WNDPROC origProc = (WNDPROC)previous;
    if (!SetPropW(hwndEdit, DIALOG_EDIT_ORIG_PROC_PROP,
                  (HANDLE)(LONG_PTR)origProc)) {
        SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)origProc);
        return FALSE;
    }
    ctx->wpOrigEditProc = origProc;
    return TRUE;
}

void Dialog_UnsubclassEdit(HWND hwndEdit, DialogContext* ctx) {
    if (!hwndEdit || !ctx) return;
    WNDPROC origProc = (WNDPROC)(LONG_PTR)GetPropW(hwndEdit,
                                                   DIALOG_EDIT_ORIG_PROC_PROP);
    if (!origProc) {
        WNDPROC current = (WNDPROC)(LONG_PTR)GetWindowLongPtr(hwndEdit,
                                                              GWLP_WNDPROC);
        if (current != Dialog_EditSubclassProc) return;
        origProc = ctx->wpOrigEditProc;
    }
    if (!origProc) return;
    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)origProc);
    RemovePropW(hwndEdit, DIALOG_EDIT_ORIG_PROC_PROP);
    if (ctx->wpOrigEditProc == origProc) ctx->wpOrigEditProc = NULL;
}

LRESULT Dialog_EditSubclassProc_Ex(HWND hwnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam,
                                   Dialog_EditCustomCallback callback,
                                   WNDPROC origProc) {
    if (!origProc) return 0;
    BOOL processed = FALSE;
    if (callback) {
        LRESULT result = callback(hwnd, msg, wParam, lParam, &processed);
        if (processed) return result;
    }
    switch (msg) {
        case WM_SETFOCUS:
            PostMessage(hwnd, EM_SETSEL, 0, -1);
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN && Dialog_InvokeDefaultCommand(hwnd)) return 0;
            if (wParam == VK_ESCAPE) {
                SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
                return 0;
            }
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessage(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;
        case WM_CHAR:
            if (wParam == 1 || ((wParam == 'a' || wParam == 'A') &&
                                GetKeyState(VK_CONTROL) < 0)) return 0;
            if (wParam == VK_RETURN &&
                Dialog_GetDefaultCommandId(GetParent(hwnd)) != 0) return 0;
            if (wParam == VK_ESCAPE) return 0;
            break;
    }
    return CallWindowProc(origProc, hwnd, msg, wParam, lParam);
}

void Dialog_SelectAllText(HWND hwndEdit) {
    if (hwndEdit && IsWindow(hwndEdit)) SendMessage(hwndEdit, EM_SETSEL, 0, -1);
}

void Dialog_InitEditWithValue(HWND hwndEdit, const wchar_t* initialValue) {
    if (!hwndEdit || !IsWindow(hwndEdit)) return;
    SetWindowTextW(hwndEdit, initialValue ? initialValue : L"");
    Dialog_SelectAllText(hwndEdit);
}

BOOL Dialog_HasFocusWithin(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return FALSE;
    HWND focused = GetFocus();
    return focused == hwndDlg || (focused && IsChild(hwndDlg, focused));
}

void Dialog_FocusControl(HWND hwndDlg, int controlId, BOOL selectAll) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return;
    HWND control = GetDlgItem(hwndDlg, controlId);
    if (!control || !IsWindow(control)) return;
    Dialog_ActivateWindow(hwndDlg);
    SetFocus(control);
    if (selectAll) Dialog_SelectAllText(control);
}
