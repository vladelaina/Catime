                if (ModernGetScrollbarRects(state, &track, &thumb) &&
                    PtInRect(&track, point)) {
                    SetCursor(LoadCursorW(NULL, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        case WM_CTLCOLORDLG:
            if (state && state->finalized) {
                SetBkColor((HDC)wParam, state->palette.surface);
                return (LRESULT)state->surfaceBrush;
            }
            break;
        case WM_CTLCOLORSTATIC:
            if (state && state->finalized) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->palette.text);
                return (LRESULT)state->surfaceBrush;
            }
            break;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (state && state->finalized) {
                SetBkColor((HDC)wParam, state->palette.field);
                SetTextColor((HDC)wParam, state->palette.text);
                return (LRESULT)state->fieldBrush;
            }
            break;
        case WM_CTLCOLORBTN:
            if (state && state->finalized) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->palette.text);
                return (LRESULT)state->surfaceBrush;
            }
            break;
        case WM_NCHITTEST:
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                RECT closeRect = {0};
                if (state->closeButton) GetWindowRect(state->closeButton, &closeRect);
                if (state->closeButton) MapWindowPoints(NULL, hwnd,
                                                       (POINT*)&closeRect, 2);
                if (PtInRect(&closeRect, point)) return HTCLIENT;
                if (point.y < DialogModern_Scale(state->dpi,
                                                 state->headerHeight96)) {
                    return HTCAPTION;
                }
                return HTCLIENT;
            }
            break;
        case WM_DPICHANGED:
            if (state && state->finalized) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernHandleDpiChanged(state, wParam, lParam);
                return result;
            }
            break;
        case WM_SIZE:
            if (state && state->finalized && !state->finalizing) {
                ModernSyncClientSizeFromWindow(state);
                ModernLayoutControls(state);
                DialogModern_ApplyWindowShape(hwnd, state->dpi, 20);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (state && state->finalized) {
                DialogModern_Refresh(hwnd);
                return 0;
            }
            break;
        case WM_NCDESTROY: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            RemovePropW(hwnd, MODERN_DIALOG_STATE_PROP);
            RemoveWindowSubclass(hwnd, ModernDialogSubclassProc,
                                 MODERN_DIALOG_SUBCLASS_ID);
            ModernFreeState(state);
            return result;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

BOOL DialogModern_Attach(HWND hwndDlg, int dialogType) {
    if (!hwndDlg || !IsWindow(hwndDlg) || DialogModern_IsAttached(hwndDlg)) {
        return hwndDlg && DialogModern_IsAttached(hwndDlg);
    }

    wchar_t className[64] = {0};
    if (!GetClassNameW(hwndDlg, className, _countof(className)) ||
        wcscmp(className, L"#32770") != 0) {
        return FALSE;
    }

    ModernDialogState* state =
        (ModernDialogState*)calloc(1, sizeof(*state));
    if (!state) return FALSE;
    state->hwnd = hwndDlg;
    state->dialogType = dialogType;
    state->dpi = DialogModern_GetDpi(hwndDlg);
    state->attached = TRUE;
    if (!SetPropW(hwndDlg, MODERN_DIALOG_STATE_PROP, (HANDLE)state) ||
        !SetWindowSubclass(hwndDlg, ModernDialogSubclassProc,
                           MODERN_DIALOG_SUBCLASS_ID, (DWORD_PTR)state)) {
        RemovePropW(hwndDlg, MODERN_DIALOG_STATE_PROP);
        free(state);
        return FALSE;
    }
    PostMessageW(hwndDlg, MODERN_DIALOG_FINALIZE_MESSAGE, 0, 0);
    return TRUE;
}

void DialogModern_Refresh(HWND hwndDlg) {
    ModernDialogState* state = ModernGetState(hwndDlg);
    if (!state) return;
    if (state->refreshing) {
        state->refreshPending = TRUE;
        return;
    }

    state->refreshing = TRUE;
    for (;;) {
        state->refreshPending = FALSE;
        ModernRebuildResources(state);
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernSetControlFont(state, &state->controls[i]);
            ModernApplyFieldRegion(&state->controls[i]);
            ModernApplyEditLayout(&state->controls[i]);
            ModernHideDateTimeSpinner(&state->controls[i]);
        }
        RedrawWindow(hwndDlg, NULL, NULL,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        if (ModernGetState(hwndDlg) != state) return;
        if (!state->refreshPending) break;
    }
    state->refreshing = FALSE;
}
