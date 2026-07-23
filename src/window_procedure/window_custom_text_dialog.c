/**
 * @file window_custom_text_dialog.c
 * @brief Custom text display dialog lifecycle and interaction.
 */

#include "window_commands_plugin_internal.h"

static LRESULT CALLBACK CustomTextDisplayEditSubclassProc(HWND hwnd, UINT msg,
                                                          WPARAM wParam,
                                                          LPARAM lParam,
                                                          UINT_PTR subclassId,
                                                          DWORD_PTR refData) {
    (void)subclassId;
    (void)refData;

    if (msg == WM_KEYDOWN &&
        wParam == VK_RETURN &&
        (GetKeyState(VK_CONTROL) & 0x8000)) {
        HWND hwndDlg = GetParent(hwnd);
        if (hwndDlg) {
            SendMessageW(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
                         (LPARAM)GetDlgItem(hwndDlg, IDOK));
            return 0;
        }
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, CustomTextDisplayEditSubclassProc,
                             CUSTOM_TEXT_DISPLAY_EDIT_SUBCLASS_ID);
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static INT_PTR CALLBACK CustomTextDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomTextDisplayState* state =
        (CustomTextDisplayState*)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);

    switch (msg) {
        case WM_INITDIALOG: {
            state = (CustomTextDisplayState*)lParam;
            if (!state) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);
            Dialog_InitializeInstance(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY, hwndDlg);
            state->initializing = TRUE;

            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Custom Text Display"));
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(NULL, L"Close"));
            SetDlgItemTextW(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_HINT,
                            GetLocalizedString(NULL, L"Use <md>...</md> to enable Markdown"));
            Dialog_CenterOnPrimaryScreen(hwndDlg);

            HWND hwndEdit = GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT);
            state->editFont = WindowPlugin_CreateCustomTextDisplayEditFont(hwndEdit);
            if (state->editFont) {
                SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)state->editFont, TRUE);
            }
            SetWindowSubclass(hwndEdit, CustomTextDisplayEditSubclassProc,
                              CUSTOM_TEXT_DISPLAY_EDIT_SUBCLASS_ID, 0);
            SendMessageW(hwndEdit, EM_LIMITTEXT, CUSTOM_TEXT_DISPLAY_MAX_CHARS, 0);
            SetDlgItemTextW(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT,
                            state && state->originalText ? state->originalText : L"");
            state->initializing = FALSE;
            WindowPlugin_StopPluginsForCustomTextDisplay(state);
            WindowPlugin_ApplyCustomTextDisplayPreview(state->owner,
                                          state->originalText ? state->originalText : L"",
                                          state->contentPath,
                                          TRUE);
            SetFocus(hwndEdit);
            WindowPlugin_MoveEditCaretToEnd(hwndEdit);
            return FALSE;
        }

        case WM_TIMER:
            if (wParam == CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID) {
                KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
                WindowPlugin_ApplyCustomTextDisplayText(hwndDlg, state, TRUE);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_CUSTOM_TEXT_DISPLAY_TEXT && HIWORD(wParam) == EN_CHANGE) {
                if (state && state->initializing) {
                    return TRUE;
                }
                if (!WindowPlugin_QueueCustomTextDisplayPreview(hwndDlg)) {
                    WindowPlugin_ApplyCustomTextDisplayText(hwndDlg, state, TRUE);
                }
                return TRUE;
            }

            if (LOWORD(wParam) == IDOK) {
                KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
                if (!WindowPlugin_ApplyCustomTextDisplayText(hwndDlg, state, FALSE)) {
                    return TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            if (LOWORD(wParam) == IDCANCEL) {
                KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
                if (!WindowPlugin_ApplyCustomTextDisplayText(hwndDlg, state, FALSE)) {
                    return TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
            if (!WindowPlugin_ApplyCustomTextDisplayText(hwndDlg, state, FALSE)) {
                return TRUE;
            }
            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
            RemoveWindowSubclass(GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT),
                                 CustomTextDisplayEditSubclassProc,
                                 CUSTOM_TEXT_DISPLAY_EDIT_SUBCLASS_ID);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY, hwndDlg);
            if (state) {
                if (state->editFont) {
                    DeleteObject(state->editFont);
                }
                free(state->originalText);
                free(state);
                SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            }
            return TRUE;
    }

    return FALSE;
}

static BOOL ShowCustomTextDisplayDialog(HWND hwnd) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY);
        SetForegroundWindow(existing);
        return TRUE;
    }

    CustomTextDisplayState* state =
        (CustomTextDisplayState*)calloc(1, sizeof(CustomTextDisplayState));
    if (!state) {
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    state->owner = hwnd;
    if (!WindowPlugin_GetCustomTextDisplayPath(state->contentPath, _countof(state->contentPath))) {
        free(state);
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    state->originalText = WindowPlugin_LoadCustomTextDisplayContent(state->contentPath);
    if (!state->originalText) {
        state->originalText = WindowPlugin_DuplicateWideString(L"");
    }

    HWND hwndDlg = CreateDialogParamW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_CUSTOM_TEXT_DISPLAY_DIALOG),
                                      hwnd,
                                      CustomTextDisplayDlgProc,
                                      (LPARAM)state);
    if (!hwndDlg) {
        free(state->originalText);
        free(state);
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    ShowWindow(hwndDlg, SW_SHOW);
    return TRUE;
}

/**
 * @brief Handle custom text display command
 */
BOOL WindowPlugin_HandleCustomTextDisplay(HWND hwnd) {
    return ShowCustomTextDisplayDialog(hwnd);
}

/* ============================================================================
 * Plugin Exit Handler (for <exit> tag)
 * ============================================================================ */

/**
 * @brief Handle plugin exit request (from <exit> tag countdown)
 * Reuses the same logic as manually clicking to stop a plugin
 */
