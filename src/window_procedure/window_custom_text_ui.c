/**
 * @file window_custom_text_ui.c
 * @brief Custom text preview state and editor appearance.
 */

#include "window_commands_plugin_internal.h"

void WindowPlugin_MoveEditCaretToEnd(HWND hwndEdit) {
    if (!hwndEdit) {
        return;
    }

    int textLen = GetWindowTextLengthW(hwndEdit);
    if (textLen < 0) {
        textLen = 0;
    }

    SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);
    SendMessageW(hwndEdit, EM_SCROLLCARET, 0, 0);
}

HFONT WindowPlugin_CreateCustomTextDisplayEditFont(HWND hwndEdit) {
    if (!hwndEdit) {
        return NULL;
    }

    LOGFONTW lf = {0};
    HFONT currentFont = (HFONT)SendMessageW(hwndEdit, WM_GETFONT, 0, 0);
    if (!currentFont || GetObjectW(currentFont, sizeof(lf), &lf) != sizeof(lf)) {
        HFONT defaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if (!defaultFont || GetObjectW(defaultFont, sizeof(lf), &lf) != sizeof(lf)) {
            return NULL;
        }
    }

    char activeFontFile[MAX_PATH] = {0};
    char activeFontInternalName[MAX_PATH] = {0};
    GetActiveFont(activeFontFile, activeFontInternalName, sizeof(activeFontInternalName));
    if (activeFontInternalName[0] != '\0') {
        wchar_t activeFaceName[MAX_PATH] = {0};
        if (Utf8ToWide(activeFontInternalName, activeFaceName, _countof(activeFaceName))) {
            wcsncpy_s(lf.lfFaceName, _countof(lf.lfFaceName), activeFaceName, _TRUNCATE);
        }
    }

    int editFontSize = CLOCK_BASE_FONT_SIZE;
    if (editFontSize < CUSTOM_TEXT_DISPLAY_EDIT_FONT_MIN_PX) {
        editFontSize = CUSTOM_TEXT_DISPLAY_EDIT_FONT_MIN_PX;
    }
    if (editFontSize > CUSTOM_TEXT_DISPLAY_EDIT_FONT_MAX_PX) {
        editFontSize = CUSTOM_TEXT_DISPLAY_EDIT_FONT_MAX_PX;
    }
    lf.lfHeight = -editFontSize;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;

    HFONT editFont = CreateFontIndirectW(&lf);
    if (!editFont) {
        return NULL;
    }

    return editFont;
}

void RefreshCustomTextDisplayDialogFont(void) {
    HWND hwndDlg = Dialog_GetInstance(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY);
    if (!hwndDlg || !IsWindow(hwndDlg)) {
        return;
    }

    CustomTextDisplayState* state =
        (CustomTextDisplayState*)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);
    if (!state) {
        return;
    }

    HWND hwndEdit = GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT);
    HFONT newFont = WindowPlugin_CreateCustomTextDisplayEditFont(hwndEdit);
    if (!newFont) {
        return;
    }

    HFONT oldFont = state->editFont;
    state->editFont = newFont;
    SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)state->editFont, TRUE);

    if (oldFont && oldFont != state->editFont) {
        DeleteObject(oldFont);
    }
    InvalidateRect(hwndEdit, NULL, TRUE);
}

static void ApplyCustomTextDisplayWindowState(HWND hwnd, BOOL preserveDialogFocus) {
    countdown_message_shown = true;

    if (!PluginData_HasCatimeTag()) {
        MainTimer_Stop();
        CLOCK_SHOW_CURRENT_TIME = false;
        CLOCK_COUNT_UP = false;
        CLOCK_IS_PAUSED = false;
    }

    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) {
        MainTimer_Start(hwnd, 66);
    }

    if (!preserveDialogFocus) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

void WindowPlugin_StopPluginsForCustomTextDisplay(CustomTextDisplayState* state) {
    if (state && state->pluginsStopped) {
        return;
    }
    PluginManager_StopAllPlugins();
    if (state) {
        state->pluginsStopped = TRUE;
    }
}

BOOL WindowPlugin_ApplyCustomTextDisplayPreview(HWND hwnd, const wchar_t* text,
                                          const wchar_t* sourcePath,
                                          BOOL preserveDialogFocus) {
    const wchar_t* previewText = (text && text[0] != L'\0')
                                     ? text
                                     : CUSTOM_TEXT_DISPLAY_EMPTY_PREVIEW_TEXT_W;
    if (!PluginData_SetPreviewTextWithSource(previewText, sourcePath)) {
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    ApplyCustomTextDisplayWindowState(hwnd, preserveDialogFocus);
    return TRUE;
}

BOOL WindowPlugin_QueueCustomTextDisplayPreview(HWND hwndDlg) {
    KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
    return SetTimer(hwndDlg,
                    CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID,
                    CUSTOM_TEXT_DISPLAY_PREVIEW_DELAY_MS,
                    NULL) != 0;
}

BOOL WindowPlugin_ApplyCustomTextDisplayText(HWND hwndDlg, CustomTextDisplayState* state,
                                       BOOL preserveDialogFocus) {
    wchar_t* text = NULL;
    if (!state || !WindowPlugin_GetCustomTextDisplayText(hwndDlg, &text)) {
        return FALSE;
    }

    if (!WindowPlugin_SaveCustomTextDisplayContent(state->contentPath, text)) {
        free(text);
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    WindowPlugin_StopPluginsForCustomTextDisplay(state);
    BOOL applied = WindowPlugin_ApplyCustomTextDisplayPreview(state->owner, text,
                                                 state->contentPath,
                                                 preserveDialogFocus);
    free(text);
    return applied;
}
