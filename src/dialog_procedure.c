/**
 * @file dialog_procedure.c
 * @brief Dialog window procedures and user interface handlers
 */

#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shellapi.h>
#include <strsafe.h>
#include <shlobj.h>
#include <uxtheme.h>
#include "../resource/resource.h"
#include "../include/dialog_procedure.h"
#include "../include/language.h"
#include "../include/config.h"
#include "../include/audio_player.h"
#include "../include/window_procedure.h"
#include "../include/hotkey.h"
#include "../include/dialog_language.h"
#include "../include/markdown_parser.h"

/** @brief Draw color selection button with custom appearance */
static void DrawColorSelectButton(HDC hdc, HWND hwnd);

/** @brief Global input text buffer for dialog operations */
extern wchar_t inputText[256];

/**
 * @brief Structure for individual link control data
 */
typedef struct {
    int controlId;
    const wchar_t* textCN;
    const wchar_t* textEN; 
    const wchar_t* url;
} AboutLinkInfo;

/**
 * @brief About dialog link information
 */
static AboutLinkInfo g_aboutLinkInfos[] = {
    {IDC_CREDIT_LINK, L"特别感谢猫屋敷梨梨Official提供的图标", L"Special thanks to Neko House Lili Official for the icon", L"https://space.bilibili.com/26087398"},
    {IDC_CREDITS, L"鸣谢", L"Credits", L"https://vladelaina.github.io/Catime/#thanks"},
    {IDC_BILIBILI_LINK, L"BiliBili", L"BiliBili", L"https://space.bilibili.com/1862395225"},
    {IDC_GITHUB_LINK, L"GitHub", L"GitHub", L"https://github.com/vladelaina/Catime"},
    {IDC_COPYRIGHT_LINK, L"版权声明", L"Copyright Notice", L"https://github.com/vladelaina/Catime#️copyright-notice"},
    {IDC_SUPPORT, L"支持", L"Support", L"https://vladelaina.github.io/Catime/support.html"}
};

static const int g_aboutLinkInfoCount = sizeof(g_aboutLinkInfos) / sizeof(g_aboutLinkInfos[0]);

#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES];
extern int POMODORO_TIMES_COUNT;
extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_LOOP_COUNT;

/** @brief Original edit control window procedure for subclassing */
WNDPROC wpOrigEditProc;

/** @brief Static dialog window handles for singleton management */
static HWND g_hwndAboutDlg = NULL;
static HWND g_hwndErrorDlg = NULL;
HWND g_hwndInputDialog = NULL;
static WNDPROC wpOrigLoopEditProc;

#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"

/**
 * @brief Subclass procedure for edit controls with enhanced keyboard handling
 * @param hwnd Handle to edit control window
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Result of message processing
 */
LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static BOOL firstKeyProcessed = FALSE;

    switch (msg) {
    case WM_SETFOCUS:
        /** Auto-select all text when control gains focus */
        PostMessage(hwnd, EM_SETSEL, 0, -1);
        firstKeyProcessed = FALSE;
        break;

    case WM_KEYDOWN:
        if (!firstKeyProcessed) {
            firstKeyProcessed = TRUE;
        }

        /** Handle Enter key to trigger OK button */
        if (wParam == VK_RETURN) {
            HWND hwndOkButton = GetDlgItem(GetParent(hwnd), CLOCK_IDC_BUTTON_OK);
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwndOkButton);
            return 0;
        }
        /** Handle Ctrl+A for select all */
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;

    case WM_CHAR:
        /** Block Ctrl+A character and Enter character from being processed */
        if (wParam == 1 || (wParam == 'a' || wParam == 'A') && GetKeyState(VK_CONTROL) < 0) {
            return 0;
        }
        if (wParam == VK_RETURN) {
            return 0;
        }
        break;
    }

    return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
}

/**
 * @brief Move dialog to the center of the primary screen
 * @param hwndDlg Dialog window handle
 */
void MoveDialogToPrimaryScreen(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) {
        return;
    }
    
    // Get primary monitor info
    HMONITOR hPrimaryMonitor = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    
    if (!GetMonitorInfo(hPrimaryMonitor, &mi)) {
        return;
    }
    
    // Get dialog dimensions
    RECT dialogRect;
    if (!GetWindowRect(hwndDlg, &dialogRect)) {
        return;
    }
    
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    
    // Calculate center position on primary monitor
    int primaryWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    int primaryHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    
    int newX = mi.rcMonitor.left + (primaryWidth - dialogWidth) / 2;
    int newY = mi.rcMonitor.top + (primaryHeight - dialogHeight) / 2;
    
    // Move dialog to center of primary screen
    SetWindowPos(hwndDlg, HWND_TOPMOST, newX, newY, 0, 0, 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Display modal error dialog with localized message
 * @param hwndParent Parent window handle
 */
void ShowErrorDialog(HWND hwndParent) {
    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(IDD_ERROR_DIALOG),
              hwndParent,
              ErrorDlgProc);
}

