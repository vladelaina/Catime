/**
 * @file dialog_modern_dialog_color.c
 * @brief Dialog control-color message handling.
 */

#include "dialog_modern_internal.h"

#define MODERN_RETURN_HANDLED(flag, value) \
    do { *(flag) = TRUE; return (value); } while (0)

LRESULT ModernHandleDialogColorMessage(
    UINT msg, WPARAM wParam, ModernDialogState* state, BOOL* handled) {
    *handled = FALSE;
    switch (msg) {
        case WM_CTLCOLORDLG:
            if (state && state->finalized) {
                SetBkColor((HDC)wParam, state->palette.surface);
                MODERN_RETURN_HANDLED(handled, (LRESULT)state->surfaceBrush);
            }
            break;
        case WM_CTLCOLORSTATIC:
            if (state && state->finalized) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->palette.text);
                MODERN_RETURN_HANDLED(handled, (LRESULT)state->surfaceBrush);
            }
            break;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (state && state->finalized) {
                SetBkColor((HDC)wParam, state->palette.field);
                SetTextColor((HDC)wParam, state->palette.text);
                MODERN_RETURN_HANDLED(handled, (LRESULT)state->fieldBrush);
            }
            break;
        case WM_CTLCOLORBTN:
            if (state && state->finalized) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->palette.text);
                MODERN_RETURN_HANDLED(handled, (LRESULT)state->surfaceBrush);
            }
            break;
    }
    return 0;
}

#undef MODERN_RETURN_HANDLED
