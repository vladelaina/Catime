/**
 * @file dialog_modern_control_proc.c
 * @brief Dispatcher for modernized child-control messages.
 */

#include "dialog_modern_internal.h"

static BOOL ModernEditNeedsFullRedraw(UINT msg) {
    switch (msg) {
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
        case WM_UNDO:
        case WM_SETTEXT:
        case EM_REPLACESEL:
            return TRUE;
    }
    return FALSE;
}

static void ModernRedrawEditAfterInput(HWND hwnd, const ModernControl* control,
                                       UINT msg) {
    if (!control || !ModernIsNativeEdit(control) ||
        !ModernEditNeedsFullRedraw(msg) || !IsWindow(hwnd)) {
        return;
    }
    RedrawWindow(hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

LRESULT CALLBACK ModernControlSubclassProc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam,
                                           UINT_PTR subclassId,
                                           DWORD_PTR refData) {
    (void)subclassId;
    ModernControl* control = (ModernControl*)refData;
    ModernDialogState* state = control ? control->owner : NULL;
    BOOL handled = FALSE;
    LRESULT result = 0;

    switch (msg) {
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        case WM_TIMER:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            result = ModernHandleControlPointerMessage(
                hwnd, msg, wParam, lParam, control, state, &handled);
            break;

        case WM_ERASEBKGND:
        case WM_PAINT:
        case WM_PRINTCLIENT:
        case WM_SIZE:
        case WM_MOUSEWHEEL:
        case WM_PASTE:
            result = ModernHandleControlPaintMessage(
                hwnd, msg, wParam, lParam, control, state, &handled);
            break;

        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_SETCONTEXT:
        case WM_SETCURSOR:
        case WM_GETDLGCODE:
        case DTM_SETSYSTEMTIME:
        case WM_ENABLE:
        case WM_NCDESTROY:
            result = ModernHandleControlKeyboardMessage(
                hwnd, msg, wParam, lParam, control, state, &handled);
            break;
    }

    if (handled) {
        ModernRedrawEditAfterInput(hwnd, control, msg);
        return result;
    }
    result = DefSubclassProc(hwnd, msg, wParam, lParam);
    ModernRedrawEditAfterInput(hwnd, control, msg);
    return result;
}
