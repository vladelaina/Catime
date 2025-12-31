/**
 * @file dialog_info.c
 * @brief Informational dialogs implementation
 */

#include "dialog/dialog_info.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "dialog/dialog_language.h"
#include "config.h"
#include "markdown/markdown_parser.h"
#include "../resource/resource.h"
#include <shellapi.h>
#include <strsafe.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

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
    {IDC_CREDIT_LINK, NULL, L"Special thanks to 猫屋敷梨梨Official for the icon", L"https://space.bilibili.com/26087398"},
    {IDC_COPYRIGHT, NULL, L"Copyright © 2025 vladelaina", L"https://vladelaina.com/"},
    {IDC_CREDITS, NULL, L"Credits", L"https://vladelaina.github.io/Catime/#thanks"},
    {IDC_BILIBILI_LINK, NULL, L"BiliBili", L"https://space.bilibili.com/1862395225"},
    {IDC_GITHUB_LINK, NULL, L"GitHub", L"https://github.com/vladelaina/Catime"},
    {IDC_COPYRIGHT_LINK, NULL, L"Copyright Notice", L"https://github.com/vladelaina/Catime#️copyright-notice"},
    {IDC_SUPPORT, NULL, L"Discord", L"https://discord.com/invite/W3tW2gtp6g"}
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
        SetForegroundWindow(existing);
        return;
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

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(IDD_ABOUT_DIALOG),
        hwndParent,
        AboutDlgProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }

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

            /* Convert build time from UTC+8 to local time */
            SYSTEMTIME buildTimeUTC8 = {0};
            buildTimeUTC8.wYear = year;
            buildTimeUTC8.wMonth = month_num;
            buildTimeUTC8.wDay = day;
            buildTimeUTC8.wHour = hour;
            buildTimeUTC8.wMinute = min;
            buildTimeUTC8.wSecond = sec;

            /* Convert to FILETIME (UTC+8 is 8 hours ahead of UTC) */
            FILETIME fileTime;
            SystemTimeToFileTime(&buildTimeUTC8, &fileTime);

            /* Subtract 8 hours to get UTC time */
            ULARGE_INTEGER uli;
            uli.LowPart = fileTime.dwLowDateTime;
            uli.HighPart = fileTime.dwHighDateTime;
            uli.QuadPart -= (ULONGLONG)8 * 60 * 60 * 10000000;  /* 8 hours in 100-nanosecond intervals */
            fileTime.dwLowDateTime = uli.LowPart;
            fileTime.dwHighDateTime = uli.HighPart;

            /* Convert to local time */
            FILETIME localFileTime;
            FileTimeToLocalFileTime(&fileTime, &localFileTime);

            SYSTEMTIME localTime;
            FileTimeToSystemTime(&localFileTime, &localTime);

            const wchar_t* buildDateLabel = GetLocalizedString(NULL, L"Build Date:");

            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), L"%s %04d/%02d/%02d %02d:%02d:%02d",
                    buildDateLabel, localTime.wYear, localTime.wMonth, localTime.wDay,
                    localTime.wHour, localTime.wMinute, localTime.wSecond);

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
                DestroyWindow(hwndDlg);
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

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    return FALSE;
}

/* ============================================================================
 * Website Input Dialog Implementation
 * ============================================================================ */

void ShowWebsiteDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_WEBSITE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_WEBSITE);
        SetForegroundWindow(existing);
        return;
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
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

            extern char CLOCK_TIMEOUT_WEBSITE_URL[];
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                wchar_t wUrl[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, wUrl, MAX_PATH);
                SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wUrl);
            }

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            SetFocus(hwndEdit);
            Dialog_SelectAllText(hwndEdit);

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
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url)/sizeof(wchar_t));

                if (Dialog_IsEmptyOrWhitespace(url)) {
                    DestroyWindow(hwndDlg);
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
                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
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
            DestroyWindow(hwndDlg);
            return TRUE;
    }

    return FALSE;
}

/* ============================================================================
 * Font License Dialog Implementation
 * ============================================================================ */

static wchar_t* g_displayText = NULL;
static MarkdownLink* g_links = NULL;
static int g_linkCount = 0;
static MarkdownHeading* g_headings = NULL;
static int g_headingCount = 0;
static MarkdownStyle* g_styles = NULL;
static int g_styleCount = 0;
static MarkdownListItem* g_listItems = NULL;
static int g_listItemCount = 0;
static MarkdownBlockquote* g_blockquotes = NULL;
static int g_blockquoteCount = 0;
static MarkdownColorTag* g_colorTags = NULL;
static int g_colorTagCount = 0;
static MarkdownFontTag* g_fontTags = NULL;
static int g_fontTagCount = 0;

/* Parent window handle for posting results */
static HWND g_fontLicenseParent = NULL;

