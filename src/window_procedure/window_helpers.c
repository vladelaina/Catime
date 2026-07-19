/**
 * @file window_helpers.c
 * @brief Window procedure helper functions implementation
 */

#include "window_procedure/window_helpers.h"
#include "window_procedure/window_utils.h"
#include "config.h"
#include "font.h"
#include "font/font_manager.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "timer/timer_events.h"
#include "window.h"
#include "pomodoro.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_common.h"
#include "drawing.h"
#include "notification.h"
#include "../resource/resource.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <winnls.h>
#include <commdlg.h>

/* Defined in config_ini.c; kept as a file-scope forward declaration until a shared header exists. */
void InvalidateIniCache(void);

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];

/* ============================================================================
 * Timer Mode Switching
 * ============================================================================ */

BOOL SwitchTimerMode(HWND hwnd, TimerMode mode, const TimerModeParams* params) {
    bool wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    TimerModeParams defaultParams = {0, TRUE, TRUE, TRUE};  /* showWindow = TRUE by default */
    if (!params) params = &defaultParams;
    
    CLOCK_SHOW_CURRENT_TIME = (mode == TIMER_MODE_SHOW_TIME);
    CLOCK_COUNT_UP = (mode == TIMER_MODE_COUNTUP);
    CLOCK_IS_PAUSED = false;
    
    if (mode == TIMER_MODE_COUNTDOWN || mode == TIMER_MODE_POMODORO) {
        CLOCK_TOTAL_TIME = params->totalSeconds;
    }
    
    if (params->resetElapsed) {
        ResetTimer();
    }
    
    if (params->showWindow) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
    
    if (params->resetInterval && (wasShowingTime || mode == TIMER_MODE_SHOW_TIME)) {
        MainTimer_Stop();
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

/* ============================================================================
 * Input Dialog System
 * ============================================================================ */

static void NotifyInputBoxPreview(HWND hwndDlg, const InputBoxParams* params) {
    if (!params || !params->previewCallback) return;

    HWND hwndEdit = GetDlgItem(hwndDlg, IDC_EDIT_INPUT);
    int textLength = hwndEdit ? GetWindowTextLengthW(hwndEdit) : 0;
    if (textLength < 0 || (size_t)textLength >= params->maxLen) return;

    wchar_t* text = (wchar_t*)calloc((size_t)textLength + 1, sizeof(wchar_t));
    if (!text) return;

    GetWindowTextW(hwndEdit, text, textLength + 1);
    params->previewCallback(text, params->previewContext);
    free(text);
}

INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    InputBoxParams* params = (InputBoxParams*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

    switch (uMsg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_INPUT_BOX, hwndDlg);
            params = (InputBoxParams*)lParam;
            if (!params || !params->result || params->maxLen == 0 || params->maxLen > INT_MAX) {
                EndDialog(hwndDlg, FALSE);
                return TRUE;
            }
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)params);

            HWND hwndEdit = GetDlgItem(hwndDlg, IDC_EDIT_INPUT);
            if (hwndEdit) {
                SendMessageW(hwndEdit, EM_SETLIMITTEXT, (WPARAM)(params->maxLen - 1), 0);
            }
            SetWindowTextW(hwndDlg, params->title);
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
            NotifyInputBoxPreview(hwndDlg, params);

            Dialog_SelectAllText(hwndEdit);
            SetFocus(hwndEdit);

            Dialog_CenterOnPrimaryScreen(hwndDlg);
            return FALSE;
        }
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                EndDialog(hwndDlg, FALSE);
                return TRUE;
            }
            break;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_EDIT_INPUT && HIWORD(wParam) == EN_CHANGE) {
                NotifyInputBoxPreview(hwndDlg, params);
                return TRUE;
            }
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (!params || !params->result || params->maxLen == 0 || params->maxLen > INT_MAX) {
                        EndDialog(hwndDlg, FALSE);
                        return TRUE;
                    }
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->result, (int)params->maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                
                case IDCANCEL:
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
        
        case WM_CLOSE:
            EndDialog(hwndDlg, FALSE);
            return TRUE;
            
        case WM_DESTROY:
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_INPUT_BOX, hwndDlg);
            break;
    }
    
    return FALSE;
}

BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt,
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    return InputBoxWithPreview(hwndParent, title, prompt, defaultText,
                               result, maxLen, NULL, NULL);
}

BOOL InputBoxWithPreview(HWND hwndParent, const wchar_t* title, const wchar_t* prompt,
                         const wchar_t* defaultText, wchar_t* result, size_t maxLen,
                         InputBoxPreviewCallback previewCallback, void* previewContext) {
    InputBoxParams params = {0};
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    params.previewCallback = previewCallback;
    params.previewContext = previewContext;
    
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_INPUTBOX), 
                          hwndParent, 
                          InputBoxProc, 
                          (LPARAM)&params) == TRUE;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