/**
 * @brief Error dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            /** Set localized error message and title */
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT,
                GetLocalizedString(L"输入格式无效，请重新输入。", L"Invalid input format, please try again."));

            SetWindowTextW(hwndDlg, GetLocalizedString(L"错误", L"Error"));
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief Main dialog procedure for input dialogs (countdown, pomodoro, shortcuts)
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter (contains dialog ID on WM_INITDIALOG)
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            /** Store dialog ID for later reference */
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

            g_hwndInputDialog = hwndDlg;

            /** Set dialog as topmost and move to primary screen */
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            MoveDialogToPrimaryScreen(hwndDlg);
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            DWORD dlgId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);

            /** Subclass edit control for enhanced keyboard handling */
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            /** Initialize dialog content based on dialog type */
            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) {
                /** Populate shortcut dialog with current time options */
                extern int time_options[];
                extern int time_options_count;
                
                /** Format current time options into human-readable string */
                char currentOptions[256] = {0};
                for (int i = 0; i < time_options_count; i++) {
                    char timeStr[32];
                    int totalSeconds = time_options[i];
                    
                    /** Convert seconds to hours/minutes/seconds format */
                    int hours = totalSeconds / 3600;
                    int minutes = (totalSeconds % 3600) / 60;
                    int seconds = totalSeconds % 60;
                    
                    if (hours > 0 && minutes > 0 && seconds > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh%dm%ds", hours, minutes, seconds);
                    } else if (hours > 0 && minutes > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh%dm", hours, minutes);
                    } else if (hours > 0 && seconds > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh%ds", hours, seconds);
                    } else if (minutes > 0 && seconds > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dm%ds", minutes, seconds);
                    } else if (hours > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh", hours);
                    } else if (minutes > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dm", minutes);
                    } else {
                        snprintf(timeStr, sizeof(timeStr), "%ds", seconds);
                    }
                    
                    if (i > 0) {
                        StringCbCatA(currentOptions, sizeof(currentOptions), " ");
                    }
                    StringCbCatA(currentOptions, sizeof(currentOptions), timeStr);
                                }
                
                /** Convert to wide char and set in edit control */
                wchar_t wcurrentOptions[256];
                MultiByteToWideChar(CP_UTF8, 0, currentOptions, -1, wcurrentOptions, 256);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcurrentOptions);
            } else if (dlgId == CLOCK_IDD_STARTUP_DIALOG) {
                /** Populate startup dialog with default start time */
                extern int CLOCK_DEFAULT_START_TIME;
                if (CLOCK_DEFAULT_START_TIME > 0) {
                    /** Format default start time for display */
                    char timeStr[64];
                    int hours = CLOCK_DEFAULT_START_TIME / 3600;
                    int minutes = (CLOCK_DEFAULT_START_TIME % 3600) / 60;
                    int seconds = CLOCK_DEFAULT_START_TIME % 60;
                    
                    if (hours > 0 && minutes > 0 && seconds > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%d %d %d", hours, minutes, seconds);
                    } else if (hours > 0 && minutes > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh%dm", hours, minutes);
                    } else if (hours > 0 && seconds > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh%ds", hours, seconds);
                    } else if (minutes > 0 && seconds > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%d %d", minutes, seconds);
                    } else if (hours > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dh", hours);
                    } else if (minutes > 0) {
                        snprintf(timeStr, sizeof(timeStr), "%dm", minutes);
                    } else {
                        snprintf(timeStr, sizeof(timeStr), "%ds", seconds);
                    }
                    
                    /** Convert to wide char and set in edit control */
                    wchar_t wtimeStr[64];
                    MultiByteToWideChar(CP_UTF8, 0, timeStr, -1, wtimeStr, 64);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                }
            }

            /** Apply localization to dialog */
            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            /** Set focus and selection on edit control */
            SetFocus(hwndEdit);

            /** Post messages to ensure proper focus handling */
            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);

            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);

            /** Set OK button as default */
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);

            /** Set timer for delayed focus setting */
            SetTimer(hwndDlg, 9999, 50, NULL);

            /** Release any stuck modifier keys */
            PostMessage(hwndDlg, WM_APP+103, 0, 0);

            /** Parse compile-time build date and time */
            char month[4];
            int day, year, hour, min, sec;

            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            /** Convert month name to number */
            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            /** Format and display build date */
            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                    year, month_num, day, hour, min, sec);

            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            return FALSE;
        }

        case WM_CLOSE: {
            g_hwndInputDialog = NULL;
            EndDialog(hwndDlg, 0);
            return TRUE;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, RGB(0xF3, 0xF3, 0xF3));
            if (!hBackgroundBrush) {
                hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            }
            return (INT_PTR)hBackgroundBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetBkColor(hdcEdit, RGB(0xFF, 0xFF, 0xFF));
            if (!hEditBrush) {
                hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            }
            return (INT_PTR)hEditBrush;
        }

        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, RGB(0xFD, 0xFD, 0xFD));
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            }
            return (INT_PTR)hButtonBrush;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                /** Get input text from edit control */
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText)/sizeof(wchar_t));

                /** Check if input is empty or only whitespace */
                BOOL isAllSpaces = TRUE;
                for (int i = 0; inputText[i]; i++) {
                    if (!iswspace(inputText[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                if (inputText[0] == L'\0' || isAllSpaces) {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }

                int dialogId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
                
                /** Handle shortcut dialog input processing */
                if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
                    /** Convert input to UTF-8 for processing */
                    char inputCopy[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputCopy, sizeof(inputCopy), NULL, NULL);
                    
                    /** Parse space-separated time options */
                    char* token = strtok(inputCopy, " ");
                    char options[256] = {0};
                    int valid = 1;
                    int count = 0;
                    
                    while (token && count < MAX_TIME_OPTIONS) {
                        int seconds = 0;
                        /** Validate each time input token */
                        if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                            valid = 0;
                            break;
                        }
                        
                        if (count > 0) {
                            StringCbCatA(options, sizeof(options), ",");
                        }
                        
                        /** Convert seconds to string and append to options */
                        char secondsStr[32];
                        snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
                        StringCbCatA(options, sizeof(options), secondsStr);
                        count++;
                        token = strtok(NULL, " ");
                    }
                    
                    if (valid && count > 0) {
                        /** Save valid options and reload configuration */
                        WriteConfigTimeOptions(options);
                        extern void ReadConfig(void);
                        ReadConfig();
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else {
                        /** Show error and refocus on edit control */
                        ShowErrorDialog(hwndDlg);

                        HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                        SetFocus(hwndEdit);
                        SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                        return TRUE;
                    }
                } else {
                    /** Handle other dialog types (pomodoro, startup, etc.) */
                    char inputUtf8[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputUtf8, sizeof(inputUtf8), NULL, NULL);
                    
                    int total_seconds;
                    if (ParseInput(inputUtf8, &total_seconds)) {
                        /** Process input based on dialog type */
                        if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        } else if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
                            WriteConfigPomodoroLoopCount(total_seconds);
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        } else if (dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                            WriteConfigDefaultStartTime(total_seconds);
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        } else {
                            g_hwndInputDialog = NULL;
                            EndDialog(hwndDlg, IDOK);
                        }
                    } else {
                        /** Show error and refocus on edit control */
                        ShowErrorDialog(hwndDlg);

                        HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                        SetFocus(hwndEdit);
                        SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                        return TRUE;
                    }
                }
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wParam == 9999) {
                /** Delayed focus setting timer */
                KillTimer(hwndDlg, 9999);

                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    SetForegroundWindow(hwndDlg);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            /** Handle Enter key to trigger OK button */
            if (wParam == VK_RETURN) {
                int dlgId = GetDlgCtrlID((HWND)lParam);
                if (dlgId == CLOCK_IDD_COLOR_DIALOG) {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                } else {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                }
                return TRUE;
            }
            break;

        /** Handle focus setting messages */
        case WM_APP+100:
        case WM_APP+101:
        case WM_APP+102:
            if (lParam) {
                HWND hwndEdit = (HWND)lParam;
                if (IsWindow(hwndEdit) && IsWindowVisible(hwndEdit)) {
                    SetForegroundWindow(hwndDlg);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
            }
            return TRUE;

        case WM_APP+103:
            /** Release all modifier keys to prevent stuck key states */
            {
                INPUT inputs[8] = {0};
                int inputCount = 0;

                /** Release left shift */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release right shift */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release left control */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release right control */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release left alt */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release right alt */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release left Windows key */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                /** Release right Windows key */
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                SendInput(inputCount, inputs, sizeof(INPUT));
            }
            return TRUE;

        case WM_DESTROY:
            /** Clean up resources and restore original window procedures */
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);

            /** Delete custom brushes */
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }

            g_hwndInputDialog = NULL;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief About dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HICON hLargeIcon = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            /** Load and set application icon */
            hLargeIcon = (HICON)LoadImage(GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_CATIME),
                IMAGE_ICON,
                ABOUT_ICON_SIZE,
                ABOUT_ICON_SIZE,
                LR_DEFAULTCOLOR);

            if (hLargeIcon) {
                SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)hLargeIcon, 0);
            }

            /** Apply localization to dialog controls */
            ApplyDialogLanguage(hwndDlg, IDD_ABOUT_DIALOG);

            /** Format and set version text with current version */
            const wchar_t* versionFormat = GetDialogLocalizedString(IDD_ABOUT_DIALOG, IDC_VERSION_TEXT);
            if (versionFormat) {
                wchar_t versionText[256];
                StringCbPrintfW(versionText, sizeof(versionText), versionFormat, CATIME_VERSION);
                SetDlgItemTextW(hwndDlg, IDC_VERSION_TEXT, versionText);
            }

            /** Parse compile-time build date and time for display */
            char month[4];
            int day, year, hour, min, sec;

            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            /** Convert month abbreviation to numeric value */
            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            /** Format and display build timestamp */
            const wchar_t* dateFormat = GetLocalizedString(L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                                                         L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)");

            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), dateFormat,
                    year, month_num, day, hour, min, sec);

            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            /** Set localized text for clickable links (using our link info structure) */
            for (int i = 0; i < g_aboutLinkInfoCount; i++) {
                const wchar_t* linkText = GetLocalizedString(g_aboutLinkInfos[i].textCN, g_aboutLinkInfos[i].textEN);
                SetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, linkText);
            }

            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);

            return TRUE;
        }

        case WM_DESTROY:
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
                hLargeIcon = NULL;
            }
            g_hwndAboutDlg = NULL;
            break;

        case WM_COMMAND:
            /** Handle button clicks and link activations */
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndAboutDlg = NULL;
                return TRUE;
            }
            
            /** Handle clicks on link controls */
            for (int i = 0; i < g_aboutLinkInfoCount; i++) {
                if (LOWORD(wParam) == g_aboutLinkInfos[i].controlId && HIWORD(wParam) == STN_CLICKED) {
                    ShellExecuteW(NULL, L"open", g_aboutLinkInfos[i].url, NULL, NULL, SW_SHOWNORMAL);
                    return TRUE;
                }
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            
            /** Check if this is one of our link controls */
            for (int i = 0; i < g_aboutLinkInfoCount; i++) {
                if (lpDrawItem->CtlID == g_aboutLinkInfos[i].controlId) {
                    /** Render link control with orange color */
                    RECT rect = lpDrawItem->rcItem;
                    HDC hdc = lpDrawItem->hDC;
                    
                    // Set background
                    FillRect(hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
                    
                    // Get control text
                    wchar_t text[256];
                    GetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, text, sizeof(text)/sizeof(text[0]));
                    
                    // Set font
                    HFONT hFont = (HFONT)SendMessage(hwndDlg, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    // Set orange color for links (same as original)
                    SetTextColor(hdc, 0x00D26919);
                    SetBkMode(hdc, TRANSPARENT);
                    
                    // Draw text
                    DrawTextW(hdc, text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(hdc, hOldFont);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CLOSE:
            /** Close dialog and clean up */
            EndDialog(hwndDlg, 0);
            g_hwndAboutDlg = NULL;
            return TRUE;
    }
    return FALSE;
}


#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2

typedef HANDLE DPI_AWARENESS_CONTEXT;

#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif


/**
 * @brief Display About dialog with DPI awareness support
 * @param hwndParent Parent window handle
 */
void ShowAboutDialog(HWND hwndParent) {
    /** Close existing about dialog if open */
    if (g_hwndAboutDlg != NULL && IsWindow(g_hwndAboutDlg)) {
        EndDialog(g_hwndAboutDlg, 0);
        g_hwndAboutDlg = NULL;
    }
    
    /** Set up DPI awareness for proper scaling on high-DPI displays */
    HANDLE hOldDpiContext = NULL;
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        /** Load DPI awareness functions dynamically for compatibility */
        typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc = 
            (GetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "GetThreadDpiAwarenessContext");
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (getThreadDpiAwarenessContextFunc && setThreadDpiAwarenessContextFunc) {
            /** Save current DPI context and set per-monitor aware mode */
            hOldDpiContext = getThreadDpiAwarenessContextFunc();
            /** Enable per-monitor DPI awareness v2 for best scaling */
            setThreadDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    
    /** Create the about dialog */
    g_hwndAboutDlg = CreateDialogW(GetModuleHandle(NULL), 
                                  MAKEINTRESOURCE(IDD_ABOUT_DIALOG), 
                                  hwndParent, 
                                  AboutDlgProc);
    
    /** Restore original DPI context */
    if (hUser32 && hOldDpiContext) {
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
    
    ShowWindow(g_hwndAboutDlg, SW_SHOW);
}


static HWND g_hwndPomodoroLoopDialog = NULL;

/**
 * @brief Display Pomodoro loop count configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (!g_hwndPomodoroLoopDialog) {
        /** Create new dialog if not already open */
        g_hwndPomodoroLoopDialog = CreateDialogW(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LOOP_DIALOG),
            hwndParent,
            PomodoroLoopDlgProc
        );
        if (g_hwndPomodoroLoopDialog) {
            ShowWindow(g_hwndPomodoroLoopDialog, SW_SHOW);
        }
    } else {
        /** Bring existing dialog to foreground */
        SetForegroundWindow(g_hwndPomodoroLoopDialog);
    }
}


/**
 * @brief Subclass procedure for loop count edit control
 * @param hwnd Handle to edit control
 * @param uMsg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Result of message processing
 */
LRESULT APIENTRY LoopEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN: {
        /** Handle Enter key to trigger OK button */
        if (wParam == VK_RETURN) {
            /** Send OK button click to parent dialog */
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwnd);
            return 0;
        }
        /** Handle Ctrl+A for select all */
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        /** Block Ctrl+A character from being processed */
        if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
            return 0;
        }
        /** Block Enter character */
        if (wParam == VK_RETURN) {
            return 0;
        }
        break;
    }
    }
    return CallWindowProc(wpOrigLoopEditProc, hwnd, uMsg, wParam, lParam);
}


