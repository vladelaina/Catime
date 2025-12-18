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
#include "drag_scale.h"
#include "log.h"

extern BOOL CLOCK_WINDOW_TOPMOST;
extern void SetWindowTopmost(HWND hwnd, BOOL topmost);

#define INPUT_BUFFER_SIZE 256



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

/**
 * @brief Parse time input and start countdown
 * 
 * This is the only command processing needed in cli.c because:
 * - Simple commands (s, u, p, r, h, e, v, pr, q1-q3) are handled by main_cli_routing.c
 *   which sends WM_HOTKEY or WM_COMMAND messages directly to the existing instance
 * - Only time inputs (like "25m", "1h30m") and unknown commands are forwarded here
 *   via WM_COPYDATA
 */

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
        return TRUE;
    }
    
    NormalizeWhitespace(input);
    
    /* Parse time input and start countdown */
    int totalSeconds = 0;
    if (ParseInput(input, &totalSeconds)) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, totalSeconds);
    } else {
        /* Unknown input - show default countdown dialog */
        StartDefaultCountDown(hwnd);
    }
    
    return TRUE;
}