BOOL ShowFilePicker(HWND hwnd, char* selectedPath, size_t bufferSize) {
    if (!selectedPath || bufferSize == 0 || bufferSize > INT_MAX) {
        return FALSE;
    }

    selectedPath[0] = '\0';

    wchar_t szFile[MAX_PATH] = {0};
    
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        int required = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, NULL, 0, NULL, NULL);
        if (required <= 0 || (size_t)required > bufferSize) {
            return FALSE;
        }

        if (WideCharToMultiByte(CP_UTF8, 0, szFile, -1, selectedPath,
                                (int)bufferSize, NULL, NULL) <= 0) {
            selectedPath[0] = '\0';
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

BOOL ValidateAndSetTimeoutFile(HWND hwnd, const char* filePathUtf8) {
    UNREFERENCED_PARAMETER(hwnd);
    if (!filePathUtf8 || filePathUtf8[0] == '\0') {
        return FALSE;
    }
    
    WideString ws = ToWide(filePathUtf8);
    if (!ws.valid) return FALSE;
    
    if (GetFileAttributesW(ws.buf) != INVALID_FILE_ATTRIBUTES) {
        if (WriteConfigTimeoutFile(filePathUtf8)) {
            if (!SaveRecentFile(filePathUtf8)) {
                LOG_WARNING("Failed to persist recent file entry: %s", filePathUtf8);
            }
            return TRUE;
        }
        LOG_WARNING("Failed to persist selected timeout file: %s", filePathUtf8);
        return FALSE;
    } else {
        LOG_WARNING("Selected timeout file does not exist: %s", filePathUtf8);
        return FALSE;
    }
}

BOOL GetFontsFolderWideFromConfig(wchar_t* out, size_t size) {
    if (!out || size == 0) return FALSE;
    
    WideString ws = ToWide(GetCachedConfigPath());
    if (!ws.valid) return FALSE;
    wchar_t wConfigPath[MAX_PATH];
    wcscpy_s(wConfigPath, MAX_PATH, ws.buf);
    
    wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
    if (!lastSep) return FALSE;
    *lastSep = L'\0';
    
    _snwprintf_s(out, size, _TRUNCATE, L"%s\\resources\\fonts", wConfigPath);
    return TRUE;
}

/* ============================================================================
 * System Reset and Initialization
 * ============================================================================ */

void ResetTimerStateToDefaults(void) {
    /* Reset timer values - actual mode will be set by HandleStartupMode */
    CLOCK_TOTAL_TIME = 0;
    elapsed_time = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    message_shown = FALSE;
    countdown_message_shown = false;
    
    CLOCK_COUNT_UP = false;
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_IS_PAUSED = false;
    
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    ResetTimer();
}

/* DetectSystemLanguage is defined in config_defaults.c */

void ResetConfigurationFile(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /* Clear in-memory INI cache first - critical for reset to work */
    InvalidateIniCache();
    
    /* Delete the config file - ReadConfig will recreate it */
    wchar_t wconfig_path[MAX_PATH];
    if (config_path[0] == '\0' ||
        MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH) == 0) {
        LOG_WARNING("Failed to convert config path for reset");
        return;
    }
    DeleteFileW(wconfig_path);
}

void ReloadDefaultFont(void) {
    
    /* Use the default font name, not the current FONT_FILE_NAME */
    const char* defaultFontName = DEFAULT_FONT_NAME;
    
    /* Update FONT_FILE_NAME to default */
    snprintf(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), "%s%s", FONTS_PATH_PREFIX, defaultFontName);
    snprintf(FONT_RUNTIME_FILE_NAME, sizeof(FONT_RUNTIME_FILE_NAME), "%s%s",
             FONTS_PATH_PREFIX, defaultFontName);
    
    /* Load the default font */
    LoadFontByNameAndGetRealName(GetModuleHandle(NULL), defaultFontName, 
                                 FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
}

void RecalculateWindowSize(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    
    wchar_t fontNameW[256];
    int fontNameLen = MultiByteToWideChar(CP_UTF8, 0, FONT_INTERNAL_NAME, -1,
                                          fontNameW, (int)(sizeof(fontNameW) / sizeof(fontNameW[0])));
    if (fontNameLen <= 0) {
        wcscpy_s(fontNameW, _countof(fontNameW), L"Segoe UI");
    }
    
    HFONT hFont = CreateFontW(
        -CLOCK_BASE_FONT_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, fontNameW
    );
    HFONT hOldFont = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;
    
    char time_text[50];
    FormatTime(CLOCK_TOTAL_TIME, time_text);
    
    wchar_t time_textW[50];
    int timeTextLen = MultiByteToWideChar(CP_UTF8, 0, time_text, -1,
                                          time_textW, (int)(sizeof(time_textW) / sizeof(time_textW[0])));
    if (timeTextLen <= 0) {
        wcscpy_s(time_textW, _countof(time_textW), L"00:00");
        timeTextLen = (int)wcslen(time_textW) + 1;
    }

    SIZE textSize = { CLOCK_BASE_FONT_SIZE * 4, CLOCK_BASE_FONT_SIZE };
    int visibleChars = timeTextLen > 0 ? timeTextLen - 1 : (int)wcslen(time_textW);
    GetTextExtentPoint32W(hdc, time_textW, visibleChars, &textSize);

    if (hOldFont) SelectObject(hdc, hOldFont);
    if (hFont) DeleteObject(hFont);
    ReleaseDC(hwnd, hdc);
    
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    if (screenHeight <= 0) {
        return;
    }

    float defaultScale = (screenHeight * 0.03f) / 20.0f;
    
    int newWidth = (int)(textSize.cx * defaultScale);
    int newHeight = (int)(textSize.cy * defaultScale);
    if (newWidth <= 0 || newHeight <= 0) {
        return;
    }

    CLOCK_WINDOW_SCALE = defaultScale;
    CLOCK_FONT_SCALE_FACTOR = defaultScale;
    PLUGIN_FONT_SCALE_FACTOR = 1.0f;
    
    ResolveConfiguredWindowPosition(newWidth, newHeight,
                                    &CLOCK_WINDOW_POS_X, &CLOCK_WINDOW_POS_Y);
    
    SetWindowPos(hwnd, NULL, 
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        newWidth, newHeight,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

/* ============================================================================
 * Timeout Action Helpers
 * ============================================================================ */

void SetTimeoutAction(const char* action) {
    (void)WriteConfigTimeoutAction(action);
}