/**
 * @brief Validate input string contains only digits and whitespace
 * @param str Input string to validate
 * @return TRUE if valid number input, FALSE otherwise
 */
BOOL IsValidNumberInput(const wchar_t* str) {
    /** Check for null or empty input */
    if (!str || !*str) {
        return FALSE;
    }
    
    BOOL hasDigit = FALSE;
    wchar_t cleanStr[16] = {0};
    int cleanIndex = 0;
    
    /** Check each character - only digits and whitespace allowed */
    for (int i = 0; str[i]; i++) {
        if (iswdigit(str[i])) {
            cleanStr[cleanIndex++] = str[i];
            hasDigit = TRUE;
        } else if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    
    return hasDigit;
}


/**
 * @brief Pomodoro loop count dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            /** Apply localization to dialog */
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);
            
            /** Set prompt text for loop count input */
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC, GetLocalizedString(L"请输入循环次数（1-100）：", L"Please enter loop count (1-100):"));
            
            /** Set focus on edit control and subclass it */
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetFocus(hwndEdit);
            
            wpOrigLoopEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                          (LONG_PTR)LoopEditSubclassProc);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }

        case WM_COMMAND:
            /** Handle OK button click for loop count input */
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                
                /** Check for empty or whitespace-only input */
                BOOL isAllSpaces = TRUE;
                for (int i = 0; input_str[i]; i++) {
                    if (!iswspace(input_str[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                /** Cancel if empty input */
                if (input_str[0] == L'\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndPomodoroLoopDialog = NULL;
                    return TRUE;
                }
                
                /** Validate numeric input */
                if (!IsValidNumberInput(input_str)) {
                    ShowErrorDialog(hwndDlg);
                    /** Reset focus and select all text for retry */
                    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                    return TRUE;
                }
                
                /** Extract digits only from input */
                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }
                
                /** Validate range (1-100) and save configuration */
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {
                    /** Save valid loop count to configuration */
                    WriteConfigPomodoroLoopCount(new_loop_count);
                    EndDialog(hwndDlg, IDOK);
                    g_hwndPomodoroLoopDialog = NULL;
                } else {
                    /** Show error for out-of-range values */
                    ShowErrorDialog(hwndDlg);
                    /** Reset focus for retry */
                    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroLoopDialog = NULL;
                return TRUE;
            }
            break;

        case WM_DESTROY:
            /** Restore original edit control procedure */
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigLoopEditProc);
            }
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndPomodoroLoopDialog = NULL;
            return TRUE;
    }
    return FALSE;
}


