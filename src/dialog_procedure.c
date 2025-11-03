/**
 * @file dialog_procedure.c
 * @brief Dialog procedures with flexible time input and auto-focus
 * 
 * Flexible time parsing: "130t" → "1 30T" (compact target time expansion).
 * Auto-focus stealing: Aggressive thread input attachment for topmost dialogs.
 * Multi-monitor aware: Dialogs always center on primary screen.
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
#include "../include/timer.h"
#include "../include/pomodoro.h"
#include "../include/audio_player.h"
#include "../include/window_procedure.h"
#include "../include/hotkey.h"
#include "../include/dialog_language.h"
#include "../include/markdown_parser.h"

static void DrawColorSelectButton(HDC hdc, HWND hwnd);

extern wchar_t inputText[256];

static BOOL IsEmptyOrWhitespace(const wchar_t* str) {
    if (!str || str[0] == L'\0') {
        return TRUE;
    }
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL IsEmptyOrWhitespaceA(const char* str) {
    if (!str || str[0] == '\0') {
        return TRUE;
    }
    for (int i = 0; str[i]; i++) {
        if (!isspace((unsigned char)str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

static void ShowErrorAndRefocus(HWND hwndDlg, int editControlId) {
    ShowErrorDialog(hwndDlg);
    HWND hwndEdit = GetDlgItem(hwndDlg, editControlId);
    if (hwndEdit) {
        SetFocus(hwndEdit);
        SendMessage(hwndEdit, EM_SETSEL, 0, -1);
    }
}

static void FormatSecondsToString(int totalSeconds, char* buffer, size_t bufferSize) {
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    if (hours > 0 && minutes > 0 && seconds > 0) {
        snprintf(buffer, bufferSize, "%dh%dm%ds", hours, minutes, seconds);
    } else if (hours > 0 && minutes > 0) {
        snprintf(buffer, bufferSize, "%dh%dm", hours, minutes);
    } else if (hours > 0 && seconds > 0) {
        snprintf(buffer, bufferSize, "%dh%ds", hours, seconds);
    } else if (minutes > 0 && seconds > 0) {
        snprintf(buffer, bufferSize, "%dm%ds", minutes, seconds);
    } else if (hours > 0) {
        snprintf(buffer, bufferSize, "%dh", hours);
    } else if (minutes > 0) {
        snprintf(buffer, bufferSize, "%dm", minutes);
    } else {
        snprintf(buffer, bufferSize, "%ds", seconds);
    }
}

typedef struct {
    HBRUSH hBackgroundBrush;
    HBRUSH hEditBrush;
    HBRUSH hButtonBrush;
    WNDPROC wpOrigEditProc;
    void* userData;
} DialogContext;

static DialogContext* CreateDialogContext(void) {
    DialogContext* ctx = (DialogContext*)calloc(1, sizeof(DialogContext));
    if (ctx) {
        ctx->hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
        ctx->hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
        ctx->hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
    }
    return ctx;
}

static void FreeDialogContext(DialogContext* ctx) {
    if (!ctx) return;
    if (ctx->hBackgroundBrush) DeleteObject(ctx->hBackgroundBrush);
    if (ctx->hEditBrush) DeleteObject(ctx->hEditBrush);
    if (ctx->hButtonBrush) DeleteObject(ctx->hButtonBrush);
    free(ctx);
}

static DialogContext* GetDialogContext(HWND hwndDlg) {
    return (DialogContext*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
}

static void SetDialogContext(HWND hwndDlg, DialogContext* ctx) {
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)ctx);
}

static BOOL HandleDialogColorMessage(UINT msg, WPARAM wParam, 
                                     DialogContext* ctx, INT_PTR* result) {
    if (!ctx) return FALSE;
    
    switch (msg) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            *result = (INT_PTR)ctx->hBackgroundBrush;
            return TRUE;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            *result = (INT_PTR)ctx->hEditBrush;
            return TRUE;
            
        case WM_CTLCOLORBTN:
            SetBkColor((HDC)wParam, RGB(0xFD, 0xFD, 0xFD));
            *result = (INT_PTR)ctx->hButtonBrush;
            return TRUE;
    }
    
    return FALSE;
}


typedef struct {
    int controlId;
    const wchar_t* textCN;
    const wchar_t* textEN; 
    const wchar_t* url;
} AboutLinkInfo;

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

void MoveDialogToPrimaryScreen(HWND hwndDlg) {
    if (!hwndDlg || !IsWindow(hwndDlg)) {
        return;
    }
    
    HMONITOR hPrimaryMonitor = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    
    if (!GetMonitorInfo(hPrimaryMonitor, &mi)) {
        return;
    }
    
    RECT dialogRect;
    if (!GetWindowRect(hwndDlg, &dialogRect)) {
        return;
    }
    
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    
    int primaryWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    int primaryHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    
    int newX = mi.rcMonitor.left + (primaryWidth - dialogWidth) / 2;
    int newY = mi.rcMonitor.top + (primaryHeight - dialogHeight) / 2;
    
    SetWindowPos(hwndDlg, HWND_TOPMOST, newX, newY, 0, 0, 
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = GetDialogContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            ctx = CreateDialogContext();
            if (!ctx) return FALSE;
            
            ctx->userData = (void*)lParam;
            SetDialogContext(hwndDlg, ctx);

            g_hwndInputDialog = hwndDlg;

            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            MoveDialogToPrimaryScreen(hwndDlg);

            DWORD dlgId = (DWORD)(LONG_PTR)ctx->userData;

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);

            ctx->wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            if (!wpOrigEditProc) wpOrigEditProc = ctx->wpOrigEditProc;

            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) {
                char currentOptions[256] = {0};
                for (int i = 0; i < time_options_count; i++) {
                    char timeStr[32];
                    FormatSecondsToString(time_options[i], timeStr, sizeof(timeStr));
                    
                    if (i > 0) {
                        StringCbCatA(currentOptions, sizeof(currentOptions), " ");
                    }
                    StringCbCatA(currentOptions, sizeof(currentOptions), timeStr);
                }
                
                wchar_t wcurrentOptions[256];
                MultiByteToWideChar(CP_UTF8, 0, currentOptions, -1, wcurrentOptions, 256);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wcurrentOptions);
            } else if (dlgId == CLOCK_IDD_STARTUP_DIALOG) {
                extern int CLOCK_DEFAULT_START_TIME;
                if (CLOCK_DEFAULT_START_TIME > 0) {
                    char timeStr[64];
                    FormatSecondsToString(CLOCK_DEFAULT_START_TIME, timeStr, sizeof(timeStr));
                    
                    wchar_t wtimeStr[64];
                    MultiByteToWideChar(CP_UTF8, 0, timeStr, -1, wtimeStr, 64);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wtimeStr);
                }
            }

            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            SetFocus(hwndEdit);

            /* Aggressive focus handling for topmost dialogs */
            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);

            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);

            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);

            SetTimer(hwndDlg, 9999, 50, NULL);

            /* Release stuck modifier keys (hotkey aftermath) */
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
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (HandleDialogColorMessage(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText)/sizeof(wchar_t));

                if (IsEmptyOrWhitespace(inputText)) {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }

                int dialogId = (int)(LONG_PTR)ctx->userData;
                
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
                        time_options_count = 0;
                        char optionsCopy[256];
                        strncpy(optionsCopy, options, sizeof(optionsCopy) - 1);
                        optionsCopy[sizeof(optionsCopy) - 1] = '\0';
                        char* tok = strtok(optionsCopy, ",");
                        while (tok && time_options_count < 10) {
                            while (*tok == ' ') tok++;
                            time_options[time_options_count++] = atoi(tok);
                            tok = strtok(NULL, ",");
                        }
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else {
                        ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
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
                        ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
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
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && ctx->wpOrigEditProc) {
                    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                }
                FreeDialogContext(ctx);
            }
            g_hwndInputDialog = NULL;
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

            for (int i = 0; i < g_aboutLinkInfoCount; i++) {
                const wchar_t* linkText = GetLocalizedString(g_aboutLinkInfos[i].textCN, g_aboutLinkInfos[i].textEN);
                SetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, linkText);
            }

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
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndAboutDlg = NULL;
                return TRUE;
            }
            
            for (int i = 0; i < g_aboutLinkInfoCount; i++) {
                if (LOWORD(wParam) == g_aboutLinkInfos[i].controlId && HIWORD(wParam) == STN_CLICKED) {
                    ShellExecuteW(NULL, L"open", g_aboutLinkInfos[i].url, NULL, NULL, SW_SHOWNORMAL);
                    return TRUE;
                }
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            
            for (int i = 0; i < g_aboutLinkInfoCount; i++) {
                if (lpDrawItem->CtlID == g_aboutLinkInfos[i].controlId) {
                    RECT rect = lpDrawItem->rcItem;
                    HDC hdc = lpDrawItem->hDC;
                    
                    FillRect(hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
                    
                    wchar_t text[256];
                    GetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, text, sizeof(text)/sizeof(text[0]));
                    
                    HFONT hFont = (HFONT)SendMessage(hwndDlg, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    /* Orange color for links */
                    SetTextColor(hdc, 0x00D26919);
                    SetBkMode(hdc, TRANSPARENT);
                    
                    DrawTextW(hdc, text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(hdc, hOldFont);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CLOSE:
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


void ShowAboutDialog(HWND hwndParent) {
    if (g_hwndAboutDlg != NULL && IsWindow(g_hwndAboutDlg)) {
        EndDialog(g_hwndAboutDlg, 0);
        g_hwndAboutDlg = NULL;
    }
    
    /* DPI awareness for high-DPI displays */
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
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                
                if (IsEmptyOrWhitespace(input_str)) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndPomodoroLoopDialog = NULL;
                    return TRUE;
                }
                
                if (!IsValidNumberInput(input_str)) {
                    ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                
                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }
                
                /* Range: 1-100 */
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {
                    WriteConfigPomodoroLoopCount(new_loop_count);
                    EndDialog(hwndDlg, IDOK);
                    g_hwndPomodoroLoopDialog = NULL;
                } else {
                    ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
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
    DialogContext* ctx = GetDialogContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            ctx = CreateDialogContext();
            if (!ctx) return FALSE;
            
            ctx->userData = (void*)lParam;
            SetDialogContext(hwndDlg, ctx);
            
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            ctx->wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            if (!wpOrigEditProc) wpOrigEditProc = ctx->wpOrigEditProc;
            
            if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }
            
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (HandleDialogColorMessage(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                wchar_t url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url)/sizeof(wchar_t));
                
                if (IsEmptyOrWhitespace(url)) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndWebsiteDialog = NULL;
                    return TRUE;
                }
                
                /* Auto-prepend https:// if no protocol */
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
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && ctx->wpOrigEditProc) {
                    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                }
                FreeDialogContext(ctx);
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
    DialogContext* ctx = GetDialogContext(hwndDlg);
    
    switch (msg) {
        case WM_INITDIALOG: {
            ctx = CreateDialogContext();
            if (!ctx) return FALSE;
            SetDialogContext(hwndDlg, ctx);
            
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            ctx->wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            if (!wpOrigEditProc) wpOrigEditProc = ctx->wpOrigEditProc;
            
            wchar_t currentOptions[256] = {0};
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                char timeStrA[32];
                wchar_t timeStr[32];
                FormatSecondsToString(POMODORO_TIMES[i], timeStrA, sizeof(timeStrA));
                MultiByteToWideChar(CP_UTF8, 0, timeStrA, -1, timeStr, 32);
                StringCbCatW(currentOptions, sizeof(currentOptions), timeStr);
                StringCbCatW(currentOptions, sizeof(currentOptions), L" ");
            }
            
            if (wcslen(currentOptions) > 0 && currentOptions[wcslen(currentOptions) - 1] == L' ') {
                currentOptions[wcslen(currentOptions) - 1] = L'\0';
            }
            
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (HandleDialogColorMessage(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || LOWORD(wParam) == IDOK) {
                char input[256] = {0};

                wchar_t winput[256];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, winput, sizeof(winput)/sizeof(wchar_t));
                WideCharToMultiByte(CP_UTF8, 0, winput, -1, input, sizeof(input), NULL, NULL);
                
                if (IsEmptyOrWhitespaceA(input)) {
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
                    ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
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
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && ctx->wpOrigEditProc) {
                    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                }
                FreeDialogContext(ctx);
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

/* Flexible format: "25m", "1h30m", "25 5 15" (space-separated) */
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
                /* Default to minutes */
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
    DialogContext* ctx = GetDialogContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            ctx = CreateDialogContext();
            if (!ctx) return FALSE;
            SetDialogContext(hwndDlg, ctx);
            
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            MoveDialogToPrimaryScreen(hwndDlg);

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
            
            ctx->wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            if (!wpOrigEditProc) wpOrigEditProc = ctx->wpOrigEditProc;
            
            SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            
            SetFocus(GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1));
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            INT_PTR result;
            if (HandleDialogColorMessage(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }
        
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
            if (ctx) {
                if (ctx->wpOrigEditProc) {
                    HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
                    HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
                    HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
                    
                    if (hEdit1) SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                    if (hEdit2) SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                    if (hEdit3) SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                }
                FreeDialogContext(ctx);
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
    DialogContext* ctx = GetDialogContext(hwndDlg);
    
    switch (msg) {
        case WM_INITDIALOG: {
            ctx = CreateDialogContext();
            if (!ctx) return FALSE;
            SetDialogContext(hwndDlg, ctx);
            
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            
            wchar_t wbuffer[32];
            
            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            /* Remove trailing .0 */
            if (wcslen(wbuffer) > 2 && wbuffer[wcslen(wbuffer)-2] == L'.' && wbuffer[wcslen(wbuffer)-1] == L'0') {
                wbuffer[wcslen(wbuffer)-2] = L'\0';
            }
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wbuffer);
            
            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wbuffer);
            
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));
            
            /* Allow decimal input */
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);
            
            ctx->wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            if (!wpOrigEditProc) wpOrigEditProc = ctx->wpOrigEditProc;
            
            SetFocus(hEditTime);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            INT_PTR result;
            if (HandleDialogColorMessage(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char timeStr[32] = {0};
                char opacityStr[32] = {0};
                
                wchar_t wtimeStr[32], wopacityStr[32];
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wtimeStr, sizeof(wtimeStr)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wopacityStr, sizeof(wopacityStr)/sizeof(wchar_t));
                
                WideCharToMultiByte(CP_UTF8, 0, wtimeStr, -1, timeStr, sizeof(timeStr), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wopacityStr, -1, opacityStr, sizeof(opacityStr), NULL, NULL);
                
                /* Normalize decimal separators (Chinese, Japanese punctuation → .) */
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
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeStr, -1, 
                                    timeStr, sizeof(timeStr), NULL, NULL);
                
                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);
                
                /* Minimum 100ms */
                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;
                
                int opacity = atoi(opacityStr);
                
                /* Clamp: 1-100 */
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
            if (ctx) {
                if (ctx->wpOrigEditProc) {
                    HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
                    HWND hEditOpacity = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                    
                    if (hEditTime) SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                    if (hEditOpacity) SetWindowLongPtr(hEditOpacity, GWLP_WNDPROC, (LONG_PTR)ctx->wpOrigEditProc);
                }
                FreeDialogContext(ctx);
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

    /* Add supported audio files (.flac, .mp3, .wav) */
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
            /* System beep is index 1 */
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
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
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
                        /* 0 = no timeout */
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
            
        /* Audio playback complete */
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
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
    return FALSE;
}

void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
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

static MarkdownLink* g_links = NULL;
static int g_linkCount = 0;
static wchar_t* g_displayText = NULL;

INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            SetWindowTextW(hwndDlg, GetLocalizedString(L"自定义字体功能说明及版权提示", L"Custom Font Feature License Agreement"));
            
            const wchar_t* licenseText = GetLocalizedString(
                L"本功能将加载以下文件夹中的所有字体文件（包括子文件夹）：\n\nC:\\Users\\[您的当前用户名]\\AppData\\Local\\Catime\\resources\\fonts\n\n请务必注意： 任何版权风险和法律责任都将由您个人承担，本软件不承担任何责任。\n\n────────────────────────────────────────\n\n为了避免版权风险：\n您可以前往 [Google Fonts](https://fonts.google.com/?preview.text=1234567890:) 下载大量可免费商用的字体。",
                L"FontLicenseAgreementText");
            
            if (!ParseMarkdownLinks(licenseText, &g_displayText, &g_links, &g_linkCount)) {
                g_displayText = _wcsdup(licenseText);
                g_links = NULL;
                g_linkCount = 0;
            }
            
            /* Text drawn by WM_DRAWITEM (SS_OWNERDRAW) */
            
            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_AGREE_BTN, GetLocalizedString(L"同意", L"Agree"));
            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_CANCEL_BTN, GetLocalizedString(L"取消", L"Cancel"));
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FONT_LICENSE_AGREE_BTN:
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
                    if (HIWORD(wParam) == STN_CLICKED) {
                        POINT pt;
                        GetCursorPos(&pt);
                        ScreenToClient(GetDlgItem(hwndDlg, IDC_FONT_LICENSE_TEXT), &pt);
                        
                        if (HandleMarkdownClick(g_links, g_linkCount, pt)) {
                            return TRUE;
                        }
                    }
                    return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_FONT_LICENSE_TEXT) {
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

INT_PTR ShowFontLicenseDialog(HWND hwndParent) {
    /* Parent prevents taskbar icon */
    return DialogBoxW(GetModuleHandle(NULL), 
                     MAKEINTRESOURCE(IDD_FONT_LICENSE_DIALOG), 
                     hwndParent,
                     FontLicenseDlgProc);
}