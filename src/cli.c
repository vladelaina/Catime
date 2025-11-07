/**
 * @file cli.c
 * @brief CLI parser with multiple input formats
 * 
 * Input expansion: "130t" → "1 30T", "130 45" → "1 30 45" (compact formats).
 * Aggressive focus stealing for help dialog (Windows fails topmost focus).
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "timer/timer.h"
#include "timer/timer_events.h"
#include "window.h"
#include "window_procedure/window_procedure.h"
#include "../resource/resource.h"
#include "notification.h"
#include "audio_player.h"
#include "dialog/dialog_procedure.h"
#include "drag_scale.h"

extern BOOL CLOCK_WINDOW_TOPMOST;
extern void SetWindowTopmost(HWND hwnd, BOOL topmost);

#define CMD_QUICK_1       "q1"
#define CMD_QUICK_2       "q2"
#define CMD_QUICK_3       "q3"
#define CMD_VISIBILITY    "v"
#define CMD_EDIT_MODE     "e"
#define CMD_PAUSE_RESUME  "pr"
#define CMD_RESTART       "r"
#define CMD_SHOW_TIME     's'
#define CMD_COUNT_UP      'u'
#define CMD_POMODORO      'p'
#define CMD_HELP          'h'

#define INPUT_BUFFER_SIZE 256
#define EXPAND_BUFFER_SIZE 64

typedef BOOL (*CommandHandler)(HWND hwnd, const char* input);

typedef struct {
    const char* command;
    CommandHandler handler;
} CommandEntry;

typedef struct {
    char command;
    UINT messageId;
} SingleCharCommand;

static HWND g_cliHelpDialog = NULL;

static BOOL ShouldCloseHelpDialog(UINT msg, WPARAM wParam) {
    switch (msg) {
        case WM_COMMAND:
            return (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL);
        case WM_KEYDOWN:
        case WM_CHAR:
            return (wParam == VK_RETURN);
        case WM_SYSCOMMAND:
            return ((wParam & 0xFFF0) == SC_CLOSE);
        case WM_CLOSE:
            return TRUE;
        default:
            return FALSE;
    }
}

/** Aggressive focus stealing (Windows fails topmost window focus) */
static void ForceForegroundAndFocus(HWND hwndDialog) {
    HWND hwndFore = GetForegroundWindow();
    DWORD foreThread = hwndFore ? GetWindowThreadProcessId(hwndFore, NULL) : 0;
    DWORD curThread = GetCurrentThreadId();
    
    if (foreThread && foreThread != curThread) {
        AttachThreadInput(foreThread, curThread, TRUE);
    }
    
    AllowSetForegroundWindow(ASFW_ANY);
    BringWindowToTop(hwndDialog);
    SetForegroundWindow(hwndDialog);
    SetActiveWindow(hwndDialog);
    
    if (GetForegroundWindow() != hwndDialog) {
        SetWindowPos(hwndDialog, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetWindowPos(hwndDialog, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    
    HWND hOk = GetDlgItem(hwndDialog, IDOK);
    if (hOk) SetFocus(hOk);
    
    if (foreThread && foreThread != curThread) {
        AttachThreadInput(foreThread, curThread, FALSE);
    }
}

static INT_PTR CALLBACK CliHelpDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    
    switch (msg) {
        case WM_INITDIALOG: {
            SendMessage(hwndDlg, DM_SETDEFID, (WPARAM)IDOK, 0);
            HWND hOk = GetDlgItem(hwndDlg, IDOK);
            if (hOk) SetFocus(hOk);
            MoveDialogToPrimaryScreen(hwndDlg);
            return FALSE;
        }
        
        case WM_DESTROY:
            if (hwndDlg == g_cliHelpDialog) {
                g_cliHelpDialog = NULL;
                HWND hMainWnd = GetParent(hwndDlg);
                if (hMainWnd && CLOCK_WINDOW_TOPMOST) {
                    SetWindowTopmost(hMainWnd, TRUE);
                }
            }
            return TRUE;
    }
    
    if (ShouldCloseHelpDialog(msg, wParam)) {
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    
    return FALSE;
}

void ShowCliHelpDialog(HWND hwnd) {
    if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
        DestroyWindow(g_cliHelpDialog);
        g_cliHelpDialog = NULL;
    } else {
        g_cliHelpDialog = CreateDialogParamW(
            GetModuleHandleW(NULL), 
            MAKEINTRESOURCE(IDD_CLI_HELP_DIALOG), 
            hwnd, 
            CliHelpDlgProc, 
            0
        );
        if (g_cliHelpDialog) {
            ShowWindow(g_cliHelpDialog, SW_SHOW);
            ForceForegroundAndFocus(g_cliHelpDialog);
        }
    }
}

HWND GetCliHelpDialog(void) {
    return g_cliHelpDialog;
}

void CloseCliHelpDialog(void) {
    if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
        DestroyWindow(g_cliHelpDialog);
        g_cliHelpDialog = NULL;
    }
}

static void TrimSpaces(char* s) {
    if (!s) return;
    
    char* p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

/** Collapse multiple spaces to one */
static void NormalizeWhitespace(char* input) {
    if (!input) return;
    
    char normalized[INPUT_BUFFER_SIZE];
    size_t j = 0;
    BOOL inSpace = FALSE;
    
    for (size_t i = 0; input[i] && j < sizeof(normalized) - 1; ++i) {
        if (isspace((unsigned char)input[i])) {
            if (!inSpace) {
                normalized[j++] = ' ';
                inSpace = TRUE;
            }
        } else {
            normalized[j++] = input[i];
            inSpace = FALSE;
        }
    }
    normalized[j] = '\0';
    
    strncpy(input, normalized, INPUT_BUFFER_SIZE - 1);
    input[INPUT_BUFFER_SIZE - 1] = '\0';
}

/** Expand compact target time: "130t" → "1 30T" */
static void ExpandCompactTargetTime(char* s) {
    if (!s) return;
    
    size_t len = strlen(s);
    if (len < 2 || (s[len - 1] != 't' && s[len - 1] != 'T')) {
        return;
    }
    
    s[len - 1] = '\0';
    TrimSpaces(s);
    size_t digitLen = strlen(s);
    
    for (size_t i = 0; i < digitLen; ++i) {
        if (!isdigit((unsigned char)s[i])) {
            s[len - 1] = 't';
            return;
        }
    }
    
    if (digitLen == 3 || digitLen == 4) {
        char expanded[EXPAND_BUFFER_SIZE];
        if (digitLen == 3) {
            int hour = s[0] - '0';
            int minute = atoi(s + 1);
            snprintf(expanded, sizeof(expanded), "%d %dT", hour, minute);
        } else {
            char hourStr[8];
            strncpy(hourStr, s, 2);
            hourStr[2] = '\0';
            snprintf(expanded, sizeof(expanded), "%s %sT", hourStr, s + 2);
        }
        strncpy(s, expanded, INPUT_BUFFER_SIZE - 1);
        s[INPUT_BUFFER_SIZE - 1] = '\0';
    } else {
        s[len - 1] = 't';
    }
}

/** Expand compact hour-minute: "130 45" → "1 30 45" */
static void ExpandCompactHourMinute(char* s) {
    if (!s) return;
    
    char copy[INPUT_BUFFER_SIZE];
    strncpy(copy, s, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    
    char* tok1 = strtok(copy, " ");
    if (!tok1) return;
    
    char* tok2 = strtok(NULL, " ");
    if (!tok2) return;
    
    char* tok3 = strtok(NULL, " ");
    if (tok3) return;
    
    if (strlen(tok1) == 3) {
        if (isdigit((unsigned char)tok1[0]) && 
            isdigit((unsigned char)tok1[1]) && 
            isdigit((unsigned char)tok1[2])) {
            
            for (const char* p = tok2; *p; ++p) {
                if (!isdigit((unsigned char)*p)) return;
            }
            
            int hour = tok1[0] - '0';
            int minute = (tok1[1] - '0') * 10 + (tok1[2] - '0');
            
            char expanded[INPUT_BUFFER_SIZE];
            snprintf(expanded, sizeof(expanded), "%d %d %s", hour, minute, tok2);
            strncpy(s, expanded, INPUT_BUFFER_SIZE - 1);
            s[INPUT_BUFFER_SIZE - 1] = '\0';
        }
    }
}

static BOOL HandleQuickCountdown(HWND hwnd, const char* input) {
    int index = atoi(input + 1);
    if (index > 0 && index <= 3) {
        StartQuickCountdownByIndex(hwnd, index);
        return TRUE;
    }
    return FALSE;
}

static BOOL HandleVisibility(HWND hwnd, const char* input) {
    (void)input;
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
    return TRUE;
}

static BOOL HandleEditMode(HWND hwnd, const char* input) {
    (void)input;
    StartEditMode(hwnd);
    return TRUE;
}

static BOOL HandlePauseResume(HWND hwnd, const char* input) {
    (void)input;
    TogglePauseResume(hwnd);
    return TRUE;
}

static BOOL HandleRestart(HWND hwnd, const char* input) {
    (void)input;
    CloseAllNotifications();
    RestartCurrentTimer(hwnd);
    return TRUE;
}

static BOOL HandlePomodoroIndex(HWND hwnd, const char* input) {
    if (strlen(input) < 2 || (input[0] != 'p' && input[0] != 'P')) {
        return FALSE;
    }
    
    if (!isdigit((unsigned char)input[1])) {
        return FALSE;
    }
    
    char* endPtr = NULL;
    long index = strtol(input + 1, &endPtr, 10);
    
    if (index > 0 && (endPtr == NULL || *endPtr == '\0')) {
        StartQuickCountdownByIndex(hwnd, (int)index);
        return TRUE;
    }
    
    StartDefaultCountDown(hwnd);
    return TRUE;
}

static const CommandEntry g_commandTable[] = {
    {CMD_QUICK_1,      HandleQuickCountdown},
    {CMD_QUICK_2,      HandleQuickCountdown},
    {CMD_QUICK_3,      HandleQuickCountdown},
    {CMD_VISIBILITY,   HandleVisibility},
    {CMD_EDIT_MODE,    HandleEditMode},
    {CMD_PAUSE_RESUME, HandlePauseResume},
    {CMD_RESTART,      HandleRestart},
    {NULL,             NULL}
};

static const SingleCharCommand g_singleCharCommands[] = {
    {CMD_SHOW_TIME, HOTKEY_ID_SHOW_TIME},
    {CMD_COUNT_UP,  HOTKEY_ID_COUNT_UP},
    {CMD_POMODORO,  HOTKEY_ID_POMODORO},
    {CMD_HELP,      WM_APP_SHOW_CLI_HELP},
    {'\0',          0}
};

static BOOL ProcessShortcutCommands(HWND hwnd, const char* input) {
    if ((input[0] == 'p' || input[0] == 'P') && isdigit((unsigned char)input[1])) {
        return HandlePomodoroIndex(hwnd, input);
    }
    
    for (const CommandEntry* cmd = g_commandTable; cmd->command; cmd++) {
        if (_stricmp(input, cmd->command) == 0) {
            return cmd->handler(hwnd, input);
        }
    }
    
    return FALSE;
}

static BOOL ProcessSingleCharCommands(HWND hwnd, const char* input) {
    if (input[0] == '\0' || input[1] != '\0') {
        return FALSE;
    }
    
    char c = (char)tolower((unsigned char)input[0]);
    
    for (const SingleCharCommand* cmd = g_singleCharCommands; cmd->command; cmd++) {
        if (c == cmd->command) {
            PostMessage(hwnd, WM_HOTKEY, cmd->messageId, 0);
            return TRUE;
        }
    }
    
    return FALSE;
}

static BOOL ParseAndStartTimer(HWND hwnd, char* input) {
    ExpandCompactTargetTime(input);
    ExpandCompactHourMinute(input);
    
    int totalSeconds = 0;
    if (!ParseInput(input, &totalSeconds)) {
        StartDefaultCountDown(hwnd);
        return TRUE;
    }
    
    CleanupBeforeTimerAction();
    StartCountdownWithTime(hwnd, totalSeconds);
    return TRUE;
}

BOOL HandleCliArguments(HWND hwnd, const char* cmdLine) {
    if (!cmdLine || !*cmdLine) {
        return FALSE;
    }
    
    char input[INPUT_BUFFER_SIZE];
    strncpy(input, cmdLine, sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';
    TrimSpaces(input);
    
    if (input[0] == '\0') {
        return FALSE;
    }
    
    if (ProcessShortcutCommands(hwnd, input)) {
        return TRUE;
    }
    
    if (ProcessSingleCharCommands(hwnd, input)) {
        return TRUE;
    }
    
    NormalizeWhitespace(input);
    return ParseAndStartTimer(hwnd, input);
}
