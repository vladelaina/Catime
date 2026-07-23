        GetDlgItemTextW(state->hwnd, controlId, wideValue,
                        (int)_countof(wideValue));
        int copied = WideCharToMultiByte(CP_UTF8, 0, wideValue, -1, value,
                                         (int)sizeof(value), NULL, NULL);
        if (copied <= 0 || !ColorStringToColorRef(value, &color)) return;
    } else if (!PickerReadRgbFields(state, &color)) {
        return;
    }
    PickerSetColor(state, color, TRUE, TRUE);
}

static void PickerSaveCurrentColor(ModernColorPickerState* state) {
    if (!state || !state->customColors || state->customColorCapacity == 0) {
        return;
    }
    size_t existing = state->customColorCount;
    for (size_t i = 0; i < state->customColorCount; i++) {
        if (state->customColors[i] == state->selectedColor) {
            existing = i;
            break;
        }
    }
    if (existing < state->customColorCount) {
        for (size_t i = existing; i > 0; i--) {
            state->customColors[i] = state->customColors[i - 1];
        }
    } else {
        size_t count = state->customColorCount;
        if (count < state->customColorCapacity) count++;
        for (size_t i = count; i > 1; i--) {
            state->customColors[i - 1] = state->customColors[i - 2];
        }
        state->customColorCount = count;
    }
    state->customColors[0] = state->selectedColor;
    state->savedFocusIndex = 0;
    InvalidateRect(GetDlgItem(state->hwnd, IDC_MODERN_COLOR_SAVED),
                   NULL, FALSE);
}

static void PickerUpdateSvFromPoint(ModernColorPickerState* state,
                                    HWND control, int x, int y) {
    RECT rect = {0};
    GetClientRect(control, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 1 || height <= 1) return;
    state->saturation = PickerClampUnit((double)x / (width - 1));
    state->value = PickerClampUnit(1.0 - (double)y / (height - 1));
    PickerSetColorFromHsv(state, TRUE);
}

static void PickerUpdateHueFromPoint(ModernColorPickerState* state,
                                     HWND control, int y) {
    RECT rect = {0};
    GetClientRect(control, &rect);
    int height = rect.bottom - rect.top;
    if (height <= 1) return;
    double clamped = PickerClampUnit((double)y / (height - 1));
    state->hue = clamped * 359.999;
    state->svCachedHue = -1;
    PickerSetColorFromHsv(state, TRUE);
}

static int PickerSavedIndexFromPoint(HWND control, int x, int y) {
    RECT bounds = {0};
    GetClientRect(control, &bounds);
    POINT point = {x, y};
    for (int i = 0; i < 16; i++) {
        RECT cell;
        if (PickerGetSavedCellRect(&bounds, (size_t)i, &cell) &&
            PtInRect(&cell, point)) {
            return i;
        }
    }
    return -1;
}

static void PickerSelectSavedColor(ModernColorPickerState* state, int index) {
    if (!state || index < 0 || (size_t)index >= state->customColorCount) {
        return;
    }
    state->savedFocusIndex = index;
    PickerSetColor(state, state->customColors[index], TRUE, TRUE);
}

static BOOL PickerSampleScreenColor(COLORREF* color) {
    if (!color) return FALSE;
    POINT point;
    if (!GetCursorPos(&point)) return FALSE;
    HDC screen = GetDC(NULL);
    if (!screen) return FALSE;
    COLORREF sampled = GetPixel(screen, point.x, point.y);
    ReleaseDC(NULL, screen);
    if (sampled == CLR_INVALID) return FALSE;
    *color = sampled;
    return TRUE;
}

static void PickerBeginEyedropper(ModernColorPickerState* state) {
    if (!state || state->eyedropperActive) return;
    state->eyedropperOriginalColor = state->selectedColor;
    state->eyedropperActive = TRUE;
    SetCapture(state->hwnd);
    SetCursor(LoadCursorW(NULL, IDC_CROSS));
}

static void PickerEndEyedropper(ModernColorPickerState* state, BOOL keepColor) {
    if (!state || !state->eyedropperActive) return;
    state->eyedropperActive = FALSE;
    if (GetCapture() == state->hwnd) ReleaseCapture();
    if (!keepColor) {
        PickerSetColor(state, state->eyedropperOriginalColor, TRUE, TRUE);
    }
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

static void PickerLayoutControls(HWND hwndDlg) {
    UINT dpi = DialogModern_GetDpi(hwndDlg);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_SV, dpi,
                                0, 0, 330, 220);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_HUE, dpi,
                                342, 0, 24, 220);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_PREVIEW, dpi,
                                386, 0, 174, 46);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_HEX_LABEL, dpi,
                                386, 62, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_HEX_EDIT, dpi,
                                426, 56, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_RED_LABEL, dpi,
                                386, 106, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_RED_EDIT, dpi,
                                426, 100, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_GREEN_LABEL, dpi,
                                386, 148, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_GREEN_EDIT, dpi,
                                426, 142, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_BLUE_LABEL, dpi,
                                386, 190, 32, 24);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_BLUE_EDIT, dpi,
                                426, 184, 134, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_SAVED, dpi,
                                0, 240, 366, 54);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_PICK_BUTTON, dpi,
                                386, 240, 82, 36);
    DialogModern_SetChildRect96(hwndDlg, IDC_MODERN_COLOR_SAVE_BUTTON, dpi,
                                478, 240, 82, 36);
    DialogModern_SetChildRect96(hwndDlg, IDCANCEL, dpi,
                                384, 316, 84, 36);
    DialogModern_SetChildRect96(hwndDlg, IDOK, dpi,
                                478, 316, 82, 36);
}

