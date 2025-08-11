/**
 * @file dialog_procedure.c
 * @brief Implementation of dialog message handling procedures
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

static void DrawColorSelectButton(HDC hdc, HWND hwnd);

extern char inputText[256];

#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES];
extern int POMODORO_TIMES_COUNT;
extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_LOOP_COUNT;

WNDPROC wpOrigEditProc;

static HWND g_hwndAboutDlg = NULL;
static HWND g_hwndErrorDlg = NULL;
HWND g_hwndInputDialog = NULL;
static WNDPROC wpOrigLoopEditProc;

#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"

LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static BOOL firstKeyProcessed = FALSE;

    switch (msg) {
    case WM_SETFOCUS:
        PostMessage(hwnd, EM_SETSEL, 0, -1);
        firstKeyProcessed = FALSE;
        break;

    case WM_KEYDOWN:
        if (!firstKeyProcessed) {
            firstKeyProcessed = TRUE;
        }

        if (wParam == VK_RETURN) {
            HWND hwndOkButton = GetDlgItem(GetParent(hwnd), CLOCK_IDC_BUTTON_OK);
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwndOkButton);
            return 0;
        }
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;

    case WM_CHAR:
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

INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void ShowErrorDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL),
             MAKEINTRESOURCE(IDD_ERROR_DIALOG),
             hwndParent,
             ErrorDlgProc);
}

INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT,
                GetLocalizedString(L"输入格式无效，请重新输入。", L"Invalid input format, please try again."));

            SetWindowTextW(hwndDlg, GetLocalizedString(L"错误", L"Error"));
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
 * @brief Input dialog procedure
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

            g_hwndInputDialog = hwndDlg;

            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            DWORD dlgId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);

            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) {
                // Read current countdown preset time options from configuration and format for display
                extern int time_options[];
                extern int time_options_count;
                
                char currentOptions[256] = {0};
                for (int i = 0; i < time_options_count; i++) {
                    char timeStr[32];
                    snprintf(timeStr, sizeof(timeStr), "%d", time_options[i]);
                    
                    if (i > 0) {
                        strcat(currentOptions, " ");
                    }
                    strcat(currentOptions, timeStr);
                }
                
                // Set edit box text with current preset values
                SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            }

            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            SetFocus(hwndEdit);

            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);

            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);

            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);

            SetTimer(hwndDlg, 9999, 50, NULL);

            PostMessage(hwndDlg, WM_APP+103, 0, 0);

            char month[4];
            int day, year, hour, min, sec;

            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

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
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText));

                BOOL isAllSpaces = TRUE;
                for (int i = 0; inputText[i]; i++) {
                    if (!isspace((unsigned char)inputText[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                if (inputText[0] == '\0' || isAllSpaces) {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }

                int total_seconds;
                if (ParseInput(inputText, &total_seconds)) {
                int dialogId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
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
                } else if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
                    // Parse input as space-separated time options (like in window_procedure.c)
                    // Use a local copy to avoid modifying the original inputText
                    char inputCopy[256];
                    strncpy(inputCopy, inputText, sizeof(inputCopy) - 1);
                    inputCopy[sizeof(inputCopy) - 1] = '\0';
                    
                    char* token = strtok(inputCopy, " ");
                    char options[256] = {0};
                    int valid = 1;
                    int count = 0;
                    
                    while (token && count < MAX_TIME_OPTIONS) {
                        int num = atoi(token);
                        if (num <= 0) {
                            valid = 0;
                            break;
                        }
                        
                        if (count > 0) {
                            strcat(options, ",");
                        }
                        strcat(options, token);
                        count++;
                        token = strtok(NULL, " ");
                    }
                    
                    if (valid && count > 0) {
                        WriteConfigTimeOptions(options);
                        extern void ReadConfig(void);
                        ReadConfig();
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else {
                        // Show error message and keep dialog open
                        MessageBoxW(hwndDlg,
                            GetLocalizedString(
                                L"请输入用空格分隔的数字\n"
                                L"例如: 25 10 5",
                                L"Enter numbers separated by spaces\n"
                                L"Example: 25 10 5"),
                            GetLocalizedString(L"无效输入", L"Invalid Input"), 
                            MB_OK);
                        SetWindowTextA(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT), "");
                        SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                        return TRUE;
                    }
                } else {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, IDOK);
                }
                } else {
                    ShowErrorDialog(hwndDlg);
                    SetWindowTextA(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT), "");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wParam == 9999) {
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
            {
                INPUT inputs[8] = {0};
                int inputCount = 0;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;

                SendInput(inputCount, inputs, sizeof(INPUT));
            }
            return TRUE;

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

            g_hwndInputDialog = NULL;
            }
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HICON hLargeIcon = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            hLargeIcon = (HICON)LoadImage(GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_CATIME),
                IMAGE_ICON,
                ABOUT_ICON_SIZE,
                ABOUT_ICON_SIZE,
                LR_DEFAULTCOLOR);

            if (hLargeIcon) {
                SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)hLargeIcon, 0);
            }

            ApplyDialogLanguage(hwndDlg, IDD_ABOUT_DIALOG);

            const wchar_t* versionFormat = GetDialogLocalizedString(IDD_ABOUT_DIALOG, IDC_VERSION_TEXT);
            if (versionFormat) {
                wchar_t versionText[256];
                StringCbPrintfW(versionText, sizeof(versionText), versionFormat, CATIME_VERSION);
                SetDlgItemTextW(hwndDlg, IDC_VERSION_TEXT, versionText);
            }

            SetDlgItemTextW(hwndDlg, IDC_CREDIT_LINK, GetLocalizedString(L"特别感谢猫屋敷梨梨Official提供的图标", L"Special thanks to Neko House Lili Official for the icon"));
            SetDlgItemTextW(hwndDlg, IDC_CREDITS, GetLocalizedString(L"鸣谢", L"Credits"));
            SetDlgItemTextW(hwndDlg, IDC_BILIBILI_LINK, GetLocalizedString(L"BiliBili", L"BiliBili"));
            SetDlgItemTextW(hwndDlg, IDC_GITHUB_LINK, GetLocalizedString(L"GitHub", L"GitHub"));
            SetDlgItemTextW(hwndDlg, IDC_COPYRIGHT_LINK, GetLocalizedString(L"版权声明", L"Copyright Notice"));
            SetDlgItemTextW(hwndDlg, IDC_SUPPORT, GetLocalizedString(L"支持", L"Support"));

            char month[4];
            int day, year, hour, min, sec;

            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            const wchar_t* dateFormat = GetLocalizedString(L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                                                         L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)");

            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), dateFormat,
                    year, month_num, day, hour, min, sec);

            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

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
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndAboutDlg = NULL;
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDIT_LINK) {
                ShellExecuteW(NULL, L"open", L"https://space.bilibili.com/26087398", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_BILIBILI_LINK) {
                ShellExecuteW(NULL, L"open", URL_BILIBILI_SPACE, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_GITHUB_LINK) {
                ShellExecuteW(NULL, L"open", URL_GITHUB_REPO, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDITS) {
                ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/#thanks", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_SUPPORT) {
                ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support.html", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_COPYRIGHT_LINK) {
                ShellExecuteW(NULL, L"open", L"https://github.com/vladelaina/Catime#️copyright-notice", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            // Close all child dialogs
            EndDialog(hwndDlg, 0);
            g_hwndAboutDlg = NULL;  // Clear dialog handle
            return TRUE;

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hwndCtl = (HWND)lParam;
            
            if (GetDlgCtrlID(hwndCtl) == IDC_CREDIT_LINK || 
                GetDlgCtrlID(hwndCtl) == IDC_BILIBILI_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_GITHUB_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_CREDITS ||
                GetDlgCtrlID(hwndCtl) == IDC_COPYRIGHT_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_SUPPORT) {
                SetTextColor(hdc, 0x00D26919); // Keep the same orange color (BGR format)
                SetBkMode(hdc, TRANSPARENT);
                return (INT_PTR)GetStockObject(NULL_BRUSH);
            }
            break;
        }
    }
    return FALSE;
}

// Add DPI awareness related type definitions (if not provided by the compiler)
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
// DPI_AWARENESS_CONTEXT is a HANDLE
typedef HANDLE DPI_AWARENESS_CONTEXT;
// Related DPI context constants definition
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

// Show the About dialog
void ShowAboutDialog(HWND hwndParent) {
    // If an About dialog already exists, close it first
    if (g_hwndAboutDlg != NULL && IsWindow(g_hwndAboutDlg)) {
        EndDialog(g_hwndAboutDlg, 0);
        g_hwndAboutDlg = NULL;
    }
    
    // Save current DPI awareness context
    HANDLE hOldDpiContext = NULL;
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        // Function pointer type definitions
        typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc = 
            (GetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "GetThreadDpiAwarenessContext");
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (getThreadDpiAwarenessContextFunc && setThreadDpiAwarenessContextFunc) {
            // Save current DPI context
            hOldDpiContext = getThreadDpiAwarenessContextFunc();
            // Set to per-monitor DPI awareness V2 mode
            setThreadDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    
    // Create new About dialog
    g_hwndAboutDlg = CreateDialog(GetModuleHandle(NULL), 
                                 MAKEINTRESOURCE(IDD_ABOUT_DIALOG), 
                                 hwndParent, 
                                 AboutDlgProc);
    
    // Restore original DPI awareness context
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

// Add global variable to track pomodoro loop count setting dialog handle
static HWND g_hwndPomodoroLoopDialog = NULL;

void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (!g_hwndPomodoroLoopDialog) {
        g_hwndPomodoroLoopDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LOOP_DIALOG),
            hwndParent,
            PomodoroLoopDlgProc
        );
        if (g_hwndPomodoroLoopDialog) {
            ShowWindow(g_hwndPomodoroLoopDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndPomodoroLoopDialog);
    }
}

// Add subclassing procedure for loop count edit box
LRESULT APIENTRY LoopEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN: {
        if (wParam == VK_RETURN) {
            // Send BM_CLICK message to the parent window (dialog)
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwnd);
            return 0;
        }
        // Handle Ctrl+A select all
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        // Handle Ctrl+A character message to prevent alert sound
        if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
            return 0;
        }
        // Prevent Enter key from generating character messages for further processing to avoid alert sound
        if (wParam == VK_RETURN) { // VK_RETURN (0x0D) is the char code for Enter
            return 0;
        }
        break;
    }
    }
    return CallWindowProc(wpOrigLoopEditProc, hwnd, uMsg, wParam, lParam);
}

// Modify helper function to handle numeric input with spaces
BOOL IsValidNumberInput(const wchar_t* str) {
    // Check if empty
    if (!str || !*str) {
        return FALSE;
    }
    
    BOOL hasDigit = FALSE;  // Used to track if at least one digit is found
    wchar_t cleanStr[16] = {0};  // Used to store cleaned string
    int cleanIndex = 0;
    
    // Traverse string, ignore spaces, only keep digits
    for (int i = 0; str[i]; i++) {
        if (iswdigit(str[i])) {
            cleanStr[cleanIndex++] = str[i];
            hasDigit = TRUE;
        } else if (!iswspace(str[i])) {  // If not a space and not a digit, then invalid
            return FALSE;
        }
    }
    
    return hasDigit;  // Return TRUE as long as there is at least one digit
}

// Modify PomodoroLoopDlgProc function
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Apply multilingual support
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);
            
            // Set static text
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC, GetLocalizedString(L"请输入循环次数（1-100）：", L"Please enter loop count (1-100):"));
            
            // Set focus to edit box
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetFocus(hwndEdit);
            
            // Subclass edit control
            wpOrigLoopEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                          (LONG_PTR)LoopEditSubclassProc);
            
            return FALSE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                
                // Check if input is empty or contains only spaces
                BOOL isAllSpaces = TRUE;
                for (int i = 0; input_str[i]; i++) {
                    if (!iswspace(input_str[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (input_str[0] == L'\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndPomodoroLoopDialog = NULL;
                    return TRUE;
                }
                
                // Validate input and handle spaces
                if (!IsValidNumberInput(input_str)) {
                    ShowErrorDialog(hwndDlg);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, L"");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
                
                // Extract digits (ignoring spaces)
                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }
                
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {
                    // Update configuration file and global variables
                    WriteConfigPomodoroLoopCount(new_loop_count);
                    EndDialog(hwndDlg, IDOK);
                    g_hwndPomodoroLoopDialog = NULL;
                } else {
                    ShowErrorDialog(hwndDlg);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, L"");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroLoopDialog = NULL;
                return TRUE;
            }
            break;

        case WM_DESTROY:
            // Restore original edit control procedure
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

// Add global variable to track website URL dialog handle
static HWND g_hwndWebsiteDialog = NULL;

// Website URL input dialog procedure
INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // Set dialog as modal
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            
            // Set background and control colors
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            // Subclass the edit control to support Enter key submission
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // If URL already exists, prefill the edit box
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }
            
            // Apply multilingual support
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            
            // Set focus to edit box and select all text
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            return FALSE;  // Because we manually set the focus
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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                char url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url));
                
                // Check if the input is empty or contains only spaces
                BOOL isAllSpaces = TRUE;
                for (int i = 0; url[i]; i++) {
                    if (!isspace((unsigned char)url[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (url[0] == '\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndWebsiteDialog = NULL;
                    return TRUE;
                }
                
                // Validate URL format - simple check, should at least contain http:// or https://
                if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
                    // Add https:// prefix
                    char tempUrl[MAX_PATH] = "https://";
                    StringCbCatA(tempUrl, sizeof(tempUrl), url);
                    StringCbCopyA(url, sizeof(url), tempUrl);
                }
                
                // Update configuration
                WriteConfigTimeoutWebsite(url);
                EndDialog(hwndDlg, IDOK);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                // User cancelled, don't change timeout action
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Restore original edit control procedure
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // Release resources
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

// Show website URL input dialog
void ShowWebsiteDialog(HWND hwndParent) {
    // Use modal dialog instead of modeless dialog, so we can know whether the user confirmed or cancelled
    INT_PTR result = DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );
    
    // Only when the user clicks OK and inputs a valid URL will IDOK be returned, at which point WebsiteDialogProc has already set CLOCK_TIMEOUT_ACTION
    // If the user cancels or closes the dialog, the timeout action won't be changed
}

// Set global variable to track the Pomodoro combination dialog handle
static HWND g_hwndPomodoroComboDialog = NULL;

// Add Pomodoro combination dialog procedure
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Set background and control colors
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            // Subclass the edit control to support Enter key submission
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Read current pomodoro time options from configuration and format for display
            char currentOptions[256] = {0};
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                char timeStr[32];
                int seconds = POMODORO_TIMES[i];
                
                // Format time into human-readable format
                if (seconds >= 3600) {
                    int hours = seconds / 3600;
                    int mins = (seconds % 3600) / 60;
                    int secs = seconds % 60;
                    if (mins == 0 && secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh ", hours);
                    else if (secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh%dm ", hours, mins);
                    else
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh%dm%ds ", hours, mins, secs);
                } else if (seconds >= 60) {
                    int mins = seconds / 60;
                    int secs = seconds % 60;
                    if (secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dm ", mins);
                    else
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dm%ds ", mins, secs);
                } else {
                    StringCbPrintfA(timeStr, sizeof(timeStr), "%ds ", seconds);
                }
                
                StringCbCatA(currentOptions, sizeof(currentOptions), timeStr);
            }
            
            // Remove trailing space
            if (strlen(currentOptions) > 0 && currentOptions[strlen(currentOptions) - 1] == ' ') {
                currentOptions[strlen(currentOptions) - 1] = '\0';
            }
            
            // Set edit box text
            SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            
            // Apply multilingual support - moved here to ensure all default text is covered
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            
            // Set focus to edit box and select all text
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            return FALSE;  // Because we manually set the focus
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
                GetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, input, sizeof(input));
                
                // Parse input time format and convert to seconds array
                char *token, *saveptr;
                char input_copy[256];
                StringCbCopyA(input_copy, sizeof(input_copy), input);
                
                int times[MAX_POMODORO_TIMES] = {0};
                int times_count = 0;
                
                token = strtok_r(input_copy, " ", &saveptr);
                while (token && times_count < MAX_POMODORO_TIMES) {
                    int seconds = 0;
                    if (ParseTimeInput(token, &seconds)) {
                        times[times_count++] = seconds;
                    }
                    token = strtok_r(NULL, " ", &saveptr);
                }
                
                if (times_count > 0) {
                    // Update global variables
                    POMODORO_TIMES_COUNT = times_count;
                    for (int i = 0; i < times_count; i++) {
                        POMODORO_TIMES[i] = times[i];
                    }
                    
                    // Update basic pomodoro times
                    if (times_count > 0) POMODORO_WORK_TIME = times[0];
                    if (times_count > 1) POMODORO_SHORT_BREAK = times[1];
                    if (times_count > 2) POMODORO_LONG_BREAK = times[2];
                    
                    // Write to configuration file
                    WriteConfigPomodoroTimeOptions(times, times_count);
                }
                
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
            // Restore original edit control procedure
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // Release resources
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

void ShowPomodoroComboDialog(HWND hwndParent) {
    if (!g_hwndPomodoroComboDialog) {
        g_hwndPomodoroComboDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_COMBO_DIALOG),
            hwndParent,
            PomodoroComboDialogProc
        );
        if (g_hwndPomodoroComboDialog) {
            ShowWindow(g_hwndPomodoroComboDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndPomodoroComboDialog);
    }
}

BOOL ParseTimeInput(const char* input, int* seconds) {
    if (!input || !seconds) return FALSE;

    *seconds = 0;
    char* buffer = _strdup(input);
    if (!buffer) return FALSE;

    int len = strlen(buffer);
    char* pos = buffer;
    int value = 0;
    int tempSeconds = 0;

    while (*pos) {
        if (isdigit((unsigned char)*pos)) {
            value = 0;
            while (isdigit((unsigned char)*pos)) {
                value = value * 10 + (*pos - '0');
                pos++;
            }

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
                tempSeconds += value * 60;
            } else {
                free(buffer);
                return FALSE;
            }
        } else {
            pos++;
        }
    }

    free(buffer);
    *seconds = tempSeconds;
    return TRUE;
}

static HWND g_hwndNotificationMessagesDialog = NULL;

INT_PTR CALLBACK NotificationMessagesDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

            ReadNotificationMessagesConfig();
            
            // For handling UTF-8 Chinese characters, we need to convert to Unicode
            wchar_t wideText[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT)];
            
            // First edit box - Countdown timeout message
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            // Second edit box - Pomodoro timeout message
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            // Third edit box - Pomodoro cycle completion message
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            // Localize label text
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1, 
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2, 
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));
            
            // Localize button text
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));
            
            // Subclass edit boxes to support Ctrl+A for select all
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            // Save original window procedure
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Apply the same subclassing process to other edit boxes
            SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Select all text in the first edit box
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            
            // Set focus to the first edit box
            SetFocus(GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1));
            
            return FALSE;  // Return FALSE because we manually set focus
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
            if (LOWORD(wParam) == IDOK) {
                // Get text from edit boxes (Unicode method)
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                // Get Unicode text
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                // Convert to UTF-8
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                // Save to configuration file and update global variables
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
            // Restore original window procedure
            {
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display notification message settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays the notification message settings dialog for modifying various notification prompt texts.
 */
