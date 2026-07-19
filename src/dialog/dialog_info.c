/**
 * @file dialog_info.c
 * @brief Informational dialogs implementation
 */

#include "dialog/dialog_info.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_form_layout.h"
#include "dialog/dialog_markdown.h"
#include "dialog/dialog_modern.h"
#include "language.h"
#include "dialog/dialog_language.h"
#include "config.h"
#include "utils/win32_dynamic_loader.h"
#include "../resource/resource.h"
#include <shellapi.h>
#include <strsafe.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define FONT_LICENSE_PARENT_PROP L"Catime.FontLicense.Parent"
#define ABOUT_LINK_ORIG_PROC_PROP L"Catime.AboutLink.OrigProc"
#define ABOUT_LINK_HOVER_PROP L"Catime.AboutLink.Hover"
#define ABOUT_MODERN_ICON_SIZE_96 88
#define ABOUT_CONTENT_WIDTH_96 448

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
    UINT controlId;
    const wchar_t* textCN;
    const wchar_t* textEN;
    const wchar_t* url;
} AboutLinkInfo;

static AboutLinkInfo g_aboutLinkInfos[] = {
    {IDC_CREDIT_LINK, NULL, L"Special thanks to 猫屋敷梨梨Official for the icon", L"https://space.bilibili.com/26087398"},
    {IDC_COPYRIGHT, NULL, L"Copyright © 2025-2026 vladelaina", L"https://vladelaina.com/"},
    {IDC_CREDITS, NULL, L"Credits", L"https://cati.me/#thanks"},
    {IDC_BILIBILI_LINK, NULL, L"BiliBili", L"https://space.bilibili.com/1862395225"},
    {IDC_GITHUB_LINK, NULL, L"GitHub", L"https://github.com/vladelaina/Catime"},
    {IDC_COPYRIGHT_LINK, NULL, L"Copyright Notice", L"https://github.com/vladelaina/Catime#️copyright-notice"},
    {IDC_SUPPORT, NULL, L"Discord", L"https://discord.com/invite/W3tW2gtp6g"}
};

static BOOL ConvertWideUrlToUtf8(const wchar_t* source, char* dest, size_t destSize) {
    if (!source || !dest || destSize == 0 || destSize > INT_MAX) {
        return FALSE;
    }

    dest[0] = '\0';
    int required = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > destSize) {
        return FALSE;
    }

    return WideCharToMultiByte(CP_UTF8, 0, source, -1, dest,
                               (int)destSize, NULL, NULL) > 0;
}

static const size_t g_aboutLinkInfoCount = sizeof(g_aboutLinkInfos) / sizeof(g_aboutLinkInfos[0]);