static HWND g_hwndWebsiteDialog = NULL;

/**
 * @brief Website URL configuration dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            /** Store dialog parameter for later use */
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            
            /** Create custom brushes for dialog appearance */
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            /** Subclass edit control for enhanced functionality */
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            /** Pre-populate with existing URL if available */
            if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }
            
            /** Apply localization to dialog controls */
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            
            /** Set focus and select all text for easy editing */
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(240, 240, 240));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (INT_PTR)hEditBrush;
            
        case WM_CTLCOLORBTN:
            return (INT_PTR)hButtonBrush;
            
        case WM_COMMAND:
            /** Handle OK button click for URL input */
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                wchar_t url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url)/sizeof(wchar_t));
                
                /** Check for empty or whitespace-only URL */
                BOOL isAllSpaces = TRUE;
                for (int i = 0; url[i]; i++) {
                    if (!iswspace(url[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                /** Cancel if empty URL */
                if (url[0] == L'\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndWebsiteDialog = NULL;
                    return TRUE;
                }
                
                /** Auto-prepend https:// if no protocol specified */
                if (wcsncmp(url, L"http://", 7) != 0 && wcsncmp(url, L"https://", 8) != 0) {
                    /** Default to secure HTTPS protocol */
                    wchar_t tempUrl[MAX_PATH] = L"https://";
                    StringCbCatW(tempUrl, sizeof(tempUrl), url);
                    StringCbCopyW(url, sizeof(url), tempUrl);
                }
                
                /** Convert to UTF-8 and save configuration */
                char urlUtf8[MAX_PATH * 3];
                WideCharToMultiByte(CP_UTF8, 0, url, -1, urlUtf8, sizeof(urlUtf8), NULL, NULL);
                WriteConfigTimeoutWebsite(urlUtf8);
                EndDialog(hwndDlg, IDOK);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                /** Handle cancel button */
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            /** Clean up resources and restore window procedures */
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            /** Delete custom brushes */
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndWebsiteDialog = NULL;
            return TRUE;
    }
    
    return FALSE;
}


/**
 * @brief Display website URL configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowWebsiteDialog(HWND hwndParent) {
    /** Create and show modal website dialog */
    INT_PTR result = DialogBoxW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );
    /** Result is handled within the dialog procedure */
}


static HWND g_hwndPomodoroComboDialog = NULL;