void ShowNotificationMessagesDialog(HWND hwndParent) {
    if (!g_hwndNotificationMessagesDialog) {
        // Ensure latest configuration values are read first
        ReadNotificationMessagesConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG), 
                 hwndParent, 
                 NotificationMessagesDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationMessagesDialog);
    }
}

// Add global variable to track notification display settings dialog handle
static HWND g_hwndNotificationDisplayDialog = NULL;

// Add notification display settings dialog procedure
INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Set window to topmost
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // Create brushes
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            
            // Read latest configuration
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            
            // Set current values to edit boxes
            char buffer[32];
            
            // Display time (seconds, support decimal) - convert milliseconds to seconds
            StringCbPrintfA(buffer, sizeof(buffer), "%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            // Remove trailing .0
            if (strlen(buffer) > 2 && buffer[strlen(buffer)-2] == '.' && buffer[strlen(buffer)-1] == '0') {
                buffer[strlen(buffer)-2] = '\0';
            }
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, buffer);
            
            // Opacity (percentage)
            StringCbPrintfA(buffer, sizeof(buffer), "%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, buffer);
            
            // Localize label text
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));
            
            // Modify edit box style, remove ES_NUMBER to allow decimal point
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);
            
            // Subclass edit boxes to support Enter key submission and input restrictions
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Set focus to time edit box
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
            if (LOWORD(wParam) == IDOK) {
                char timeStr[32] = {0};
                char opacityStr[32] = {0};
                
                // Get user input values
                GetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, timeStr, sizeof(timeStr));
                GetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, opacityStr, sizeof(opacityStr));
                
                // Use more robust method to replace Chinese period
                // First get the text in Unicode format
                wchar_t wTimeStr[32] = {0};
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wTimeStr, sizeof(wTimeStr)/sizeof(wchar_t));
                
                // Replace Chinese punctuation marks in Unicode text
                for (int i = 0; wTimeStr[i] != L'\0'; i++) {
                    // Recognize various punctuation marks as decimal point
                    if (wTimeStr[i] == L'。' ||  // Chinese period
                        wTimeStr[i] == L'，' ||  // Chinese comma
                        wTimeStr[i] == L',' ||   // English comma
                        wTimeStr[i] == L'·' ||   // Chinese middle dot
                        wTimeStr[i] == L'`' ||   // Backtick
                        wTimeStr[i] == L'：' ||  // Chinese colon
                        wTimeStr[i] == L':' ||   // English colon
                        wTimeStr[i] == L'；' ||  // Chinese semicolon
                        wTimeStr[i] == L';' ||   // English semicolon
                        wTimeStr[i] == L'/' ||   // Forward slash
                        wTimeStr[i] == L'\\' ||  // Backslash
                        wTimeStr[i] == L'~' ||   // Tilde
                        wTimeStr[i] == L'～' ||  // Full-width tilde
                        wTimeStr[i] == L'、' ||  // Chinese enumeration comma
                        wTimeStr[i] == L'．') {  // Full-width period
                        wTimeStr[i] = L'.';      // Replace with English decimal point
                    }
                }
                
                // Convert processed Unicode text back to ASCII
                WideCharToMultiByte(CP_ACP, 0, wTimeStr, -1, 
                                    timeStr, sizeof(timeStr), NULL, NULL);
                
                // Parse time (seconds) and convert to milliseconds
                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);
                
                // Allow time to be set to 0 (no notification) or at least 100 milliseconds
                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;
                
                // Parse opacity
                int opacity = atoi(opacityStr);
                
                // Ensure opacity is in range 1-100
                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;
                
                // Write to configuration
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
            
        // Add handling for WM_CLOSE message
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationDisplayDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // Restore original window procedure
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
 * 
 * Displays the notification display settings dialog for modifying notification display time and opacity.
 */
