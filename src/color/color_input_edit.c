#include "color/color_input_edit.h"
#include "color/color_input_dialog.h"
#include "dialog/dialog_common.h"

#define COLOR_EDIT_ORIG_PROC_PROP L"Catime.ColorInput.OrigEditProc"

static WNDPROC GetOriginalProc(HWND hwndEdit) {
    return (WNDPROC)(LONG_PTR)GetPropW(hwndEdit, COLOR_EDIT_ORIG_PROC_PROP);
}

static LRESULT OnEditMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                             BOOL* processed) {
    (void)wParam;
    (void)lParam;
    *processed = FALSE;
    if (msg == WM_CHAR || msg == WM_PASTE || msg == WM_CUT) {
        PostMessageW(GetParent(hwnd), COLOR_INPUT_EDIT_CHANGED, 0, 0);
    }
    return 0;
}

LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam) {
    WNDPROC original = GetOriginalProc(hwnd);
    if (!original) return DefWindowProcW(hwnd, msg, wParam, lParam);
    return Dialog_EditSubclassProc_Ex(hwnd, msg, wParam, lParam,
                                      OnEditMessage, original);
}

BOOL ColorInputEdit_Attach(HWND hwndEdit) {
    if (!hwndEdit) return FALSE;
    if (GetOriginalProc(hwndEdit)) return TRUE;

    SetLastError(0);
    WNDPROC original = (WNDPROC)SetWindowLongPtrW(
        hwndEdit, GWLP_WNDPROC, (LONG_PTR)ColorEditSubclassProc);
    if (!original && GetLastError() != 0) return FALSE;

    if (!SetPropW(hwndEdit, COLOR_EDIT_ORIG_PROC_PROP,
                  (HANDLE)(LONG_PTR)original)) {
        SetWindowLongPtrW(hwndEdit, GWLP_WNDPROC, (LONG_PTR)original);
        return FALSE;
    }
    return TRUE;
}

void ColorInputEdit_Detach(HWND hwndEdit) {
    if (!hwndEdit) return;
    WNDPROC original = GetOriginalProc(hwndEdit);
    if (!original) return;
    SetWindowLongPtrW(hwndEdit, GWLP_WNDPROC, (LONG_PTR)original);
    RemovePropW(hwndEdit, COLOR_EDIT_ORIG_PROC_PROP);
}