/**
 * @brief Pomodoro time options configuration dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            /** Create custom brushes for dialog appearance */
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            /** Subclass edit control for enhanced functionality */
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            /** Format current Pomodoro time options for display */
            wchar_t currentOptions[256] = {0};
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                wchar_t timeStr[32];
                int seconds = POMODORO_TIMES[i];
                
                /** Format time based on duration (hours, minutes, seconds) */
                if (seconds >= 3600) {
                    int hours = seconds / 3600;
                    int mins = (seconds % 3600) / 60;
                    int secs = seconds % 60;
                    if (mins == 0 && secs == 0)
                        StringCbPrintfW(timeStr, sizeof(timeStr), L"%dh ", hours);
                    else if (secs == 0)
                        StringCbPrintfW(timeStr, sizeof(timeStr), L"%dh%dm ", hours, mins);
                    else
                        StringCbPrintfW(timeStr, sizeof(timeStr), L"%dh%dm%ds ", hours, mins, secs);
                } else if (seconds >= 60) {
                    int mins = seconds / 60;
                    int secs = seconds % 60;
                    if (secs == 0)
                        StringCbPrintfW(timeStr, sizeof(timeStr), L"%dm ", mins);
                    else
                        StringCbPrintfW(timeStr, sizeof(timeStr), L"%dm%ds ", mins, secs);
                } else {
                    StringCbPrintfW(timeStr, sizeof(timeStr), L"%ds ", seconds);
                }
                
                StringCbCatW(currentOptions, sizeof(currentOptions), timeStr);
            }
            
            /** Remove trailing space from formatted string */
            if (wcslen(currentOptions) > 0 && currentOptions[wcslen(currentOptions) - 1] == L' ') {
                currentOptions[wcslen(currentOptions) - 1] = L'\0';
            }
            
            /** Pre-populate edit control with current options */
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            
            /** Apply localization to dialog controls */
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            
            /** Set focus and select all text for easy editing */
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(240, 240, 240));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (INT_PTR)hEditBrush;
            
        case WM_CTLCOLORBTN:
            return (INT_PTR)hButtonBrush;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || LOWORD(wParam) == IDOK) {
                char input[256] = {0};

                wchar_t winput[256];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, winput, sizeof(winput)/sizeof(wchar_t));
                WideCharToMultiByte(CP_UTF8, 0, winput, -1, input, sizeof(input), NULL, NULL);
                

                BOOL isAllSpaces = TRUE;
                for (int i = 0; input[i]; i++) {
                    if (!isspace((unsigned char)input[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                if (input[0] == '\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndPomodoroComboDialog = NULL;
                    return TRUE;
                }
                

                char *token, *saveptr;
                char input_copy[256];
                StringCbCopyA(input_copy, sizeof(input_copy), input);
                
                int times[MAX_POMODORO_TIMES] = {0};
                int times_count = 0;
                BOOL hasInvalidInput = FALSE;
                
                token = strtok_r(input_copy, " ", &saveptr);
                while (token && times_count < MAX_POMODORO_TIMES) {
                    int seconds = 0;
                    if (ParseTimeInput(token, &seconds)) {
                        times[times_count++] = seconds;
                    } else {
                        hasInvalidInput = TRUE;
                        break;
                    }
                    token = strtok_r(NULL, " ", &saveptr);
                }
                

                if (hasInvalidInput || times_count == 0) {
                    ShowErrorDialog(hwndDlg);

                    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                    return TRUE;
                }
                

                POMODORO_TIMES_COUNT = times_count;
                for (int i = 0; i < times_count; i++) {
                    POMODORO_TIMES[i] = times[i];
                }
                

                if (times_count > 0) POMODORO_WORK_TIME = times[0];
                if (times_count > 1) POMODORO_SHORT_BREAK = times[1];
                if (times_count > 2) POMODORO_LONG_BREAK = times[2];
                

                WriteConfigPomodoroTimeOptions(times, times_count);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndPomodoroComboDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroComboDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:

            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            

            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display Pomodoro time options configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowPomodoroComboDialog(HWND hwndParent) {
    if (!g_hwndPomodoroComboDialog) {
        /** Create new dialog if not already open */
        g_hwndPomodoroComboDialog = CreateDialogW(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_COMBO_DIALOG),
            hwndParent,
            PomodoroComboDialogProc
        );
        if (g_hwndPomodoroComboDialog) {
            ShowWindow(g_hwndPomodoroComboDialog, SW_SHOW);
        }
    } else {
        /** Bring existing dialog to foreground */
        SetForegroundWindow(g_hwndPomodoroComboDialog);
    }
}

/**
 * @brief Parse time input string with format like "25m 5m 15m" or "1h30m"
 * @param input Input string to parse
 * @param seconds Output parameter for parsed seconds
 * @return TRUE if parsing successful, FALSE otherwise
 */
BOOL ParseTimeInput(const char* input, int* seconds) {
    if (!input || !seconds) return FALSE;

    *seconds = 0;
    
    /** Create working copy of input string */
    char buffer[256];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* pos = buffer;
    int tempSeconds = 0;

    while (*pos) {
        /** Skip whitespace */
        while (*pos == ' ' || *pos == '\t') pos++;
        if (*pos == '\0') break;
        
        /** Parse numeric value */
        if (isdigit((unsigned char)*pos)) {
            int value = 0;
            while (isdigit((unsigned char)*pos)) {
                value = value * 10 + (*pos - '0');
                pos++;
            }

            /** Skip whitespace after number */
            while (*pos == ' ' || *pos == '\t') pos++;

            /** Parse time unit suffix */
            if (*pos == 'h' || *pos == 'H') {
                tempSeconds += value * 3600;
                pos++;
            } else if (*pos == 'm' || *pos == 'M') {
                tempSeconds += value * 60;
                pos++;
            } else if (*pos == 's' || *pos == 'S') {
                tempSeconds += value;
                pos++;
            } else if (*pos == '\0') {
                /** Default to minutes if no unit specified */
                tempSeconds += value * 60;
                break;
            } else {
                /** Invalid character encountered */
                return FALSE;
            }
        } else {
            /** Non-digit character where number expected */
            return FALSE;
        }
    }

    *seconds = tempSeconds;
    return TRUE;
}

static HWND g_hwndNotificationMessagesDialog = NULL;

/**
 * @brief Notification messages configuration dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK NotificationMessagesDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            /** Make dialog topmost for visibility */
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);

            /** Create custom brushes for dialog appearance */
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

            /** Load current notification messages from config */
            ReadNotificationMessagesConfig();
            
            /** Convert and display timeout message */
            wchar_t wideText[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT)];
            
            /** Convert UTF-8 timeout message to wide char */
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            /** Convert and display Pomodoro timeout message */
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            /** Convert and display cycle complete message */
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            /** Set localized labels for edit controls */
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1, 
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2, 
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));
            
            /** Set localized button text */
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));
            
            /** Get handles to all edit controls */
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            /** Subclass first edit control and store original procedure */
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            /** Subclass remaining edit controls */
            SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            /** Select all text in first edit control */
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            
            /** Set focus to first edit control */
            SetFocus(GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1));
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
        
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            return (INT_PTR)hEditBrush;
        
        case WM_COMMAND:
            /** Handle OK button to save notification messages */
            if (LOWORD(wParam) == IDOK) {
                /** Get text from all three edit controls */
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                /** Retrieve text from edit controls */
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                /** Convert to UTF-8 for configuration storage */
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                /** Save all messages to configuration */
                WriteConfigNotificationMessages(timeout_msg, pomodoro_msg, cycle_complete_msg);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationMessagesDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationMessagesDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            /** Clean up resources and restore window procedures */
            {
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            /** Restore original window procedures for all edit controls */
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            /** Delete custom brushes */
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display notification messages configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowNotificationMessagesDialog(HWND hwndParent) {
    if (!g_hwndNotificationMessagesDialog) {
        /** Load current configuration before showing dialog */
        ReadNotificationMessagesConfig();
        
        /** Create and show modal dialog */
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG), 
                  hwndParent, 
                  NotificationMessagesDlgProc);
    } else {
        /** Bring existing dialog to foreground */
        SetForegroundWindow(g_hwndNotificationMessagesDialog);
    }
}


static HWND g_hwndNotificationDisplayDialog = NULL;