static LRESULT CALLBACK AboutLinkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static BOOL IsAboutLinkControlId(UINT controlId) {
    for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
        if (g_aboutLinkInfos[i].controlId == controlId) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL IsAboutFooterLinkControlId(UINT controlId) {
    return controlId != IDC_CREDIT_LINK && controlId != IDC_COPYRIGHT &&
           IsAboutLinkControlId(controlId);
}

static WNDPROC GetAboutLinkOrigProc(HWND hwndLink) {
    return (WNDPROC)(LONG_PTR)GetPropW(hwndLink, ABOUT_LINK_ORIG_PROC_PROP);
}

static BOOL SubclassAboutLinkControl(HWND hwndLink) {
    if (!hwndLink || GetAboutLinkOrigProc(hwndLink)) {
        return hwndLink != NULL;
    }

    WNDPROC origProc = (WNDPROC)(LONG_PTR)SetWindowLongPtrW(
        hwndLink, GWLP_WNDPROC, (LONG_PTR)AboutLinkSubclassProc);
    if (!origProc) {
        return FALSE;
    }

    if (!SetPropW(hwndLink, ABOUT_LINK_ORIG_PROC_PROP, (HANDLE)(LONG_PTR)origProc)) {
        SetWindowLongPtrW(hwndLink, GWLP_WNDPROC, (LONG_PTR)origProc);
        return FALSE;
    }

    return TRUE;
}

static void UnsubclassAboutLinkControl(HWND hwndLink) {
    if (!hwndLink) return;

    WNDPROC origProc = GetAboutLinkOrigProc(hwndLink);
    if (!origProc) return;

    SetWindowLongPtrW(hwndLink, GWLP_WNDPROC, (LONG_PTR)origProc);
    RemovePropW(hwndLink, ABOUT_LINK_ORIG_PROC_PROP);
}

static LRESULT CALLBACK AboutLinkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = GetAboutLinkOrigProc(hwnd);
    if (!origProc) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_MOUSEMOVE:
            if (!GetPropW(hwnd, ABOUT_LINK_HOVER_PROP)) {
                TRACKMOUSEEVENT track = {0};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                SetPropW(hwnd, ABOUT_LINK_HOVER_PROP, (HANDLE)1);
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_MOUSELEAVE:
            RemovePropW(hwnd, ABOUT_LINK_HOVER_PROP);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_SPACE) {
                SendMessageW(GetParent(hwnd), WM_COMMAND,
                             MAKEWPARAM(GetDlgCtrlID(hwnd), STN_CLICKED),
                             (LPARAM)hwnd);
                return 0;
            }
            break;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;

        case WM_NCDESTROY:
            RemovePropW(hwnd, ABOUT_LINK_HOVER_PROP);
            UnsubclassAboutLinkControl(hwnd);
            return CallWindowProcW(origProc, hwnd, msg, wParam, lParam);
    }

    return CallWindowProcW(origProc, hwnd, msg, wParam, lParam);
}

static void ConfigureAboutLinkControls(HWND hwndDlg) {
    for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
        HWND hwndLink = GetDlgItem(hwndDlg, g_aboutLinkInfos[i].controlId);
        if (!hwndLink) continue;

        LONG_PTR style = GetWindowLongPtrW(hwndLink, GWL_STYLE);
        LONG_PTR exStyle = GetWindowLongPtrW(hwndLink, GWL_EXSTYLE);
        SetWindowLongPtrW(hwndLink, GWL_STYLE, style | WS_TABSTOP);
        SetWindowLongPtrW(hwndLink, GWL_EXSTYLE,
                          exStyle & ~WS_EX_TRANSPARENT);
        SetWindowPos(hwndLink, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SubclassAboutLinkControl(hwndLink);
    }
}

static void PaintAboutLinkBackground(HWND hwndDlg, const DRAWITEMSTRUCT* drawItem) {
    DialogModernPalette palette;
    DialogModern_CopyPalette(hwndDlg, &palette);
    RECT rect = drawItem->rcItem;
    HBRUSH brush = CreateSolidBrush(palette.surface);
    if (brush) {
        FillRect(drawItem->hDC, &rect, brush);
        DeleteObject(brush);
    }
}

/* ============================================================================
 * Global State
 * ============================================================================ */

char g_websiteInput[512] = {0};

typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);

static GetThreadDpiAwarenessContextFunc LoadGetThreadDpiAwarenessContext(HMODULE module) {
    GetThreadDpiAwarenessContextFunc func = NULL;
    CATIME_LOAD_PROC_ADDRESS(module, "GetThreadDpiAwarenessContext", func);
    return func;
}

static SetThreadDpiAwarenessContextFunc LoadSetThreadDpiAwarenessContext(HMODULE module) {
    SetThreadDpiAwarenessContextFunc func = NULL;
    CATIME_LOAD_PROC_ADDRESS(module, "SetThreadDpiAwarenessContext", func);
    return func;
}

static UINT GetAboutDialogDpi(HWND hwndDlg) {
    return DialogModern_GetDpi(hwndDlg);
}

static void SetAboutControlRect96(HWND hwndDlg, UINT controlId, UINT dpi,
                                  int x, int y, int width, int height) {
    DialogModern_SetChildRect96(hwndDlg, (int)controlId, dpi,
                                x, y, width, height);
}