void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (!g_hwndNotificationDisplayDialog) {
        // Ensure latest configuration values are read first
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG), 
                 hwndParent, 
                 NotificationDisplayDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationDisplayDialog);
    }
}

// Add global variable to track the integrated notification settings dialog handle
static HWND g_hwndNotificationSettingsDialog = NULL;

/**
 * @brief Audio playback completion callback function
 * @param hwnd Window handle
 * 
 * When audio playback completes, changes "Stop" button back to "Test" button
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        const wchar_t* testText = GetLocalizedString(L"Test", L"Test");
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, testText);
        
        // Get dialog data
        HWND hwndTestButton = GetDlgItem(hwnd, IDC_TEST_SOUND_BUTTON);
        
        // Send WM_SETTEXT message to update button text
        if (hwndTestButton && IsWindow(hwndTestButton)) {
            SendMessageW(hwndTestButton, WM_SETTEXT, 0, (LPARAM)testText);
        }
        
        // Update global playback state
        if (g_hwndNotificationSettingsDialog == hwnd) {
            // Send message to dialog to notify state change
            SendMessage(hwnd, WM_APP + 100, 0, 0);
        }
    }
}

/**
 * @brief Populate audio dropdown box
 * @param hwndDlg Dialog handle
 */
static void PopulateSoundComboBox(HWND hwndDlg) {
    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    if (!hwndCombo) return;

    // Clear dropdown list
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    // Add "None" option
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"None", L"None"));
    
    // Add "System Beep" option
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"System Beep", L"System Beep"));

    // Get audio folder path
    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    
    // Convert to wide character path
    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);

    // Build search path
    wchar_t wSearchPath[MAX_PATH];
    StringCbPrintfW(wSearchPath, sizeof(wSearchPath), L"%s\\*.*", wAudioPath);

    // Find audio files - using Unicode version of API
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Check file extension
            wchar_t* ext = wcsrchr(find_data.cFileName, L'.');
            if (ext && (
                _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".mp3") == 0 ||
                _wcsicmp(ext, L".wav") == 0
            )) {
                // Add Unicode filename directly to dropdown
                SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)find_data.cFileName);
            }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }

    // Set currently selected audio file
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        // Check if it's the special system beep marker
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            // Select "System Beep" option (index 1)
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, NOTIFICATION_SOUND_FILE, -1, wSoundFile, MAX_PATH);
            
            // Get filename part
            wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
            if (fileName) fileName++;
            else fileName = wSoundFile;
            
            // Find and select the file in dropdown
            int index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fileName);
            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0); // Select "None"
            }
        }
    } else {
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0); // Select "None"
    }
}

