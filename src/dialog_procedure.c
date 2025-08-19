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

extern wchar_t inputText[256];

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
    DialogBoxW(GetModuleHandle(NULL),
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

                extern int time_options[];
                extern int time_options_count;
                
                char currentOptions[256] = {0};
                for (int i = 0; i < time_options_count; i++) {
                    char timeStr[32];
                    int totalSeconds = time_options[i];
                    

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
                


                wchar_t wcurrentOptions[256];
                MultiByteToWideChar(CP_ACP, 0, currentOptions, -1, wcurrentOptions, 256);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcurrentOptions);
            } else if (dlgId == CLOCK_IDD_STARTUP_DIALOG) {

                extern int CLOCK_DEFAULT_START_TIME;
                if (CLOCK_DEFAULT_START_TIME > 0) {

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
                    
    
                    wchar_t wtimeStr[64];
                    MultiByteToWideChar(CP_ACP, 0, timeStr, -1, wtimeStr, 64);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                }
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
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText)/sizeof(wchar_t));

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
                
                if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {


                    char inputCopy[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputCopy, sizeof(inputCopy), NULL, NULL);
                    
                    char* token = strtok(inputCopy, " ");
                    char options[256] = {0};
                    int valid = 1;
                    int count = 0;
                    
                    while (token && count < MAX_TIME_OPTIONS) {
                        int seconds = 0;

                        if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                            valid = 0;
                            break;
                        }
                        
                        if (count > 0) {
                            StringCbCatA(options, sizeof(options), ",");
                        }
                        

                        char secondsStr[32];
                        snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
                        StringCbCatA(options, sizeof(options), secondsStr);
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

                        ShowErrorDialog(hwndDlg);

                        HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                        SetFocus(hwndEdit);
                        SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                        return TRUE;
                    }
                } else {


                    char inputUtf8[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputUtf8, sizeof(inputUtf8), NULL, NULL);
                    
                    int total_seconds;
                    if (ParseInput(inputUtf8, &total_seconds)) {
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

            EndDialog(hwndDlg, 0);
            g_hwndAboutDlg = NULL;
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
                SetTextColor(hdc, 0x00D26919);
                SetBkMode(hdc, TRANSPARENT);
                return (INT_PTR)GetStockObject(NULL_BRUSH);
            }
            break;
        }
    }
    return FALSE;
}


#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2

typedef HANDLE DPI_AWARENESS_CONTEXT;

#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif


void ShowAboutDialog(HWND hwndParent) {

    if (g_hwndAboutDlg != NULL && IsWindow(g_hwndAboutDlg)) {
        EndDialog(g_hwndAboutDlg, 0);
        g_hwndAboutDlg = NULL;
    }
    

    HANDLE hOldDpiContext = NULL;
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {

        typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc = 
            (GetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "GetThreadDpiAwarenessContext");
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (getThreadDpiAwarenessContextFunc && setThreadDpiAwarenessContextFunc) {

            hOldDpiContext = getThreadDpiAwarenessContextFunc();

            setThreadDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    

    g_hwndAboutDlg = CreateDialogW(GetModuleHandle(NULL), 
                                  MAKEINTRESOURCE(IDD_ABOUT_DIALOG), 
                                  hwndParent, 
                                  AboutDlgProc);
    

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

void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (!g_hwndPomodoroLoopDialog) {
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
        SetForegroundWindow(g_hwndPomodoroLoopDialog);
    }
}


LRESULT APIENTRY LoopEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN: {
        if (wParam == VK_RETURN) {

            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwnd);
            return 0;
        }

        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    }
    case WM_CHAR: {

        if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
            return 0;
        }

        if (wParam == VK_RETURN) {
            return 0;
        }
        break;
    }
    }
    return CallWindowProc(wpOrigLoopEditProc, hwnd, uMsg, wParam, lParam);
}