/**
 * @brief Notification display settings dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            /** Make dialog topmost for visibility */
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            /** Create custom brushes for dialog appearance */
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            
            /** Load current notification configuration */
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            
            /** Format timeout value for display */
            wchar_t wbuffer[32];
            
            /** Convert milliseconds to seconds with one decimal place */
            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            /** Remove trailing .0 for whole numbers */
            if (wcslen(wbuffer) > 2 && wbuffer[wcslen(wbuffer)-2] == L'.' && wbuffer[wcslen(wbuffer)-1] == L'0') {
                wbuffer[wcslen(wbuffer)-2] = L'\0';
            }
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wbuffer);
            
            /** Set opacity value */
            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wbuffer);
            
            /** Set localized label text */
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));
            
            /** Configure time edit control to allow decimal input */
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);
            
            /** Subclass edit control for enhanced functionality */
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            /** Set focus to time edit control */
            SetFocus(hEditTime);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
        
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            return (INT_PTR)hEditBrush;
        
        case WM_COMMAND:
            /** Handle OK button to save display settings */
            if (LOWORD(wParam) == IDOK) {
                char timeStr[32] = {0};
                char opacityStr[32] = {0};
                
                /** Get text from edit controls */
                wchar_t wtimeStr[32], wopacityStr[32];
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wtimeStr, sizeof(wtimeStr)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wopacityStr, sizeof(wopacityStr)/sizeof(wchar_t));
                
                /** Convert to multibyte for processing */
                WideCharToMultiByte(CP_UTF8, 0, wtimeStr, -1, timeStr, sizeof(timeStr), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wopacityStr, -1, opacityStr, sizeof(opacityStr), NULL, NULL);
                
                /** Normalize decimal separators from various input methods */
                wchar_t wTimeStr[32] = {0};
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wTimeStr, sizeof(wTimeStr)/sizeof(wchar_t));
                
                /** Replace various decimal separators with standard dot */
                for (int i = 0; wTimeStr[i] != L'\0'; i++) {
                    /** Convert Chinese, Japanese and other punctuation to decimal point */
                    if (wTimeStr[i] == L'。' ||
                        wTimeStr[i] == L'，' ||
                        wTimeStr[i] == L',' ||
                        wTimeStr[i] == L'·' ||
                        wTimeStr[i] == L'`' ||
                        wTimeStr[i] == L'：' ||
                        wTimeStr[i] == L':' ||
                        wTimeStr[i] == L'；' ||
                        wTimeStr[i] == L';' ||
                        wTimeStr[i] == L'/' ||
                        wTimeStr[i] == L'\\' ||
                        wTimeStr[i] == L'~' ||
                        wTimeStr[i] == L'～' ||
                        wTimeStr[i] == L'、' ||
                        wTimeStr[i] == L'．') {
                        wTimeStr[i] = L'.';
                    }
                }
                
                /** Convert normalized string to multibyte */
                WideCharToMultiByte(CP_UTF8, 0, wTimeStr, -1, 
                                    timeStr, sizeof(timeStr), NULL, NULL);
                
                /** Parse time value and convert to milliseconds */
                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);
                
                /** Enforce minimum timeout of 100ms for very small values */
                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;
                
                /** Parse and validate opacity value */
                int opacity = atoi(opacityStr);
                
                /** Clamp opacity to valid range (1-100) */
                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;
                
                /** Save configuration values */
                WriteConfigNotificationTimeout(timeInMs);
                WriteConfigNotificationOpacity(opacity);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationDisplayDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationDisplayDialog = NULL;
                return TRUE;
            }
            break;
            

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationDisplayDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:

            {
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            HWND hEditOpacity = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEditOpacity, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display notification display settings dialog
 * @param hwndParent Parent window handle
 */
void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (!g_hwndNotificationDisplayDialog) {
        /** Load current configuration before showing dialog */
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        
        /** Create and show modal dialog */
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG), 
                  hwndParent, 
                  NotificationDisplayDlgProc);
    } else {
        /** Bring existing dialog to foreground */
        SetForegroundWindow(g_hwndNotificationDisplayDialog);
    }
}


static HWND g_hwndNotificationSettingsDialog = NULL;

/**
 * @brief Audio playback completion callback for notification settings dialog
 * @param hwnd Dialog window handle
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        /** Reset test button text when playback completes */
        const wchar_t* testText = GetLocalizedString(L"Test", L"Test");
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, testText);
        
        /** Get handle to test button */
        HWND hwndTestButton = GetDlgItem(hwnd, IDC_TEST_SOUND_BUTTON);
        
        /** Ensure button text is properly updated */
        if (hwndTestButton && IsWindow(hwndTestButton)) {
            SendMessageW(hwndTestButton, WM_SETTEXT, 0, (LPARAM)testText);
        }
        
        /** Send custom message if this is the notification settings dialog */
        if (g_hwndNotificationSettingsDialog == hwnd) {
            /** Notify dialog that playback is complete */
            SendMessage(hwnd, WM_APP + 100, 0, 0);
        }
    }
}

/**
 * @brief Populate sound selection combo box with available audio files
 * @param hwndDlg Dialog window handle
 */
static void PopulateSoundComboBox(HWND hwndDlg) {
    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    if (!hwndCombo) return;

    /** Clear existing items */
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    /** Add "None" option for no sound */
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"None", L"None"));
    
    /** Add system beep option */
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"System Beep", L"System Beep"));

    /** Get audio files directory path */
    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    
    /** Convert to wide char for file operations */
    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);

    /** Create search pattern for all files */
    wchar_t wSearchPath[MAX_PATH];
    StringCbPrintfW(wSearchPath, sizeof(wSearchPath), L"%s\\*.*", wAudioPath);

    /** Find and add supported audio files */
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /** Check for supported audio file extensions */
            wchar_t* ext = wcsrchr(find_data.cFileName, L'.');
            if (ext && (
                _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".mp3") == 0 ||
                _wcsicmp(ext, L".wav") == 0
            )) {
                /** Add supported audio file to combo box */
                SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)find_data.cFileName);
            }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }

    /** Select current sound file if configured */
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        /** Check if system beep is selected */
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            /** Select system beep option (index 1) */
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, NOTIFICATION_SOUND_FILE, -1, wSoundFile, MAX_PATH);
            
            /** Extract filename from full path */
            wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
            if (fileName) fileName++;
            else fileName = wSoundFile;
            
            /** Find and select the matching file */
            int index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fileName);
            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                /** Default to "None" if file not found */
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
            }
        }
    } else {
        /** Default to "None" if no sound configured */
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
    }
}