static void CleanupFontLicenseResources(void) {
    FreeMarkdownLinks(g_links, g_linkCount);
    g_links = NULL;
    g_linkCount = 0;
    if (g_headings) { free(g_headings); g_headings = NULL; }
    g_headingCount = 0;
    if (g_styles) { free(g_styles); g_styles = NULL; }
    g_styleCount = 0;
    if (g_listItems) { free(g_listItems); g_listItems = NULL; }
    g_listItemCount = 0;
    if (g_blockquotes) { free(g_blockquotes); g_blockquotes = NULL; }
    g_blockquoteCount = 0;
    if (g_colorTags) { free(g_colorTags); g_colorTags = NULL; }
    g_colorTagCount = 0;
    if (g_fontTags) { free(g_fontTags); g_fontTags = NULL; }
    g_fontTagCount = 0;
    if (g_displayText) { free(g_displayText); g_displayText = NULL; }
}

INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_FONT_LICENSE, hwndDlg);
            
            const wchar_t* title = GetLocalizedString(
                NULL,
                L"Custom Font Feature License Agreement"
            );
            SetWindowTextW(hwndDlg, title);

            const wchar_t* licenseText = GetLocalizedString(
                NULL,
                L"FontLicenseAgreementText"
            );

            /* Wrap license text with <md> tags for markdown parsing */
            size_t textLen = wcslen(licenseText);
            size_t bufSize = textLen + 16;
            wchar_t* wrappedText = (wchar_t*)malloc(bufSize * sizeof(wchar_t));
            if (wrappedText) {
                wcscpy_s(wrappedText, bufSize, L"<md>\n");
                wcscat_s(wrappedText, bufSize, licenseText);
                wcscat_s(wrappedText, bufSize, L"\n</md>");
                ParseMarkdownLinks(wrappedText, &g_displayText, &g_links, &g_linkCount,
                                   &g_headings, &g_headingCount,
                                   &g_styles, &g_styleCount,
                                   &g_listItems, &g_listItemCount,
                                   &g_blockquotes, &g_blockquoteCount,
                                   &g_colorTags, &g_colorTagCount,
                                   &g_fontTags, &g_fontTagCount);
                free(wrappedText);
            } else {
                ParseMarkdownLinks(licenseText, &g_displayText, &g_links, &g_linkCount,
                                   &g_headings, &g_headingCount,
                                   &g_styles, &g_styleCount,
                                   &g_listItems, &g_listItemCount,
                                   &g_blockquotes, &g_blockquoteCount,
                                   &g_colorTags, &g_colorTagCount,
                                   &g_fontTags, &g_fontTagCount);
            }

            const wchar_t* agreeText = GetLocalizedString(NULL, L"Agree");
            const wchar_t* cancelText = GetLocalizedString(NULL, L"Cancel");

            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_AGREE_BTN, agreeText);
            SetDlgItemTextW(hwndDlg, IDC_FONT_LICENSE_CANCEL_BTN, cancelText);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FONT_LICENSE_AGREE_BTN:
                    CleanupFontLicenseResources();
                    if (g_fontLicenseParent) {
                        PostMessage(g_fontLicenseParent, WM_DIALOG_FONT_LICENSE, IDOK, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                case IDC_FONT_LICENSE_CANCEL_BTN:
                case IDCANCEL:
                    CleanupFontLicenseResources();
                    if (g_fontLicenseParent) {
                        PostMessage(g_fontLicenseParent, WM_DIALOG_FONT_LICENSE, IDCANCEL, 0);
                    }
                    DestroyWindow(hwndDlg);
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

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CleanupFontLicenseResources();
                if (g_fontLicenseParent) {
                    PostMessage(g_fontLicenseParent, WM_DIALOG_FONT_LICENSE, IDCANCEL, 0);
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_FONT_LICENSE_TEXT) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;

                HBRUSH hBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);

                if (g_displayText) {
                    SetBkMode(hdc, TRANSPARENT);

                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

                    RECT drawRect = rect;
                    drawRect.left += 5;
                    drawRect.top += 5;

                    RenderMarkdownText(hdc, g_displayText, g_links, g_linkCount,
                                       g_headings, g_headingCount,
                                       g_styles, g_styleCount,
                                       g_listItems, g_listItemCount,
                                       g_blockquotes, g_blockquoteCount,
                                       drawRect, MARKDOWN_DEFAULT_LINK_COLOR, MARKDOWN_DEFAULT_TEXT_COLOR);

                    SelectObject(hdc, hOldFont);
                }

                return TRUE;
            }
            break;
        }

        case WM_DESTROY:
            CleanupFontLicenseResources();
            Dialog_UnregisterInstance(DIALOG_INSTANCE_FONT_LICENSE);
            break;

        case WM_CLOSE:
            CleanupFontLicenseResources();
            if (g_fontLicenseParent) {
                PostMessage(g_fontLicenseParent, WM_DIALOG_FONT_LICENSE, IDCANCEL, 0);
            }
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    return FALSE;
}

void ShowFontLicenseDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_LICENSE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_FONT_LICENSE);
        SetForegroundWindow(existing);
        return;
    }

    g_fontLicenseParent = hwndParent;

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FONT_LICENSE_DIALOG),
        hwndParent,
        FontLicenseDlgProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}