static int MeasureAboutLinkWidth96(HWND control, UINT dpi) {
    wchar_t text[256] = {0};
    GetWindowTextW(control, text, (int)_countof(text));
    HDC hdc = GetDC(control);
    if (!hdc) return 44;
    HFONT font = (HFONT)SendMessageW(control, WM_GETFONT, 0, 0);
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    SIZE size = {0};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(control, hdc);
    int width = MulDiv(size.cx, 96, (int)(dpi ? dpi : 96u)) + 14;
    return width < 44 ? 44 : width;
}

static void LayoutAboutDialogControls(HWND hwndDlg) {
    static const UINT footerLinks[] = {
        IDC_CREDITS,
        IDC_BILIBILI_LINK,
        IDC_GITHUB_LINK,
        IDC_COPYRIGHT_LINK,
        IDC_SUPPORT
    };
    UINT dpi = GetAboutDialogDpi(hwndDlg);
    SetAboutControlRect96(hwndDlg, IDC_ABOUT_ICON, dpi,
                          0, 4, ABOUT_MODERN_ICON_SIZE_96,
                          ABOUT_MODERN_ICON_SIZE_96);
    SetAboutControlRect96(hwndDlg, IDC_VERSION_TEXT, dpi, 112, 0, 336, 22);
    SetAboutControlRect96(hwndDlg, IDC_BUILD_DATE, dpi, 112, 26, 336, 22);
    SetAboutControlRect96(hwndDlg, IDC_COPYRIGHT, dpi, 112, 52, 336, 22);
    SetAboutControlRect96(hwndDlg, IDC_CREDIT_LINK, dpi, 112, 78, 336, 24);

    int widths[_countof(footerLinks)] = {0};
    int totalWidth = 0;
    const int gap = 12;
    for (size_t i = 0; i < _countof(footerLinks); i++) {
        HWND link = GetDlgItem(hwndDlg, (int)footerLinks[i]);
        widths[i] = link ? MeasureAboutLinkWidth96(link, dpi) : 44;
        totalWidth += widths[i];
    }
    totalWidth += gap * ((int)_countof(footerLinks) - 1);

    int availableForText = ABOUT_CONTENT_WIDTH_96 -
                           gap * ((int)_countof(footerLinks) - 1);
    int textWidth = totalWidth -
                    gap * ((int)_countof(footerLinks) - 1);
    if (textWidth > availableForText) {
        int remainingWidth = availableForText;
        int remainingIdealWidth = textWidth;
        for (size_t i = 0; i < _countof(footerLinks); i++) {
            int remainingItems = (int)_countof(footerLinks) - (int)i;
            int idealWidth = widths[i];
            int width = remainingItems == 1 ? remainingWidth :
                MulDiv(idealWidth, remainingWidth, remainingIdealWidth);
            int maximum = remainingWidth - (remainingItems - 1) * 36;
            if (width < 36) width = 36;
            if (width > maximum) width = maximum;
            widths[i] = width;
            remainingWidth -= width;
            remainingIdealWidth -= idealWidth;
        }
        totalWidth = ABOUT_CONTENT_WIDTH_96;
    }

    int x = (ABOUT_CONTENT_WIDTH_96 - totalWidth) / 2;
    for (size_t i = 0; i < _countof(footerLinks); i++) {
        SetAboutControlRect96(hwndDlg, footerLinks[i], dpi,
                              x, 122, widths[i], 24);
        x += widths[i] + gap;
    }
}

