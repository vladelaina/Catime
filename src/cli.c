/**
 * @file cli.c
 * @brief CLI parser with multiple input formats
 *
 * Supports natural time input: "25m", "1h30m", "1 30" (minutes:seconds), "14 30t" (absolute time).
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
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "drag_scale.h"
#include "language.h"
#include "log.h"

extern BOOL CLOCK_WINDOW_TOPMOST;

#define INPUT_BUFFER_SIZE 256
#define CLI_HELP_TEXT_BUFFER_SIZE 2048



static HWND g_cliHelpDialog = NULL;

static void LocalizeCliHelpDialog(HWND hwndDlg) {
    SetWindowTextW(hwndDlg,
                   GetLocalizedString(NULL, L"CliHelpTitle"));
    SetDlgItemTextW(hwndDlg, IDOK,
                    GetLocalizedString(NULL, L"OK"));

    const wchar_t* localized =
        GetLocalizedString(NULL, L"CliHelpContent");
    wchar_t normalized[CLI_HELP_TEXT_BUFFER_SIZE] = {0};
    size_t output = 0;
    wchar_t previous = L'\0';

    while (localized && *localized && output + 1 < _countof(normalized)) {
        if (*localized == L'\n' && previous != L'\r') {
            if (output + 2 >= _countof(normalized)) break;
            normalized[output++] = L'\r';
        }
        normalized[output++] = *localized;
        previous = *localized++;
    }
    normalized[output] = L'\0';
    SetDlgItemTextW(hwndDlg, IDC_CLI_HELP_EDIT, normalized);
}

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
    
    Dialog_EnsureWindowVisible(hwndDialog);
    AllowSetForegroundWindow(ASFW_ANY);
    BringWindowToTop(hwndDialog);
    SetForegroundWindow(hwndDialog);
    SetActiveWindow(hwndDialog);
    
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
            Dialog_InitializeInstance(DIALOG_INSTANCE_CLI_HELP, hwndDlg);
            LocalizeCliHelpDialog(hwndDlg);
            SendMessage(hwndDlg, DM_SETDEFID, (WPARAM)IDOK, 0);
            HWND hOk = GetDlgItem(hwndDlg, IDOK);
            if (hOk) SetFocus(hOk);
            MoveDialogToPrimaryScreen(hwndDlg);
            return FALSE;
        }
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
        
        case WM_DESTROY: {
            HWND hMainWnd = Dialog_GetOwnerWindow(hwndDlg);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_CLI_HELP, hwndDlg);
            if (hwndDlg == g_cliHelpDialog) {
                g_cliHelpDialog = NULL;
                if (hMainWnd && CLOCK_WINDOW_TOPMOST) {
                    RefreshWindowTopmostState(hMainWnd);
                }
            }
            return TRUE;
        }
    }
    
    if (ShouldCloseHelpDialog(msg, wParam)) {
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    
    return FALSE;
}

void ShowCliHelpDialog(HWND hwndParent) {
    if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
        DestroyWindow(g_cliHelpDialog);
        g_cliHelpDialog = NULL;
    } else {
        g_cliHelpDialog = CreateDialogParamW(
            GetModuleHandleW(NULL), 
            MAKEINTRESOURCE(IDD_CLI_HELP_DIALOG), 
            hwndParent,
            CliHelpDlgProc, 
            0
        );
        if (g_cliHelpDialog) {
            DialogModern_ShowPaintedWindow(g_cliHelpDialog, SW_SHOW);
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

typedef struct {
    const char* command;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
} CliShortcut;

static const CliShortcut CLI_SHORTCUTS[] = {
    {"s",  WM_HOTKEY, HOTKEY_ID_SHOW_TIME, 0},
    {"u",  WM_HOTKEY, HOTKEY_ID_COUNT_UP, 0},
    {"p",  WM_HOTKEY, HOTKEY_ID_POMODORO, 0},
    {"r",  WM_HOTKEY, HOTKEY_ID_RESTART_TIMER, 0},
    {"h",  WM_APP_SHOW_CLI_HELP, 0, 0},
    {"e",  WM_COMMAND, CLOCK_IDC_EDIT_MODE, 0},
    {"v",  WM_COMMAND, CLOCK_IDC_TOGGLE_VISIBILITY, 0},
    {"pr", WM_HOTKEY, HOTKEY_ID_PAUSE_RESUME, 0},
    {"q1", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN1, 0},
    {"q2", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN2, 0},
    {"q3", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN3, 0},
};

static BOOL HandleShortcut(HWND hwnd, const char* input) {
    if (!hwnd || !input) return FALSE;

    for (size_t i = 0; i < _countof(CLI_SHORTCUTS); ++i) {
        if (_stricmp(input, CLI_SHORTCUTS[i].command) == 0) {
            SendMessageW(hwnd, CLI_SHORTCUTS[i].message,
                         CLI_SHORTCUTS[i].wParam, CLI_SHORTCUTS[i].lParam);
            return TRUE;
        }
    }

    if (tolower((unsigned char)input[0]) == 'p' && input[1] != '\0') {
        char* end = NULL;
        long index = strtol(input + 1, &end, 10);
        if (index > 0 && end && *end == '\0') {
            SendMessageW(hwnd, WM_APP_QUICK_COUNTDOWN_INDEX, 0,
                         (LPARAM)index);
            return TRUE;
        }
    }

    return FALSE;
}

/** Parse time input and start countdown. */

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
    
    /* Ignore command-line flags (double-dash options) */
    if (input[0] == '-' && input[1] == '-') {
        LOG_INFO("Ignoring command-line flag: %s", input);
        return FALSE;
    }
    
    NormalizeWhitespace(input);

    if (HandleShortcut(hwnd, input)) {
        return TRUE;
    }

    /* Parse time input and start countdown */
    int totalSeconds = 0;
    if (ParseInput(input, &totalSeconds)) {
        CleanupBeforeTimerAction(hwnd);
        StartCountdownWithTime(hwnd, totalSeconds);
    } else {
        /* Unknown input - show default countdown dialog */
        StartDefaultCountDown(hwnd);
    }
    
    return TRUE;
}