BOOL IsValidNumberInput(const wchar_t* str) {

    if (!str || !*str) {
        return FALSE;
    }
    
    BOOL hasDigit = FALSE;
    wchar_t cleanStr[16] = {0};
    int cleanIndex = 0;
    

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


INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);
            
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC, GetLocalizedString(L"请输入循环次数（1-100）：", L"Please enter loop count (1-100):"));
            
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetFocus(hwndEdit);
            
            wpOrigLoopEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                          (LONG_PTR)LoopEditSubclassProc);
            
            return FALSE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                
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
                
                if (!IsValidNumberInput(input_str)) {
                    ShowErrorDialog(hwndDlg);

                    HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                    SetFocus(hwndEdit);
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                    return TRUE;
                }
                

                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }
                
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {

                    WriteConfigPomodoroLoopCount(new_loop_count);
                    EndDialog(hwndDlg, IDOK);
                    g_hwndPomodoroLoopDialog = NULL;
                } else {
                    ShowErrorDialog(hwndDlg);

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


INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {

            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            

            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            

            if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }
            

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            

            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                wchar_t url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url)/sizeof(wchar_t));
                

                BOOL isAllSpaces = TRUE;
                for (int i = 0; url[i]; i++) {
                    if (!iswspace(url[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (url[0] == L'\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndWebsiteDialog = NULL;
                    return TRUE;
                }
                

                if (wcsncmp(url, L"http://", 7) != 0 && wcsncmp(url, L"https://", 8) != 0) {

                    wchar_t tempUrl[MAX_PATH] = L"https://";
                    StringCbCatW(tempUrl, sizeof(tempUrl), url);
                    StringCbCopyW(url, sizeof(url), tempUrl);
                }
                

                char urlUtf8[MAX_PATH * 3];
                WideCharToMultiByte(CP_UTF8, 0, url, -1, urlUtf8, sizeof(urlUtf8), NULL, NULL);
                WriteConfigTimeoutWebsite(urlUtf8);
                EndDialog(hwndDlg, IDOK);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {

                EndDialog(hwndDlg, IDCANCEL);
                g_hwndWebsiteDialog = NULL;
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
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndWebsiteDialog = NULL;
            return TRUE;
    }
    
    return FALSE;
}


void ShowWebsiteDialog(HWND hwndParent) {

    INT_PTR result = DialogBoxW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );
    

}


static HWND g_hwndPomodoroComboDialog = NULL;


INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {

            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            

            wchar_t currentOptions[256] = {0};
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                wchar_t timeStr[32];
                int seconds = POMODORO_TIMES[i];
                

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
            

            if (wcslen(currentOptions) > 0 && currentOptions[wcslen(currentOptions) - 1] == L' ') {
                currentOptions[wcslen(currentOptions) - 1] = L'\0';
            }
            

            SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            

            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
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
                WideCharToMultiByte(CP_ACP, 0, winput, -1, input, sizeof(input), NULL, NULL);
                

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

void ShowPomodoroComboDialog(HWND hwndParent) {
    if (!g_hwndPomodoroComboDialog) {
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
        SetForegroundWindow(g_hwndPomodoroComboDialog);
    }
}

BOOL ParseTimeInput(const char* input, int* seconds) {
    if (!input || !seconds) return FALSE;

    *seconds = 0;
    

    char buffer[256];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* pos = buffer;
    int tempSeconds = 0;

    while (*pos) {

        while (*pos == ' ' || *pos == '\t') pos++;
        if (*pos == '\0') break;
        

        if (isdigit((unsigned char)*pos)) {
            int value = 0;
            while (isdigit((unsigned char)*pos)) {
                value = value * 10 + (*pos - '0');
                pos++;
            }


            while (*pos == ' ' || *pos == '\t') pos++;


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
                break;
            } else {

                return FALSE;
            }
        } else {

            return FALSE;
        }
    }

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
            

            wchar_t wideText[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT)];
            

            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            

            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            

            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            

            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1, 
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2, 
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));
            

            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));
            

            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            

            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            

            SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            

            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            

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
            if (LOWORD(wParam) == IDOK) {

                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                

                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                

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

void ShowNotificationMessagesDialog(HWND hwndParent) {
    if (!g_hwndNotificationMessagesDialog) {

        ReadNotificationMessagesConfig();
        
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG), 
                  hwndParent, 
                  NotificationMessagesDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationMessagesDialog);
    }
}


