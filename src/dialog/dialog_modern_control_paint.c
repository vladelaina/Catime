/**
 * @file dialog_modern_control_paint.c
 * @brief Painting, sizing, scrolling, and paste handling for controls.
 */

#include "dialog_modern_internal.h"

#define MODERN_RETURN_HANDLED(flag, value) \
    do { *(flag) = TRUE; return (value); } while (0)

LRESULT ModernHandleControlPaintMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    ModernControl* control, ModernDialogState* state, BOOL* handled) {
    *handled = FALSE;
    switch (msg) {
        case WM_ERASEBKGND:
            if (control && (control->kind == MODERN_CONTROL_SLIDER ||
                            ModernIsDateTimeControl(control))) {
                MODERN_RETURN_HANDLED(handled, 1);
            }
            break;
        case WM_PAINT:
            if (control && (control->kind == MODERN_CONTROL_CHECK ||
                            control->kind == MODERN_CONTROL_RADIO ||
                            control->kind == MODERN_CONTROL_GROUP)) {
                ModernPaintChoiceControl(control, NULL);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && state && ModernIsDateTimeControl(control)) {
                ModernPaintDateTime(control, NULL);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && control->kind == MODERN_CONTROL_COMBO) {
                ModernPaintCombo(control, NULL);
                ModernDrawFieldOutline(control);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER) {
                ModernPaintSlider(control, NULL);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && (control->kind == MODERN_CONTROL_FIELD ||
                            control->kind == MODERN_CONTROL_LIST ||
                            control->kind == MODERN_CONTROL_COMBO)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernDrawFieldOutline(control);
                MODERN_RETURN_HANDLED(handled, result);
            }
            break;
        case WM_PRINTCLIENT:
            if (control && (control->kind == MODERN_CONTROL_CHECK ||
                            control->kind == MODERN_CONTROL_RADIO ||
                            control->kind == MODERN_CONTROL_GROUP)) {
                ModernPaintChoiceControl(control, (HDC)wParam);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && state && ModernIsDateTimeControl(control)) {
                ModernPaintDateTime(control, (HDC)wParam);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && control->kind == MODERN_CONTROL_COMBO) {
                ModernPaintCombo(control, (HDC)wParam);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER) {
                ModernPaintSlider(control, (HDC)wParam);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
        case WM_SIZE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            ModernApplyFieldRegion(control);
            ModernApplyEditLayout(control);
            ModernHideDateTimeSpinner(control);
            MODERN_RETURN_HANDLED(handled, result);
        }
        case WM_MOUSEWHEEL:
            if (state && ModernHandleInteractiveWheel(state, wParam, lParam)) {
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (state && state->bodyScrollMax96 > 0 &&
                !ModernControlOwnsVerticalScroll(control)) {
                ModernHandleBodyWheel(state, wParam);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
        case WM_PASTE:
            if (control && ModernIsCompactEdit(control) &&
                ModernPasteCompactEdit(hwnd)) {
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
    }
    return 0;
}

#undef MODERN_RETURN_HANDLED