/**
 * @brief Notification settings dialog window procedure
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if message processed, FALSE otherwise
 */
INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isPlaying = FALSE;
    static int originalVolume = 0;
    
    switch (msg) {
        case WM_INITDIALOG: {
            /** Load all notification-related configuration */
            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            
            /** Store original volume for potential restoration */
            originalVolume = NOTIFICATION_SOUND_VOLUME;
            
            /** Apply localization to dialog controls */
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);
            
            /** Convert and display notification message texts */
            wchar_t wideText[256];
            
            /** Display countdown timeout message */
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            /** Display Pomodoro timeout message */
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            /** Display Pomodoro cycle complete message */
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            /** Initialize system time structure for time picker */
            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            
            /** Load notification disabled state */
            ReadNotificationDisabledConfig();
            
            /** Set notification disabled checkbox state */
            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, NOTIFICATION_DISABLED ? BST_CHECKED : BST_UNCHECKED);
            
            /** Enable/disable time edit based on notification state */
            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !NOTIFICATION_DISABLED);
            
            /** Convert timeout milliseconds to time components */
            int totalSeconds = NOTIFICATION_TIMEOUT_MS / 1000;
            st.wHour = totalSeconds / 3600;
            st.wMinute = (totalSeconds % 3600) / 60;
            st.wSecond = totalSeconds % 60;
            
            /** Set time picker control value */
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME, 
                              GDT_VALID, (LPARAM)&st);

            /** Configure opacity slider control */
            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, NOTIFICATION_MAX_OPACITY);
            
            /** Display current opacity percentage */
            wchar_t opacityText[16];
            StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
            
            /** Set notification type radio buttons based on current type */
            switch (NOTIFICATION_TYPE) {
                case NOTIFICATION_TYPE_CATIME:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_OS:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_OS, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_SYSTEM_MODAL:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, BST_CHECKED);
                    break;
            }
            
            /** Populate sound selection combo box with available files */
            PopulateSoundComboBox(hwndDlg);
            
            /** Configure volume slider control */
            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, NOTIFICATION_SOUND_VOLUME);
            
            /** Display current volume percentage */
            wchar_t volumeText[16];
            StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", NOTIFICATION_SOUND_VOLUME);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
            
            /** Initialize audio playback state */
            isPlaying = FALSE;
            
            /** Set up audio playback completion callback */
            SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
            
            /** Store dialog handle for callback reference */
            g_hwndNotificationSettingsDialog = hwndDlg;
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return TRUE;
        }
        
        case WM_HSCROLL: {
            /** Handle slider control changes */
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                /** Volume slider changed */
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                /** Update volume percentage display */
                wchar_t volumeText[16];
                StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
                
                /** Apply volume change to audio system */
                SetAudioVolume(volume);
                
                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                /** Opacity slider changed */
                int opacity = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                

                /** Update opacity percentage display */
                wchar_t opacityText[16];
                StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:
            /** Handle button clicks and control changes */
            if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                /** Toggle notification enabled/disabled state */
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
                /** Save all notification settings */
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                /** Get notification message texts from edit controls */
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                /** Convert to UTF-8 for configuration storage */
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                

                /** Get notification timeout from time picker */
                SYSTEMTIME st = {0};
                
                /** Check if notifications are disabled */
                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                
                /** Save notification disabled state */
                NOTIFICATION_DISABLED = isDisabled;
                WriteConfigNotificationDisabled(isDisabled);
                

                /** Get and process timeout value from time picker */
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    /** Convert time components to total seconds */
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    
                    if (totalSeconds == 0) {
                        /** Zero timeout means no timeout */
                        NOTIFICATION_TIMEOUT_MS = 0;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                        
                    } else if (!isDisabled) {
                        /** Convert seconds to milliseconds and save */
                        NOTIFICATION_TIMEOUT_MS = totalSeconds * 1000;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                    }
                }

                /** Get and validate opacity value from slider */
                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
                if (opacity >= 1 && opacity <= 100) {
                    NOTIFICATION_MAX_OPACITY = opacity;
                }
                
                /** Determine selected notification type from radio buttons */
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
                
                /** Get selected sound file from combo box */
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                if (index > 0) {
                    wchar_t wFileName[MAX_PATH];
                    SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                    
                    /** Check if system beep is selected */
                    const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                    if (wcscmp(wFileName, sysBeepText) == 0) {
                        /** Set system beep as sound file */
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                    } else {
                        /** Build full path to audio file */
                        char audio_path[MAX_PATH];
                        GetAudioFolderPath(audio_path, MAX_PATH);
                        
                        char fileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                        
                        /** Construct full file path */
                        memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                        StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                    }
                } else {
                    /** No sound selected */
                    NOTIFICATION_SOUND_FILE[0] = '\0';
                }
                
                /** Get volume value from slider */
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                NOTIFICATION_SOUND_VOLUME = volume;
                
                /** Write all configuration values to file */
                WriteConfigNotificationMessages(
                    timeout_msg,
                    pomodoro_msg,
                    cycle_complete_msg
                );
                WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                WriteConfigNotificationOpacity(NOTIFICATION_MAX_OPACITY);
                WriteConfigNotificationType(NOTIFICATION_TYPE);
                WriteConfigNotificationSound(NOTIFICATION_SOUND_FILE);
                WriteConfigNotificationVolume(NOTIFICATION_SOUND_VOLUME);
                
                /** Stop any playing audio and clean up */
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                /** Clear audio callback */
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                /** Cancel - stop audio and restore original settings */
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                /** Restore original volume */
                NOTIFICATION_SOUND_VOLUME = originalVolume;
                
                /** Apply original volume to audio system */
                SetAudioVolume(originalVolume);
                
                /** Clear audio callback */
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                /** Handle test sound button click */
                if (!isPlaying) {
                    /** Start playing selected sound */
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                    
                    if (index > 0) {
                        /** Set volume from slider before playing */
                        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                        int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                        SetAudioVolume(volume);
                        
                        wchar_t wFileName[MAX_PATH];
                        SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                        
                        /** Backup current sound file setting */
                        char tempSoundFile[MAX_PATH];
                        StringCbCopyA(tempSoundFile, sizeof(tempSoundFile), NOTIFICATION_SOUND_FILE);
                        
                        /** Temporarily set sound file for testing */
                        const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                        if (wcscmp(wFileName, sysBeepText) == 0) {
                            /** Test system beep */
                            StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                        } else {
                            /** Test audio file */
                            char audio_path[MAX_PATH];
                            GetAudioFolderPath(audio_path, MAX_PATH);
                            
                            char fileName[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                            
                            /** Build full path for testing */
                            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                            StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                        }
                        
                        /** Play the sound */
                        if (PlayNotificationSound(hwndDlg)) {
                            /** Update button to show stop option */
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Stop", L"Stop"));
                            isPlaying = TRUE;
                        }
                        
                        /** Restore original sound file setting */
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), tempSoundFile);
                    }
                } else {
                    /** Stop currently playing sound */
                    StopNotificationSound();
                    SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Test", L"Test"));
                    isPlaying = FALSE;
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                /** Open audio files directory in explorer */
                char audio_path[MAX_PATH];
                GetAudioFolderPath(audio_path, MAX_PATH);
                
                /** Convert path to wide char for shell execute */
                wchar_t wAudioPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
                
                /** Open directory in Windows Explorer */
                ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
                
                /** Remember currently selected item */
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                /** Refresh combo box with updated file list */
                PopulateSoundComboBox(hwndDlg);
                
                /** Restore selection if file still exists */
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    } else {
                        /** File no longer exists, select "None" */
                        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
                    }
                }
                
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                /** Refresh sound combo box when dropdown is opened */
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                
                /** Remember currently selected item */
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                /** Refresh combo box with current file list */
                PopulateSoundComboBox(hwndDlg);
                
                /** Restore selection if file still exists */
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    }
                }
                
                return TRUE;
            }
            break;
            
        /** Custom message from audio playback completion callback */
        case WM_APP + 100:
            /** Reset playing state when audio completes */
            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:
            /** Handle dialog close */
            if (isPlaying) {
                StopNotificationSound();
            }
            
            /** Clean up audio callback */
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            /** Clean up resources on dialog destruction */
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
    return FALSE;
}