static HWND g_hwndNotificationDisplayDialog = NULL;


INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {

            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            

            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            

            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            

            wchar_t wbuffer[32];
            

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);

            if (wcslen(wbuffer) > 2 && wbuffer[wcslen(wbuffer)-2] == L'.' && wbuffer[wcslen(wbuffer)-1] == L'0') {
                wbuffer[wcslen(wbuffer)-2] = L'\0';
            }
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wbuffer);
            

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wbuffer);
            

            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));
            

            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);
            

            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            

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
                

                wchar_t wtimeStr[32], wopacityStr[32];
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wtimeStr, sizeof(wtimeStr)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wopacityStr, sizeof(wopacityStr)/sizeof(wchar_t));
                

                WideCharToMultiByte(CP_ACP, 0, wtimeStr, -1, timeStr, sizeof(timeStr), NULL, NULL);
                WideCharToMultiByte(CP_ACP, 0, wopacityStr, -1, opacityStr, sizeof(opacityStr), NULL, NULL);
                


                wchar_t wTimeStr[32] = {0};
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wTimeStr, sizeof(wTimeStr)/sizeof(wchar_t));
                

                for (int i = 0; wTimeStr[i] != L'\0'; i++) {

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
                

                WideCharToMultiByte(CP_ACP, 0, wTimeStr, -1, 
                                    timeStr, sizeof(timeStr), NULL, NULL);
                

                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);
                

                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;
                

                int opacity = atoi(opacityStr);
                

                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;
                

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

void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (!g_hwndNotificationDisplayDialog) {

        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG), 
                  hwndParent, 
                  NotificationDisplayDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationDisplayDialog);
    }
}


static HWND g_hwndNotificationSettingsDialog = NULL;

static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        const wchar_t* testText = GetLocalizedString(L"Test", L"Test");
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, testText);
        

        HWND hwndTestButton = GetDlgItem(hwnd, IDC_TEST_SOUND_BUTTON);
        

        if (hwndTestButton && IsWindow(hwndTestButton)) {
            SendMessageW(hwndTestButton, WM_SETTEXT, 0, (LPARAM)testText);
        }
        

        if (g_hwndNotificationSettingsDialog == hwnd) {

            SendMessage(hwnd, WM_APP + 100, 0, 0);
        }
    }
}

static void PopulateSoundComboBox(HWND hwndDlg) {
    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    if (!hwndCombo) return;


    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);


    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"None", L"None"));
    

    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"System Beep", L"System Beep"));


    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    

    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);


    wchar_t wSearchPath[MAX_PATH];
    StringCbPrintfW(wSearchPath, sizeof(wSearchPath), L"%s\\*.*", wAudioPath);


    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {

            wchar_t* ext = wcsrchr(find_data.cFileName, L'.');
            if (ext && (
                _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".mp3") == 0 ||
                _wcsicmp(ext, L".wav") == 0
            )) {

                SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)find_data.cFileName);
            }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }


    if (NOTIFICATION_SOUND_FILE[0] != '\0') {

        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {

            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, NOTIFICATION_SOUND_FILE, -1, wSoundFile, MAX_PATH);
            

            wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
            if (fileName) fileName++;
            else fileName = wSoundFile;
            

            int index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fileName);
            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
            }
        }
    } else {
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
    }
}

INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isPlaying = FALSE;
    static int originalVolume = 0;
    
    switch (msg) {
        case WM_INITDIALOG: {

            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            

            originalVolume = NOTIFICATION_SOUND_VOLUME;
            

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);
            

            wchar_t wideText[256];
            

            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            

            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            

            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            

            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            

            ReadNotificationDisabledConfig();
            

            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, NOTIFICATION_DISABLED ? BST_CHECKED : BST_UNCHECKED);
            

            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !NOTIFICATION_DISABLED);
            

            int totalSeconds = NOTIFICATION_TIMEOUT_MS / 1000;
            st.wHour = totalSeconds / 3600;
            st.wMinute = (totalSeconds % 3600) / 60;
            st.wSecond = totalSeconds % 60;
            

            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME, 
                              GDT_VALID, (LPARAM)&st);


            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, NOTIFICATION_MAX_OPACITY);
            

            wchar_t opacityText[16];
            StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
            

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
            

            PopulateSoundComboBox(hwndDlg);
            

            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, NOTIFICATION_SOUND_VOLUME);
            

            wchar_t volumeText[16];
            StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", NOTIFICATION_SOUND_VOLUME);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
            

            isPlaying = FALSE;
            

            SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
            

            g_hwndNotificationSettingsDialog = hwndDlg;
            
            return TRUE;
        }
        
        case WM_HSCROLL: {

            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {

                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                

                wchar_t volumeText[16];
                StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
                

                SetAudioVolume(volume);
                
                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {

                int opacity = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                

                wchar_t opacityText[16];
                StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:

            if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {

                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                

                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                

                SYSTEMTIME st = {0};
                

                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                

                NOTIFICATION_DISABLED = isDisabled;
                WriteConfigNotificationDisabled(isDisabled);
                


                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {

                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    
                    if (totalSeconds == 0) {

                        NOTIFICATION_TIMEOUT_MS = 0;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                        
                    } else if (!isDisabled) {

                        NOTIFICATION_TIMEOUT_MS = totalSeconds * 1000;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                    }
                }

                

                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
                if (opacity >= 1 && opacity <= 100) {
                    NOTIFICATION_MAX_OPACITY = opacity;
                }
                

                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
                

                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                if (index > 0) {
                    wchar_t wFileName[MAX_PATH];
                    SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                    

                    const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                    if (wcscmp(wFileName, sysBeepText) == 0) {

                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                    } else {
                    
                        char audio_path[MAX_PATH];
                        GetAudioFolderPath(audio_path, MAX_PATH);
                        
                        char fileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                        

                        memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                        StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                    }
                } else {
                    NOTIFICATION_SOUND_FILE[0] = '\0';
                }
                

                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                NOTIFICATION_SOUND_VOLUME = volume;
                

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
                

                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                

                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {

                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                

                NOTIFICATION_SOUND_VOLUME = originalVolume;
                

                SetAudioVolume(originalVolume);
                

                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                if (!isPlaying) {

                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                    
                    if (index > 0) {

                        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                        int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                        SetAudioVolume(volume);
                        
                        wchar_t wFileName[MAX_PATH];
                        SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                        

                        char tempSoundFile[MAX_PATH];
                        StringCbCopyA(tempSoundFile, sizeof(tempSoundFile), NOTIFICATION_SOUND_FILE);
                        

                        const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                        if (wcscmp(wFileName, sysBeepText) == 0) {

                            StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                        } else {
                        
                            char audio_path[MAX_PATH];
                            GetAudioFolderPath(audio_path, MAX_PATH);
                            
                            char fileName[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                            
    
                            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                            StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                        }
                        

                        if (PlayNotificationSound(hwndDlg)) {

                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Stop", L"Stop"));
                            isPlaying = TRUE;
                        }
                        

                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), tempSoundFile);
                    }
                } else {

                    StopNotificationSound();
                    SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Test", L"Test"));
                    isPlaying = FALSE;
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {

                char audio_path[MAX_PATH];
                GetAudioFolderPath(audio_path, MAX_PATH);
                

                wchar_t wAudioPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
                

                ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
                

                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                

                PopulateSoundComboBox(hwndDlg);
                

                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    } else {

                        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
                    }
                }
                
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {

                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                

                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                

                PopulateSoundComboBox(hwndDlg);
                

                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    }
                }
                
                return TRUE;
            }
            break;
            

        case WM_APP + 100:

            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:

            if (isPlaying) {
                StopNotificationSound();
            }
            

            SetAudioPlaybackCompleteCallback(NULL, NULL);
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // Clean up callback
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
    return FALSE;
}

void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
        // Ensure the latest configuration values are read first
        ReadNotificationMessagesConfig();
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        ReadNotificationTypeConfig();
        ReadNotificationSoundConfig();
        ReadNotificationVolumeConfig();
        
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG), 
                  hwndParent, 
                  NotificationSettingsDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationSettingsDialog);
    }
}