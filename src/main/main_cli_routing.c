/**
 * @file main_cli_routing.c
 * @brief CLI command routing and forwarding implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include "main/main_cli_routing.h"
#include "config.h"
#include "timer/timer.h"
#include "window_procedure/window_procedure.h"
#include "utils/string_convert.h"
#include "../resource/resource.h"

typedef struct {
    const wchar_t* command;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
} CliCommandMapping;

/** Adding new commands only requires table entries */

static const CliCommandMapping SINGLE_CHAR_COMMANDS[] = {
    {L"s", WM_HOTKEY, HOTKEY_ID_SHOW_TIME, 0},
    {L"u", WM_HOTKEY, HOTKEY_ID_COUNT_UP, 0},
    {L"p", WM_HOTKEY, HOTKEY_ID_POMODORO, 0},
    {L"r", WM_HOTKEY, HOTKEY_ID_RESTART_TIMER, 0},
    {L"h", WM_APP_SHOW_CLI_HELP, 0, 0},
    {L"e", WM_COMMAND, CLOCK_IDC_EDIT_MODE, 0},
    {L"v", WM_COMMAND, CLOCK_IDC_TOGGLE_VISIBILITY, 0},
};

static const CliCommandMapping QUICK_COUNTDOWN_COMMANDS[] = {
    {L"q1", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN1, 0},
    {L"q2", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN2, 0},
    {L"q3", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN3, 0},
};

/** @return Pointer to trimmed string (within original buffer) */
static wchar_t* NormalizeWhitespace(wchar_t* str) {
    if (!str) return NULL;
    
    while (*str && iswspace(*str)) str++;
    
    size_t len = wcslen(str);
    while (len > 0 && iswspace(str[len - 1])) {
        str[--len] = L'\0';
    }
    
    return str;
}

static BOOL RouteSingleCharCommand(HWND hwnd, wchar_t cmd) {
    wchar_t cmdStr[2] = {cmd, L'\0'};
    
    for (size_t i = 0; i < sizeof(SINGLE_CHAR_COMMANDS) / sizeof(SINGLE_CHAR_COMMANDS[0]); i++) {
        if (wcscmp(cmdStr, SINGLE_CHAR_COMMANDS[i].command) == 0) {
            /* All commands use SendMessage for consistent behavior */
            SendMessage(hwnd, SINGLE_CHAR_COMMANDS[i].message, 
                       SINGLE_CHAR_COMMANDS[i].wParam, 
                       SINGLE_CHAR_COMMANDS[i].lParam);
            return TRUE;
        }
    }
    
    return FALSE;
}

static BOOL RouteTwoCharCommand(HWND hwnd, const wchar_t* cmdStr) {
    if (towlower(cmdStr[0]) == L'p' && towlower(cmdStr[1]) == L'r') {
        SendMessage(hwnd, WM_HOTKEY, HOTKEY_ID_PAUSE_RESUME, 0);
        return TRUE;
    }
    
    for (size_t i = 0; i < sizeof(QUICK_COUNTDOWN_COMMANDS) / sizeof(QUICK_COUNTDOWN_COMMANDS[0]); i++) {
        if (_wcsicmp(cmdStr, QUICK_COUNTDOWN_COMMANDS[i].command) == 0) {
            SendMessage(hwnd, QUICK_COUNTDOWN_COMMANDS[i].message,
                       QUICK_COUNTDOWN_COMMANDS[i].wParam,
                       QUICK_COUNTDOWN_COMMANDS[i].lParam);
            return TRUE;
        }
    }
    
    return FALSE;
}

static BOOL RoutePomodoroCommand(HWND hwnd, const wchar_t* cmdStr) {
    if (towlower(cmdStr[0]) != L'p' || !iswdigit(cmdStr[1])) {
        return FALSE;
    }
    
    wchar_t* endp = NULL;
    long idx = wcstol(cmdStr + 1, &endp, 10);
    
    if (idx > 0 && (endp == NULL || *endp == L'\0')) {
        SendMessage(hwnd, WM_APP_QUICK_COUNTDOWN_INDEX, 0, (LPARAM)idx);
    } else {
        SendMessage(hwnd, WM_HOTKEY, HOTKEY_ID_COUNTDOWN, 0);
    }
    
    return TRUE;
}

static BOOL ForwardTimerInput(HWND hwnd, const wchar_t* cmdStr) {
    char* utf8Str = WideToUtf8Alloc(cmdStr);
    if (!utf8Str) return FALSE;
    
    COPYDATASTRUCT cds;
    cds.dwData = COPYDATA_ID_CLI_TEXT;
    cds.cbData = (DWORD)(strlen(utf8Str) + 1);
    cds.lpData = utf8Str;
    
    SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    free(utf8Str);
    
    return TRUE;
}

BOOL TryForwardSimpleCliToExisting(HWND hwndExisting, const wchar_t* lpCmdLine) {
    if (!lpCmdLine || lpCmdLine[0] == L'\0') return FALSE;
    
    wchar_t buf[256];
    wcsncpy(buf, lpCmdLine, sizeof(buf)/sizeof(wchar_t) - 1);
    buf[sizeof(buf)/sizeof(wchar_t) - 1] = L'\0';
    
    wchar_t* cmd = NormalizeWhitespace(buf);
    if (!cmd || *cmd == L'\0') return FALSE;
    
    size_t len = wcslen(cmd);
    
    if (len == 1) {
        if (RouteSingleCharCommand(hwndExisting, towlower(cmd[0]))) {
            return TRUE;
        }
        /* Unknown single char - forward to existing instance for handling */
        return ForwardTimerInput(hwndExisting, cmd);
    }
    
    if (len == 2) {
        if (RouteTwoCharCommand(hwndExisting, cmd)) {
            return TRUE;
        }
        /* Unknown two char - check if it's a pomodoro command or forward */
    }
    
    if (RoutePomodoroCommand(hwndExisting, cmd)) {
        return TRUE;
    }
    
    /* Forward all other commands (including time input and unknown commands)
     * to the existing instance via WM_COPYDATA. This ensures:
     * 1. Time inputs like "25m", "1h30m" are handled
     * 2. Unknown commands are handled by cli.c (shows default countdown dialog)
     * 3. Only one instance is ever running */
    return ForwardTimerInput(hwndExisting, cmd);
}