static void ReloadAboutDialogIcon(HWND hwndDlg) {
    UINT dpi = GetAboutDialogDpi(hwndDlg);
    int iconSize = DialogModern_Scale(dpi, ABOUT_MODERN_ICON_SIZE_96);
    if (iconSize <= 0) {
        iconSize = ABOUT_MODERN_ICON_SIZE_96;
    }

    HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL),
                                    MAKEINTRESOURCEW(IDI_CATIME),
                                    IMAGE_ICON,
                                    iconSize,
                                    iconSize,
                                    LR_DEFAULTCOLOR);
    if (!hIcon) {
        return;
    }

    HICON hOldIcon = (HICON)SendDlgItemMessageW(hwndDlg, IDC_ABOUT_ICON,
                                                STM_SETICON,
                                                (WPARAM)hIcon, 0);
    if (hOldIcon && hOldIcon != hIcon) {
        DestroyIcon(hOldIcon);
    }
}

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
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc =
            LoadGetThreadDpiAwarenessContext(hUser32);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc =
            LoadSetThreadDpiAwarenessContext(hUser32);

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
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc =
            LoadSetThreadDpiAwarenessContext(hUser32);
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
}

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_InitializeInstance(DIALOG_INSTANCE_ABOUT, hwndDlg);

            ReloadAboutDialogIcon(hwndDlg);

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
            buildTimeUTC8.wYear = (WORD)year;
            buildTimeUTC8.wMonth = (WORD)month_num;
            buildTimeUTC8.wDay = (WORD)day;
            buildTimeUTC8.wHour = (WORD)hour;
            buildTimeUTC8.wMinute = (WORD)min;
            buildTimeUTC8.wSecond = (WORD)sec;

            /* Convert to FILETIME (UTC+8 is 8 hours ahead of UTC) */
            FILETIME fileTime;
            SystemTimeToFileTime(&buildTimeUTC8, &fileTime);

            /* Subtract 8 hours to get UTC time */
            ULONGLONG utcTicks = (((ULONGLONG)fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
            utcTicks -= (ULONGLONG)8 * 60 * 60 * 10000000;  /* 8 hours in 100-nanosecond intervals */
            fileTime.dwLowDateTime = (DWORD)(utcTicks & 0xFFFFFFFFULL);
            fileTime.dwHighDateTime = (DWORD)(utcTicks >> 32);

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

            for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
                const wchar_t* linkText = GetLocalizedString(g_aboutLinkInfos[i].textCN, g_aboutLinkInfos[i].textEN);
                SetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, linkText);
            }
            ConfigureAboutLinkControls(hwndDlg);
            LayoutAboutDialogControls(hwndDlg);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            return TRUE;
        }

        case WM_DPICHANGED:
            ReloadAboutDialogIcon(hwndDlg);
            LayoutAboutDialogControls(hwndDlg);
            break;

        case WM_DESTROY: {
            HICON hLargeIcon = (HICON)SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON,
                                                         STM_SETICON, 0, 0);
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_ABOUT, hwndDlg);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
                if (LOWORD(wParam) == g_aboutLinkInfos[i].controlId && HIWORD(wParam) == STN_CLICKED) {
                    ShellExecuteW(NULL, L"open", g_aboutLinkInfos[i].url, NULL, NULL, SW_SHOWNORMAL);
                    return TRUE;
                }
            }
            break;

        case WM_SETCURSOR: {
            HWND hwndControl = (HWND)wParam;
            if (LOWORD(lParam) == HTCLIENT &&
                IsAboutLinkControlId((UINT)GetDlgCtrlID(hwndControl))) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;

            for (size_t i = 0; i < g_aboutLinkInfoCount; i++) {
                if (lpDrawItem->CtlID == g_aboutLinkInfos[i].controlId) {
                    RECT rect = lpDrawItem->rcItem;
                    HDC hdc = lpDrawItem->hDC;

                    PaintAboutLinkBackground(hwndDlg, lpDrawItem);

                    wchar_t text[256];
                    GetDlgItemTextW(hwndDlg, g_aboutLinkInfos[i].controlId, text, sizeof(text)/sizeof(text[0]));

                    HFONT hFont = (HFONT)SendMessageW(
                        lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    DialogModernPalette palette;
                    DialogModern_CopyPalette(hwndDlg, &palette);
                    BOOL active =
                        GetPropW(lpDrawItem->hwndItem, ABOUT_LINK_HOVER_PROP) ||
                        GetFocus() == lpDrawItem->hwndItem;
                    COLORREF color = active ? palette.accentHover :
                                              palette.accent;
                    UINT format = DT_VCENTER | DT_SINGLELINE |
                                  DT_END_ELLIPSIS;
                    format |= IsAboutFooterLinkControlId(lpDrawItem->CtlID) ?
                              DT_CENTER : DT_LEFT;
                    DialogModern_DrawText(hdc, hFont, color, &rect, text,
                                          format);

                    if (active) {
                        HGDIOBJ oldFont = hFont ? SelectObject(hdc, hFont) : NULL;
                        SIZE textSize = {0};
                        GetTextExtentPoint32W(hdc, text, (int)wcslen(text),
                                              &textSize);
                        if (oldFont) SelectObject(hdc, oldFont);
                        int textWidth = textSize.cx;
                        int controlWidth = rect.right - rect.left;
                        if (textWidth > controlWidth) textWidth = controlWidth;
                        int left = IsAboutFooterLinkControlId(lpDrawItem->CtlID) ?
                            rect.left + (controlWidth - textWidth) / 2 :
                            rect.left;
                        RECT underline = {
                            left,
                            rect.bottom - DialogModern_Scale(
                                DialogModern_GetDpi(hwndDlg), 2),
                            left + textWidth,
                            rect.bottom
                        };
                        DialogModern_DrawRoundedRect(
                            hdc, &underline,
                            DialogModern_Scale(DialogModern_GetDpi(hwndDlg), 2),
                            color, color, 0);
                    }
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
            Dialog_InitializeInstance(DIALOG_INSTANCE_WEBSITE, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) {
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_WEBSITE, hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            ctx->userData = (void*)lParam;
            Dialog_SetContext(hwndDlg, ctx);

            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            Dialog_SubclassEdit(hwndEdit, ctx);
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                wchar_t wUrl[MAX_PATH];
                if (MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1,
                                        wUrl, MAX_PATH) > 0) {
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, wUrl);
                }
            }

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            DialogFormLayout_ApplyInstruction(
                hwndDlg, CLOCK_IDC_STATIC, CLOCK_IDC_EDIT,
                CLOCK_IDC_BUTTON_OK);

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
                    if (FAILED(StringCbCatW(tempUrl, sizeof(tempUrl), url)) ||
                        FAILED(StringCbCopyW(url, sizeof(url), tempUrl))) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                        return TRUE;
                    }
                }

                char urlUtf8[MAX_PATH * 3] = {0};
                if (!ConvertWideUrlToUtf8(url, urlUtf8, sizeof(urlUtf8))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
                if (!WriteConfigTimeoutWebsite(urlUtf8)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, CLOCK_IDC_EDIT);
                    return TRUE;
                }
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
                Dialog_DestroyContext(hwndDlg);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_WEBSITE, hwndDlg);
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

