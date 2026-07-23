
        case WM_DESTROY:
            if (!g_fontState.closeHandled) {
                RestoreOriginalFont();
                g_fontState.closeHandled = TRUE;
            }
            KillTimer(hdlg, FONT_PICKER_TOPMOST_TIMER_ID);
            KillTimer(hdlg, FONT_ENUM_POLL_TIMER_ID);
            KillTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_FONT_PICKER, hdlg);

            BOOL enumStopped = StopFontEnumerationWithTimeout(FONT_ENUM_STOP_WAIT_MS);
            if (!enumStopped) {
                LOG_WARNING("FontPicker: Leaving slow enumeration thread to finish asynchronously");
                ScheduleDeferredFontEnumerationCleanup();
            }

            /* Clear checkmark index */
            g_currentFontIndex = -1;
            g_previewFontIndex = -1;
            g_fontListReady = FALSE;
            g_fontState.closeHandled = FALSE;

            if (enumStopped) {
                ResetFontMap();
            }
            return TRUE;
    }
    return FALSE;
}

BOOL ShowSystemFontDialog(HWND hwndParent) {
    /* Check if dialog is already open */
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER);
        SetForegroundWindow(existing);
        return TRUE;
    }

    /* Create modeless dialog */
    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(IDD_FONT_PICKER_SIMPLE),
        hwndParent,
        SimpleFontPickerProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
        return TRUE;
    }
    return FALSE;
}
