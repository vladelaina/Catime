/**
 * @file dialog_info.c
 * @brief Informational dialogs implementation
 */

#include "dialog/dialog_info.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "dialog/dialog_language.h"
#include "config.h"
#include "../resource/resource.h"
#include <shellapi.h>
#include <strsafe.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CATIME_VERSION L"v2.2.0"
#define ABOUT_ICON_SIZE 96

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
typedef HANDLE DPI_AWARENESS_CONTEXT;
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

/* ============================================================================
 * About Dialog Data
 * ============================================================================ */

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

/* ============================================================================
 * Global State
 * ============================================================================ */

char g_websiteInput[512] = {0};

/* ============================================================================
 * About Dialog Implementation
 * ============================================================================ */

void ShowAboutDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_ABOUT)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_ABOUT);
        EndDialog(existing, 0);
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

    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(IDD_ABOUT_DIALOG),
              hwndParent,
              AboutDlgProc);

    if (hOldDpiContext && hUser32) {
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc =
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
}

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HICON hLargeIcon = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_ABOUT, hwndDlg);

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

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            return TRUE;
        }

        case WM_DESTROY:
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
                hLargeIcon = NULL;
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_ABOUT);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
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
            return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Website Input Dialog Implementation
 * ============================================================================ */

void ShowWebsiteDialog(HWND hwndParent) {
    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
              hwndParent,
              WebsiteDialogProc);
}

INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_WEBSITE, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;

            ctx->userData = (void*)lParam;
            Dialog_SetContext(hwndDlg, ctx);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);

            extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[];
            if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);

            return FALSE;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (Dialog_HandleColorMessages(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                wchar_t url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url)/sizeof(wchar_t));

                if (Dialog_IsEmptyOrWhitespace(url)) {
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }

                /* Auto-prepend https:// if no protocol */
                if (wcsncmp(url, L"http://", 7) != 0 && wcsncmp(url, L"https://", 8) != 0) {
                    wchar_t tempUrl[MAX_PATH] = L"https://";
                    StringCbCatW(tempUrl, sizeof(tempUrl), url);
                    StringCbCopyW(url, sizeof(url), tempUrl);
                }

                extern void WriteConfigTimeoutWebsite(const char* url);
                char urlUtf8[MAX_PATH * 3];
                WideCharToMultiByte(CP_UTF8, 0, url, -1, urlUtf8, sizeof(urlUtf8), NULL, NULL);
                WriteConfigTimeoutWebsite(urlUtf8);
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;

        case WM_DESTROY:
            if (ctx) {
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit) {
                    Dialog_UnsubclassEdit(hwndEdit, ctx);
                }
                Dialog_FreeContext(ctx);
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_WEBSITE);
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

/* ============================================================================
 * Font License Dialog Implementation (Stub)
 * ============================================================================ */

/**
 * @note Full implementation was in original dialog_procedure.c (lines 1938-2057)
 *       This is a minimal stub for compilation.
 *       TODO: Migrate full implementation with markdown parsing if needed.
 */
INT_PTR ShowFontLicenseDialog(HWND hwndParent) {
    /* Temporary stub - always return IDOK */
    /* Full implementation should be migrated from original dialog_procedure.c */
    MessageBoxW(hwndParent, 
               L"Font License Dialog\n\nThis is a placeholder implementation.\nPlease migrate the full dialog from original code if needed.",
               L"Font License",
               MB_OK | MB_ICONINFORMATION);
    return IDOK;
}

INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Stub implementation */
    (void)wParam;
    (void)lParam;
    
    switch (msg) {
        case WM_INITDIALOG:
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

/* ============================================================================
 * CLI Help Dialog - Implemented in cli.c
 * ============================================================================ */

/* 
 * ShowCliHelpDialog - Implemented in cli.c
 * Kept in CLI module for better cohesion.
 */