static DialogMarkdownState* GetFontLicenseMarkdown(HWND hwndDlg) {
    return hwndDlg ? (DialogMarkdownState*)GetWindowLongPtrW(
                         hwndDlg, GWLP_USERDATA)
                   : NULL;
}

static void CleanupFontLicenseResources(HWND hwndDlg) {
    DialogMarkdownState* markdown = GetFontLicenseMarkdown(hwndDlg);
    SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
    DialogMarkdown_Destroy(markdown);
}

static BOOL IsValidFontLicenseParentWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static HWND GetFontLicenseParent(HWND hwndDlg) {
    HWND hwndParent = (HWND)GetPropW(hwndDlg, FONT_LICENSE_PARENT_PROP);
    return IsValidFontLicenseParentWindow(hwndParent) ? hwndParent : NULL;
}

static BOOL PostFontLicenseResult(HWND hwndDlg, WPARAM result) {
    HWND hwndParent = GetFontLicenseParent(hwndDlg);
    if (!hwndParent) {
        return FALSE;
    }

    return PostMessage(hwndParent, WM_DIALOG_FONT_LICENSE, result, 0) != 0;
}

INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            DialogMarkdownState* markdown = DialogMarkdown_Create();
            if (!markdown) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)markdown);
            Dialog_InitializeInstance(DIALOG_INSTANCE_FONT_LICENSE, hwndDlg);
            HWND hwndParent = (HWND)lParam;
            if (IsValidFontLicenseParentWindow(hwndParent)) {
                SetPropW(hwndDlg, FONT_LICENSE_PARENT_PROP, (HANDLE)hwndParent);
            }
            
            const wchar_t* title = GetLocalizedString(
                NULL,
                L"Custom Font Feature License Agreement"
            );
            SetWindowTextW(hwndDlg, title);

            const wchar_t* licenseText = GetLocalizedString(
                NULL,
                L"FontLicenseAgreementText"
            );

            if (!DialogMarkdown_Parse(markdown, licenseText, TRUE)) {
                CleanupFontLicenseResources(hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
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
                    CleanupFontLicenseResources(hwndDlg);
                    PostFontLicenseResult(hwndDlg, IDOK);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                case IDC_FONT_LICENSE_CANCEL_BTN:
                case IDCANCEL:
                    CleanupFontLicenseResources(hwndDlg);
                    PostFontLicenseResult(hwndDlg, IDCANCEL);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                case IDC_FONT_LICENSE_TEXT:
                    if (HIWORD(wParam) == STN_CLICKED) {
                        POINT pt;
                        GetCursorPos(&pt);
                        ScreenToClient(GetDlgItem(hwndDlg, IDC_FONT_LICENSE_TEXT), &pt);

                        if (DialogMarkdown_HandleClick(
                                GetFontLicenseMarkdown(hwndDlg), pt)) {
                            return TRUE;
                        }
                    }
                    return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CleanupFontLicenseResources(hwndDlg);
                PostFontLicenseResult(hwndDlg, IDCANCEL);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_FONT_LICENSE_TEXT) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;

                DialogModernPalette palette;
                DialogModern_CopyPalette(hwndDlg, &palette);
                HBRUSH surfaceBrush = CreateSolidBrush(palette.surface);
                if (surfaceBrush) {
                    FillRect(hdc, &rect, surfaceBrush);
                    DeleteObject(surfaceBrush);
                }
                RECT panelRect = rect;
                InflateRect(&panelRect, -1, -1);
                DialogModern_DrawRoundedRect(
                    hdc, &panelRect,
                    DialogModern_Scale(DialogModern_GetDpi(hwndDlg), 14),
                    palette.field, palette.border, 1);

                DialogMarkdownState* markdown =
                    GetFontLicenseMarkdown(hwndDlg);
                if (markdown) {
                    int oldBkMode = SetBkMode(hdc, TRANSPARENT);

                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;

                    RECT drawRect = panelRect;
                    int inset = DialogModern_Scale(
                        DialogModern_GetDpi(hwndDlg), 10);
                    InflateRect(&drawRect, -inset, -inset);

                    DialogMarkdown_Render(markdown, hdc, drawRect,
                                          palette.accent, palette.text);

                    if (hOldFont) {
                        SelectObject(hdc, hOldFont);
                    }
                    if (oldBkMode != 0) {
                        SetBkMode(hdc, oldBkMode);
                    }
                }

                return TRUE;
            }
            break;
        }

        case WM_DESTROY:
            CleanupFontLicenseResources(hwndDlg);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_FONT_LICENSE, hwndDlg);
            RemovePropW(hwndDlg, FONT_LICENSE_PARENT_PROP);
            break;

        case WM_CLOSE:
            CleanupFontLicenseResources(hwndDlg);
            PostFontLicenseResult(hwndDlg, IDCANCEL);
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

    if (!IsValidFontLicenseParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogParamW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FONT_LICENSE_DIALOG),
        hwndParent,
        FontLicenseDlgProc,
        (LPARAM)hwndParent
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}
