/**
 * @file cli.c
 * @brief Command-line interface parsing and help dialog management
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "../include/timer.h"
#include "../include/window.h"
#include "../include/window_procedure.h"
#include "../resource/resource.h"
#include "../include/notification.h"
#include "../include/audio_player.h"

extern int elapsed_time;
extern int message_shown;

/** @brief Global handle for CLI help dialog window */
static HWND g_cliHelpDialog = NULL;
/**
 * @brief Force dialog to foreground with aggressive focus stealing
 * @param hwndDialog Dialog window to bring to front
 * Handles Windows focus restrictions by thread attachment and topmost tricks
 */
static void ForceForegroundAndFocus(HWND hwndDialog) {
	HWND hwndFore = GetForegroundWindow();
	DWORD foreThread = hwndFore ? GetWindowThreadProcessId(hwndFore, NULL) : 0;
	DWORD curThread = GetCurrentThreadId();
	/** Attach to foreground thread to bypass focus restrictions */
	if (foreThread && foreThread != curThread) {
		AttachThreadInput(foreThread, curThread, TRUE);
	}
	AllowSetForegroundWindow(ASFW_ANY);
	BringWindowToTop(hwndDialog);
	SetForegroundWindow(hwndDialog);
	SetActiveWindow(hwndDialog);
	/** Topmost trick if normal focus failed */
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

/**
 * @brief Dialog procedure for CLI help window
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message handling result
 */
static INT_PTR CALLBACK CliHelpDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		SendMessage(hwndDlg, DM_SETDEFID, (WPARAM)IDOK, 0);
		HWND hOk = GetDlgItem(hwndDlg, IDOK);
		if (hOk) SetFocus(hOk);
		return FALSE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hwndDlg);
			return TRUE;
		}
		break;
	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
            DestroyWindow(hwndDlg);
			return TRUE;
		}
		break;
	case WM_CHAR:
		if (wParam == VK_RETURN) {
            DestroyWindow(hwndDlg);
			return TRUE;
		}
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_CLOSE) {
			DestroyWindow(hwndDlg);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return TRUE;
	case WM_DESTROY:
		/** Restore main window topmost state when help closes */
		if (hwndDlg == g_cliHelpDialog) {
			g_cliHelpDialog = NULL;
			extern BOOL CLOCK_WINDOW_TOPMOST;
			extern void SetWindowTopmost(HWND hwnd, BOOL topmost);
			HWND hMainWnd = GetParent(hwndDlg);
			if (hMainWnd && CLOCK_WINDOW_TOPMOST) {
				SetWindowTopmost(hMainWnd, TRUE);
			}
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Show or toggle CLI help dialog window
 * @param hwnd Parent window handle
 */
void ShowCliHelpDialog(HWND hwnd) {
	if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
		/** Toggle: close if already open */
		DestroyWindow(g_cliHelpDialog);
		g_cliHelpDialog = NULL;
	} else {
		g_cliHelpDialog = CreateDialogParamW(GetModuleHandleW(NULL), MAKEINTRESOURCE(IDD_CLI_HELP_DIALOG), hwnd, CliHelpDlgProc, 0);
		if (g_cliHelpDialog) {
			ShowWindow(g_cliHelpDialog, SW_SHOW);
			ForceForegroundAndFocus(g_cliHelpDialog);
		}
	}
}

/**
 * @brief Remove leading and trailing whitespace from string
 * @param s String to trim in-place
 */
static void trimSpaces(char* s) {
	if (!s) return;
	char* p = s;
	while (*p && isspace((unsigned char)*p)) p++;
	if (p != s) memmove(s, p, strlen(p) + 1);
	size_t len = strlen(s);
	while (len > 0 && isspace((unsigned char)s[len - 1])) {
		s[--len] = '\0';
	}
}

/**
 * @brief Expand compact target time format (e.g., "130t" → "1 30T")
 * @param s String to expand in-place
 * Handles 3-digit (HMM) and 4-digit (HHMM) compact formats
 */
static void expandCompactTargetTime(char* s) {
	if (!s) return;
	size_t len = strlen(s);
	if (len >= 2 && (s[len - 1] == 't' || s[len - 1] == 'T')) {
		s[len - 1] = '\0';
		trimSpaces(s);
		size_t dlen = strlen(s);
		int allDigits = 1;
		for (size_t i = 0; i < dlen; ++i) {
			if (!isdigit((unsigned char)s[i])) { allDigits = 0; break; }
		}
		/** Convert compact formats: 130t → "1 30T", 1030t → "10 30T" */
		if (allDigits && (dlen == 3 || dlen == 4)) {
			char buf[64];
			if (dlen == 3) {
				buf[0] = s[0]; buf[1] = '\0';
				int mm = atoi(s + 1);
				char out[64];
				snprintf(out, sizeof(out), "%s %dT", buf, mm);
				strncpy(s, out, 255); s[255] = '\0';
            } else {
                char hh[8] = {0};
                char mm[8] = {0};
                strncpy(hh, s, 2);
                hh[2] = '\0';
                strncpy(mm, s + 2, sizeof(mm) - 1);
                mm[sizeof(mm) - 1] = '\0';
                char out[64];
                snprintf(out, sizeof(out), "%s %sT", hh, mm);
                strncpy(s, out, 255); s[255] = '\0';
            }
		} else {
			/** Restore 't' if pattern didn't match */
			s[len - 1] = 't';
		}
	}
}

/**
 * @brief Expand compact hour-minute format (e.g., "130 45" → "1 30 45")
 * @param s String to expand in-place
 * Converts 3-digit hour-minute followed by seconds
 */
static void expandCompactHourMinutePlusSecond(char* s) {
	if (!s) return;
	char copy[256];
	strncpy(copy, s, sizeof(copy) - 1);
	copy[sizeof(copy) - 1] = '\0';

	char* tok1 = strtok(copy, " ");
	if (!tok1) return;
	char* tok2 = strtok(NULL, " ");
	char* tok3 = strtok(NULL, " ");
	if (!tok2 || tok3) return;

	/** Pattern: "HMM SS" → "H MM SS" */
	if (strlen(tok1) == 3) {
		if (isdigit((unsigned char)tok1[0]) && isdigit((unsigned char)tok1[1]) && isdigit((unsigned char)tok1[2])) {
			int hour = tok1[0] - '0';
			int minute = (tok1[1] - '0') * 10 + (tok1[2] - '0');
			for (const char* p = tok2; *p; ++p) { if (!isdigit((unsigned char)*p)) return; }
			char out[256];
			snprintf(out, sizeof(out), "%d %d %s", hour, minute, tok2);
			strncpy(s, out, 255); s[255] = '\0';
		}
	}
}

/**
 * @brief Parse and execute command-line timer arguments
 * @param hwnd Main window handle
 * @param cmdLine Command line string to parse
 * @return TRUE if command was recognized and executed
 */
BOOL HandleCliArguments(HWND hwnd, const char* cmdLine) {
	if (!cmdLine || !*cmdLine) return FALSE;

	char input[256];
	strncpy(input, cmdLine, sizeof(input) - 1);
	input[sizeof(input) - 1] = '\0';
	trimSpaces(input);
	if (input[0] == '\0') return FALSE;

    /** Handle predefined command shortcuts */
    {
        if (_stricmp(input, "q1") == 0) {
            StartQuickCountdown1(hwnd);
            return TRUE;
        }
        if (_stricmp(input, "q2") == 0) {
            StartQuickCountdown2(hwnd);
            return TRUE;
        }
        if (_stricmp(input, "q3") == 0) {
            StartQuickCountdown3(hwnd);
            return TRUE;
        }

        if (_stricmp(input, "v") == 0) {
            if (IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            return TRUE;
        }

        if (_stricmp(input, "e") == 0) {
            extern void StartEditMode(HWND hwnd);
            StartEditMode(hwnd);
            return TRUE;
        }

        if (_stricmp(input, "pr") == 0) {
            TogglePauseResume(hwnd);
            return TRUE;
        }

        if (_stricmp(input, "r") == 0) {
            CloseAllNotifications();
            RestartCurrentTimer(hwnd);
            return TRUE;
        }

        /** Handle pomodoro with optional index (p1, p2, etc.) */
        if ((input[0] == 'p' || input[0] == 'P') && isdigit((unsigned char)input[1])) {
            long val = 0;
            const char* num = input + 1;
            char* endp = NULL;
            val = strtol(num, &endp, 10);
            if (val > 0 && (endp == NULL || *endp == '\0')) {
                extern void StartQuickCountdownByIndex(HWND hwnd, int index);
                StartQuickCountdownByIndex(hwnd, (int)val);
                return TRUE;
            } else {
                StartDefaultCountDown(hwnd);
                return TRUE;
            }
        }
    }

    /** Handle single-character commands */
    if (input[1] == '\0') {
        char c = (char)tolower((unsigned char)input[0]);
        if (c == 's') {
            PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_SHOW_TIME, 0);
            return TRUE;
        } else if (c == 'u') {
            PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_COUNT_UP, 0);
            return TRUE;
		} else if (c == 'p') {
            PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_POMODORO, 0);
            return TRUE;
		} else if (c == 'h') {
			PostMessage(hwnd, WM_APP_SHOW_CLI_HELP, 0, 0);
			return TRUE;
        }
    }

	/** Normalize whitespace for consistent parsing */
	{
		char norm[256]; size_t j = 0; int inSpace = 0;
		for (size_t i = 0; input[i] && j < sizeof(norm) - 1; ++i) {
			if (isspace((unsigned char)input[i])) {
				if (!inSpace) { norm[j++] = ' '; inSpace = 1; }
			} else { norm[j++] = input[i]; inSpace = 0; }
		}
		norm[j] = '\0';
		strncpy(input, norm, sizeof(input) - 1); input[sizeof(input) - 1] = '\0';
	}

	/** Apply input format expansions */
	expandCompactTargetTime(input);
	expandCompactHourMinutePlusSecond(input);

	/** Parse as timer duration and start countdown */
	int total_seconds = 0;
    if (!ParseInput(input, &total_seconds)) {
        StartDefaultCountDown(hwnd);
        return TRUE;
    }
	StopNotificationSound();
	CloseAllNotifications();

	/** Initialize new countdown timer */
	KillTimer(hwnd, 1);
	CLOCK_TOTAL_TIME = total_seconds;
	countdown_elapsed_time = 0;
	elapsed_time = 0;
	message_shown = FALSE;
	countdown_message_shown = FALSE;
	CLOCK_COUNT_UP = FALSE;
	CLOCK_SHOW_CURRENT_TIME = FALSE;
	CLOCK_IS_PAUSED = FALSE;
	SetTimer(hwnd, 1, 1000, NULL);
	InvalidateRect(hwnd, NULL, TRUE);

	return TRUE;
}

/**
 * @brief Get handle to CLI help dialog if open
 * @return Dialog window handle or NULL
 */
HWND GetCliHelpDialog(void) {
    return g_cliHelpDialog;
}

/**
 * @brief Close CLI help dialog if open
 */
void CloseCliHelpDialog(void) {
    if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
        DestroyWindow(g_cliHelpDialog);
        g_cliHelpDialog = NULL;
    }
}