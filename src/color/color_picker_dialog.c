/**
 * @file color_picker_dialog.c
 * @brief Owns the modern color picker dialog procedure and public entry point.
 */

#include "color/color_picker_internal.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "../resource/resource.h"

#include <stdlib.h>
#include <string.h>

static INT_PTR CALLBACK ModernColorPickerDlgProc(HWND hwndDlg, UINT msg,
                                                 WPARAM wParam, LPARAM lParam) {
    ModernColorPickerState* state =
        (ModernColorPickerState*)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);
    switch (msg) {
        case WM_INITDIALOG: {
            state = (ModernColorPickerState*)lParam;
            if (!state) return FALSE;
            state->hwnd = hwndDlg;
            state->svCachedHue = -1;
            state->savedFocusIndex =
                state->customColorCount > 0 ? 0 : -1;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);
            Dialog_InitializeInstance(DIALOG_INSTANCE_COLOR_PICKER, hwndDlg);
            ColorPickerInternal_LayoutControls(hwndDlg);

            SetWindowTextW(hwndDlg,
                           GetLocalizedString(NULL, L"Set Color Value"));
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL,
                            GetLocalizedString(NULL, L"Cancel"));
            SetDlgItemTextW(hwndDlg, IDC_MODERN_COLOR_PICK_BUTTON,
                            GetLocalizedString(NULL, L"Pick"));
            SetDlgItemTextW(hwndDlg, IDC_MODERN_COLOR_SAVE_BUTTON,
                            GetLocalizedString(NULL, L"Save"));

            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_HEX_EDIT,
                                EM_SETLIMITTEXT, 7, 0);
            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_RED_EDIT,
                                EM_SETLIMITTEXT, 3, 0);
            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_GREEN_EDIT,
                                EM_SETLIMITTEXT, 3, 0);
            SendDlgItemMessageW(hwndDlg, IDC_MODERN_COLOR_BLUE_EDIT,
                                EM_SETLIMITTEXT, 3, 0);

            ColorPickerInternal_SetupCanvas(hwndDlg, IDC_MODERN_COLOR_SV, state);
            ColorPickerInternal_SetupCanvas(hwndDlg, IDC_MODERN_COLOR_HUE, state);
            ColorPickerInternal_SetupCanvas(hwndDlg, IDC_MODERN_COLOR_SAVED, state);
            ColorPickerInternal_ColorToHsv(state->selectedColor, 0.0, &state->hue,
                             &state->saturation, &state->value);
            ColorPickerInternal_UpdateFields(state);
            SetFocus(GetDlgItem(hwndDlg, IDC_MODERN_COLOR_SV));
            DialogModern_PrepareForShow(hwndDlg);
            return FALSE;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
            if (!state || !item) break;
            if (item->CtlID == IDC_MODERN_COLOR_SV) {
                ColorPickerInternal_PaintSv(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_HUE) {
                ColorPickerInternal_PaintHue(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_SAVED) {
                ColorPickerInternal_PaintSavedColors(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_PREVIEW) {
                ColorPickerInternal_PaintPreview(state, item);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            int notification = HIWORD(wParam);
            if (controlId == IDOK) {
                ColorPickerInternal_EndEyedropper(state, TRUE);
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            if (controlId == IDCANCEL) {
                if (state && state->eyedropperActive) {
                    ColorPickerInternal_EndEyedropper(state, FALSE);
                    return TRUE;
                }
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            if (controlId == IDC_MODERN_COLOR_PICK_BUTTON &&
                notification == BN_CLICKED) {
                ColorPickerInternal_BeginEyedropper(state);
                return TRUE;
            }
            if (controlId == IDC_MODERN_COLOR_SAVE_BUTTON &&
                notification == BN_CLICKED) {
                ColorPickerInternal_SaveCurrentColor(state);
                return TRUE;
            }
            if (notification == EN_CHANGE &&
                (controlId == IDC_MODERN_COLOR_HEX_EDIT ||
                 controlId == IDC_MODERN_COLOR_RED_EDIT ||
                 controlId == IDC_MODERN_COLOR_GREEN_EDIT ||
                 controlId == IDC_MODERN_COLOR_BLUE_EDIT)) {
                ColorPickerInternal_HandleFieldChange(state, controlId);
                return TRUE;
            }
            if (notification == EN_KILLFOCUS &&
                (controlId == IDC_MODERN_COLOR_HEX_EDIT ||
                 controlId == IDC_MODERN_COLOR_RED_EDIT ||
                 controlId == IDC_MODERN_COLOR_GREEN_EDIT ||
                 controlId == IDC_MODERN_COLOR_BLUE_EDIT)) {
                ColorPickerInternal_UpdateFields(state);
                return TRUE;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (state && state->eyedropperActive) {
                COLORREF sampled = 0;
                if (ColorPickerInternal_SampleScreenColor(&sampled)) {
                    ColorPickerInternal_SetColor(state, sampled, TRUE, TRUE);
                }
                return TRUE;
            }
            break;
        case WM_LBUTTONUP:
            if (state && state->eyedropperActive) {
                COLORREF sampled = 0;
                if (ColorPickerInternal_SampleScreenColor(&sampled)) {
                    ColorPickerInternal_SetColor(state, sampled, TRUE, TRUE);
                }
                ColorPickerInternal_EndEyedropper(state, TRUE);
                return TRUE;
            }
            break;
        case WM_RBUTTONUP:
            if (state && state->eyedropperActive) {
                ColorPickerInternal_EndEyedropper(state, FALSE);
                return TRUE;
            }
            break;
        case WM_CANCELMODE:
            if (state && state->eyedropperActive) {
                ColorPickerInternal_EndEyedropper(state, FALSE);
                return TRUE;
            }
            break;
        case WM_CAPTURECHANGED:
            if (state && state->eyedropperActive &&
                (HWND)lParam != hwndDlg) {
                ColorPickerInternal_EndEyedropper(state, FALSE);
            }
            break;
        case WM_SETCURSOR:
            if (state && state->eyedropperActive) {
                SetCursor(LoadCursorW(NULL, IDC_CROSS));
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (state && state->eyedropperActive) {
                    ColorPickerInternal_EndEyedropper(state, FALSE);
                } else {
                    EndDialog(hwndDlg, IDCANCEL);
                }
                return TRUE;
            }
            break;
        case WM_DPICHANGED:
            ColorPickerInternal_LayoutControls(hwndDlg);
            break;
        case WM_CLOSE:
            if (state) ColorPickerInternal_EndEyedropper(state, FALSE);
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        case WM_DESTROY:
            if (state) {
                if (GetCapture() == hwndDlg) ReleaseCapture();
                free(state->svPixels);
                free(state->huePixels);
                state->svPixels = NULL;
                state->huePixels = NULL;
            }
            Dialog_UnregisterInstanceForWindow(
                DIALOG_INSTANCE_COLOR_PICKER, hwndDlg);
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            break;
    }
    return FALSE;
}

BOOL ModernColorPicker_Show(HWND hwndParent,
                            COLORREF initialColor,
                            COLORREF* customColors,
                            size_t customColorCapacity,
                            size_t* customColorCount,
                            COLORREF* selectedColor) {
    if (!hwndParent || !IsWindow(hwndParent) || !customColors ||
        customColorCapacity == 0 || !customColorCount || !selectedColor) {
        return FALSE;
    }
    if (Dialog_IsOpen(DIALOG_INSTANCE_COLOR_PICKER)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_COLOR_PICKER);
        Dialog_ActivateWindow(existing);
        return FALSE;
    }

    COLORREF* workingColors =
        (COLORREF*)calloc(customColorCapacity, sizeof(*workingColors));
    if (!workingColors) return FALSE;
    size_t count = *customColorCount;
    if (count > customColorCapacity) count = customColorCapacity;
    memcpy(workingColors, customColors, count * sizeof(*workingColors));

    ModernColorPickerState state = {0};
    state.hwndParent = hwndParent;
    state.selectedColor = initialColor;
    state.customColors = workingColors;
    state.customColorCapacity = customColorCapacity;
    state.customColorCount = count;

    INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(NULL),
        MAKEINTRESOURCEW(IDD_MODERN_COLOR_PICKER_DIALOG),
        hwndParent, ModernColorPickerDlgProc, (LPARAM)&state);
    BOOL accepted = result == IDOK;
    if (accepted) {
        memcpy(customColors, workingColors,
               state.customColorCount * sizeof(*customColors));
        *customColorCount = state.customColorCount;
        *selectedColor = state.selectedColor;
    }
    free(workingColors);
    return accepted;
}
