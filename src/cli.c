/**
 * @file cli.c
 * @brief Command line helper to parse countdown arguments and start timer
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

// Variables defined in main.c
extern int elapsed_time;
extern int message_shown;

static HWND g_cliHelpDialog = NULL;
static void ForceForegroundAndFocus(HWND hwndDialog) {
	HWND hwndFore = GetForegroundWindow();
	DWORD foreThread = hwndFore ? GetWindowThreadProcessId(hwndFore, NULL) : 0;
	DWORD curThread = GetCurrentThreadId();
	if (foreThread && foreThread != curThread) {
		AttachThreadInput(foreThread, curThread, TRUE);
	}
	// Try allow foreground switch (best-effort)
	AllowSetForegroundWindow(ASFW_ANY);
	// Toggle topmost to bring to front reliably
	SetWindowPos(hwndDialog, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetWindowPos(hwndDialog, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
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
	switch (msg) {
	case WM_INITDIALOG:
		// Set OK as default and focus so Enter will close immediately even if not clicked first
		SendMessage(hwndDlg, DM_SETDEFID, (WPARAM)IDOK, 0);
		HWND hOk = GetDlgItem(hwndDlg, IDOK);
		if (hOk) SetFocus(hOk);
		return FALSE; // allow dialog manager to set focus since we set it explicitly
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
		if (hwndDlg == g_cliHelpDialog) {
			g_cliHelpDialog = NULL;
		}
		return TRUE;
	}
	return FALSE;
}

void ShowCliHelpDialog(HWND hwnd) {
	// Toggle help dialog: close if already open, show if closed
	if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
		// Dialog is already open, close it
		DestroyWindow(g_cliHelpDialog);
		g_cliHelpDialog = NULL;
	} else {
		// Dialog is not open, create and show it
		g_cliHelpDialog = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CLI_HELP_DIALOG), hwnd, CliHelpDlgProc, 0);
		if (g_cliHelpDialog) {
			ShowWindow(g_cliHelpDialog, SW_SHOW);
			ForceForegroundAndFocus(g_cliHelpDialog);
		}
	}
}

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

// Convert compact HHMMt -> "HH MMt" (e.g., 1720t -> "17 20t")
static void expandCompactTargetTime(char* s) {
	if (!s) return;
	size_t len = strlen(s);
	if (len >= 2 && (s[len - 1] == 't' || s[len - 1] == 'T')) {
		// find last non-space before t
		s[len - 1] = '\0';
		trimSpaces(s);
		// now s is digits possibly with spaces; only handle pure digits of length 3-4
		size_t dlen = strlen(s);
		int allDigits = 1;
		for (size_t i = 0; i < dlen; ++i) {
			if (!isdigit((unsigned char)s[i])) { allDigits = 0; break; }
		}
		if (allDigits && (dlen == 3 || dlen == 4)) {
			// HHMM or HMM -> split hour/min
			char buf[64];
			if (dlen == 3) {
				// HMM -> H MM
				buf[0] = s[0]; buf[1] = '\0';
				int mm = atoi(s + 1);
				char out[64];
				snprintf(out, sizeof(out), "%s %dT", buf, mm);
				strncpy(s, out, 255); s[255] = '\0';
            } else {
                // HHMM -> HH MM
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
			// restore trailing t
			s[len - 1] = 't';
		}
	}
}

// Convert patterns like "130 20" into "1 30 20" only when the first token is 3 digits (HMM)
static void expandCompactHourMinutePlusSecond(char* s) {
	if (!s) return;
	// copy to buffer to tokenize safely
	char copy[256];
	strncpy(copy, s, sizeof(copy) - 1);
	copy[sizeof(copy) - 1] = '\0';

	char* tok1 = strtok(copy, " ");
	if (!tok1) return;
	char* tok2 = strtok(NULL, " ");
	char* tok3 = strtok(NULL, " ");
	if (!tok2 || tok3) return; // only handle exactly two tokens

	// Only when tok1 is 3 digits and tok2 is digits-only (seconds)
	if (strlen(tok1) == 3) {
		if (isdigit((unsigned char)tok1[0]) && isdigit((unsigned char)tok1[1]) && isdigit((unsigned char)tok1[2])) {
			int hour = tok1[0] - '0';
			int minute = (tok1[1] - '0') * 10 + (tok1[2] - '0');
			// verify tok2 is digits
			for (const char* p = tok2; *p; ++p) { if (!isdigit((unsigned char)*p)) return; }
			char out[256];
			snprintf(out, sizeof(out), "%d %d %s", hour, minute, tok2);
			strncpy(s, out, 255); s[255] = '\0';
		}
	}
}

BOOL HandleCliArguments(HWND hwnd, const char* cmdLine) {
	if (!cmdLine || !*cmdLine) return FALSE;

	char input[256];
	strncpy(input, cmdLine, sizeof(input) - 1);
	input[sizeof(input) - 1] = '\0';
	trimSpaces(input);
	if (input[0] == '\0') return FALSE;

    // Abbreviated commands (case-insensitive for ASCII)
    {
        // Quick countdown 1/2/3
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

        // Hide/Show window (toggle visibility)
        if (_stricmp(input, "v") == 0) {
            if (IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            return TRUE;
        }

        // Enter edit mode (use specific enter rather than toggle)
        if (_stricmp(input, "e") == 0) {
            extern void StartEditMode(HWND hwnd);
            StartEditMode(hwnd);
            return TRUE;
        }

        // Pause/Resume timer (toggle)
        if (_stricmp(input, "pr") == 0) {
            TogglePauseResume(hwnd);
            return TRUE;
        }

        // Restart current timer
        if (_stricmp(input, "r") == 0) {
            CloseAllNotifications();
            RestartCurrentTimer(hwnd);
            return TRUE;
        }

        // Quick countdown by index: p<number> (e.g., p4 -> 4th preset)
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
                // Illegal p<number> -> fallback to default countdown
                StartDefaultCountDown(hwnd);
                return TRUE;
            }
        }
    }

    // Single-letter mode shortcuts: s (show current time), u (count up), p (pomodoro), h (help)
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
			// Use PostMessage to be consistent with instance forwarding logic
			PostMessage(hwnd, WM_APP_SHOW_CLI_HELP, 0, 0);
			return TRUE;
        }
    }

	// Normalize consecutive spaces to single spaces
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

	// Apply CLI conveniences
	expandCompactTargetTime(input);      // e.g., 1720t -> "17 20t"
	expandCompactHourMinutePlusSecond(input); // e.g., 130 20 -> "1 30 20"

	int total_seconds = 0;
    if (!ParseInput(input, &total_seconds)) {
        // Any unparsable CLI input should fallback to default countdown
        StartDefaultCountDown(hwnd);
        return TRUE;
    }
	// Stop any notification sound and close notifications
	StopNotificationSound();
	CloseAllNotifications();

	// Apply countdown state (mirror logic used in window_procedure)
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

HWND GetCliHelpDialog(void) {
    return g_cliHelpDialog;
}

void CloseCliHelpDialog(void) {
    if (g_cliHelpDialog && IsWindow(g_cliHelpDialog)) {
        DestroyWindow(g_cliHelpDialog);
        g_cliHelpDialog = NULL;
    }
}
