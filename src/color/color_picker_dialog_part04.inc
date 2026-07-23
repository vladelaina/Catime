            PickerLayoutControls(hwndDlg);

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

            PickerSetupCanvas(hwndDlg, IDC_MODERN_COLOR_SV, state);
            PickerSetupCanvas(hwndDlg, IDC_MODERN_COLOR_HUE, state);
            PickerSetupCanvas(hwndDlg, IDC_MODERN_COLOR_SAVED, state);
            PickerColorToHsv(state->selectedColor, 0.0, &state->hue,
                             &state->saturation, &state->value);
            PickerUpdateFields(state);
            SetFocus(GetDlgItem(hwndDlg, IDC_MODERN_COLOR_SV));
            return FALSE;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
            if (!state || !item) break;
            if (item->CtlID == IDC_MODERN_COLOR_SV) {
                PickerPaintSv(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_HUE) {
                PickerPaintHue(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_SAVED) {
                PickerPaintSavedColors(state, item);
                return TRUE;
            }
            if (item->CtlID == IDC_MODERN_COLOR_PREVIEW) {
                PickerPaintPreview(state, item);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            int notification = HIWORD(wParam);
            if (controlId == IDOK) {
                PickerEndEyedropper(state, TRUE);
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            if (controlId == IDCANCEL) {
                if (state && state->eyedropperActive) {
                    PickerEndEyedropper(state, FALSE);
                    return TRUE;
                }
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            if (controlId == IDC_MODERN_COLOR_PICK_BUTTON &&
                notification == BN_CLICKED) {
                PickerBeginEyedropper(state);
                return TRUE;
            }
            if (controlId == IDC_MODERN_COLOR_SAVE_BUTTON &&
                notification == BN_CLICKED) {
                PickerSaveCurrentColor(state);
                return TRUE;
            }
            if (notification == EN_CHANGE &&
                (controlId == IDC_MODERN_COLOR_HEX_EDIT ||
                 controlId == IDC_MODERN_COLOR_RED_EDIT ||
                 controlId == IDC_MODERN_COLOR_GREEN_EDIT ||
                 controlId == IDC_MODERN_COLOR_BLUE_EDIT)) {
                PickerHandleFieldChange(state, controlId);
                return TRUE;
            }
            if (notification == EN_KILLFOCUS &&
                (controlId == IDC_MODERN_COLOR_HEX_EDIT ||
                 controlId == IDC_MODERN_COLOR_RED_EDIT ||
                 controlId == IDC_MODERN_COLOR_GREEN_EDIT ||
                 controlId == IDC_MODERN_COLOR_BLUE_EDIT)) {
                PickerUpdateFields(state);
                return TRUE;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (state && state->eyedropperActive) {
                COLORREF sampled = 0;
                if (PickerSampleScreenColor(&sampled)) {
                    PickerSetColor(state, sampled, TRUE, TRUE);
                }
                return TRUE;
            }
            break;
        case WM_LBUTTONUP:
            if (state && state->eyedropperActive) {
                COLORREF sampled = 0;
                if (PickerSampleScreenColor(&sampled)) {
                    PickerSetColor(state, sampled, TRUE, TRUE);
                }
                PickerEndEyedropper(state, TRUE);
                return TRUE;
            }
            break;
        case WM_RBUTTONUP:
            if (state && state->eyedropperActive) {
                PickerEndEyedropper(state, FALSE);
                return TRUE;
            }
            break;
        case WM_CANCELMODE:
            if (state && state->eyedropperActive) {
                PickerEndEyedropper(state, FALSE);
                return TRUE;
            }
            break;
        case WM_CAPTURECHANGED:
            if (state && state->eyedropperActive &&
                (HWND)lParam != hwndDlg) {
                PickerEndEyedropper(state, FALSE);
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
                    PickerEndEyedropper(state, FALSE);
                } else {
                    EndDialog(hwndDlg, IDCANCEL);
                }
                return TRUE;
            }
            break;
        case WM_DPICHANGED:
            PickerLayoutControls(hwndDlg);
            break;
        case WM_CLOSE:
            if (state) PickerEndEyedropper(state, FALSE);
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
        SetForegroundWindow(existing);
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