/**
 * @brief Integrated notification settings dialog procedure
 * @param hwndDlg Dialog handle
 * @param msg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message processing result
 * 
 * Integrates notification content and notification display settings in a unified interface
 */
INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isPlaying = FALSE; // Add a static variable to track playback status
    static int originalVolume = 0; // Add a static variable to store original volume
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Read latest configuration to global variables
            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            
            // Save original volume value for restoration when canceling
            originalVolume = NOTIFICATION_SOUND_VOLUME;
            
            // Apply multilingual support
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);
            
            // Set notification message text - using Unicode functions
            wchar_t wideText[256];
            
            // First edit box - Countdown timeout message
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            // Second edit box - Pomodoro timeout message
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            // Third edit box - Pomodoro cycle completion message
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            // Set notification display time
            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            
            // Read notification disabled setting
            ReadNotificationDisabledConfig();
            
            // Set checkbox based on disabled state
            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, NOTIFICATION_DISABLED ? BST_CHECKED : BST_UNCHECKED);
            
            // Enable/disable time control based on state
            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !NOTIFICATION_DISABLED);
            
            // Set time control value - display actual configured time regardless of disabled state
            int totalSeconds = NOTIFICATION_TIMEOUT_MS / 1000;
            st.wHour = totalSeconds / 3600;
            st.wMinute = (totalSeconds % 3600) / 60;
            st.wSecond = totalSeconds % 60;
            
            // Set time control's initial value
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME, 
                              GDT_VALID, (LPARAM)&st);

            // Set notification opacity slider
            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, NOTIFICATION_MAX_OPACITY);
            
            // Update opacity text
            wchar_t opacityText[16];
            StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
            
            // Set notification type radio buttons
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
            
            // Populate audio dropdown
            PopulateSoundComboBox(hwndDlg);
            
            // Set volume slider
            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, NOTIFICATION_SOUND_VOLUME);
            
            // Update volume text
            wchar_t volumeText[16];
            StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", NOTIFICATION_SOUND_VOLUME);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
            
            // Reset playback state on initialization
            isPlaying = FALSE;
            
            // Set audio playback completion callback
            SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
            
            // Save dialog handle
            g_hwndNotificationSettingsDialog = hwndDlg;
            
            return TRUE;
        }
        
        case WM_HSCROLL: {
            // Handle slider drag events
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                // Get slider's current position
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                // Update volume percentage text
                wchar_t volumeText[16];
                StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
                
                // Apply volume setting in real-time
                SetAudioVolume(volume);
                
                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                // Get slider's current position
                int opacity = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                // Update opacity percentage text
                wchar_t opacityText[16];
                StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:
            // Handle notification disable checkbox state change
            if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
                // Get notification message text - using Unicode functions
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                // Get Unicode text
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                // Convert to UTF-8
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                // Get notification display time
                SYSTEMTIME st = {0};
                
                // Check if notification disable checkbox is checked
                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                
                // Save disabled state
                NOTIFICATION_DISABLED = isDisabled;
                WriteConfigNotificationDisabled(isDisabled);
                
                // Get notification time settings
                // Get notification time settings
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    // Calculate total seconds: hours*3600 + minutes*60 + seconds
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    
                    if (totalSeconds == 0) {
                        // If time is 00:00:00, set to 0 (meaning disable notifications)
                        NOTIFICATION_TIMEOUT_MS = 0;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                        
                    } else if (!isDisabled) {
                        // Only update non-zero notification time if not disabled
                        NOTIFICATION_TIMEOUT_MS = totalSeconds * 1000;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                    }
                }
                // If notifications are disabled, don't modify notification time configuration
                
                // Get notification opacity (from slider)
                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
                if (opacity >= 1 && opacity <= 100) {
                    NOTIFICATION_MAX_OPACITY = opacity;
                }
                
                // Get notification type
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
                
                // Get selected audio file
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                if (index > 0) { // 0 is "None" option
                    wchar_t wFileName[MAX_PATH];
                    SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                    
                    // Check if "System Beep" is selected
                    const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                    if (wcscmp(wFileName, sysBeepText) == 0) {
                        // Use special marker to represent system beep
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                    } else {
                        // Get audio folder path
                        char audio_path[MAX_PATH];
                        GetAudioFolderPath(audio_path, MAX_PATH);
                        
                        // Convert to UTF-8 path
                        char fileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                        
                        // Build complete file path
                        memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                        StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                    }
                } else {
                    NOTIFICATION_SOUND_FILE[0] = '\0';
                }
                
                // Get volume slider position
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                NOTIFICATION_SOUND_VOLUME = volume;
                
                // Save all settings
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
                
                // Ensure any playing audio is stopped
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                // Clean up callback before closing dialog
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                // Ensure any playing audio is stopped
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                // Restore original volume setting
                NOTIFICATION_SOUND_VOLUME = originalVolume;
                
                // Reapply original volume
                SetAudioVolume(originalVolume);
                
                // Clean up callback before closing dialog
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                if (!isPlaying) {
                    // Currently not playing, start playback and change button text to "Stop"
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                    
                    if (index > 0) { // 0 is the "None" option
                        // Get current slider volume and apply it
                        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                        int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                        SetAudioVolume(volume);
                        
                        wchar_t wFileName[MAX_PATH];
                        SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                        
                        // Temporarily save current audio settings
                        char tempSoundFile[MAX_PATH];
                        StringCbCopyA(tempSoundFile, sizeof(tempSoundFile), NOTIFICATION_SOUND_FILE);
                        
                        // Temporarily set audio file
                        const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                        if (wcscmp(wFileName, sysBeepText) == 0) {
                            // Use special marker
                            StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                        } else {
                            // Get audio folder path
                            char audio_path[MAX_PATH];
                            GetAudioFolderPath(audio_path, MAX_PATH);
                            
                            // Convert to UTF-8 path
                            char fileName[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                            
                            // Build complete file path
                            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                            StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                        }
                        
                        // Play audio
                        if (PlayNotificationSound(hwndDlg)) {
                            // Playback successful, change button text to "Stop"
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Stop", L"Stop"));
                            isPlaying = TRUE;
                        }
                        
                        // Restore previous settings
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), tempSoundFile);
                    }
                } else {
                    // Currently playing, stop playback and restore button text
                    StopNotificationSound();
                    SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Test", L"Test"));
                    isPlaying = FALSE;
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                // Get audio directory path
                char audio_path[MAX_PATH];
                GetAudioFolderPath(audio_path, MAX_PATH);
                
                // Ensure directory exists
                wchar_t wAudioPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
                
                // Open directory
                ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
                
                // Record currently selected audio file
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                // Repopulate audio dropdown
                PopulateSoundComboBox(hwndDlg);
                
                // Try to restore previous selection
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    } else {
                        // If previous selection not found, default to "None"
                        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
                    }
                }
                
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                // When dropdown is about to open, reload file list
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                
                // Record currently selected file
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                // Repopulate dropdown
                PopulateSoundComboBox(hwndDlg);
                
                // Restore previous selection
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    }
                }
                
                return TRUE;
            }
            break;
            
        // Add custom message handling for audio playback completion notification
        case WM_APP + 100:
            // Audio playback is complete, update button state
            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:
            // Make sure to stop playback when closing dialog
            if (isPlaying) {
                StopNotificationSound();
            }
            
            // Clean up callback
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // Clean up callback when dialog is destroyed
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
    return FALSE;
}

/**
 * @brief Display integrated notification settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays a unified dialog that includes both notification content and display settings
 */
void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
        // Ensure the latest configuration values are read first
        ReadNotificationMessagesConfig();
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        ReadNotificationTypeConfig();
        ReadNotificationSoundConfig();
        ReadNotificationVolumeConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG), 
                 hwndParent, 
                 NotificationSettingsDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationSettingsDialog);
    }
}