/**
 * @brief Display comprehensive notification settings dialog
 * @param hwndParent Parent window handle
 */
void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
        /** Load all current configuration values before showing dialog */
        ReadNotificationMessagesConfig();
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        ReadNotificationTypeConfig();
        ReadNotificationSoundConfig();
        ReadNotificationVolumeConfig();
        
        /** Create and show modal dialog */
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG), 
                  hwndParent, 
                  NotificationSettingsDlgProc);
    } else {
        /** Bring existing dialog to foreground */
        SetForegroundWindow(g_hwndNotificationSettingsDialog);
    }
}

/**
 * @brief Global storage for parsed links and dialog data (Font License)
 */
static MarkdownLink* g_links = NULL;
static int g_linkCount = 0;
static wchar_t* g_displayText = NULL;

/**
 * @brief Font license agreement dialog procedure
 * @param hwndDlg Dialog window handle
 * @param message Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            /** Set dialog title and content based on current language */
            SetWindowTextW(hwndDlg, GetLocalizedString(L"自定义字体功能说明及版权提示", L"Custom Font Feature License Agreement"));
            
            /** Get license agreement text with markdown links */
            const wchar_t* licenseText = GetLocalizedString(
                L"本功能将加载以下文件夹中的所有字体文件（包括子文件夹）：\n\nC:\\Users\\[您的当前用户名]\\AppData\\Local\\Catime\\resources\\fonts\n\n请务必注意： 任何版权风险和法律责任都将由您个人承担，本软件不承担任何责任。\n\n────────────────────────────────────────\n\n为了避免版权风险：\n您可以前往 [Google Fonts](https://fonts.google.com/?preview.text=1234567890:) 下载大量可免费商用的字体。",
                L"FontLicenseAgreementText");
            
            /** Parse markdown links and set display text */
            ParseMarkdownLinks(licenseText, &g_displayText, &g_links, &g_linkCount);
            
            // Text will be drawn by WM_DRAWITEM handler
            // No need to set text here since we're using SS_OWNERDRAW
            
            /** Set button text */
            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_AGREE_BTN, GetLocalizedString(L"同意", L"Agree"));
            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_CANCEL_BTN, GetLocalizedString(L"取消", L"Cancel"));
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FONT_LICENSE_AGREE_BTN:
                    /** Clean up and close dialog */
                    FreeMarkdownLinks(g_links, g_linkCount);
                    g_links = NULL;
                    g_linkCount = 0;
                    if (g_displayText) {
                        free(g_displayText);
                        g_displayText = NULL;
                    }
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                case IDC_FONT_LICENSE_CANCEL_BTN:
                    /** Clean up and close dialog */
                    FreeMarkdownLinks(g_links, g_linkCount);
                    g_links = NULL;
                    g_linkCount = 0;
                    if (g_displayText) {
                        free(g_displayText);
                        g_displayText = NULL;
                    }
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                case IDC_FONT_LICENSE_TEXT:
                    /** Handle clicks on the text control to detect link clicks */
                    if (HIWORD(wParam) == STN_CLICKED) {
                        // Get click position
                        POINT pt;
                        GetCursorPos(&pt);
                        ScreenToClient(GetDlgItem(hwndDlg, IDC_FONT_LICENSE_TEXT), &pt);
                        
                        // Handle markdown link click
                        if (HandleMarkdownClick(g_links, g_linkCount, pt)) {
                            return TRUE;
                        }
                        // If click is not on a link, do nothing
                    }
                    return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_FONT_LICENSE_TEXT) {
                /** Custom draw the text with blue links */
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;
                
                // Fill background
                HBRUSH hBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);
                
                if (g_displayText) {
                    // Set text properties
                    SetBkMode(hdc, TRANSPARENT);
                    
                    // Get font
                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    // Render markdown text with clickable links
                    RECT drawRect = rect;
                    drawRect.left += 5; // Small margin
                    drawRect.top += 5;
                    
                    RenderMarkdownText(hdc, g_displayText, g_links, g_linkCount, 
                                       drawRect, MARKDOWN_DEFAULT_LINK_COLOR, MARKDOWN_DEFAULT_TEXT_COLOR);
                    
                    SelectObject(hdc, hOldFont);
                }
                
                return TRUE;
            }
            break;
        }
        
        case WM_CLOSE:
            /** Handle window close (X button) */
            FreeMarkdownLinks(g_links, g_linkCount);
            g_links = NULL;
            g_linkCount = 0;
            if (g_displayText) {
                free(g_displayText);
                g_displayText = NULL;
            }
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

/**
 * @brief Show font license agreement dialog
 * @param hwndParent Parent window handle for proper modal behavior
 * @return Dialog result (IDOK if agreed, IDCANCEL if declined)
 */
INT_PTR ShowFontLicenseDialog(HWND hwndParent) {
    return DialogBoxW(GetModuleHandle(NULL), 
                     MAKEINTRESOURCE(IDD_FONT_LICENSE_DIALOG), 
                     hwndParent,  // Use parent window to prevent taskbar icon
                     FontLicenseDlgProc);
}