static void PickerSetupCanvas(HWND hwndDlg, int controlId,
                              ModernColorPickerState* state) {
    HWND control = GetDlgItem(hwndDlg, controlId);
    if (!control) return;
    SetWindowSubclass(control, ColorPickerCanvasSubclassProc,
                      (UINT_PTR)controlId, (DWORD_PTR)state);
}

static BOOL PickerHandleCanvasKey(ModernColorPickerState* state,
                                  UINT_PTR controlId, WPARAM key) {
    if (!state) return FALSE;
    double smallStep = GetKeyState(VK_SHIFT) < 0 ? 0.05 : 0.01;
    if (controlId == IDC_MODERN_COLOR_SV) {
        if (key == VK_LEFT) state->saturation -= smallStep;
        else if (key == VK_RIGHT) state->saturation += smallStep;
        else if (key == VK_UP) state->value += smallStep;
        else if (key == VK_DOWN) state->value -= smallStep;
        else return FALSE;
        PickerSetColorFromHsv(state, TRUE);
        return TRUE;
    }
    if (controlId == IDC_MODERN_COLOR_HUE) {
        double hueStep = GetKeyState(VK_SHIFT) < 0 ? 10.0 : 2.0;
        if (key == VK_LEFT || key == VK_UP) state->hue -= hueStep;
        else if (key == VK_RIGHT || key == VK_DOWN) state->hue += hueStep;
        else return FALSE;
        state->svCachedHue = -1;
        PickerSetColorFromHsv(state, TRUE);
        return TRUE;
    }
    if (controlId == IDC_MODERN_COLOR_SAVED &&
        state->customColorCount > 0) {
        int index = state->savedFocusIndex;
        if (index < 0 || (size_t)index >= state->customColorCount) index = 0;
        if (key == VK_LEFT) index--;
        else if (key == VK_RIGHT) index++;
        else if (key == VK_UP) index -= 8;
        else if (key == VK_DOWN) index += 8;
        else if (key == VK_RETURN || key == VK_SPACE) {
            PickerSelectSavedColor(state, index);
            return TRUE;
        } else {
            return FALSE;
        }
        if (index < 0) index = 0;
        if ((size_t)index >= state->customColorCount) {
            index = (int)state->customColorCount - 1;
        }
        state->savedFocusIndex = index;
        InvalidateRect(GetDlgItem(state->hwnd, IDC_MODERN_COLOR_SAVED),
                       NULL, FALSE);
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK ColorPickerCanvasSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    ModernColorPickerState* state = (ModernColorPickerState*)refData;
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            SetCapture(hwnd);
            if (subclassId == IDC_MODERN_COLOR_SV) {
                PickerUpdateSvFromPoint(state, hwnd,
                                        GET_X_LPARAM(lParam),
                                        GET_Y_LPARAM(lParam));
            } else if (subclassId == IDC_MODERN_COLOR_HUE) {
                PickerUpdateHueFromPoint(state, hwnd, GET_Y_LPARAM(lParam));
            } else if (subclassId == IDC_MODERN_COLOR_SAVED) {
                PickerSelectSavedColor(
                    state, PickerSavedIndexFromPoint(
                               hwnd, GET_X_LPARAM(lParam),
                               GET_Y_LPARAM(lParam)));
            }
            return 0;
        case WM_MOUSEMOVE:
            if ((wParam & MK_LBUTTON) && GetCapture() == hwnd) {
                if (subclassId == IDC_MODERN_COLOR_SV) {
                    PickerUpdateSvFromPoint(state, hwnd,
                                            GET_X_LPARAM(lParam),
                                            GET_Y_LPARAM(lParam));
                } else if (subclassId == IDC_MODERN_COLOR_HUE) {
                    PickerUpdateHueFromPoint(state, hwnd,
                                             GET_Y_LPARAM(lParam));
                }
            }
            break;
        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) ReleaseCapture();
            return 0;
        case WM_KEYDOWN:
            if (PickerHandleCanvasKey(state, subclassId, wParam)) return 0;
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(NULL,
                subclassId == IDC_MODERN_COLOR_SAVED ? IDC_HAND : IDC_CROSS));
            return TRUE;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ColorPickerCanvasSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

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
