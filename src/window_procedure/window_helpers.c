/**
 * @file window_helpers.c
 * @brief Window procedure helper functions implementation
 */

#include "window_procedure/window_helpers.h"
#include "window_procedure/window_utils.h"
#include "config.h"
#include "font.h"
#include "timer/timer.h"
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
#include <string.h>
#include <winnls.h>
#include <commdlg.h>

extern wchar_t inputText[256];
extern int elapsed_time;
extern int message_shown;
extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];

/* ============================================================================
 * Timer Mode Switching
 * ============================================================================ */

BOOL SwitchTimerMode(HWND hwnd, TimerMode mode, const TimerModeParams* params) {
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    TimerModeParams defaultParams = {0, TRUE, FALSE, TRUE};
    if (!params) params = &defaultParams;
    
    CLOCK_SHOW_CURRENT_TIME = (mode == TIMER_MODE_SHOW_TIME);
    CLOCK_COUNT_UP = (mode == TIMER_MODE_COUNTUP);
    CLOCK_IS_PAUSED = FALSE;
    
    if (params->resetElapsed) {
        elapsed_time = 0;
        countdown_elapsed_time = 0;
        countup_elapsed_time = 0;
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        ResetMillisecondAccumulator();
    }
    
    if (mode == TIMER_MODE_COUNTDOWN || mode == TIMER_MODE_POMODORO) {
        CLOCK_TOTAL_TIME = params->totalSeconds;
    }
    
    if (params->showWindow) {
        ShowWindow(hwnd, SW_SHOW);
    }
    
    if (params->resetInterval && (wasShowingTime || mode == TIMER_MODE_SHOW_TIME)) {
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

/* ============================================================================
 * Input Validation Framework
 * ============================================================================ */

static BOOL ValidateTimeInput(const char* input, void* output) {
    return ParseInput(input, (int*)output);
}

BOOL ValidatedInputLoop(HWND hwnd, UINT dialogId, 
                        InputValidator validator, void* output) {
    while (1) {
        memset(inputText, 0, sizeof(inputText));
        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(dialogId), 
                       hwnd, DlgProc, (LPARAM)dialogId);
        
        if (inputText[0] == L'\0' || isAllSpacesOnly(inputText)) {
            return FALSE;
        }
        
        Utf8String us = ToUtf8(inputText);
        if (!us.valid) {
            ShowErrorDialog(hwnd);
            continue;
        }
        char inputTextA[256];
        strcpy_s(inputTextA, sizeof(inputTextA), us.buf);
        
        if (validator(inputTextA, output)) {
            return TRUE;
        } else {
            ShowErrorDialog(hwnd);
        }
    }
}

BOOL ValidatedTimeInputLoop(HWND hwnd, UINT dialogId, int* outSeconds) {
    int result = 0;
    if (ValidatedInputLoop(hwnd, dialogId, ValidateTimeInput, &result)) {
        if (outSeconds) *outSeconds = result;
        return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Input Dialog System
 * ============================================================================ */

INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* result;
    static size_t maxLen;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            InputBoxParams* params = (InputBoxParams*)lParam;
            result = params->result;
            maxLen = params->maxLen;
            
            SetWindowTextW(hwndDlg, params->title);
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);

            HWND hwndEdit = GetDlgItem(hwndDlg, IDC_EDIT_INPUT);
            Dialog_SelectAllText(hwndEdit);
            SetFocus(hwndEdit);

            Dialog_CenterOnPrimaryScreen(hwndDlg);
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, result, (int)maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                
                case IDCANCEL:
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
    }
    
    return FALSE;
}

BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    InputBoxParams params;
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    
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
        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, selectedPath, (int)bufferSize, NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

BOOL ValidateAndSetTimeoutFile(HWND hwnd, const char* filePathUtf8) {
    if (!filePathUtf8 || filePathUtf8[0] == '\0') {
        return FALSE;
    }
    
    WideString ws = ToWide(filePathUtf8);
    if (!ws.valid) return FALSE;
    
    if (GetFileAttributesW(ws.buf) != INVALID_FILE_ATTRIBUTES) {
        WriteConfigTimeoutFile(filePathUtf8);
        SaveRecentFile(filePathUtf8);
        return TRUE;
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
    CLOCK_TOTAL_TIME = 25 * 60;
    elapsed_time = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    message_shown = FALSE;
    countdown_message_shown = FALSE;
    countup_message_shown = FALSE;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_IS_PAUSED = FALSE;
    
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    ResetTimer();
}

/* DetectSystemLanguage is defined in config_defaults.c */

void ResetConfigurationFile(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    wchar_t wconfig_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
    
    FILE* test = _wfopen(wconfig_path, L"r");
    if (test) {
        fclose(test);
        remove(config_path);
    }
    
    CreateDefaultConfig(config_path);
    
    extern void ReadNotificationMessagesConfig(void);
    ReadNotificationMessagesConfig();
    
    extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE);
    ExtractEmbeddedFontsToFolder(GetModuleHandle(NULL));
}

void ReloadDefaultFont(void) {
    extern BOOL LoadFontByNameAndGetRealName(HINSTANCE, const char*, char*, size_t);
    
    /* Use the default font name, not the current FONT_FILE_NAME */
    const char* defaultFontName = DEFAULT_FONT_NAME;
    
    /* Update FONT_FILE_NAME to default */
    snprintf(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), "%s%s", FONTS_PATH_PREFIX, defaultFontName);
    
    /* Load the default font */
    LoadFontByNameAndGetRealName(GetModuleHandle(NULL), defaultFontName, 
                                 FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
}

void RecalculateWindowSize(HWND hwnd) {
    extern float CLOCK_FONT_SCALE_FACTOR;
    
    CLOCK_WINDOW_SCALE = 1.0f;
    CLOCK_FONT_SCALE_FACTOR = 1.0f;
    
    HDC hdc = GetDC(hwnd);
    
    wchar_t fontNameW[256];
    MultiByteToWideChar(CP_UTF8, 0, FONT_INTERNAL_NAME, -1, fontNameW, 256);
    
    HFONT hFont = CreateFontW(
        -CLOCK_BASE_FONT_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, fontNameW
    );
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    char time_text[50];
    FormatTime(CLOCK_TOTAL_TIME, time_text);
    
    wchar_t time_textW[50];
    MultiByteToWideChar(CP_UTF8, 0, time_text, -1, time_textW, 50);
    
    SIZE textSize;
    GetTextExtentPoint32(hdc, time_textW, (int)wcslen(time_textW), &textSize);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hwnd, hdc);
    
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    float defaultScale = (screenHeight * 0.03f) / 20.0f;
    CLOCK_WINDOW_SCALE = defaultScale;
    CLOCK_FONT_SCALE_FACTOR = defaultScale;
    
    SetWindowPos(hwnd, NULL, 
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        (int)(textSize.cx * defaultScale), (int)(textSize.cy * defaultScale),
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

/* ============================================================================
 * Timeout Action Helpers
 * ============================================================================ */

void SetTimeoutAction(const char* action) {
    if (strcmp(action, "MESSAGE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    } else if (strcmp(action, "LOCK") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
    } else if (strcmp(action, "SHUTDOWN") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
    } else if (strcmp(action, "RESTART") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
    } else if (strcmp(action, "OPEN_FILE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    } else if (strcmp(action, "SHOW_TIME") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
    } else if (strcmp(action, "COUNT_UP") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
    } else if (strcmp(action, "OPEN_WEBSITE") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    } else if (strcmp(action, "SLEEP") == 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SLEEP;
    }
    WriteConfigTimeoutAction(action);
}

