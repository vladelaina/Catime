                CountdownTrackNonClientMouse(hwnd);
            }
            break;

        case WM_MOUSELEAVE:
            if (state) {
                CountdownRefreshTitleHoverFromCursor(hwnd, state);
                if (state->hoverPart == COUNTDOWN_HOVER_CLOSE) {
                    state->hoverPart = COUNTDOWN_HOVER_NONE;
                    InvalidateRect(hwnd, &state->closeFrame, FALSE);
                }
            }
            break;

        case WM_NCMOUSELEAVE:
            if (state) {
                CountdownRefreshTitleHoverFromCursor(hwnd, state);
            }
            break;

        case WM_DPICHANGED:
            if (state) {
                state->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96;
                RECT* suggested = (RECT*)lParam;
                if (suggested) {
                    SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                CountdownRefreshPalette(state);
                CountdownBuildFonts(state);
                CountdownUpdateTextMetrics(hwnd, state);
                CountdownEnsureContentWidth(hwnd, state);
                CountdownLayout(hwnd, state);
                return 0;
            }
            break;

        case WM_SIZE:
            if (state) {
                CountdownUpdateTextMetrics(hwnd, state);
                CountdownLayout(hwnd, state);
                return 0;
            }
            break;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (state) {
                CountdownRefreshPalette(state);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_NCDESTROY:
            if (state) {
                KillTimer(hwnd, INPUT_FOCUS_TIMER_ID);
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_INPUT, hwnd);
                if (g_hwndInputDialog == hwnd) {
                    g_hwndInputDialog = NULL;
                }
                if (state->editBrush) DeleteObject(state->editBrush);
                CountdownDestroyFonts(state);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                free(state);
            }
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}


/**
 * @brief Show countdown input dialog (modeless)
 * @param hwndParent Parent window handle
 */
void ShowCountdownInputDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_INPUT)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_INPUT);
        SetForegroundWindow(existing);
        return;
    }

    if (!DialogInput_IsValidParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateCustomCountdownDialog(hwndParent);
    if (!hwndDlg) {
        LOG_WARNING("CountdownDialog: custom window creation failed; using resource fallback");
        hwndDlg = DialogInput_CreateResourceDialog(hwndParent, CLOCK_IDD_DIALOG1,
                                            CLOCK_IDD_DIALOG1, -1);
    }

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOWNORMAL);
        UpdateWindow(hwndDlg);
        SetForegroundWindow(hwndDlg);
    }
}
